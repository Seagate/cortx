/* -*- c -*- */
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 17-Sep-2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "conf/confc.h"
#include "conf/obj_ops.h"         /* M0_CONF_DIREND */
#include "conf/helpers.h"         /* m0_confc_root_open */
#include "conf/ut/confc.h"        /* m0_ut_conf_fids */
#include "conf/ut/common.h"       /* m0_conf_ut_waiter */
#include "conf/ut/rpc_helpers.h"  /* m0_ut_rpc_machine_start */
#include "rpc/rpclib.h"           /* m0_rpc_server_ctx */
#include "lib/finject.h"          /* m0_fi_enable_once */
#include "lib/errno.h"            /* ENOENT */
#include "lib/fs.h"               /* m0_file_read */
#include "lib/memory.h"           /* m0_free */
#include "ut/misc.h"              /* M0_UT_PATH */
#include "ut/ut.h"

static uint8_t  g_num;
static uint8_t  g_num_normal[] = {4, 2, 2};
static uint8_t *g_num_expected = g_num_normal;

static void root_open_test(struct m0_confc *confc)
{
	struct m0_conf_root *root_obj;
	int                  rc;

	rc = m0_confc_root_open(confc, &root_obj);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_fid_eq(&root_obj->rt_obj.co_id,
	                       &m0_ut_conf_fids[M0_UT_CONF_ROOT]));
	M0_UT_ASSERT(root_obj->rt_verno == 1);

	m0_confc_close(&root_obj->rt_obj);
}

static void sync_open_test(struct m0_conf_obj *nodes_dir)
{
	struct m0_conf_obj  *obj;
	struct m0_conf_node *node;
	int                  rc;

	M0_PRE(m0_conf_obj_type(nodes_dir) == &M0_CONF_DIR_TYPE);

	rc = m0_confc_open_sync(&obj, nodes_dir,
				m0_ut_conf_fids[M0_UT_CONF_UNKNOWN_NODE]);
	M0_UT_ASSERT(rc == -ENOENT);

	rc = m0_confc_open_sync(&obj, nodes_dir,
				m0_ut_conf_fids[M0_UT_CONF_NODE]);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(obj->co_status == M0_CS_READY);
	M0_UT_ASSERT(obj->co_cache == nodes_dir->co_cache);
	M0_UT_ASSERT(m0_fid_eq(&obj->co_id, &m0_ut_conf_fids[M0_UT_CONF_NODE]));

	node = M0_CONF_CAST(obj, m0_conf_node);
	M0_UT_ASSERT(node->cn_memsize    == 16000);
	M0_UT_ASSERT(node->cn_nr_cpu     == 2);
	M0_UT_ASSERT(node->cn_last_state == 3);
	M0_UT_ASSERT(node->cn_flags      == 2);

	M0_UT_ASSERT(obj == &node->cn_obj);
	m0_confc_close(obj);

	m0_fi_enable_once("m0_confc__open", "invalid-origin");
	rc = m0_confc_open_sync(&obj, nodes_dir,
				m0_ut_conf_fids[M0_UT_CONF_NODE]);
	M0_UT_ASSERT(rc == -EAGAIN);
}

static void sdev_disk_check(struct m0_confc *confc)
{
	struct m0_conf_obj  *sdev_obj;
	struct m0_conf_obj  *disk_obj;
	struct m0_conf_sdev *sdev;
	int                  rc;

	/* Verify disk fid from sdev object, if disk object defined. */
	rc = m0_confc_open_sync(&sdev_obj, confc->cc_root,
				M0_CONF_ROOT_NODES_FID,
				m0_ut_conf_fids[M0_UT_CONF_NODE],
				M0_CONF_NODE_PROCESSES_FID,
				m0_ut_conf_fids[M0_UT_CONF_PROCESS0],
				M0_CONF_PROCESS_SERVICES_FID,
				m0_ut_conf_fids[M0_UT_CONF_SERVICE0],
				M0_CONF_SERVICE_SDEVS_FID,
				m0_ut_conf_fids[M0_UT_CONF_SDEV2]);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(sdev_obj->co_status == M0_CS_READY);
	sdev = M0_CONF_CAST(sdev_obj, m0_conf_sdev);

	rc = m0_confc_open_sync(&disk_obj, confc->cc_root,
				M0_CONF_ROOT_SITES_FID,
				m0_ut_conf_fids[M0_UT_CONF_SITE],
				M0_CONF_SITE_RACKS_FID,
				m0_ut_conf_fids[M0_UT_CONF_RACK],
				M0_CONF_RACK_ENCLS_FID,
				m0_ut_conf_fids[M0_UT_CONF_ENCLOSURE],
				M0_CONF_ENCLOSURE_CTRLS_FID,
				m0_ut_conf_fids[M0_UT_CONF_CONTROLLER],
				M0_CONF_CONTROLLER_DRIVES_FID,
				m0_ut_conf_fids[M0_UT_CONF_DISK]);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(disk_obj->co_status == M0_CS_READY);

	M0_UT_ASSERT(m0_fid_eq(&sdev->sd_drive, &disk_obj->co_id));

	m0_confc_close(sdev_obj);
	m0_confc_close(disk_obj);

	/* Verify disk fid from sdev object, if disk object not defined. */
	rc = m0_confc_open_sync(&sdev_obj, confc->cc_root,
				M0_CONF_ROOT_NODES_FID,
				m0_ut_conf_fids[M0_UT_CONF_NODE],
				M0_CONF_NODE_PROCESSES_FID,
				m0_ut_conf_fids[M0_UT_CONF_PROCESS0],
				M0_CONF_PROCESS_SERVICES_FID,
				m0_ut_conf_fids[M0_UT_CONF_SERVICE1],
				M0_CONF_SERVICE_SDEVS_FID,
				m0_ut_conf_fids[M0_UT_CONF_SDEV0]);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(sdev_obj->co_status == M0_CS_READY);
	sdev = M0_CONF_CAST(sdev_obj, m0_conf_sdev);
	M0_UT_ASSERT(!m0_fid_is_set(&sdev->sd_drive));

	m0_confc_close(sdev_obj);

}

