/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Atsuro Hoshino <atsuro_hoshino@xyratex.com>
 * Original creation date: 19-Sep-2013
 */

#include "ha/note.h"
#include "conf/pvers.h"     /* m0_conf_pver_kind */
#include "conf/obj_ops.h"   /* m0_conf_obj_find_lock */
#include "conf/helpers.h"   /* m0_conf_full_load */
#include "rpc/rpclib.h"     /* m0_rpc_client_ctx */
#include "net/lnet/lnet.h"  /* m0_net_lnet_xprt */

#include "ha/note.c"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "lib/fs.h"         /* m0_file_read */
#include "lib/finject.h"    /* m0_fi_enable/disable */
#include "ut/misc.h"        /* M0_UT_PATH */
#include "ut/ut.h"

#define CLIENT_DB_NAME        "ha_ut_client.db"
#define CLIENT_ENDPOINT_ADDR  "0@lo:12345:34:*"

#define SERVER_DB_NAME        "ha_ut_confd_server.db"
#define SERVER_STOB_NAME      "ha_ut_confd_server.stob"
#define SERVER_ADDB_STOB_NAME "linuxstob:ha_ut_confd_server.addb_stob"
#define SERVER_LOG_NAME       "ha_ut_confd_server.log"
#define SERVER_ENDPOINT_ADDR  "0@lo:12345:34:1"
#define SERVER_ENDPOINT       "lnet:" SERVER_ENDPOINT_ADDR

static struct m0_net_xprt    *xprt = &m0_net_lnet_xprt;
static struct m0_net_domain   client_net_dom;
struct m0_conf_root          *root;

enum {
	CLIENT_COB_DOM_ID  = 16,
	SESSION_SLOTS      = 1,
	MAX_RPCS_IN_FLIGHT = 1,
	NR_EQUEUE_LENGTH   = 5,
	NR_FAILURES        = 10,
	NR_OVERLAP         = 5,
	NR_NO_OVERLAP      = 10,
	NR_NODES           = 10,
	NR_DISKS           = 200,
	MAX_FAILURES       = 20,
};

static struct m0_rpc_client_ctx cctx = {
	.rcx_net_dom            = &client_net_dom,
	.rcx_local_addr         = CLIENT_ENDPOINT_ADDR,
	.rcx_remote_addr        = SERVER_ENDPOINT_ADDR,
	.rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT,
	.rcx_fid                = &g_process_fid,
};

static char *server_argv[] = {
	"ha_ut", "-T", "AD", "-D", SERVER_DB_NAME,
	"-S", SERVER_STOB_NAME, "-A", SERVER_ADDB_STOB_NAME,
	"-w", "10", "-e", SERVER_ENDPOINT, "-H", SERVER_ENDPOINT_ADDR,
	"-f", M0_UT_CONF_PROCESS,
	"-c", M0_UT_PATH("conf.xc")
};

static struct m0_rpc_server_ctx sctx = {
	.rsx_xprts         = &xprt,
	.rsx_xprts_nr      = 1,
	.rsx_argv          = server_argv,
	.rsx_argc          = ARRAY_SIZE(server_argv),
	.rsx_log_file_name = SERVER_LOG_NAME,
};

static struct m0_reqh_init_args   reqh_args = {
	.rhia_fid = &M0_FID_TINIT('r', 1, 1),
	.rhia_mdstore =  (void *)1
};

extern struct m0_reqh_service_type m0_rpc_service_type;

static struct m0_mutex chan_lock;
static struct m0_chan  chan;
static struct m0_clink clink;

static struct m0_sm_group g_grp;

static struct {
	bool             run;
	struct m0_thread thread;
} g_ast;

/* ----------------------------------------------------------------
 * Auxiliary functions
 * ---------------------------------------------------------------- */

static void start_rpc_client_and_server(void)
{
	int rc;

	rc = m0_net_domain_init(&client_net_dom, xprt);
	M0_ASSERT(rc == 0);
	M0_SET0(&sctx.rsx_mero_ctx);
	rc = m0_rpc_server_start(&sctx);
	M0_ASSERT(rc == 0);
	rc = m0_rpc_client_start(&cctx);
	M0_ASSERT(rc == 0);
}

