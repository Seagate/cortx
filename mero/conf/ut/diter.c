/* -*- c -*- */
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
 * Original author: Mandar Sawant <mandar_sawant@seagate.com>
 * Authors:         Andriy Tkachuk <andriy.tkachuk@seagate.com>
 * Original creation date: 20-Jan-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "conf/diter.h"
#include "conf/obj_ops.h"         /* M0_CONF_DIRNEXT */
#include "conf/ut/common.h"       /* m0_conf_ut_grp */
#include "conf/ut/rpc_helpers.h"  /* m0_ut_rpc_machine_start */
#include "rpc/rpclib.h"           /* m0_rpc_server_ctx */
#include "lib/fs.h"               /* m0_file_read */
#include "lib/memory.h"           /* m0_free */
#include "ut/misc.h"              /* M0_UT_PATH, M0_UT_CONF_PROCESS */
#include "ut/ut.h"

enum {
	PROF,
	FS,
	NODE,
	PROCESS0,
	SERVICE0,
	SERVICE1,
	SERVICE2,
	SERVICE3,
	SERVICE4,
	SERVICE5,
	SERVICE6,
	SERVICE7,
	SERVICE8,
	SDEV0,
	SDEV1,
	SDEV2,
	SDEV3,
	SDEV4,
	RACK,
	ENCLOSURE,
	CONTROLLER,
	DISK0,
	DISK1,
	DISK2,
	DISK3,
	DISK4,
	POOL,
	PVER,
	RACKV,
	ENCLOSUREV,
	CONTROLLERV,
	DISKV0,
	DISKV1,
	DISKV2,
	DISKV3,
	DISKV4,
	UNKNOWN_SVC
};

static const struct m0_fid fids[] = {
	[PROF]        = M0_FID_TINIT('p', 1, 0),
	[FS]          = M0_FID_TINIT('f', 1, 1),
	[NODE]        = M0_FID_TINIT('n', 1, 2),
	[PROCESS0]    = M0_FID_TINIT('r', 1, 5),
	[SERVICE0]    = M0_FID_TINIT('s', 1, 0),
	[SERVICE1]    = M0_FID_TINIT('s', 1, 1),
	[SERVICE2]    = M0_FID_TINIT('s', 1, 2),
	[SERVICE3]    = M0_FID_TINIT('s', 1, 3),
	[SERVICE4]    = M0_FID_TINIT('s', 1, 4),
	[SERVICE5]    = M0_FID_TINIT('s', 1, 5),
	[SERVICE6]    = M0_FID_TINIT('s', 1, 6),
	[SERVICE7]    = M0_FID_TINIT('s', 1, 7),
	[SERVICE8]    = M0_FID_TINIT('s', 1, 8),
	[SDEV0]       = M0_FID_TINIT('d', 1, 10),
	[SDEV1]       = M0_FID_TINIT('d', 1, 11),
	[SDEV2]       = M0_FID_TINIT('d', 1, 12),
	[SDEV3]       = M0_FID_TINIT('d', 1, 13),
	[SDEV4]       = M0_FID_TINIT('d', 1, 14),
	[RACK]        = M0_FID_TINIT('a', 1, 15),
	[ENCLOSURE]   = M0_FID_TINIT('e', 1, 16),
	[CONTROLLER]  = M0_FID_TINIT('c', 1, 17),
	[DISK0]       = M0_FID_TINIT('k', 1, 18),
	[DISK1]       = M0_FID_TINIT('k', 1, 19),
	[DISK2]       = M0_FID_TINIT('k', 1, 20),
	[DISK3]       = M0_FID_TINIT('k', 1, 21),
	[DISK4]       = M0_FID_TINIT('k', 1, 22),
	[POOL]        = M0_FID_TINIT('o', 1, 23),
	[PVER]        = M0_FID_TINIT('v', 1, 24),
	[RACKV]       = M0_FID_TINIT('j', 1, 25),
	[ENCLOSUREV]  = M0_FID_TINIT('j', 1, 26),
	[CONTROLLERV] = M0_FID_TINIT('j', 1, 27),
	[DISKV0]      = M0_FID_TINIT('j', 1, 28),
	[DISKV1]      = M0_FID_TINIT('j', 1, 29),
	[DISKV2]      = M0_FID_TINIT('j', 1, 30),
	[DISKV3]      = M0_FID_TINIT('j', 1, 31),
	[DISKV4]      = M0_FID_TINIT('j', 1, 32),
	[UNKNOWN_SVC] = M0_FID_TINIT('s', 5, 33),
};