static void nodes_open(struct m0_conf_obj **result,
		       struct m0_confc     *confc)
{
	struct m0_conf_ut_waiter w;
	int                      rc;

	m0_conf_ut_waiter_init(&w, confc);
	m0_confc_open(&w.w_ctx, NULL, M0_CONF_ROOT_NODES_FID);
	rc = m0_conf_ut_waiter_wait(&w, result);
	M0_UT_ASSERT(rc == 0);
	m0_conf_ut_waiter_fini(&w);

	M0_UT_ASSERT((*result)->co_status == M0_CS_READY);
	M0_UT_ASSERT((*result)->co_cache == &confc->cc_cache);
}

static void _proc_cores_add(const struct m0_conf_obj *obj)
{
	g_num += m0_bitmap_set_nr(
			&M0_CONF_CAST(obj, m0_conf_process)->pc_cores);
}

static bool _proc_has_services(const struct m0_conf_obj *obj)
{
	return M0_CONF_CAST(obj, m0_conf_process)->pc_services != NULL;
}

M0_BASSERT(M0_CONF_DIREND == 0);

/* This code originates from `confc-fspec-recipe4' in conf/confc.h. */
static int dir_entries_use(struct m0_conf_obj *dir,
			   void (*use)(const struct m0_conf_obj *),
			   bool (*stop_at)(const struct m0_conf_obj *))
{
	struct m0_conf_ut_waiter  w;
	struct m0_conf_obj       *entry = NULL;
	int                       rc;

	m0_conf_ut_waiter_init(&w, m0_confc_from_obj(dir));

	while ((rc = m0_confc_readdir(&w.w_ctx, dir, &entry)) > 0) {
		if (rc == M0_CONF_DIRNEXT) {
			/* The entry is available immediately. */
			M0_ASSERT(entry != NULL);

			use(entry);
			if (stop_at != NULL && stop_at(entry)) {
				rc = 0;
				break;
			}
			continue; /* Note, that `entry' will be closed
				   * by m0_confc_readdir(). */
		}

		/* Cache miss. */
		M0_ASSERT(rc == M0_CONF_DIRMISS);
		rc = m0_conf_ut_waiter_wait(&w, &entry);
		if (rc != 0 /* error */ || entry == NULL /* end of directory */)
			break;

		use(entry);
		if (stop_at != NULL && stop_at(entry))
			break;

		/* Re-initialise m0_confc_ctx. */
		m0_conf_ut_waiter_fini(&w);
		m0_conf_ut_waiter_init(&w, m0_confc_from_obj(dir));
	}

	m0_confc_close(entry);
	m0_conf_ut_waiter_fini(&w);
	return rc;
}

static void dir_test(struct m0_confc *confc)
{
	struct m0_conf_obj *procs_dir;
	struct m0_conf_obj *entry = NULL;
	int                 rc;

	rc = m0_confc_open_sync(&procs_dir, confc->cc_root,
				M0_CONF_ROOT_NODES_FID,
				m0_ut_conf_fids[M0_UT_CONF_NODE],
				M0_CONF_NODE_PROCESSES_FID);
	M0_UT_ASSERT(rc == 0);

	g_num = 0;
	rc = dir_entries_use(procs_dir, _proc_cores_add, NULL);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(g_num == g_num_expected[0]);

	g_num = 0;
	rc = dir_entries_use(procs_dir, _proc_cores_add, _proc_has_services);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(g_num == g_num_expected[1]);

	g_num = 0;
	while (m0_confc_readdir_sync(procs_dir, &entry) > 0)
		++g_num;
	M0_UT_ASSERT(g_num == g_num_expected[2]);

	m0_confc_close(entry);
	m0_confc_close(procs_dir);
}

