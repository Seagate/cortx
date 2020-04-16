/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF XYRATEX TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF XYRATEX TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF XYRATEX LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF XYRATEX'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A XYRATEX REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 20-Jun-2016
 */


/**
 * @addtogroup dix
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIX
#include "lib/trace.h"
#include "lib/ext.h"    /* struct m0_ext */
#include "sm/sm.h"
#include "pool/pool.h"  /* m0_pools_common, m0_pool_version_find */
#include "dix/layout.h"
#include "dix/req.h"
#include "dix/meta.h"
#include "dix/client.h"
#include "dix/client_internal.h"

static struct m0_sm_state_descr dix_cli_states[] = {
	[DIXCLI_INIT] = {
		.sd_flags     = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name      = "init",
		.sd_allowed   = M0_BITS(DIXCLI_STARTING,
					DIXCLI_BOOTSTRAP)
	},
	[DIXCLI_BOOTSTRAP] = {
		.sd_name      = "bootstrap_mode",
		.sd_allowed   = M0_BITS(DIXCLI_STARTING, DIXCLI_FINAL)
	},
	[DIXCLI_STARTING] = {
		.sd_name      = "starting",
		.sd_allowed   = M0_BITS(DIXCLI_READY, DIXCLI_FAILURE)
	},
	[DIXCLI_READY] = {
		.sd_name      = "ready",
		.sd_allowed   = M0_BITS(DIXCLI_FINAL)
	},
	[DIXCLI_FINAL] = {
		.sd_name      = "final",
		.sd_flags     = M0_SDF_TERMINAL,
	},
	[DIXCLI_FAILURE] = {
		.sd_name      = "failure",
		.sd_flags     = M0_SDF_TERMINAL | M0_SDF_FAILURE
	}
};

static struct m0_sm_trans_descr dix_cli_trans[] = {
	{ "start",             DIXCLI_INIT,             DIXCLI_STARTING   },
	{ "bootstrap",         DIXCLI_INIT,             DIXCLI_BOOTSTRAP  },
	{ "ready",             DIXCLI_STARTING,         DIXCLI_READY      },
	{ "start-failure",     DIXCLI_STARTING,         DIXCLI_FAILURE    },
	{ "start",             DIXCLI_BOOTSTRAP,        DIXCLI_STARTING   },
	{ "bootstrap-exit",    DIXCLI_BOOTSTRAP,        DIXCLI_FINAL      },
	{ "stop",              DIXCLI_READY,            DIXCLI_FINAL      },
};

static const struct m0_sm_conf dix_cli_sm_conf = {
	.scf_name      = "dix_client",
	.scf_nr_states = ARRAY_SIZE(dix_cli_states),
	.scf_state     = dix_cli_states,
	.scf_trans_nr  = ARRAY_SIZE(dix_cli_trans),
	.scf_trans     = dix_cli_trans
};

static struct m0_sm_group *dix_cli_smgrp(const struct m0_dix_cli *cli)
{
	return cli->dx_sm.sm_grp;
}

M0_INTERNAL void m0_dix_cli_lock(struct m0_dix_cli *cli)
{
	M0_ENTRY();
	m0_sm_group_lock(dix_cli_smgrp(cli));
}

M0_INTERNAL void m0_dix_cli_unlock(struct m0_dix_cli *cli)
{
	M0_ENTRY();
	m0_sm_group_unlock(dix_cli_smgrp(cli));
}

M0_INTERNAL bool m0_dix_cli_is_locked(const struct m0_dix_cli *cli)
{
	return m0_mutex_is_locked(&dix_cli_smgrp(cli)->s_lock);
}

static enum m0_dix_cli_state dix_cli_state(const struct m0_dix_cli *cli)
{
	return cli->dx_sm.sm_state;
}

static void dix_cli_failure(struct m0_dix_cli *cli, int32_t rc)
{
	M0_PRE(rc != 0);
	m0_sm_fail(&cli->dx_sm, DIXCLI_FAILURE, rc);
}

