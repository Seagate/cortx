/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Mikhail Antropov <mikhail.antropov@xyratex.com>
 * Original creation date: 26-Aug-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_IOSERVICE
#include "lib/trace.h"

#include "lib/types.h"
#include "lib/errno.h"
#include "lib/lockers.h"
#include "lib/memory.h"              /* M0_ALLOC_PTR */
#include "lib/misc.h"                /* M0_SET0 */
#include "be/seg.h"
#include "be/tx.h"
#include "cob/cob.h"
#include "ioservice/io_service.h"
#include "ioservice/ios_start_sm.h"
#include "mero/setup.h"
#include "module/instance.h"         /* m0_get */
#include "pool/pool.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"

M0_INTERNAL int m0_ios_create_buffer_pool(struct m0_reqh_service *service);
M0_INTERNAL void m0_ios_delete_buffer_pool(struct m0_reqh_service *service);

static struct m0_sm_state_descr ios_start_sm_states[] = {
	[M0_IOS_START_INIT] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "ios_start_init",
		.sd_allowed = M0_BITS(M0_IOS_START_CDOM_CREATE,
				      M0_IOS_START_BUFFER_POOL_CREATE,
				      M0_IOS_START_FAILURE)
	},
	[M0_IOS_START_CDOM_CREATE] = {
		.sd_flags   = 0,
		.sd_name    = "ios_start_be_create",
		.sd_allowed = M0_BITS(M0_IOS_START_CDOM_CREATE_RES)
	},
	[M0_IOS_START_CDOM_CREATE_RES] = {
		.sd_flags   = 0,
		.sd_name    = "ios_start_be_result",
		.sd_allowed = M0_BITS(M0_IOS_START_MKFS,
				      M0_IOS_START_FAILURE)
	},
	[M0_IOS_START_MKFS] = {
		.sd_flags   = 0,
		.sd_name    = "ios_start_mkfs",
		.sd_allowed = M0_BITS(M0_IOS_START_MKFS_RESULT)
	},
	[M0_IOS_START_MKFS_RESULT] = {
		.sd_flags   = 0,
		.sd_name    = "ios_start_mkfs_result",
		.sd_allowed = M0_BITS(M0_IOS_START_BUFFER_POOL_CREATE,
				      M0_IOS_START_FAILURE)
	},
	[M0_IOS_START_BUFFER_POOL_CREATE] = {
		.sd_flags   = 0,
		.sd_name    = "ios_start_buffer_pool_create",
		.sd_allowed = M0_BITS(M0_IOS_START_COMPLETE,
				      M0_IOS_START_FAILURE)
	},
	[M0_IOS_START_FAILURE] = {
		.sd_flags     = M0_SDF_FAILURE | M0_SDF_FINAL,
		.sd_name      = "ios_start_failure",
	},
	[M0_IOS_START_COMPLETE] = {
		.sd_flags   = M0_SDF_TERMINAL,
		.sd_name    = "ios_start_fini",
		.sd_allowed = 0
	},
};

static const struct m0_sm_conf ios_start_sm_conf = {
	.scf_name      = "ios_cdom conf",
	.scf_nr_states = ARRAY_SIZE(ios_start_sm_states),
	.scf_state     = ios_start_sm_states
};

static void ios_start_buffer_pool_create(struct m0_ios_start_sm *ios_sm);
static void ios_start_sm_tick(struct m0_ios_start_sm *ios_sm);

static struct m0_ios_start_sm *ios_start_clink2sm(struct m0_clink *cl)
{
	return container_of(cl, struct m0_ios_start_sm, ism_clink);
}

static struct m0_ios_start_sm *ios_start_ast2sm(struct m0_sm_ast *ast)
{
	return (struct m0_ios_start_sm *)ast->sa_datum;
}

M0_INTERNAL void m0_ios_start_lock(struct m0_ios_start_sm *ios_sm)
{
	m0_sm_group_lock(ios_sm->ism_sm.sm_grp);
}

M0_INTERNAL void m0_ios_start_unlock(struct m0_ios_start_sm *ios_sm)
{
	m0_sm_group_unlock(ios_sm->ism_sm.sm_grp);
}

static bool ios_start_is_locked(const struct m0_ios_start_sm *ios_sm)
{
	return m0_mutex_is_locked(&ios_sm->ism_sm.sm_grp->s_lock);
}

static enum m0_ios_start_state ios_start_state_get(
					const struct m0_ios_start_sm *ios_sm)
{
	return (enum m0_ios_start_state)ios_sm->ism_sm.sm_state;
}

static void ios_start_state_set(struct m0_ios_start_sm  *ios_sm,
				enum m0_ios_start_state  state)
{
	M0_PRE(ios_start_is_locked(ios_sm));