/*
 * We need this function in order for m0_confc_open() to be called in
 * a separate stack frame.
 */
static void _retrieval_initiate(struct m0_confc_ctx *ctx)
{
	m0_confc_open(ctx, NULL,
		      M0_CONF_ROOT_NODES_FID,
		      m0_ut_conf_fids[M0_UT_CONF_NODE],
		      M0_CONF_NODE_PROCESSES_FID,
		      m0_ut_conf_fids[M0_UT_CONF_PROCESS0]);
}

static void misc_test(struct m0_confc *confc)
{
	struct m0_conf_obj       *obj;
	struct m0_conf_ut_waiter  w;
	struct m0_fid             fids[M0_CONF_PATH_MAX + 2];
	int                       i;
	int                       rc;
	struct m0_conf_obj       *result;

	/*
	 * We should be able to call m0_confc_ctx_fini() right after
	 * m0_confc_ctx_init().
	 *
	 * Application code may depend on this ability (e.g.,
	 * dir_entries_use() does).
	 */
	m0_confc_ctx_init(&w.w_ctx, confc);
	m0_confc_ctx_fini(&w.w_ctx);

	/*
	 * A path to configuration object is created on stack (see the
	 * definition of m0_confc_open()).  We need to allow this
	 * stack frame to be destructed even before the result of
	 * configuration retrieval operation is obtained.
	 *
	 * In other words, m0_confc_ctx::fc_path should be able to
	 * outlive the array of path components, constructed by
	 * m0_confc_open().
	 */
	m0_conf_ut_waiter_init(&w, confc);
	_retrieval_initiate(&w.w_ctx);
	(void)m0_conf_ut_waiter_wait(&w, &obj);
	m0_confc_close(obj);
	m0_conf_ut_waiter_fini(&w);

	/*
	 * Check for too long path requested by user.
	 */
	for (i = 0; i < ARRAY_SIZE(fids) - 1; i++)
		fids[i] = M0_FID_INIT(1, 1);

	/* Terminate array with M0_FID0 */
	fids[ARRAY_SIZE(fids) - 1] = M0_FID_INIT(0, 0);

	m0_conf_ut_waiter_init(&w, confc);
	m0_confc__open(&w.w_ctx, NULL, fids);
	rc = m0_conf_ut_waiter_wait(&w, &result);
	M0_UT_ASSERT(rc == -E2BIG);
	m0_conf_ut_waiter_fini(&w);
}

static void open_by_fid_test(struct m0_confc *confc)
{
	struct m0_conf_ut_waiter w;
	struct m0_conf_obj      *obj;
	struct m0_conf_node     *node;
	int                      rc;

	m0_conf_ut_waiter_init(&w, confc);
	m0_confc_open_by_fid(&w.w_ctx, &m0_ut_conf_fids[M0_UT_CONF_NODE]);
	rc = m0_conf_ut_waiter_wait(&w, &obj);
	M0_UT_ASSERT(rc == 0);
	node = M0_CONF_CAST(obj, m0_conf_node);
	M0_UT_ASSERT(node->cn_memsize == 16000);
	M0_UT_ASSERT(node->cn_nr_cpu == 2);
	m0_confc_close(obj);
	m0_conf_ut_waiter_fini(&w);

	obj = NULL;
	rc = m0_confc_open_by_fid_sync(confc, &m0_ut_conf_fids[M0_UT_CONF_NODE],
				       &obj);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(obj != NULL);
	m0_confc_close(obj);
}

static void confc_test(const char *confd_addr, struct m0_rpc_machine *rpc_mach,
		       const char *conf_str)
{
	struct m0_confc     confc = {};
	struct m0_conf_obj *nodes_dir;
	int                 rc;

	rc = m0_confc_init(&confc, &m0_conf_ut_grp, confd_addr, rpc_mach,
			   conf_str);
	M0_UT_ASSERT(rc == 0);

	root_open_test(&confc);
	dir_test(&confc);
	misc_test(&confc);
	nodes_open(&nodes_dir, &confc); /* tests asynchronous interface */
	sync_open_test(nodes_dir);
	open_by_fid_test(&confc);
	sdev_disk_check(&confc);

	m0_confc_close(nodes_dir);
	m0_confc_fini(&confc);
}