static void stop_rpc_client_and_server(void)
{
	int rc;

	rc = m0_rpc_client_stop(&cctx);
	M0_ASSERT(rc == 0);
	m0_rpc_server_stop(&sctx);
	m0_net_domain_fini(&client_net_dom);
}

static void ast_thread(int _ M0_UNUSED)
{
	while (g_ast.run) {
		m0_chan_wait(&g_grp.s_clink);
		m0_sm_group_lock(&g_grp);
		m0_sm_asts_run(&g_grp);
		m0_sm_group_unlock(&g_grp);
	}
}

static int ast_thread_init(void)
{
	m0_sm_group_init(&g_grp);
	g_ast.run = true;
	return M0_THREAD_INIT(&g_ast.thread, int, NULL, &ast_thread, 0,
			      "ast_thread");
}

static void ast_thread_fini(void)
{
	g_ast.run = false;
	m0_clink_signal(&g_grp.s_clink);
	m0_thread_join(&g_ast.thread);
	m0_thread_fini(&g_ast.thread);
	m0_sm_group_fini(&g_grp);
}

static void done_get_chan_init(void)
{
	m0_mutex_init(&chan_lock);
	m0_chan_init(&chan, &chan_lock);
	m0_clink_init(&clink, NULL);
	m0_clink_add_lock(&chan, &clink);
}

static void done_get_chan_fini(void)
{
	m0_clink_del_lock(&clink);
	m0_clink_fini(&clink);
	m0_chan_fini_lock(&chan);
	m0_mutex_fini(&chan_lock);
}

static void local_confc_init(struct m0_confc *confc)
{
	char *confstr;
	int   rc;

	rc = m0_file_read(M0_UT_PATH("conf.xc"), &confstr);
	M0_UT_ASSERT(rc == 0);
	/*
	 * All configuration objects need to be preloaded, since
	 * m0_ha_state_accept() function traverses all the descendants appearing
	 * in preloaded data. When missing configuration object is found,
	 * unit test for m0_ha_state_accept() will likely to show an error
	 * similar to below:
	 *
	 *  FATAL : [lib/assert.c:43:m0_panic] panic:
	 *    obj->co_status == M0_CS_READY m0_conf_obj_put()
	 */
	rc = m0_confc_init(confc, &g_grp, SERVER_ENDPOINT_ADDR,
			   &cctx.rcx_rpc_machine, confstr);
	M0_UT_ASSERT(rc == 0);
	m0_free(confstr);
}

static void compare_ha_state(struct m0_confc *confc,
			     enum m0_ha_obj_state state)
{
	struct m0_conf_obj *node_dir;
	struct m0_conf_obj *node;
	struct m0_conf_obj *svc_dir;
	struct m0_conf_obj *svc;
	int                 rc;

	rc = m0_confc_open_sync(&node_dir, confc->cc_root,
				M0_CONF_ROOT_NODES_FID);
	M0_UT_ASSERT(rc == 0);

	rc = m0_confc_open_sync(&node, node_dir, M0_FID_TINIT('n', 1, 2));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(node->co_ha_state == state);

	rc = m0_confc_open_sync(&svc_dir, node, M0_CONF_NODE_PROCESSES_FID,
				M0_FID_TINIT('r', 1, 5),
				M0_CONF_PROCESS_SERVICES_FID);
	M0_UT_ASSERT(rc == 0);

	rc = m0_confc_open_sync(&svc, svc_dir, M0_FID_TINIT('s', 1, 9));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(svc->co_ha_state == state);

	m0_confc_close(svc);

	rc = m0_confc_open_sync(&svc, svc_dir, M0_FID_TINIT('s', 1, 10));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(svc->co_ha_state == state);

	m0_confc_close(svc);
	m0_confc_close(svc_dir);
	m0_confc_close(node);
	m0_confc_close(node_dir);
}


static void pool_machine_fid_populate(struct m0_poolmach *pool_mach);
static int pm_event_construct_and_apply(struct m0_poolmach *pm,
					uint32_t dev_idx, uint32_t state);

/* ----------------------------------------------------------------
 * Unit tests
 * ---------------------------------------------------------------- */