static void dix_cli_state_set(struct m0_dix_cli     *cli,
			      enum m0_dix_cli_state  state)
{
	M0_LOG(M0_DEBUG, "DIX client: %p, state change:[%s -> %s]\n",
	       cli, m0_sm_state_name(&cli->dx_sm, cli->dx_sm.sm_state),
	       m0_sm_state_name(&cli->dx_sm, state));
	m0_sm_state_set(&cli->dx_sm, state);
}

M0_INTERNAL int m0_dix_cli_init(struct m0_dix_cli       *cli,
				struct m0_sm_group      *sm_group,
				struct m0_pools_common  *pc,
			        struct m0_layout_domain *ldom,
				const struct m0_fid     *pver)
{
	M0_ENTRY();
	M0_SET0(cli);
	cli->dx_pc   = pc;
	cli->dx_ldom = ldom;
	cli->dx_pver = m0_pool_version_find(pc, pver);
	cli->dx_sync_rec_update = NULL;
	m0_dix_ldesc_init(&cli->dx_root,
			  &(struct m0_ext) { .e_start = 0,
			                     .e_end = IMASK_INF },
			  1, HASH_FNC_FNV1,
			  &cli->dx_pver->pv_id);
	m0_sm_init(&cli->dx_sm, &dix_cli_sm_conf, DIXCLI_INIT, sm_group);
	return M0_RC(0);
}

static void dix_cli_ast_post(struct m0_dix_cli  *cli,
			     void              (*cb)(struct m0_sm_group *,
						     struct m0_sm_ast *))
{
	struct m0_sm_ast *ast = &cli->dx_ast;

	ast->sa_cb = cb;
	ast->sa_datum = cli;
	m0_sm_ast_post(dix_cli_smgrp(cli), ast);
}

static void dix_meta_read_ast_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_dix_cli      *cli = ast->sa_datum;
	struct m0_dix_meta_req *mreq = &cli->dx_mreq;
	struct m0_dix_ldesc    *layout = &cli->dx_layout;
	struct m0_dix_ldesc    *ldescr = &cli->dx_ldescr;
	int                     rc;

	(void)grp;
	rc = m0_dix_meta_generic_rc(mreq) ?:
	     m0_dix_root_read_rep(mreq, layout, ldescr);
	m0_dix_meta_req_fini(mreq);
	if (rc == 0)
		dix_cli_state_set(cli, DIXCLI_READY);
	else
		dix_cli_failure(cli, rc);
}

static bool dix_cli_meta_read_clink_cb(struct m0_clink *cl)
{
	struct m0_dix_cli *cli = container_of(cl, struct m0_dix_cli, dx_clink);

	m0_clink_fini(cl);
	dix_cli_ast_post(cli, dix_meta_read_ast_cb);
	return true;
}

static void dix_cli_start_ast_cb(struct m0_sm_group *grp M0_UNUSED,
				 struct m0_sm_ast   *ast)
{
	struct m0_dix_cli      *cli = ast->sa_datum;
	struct m0_dix_meta_req *mreq = &cli->dx_mreq;
	struct m0_clink        *cl = &cli->dx_clink;
	int                     rc;

	M0_ENTRY();
	dix_cli_state_set(cli, DIXCLI_STARTING);
	m0_dix_meta_req_init(mreq, cli, dix_cli_smgrp(cli));
	m0_clink_init(&cli->dx_clink, dix_cli_meta_read_clink_cb);
	m0_clink_add_lock(&mreq->dmr_chan, cl);
	cl->cl_is_oneshot = true;
	rc = m0_dix_root_read(mreq);
	if (rc != 0) {
		m0_clink_del_lock(cl);
		m0_clink_fini(cl);
		m0_dix_meta_req_fini(mreq);
		dix_cli_failure(cli, rc);
	}
	M0_LEAVE();
}