static void test_confc_local(void)
{
	struct m0_confc     confc = {};
	struct m0_conf_obj *obj;
	char               *confstr = NULL;
	int                 rc;

	rc = m0_confc_init(&confc, &m0_conf_ut_grp, NULL, NULL,
			   "bad configuration string");
	M0_UT_ASSERT(rc == -EPROTO);

	rc = m0_file_read(M0_UT_PATH("conf.xc"), &confstr);
	M0_UT_ASSERT(rc == 0);
	rc = m0_confc_init(&confc, &m0_conf_ut_grp, NULL, NULL, confstr);
	M0_UT_ASSERT(rc == 0);

	/* normal case - profile exists in conf */
	rc = m0_confc_open_sync(&obj, confc.cc_root,
				M0_CONF_ROOT_PROFILES_FID,
				m0_ut_conf_fids[M0_UT_CONF_PROF]);
	M0_UT_ASSERT(rc == 0);
	m0_confc_close(obj);

	/* fail case - profile does not exist */
	rc = m0_confc_open_sync(&obj, confc.cc_root,
				M0_CONF_ROOT_PROFILES_FID,
				M0_FID_TINIT('p', 7, 7));
	M0_UT_ASSERT(rc == -ENOENT);
	m0_confc_fini(&confc);

	confc_test(NULL, NULL, confstr);
	m0_free(confstr);
}

static void test_confc_multiword_core_mask(void)
{
	struct m0_confc     confc = {};
	struct m0_conf_obj *obj;
	char               *confstr = NULL;
	int                 rc;
	uint8_t             mw_core_num[] = {12, 10, 2};

	/* load conf with a process having multi-word core mask */
	rc = m0_file_read(M0_SRC_PATH("conf/ut/multiword-core-mask.xc"),
			  &confstr);
	M0_UT_ASSERT(rc == 0);
	rc = m0_confc_init(&confc, &m0_conf_ut_grp, NULL, NULL, confstr);
	M0_UT_ASSERT(rc == 0);

	rc = m0_confc_open_sync(&obj, confc.cc_root,
				M0_CONF_ROOT_PROFILES_FID,
				m0_ut_conf_fids[M0_UT_CONF_PROF]);
	M0_UT_ASSERT(rc == 0);
	m0_confc_close(obj);
	m0_confc_fini(&confc);

	g_num_expected = mw_core_num;
	confc_test(NULL, NULL, confstr);
	g_num_expected = g_num_normal;
	m0_free(confstr);
}

static void test_confc_net(void)
{
	struct m0_rpc_machine mach;
	int                   rc;
#define NAME(ext) "ut_confd" ext
	char                    *argv[] = {
		NAME(""), "-T", "AD", "-D", NAME(".db"),
		"-S", NAME(".stob"), "-A", "linuxstob:"NAME("-addb_stob"),
		"-w", "10", "-e", SERVER_ENDPOINT, "-H", SERVER_ENDPOINT_ADDR,
		"-f", M0_UT_CONF_PROCESS,
		"-c", M0_UT_PATH("conf.xc")
	};
	struct m0_rpc_server_ctx confd = {
		.rsx_xprts         = &m0_conf_ut_xprt,
		.rsx_xprts_nr      = 1,
		.rsx_argv          = argv,
		.rsx_argc          = ARRAY_SIZE(argv),
		.rsx_log_file_name = NAME(".log")
	};
#undef NAME

	rc = m0_rpc_server_start(&confd);
	M0_UT_ASSERT(rc == 0);

	rc = m0_ut_rpc_machine_start(&mach, m0_conf_ut_xprt,
				     CLIENT_ENDPOINT_ADDR);
	M0_UT_ASSERT(rc == 0);

	confc_test(SERVER_ENDPOINT_ADDR, &mach, NULL);

	m0_ut_rpc_machine_stop(&mach);
	m0_rpc_server_stop(&confd);
}

static void test_confc_invalid_input(void)
{
	struct m0_confc confc = {};
	char           *confstr = NULL;
	int             rc;

	rc = m0_file_read(M0_SRC_PATH("conf/ut/duplicated-ids.xc"), &confstr);
	M0_UT_ASSERT(rc == 0);
	rc = m0_confc_init(&confc, &m0_conf_ut_grp, NULL, NULL, confstr);
	M0_UT_ASSERT(rc == -EEXIST);
	m0_free(confstr);
}

struct m0_ut_suite confc_ut = {
	.ts_name  = "confc-ut",
	.ts_init  = m0_conf_ut_ast_thread_init,
	.ts_fini  = m0_conf_ut_ast_thread_fini,
	.ts_tests = {
		{ "local",        test_confc_local },
		{ "mw-core-mask", test_confc_multiword_core_mask },
		{ "net",          test_confc_net },
		{ "bad-input",    test_confc_invalid_input },
		{ NULL, NULL }
	}
};

#undef M0_TRACE_SUBSYSTEM