static void verify_obj(const struct m0_conf_obj *obj, const struct m0_fid *fid)
{
	M0_UT_ASSERT(obj->co_status == M0_CS_READY);
	M0_UT_ASSERT(m0_fid_eq(&obj->co_id, fid));
}

static void verify_node(const struct m0_conf_obj *obj)
{
	const struct m0_conf_node *node = M0_CONF_CAST(obj, m0_conf_node);

	verify_obj(obj, &fids[NODE]);
	M0_UT_ASSERT(node->cn_memsize == 16000);
	M0_UT_ASSERT(node->cn_nr_cpu == 2);
	M0_UT_ASSERT(node->cn_last_state == 3);
	M0_UT_ASSERT(node->cn_flags == 2);
}

static void verify_disk(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *a[] = {
		&fids[DISK0], &fids[DISK1], &fids[DISK2],
		&fids[DISK3], &fids[DISK4]
	};
	M0_UT_ASSERT(obj->co_status == M0_CS_READY);
	M0_UT_ASSERT(m0_exists(i, ARRAY_SIZE(a), m0_fid_eq(&obj->co_id, a[i])));
}

static void check_objv(const struct m0_conf_obj *obj)
{
	const struct m0_conf_obj *r = M0_CONF_CAST(obj, m0_conf_objv)->cv_real;
	const struct m0_conf_obj_type *t = m0_conf_obj_type(r);

	if (t == &M0_CONF_RACK_TYPE)
		verify_obj(r, &fids[RACK]);
	else if (t == &M0_CONF_ENCLOSURE_TYPE)
		verify_obj(r, &fids[ENCLOSURE]);
	else if (t == &M0_CONF_CONTROLLER_TYPE)
		verify_obj(r, &fids[CONTROLLER]);
	else if (t == &M0_CONF_DRIVE_TYPE)
		verify_disk(r);
}

static void check_obj(const struct m0_conf_obj *obj)
{
	const struct m0_conf_obj_type *t = m0_conf_obj_type(obj);

	if (t == &M0_CONF_OBJV_TYPE) {
		check_objv(obj);
	} else if (t == &M0_CONF_RACK_TYPE) {
		verify_obj(obj, &fids[RACK]);
	} else if (t == &M0_CONF_ENCLOSURE_TYPE) {
		verify_obj(obj, &fids[ENCLOSURE]);
	} else if (t == &M0_CONF_CONTROLLER_TYPE) {
		verify_obj(obj, &fids[CONTROLLER]);
	} else if (t == &M0_CONF_DRIVE_TYPE) {
		verify_disk(obj);
	} else if (t == &M0_CONF_PVER_TYPE) {
		verify_obj(obj, &fids[PVER]);
	} else if (t == &M0_CONF_NODE_TYPE) {
		verify_node(obj);
	} else if (t == &M0_CONF_PROCESS_TYPE) {
		M0_UT_ASSERT(m0_fid_eq(&obj->co_id, &fids[PROCESS0]));
	} else if (t == &M0_CONF_SERVICE_TYPE) {
		static const struct m0_fid *a[] = {
			&fids[SERVICE0], &fids[SERVICE1],
			&fids[SERVICE2], &fids[SERVICE3],
			&fids[SERVICE4], &fids[SERVICE5],
			&fids[SERVICE6], &fids[SERVICE7],
			&fids[SERVICE8]
		};
		M0_UT_ASSERT(m0_exists(i, ARRAY_SIZE(a),
				       m0_fid_eq(&obj->co_id, a[i])));
	} else if (t == &M0_CONF_SDEV_TYPE) {
		static const struct m0_fid *a[] = {
			&fids[SDEV0], &fids[SDEV1], &fids[SDEV2],
			&fids[SDEV3], &fids[SDEV4]
		};
		M0_UT_ASSERT(m0_exists(i, ARRAY_SIZE(a),
				       m0_fid_eq(&obj->co_id, a[i])));
	}
}

