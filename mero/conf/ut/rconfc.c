/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov
 * Original creation date: 2015-03-11
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#define M0_UT_TRACE 0

#include <unistd.h>                    /* usleep */
#include "conf/rconfc.h"
#include "conf/rconfc_internal.h"      /* rlock_ctx */
#include "conf/confd.h"                /* m0_confd_stype */
#include "conf/helpers.h"              /* m0_confc_expired_cb */
#include "conf/ut/common.h"            /* SERVER_ENDPOINT */
#include "conf/ut/confc.h"             /* m0_ut_conf_fids */
#include "conf/ut/rpc_helpers.h"       /* m0_ut_rpc_machine_start */
#include "rpc/rpclib.h"                /* m0_rpc_server_ctx */
#include "lib/finject.h"
#include "lib/fs.h"                    /* m0_file_read */
#include "module/instance.h"           /* m0_get */
#include "ut/misc.h"                   /* M0_UT_PATH, M0_UT_CONF_PROCESS */
#include "ut/ut.h"

static struct m0_semaphore   g_expired_sem;
static struct m0_semaphore   g_ready_sem;
static struct m0_semaphore   g_fatal_sem;
static struct m0_reqh       *ut_reqh;
static struct m0_net_domain  client_net_dom;
static struct m0_net_xprt   *xprt = &m0_net_lnet_xprt;
static struct m0_fid         profile = M0_FID_TINIT('p', 1, 0);
static bool (*ha_clink_cb_orig)(struct m0_clink *clink);


M0_EXTERN struct m0_confc_gate_ops  m0_rconfc_gate_ops;
M0_EXTERN struct m0_rm_incoming_ops m0_rconfc_ri_ops;

enum {
	CLIENT_COB_DOM_ID  = 16,
	SESSION_SLOTS      = 1,
	MAX_RPCS_IN_FLIGHT = 1,
};

struct root_object {
	struct m0_conf_root *root;
	struct m0_clink      clink_x;
	struct m0_clink      clink_r;
	struct m0_confc     *confc;
};

static struct m0_rpc_client_ctx cctx = {
        .rcx_net_dom            = &client_net_dom,
        .rcx_local_addr         = CLIENT_ENDPOINT_ADDR,
        .rcx_remote_addr        = SERVER_ENDPOINT_ADDR,
        .rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT,
        .rcx_fid                = &g_process_fid,
};

static int rconfc_ut_mero_start(struct m0_rpc_machine    *mach,
				struct m0_rpc_server_ctx *rctx)
{
	int rc;
#define NAME(ext) "rconfc-ut" ext
	char *argv[] = {
		NAME(""), "-T", "AD", "-D", NAME(".db"),
		"-S", NAME(".stob"), "-A", "linuxstob:"NAME("-addb.stob"),
		"-w", "10", "-e", SERVER_ENDPOINT, "-H", SERVER_ENDPOINT_ADDR,
		"-f", M0_UT_CONF_PROCESS,
		"-c", M0_UT_PATH("diter.xc")
	};
	*rctx = (struct m0_rpc_server_ctx) {
		.rsx_xprts         = &m0_conf_ut_xprt,
		.rsx_xprts_nr      = 1,
		.rsx_argv          = argv,
		.rsx_argc          = ARRAY_SIZE(argv),
		.rsx_log_file_name = NAME(".log")
	};
#undef NAME

	M0_SET0(mach);
	rc = m0_rpc_server_start(rctx);
	M0_UT_ASSERT(rc == 0);

	rc = m0_ut_rpc_machine_start(mach, m0_conf_ut_xprt,
				     CLIENT_ENDPOINT_ADDR);
	M0_UT_ASSERT(rc == 0);
	ut_reqh = mach->rm_reqh;
	mach->rm_reqh = &rctx->rsx_mero_ctx.cc_reqh_ctx.rc_reqh;
	return rc;
}

static void rconfc_ut_mero_stop(struct m0_rpc_machine    *mach,
				struct m0_rpc_server_ctx *rctx)
{
	mach->rm_reqh = ut_reqh;
	m0_ut_rpc_machine_stop(mach);
	m0_rpc_server_stop(rctx);
}

static void test_null_exp_cb(struct m0_rconfc *rconfc)
{
	/*
	 * Test expiration callback that shouldn't be called, because
	 * test doesn't expect reelection.
	 */
	M0_UT_ASSERT(0);
}

static void conflict_exp_cb(struct m0_rconfc *rconfc)
{
	M0_UT_ENTER();
	m0_semaphore_up(&g_expired_sem);
	M0_UT_RETURN();
}

static void conflict_ready_cb(struct m0_rconfc *rconfc)
{
	M0_UT_ENTER();
	m0_semaphore_up(&g_ready_sem);
	M0_UT_RETURN();
}

static void test_init_fini(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	struct m0_rconfc         rconfc;
	int                      rc;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach,
			    test_null_exp_cb, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_rconfc_fini(&rconfc);
	rconfc_ut_mero_stop(&mach, &rctx);
}

static void test_start_stop(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;
	uint64_t                 ver;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rconfc.rc_profile = profile;
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rconfc.rc_ver != 0);
	ver = m0_rconfc_ver_max_read(&rconfc);
	M0_UT_ASSERT(ver == rconfc.rc_ver);
	/** @todo Check addresses used by rconfc */
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);
	rconfc_ut_mero_stop(&mach, &rctx);
}

static void test_start_stop_local(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_file_read(M0_UT_PATH("conf.xc"), &rconfc.rc_local_conf);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_rconfc_is_preloaded(&rconfc));
	M0_UT_ASSERT(rconfc.rc_ver != 0);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);
	rconfc_ut_mero_stop(&mach, &rctx);
}