static void test_ha_state_set_and_get(void)
{
	struct m0_confc   confc;
	int               rc;
	struct m0_ha_note n1[] = {
		{ M0_FID_TINIT('n', 1, 2), M0_NC_ONLINE },
		{ M0_FID_TINIT('s', 1, 9), M0_NC_ONLINE },
		{ M0_FID_TINIT('s', 1, 10), M0_NC_ONLINE},
	};
	struct m0_ha_nvec nvec = { ARRAY_SIZE(n1), n1 };

	local_confc_init(&confc);
	m0_ha_client_add(&confc);
	m0_ha_state_set(&nvec);

	n1[0].no_state = M0_NC_UNKNOWN;
	n1[1].no_state = M0_NC_UNKNOWN;
	n1[2].no_state = M0_NC_UNKNOWN;
	rc = m0_ha_state_get(&nvec, &chan);
	M0_UT_ASSERT(rc == 0);
	m0_chan_wait(&clink);
	m0_ha_state_accept(&nvec, false);
	compare_ha_state(&confc, M0_NC_ONLINE);
	m0_ha_client_del(&confc);

	m0_confc_fini(&confc);
}

static void test_ha_state_accept(void)
{
	struct m0_confc confc;
	struct m0_fid u[] = {
		M0_FID_TINIT('n', 1, 2),
		M0_FID_TINIT('s', 1, 9),
		M0_FID_TINIT('s', 1, 10),
	};
	struct m0_ha_note *n;

	M0_ALLOC_ARR(n, ARRAY_SIZE(u));
	struct m0_ha_nvec  nvec;
	int                i;

	local_confc_init(&confc);
	m0_ha_client_add(&confc);

	nvec.nv_nr   = ARRAY_SIZE(u);
	nvec.nv_note = n;

	/* To initialize */
	for (i = 0; i < ARRAY_SIZE(u); ++i) {
		n[i].no_id    = u[i];
		n[i].no_state = M0_NC_ONLINE;
	}
	m0_ha_state_accept(&nvec, false);
	compare_ha_state(&confc, M0_NC_ONLINE);

	/* To check updates */
	for (i = 0; i < ARRAY_SIZE(u); ++i) {
		n[i].no_state = M0_NC_FAILED;
	}
	m0_ha_state_accept(&nvec, false);
	compare_ha_state(&confc, M0_NC_FAILED);

	m0_ha_client_del(&confc);
	m0_confc_fini(&confc);
	m0_free(n);
}

static void ha_ut_conf_init(struct m0_reqh *reqh)
{
	int              rc;
	struct m0_confc *confc = m0_reqh2confc(reqh);

	rc = m0_fid_sscanf(M0_UT_CONF_PROFILE, m0_reqh2profile(reqh));
	M0_UT_ASSERT(rc == 0);
	local_confc_init(confc);
	m0_ha_client_add(confc);

	rc = m0_confc_root_open(confc, &root);
	M0_UT_ASSERT(rc == 0);

        rc = m0_conf_full_load(root);
        M0_UT_ASSERT(rc == 0);
	/*
	 * All conf objects are M0_NC_ONLINE now; see m0_conf_obj_create().
	 */
}

static void ha_ut_conf_fini(struct m0_reqh *reqh)
{
	m0_confc_close(&root->rt_obj);
	m0_ha_client_del(m0_reqh2confc(reqh));
	m0_confc_fini(m0_reqh2confc(reqh));
}

static void ha_ut_pver_kind_check(const struct m0_fid *pver_fid,
				  enum m0_conf_pver_kind expected)
{
	enum m0_conf_pver_kind actual;
	int                    rc;

	rc = m0_conf_pver_fid_read(pver_fid, &actual, NULL, NULL);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(actual == expected);
}

static bool test_is_objv(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_OBJV_TYPE;
}

/** Ensures that pver subtree exists and is traversable. */
static void check_pver_subtree(struct m0_confc *confc,
			       const struct m0_fid *pver_fid)
{
	struct m0_conf_diter  it;
	struct m0_conf_obj   *pver_obj;
	struct m0_conf_objv  *objv;
	int                   rc;

	rc = m0_conf_obj_find_lock(&confc->cc_cache, pver_fid, &pver_obj);
	M0_UT_ASSERT(rc == 0);
	rc = m0_conf_diter_init(&it, confc, pver_obj,
				M0_CONF_PVER_SITEVS_FID,
				M0_CONF_SITEV_RACKVS_FID,
				M0_CONF_RACKV_ENCLVS_FID,
				M0_CONF_ENCLV_CTRLVS_FID,
				M0_CONF_CTRLV_DRIVEVS_FID);
	M0_UT_ASSERT(rc == 0);
	while ((rc = m0_conf_diter_next_sync(&it, test_is_objv)) ==
	       M0_CONF_DIRNEXT) {
		struct m0_conf_obj *obj = m0_conf_diter_result(&it);
		objv = M0_CONF_CAST(obj, m0_conf_objv);
		M0_LOG(M0_DEBUG, "fid="FID_F" idx=%d state=%d",
				  FID_P(&obj->co_id), objv->cv_ix,
				  objv->cv_real->co_ha_state);
	}
	m0_conf_diter_fini(&it);
}