	M0_LOG(M0_DEBUG, "IO start sm:%p, state_change:[%s -> %s]", ios_sm,
		m0_sm_state_name(&ios_sm->ism_sm, ios_start_state_get(ios_sm)),
		m0_sm_state_name(&ios_sm->ism_sm, state));
	m0_sm_state_set(&ios_sm->ism_sm, state);
}

static void ios_start_sm_failure(struct m0_ios_start_sm *ios_sm, int rc)
{
	enum m0_ios_start_state state = ios_start_state_get(ios_sm);
	struct m0_be_seg       *seg   = ios_sm->ism_reqh->rh_beseg; /* XXX */
	struct m0_be_domain    *bedom = seg->bs_domain;
	int                     rc2   = 0;

	switch (state) {
	case M0_IOS_START_INIT:
		/* fallthrough */
	case M0_IOS_START_CDOM_CREATE_RES:
		/* Possibly errors source: tx close */
		if (ios_sm->ism_dom != NULL)
			m0_cob_domain_fini(ios_sm->ism_dom);
		break;
	case M0_IOS_START_MKFS:
		/* Possibly errors source: tx open */
		rc2 = m0_cob_domain_destroy(ios_sm->ism_dom,
					   ios_sm->ism_sm.sm_grp, bedom);
		break;
	case M0_IOS_START_MKFS_RESULT:
		/* Possibly errors source: tx close, m0_cob_domain_mkfs */
		rc2 = m0_cob_domain_destroy(ios_sm->ism_dom,
					   ios_sm->ism_sm.sm_grp, bedom);
		break;
	case M0_IOS_START_BUFFER_POOL_CREATE:
		rc2 = m0_cob_domain_destroy(ios_sm->ism_dom,
					   ios_sm->ism_sm.sm_grp, bedom);
		m0_ios_delete_buffer_pool(ios_sm->ism_service);
		break;
	case M0_IOS_START_COMPLETE:
		/* Possibly errors source: tx close */
		m0_ios_delete_buffer_pool(ios_sm->ism_service);
		m0_cob_domain_fini(ios_sm->ism_dom);
		break;
	default:
		M0_ASSERT(false);
		break;
	}

	if (rc2 != 0)
		M0_LOG(M0_ERROR,
		       "Ignoring rc2=%d from m0_cob_domain_destroy()", rc2);

	m0_sm_fail(&ios_sm->ism_sm, M0_IOS_START_FAILURE, rc);
}

M0_INTERNAL int m0_ios_start_sm_init(struct m0_ios_start_sm  *ios_sm,
				     struct m0_reqh_service  *service,
				     struct m0_sm_group      *grp)
{
	static uint64_t cid = M0_IOS_COB_ID_START;

	M0_ENTRY();
	M0_PRE(ios_sm != NULL);
	M0_PRE(service != NULL);

	ios_sm->ism_cdom_id.id = cid++;
	ios_sm->ism_dom = NULL;
	ios_sm->ism_service = service;
	ios_sm->ism_reqh = service->rs_reqh;
	ios_sm->ism_last_rc = 0;

	m0_sm_init(&ios_sm->ism_sm, &ios_start_sm_conf, M0_IOS_START_INIT, grp);

	return M0_RC(0);
}

M0_INTERNAL void m0_ios_start_sm_exec(struct m0_ios_start_sm *ios_sm)
{
	M0_ENTRY();
	ios_start_sm_tick(ios_sm);
	M0_LEAVE();
}

/* TX section  */

static void ios_start_tx_waiter(struct m0_clink *cl, uint32_t flag)
{
	struct m0_ios_start_sm *ios_sm = ios_start_clink2sm(cl);
	struct m0_sm           *tx_sm = &ios_sm->ism_tx.t_sm;

	if (M0_IN(tx_sm->sm_state, (flag, M0_BTS_FAILED))) {
		if (tx_sm->sm_rc != 0)
			ios_start_sm_failure(ios_sm, tx_sm->sm_rc);
		else
			ios_start_sm_tick(ios_sm);
		m0_clink_del(cl);
		m0_clink_fini(cl);
	}
}

static bool ios_start_tx_open_wait_cb(struct m0_clink *cl)
{
	ios_start_tx_waiter(cl, M0_BTS_ACTIVE);
	return true;
}

static bool ios_start_tx_done_wait_cb(struct m0_clink *cl)
{
	ios_start_tx_waiter(cl, M0_BTS_DONE);
	return true;
}