static void test_local_load_fail(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL, NULL);
	M0_UT_ASSERT(rc == 0);
	rconfc.rc_local_conf = m0_strdup("abracadabra");
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc != 0);
	M0_UT_ASSERT(!m0_rconfc_is_preloaded(&rconfc));
	M0_UT_ASSERT(rconfc.rc_ver == 0);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);
	rconfc_ut_mero_stop(&mach, &rctx);
}

static void test_start_failures(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);
	m0_fi_enable_once("rlock_ctx_connect", "rm_conn_failed");
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	/*
	 * If connection to RM fails, then rconfc will try to start from
	 * beginning, because it is possible that RM creditor has changed during
	 * connection. Second attempt will succeed.
	 */
	M0_UT_ASSERT(rc == 0);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	m0_fi_enable_once("rconfc_read_lock_complete", "rlock_req_failed");
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == -ESRCH);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	rconfc_ut_mero_stop(&mach, &rctx);
}

static void rconfc_ut_fatal_cb(struct m0_rconfc *rconfc)
{
	M0_UT_ASSERT(rconfc->rc_sm.sm_state == M0_RCS_FAILURE);
	m0_semaphore_up(&g_fatal_sem);
}

static bool ha_clink_cb_suppress(struct m0_clink *clink)
{
	struct m0_rconfc *rconfc = M0_AMB(rconfc, clink, rc_ha_entrypoint_cl);

	m0_rconfc_lock(rconfc);
	M0_UT_ASSERT(rconfc->rc_sm.sm_state == M0_RCS_ENTRYPOINT_WAIT);
	if (rconfc->rc_fatal_cb == NULL)
		m0_rconfc_fatal_cb_set(rconfc, rconfc_ut_fatal_cb);
	else /* installed previously */
		M0_UT_ASSERT(rconfc->rc_fatal_cb == rconfc_ut_fatal_cb);
	m0_rconfc_unlock(rconfc);
	/* do nothing as if HA client went dead */
	return true;
}

/*
 * The test is to check timeout expiration during rconfc synchronous start with
 * limited timeout. As well, fatal callback passage is checked due to rconfc
 * failure. The callback is installed in the course of rconfc start.
 */
static void test_fail_abort(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);

	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	/*
	 * Suppress ha entrypoint response to cause timeout. Imitation of not
	 * responding HA.
	 */
	m0_semaphore_init(&g_fatal_sem, 0);
	ha_clink_cb_orig = rconfc.rc_ha_entrypoint_cl.cl_cb;
	rconfc.rc_ha_entrypoint_cl.cl_cb = ha_clink_cb_suppress;
	rc = m0_rconfc_start_wait(&rconfc, 3ULL * M0_TIME_ONE_SECOND);
	m0_semaphore_down(&g_fatal_sem);
	m0_semaphore_fini(&g_fatal_sem);
	M0_UT_ASSERT(rc == -ETIMEDOUT);
	M0_UT_ASSERT(rconfc.rc_sm_state_on_abort == M0_RCS_ENTRYPOINT_WAIT);
	M0_UT_ASSERT(rconfc.rc_ha_entrypoint_retries == 0);
	/* restore original callback */
	rconfc.rc_ha_entrypoint_cl.cl_cb = ha_clink_cb_orig;
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	rconfc_ut_mero_stop(&mach, &rctx);
}

static char *suffix_subst(const char *src, char delim, const char *suffix)
{
	const size_t len = strlen(src) + 1 + strlen(suffix) + 1;
	char        *s;
	char        *p;

	s = m0_alloc(len);
	M0_ASSERT(s != NULL);
	strncpy(s, src, len);
	p = strrchr(s, delim);
	M0_ASSERT(p != NULL);
	strncpy(p, suffix, strlen(suffix) + 1);
	return s;
}

static bool ha_clink_cb_bad_rm(struct m0_clink *clink)
{
	struct m0_rconfc               *rconfc = M0_AMB(rconfc, clink,
							rc_ha_entrypoint_cl);
	struct m0_ha_entrypoint_client *ecl =
		&m0_get()->i_ha->h_entrypoint_client;

	M0_PRE(rconfc->rc_sm.sm_state == M0_RCS_ENTRYPOINT_WAIT);
        if (m0_ha_entrypoint_client_state_get(ecl) == M0_HEC_AVAILABLE &&
	    ecl->ecl_rep.hae_control != M0_HA_ENTRYPOINT_QUERY) {
		char       *rm_addr = ecl->ecl_rep.hae_active_rm_ep;
		char       *rm_fake = NULL;
		static bool do_fake = true;
		/*
		 * The test is to fake the rm addr only once to provide that the
		 * next time correct rm addr reaches rconfc non-distorted, which
		 * guarantees successful connection to RM.
		 */
		if (do_fake) {
			rm_fake = suffix_subst(rm_addr, ':', ":999");
			ecl->ecl_rep.hae_active_rm_ep = rm_fake;
			do_fake = false; /* not next time */
		}
		/*
		 * call original handler to let rconfc copy this version of
		 * response and go on with connection
		 */
		ha_clink_cb_orig(clink);
		/*
		 * get real active rm address back to entrypoint response
		 */
		ecl->ecl_rep.hae_active_rm_ep = rm_addr;
		m0_free(rm_fake);
	}
	return true;
}