static bool _filter_diskv(const struct m0_conf_obj *obj)
{
	return (m0_conf_obj_type(obj) == &M0_CONF_OBJV_TYPE &&
	       m0_conf_obj_type(M0_CONF_CAST(obj, m0_conf_objv)->cv_real) ==
	       &M0_CONF_DRIVE_TYPE);
}

static void
all_fs_to_diskv_check(struct m0_confc *confc, struct m0_conf_obj *root)
{
	struct m0_conf_diter    it;
	struct m0_conf_obj     *obj;
	struct m0_conf_objv    *ov;
	struct m0_conf_service *s;
	struct m0_conf_drive   *d;
	int                     rc;

	rc = m0_conf_diter_init(&it, confc, root,
				M0_CONF_ROOT_POOLS_FID,
				M0_CONF_POOL_PVERS_FID,
				M0_CONF_PVER_SITEVS_FID,
				M0_CONF_SITEV_RACKVS_FID,
				M0_CONF_RACKV_ENCLVS_FID,
				M0_CONF_ENCLV_CTRLVS_FID,
				M0_CONF_CTRLV_DRIVEVS_FID);
	M0_UT_ASSERT(rc == 0);

	while ((rc = m0_conf_diter_next_sync(&it, _filter_diskv)) ==
	       M0_CONF_DIRNEXT) {
		obj = m0_conf_diter_result(&it);
		M0_ASSERT(m0_conf_obj_type(obj) == &M0_CONF_OBJV_TYPE);
		ov = M0_CONF_CAST(obj, m0_conf_objv);
		M0_ASSERT(m0_conf_obj_type(ov->cv_real) == &M0_CONF_DRIVE_TYPE);
		d = M0_CONF_CAST(ov->cv_real, m0_conf_drive);
		check_obj(&d->ck_obj);
		s = M0_CONF_CAST(m0_conf_obj_grandparent(&d->ck_sdev->sd_obj),
				 m0_conf_service);
		check_obj(&s->cs_obj);
	};

	m0_conf_diter_fini(&it);
	M0_UT_ASSERT(rc == 0);
}

static void
all_fs_to_disks_check(struct m0_confc *confc, struct m0_conf_obj *root)
{
	struct m0_conf_diter it;
	int                  rc;

	rc = m0_conf_diter_init(&it, confc, root,
				M0_CONF_ROOT_SITES_FID,
				M0_CONF_SITE_RACKS_FID,
				M0_CONF_RACK_ENCLS_FID,
				M0_CONF_ENCLOSURE_CTRLS_FID,
				M0_CONF_CONTROLLER_DRIVES_FID);
	M0_UT_ASSERT(rc == 0);

	while ((rc = m0_conf_diter_next_sync(&it, NULL)) == M0_CONF_DIRNEXT)
		check_obj(m0_conf_diter_result(&it));

	m0_conf_diter_fini(&it);
	M0_UT_ASSERT(rc == 0);
}

static bool _filter_service(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE;
}

static void all_fs_to_sdevs_check(struct m0_confc *confc,
				  struct m0_conf_obj *root, bool filter)
{
	struct m0_conf_diter it;
	int                  rc;

	rc = m0_conf_diter_init(&it, confc, root,
				M0_CONF_ROOT_NODES_FID,
				M0_CONF_NODE_PROCESSES_FID,
				M0_CONF_PROCESS_SERVICES_FID,
				M0_CONF_SERVICE_SDEVS_FID);
	M0_UT_ASSERT(rc == 0);

	while ((rc = m0_conf_diter_next_sync(&it, filter ?
						  _filter_service :
						  NULL)) == M0_CONF_DIRNEXT)
		check_obj(m0_conf_diter_result(&it));

	m0_conf_diter_fini(&it);
	M0_UT_ASSERT(rc == 0);
}