static void test_failvec_fetch(void)
{
        struct m0_pool         pool;
        struct m0_fid          pool_fid;
        struct m0_poolmach     pool_mach;
        struct m0_pool_version pool_ver;
        uint32_t               i;
        uint64_t               qlen;
        uint64_t               failed_nr;
        int                    rc;

        M0_SET0(&pool);
        M0_SET0(&pool_ver);
        M0_SET0(&pool_mach);

        m0_fid_set(&pool_fid, 0, 999);
        rc = m0_pool_init(&pool, &pool_fid, 0);
        M0_UT_ASSERT(rc == 0);
        pool_ver.pv_pool = &pool;
        rc = m0_poolmach_init(&pool_mach, &pool_ver, NR_NODES, NR_DISKS,
                              MAX_FAILURES, MAX_FAILURES);
        pool_machine_fid_populate(&pool_mach);
        /* Enqueue events received till fetching the failvec. */
        for (i = 0; i < NR_OVERLAP; ++i) {
                rc = pm_event_construct_and_apply(&pool_mach, i,
                                                  M0_PNDS_SNS_REPAIRING);
                M0_UT_ASSERT(rc == 0);
        }
        qlen = m0_poolmach_equeue_length(&pool_mach);
        M0_UT_ASSERT(qlen == NR_OVERLAP);
        for (i = NR_NO_OVERLAP; i < MAX_FAILURES; ++i) {
                rc = pm_event_construct_and_apply(&pool_mach, i,
                                                  M0_PNDS_FAILED);
                M0_UT_ASSERT(rc == 0);
        }
        qlen = m0_poolmach_equeue_length(&pool_mach);
        M0_UT_ASSERT(qlen == MAX_FAILURES - NR_NO_OVERLAP + NR_OVERLAP);
        m0_fi_enable("m0_ha_msg_fvec_send", "non-trivial-fvec");
        rc = m0_ha_failvec_fetch(&pool_fid, &pool_mach, &chan);
        M0_UT_ASSERT(rc == 0);
        m0_chan_wait(&clink);
        m0_fi_disable("m0_ha_msg_fvec_send", "non-trivial-fvec");
        failed_nr = m0_poolmach_nr_dev_failures(&pool_mach);
        M0_UT_ASSERT(failed_nr == MAX_FAILURES);
        M0_UT_ASSERT(m0_poolmach_equeue_length(&pool_mach) == 0);
	m0_fi_enable_once("m0_poolmach_fini", "poolmach_init_by_conf_skipped");
	m0_poolmach_fini(&pool_mach);
        m0_pool_fini(&pool);
}

static void pool_machine_fid_populate(struct m0_poolmach *pool_mach)
{
	uint32_t i;

	for (i = 0; i < pool_mach->pm_state->pst_nr_devices; ++i)
		pool_mach->pm_state->pst_devices_array[i].pd_id =
			M0_FID_TINIT('d', 1, i);
}

static int pm_event_construct_and_apply(struct m0_poolmach *pm,
					uint32_t dev_idx, uint32_t state)
{
	const struct m0_poolmach_event event = {
		.pe_type  = M0_POOL_DEVICE,
		.pe_index = dev_idx,
		.pe_state = state
	};
	return m0_poolmach_state_transit(pm, &event);
}

/* XXX Move to ha/note.h? */
static void ha_state_accept_1(const struct m0_fid *fid, uint32_t state)
{
	struct m0_ha_note note = { .no_id = *fid, .no_state = state };

	m0_ha_state_accept(&(const struct m0_ha_nvec){ 1, &note }, false);
}