static void test_fail_retry_rm(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);

	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	/*
	 * Distort ha entrypoint response (active rm) to cause HA request
	 * retry. Imitation of the situation when HA reports dead active RM
	 * endpoint and later reports a connectable one.
	 */
	ha_clink_cb_orig = rconfc.rc_ha_entrypoint_cl.cl_cb;
	rconfc.rc_ha_entrypoint_cl.cl_cb = ha_clink_cb_bad_rm;
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rconfc.rc_ha_entrypoint_retries > 0);
	/* restore original callback */
	rconfc.rc_ha_entrypoint_cl.cl_cb = ha_clink_cb_orig;
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	rconfc_ut_mero_stop(&mach, &rctx);
}

static bool ha_clink_cb_bad_confd(struct m0_clink *clink)
{
	struct m0_rconfc               *rconfc = M0_AMB(rconfc, clink,
							rc_ha_entrypoint_cl);
	struct m0_ha_entrypoint_client *ecl =
		&m0_get()->i_ha->h_entrypoint_client;

	M0_PRE(rconfc->rc_sm.sm_state == M0_RCS_ENTRYPOINT_WAIT);
        if (m0_ha_entrypoint_client_state_get(ecl) == M0_HEC_AVAILABLE &&
	    ecl->ecl_rep.hae_control == M0_HA_ENTRYPOINT_CONSUME) {
		const char *confd_addr = ecl->ecl_rep.hae_confd_eps[0];
		char       *confd_fake = NULL;
		static bool do_fake = true;
		/*
		 * The test is to fake the confd addr only once to provide that
		 * the next time correct confd addr reaches rconfc
		 * non-distorted, which guarantees successful connection to it.
		 */
		if (do_fake) {
			do_fake = false;
			confd_fake = suffix_subst(confd_addr, ':', ":999");
			*(char **)&ecl->ecl_rep.hae_confd_eps[0] = confd_fake;
		}
		/*
		 * call original handler to let rconfc copy this version of
		 * response and go on with connection
		 */
		ha_clink_cb_orig(clink);
		/*
		 * get real active rm address back to entrypoint response
		 */
		ecl->ecl_rep.hae_confd_eps[0] = confd_addr;
		m0_free(confd_fake);
	}
	return true;
}

static void test_fail_retry_confd(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);

	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	/*
	 * Distort ha entrypoint response (confd) to cause HA request
	 * retry. Imitation of the situation when HA reports dead confd
	 * endpoint and later reports a connectable one.
	 */
	ha_clink_cb_orig = rconfc.rc_ha_entrypoint_cl.cl_cb;
	rconfc.rc_ha_entrypoint_cl.cl_cb = ha_clink_cb_bad_confd;
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rconfc.rc_ha_entrypoint_retries > 0);
	/* restore original callback */
	rconfc.rc_ha_entrypoint_cl.cl_cb = ha_clink_cb_orig;
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	rconfc_ut_mero_stop(&mach, &rctx);
}

static void _stop_rms(struct m0_rpc_machine *rmach)
{
	struct m0_reqh_service *service;
	struct m0_reqh         *reqh = rmach->rm_reqh;
	struct m0_fid           rm_fid = M0_FID_TINIT('s', 1, 2);

	service = m0_reqh_service_lookup(reqh, &rm_fid);
	M0_UT_ASSERT(service != NULL);
	M0_UT_ASSERT(m0_streq(service->rs_type->rst_name, "M0_CST_RMS"));
	m0_reqh_service_prepare_to_stop(service);
	m0_reqh_idle_wait_for(reqh, service);
	m0_reqh_service_stop(service);
}

static void test_no_rms(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);

	/* quit on command from HA */
	m0_fi_enable_once("mero_ha_entrypoint_rep_rm_fill", "no_rms_fid");
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == -EPERM);
	/* see if we can stop after start failure */
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	/* start with rms up and running */
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	/* see if we can stop when there is no rms around */
	_stop_rms(&mach);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	/* repeat with no rms around */
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == -ECONNREFUSED);
	/* see if we can stop after start failure */
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	rconfc_ut_mero_stop(&mach, &rctx);
}

static void rconfc_ut_ha_state_set(const struct m0_fid *fid, uint32_t state)
{
	struct m0_ha_note note = { .no_id = *fid, .no_state = state };
	struct m0_ha_nvec nvec = { .nv_nr = 1, .nv_note = &note };

	m0_ha_state_set(&nvec);
}

static struct m0_semaphore sem_death;
static bool expected_fom_queued_value;

M0_TL_DESCR_DECLARE(rpc_conn, M0_EXTERN);
M0_TL_DECLARE(rpc_conn, M0_INTERNAL, struct m0_rpc_conn);

static void _on_death_cb(struct rconfc_link *lnk)
{
	M0_UT_ASSERT(m0_mutex_is_locked(&lnk->rl_rconfc->rc_herd_lock));
	if (lnk->rl_fom_queued) {
		M0_UT_ASSERT(lnk->rl_state == CONFC_DEAD);
		/* herd link confc not connected */
		M0_UT_ASSERT(!m0_confc_is_online(&lnk->rl_confc));
		/* herd link confc uninitialised */
		M0_UT_ASSERT(!m0_confc_is_inited(&lnk->rl_confc));
	}
	if (lnk->rl_fom_queued == expected_fom_queued_value) {
		M0_LOG(M0_DEBUG, "Done %s waiting for FOM fini",
		       lnk->rl_fom_queued ? "after" : "with no");
		m0_semaphore_up(&sem_death);
	}
}

M0_TL_DESCR_DECLARE(rcnf_herd, M0_EXTERN);
M0_TL_DECLARE(rcnf_herd, M0_INTERNAL, struct rconfc_link);

enum ut_confc_control {
	UT_CC_KEEP_AS_IS,
	UT_CC_DISCONNECT,
	UT_CC_DEINITIALISE,
};

static void on_death_cb_install(struct m0_rconfc     *rconfc,
				enum ut_confc_control ctrl)
{
	struct rconfc_link *lnk;