static void conf_diter_test(const char *confd_addr,
			    struct m0_rpc_machine *rpc_mach,
			    const char *local_conf)
{
	struct m0_confc     confc;
	struct m0_conf_obj *root = NULL;
	int                 rc;

	M0_SET0(&confc);
	rc = m0_confc_init(&confc, &m0_conf_ut_grp, confd_addr, rpc_mach,
			   local_conf);
	M0_UT_ASSERT(rc == 0);
	rc = m0_confc_open_sync(&root, confc.cc_root, M0_FID0);
	M0_UT_ASSERT(rc == 0);

	all_fs_to_disks_check(&confc, root);
	all_fs_to_sdevs_check(&confc, root, true);
	all_fs_to_sdevs_check(&confc, root, false);
	all_fs_to_diskv_check(&confc, root);
	m0_confc_close(root);

	m0_confc_fini(&confc);
}

static void test_diter_local(void)
{
	char *confstr = NULL;
	int   rc;

	rc = m0_file_read(M0_UT_PATH("diter.xc"), &confstr);
	M0_UT_ASSERT(rc == 0);
	conf_diter_test(NULL, NULL, confstr);
	m0_free(confstr);
}

static void test_diter_net(void)
{
	struct m0_rpc_machine mach;
	int                   rc;
#define NAME(ext) "utconfd" ext
	char *argv[] = {
		NAME(""), "-T", "AD", "-D", NAME(".db"),
		"-S", NAME(".stob"), "-A", "linuxstob:"NAME("-addb.stob"),
		"-w", "10", "-e", SERVER_ENDPOINT, "-H", SERVER_ENDPOINT_ADDR,
		"-f", M0_UT_CONF_PROCESS,
		"-c", M0_UT_PATH("diter.xc")
	};
	struct m0_rpc_server_ctx confd = {
		.rsx_xprts         = &m0_conf_ut_xprt,
		.rsx_xprts_nr      = 1,
		.rsx_argv          = argv,
		.rsx_argc          = ARRAY_SIZE(argv),
		.rsx_log_file_name = NAME(".log")
	};
#undef NAME

	M0_SET0(&mach);
	rc = m0_rpc_server_start(&confd);
	M0_UT_ASSERT(rc == 0);

	rc = m0_ut_rpc_machine_start(&mach, m0_conf_ut_xprt,
				     CLIENT_ENDPOINT_ADDR);
	M0_UT_ASSERT(rc == 0);

	conf_diter_test(SERVER_ENDPOINT_ADDR, &mach, NULL);

	m0_ut_rpc_machine_stop(&mach);
	m0_rpc_server_stop(&confd);
}

static void test_diter_invalid_input(void)
{
	struct m0_confc       confc;
	struct m0_conf_diter  it;
	struct m0_conf_obj   *root;
	char                 *confstr = NULL;
	int                   rc;

	rc = m0_file_read(M0_UT_PATH("diter.xc"), &confstr);
	M0_UT_ASSERT(rc == 0);
	rc = m0_confc_init(&confc, &m0_conf_ut_grp, NULL, NULL, confstr);
	M0_UT_ASSERT(rc == 0);
	m0_free0(&confstr);
	rc = m0_confc_open_sync(&root, confc.cc_root, M0_FID0);
	M0_UT_ASSERT(rc == 0);

	rc = m0_conf_diter_init(&it, &confc, root,
				M0_CONF_NODE_PROCESSES_FID,
				M0_CONF_PROCESS_SERVICES_FID,
				M0_CONF_SERVICE_SDEVS_FID);
	M0_UT_ASSERT(rc == 0);

	while ((rc = m0_conf_diter_next_sync(&it, NULL)) == M0_CONF_DIRNEXT)
		check_obj(m0_conf_diter_result(&it));

	M0_UT_ASSERT(rc == -ENOENT);
	m0_conf_diter_fini(&it);
	m0_confc_close(root);
	m0_confc_fini(&confc);
}

/* ------------------------------------------------------------------ */
struct m0_ut_suite conf_diter_ut = {
	.ts_name  = "conf-diter-ut",
	.ts_init  = m0_conf_ut_ast_thread_init,
	.ts_fini  = m0_conf_ut_ast_thread_fini,
	.ts_tests = {
		{ "local",         test_diter_local },
		{ "net",           test_diter_net },
		{ "invalid input", test_diter_invalid_input },
		{ NULL, NULL }
	}
};

#undef M0_TRACE_SUBSYSTEM