static uint32_t recd_disks(const struct m0_conf_pver *pver)
{
	M0_PRE(M0_IN(pver->pv_kind, (M0_CONF_PVER_ACTUAL,
				     M0_CONF_PVER_VIRTUAL)));
	return pver->pv_u.subtree.pvs_recd[M0_CONF_PVER_LVL_DRIVES];
}

static void test_poolversion_get(void)
{
	static const struct m0_fid disk_76 = M0_FID_TINIT('k', 1, 76);
	static const struct m0_fid disk_77 = M0_FID_TINIT('k', 1, 77);
	static const struct m0_fid disk_78 = M0_FID_TINIT('k', 1, 78);
	static const struct m0_fid pool4_fid = M0_FID_TINIT('o', 1, 4);
	static const struct m0_fid pool56_fid = M0_FID_TINIT('o', 1, 56);
	struct m0_ha_note notes[] = {
		{ M0_FID_TINIT('a', 1, 3),  M0_NC_ONLINE }, // rack-3
		{ M0_FID_TINIT('e', 1, 7),  M0_NC_ONLINE }, // enclosure-7
		{ M0_FID_TINIT('c', 1, 11), M0_NC_ONLINE }, // controller-11
		{ disk_76,                  M0_NC_ONLINE },
		{ disk_77,                  M0_NC_ONLINE },
		{ disk_78,                  M0_NC_ONLINE }
	};
	const struct m0_ha_nvec nvec = { ARRAY_SIZE(notes), notes };
	struct m0_conf_pver    *pver0 = NULL;
	struct m0_conf_pver    *pver1 = NULL;
	struct m0_conf_pver    *pver2 = NULL;
	struct m0_conf_pver    *pver3 = NULL;
	struct m0_conf_pver    *pver4 = NULL;
	struct m0_reqh          reqh;
	struct m0_confc        *confc = m0_reqh2confc(&reqh);
	int                     rc;
	/*
	 * Use these bash commands to visualize the tree of conf objects:
	 *
	 *     # helper function
	 *     cg() { utils/m0confgen "$@"; }
	 *
	 *     cg -t confgen ut/conf.cg | # reformat to one line per object
	 *         # exclude unwanted objects
	 *         grep -vE '^.(node|process|service|sdev)' |
	 *         # generate DOT output
	 *         cg -t dot >/tmp/conf.dot
	 *     # convert to PNG
	 *     dot -Tpng -o /tmp/conf.png /tmp/conf.dot
	 *
	 *     open /tmp/conf.png
	 */
	m0_reqh_init(&reqh, &reqh_args);
	ha_ut_conf_init(&reqh);

	rc = m0_conf_pver_get(confc, &pool4_fid, &pver0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pver0 != NULL);
	ha_ut_pver_kind_check(&pver0->pv_obj.co_id, M0_CONF_PVER_ACTUAL);
	M0_UT_ASSERT(m0_fid_eq(&pver0->pv_obj.co_id, &M0_FID_TINIT('v', 1, 8)));
	check_pver_subtree(confc, &pver0->pv_obj.co_id);
	M0_UT_ASSERT(recd_disks(pver0) == 0);

	ha_state_accept_1(&disk_77, M0_NC_TRANSIENT);
	rc = m0_conf_pver_get(confc, &pool4_fid, &pver1);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pver1 != NULL);
	M0_UT_ASSERT(pver1 != pver0);
	ha_ut_pver_kind_check(&pver1->pv_obj.co_id, M0_CONF_PVER_VIRTUAL);
	check_pver_subtree(confc, &pver1->pv_obj.co_id);
	M0_UT_ASSERT(recd_disks(pver0) == 1);

	ha_state_accept_1(&disk_78, M0_NC_FAILED);
	rc = m0_conf_pver_get(confc, &pool4_fid, &pver2);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pver2 != NULL);
	M0_UT_ASSERT(pver2 != pver1 && pver2 != pver0);
	ha_ut_pver_kind_check(&pver2->pv_obj.co_id, M0_CONF_PVER_VIRTUAL);
	check_pver_subtree(confc, &pver2->pv_obj.co_id);
	M0_UT_ASSERT(recd_disks(pver0) == 2);

	ha_state_accept_1(&disk_76, M0_NC_TRANSIENT);
	rc = m0_conf_pver_get(confc, &pool4_fid, &pver3);
	M0_UT_ASSERT(rc == -ENOENT);
	M0_UT_ASSERT(pver3 == NULL);


	rc = m0_conf_pver_get(confc, &pool56_fid, &pver3);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pver3 != NULL);
	M0_UT_ASSERT(pver3 != pver2 && pver3 != pver1 && pver3 != pver0);
	/*
	 * Three disks has failed. This exceeds allowances of formulaic
	 * pvers of pool-4 ==> Mero switches to another pool.
	 */
	ha_ut_pver_kind_check(&pver3->pv_obj.co_id, M0_CONF_PVER_ACTUAL);
	M0_UT_ASSERT(m0_fid_eq(&pver3->pv_obj.co_id,
			       &M0_FID_TINIT('v', 1, 57)));
	check_pver_subtree(confc, &pver3->pv_obj.co_id);
	M0_UT_ASSERT(recd_disks(pver0) == 3);
	M0_UT_ASSERT(recd_disks(pver3) == 0);

	/* Put all disks back online. */
	m0_ha_state_accept(&nvec, false);
	rc = m0_conf_pver_get(confc, &pool4_fid, &pver4);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(pver4 == pver0);
	M0_UT_ASSERT(recd_disks(pver0) == 0);

	m0_confc_close(&pver0->pv_obj);
	m0_confc_close(&pver1->pv_obj);
	m0_confc_close(&pver2->pv_obj);
	m0_confc_close(&pver3->pv_obj);
	m0_confc_close(&pver4->pv_obj);
	ha_ut_conf_fini(&reqh);
	m0_reqh_fini(&reqh);
}