	m0_tl_for(rcnf_herd, &rconfc->rc_herd, lnk) {
		lnk->rl_on_state_cb = _on_death_cb;
		switch (ctrl) {
		case UT_CC_KEEP_AS_IS:
			break;
		case UT_CC_DISCONNECT:
			m0_confc_reconnect(&lnk->rl_confc, NULL, NULL);
			break;
		case UT_CC_DEINITIALISE:
			m0_confc_reconnect(&lnk->rl_confc, NULL, NULL);
			m0_confc_fini(&lnk->rl_confc);
			break;
		}
	} m0_tl_endfor;
}

/*
 * The test is to verify rconfc_herd_link__on_death_cb() passage as well as
 * ability for rconfc to stop while having herd link dead.
 *
 * Rconfc gets successfully started with a single confd in conf. Later the confd
 * is announced M0_NC_FAILED that invokes the corresponding herd link callback.
 * When notification is successfully processed, rconfc is tested for shutting
 * down with the herd link being in CONFC_DEAD state.
 *
 * As the death notification may arrive when the link's confc can be in any
 * state, all the states (online, disconnected, finalised) must be tested for
 * safe going down.
 */
static void test_dead_down(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;
	struct m0_fid            confd_fid = M0_FID_TINIT('s', 1, 6);

	/*
	 * The tests below are going to wait for FOM being queued and finalised
	 * regular way.
	 */
	expected_fom_queued_value = true;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);

	/* 1. Normal case */
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_ONLINE);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	m0_rconfc_lock(&rconfc);
	on_death_cb_install(&rconfc, UT_CC_KEEP_AS_IS);
	m0_rconfc_unlock(&rconfc);
	m0_semaphore_init(&sem_death, 0);
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_FAILED);
	m0_semaphore_down(&sem_death);
	m0_semaphore_fini(&sem_death);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	/* 2. Notification jitters */
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_ONLINE);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	m0_rconfc_lock(&rconfc);
	on_death_cb_install(&rconfc, UT_CC_KEEP_AS_IS);
	m0_rconfc_unlock(&rconfc);
	m0_semaphore_init(&sem_death, 0);
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_FAILED); /* legal tap  */
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_FAILED); /* double tap */
	m0_semaphore_down(&sem_death);
	m0_semaphore_fini(&sem_death);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	/* 3. Deal with already disconnected confc */
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_ONLINE);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	m0_rconfc_lock(&rconfc);
	on_death_cb_install(&rconfc, UT_CC_DISCONNECT);
	m0_rconfc_unlock(&rconfc);
	m0_semaphore_init(&sem_death, 0);
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_FAILED);
	m0_semaphore_down(&sem_death);
	m0_semaphore_fini(&sem_death);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	/* 4. Deal with already finalised confc */
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_ONLINE);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	m0_rconfc_lock(&rconfc);
	on_death_cb_install(&rconfc, UT_CC_DEINITIALISE);
	m0_rconfc_unlock(&rconfc);
	m0_semaphore_init(&sem_death, 0);
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_FAILED);
	m0_semaphore_down(&sem_death);
	m0_semaphore_fini(&sem_death);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	/*
	 * Test error paths in fom tick
	 *
	 * 5. Survive from session failure
	 */
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_ONLINE);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	m0_rconfc_lock(&rconfc);
	on_death_cb_install(&rconfc, UT_CC_KEEP_AS_IS);
	m0_rconfc_unlock(&rconfc);
	m0_semaphore_init(&sem_death, 0);
	m0_fi_enable("rconfc_link_fom_tick", "sess_fail");
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_FAILED);
	m0_semaphore_down(&sem_death);
	m0_semaphore_fini(&sem_death);
	m0_fi_disable("rconfc_link_fom_tick", "sess_fail");
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	/* 6. Survive from connection failure */
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_ONLINE);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	m0_rconfc_lock(&rconfc);
	on_death_cb_install(&rconfc, UT_CC_KEEP_AS_IS);
	m0_rconfc_unlock(&rconfc);
	m0_semaphore_init(&sem_death, 0);
	m0_fi_enable("rconfc_link_fom_tick", "conn_fail");
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_FAILED);
	m0_semaphore_down(&sem_death);
	m0_semaphore_fini(&sem_death);
	m0_fi_disable("rconfc_link_fom_tick", "conn_fail");
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	/* 7. Survive from both session and connection failures */
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_ONLINE);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	m0_rconfc_lock(&rconfc);
	on_death_cb_install(&rconfc, UT_CC_KEEP_AS_IS);
	m0_rconfc_unlock(&rconfc);
	m0_semaphore_init(&sem_death, 0);
	m0_fi_enable("rconfc_link_fom_tick", "sess_fail");
	m0_fi_enable("rconfc_link_fom_tick", "conn_fail");
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_FAILED);
	m0_semaphore_down(&sem_death);
	m0_semaphore_fini(&sem_death);
	m0_fi_disable("rconfc_link_fom_tick", "conn_fail");
	m0_fi_disable("rconfc_link_fom_tick", "sess_fail");
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	rconfc_ut_mero_stop(&mach, &rctx);
}

/*
 * The test is to verify rconfc_herd_link__on_death_cb() passing safe being
 * concurrent with rconfc stopping.
 */