static void ios_start_tx_open(struct m0_ios_start_sm *ios_sm, bool exclusive)
{
	struct m0_sm *tx_sm = &ios_sm->ism_tx.t_sm;

	m0_clink_init(&ios_sm->ism_clink, ios_start_tx_open_wait_cb);
	m0_clink_add(&tx_sm->sm_chan, &ios_sm->ism_clink);
	if (exclusive)
		m0_be_tx_exclusive_open(&ios_sm->ism_tx);
	else
		m0_be_tx_open(&ios_sm->ism_tx);
}

static void ios_start_tx_close(struct m0_ios_start_sm *ios_sm)
{
	struct m0_sm *tx_sm = &ios_sm->ism_tx.t_sm;

	m0_clink_init(&ios_sm->ism_clink, ios_start_tx_done_wait_cb);
	m0_clink_add(&tx_sm->sm_chan, &ios_sm->ism_clink);
	m0_be_tx_close(&ios_sm->ism_tx);
}

static void ios_start_cdom_tx_open(struct m0_ios_start_sm *ios_sm)
{
	struct m0_be_tx_credit  cred  = {};
	struct m0_be_seg       *seg = ios_sm->ism_reqh->rh_beseg;
	struct m0_be_tx        *tx = &ios_sm->ism_tx;

	m0_cob_domain_credit_add(ios_sm->ism_dom, seg->bs_domain, seg,
				 &ios_sm->ism_cdom_id, &cred);

	M0_SET0(tx);
	m0_be_tx_init(tx, 0, seg->bs_domain, ios_sm->ism_sm.sm_grp,
		      NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, &cred);

	ios_start_state_set(ios_sm, M0_IOS_START_CDOM_CREATE);
	ios_start_tx_open(ios_sm, true);
}

/* AST section */

static void ios_start_ast_start(struct m0_sm_group *grp,
				struct m0_sm_ast   *ast)
{
	struct m0_ios_start_sm *ios_sm = ios_start_ast2sm(ast);
	int                     rc;

	M0_ENTRY();
	M0_ASSERT(ios_start_state_get(ios_sm) == M0_IOS_START_INIT);
	m0_rwlock_read_lock(&ios_sm->ism_reqh->rh_rwlock);
	ios_sm->ism_dom = m0_reqh_lockers_get(ios_sm->ism_reqh,
					       m0_get()->i_ios_cdom_key);
	m0_rwlock_read_unlock(&ios_sm->ism_reqh->rh_rwlock);
	if (ios_sm->ism_dom == NULL) {
		ios_start_cdom_tx_open(ios_sm);
	} else {
		rc = m0_cob_domain_init(ios_sm->ism_dom,
					ios_sm->ism_reqh->rh_beseg);
		if (rc == 0) {
			ios_start_state_set(ios_sm,
					    M0_IOS_START_BUFFER_POOL_CREATE);
			ios_start_buffer_pool_create(ios_sm);
		} else {
			ios_start_sm_failure(ios_sm, rc);
		}
	}
	M0_LEAVE();
}

static void ios_start_ast_cdom_create(struct m0_sm_group *grp,
				   struct m0_sm_ast *ast)
{
	struct m0_ios_start_sm *ios_sm = ios_start_ast2sm(ast);
	struct m0_be_seg       *seg    = ios_sm->ism_reqh->rh_beseg;
	int                     rc;

	M0_ENTRY();
	M0_ASSERT(ios_start_state_get(ios_sm) == M0_IOS_START_CDOM_CREATE);

	rc = m0_cob_domain_create_prepared(&ios_sm->ism_dom,
					   grp,
					   &ios_sm->ism_cdom_id,
					   seg->bs_domain,
					   seg,
					   &ios_sm->ism_tx);

	if (rc != 0)
		ios_sm->ism_last_rc = M0_ERR(rc);

	ios_start_state_set(ios_sm, M0_IOS_START_CDOM_CREATE_RES);
	ios_start_tx_close(ios_sm);
	M0_LEAVE();
}