/*
 * The test reproduces a MERO-1997 regression. See
 * https://jira.xyratex.com/browse/MERO-1997
 */
static void test_ha_session_states(void)
{
	struct m0_reqh    *reqh = &sctx.rsx_mero_ctx.cc_reqh_ctx.rc_reqh;
	struct m0_rconfc  *cl_rconfc = &cctx.rcx_reqh.rh_rconfc;
	struct m0_fid      fid       = M0_FID_TINIT('s', 1, 9);
	struct m0_ha_note  note = { .no_id = fid, .no_state = M0_NC_FAILED};
	struct m0_ha_nvec  nvec = { .nv_nr = 1, .nv_note = &note};
	int                rc;

	rc = m0_rconfc_init(cl_rconfc, m0_reqh2profile(reqh),
			    m0_locality0_get()->lo_grp, &cctx.rcx_rpc_machine,
			    NULL, NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_file_read(M0_UT_PATH("conf.xc"), &cl_rconfc->rc_local_conf);
	M0_UT_ASSERT(rc == 0);
	m0_rconfc_start(cl_rconfc);
	m0_ha_client_add(&cl_rconfc->rc_confc);
	rc = m0_rpc_conn_ha_subscribe(&cctx.rcx_connection, &fid);
	M0_UT_ASSERT(rc == 0);

	m0_ha_state_accept(&nvec, false);

	m0_rpc_conn_ha_unsubscribe(&cctx.rcx_connection);
	m0_ha_client_del(&cl_rconfc->rc_confc);
	m0_rconfc_stop_sync(cl_rconfc);
	m0_rconfc_fini(cl_rconfc);
}

/* -------------------------------------------------------------------
 * Test suite
 * ------------------------------------------------------------------- */

static int ha_state_ut_init(void)
{
	int rc;

	rc = ast_thread_init();
	M0_ASSERT(rc == 0);

	done_get_chan_init();
	start_rpc_client_and_server();
	return 0;
}

static int ha_state_ut_fini(void)
{
	stop_rpc_client_and_server();
	done_get_chan_fini();
	ast_thread_fini();
	return 0;
}

struct m0_ut_suite ha_state_ut = {
	.ts_name = "ha-state-ut",
	.ts_init = ha_state_ut_init,
	.ts_fini = ha_state_ut_fini,
	.ts_tests = {
		{ "ha-state-set-and-get", test_ha_state_set_and_get },
		{ "ha-state-accept",      test_ha_state_accept },
		{ "ha-failvecl-fetch",    test_failvec_fetch },
		{ "ha-poolversion-get",   test_poolversion_get },
		{ "ha-session-states",    test_ha_session_states },
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