static void test_dead_stop(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;
	struct m0_fid            confd_fid = M0_FID_TINIT('s', 1, 6);

	/*
	 * Run link FOM concurrent with stopping.
	 *
	 * The concurrent execution is achieved by releasing sem_dead right
	 * before queueing FOM in rconfc_herd_link__on_death_cb().
	 */
	expected_fom_queued_value = false;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);

	rconfc_ut_ha_state_set(&confd_fid, M0_NC_ONLINE);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	m0_rconfc_lock(&rconfc);
	on_death_cb_install(&rconfc, UT_CC_KEEP_AS_IS);
	m0_rconfc_unlock(&rconfc);
	m0_semaphore_init(&sem_death, 0);
	rconfc_ut_ha_state_set(&confd_fid, M0_NC_FAILED);
	m0_semaphore_down(&sem_death);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);
	m0_semaphore_fini(&sem_death);

	rconfc_ut_mero_stop(&mach, &rctx);
}

static void test_reading(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;
	uint64_t                 ver;
	struct m0_conf_obj      *cobj;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	ver = m0_rconfc_ver_max_read(&rconfc);
	M0_UT_ASSERT(ver == rconfc.rc_ver);
	/* do regular path opening */
	rc = m0_confc_open_sync(&cobj, rconfc.rc_confc.cc_root,
				M0_CONF_ROOT_PROFILES_FID,
				m0_ut_conf_fids[M0_UT_CONF_PROF]);
	M0_UT_ASSERT(rc == 0);
	m0_confc_close(cobj);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);
	rconfc_ut_mero_stop(&mach, &rctx);
}

static bool quorum_impossible_clink_cb(struct m0_clink *cl)
{
	struct m0_rconfc *rconfc = container_of(cl->cl_chan, struct m0_rconfc,
						rc_sm.sm_chan);
	static bool       do_override = true;

	if (do_override && rconfc->rc_sm.sm_state == M0_RCS_GET_RLOCK) {
		/*
		 * Override required quorum value to be greater then number of
		 * confd, so quorum is impossible.
		 */
		M0_PRE(rconfc->rc_quorum != 0);
		rconfc->rc_quorum *= 2;
		m0_clink_del(cl);
		/*
		 * Overridden once, next time entrypoint re-tried it must remain
		 * untouched to let rconfc start successfully.
		 */
		do_override = false;
	}
	return true;
}

static void test_quorum_impossible(void)
{
	struct m0_rconfc         rconfc;
	struct m0_rpc_machine    mach;
	int                      rc;
	uint64_t                 ver;
	struct m0_rpc_server_ctx rctx;
	struct m0_clink          clink;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	/*
	 * Have the number of online herd links less than required to reach the
	 * quorum. Once failed, next re-election succeeds.
	 */
	M0_UT_ASSERT(rc == 0);
	M0_SET0(&rconfc);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	m0_clink_init(&clink, quorum_impossible_clink_cb);
	m0_clink_add_lock(&rconfc.rc_sm.sm_chan, &clink);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	ver = m0_rconfc_ver_max_read(&rconfc);
	M0_UT_ASSERT(ver != M0_CONF_VER_UNKNOWN);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);
	m0_clink_fini(&clink);
	rconfc_ut_mero_stop(&mach, &rctx);
}

static void test_quorum_retry(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;
	uint64_t                 ver;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);

	/*
	 * 1. Have herd link online, but fetching bad version number. Once
	 * failed, next re-election succeeds.
	 */
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rconfc.rc_ha_entrypoint_retries == 0);
	m0_fi_enable_once("rconfc__cb_quorum_test", "read_ver_failed");
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rconfc.rc_ver != 0);
	ver = m0_rconfc_ver_max_read(&rconfc);
	M0_UT_ASSERT(ver == rconfc.rc_ver);
	M0_UT_ASSERT(rconfc.rc_ha_entrypoint_retries > 0);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	/*
	 * 2. Have all herd links CONFC_DEAD. Once failed, next re-election
	 * succeeds.
	 */
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rconfc.rc_ha_entrypoint_retries == 0);
	m0_fi_enable_once("rconfc_herd_link_init", "confc_init");
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rconfc.rc_ver != 0);
	ver = m0_rconfc_ver_max_read(&rconfc);
	M0_UT_ASSERT(ver == rconfc.rc_ver);
	M0_UT_ASSERT(rconfc.rc_ha_entrypoint_retries > 0);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	/*
	 * 3. Have conductor failed to engage. Once failed, next re-election
	 * succeeds.
	 */
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rconfc.rc_ha_entrypoint_retries == 0);
	m0_fi_enable_once("rconfc_conductor_iterate", "conductor_conn_fail");
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rconfc.rc_ver != 0);
	ver = m0_rconfc_ver_max_read(&rconfc);
	M0_UT_ASSERT(ver == rconfc.rc_ver);
	M0_UT_ASSERT(rconfc.rc_ha_entrypoint_retries > 0);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);

	rconfc_ut_mero_stop(&mach, &rctx);
}

struct m0_semaphore gops_sem;

static void test_gops(void)
{
	struct m0_rconfc         rconfc;
	struct m0_rpc_machine    mach;
	int                      rc;
	struct m0_rpc_server_ctx rctx;
	bool                     check_res;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);
	M0_SET0(&rconfc);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);

	/* imitate check op */
	m0_mutex_lock(&rconfc.rc_confc.cc_lock);
	check_res = rconfc.rc_gops.go_check(&rconfc.rc_confc);
	M0_UT_ASSERT(check_res == true);
	m0_mutex_unlock(&rconfc.rc_confc.cc_lock);

	/* imitate skip op */
	rc = rconfc.rc_gops.go_skip(&rconfc.rc_confc);
	M0_UT_ASSERT(rc == -ENOENT);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);
	rconfc_ut_mero_stop(&mach, &rctx);
	m0_semaphore_fini(&gops_sem);
}