static void ios_start_cob_tx_open(struct m0_ios_start_sm *ios_sm)
{
	struct m0_be_tx_credit  cred  = {};
	struct m0_be_seg       *seg = ios_sm->ism_reqh->rh_beseg;

	M0_ENTRY("key init for reqh=%p, key=%d",
		 ios_sm->ism_reqh, m0_get()->i_ios_cdom_key);

	m0_cob_tx_credit(ios_sm->ism_dom, M0_COB_OP_DOMAIN_MKFS, &cred);
	M0_SET0(&ios_sm->ism_tx);
	m0_be_tx_init(&ios_sm->ism_tx, 0, seg->bs_domain,
		      ios_sm->ism_sm.sm_grp, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(&ios_sm->ism_tx, &cred);

	ios_start_tx_open(ios_sm, false);

	M0_LEAVE();
}

static void ios_start_ast_cdom_create_res(struct m0_sm_group *grp,
					  struct m0_sm_ast   *ast)
{
	struct m0_ios_start_sm *ios_sm = ios_start_ast2sm(ast);

	M0_ENTRY();

	M0_ASSERT(ios_start_state_get(ios_sm) == M0_IOS_START_CDOM_CREATE_RES);

	m0_be_tx_fini(&ios_sm->ism_tx);

	if (ios_sm->ism_last_rc != 0) {
		ios_start_sm_failure(ios_sm, ios_sm->ism_last_rc);
	} else {
		/*
		 * COB domain is successfully created.
		 * Store its address in m0 instance and then format it.
		 */
		m0_rwlock_write_lock(&ios_sm->ism_reqh->rh_rwlock);
		m0_reqh_lockers_set(ios_sm->ism_reqh, m0_get()->i_ios_cdom_key,
				    ios_sm->ism_dom);
		m0_rwlock_write_unlock(&ios_sm->ism_reqh->rh_rwlock);

		ios_start_state_set(ios_sm, M0_IOS_START_MKFS);
		ios_start_cob_tx_open(ios_sm);
	}
	M0_LEAVE();
}

static void ios_start_ast_mkfs(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_ios_start_sm *ios_sm = ios_start_ast2sm(ast);

	M0_ENTRY();
	M0_ASSERT(ios_start_state_get(ios_sm) == M0_IOS_START_MKFS);

	ios_sm->ism_last_rc = m0_cob_domain_mkfs(ios_sm->ism_dom,
						 &M0_MDSERVICE_SLASH_FID,
						 &ios_sm->ism_tx);

	ios_start_state_set(ios_sm, M0_IOS_START_MKFS_RESULT);
	ios_start_tx_close(ios_sm);
	M0_LEAVE();
}

static void ios_start_ast_mkfs_result(struct m0_sm_group *grp,
				     struct m0_sm_ast *ast)
{
	struct m0_ios_start_sm *ios_sm = ios_start_ast2sm(ast);

	M0_ENTRY();
	M0_ASSERT(ios_start_state_get(ios_sm) == M0_IOS_START_MKFS_RESULT);

	m0_be_tx_fini(&ios_sm->ism_tx);

	ios_start_state_set(ios_sm, M0_IOS_START_BUFFER_POOL_CREATE);
	ios_start_buffer_pool_create(ios_sm);

	M0_LEAVE();
}

static void ios_start_buffer_pool_create(struct m0_ios_start_sm *ios_sm)
{
	int rc;

	M0_ENTRY();
	M0_ASSERT(ios_start_state_get(ios_sm) ==
		  M0_IOS_START_BUFFER_POOL_CREATE);

	rc = m0_ios_create_buffer_pool(ios_sm->ism_service);
	if (rc != 0) {
		ios_start_sm_failure(ios_sm, rc);
	} else {
		ios_start_state_set(ios_sm, M0_IOS_START_COMPLETE);
	}

	M0_LEAVE();
}

/* SM section */

static void ios_start_sm_tick(struct m0_ios_start_sm *ios_sm)
{
	int                             state;
	void (*handlers[])(struct m0_sm_group *, struct m0_sm_ast *) = {
		[M0_IOS_START_INIT] = ios_start_ast_start,
		[M0_IOS_START_CDOM_CREATE] = ios_start_ast_cdom_create,
		[M0_IOS_START_CDOM_CREATE_RES] = ios_start_ast_cdom_create_res,
		[M0_IOS_START_MKFS] = ios_start_ast_mkfs,
		[M0_IOS_START_MKFS_RESULT] = ios_start_ast_mkfs_result,
	};

	M0_ENTRY();
	state = ios_start_state_get(ios_sm);
	M0_LOG(M0_DEBUG, "State: %d", state);
	M0_ASSERT(handlers[state] != NULL);
	M0_SET0(&ios_sm->ism_ast);
	ios_sm->ism_ast.sa_cb = handlers[state];
	ios_sm->ism_ast.sa_datum = ios_sm;
	m0_sm_ast_post(ios_sm->ism_sm.sm_grp, &ios_sm->ism_ast);
	M0_LEAVE();
}

M0_INTERNAL void m0_ios_start_sm_fini(struct m0_ios_start_sm *ios_sm)
{
	M0_ENTRY("ios_sm: %p", ios_sm);
	M0_PRE(ios_sm != NULL);
	M0_PRE(M0_IN(ios_start_state_get(ios_sm),
		(M0_IOS_START_COMPLETE, M0_IOS_START_FAILURE)));
	M0_PRE(ios_start_is_locked(ios_sm));

	m0_sm_fini(&ios_sm->ism_sm);
	M0_LEAVE();
}

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