M0_INTERNAL void m0_dix_cli_start(struct m0_dix_cli *cli)
{
	M0_ENTRY();
	dix_cli_ast_post(cli, dix_cli_start_ast_cb);
	M0_LEAVE();
}

M0_INTERNAL int m0_dix_cli_start_sync(struct m0_dix_cli *cli)
{
	int rc;

	M0_ENTRY();
	m0_dix_cli_start(cli);
	m0_dix_cli_lock(cli);
	rc = m0_sm_timedwait(&cli->dx_sm,
			     M0_BITS(DIXCLI_READY, DIXCLI_FAILURE),
			     M0_TIME_NEVER);
	m0_dix_cli_unlock(cli);
	if (rc == 0)
		rc = cli->dx_sm.sm_rc;
	return M0_RC(rc);
}

M0_INTERNAL void m0_dix_cli_bootstrap(struct m0_dix_cli *cli)
{
	M0_ENTRY();
	M0_PRE(dix_cli_state(cli) == DIXCLI_INIT);
	M0_PRE(m0_dix_cli_is_locked(cli));
	dix_cli_state_set(cli, DIXCLI_BOOTSTRAP);
}

M0_INTERNAL void m0_dix_cli_bootstrap_lock(struct m0_dix_cli *cli)
{
	M0_PRE(!m0_dix_cli_is_locked(cli));
	m0_dix_cli_lock(cli);
	m0_dix_cli_bootstrap(cli);
	m0_dix_cli_unlock(cli);
}

M0_INTERNAL void m0_dix_cli_stop(struct m0_dix_cli *cli)
{
	M0_ENTRY();
	M0_PRE(m0_dix_cli_is_locked(cli));
	dix_cli_state_set(cli, DIXCLI_FINAL);
	M0_LEAVE();
}

M0_INTERNAL void m0_dix_cli_stop_lock(struct m0_dix_cli *cli)
{
	m0_dix_cli_lock(cli);
	m0_dix_cli_stop(cli);
	m0_dix_cli_unlock(cli);
}

M0_INTERNAL void m0_dix_cli_fini(struct m0_dix_cli *cli)
{
	M0_PRE(m0_dix_cli_is_locked(cli));
	m0_dix_ldesc_fini(&cli->dx_root);
	m0_dix_ldesc_fini(&cli->dx_layout);
	m0_dix_ldesc_fini(&cli->dx_ldescr);
	m0_sm_fini(&cli->dx_sm);
}

M0_INTERNAL void m0_dix_cli_fini_lock(struct m0_dix_cli *cli)
{
	struct m0_sm_group *grp = dix_cli_smgrp(cli);

	M0_PRE(!m0_dix_cli_is_locked(cli));
	m0_sm_group_lock(grp);
	m0_dix_cli_fini(cli);
	m0_sm_group_unlock(grp);
}

M0_INTERNAL int m0_dix__root_set(const struct m0_dix_cli *cli,
				 struct m0_dix           *out)
{
	out->dd_fid = m0_dix_root_fid;
	return m0_dix_desc_set(out, &cli->dx_root);
}

M0_INTERNAL int m0_dix__layout_set(const struct m0_dix_cli *cli,
				   struct m0_dix           *out)
{
	out->dd_fid = m0_dix_layout_fid;
	return m0_dix_desc_set(out, &cli->dx_layout);
}

M0_INTERNAL int m0_dix__ldescr_set(const struct m0_dix_cli *cli,
				   struct m0_dix           *out)
{
	out->dd_fid = m0_dix_ldescr_fid;
	return m0_dix_desc_set(out, &cli->dx_ldescr);
}

M0_INTERNAL struct m0_pool_version *m0_dix_pver(const struct m0_dix_cli *cli,
						const struct m0_dix     *dix)
{
	M0_PRE(dix->dd_layout.dl_type == DIX_LTYPE_DESCR);
	return m0_pool_version_find(cli->dx_pc,
				    &dix->dd_layout.u.dl_desc.ld_pver);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dix group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