static void update_confd_version(struct m0_rpc_server_ctx *rctx,
				 uint64_t                  new_ver)
{
	struct m0_reqh         *reqh;
	struct m0_reqh_service *svc;
	struct m0_confd        *confd = NULL;
	struct m0_conf_cache   *cc;
	struct m0_conf_root    *root;

	/* Find confd instance through corresponding confd service */
	reqh = &rctx->rsx_mero_ctx.cc_reqh_ctx.rc_reqh;
	m0_tl_for(m0_reqh_svc, &reqh->rh_services, svc) {
		if (svc->rs_type == &m0_confd_stype) {
			confd = container_of(svc, struct m0_confd, d_reqh);
			break;
		}
	} m0_tl_endfor;
	M0_UT_ASSERT(confd != NULL);

	cc = confd->d_cache;
	cc->ca_ver = new_ver;
	root = M0_CONF_CAST(m0_conf_cache_lookup(cc, &M0_CONF_ROOT_FID),
			    m0_conf_root);
	M0_UT_ASSERT(root != NULL);
	root->rt_verno = new_ver;
	root->rt_mdredundancy = 51212;
}

static void test_version_change(void)
{
	struct m0_rconfc         rconfc;
	struct m0_rpc_machine    mach;
	int                      rc;
	struct m0_rpc_server_ctx rctx;
	struct rlock_ctx        *rlx;
	struct m0_conf_obj      *root_obj;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);
	m0_semaphore_init(&g_ready_sem, 0);

	M0_SET0(&rconfc);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach,
			    conflict_exp_cb, conflict_ready_cb);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);

	/* Check that version 1 in use */
	M0_UT_ASSERT(rconfc.rc_ver == 1);
	rc = m0_confc_open_sync(&root_obj, rconfc.rc_confc.cc_root, M0_FID0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(M0_CONF_CAST(root_obj, m0_conf_root)->rt_verno == 1);
	m0_confc_close(root_obj);

	/* Update conf DB version and immitate read lock conflict */
	update_confd_version(&rctx, 2);
	rlx = rconfc.rc_rlock_ctx;
	m0_rconfc_ri_ops.rio_conflict(&rlx->rlc_req);

	/* Wait till version reelection is finished */
	m0_semaphore_down(&g_ready_sem);
	m0_sm_group_lock(rconfc.rc_sm.sm_grp);
	m0_sm_timedwait(&rconfc.rc_sm, M0_BITS(M0_RCS_IDLE, M0_RCS_FAILURE),
			M0_TIME_NEVER);
	m0_sm_group_unlock(rconfc.rc_sm.sm_grp);
	M0_UT_ASSERT(rconfc.rc_sm.sm_state == M0_RCS_IDLE);

	/* Check that version in use is 2 */
	M0_UT_ASSERT(rconfc.rc_ver == 2);
	rc = m0_confc_open_sync(&root_obj, rconfc.rc_confc.cc_root, M0_FID0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(M0_CONF_CAST(root_obj, m0_conf_root)->rt_verno == 2);
	m0_confc_close(root_obj);

	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);
	m0_semaphore_fini(&g_ready_sem);
	rconfc_ut_mero_stop(&mach, &rctx);
}

static void test_cache_drop(void)
{
	struct m0_rconfc         rconfc;
	struct m0_rpc_machine    mach;
	int                      rc;
	struct m0_rpc_server_ctx rctx;
	struct rlock_ctx        *rlx;
	struct m0_conf_obj      *root_obj;
	struct m0_confc         *confc;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);
	m0_semaphore_init(&g_expired_sem, 0);
	M0_SET0(&rconfc);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach,
			    conflict_exp_cb, NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	/* Open root conf object */
	confc = &rconfc.rc_confc;
	rc = m0_confc_open_sync(&root_obj, confc->cc_root, M0_FID0);
	M0_UT_ASSERT(rc == 0);
	/*
	 * Imitate conflict for read lock, so rconfc asks its
	 * user to put all opened conf objects.
	 */
	rlx = rconfc.rc_rlock_ctx;
	m0_rconfc_ri_ops.rio_conflict(&rlx->rlc_req);
	m0_semaphore_down(&g_expired_sem);
	/* Sleep to make sure rconfc wait for us */
	while (usleep(200) == -1);
	/*
	 * Close root conf object, expecting rconfc to release
	 * read lock and start reelection process.
	 */
	m0_confc_close(root_obj);
	m0_semaphore_fini(&g_expired_sem);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);
	rconfc_ut_mero_stop(&mach, &rctx);
}

static void test_confc_ctx_block(void)
{
	struct m0_rconfc         rconfc;
	struct m0_rpc_machine    mach;
	int                      rc;
	struct m0_rpc_server_ctx rctx;
	struct rlock_ctx        *rlx;
	struct m0_confc_ctx      confc_ctx;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);
	M0_SET0(&rconfc);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	rlx = rconfc.rc_rlock_ctx;
	m0_rconfc_ri_ops.rio_conflict(&rlx->rlc_req);

	m0_confc_ctx_init(&confc_ctx, &rconfc.rc_confc);
	m0_confc_ctx_fini(&confc_ctx);

	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);
	rconfc_ut_mero_stop(&mach, &rctx);
}

static int _skip(struct m0_confc *confc)
{
	m0_fi_disable("on_replied", "fail_rpc_reply");
	return m0_rconfc_gate_ops.go_skip(confc);
}

static void test_reconnect_success(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;
	uint64_t                 ver;
	struct m0_conf_obj      *cobj;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	ver = m0_rconfc_ver_max_read(&rconfc);
	M0_UT_ASSERT(ver == rconfc.rc_ver);
	/* imitate successful reconnection */
	m0_fi_enable_off_n_on_m("skip_confd_st_in", "force_reconnect_success",
				0, 1);
	m0_fi_enable_off_n_on_m("on_replied", "fail_rpc_reply", 0, 1);
	m0_rconfc_lock(&rconfc);
	rconfc.rc_gops.go_skip = _skip;
	m0_rconfc_unlock(&rconfc);
	/* do regular path opening with disconnected confc */
	rc = m0_confc_open_sync(&cobj, rconfc.rc_confc.cc_root,
				M0_CONF_ROOT_PROFILES_FID,
				m0_ut_conf_fids[M0_UT_CONF_PROF]);
	M0_UT_ASSERT(rc == 0);
	m0_confc_close(cobj);
	m0_fi_disable("skip_confd_st_in", "force_reconnect_success");
	m0_fi_disable("on_replied", "fail_rpc_reply");
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);
	rconfc_ut_mero_stop(&mach, &rctx);
}

static void _subscribe_to_service(struct m0_rconfc *rconfc,
				  struct m0_fid    *fid,
				  struct m0_clink  *clink)
{
	struct m0_conf_obj   *obj;
	struct m0_confc      *phony = &rconfc->rc_phony;
	struct m0_conf_cache *cache = &phony->cc_cache;

	obj = m0_conf_cache_lookup(cache, fid);
	M0_UT_ASSERT(obj != NULL);

	m0_clink_add_lock(&obj->co_ha_chan, clink);
}

struct _ha_notify_ctx {
	struct m0_clink     clink;
	struct m0_semaphore sem;
	struct m0_fid       fid;
};

static void _notify_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct _ha_notify_ctx *x    = ast->sa_datum;
	struct m0_ha_note      n1[] = { { x->fid, M0_NC_FAILED } };
	struct m0_ha_nvec      nvec = { ARRAY_SIZE(n1), n1 };

	m0_ha_state_accept(&nvec, false);
}

static bool _clink_cb(struct m0_clink *link)
{
	struct _ha_notify_ctx *x =
		container_of(link, struct _ha_notify_ctx, clink);
	struct m0_conf_obj    *obj =
		container_of(link->cl_chan, struct m0_conf_obj, co_ha_chan);

	/* now make sure the signal came from the right object ... */
	M0_UT_ASSERT(m0_fid_eq(&x->fid, &obj->co_id));
	M0_UT_ASSERT(obj->co_ha_state == M0_NC_FAILED);
	/* ... and let the test move on */
	m0_semaphore_up(&x->sem);
	return false;
}

M0_UNUSED static void test_ha_notify(void)
{
	struct m0_rpc_machine    mach;
	struct m0_rpc_server_ctx rctx;
	int                      rc;
	struct m0_rconfc         rconfc;
	struct m0_fid            rm_fid = M0_FID_TINIT('s', 1, 2);
	struct m0_sm_ast         notify_ast = {0};
	struct _ha_notify_ctx    hnx;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_init(&rconfc, &profile, &m0_conf_ut_grp, &mach, NULL,
			    NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(&rconfc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rconfc.rc_ver != 0);

	/* make sure rconfc is ready */
	M0_UT_ASSERT(rconfc.rc_sm.sm_state == M0_RCS_IDLE);
	/* prepare notification context */
	m0_semaphore_init(&hnx.sem, 0);
	m0_clink_init(&hnx.clink, _clink_cb);
	_subscribe_to_service(&rconfc, &rm_fid, &hnx.clink);
	hnx.fid = rm_fid;

	/* imitate HA note arrived from outside */
	notify_ast.sa_datum = &hnx;
	notify_ast.sa_cb = _notify_cb;
	m0_sm_ast_post(&m0_conf_ut_grp, &notify_ast);

	/* now wait for notification fired ... */
	m0_semaphore_down(&hnx.sem);
	m0_semaphore_fini(&hnx.sem);
	/* ... unsubscribe ... */
	m0_clink_del_lock(&hnx.clink);
	m0_clink_fini(&hnx.clink);
	/* ... and leave */
	m0_rconfc_lock(&rconfc);
	m0_sm_timedwait(&rconfc.rc_sm, M0_BITS(M0_RCS_FAILURE), M0_TIME_NEVER);
	m0_rconfc_unlock(&rconfc);
	m0_rconfc_stop_sync(&rconfc);
	m0_rconfc_fini(&rconfc);
	/*
	 * rconfc RM owner didn't return credits to creditor and now
	 * this owner is finalised. Drop this loan on creditor side.
	 */
	m0_fi_enable_once("owner_finalisation_check", "drop_loans");
	rconfc_ut_mero_stop(&mach, &rctx);
}

struct m0_fid drain_fs_fid = M0_FID0;

static void drain_expired_cb(struct m0_rconfc *rconfc)
{
	struct m0_conf_cache *cache = &rconfc->rc_confc.cc_cache;
	struct m0_conf_obj   *obj;

	M0_UT_ENTER();
	if (m0_fid_is_set(&drain_fs_fid)) {
		obj = m0_conf_cache_lookup(cache, &drain_fs_fid);
		M0_UT_ASSERT(obj != NULL);
		m0_confc_close(obj);
	}
	M0_UT_RETURN();
}

static bool fs_expired(struct m0_clink *clink)
{
	struct root_object *root_obj =
		container_of(clink, struct root_object, clink_x);
	struct m0_rconfc *rconfc =
		container_of(root_obj->confc, struct m0_rconfc, rc_confc);

	M0_UT_ENTER();
	drain_expired_cb(rconfc);
	M0_UT_RETURN();

	return true;
}

static void drain_ready_cb(struct m0_rconfc *rconfc)
{
	M0_UT_ENTER();
	if (m0_fid_is_set(&drain_fs_fid))
		m0_semaphore_up(&g_ready_sem);
	M0_UT_RETURN();
}

static bool fs_ready(struct m0_clink *clink)
{
	struct root_object *root_obj =
		container_of(clink, struct root_object, clink_r);
	struct m0_rconfc *rconfc =
		container_of(root_obj->confc, struct m0_rconfc, rc_confc);

	M0_UT_ENTER();
	drain_ready_cb(rconfc);
	M0_UT_RETURN();

	return true;
}

static void test_drain(void)
{
	struct m0_rpc_machine    mach;
	int                      rc;
	struct m0_rpc_server_ctx rctx;
	struct rlock_ctx        *rlx;
	struct m0_rconfc        *rconfc;
	struct root_object       root_obj;

	rc = rconfc_ut_mero_start(&mach, &rctx);
	M0_UT_ASSERT(rc == 0);
	m0_semaphore_init(&g_ready_sem, 0);

	rc = m0_net_domain_init(&client_net_dom, xprt);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_client_start(&cctx);
	M0_UT_ASSERT(rc == 0);

	M0_UT_LOG("\n\n\t@reqh %p", &cctx.rcx_reqh);
	rconfc = &cctx.rcx_reqh.rh_rconfc;
	rc = m0_rconfc_init(rconfc, &profile, &m0_conf_ut_grp, &mach,
			    m0_confc_expired_cb, m0_confc_ready_cb);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rconfc_start_sync(rconfc);
	M0_UT_ASSERT(rc == 0);

	rc = m0_confc_root_open(&rconfc->rc_confc, &root_obj.root);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(root_obj.root->rt_mdredundancy == 1);
	M0_UT_ASSERT(root_obj.root->rt_obj.co_nrefs != 0);
	drain_fs_fid = root_obj.root->rt_obj.co_id;
	/*
	 * Here we intentionally leave root object pinned simulating working
	 * rconfc environment. It is expected to be closed later during expired
	 * callback processing being found by fs_fid (see drain_expired_cb()).
	 */

	root_obj.confc = &rconfc->rc_confc;
	m0_clink_init(&root_obj.clink_x, fs_expired);
	m0_clink_init(&root_obj.clink_r, fs_ready);
	m0_clink_add_lock(&cctx.rcx_reqh.rh_conf_cache_exp, &root_obj.clink_x);
	m0_clink_add_lock(&cctx.rcx_reqh.rh_conf_cache_ready, &root_obj.clink_r);

	/* Update conf DB version and immitate read lock conflict */
	update_confd_version(&rctx, 2);
	rlx = rconfc->rc_rlock_ctx;
	m0_rconfc_ri_ops.rio_conflict(&rlx->rlc_req);

	/* Wait till version reelection is finished */
	m0_semaphore_down(&g_ready_sem);
	/* Here we are to disable waiting for ready as not needed anymore */
	drain_fs_fid = M0_FID0;

	m0_sm_group_lock(rconfc->rc_sm.sm_grp);
	m0_sm_timedwait(&rconfc->rc_sm, M0_BITS(M0_RCS_IDLE, M0_RCS_FAILURE),
			M0_TIME_NEVER);
	m0_sm_group_unlock(rconfc->rc_sm.sm_grp);
	M0_UT_ASSERT(rconfc->rc_sm.sm_state == M0_RCS_IDLE);

	rc = m0_confc_root_open(&rconfc->rc_confc, &root_obj.root);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(root_obj.root->rt_mdredundancy == 51212);
	m0_confc_close(&root_obj.root->rt_obj);

	m0_clink_del_lock(&root_obj.clink_x);
	m0_clink_del_lock(&root_obj.clink_r);
	m0_clink_fini(&root_obj.clink_x);
	m0_clink_fini(&root_obj.clink_r);
	m0_rconfc_stop_sync(rconfc);
	m0_rconfc_fini(rconfc);
	m0_rpc_client_stop(&cctx);
	m0_semaphore_fini(&g_ready_sem);
	rconfc_ut_mero_stop(&mach, &rctx);
}

static int rconfc_ut_init(void)
{
	return m0_conf_ut_ast_thread_init();
}

static int rconfc_ut_fini(void)
{
	return m0_conf_ut_ast_thread_fini();
}

struct m0_ut_suite rconfc_ut = {
	.ts_name  = "rconfc-ut",
	.ts_init  = rconfc_ut_init,
	.ts_fini  = rconfc_ut_fini,
	.ts_tests = {
		{ "init-fini",        test_init_fini },
		{ "start-stop",       test_start_stop },
		{ "local-conf",       test_start_stop_local },
		{ "local-load-fail",  test_local_load_fail },
		{ "start-fail",       test_start_failures },
		{ "fail-abort",       test_fail_abort },
		{ "fail-retry-rm",    test_fail_retry_rm },
		{ "fail-retry-confd", test_fail_retry_confd },
		{ "no-rms",           test_no_rms },
		{ "dead-down",        test_dead_down },
		{ "dead-stop",        test_dead_stop },
		{ "reading",          test_reading },
		{ "impossible",       test_quorum_impossible },
		{ "quorum-retry",     test_quorum_retry },
		{ "gate-ops",         test_gops },
		{ "change-ver",       test_version_change },
		{ "cache-drop",       test_cache_drop },
		{ "ctx-block",        test_confc_ctx_block },
		{ "reconnect",        test_reconnect_success },
		/*
		 * Temporary disabled because now rconfc entrypoint request
		 * can't fail. Will be fixed somehow in MERO-1774 patch.
		 */
		/* { "ha-notify",  test_ha_notify }, */
		{ "test-drain", test_drain },
		{ NULL, NULL }
	}
};

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
