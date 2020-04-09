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
 * Original author: Egor Nikulenkov <egor.nikulenkov@seagate.com>
 * Original creation date: 06-Mar-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "spiel/spiel.h"
#include "spiel/spiel_internal.h"
#include "spiel/ut/spiel_ut_common.h"
#include "conf/obj_ops.h"     /* M0_CONF_DIRNEXT */
#include "conf/confd.h"       /* m0_confd_stype */
#include "module/instance.h"  /* m0_get */
#include "stob/domain.h"      /* m0_stob_domain */
#include "rpc/rpc_opcodes.h"  /* M0_RPC_SESSION_ESTABLISH_REP_OPCODE */
#include "rm/rm_service.h"    /* m0_rms_type */
#include "lib/finject.h"
#include "lib/fs.h"           /* m0_file_read */
#include "conf/ut/common.h"   /* m0_conf_ut_ast_thread_fini */
#include "ut/misc.h"          /* M0_UT_PATH */
#include "ut/ut.h"
#include "mero/version.h"     /* m0_build_info_get */
#include "ha/link.h"          /* m0_ha_link_chan */


static struct m0_spiel spiel;

static void spiel_ci_ut_init(void)
{
	m0_spiel__ut_init(&spiel, M0_SRC_PATH("spiel/ut/conf.xc"), true);
	m0_confd_stype.rst_keep_alive = true;
	m0_rms_type.rst_keep_alive = true;
}

static void spiel_ci_ut_fini(void)
{
	m0_confd_stype.rst_keep_alive = false;
	m0_rms_type.rst_keep_alive = false;
	m0_spiel__ut_fini(&spiel, true);
}

static void spiel_ci_ut_ha_state_set(const struct m0_fid *fid, uint32_t state)
{
	struct m0_ha_note note = { .no_id = *fid, .no_state = state };
	struct m0_ha_nvec nvec = { .nv_nr = 1, .nv_note = &note };
	struct m0_ha_link *hl;
	uint64_t           tag;

	hl = m0_get()->i_ha_link;
	tag = m0_ha_msg_nvec_send(&nvec, 0, false, M0_HA_NVEC_SET, hl);
	m0_ha_link_wait_delivery(hl, tag);
}

static void test_spiel_service_cmds(void)
{
	const struct m0_fid svc_fid = M0_FID_TINIT(
			M0_CONF_SERVICE_TYPE.cot_ftype.ft_id, 1, 10);
	const struct m0_fid svc_invalid_fid = M0_FID_TINIT(
			M0_CONF_SERVICE_TYPE.cot_ftype.ft_id, 1, 13);
	const struct m0_fid top_level_rm_fid = M0_FID_TINIT(
			M0_CONF_SERVICE_TYPE.cot_ftype.ft_id, 1, 22);
	int                 rc;
	int                 status;

	spiel_ci_ut_init();

	rc = m0_spiel_service_status(&spiel, &svc_fid, &status);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(status == M0_RST_STARTED);

	/* Doing `service stop` intialised during startup. */
	rc = m0_spiel_service_quiesce(&spiel, &svc_fid);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_service_stop(&spiel, &svc_fid);
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_service_status(&spiel, &svc_fid, &status);
	M0_UT_ASSERT(rc != 0);

	rc = m0_spiel_service_init(&spiel, &svc_fid);
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_service_status(&spiel, &svc_fid, &status);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(status == M0_RST_INITIALISED);

	rc = m0_spiel_service_start(&spiel, &svc_fid);
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_service_status(&spiel, &svc_fid, &status);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(status == M0_RST_STARTED);

	rc = m0_spiel_service_health(&spiel, &svc_fid);
	/* This is true while the service doesn't implement rso_health */
	M0_UT_ASSERT(rc == M0_HEALTH_UNKNOWN);

	rc = m0_spiel_service_quiesce(&spiel, &svc_fid);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_service_stop(&spiel, &svc_fid);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_service_health(&spiel, &svc_invalid_fid);
	M0_UT_ASSERT(rc == -ENOENT);

	/* Stopping of Top Level RM is disallowed */
	rc = m0_spiel_service_stop(&spiel, &top_level_rm_fid);
	M0_UT_ASSERT(rc == -EPERM);
	spiel_ci_ut_fini();
}

static void test_spiel_process_services_list(void)
{
	struct m0_spiel_running_svc *svcs;
	int                          rc;
	int                          i;
	const struct m0_fid          proc_fid = M0_FID_TINIT(
				M0_CONF_PROCESS_TYPE.cot_ftype.ft_id, 1, 5);

	spiel_ci_ut_init();
	rc = m0_spiel_process_list_services(&spiel, &proc_fid, &svcs);
	M0_UT_ASSERT(rc > 0);
	M0_UT_ASSERT(svcs != NULL);
	for (i = 0; i < rc; ++i)
		m0_free(svcs[i].spls_name);
	m0_free(svcs);

	spiel_ci_ut_fini();
}

static void test_spiel_process_cmds(void)
{
	int                 rc;
	const struct m0_fid process_fid = M0_FID_TINIT(
				M0_CONF_PROCESS_TYPE.cot_ftype.ft_id, 1, 5);
	const struct m0_fid process_second_fid = M0_FID_TINIT(
				M0_CONF_PROCESS_TYPE.cot_ftype.ft_id, 1, 13);
	const struct m0_fid process_invalid_fid = M0_FID_TINIT(
				M0_CONF_SERVICE_TYPE.cot_ftype.ft_id, 4, 15);
	char *libpath;

	spiel_ci_ut_init();
	/* Reconfig */
	rc = m0_spiel_process_reconfig(&spiel, &process_invalid_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	m0_fi_enable_once("m0_alloc", "fail_allocation");
	rc = m0_spiel_process_reconfig(&spiel, &process_fid);
	M0_UT_ASSERT(rc == -ENOMEM);

	rc = m0_spiel_process_reconfig(&spiel, &process_second_fid);
	M0_UT_ASSERT(rc == -ENOENT);

	m0_fi_enable_once("ss_process_reconfig", "unit_test");
	rc = m0_spiel_process_reconfig(&spiel, &process_fid);
	M0_UT_ASSERT(rc == 0);

	/* Health */
	rc = m0_spiel_process_health(&spiel, &process_invalid_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_process_health(&spiel, &process_fid);
	M0_UT_ASSERT(rc == M0_HEALTH_GOOD);

	/* Load test library. */
	rc = asprintf(&libpath, "%s/%s", m0_build_info_get()->bi_build_dir,
		      "ut/.libs/libtestlib.so.0.0.0");
	M0_UT_ASSERT(rc >= 0);
	rc = m0_spiel_process_lib_load(&spiel, &process_fid, libpath);
	M0_UT_ASSERT(rc == 0);
	free(libpath);
	/* Cleanup after the libtestlib.so. */
	m0_fi_disable("sss_process_lib_load_testlib_test", "loaded");

	/* Load non-existent library. */
	rc = m0_spiel_process_lib_load(&spiel, &process_fid, "/funnylib");
	M0_UT_ASSERT(rc == -EINVAL);

	/* Stop */
	rc = m0_spiel_process_stop(&spiel, &process_invalid_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	m0_fi_enable_once("m0_ss_process_stop_fop_release", "no_kill");
	rc = m0_spiel_process_stop(&spiel, &process_fid);
	M0_UT_ASSERT(rc == 0);

	/* Quiesce */
	rc = m0_spiel_process_quiesce(&spiel, &process_invalid_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	/**
	 * Must be last test in tests set -
	 * because switch off services on server side
	 */
	rc = m0_spiel_process_quiesce(&spiel, &process_fid);
	M0_UT_ASSERT(rc == 0);
	spiel_ci_ut_fini();
}

static bool spiel_stob_exists(uint64_t cid)
{
	struct m0_storage_devs *devs = &m0_get()->i_storage_devs;
	struct m0_stob_id       id;
	struct m0_stob         *stob;
	int                     rc;

	/* Current function is aware of internal logic of adstob storage */
	M0_PRE(devs->sds_type == M0_STORAGE_DEV_TYPE_AD);

	m0_stob_id_make(0, cid, &devs->sds_back_domain->sd_id, &id);
	rc = m0_stob_lookup(&id, &stob);
	if (rc == 0 && stob != NULL)
		m0_stob_put(stob);
	return rc == 0 && stob != NULL;
}

static void test_spiel_device_cmds(void)
{
	/*
	 * According to ut/conf.xc:
	 * - disk-78 is associated with sdev-74;
	 * - sdev-74 belongs to IO service-9 of process-5;
	 * - disk-55 is associated with sdev-51;
	 * - sdev-51 belongs to IO service-27 of process-49;
	 * - disk-23 does not exist.
	 */
	uint64_t            io_sdev = 5;
	uint64_t            foreign_sdev = 51;
	const struct m0_fid io_disk = M0_FID_TINIT(
				M0_CONF_DRIVE_TYPE.cot_ftype.ft_id, 1, 78);
	const struct m0_fid foreign_disk = M0_FID_TINIT(
				M0_CONF_DRIVE_TYPE.cot_ftype.ft_id, 1, 55);
	const struct m0_fid nosuch_disk = M0_FID_TINIT(
				M0_CONF_DRIVE_TYPE.cot_ftype.ft_id, 1, 23);
	int                 rc;
	uint32_t            ha_state;

	spiel_ci_ut_init();
	/*
	 * After mero startup devices are online by default,
	 * so detach them at first.
	 */
	m0_fi_enable_once("m0_rpc_reply_post", "delay_reply");
	m0_fi_enable_once("spiel_cmd_send", "timeout");
	rc = m0_spiel_device_detach(&spiel, &io_disk);
	M0_UT_ASSERT(rc == -ETIMEDOUT);

	rc = m0_spiel_device_detach(&spiel, &io_disk);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(!spiel_stob_exists(io_sdev));

	rc = m0_spiel_device_format(&spiel, &io_disk);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(!spiel_stob_exists(io_sdev));

	/*
	 * in the course of action test as well if device state is updated on
	 * remote side when attaching
	 */
	spiel_ci_ut_ha_state_set(&io_disk, M0_NC_FAILED);
	m0_fi_enable_once("m0_storage_dev_new_by_conf", "no_real_dev");
	rc = m0_spiel_device_attach_state(&spiel, &io_disk, &ha_state);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(spiel_stob_exists(io_sdev));

	/*
	 * Server part processes foreign_disk as disk from another process.
	 */
	rc = m0_spiel_device_format(&spiel, &foreign_disk);
	M0_UT_ASSERT(rc == 0);

	m0_fi_enable_once("m0_storage_dev_new_by_conf", "no_real_dev");
	rc = m0_spiel_device_attach(&spiel, &foreign_disk);
	M0_UT_ASSERT(rc == 0);
	/*
	 * Stob is not created for device that does not belong to IO service
	 * of current process, which is process-5.
	 */
	M0_UT_ASSERT(!spiel_stob_exists(foreign_sdev));

	rc = m0_spiel_device_detach(&spiel, &foreign_disk);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(!spiel_stob_exists(foreign_sdev));

	rc = m0_spiel_device_attach(&spiel, &nosuch_disk);
	M0_UT_ASSERT(rc == -ENOENT);
	spiel_ci_ut_fini();

	/*
	 * XXX This "once" injection is not triggered. It may be redundant
	 * or the test expects call of the function and it doesn't happen.
	 */
	m0_fi_disable("m0_storage_dev_new_by_conf", "no_real_dev");
}

static void test_spiel_service_order(void)
{
	int                          rc;
	int                          i;
	struct m0_spiel_running_svc *svcs;
	const struct m0_fid          process_fid = M0_FID_TINIT(
				M0_CONF_PROCESS_TYPE.cot_ftype.ft_id, 1, 5);
	const struct m0_fid          svc_fid = M0_FID_TINIT(
				M0_CONF_SERVICE_TYPE.cot_ftype.ft_id, 1, 10);
	bool                         found = false;

	spiel_ci_ut_init();
	/* Doing process quiesce */
	rc = m0_spiel_process_quiesce(&spiel, &process_fid);
	M0_UT_ASSERT(rc == 0);

	/* Doing `service stop` intialised during startup. */
	rc = m0_spiel_service_stop(&spiel, &svc_fid);
	M0_UT_ASSERT(rc == 0);

	/* Doing `service start` after process quiesce. */
	rc = m0_spiel_service_init(&spiel, &svc_fid);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_service_start(&spiel, &svc_fid);
	M0_UT_ASSERT(rc == 0);

	/* Read Service list - rmservice must be started */
	rc = m0_spiel_process_list_services(&spiel, &process_fid, &svcs);
	M0_UT_ASSERT(rc > 0);
	M0_UT_ASSERT(svcs != NULL);
	for (i = 0; i < rc; ++i) {
		found |= m0_streq(svcs[i].spls_name, "M0_CST_RMS");
		m0_free(svcs[i].spls_name);
	}
	m0_free(svcs);
	M0_UT_ASSERT(found);
	spiel_ci_ut_fini();
}

static uint64_t test_spiel_fs_stats_sdevs_total(struct m0_confc        *confc,
						struct m0_conf_service *ios)
{
	struct m0_conf_obj  *sdevs_dir = &ios->cs_sdevs->cd_obj;
	struct m0_conf_obj  *obj;
	struct m0_conf_sdev *sdev;
	uint64_t             total = 0;
	int                  rc;

	rc = m0_confc_open_sync(&sdevs_dir, sdevs_dir, M0_FID0);
	M0_UT_ASSERT(rc == 0);
	obj = NULL;
	while (m0_confc_readdir_sync(sdevs_dir, &obj) > 0) {
		sdev = M0_CONF_CAST(obj, m0_conf_sdev);
		M0_UT_ASSERT(!m0_addu64_will_overflow(total, sdev->sd_size));
		total += sdev->sd_size;
	}

	m0_confc_close(obj);
	m0_confc_close(sdevs_dir);
	return total;
}

static bool test_spiel_filter_service(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE;
}

static uint64_t test_spiel_fs_ios_total(void)
{
	struct m0_confc         confc;
	struct m0_conf_diter    it;
	struct m0_conf_obj     *root_obj;
	struct m0_conf_service *svc;
	char                   *confstr = NULL;
	int                     rc;
	uint64_t                svc_total = 0;
	uint64_t                total = 0;

	rc = m0_file_read(M0_UT_PATH("conf.xc"), &confstr);
	M0_UT_ASSERT(rc == 0);
	rc = m0_confc_init(&confc, m0_locality0_get()->lo_grp, NULL, NULL,
			   confstr);
	M0_UT_ASSERT(rc == 0);
	m0_free0(&confstr);
	rc = m0_confc_open_sync(&root_obj, confc.cc_root, M0_FID0);
	M0_UT_ASSERT(rc == 0);
	rc = m0_conf_diter_init(&it, &confc, root_obj,
				M0_CONF_ROOT_NODES_FID,
				M0_CONF_NODE_PROCESSES_FID,
				M0_CONF_PROCESS_SERVICES_FID);
	M0_UT_ASSERT(rc == 0);
	while (m0_conf_diter_next_sync(&it, test_spiel_filter_service) ==
	       M0_CONF_DIRNEXT) {
		svc = M0_CONF_CAST(m0_conf_diter_result(&it), m0_conf_service);
		if(svc->cs_type == M0_CST_IOS) {
			svc_total = test_spiel_fs_stats_sdevs_total(&confc,
								    svc);
			M0_UT_ASSERT(!m0_addu64_will_overflow(total,
							      svc_total));
			total += svc_total;
		}
	}
	m0_conf_diter_fini(&it);
	m0_confc_close(root_obj);
	m0_confc_fini(&confc);
	return total;
}

static void spiel_fs_stats_check(const struct m0_fs_stats *stats)
{
	M0_UT_ASSERT(stats->fs_free_disk <= stats->fs_total_disk);
	M0_UT_ASSERT(stats->fs_svc_total >= stats->fs_svc_replied);
}

extern uint32_t m0_rpc__filter_opcode[4];

void test_spiel_fs_stats(void)
{
	int                 rc;
	uint64_t            ios_total;
	struct m0_fs_stats  fs_stats = {0};
	struct m0_ha_note   note = {
		.no_id = M0_FID_TINIT('r', 1, 5),
		.no_state = M0_NC_FAILED
	};
	struct m0_ha_nvec   nvec = {
		.nv_nr = 1,
		.nv_note = &note
	};

	m0_fi_enable_once("cs_storage_devs_init", "init_via_conf");
	m0_fi_enable("m0_storage_dev_new_by_conf", "no_real_dev");
	m0_fi_enable("m0_storage_dev_new", "no_real_dev");
	spiel_ci_ut_init();
	m0_fi_disable("m0_storage_dev_new_by_conf", "no_real_dev");
	m0_fi_disable("m0_storage_dev_new", "no_real_dev");

	m0_fi_enable_once("spiel__item_enlist", "alloc fail");
	rc = m0_spiel_filesystem_stats_fetch(&spiel, &fs_stats);
	M0_UT_ASSERT(rc == -ENOMEM);

	/* spiel__proc_is_to_update_stats */
	m0_fi_enable_once("spiel__proc_is_to_update_stats", "open_by_fid");
	rc = m0_spiel_filesystem_stats_fetch(&spiel, &fs_stats);
	M0_UT_ASSERT(rc == 0);
	spiel_fs_stats_check(&fs_stats);
	m0_fi_enable_once("spiel__proc_is_to_update_stats", "diter_init");
	rc = m0_spiel_filesystem_stats_fetch(&spiel, &fs_stats);
	M0_UT_ASSERT(rc == 0);
	spiel_fs_stats_check(&fs_stats);
	m0_fi_enable_once("spiel__proc_is_to_update_stats", "ha_update");
	rc = m0_spiel_filesystem_stats_fetch(&spiel, &fs_stats);
	M0_UT_ASSERT(rc == 0);
	spiel_fs_stats_check(&fs_stats);
	m0_ha_client_add(spiel.spl_core.spc_confc);
	m0_ha_state_accept(&nvec, false);
	rc = m0_spiel_filesystem_stats_fetch(&spiel, &fs_stats);
	M0_UT_ASSERT(rc == 0);
	spiel_fs_stats_check(&fs_stats);
	note.no_state = M0_NC_ONLINE;
	m0_ha_state_accept(&nvec, false);
	m0_ha_client_del(spiel.spl_core.spc_confc);

	/* spiel_process__health_async */
	m0_fi_enable_once("m0_net_end_point_create", "fake_error");
	rc = m0_spiel_filesystem_stats_fetch(&spiel, &fs_stats);
	M0_UT_ASSERT(rc == 0);
	spiel_fs_stats_check(&fs_stats);
	m0_fi_enable_once("spiel_process__health_async", "obj_find");
	rc = m0_spiel_filesystem_stats_fetch(&spiel, &fs_stats);
	M0_UT_ASSERT(rc == 0);
	spiel_fs_stats_check(&fs_stats);

	/* spiel_proc_item_rlink_cb */
	m0_fi_enable("item_received_fi", "drop_opcode");
	m0_rpc__filter_opcode[0] = M0_RPC_SESSION_ESTABLISH_REP_OPCODE;
	rc = m0_spiel_filesystem_stats_fetch(&spiel, &fs_stats);
	m0_fi_disable("item_received_fi", "drop_opcode");
	M0_UT_ASSERT(rc == 0);
	spiel_fs_stats_check(&fs_stats);
	m0_fi_enable_once("spiel_proc_item_rlink_cb", "alloc_fail");
	rc = m0_spiel_filesystem_stats_fetch(&spiel, &fs_stats);
	M0_UT_ASSERT(rc == 0);
	spiel_fs_stats_check(&fs_stats);
	m0_fi_enable_once("spiel_proc_item_rlink_cb", "rpc_post");
	rc = m0_spiel_filesystem_stats_fetch(&spiel, &fs_stats);
	M0_UT_ASSERT(rc == 0);
	spiel_fs_stats_check(&fs_stats);
	/* spiel_process_health_replied_ast */
	m0_fi_enable_once("spiel_process_health_replied_ast", "item_error");
	rc = m0_spiel_filesystem_stats_fetch(&spiel, &fs_stats);
	M0_UT_ASSERT(rc == 0);
	spiel_fs_stats_check(&fs_stats);
	m0_fi_enable_once("spiel_process_health_replied_ast", "overflow");
	rc = m0_spiel_filesystem_stats_fetch(&spiel, &fs_stats);
	M0_UT_ASSERT(rc == -EOVERFLOW);
	/* test the existent one */
	m0_fi_enable("ss_ios_stats_ingest", "take_dsx_in_effect");
	rc = m0_spiel_filesystem_stats_fetch(&spiel, &fs_stats);
	M0_UT_ASSERT(rc == 0);
	m0_fi_disable("ss_ios_stats_ingest", "take_dsx_in_effect");
	spiel_fs_stats_check(&fs_stats);
	ios_total = test_spiel_fs_ios_total();
	M0_UT_ASSERT(fs_stats.fs_total_disk > ios_total);
	spiel_ci_ut_fini();
}

static void spiel_repair_start(const struct m0_fid *pool_fid,
			       const struct m0_fid *svc_fid,
			       enum m0_repreb_type  type)
{
	struct m0_spiel_repreb_status *status;
	enum m0_cm_status              state;
	int                            rc;

	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_repair_start(&spiel, pool_fid) :
		m0_spiel_dix_repair_start(&spiel, pool_fid);
	M0_UT_ASSERT(rc == 0);
	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_repair_status(&spiel, pool_fid, &status) :
		m0_spiel_dix_repair_status(&spiel, pool_fid, &status);
	M0_UT_ASSERT(rc == 1);
	state = status[0].srs_state;
	M0_UT_ASSERT(m0_fid_eq(&status[0].srs_fid, svc_fid));
	M0_UT_ASSERT(M0_IN(state, (CM_STATUS_IDLE, CM_STATUS_STARTED)));
	M0_UT_ASSERT(status[0].srs_progress >= 0);
	m0_free(status);
}

static void wait_for_repair_rebalance(enum m0_repreb_type type,
				      enum m0_cm_op op,
				      struct m0_spiel_repreb_status **status,
				      const struct m0_fid *pool_fid,
				      const struct m0_fid *svc_fid)
{
	enum m0_cm_status state;
	int               rc;

	while(1) {
		if (type == M0_REPREB_TYPE_SNS)
			rc = op == CM_OP_REPAIR ?
				m0_spiel_sns_repair_status(&spiel, pool_fid,
							   status) :
				m0_spiel_sns_rebalance_status(&spiel, pool_fid,
							      status);
		else
			rc = op == CM_OP_REPAIR ?
				m0_spiel_dix_repair_status(&spiel, pool_fid,
							   status) :
				m0_spiel_dix_rebalance_status(&spiel, pool_fid,
							      status);

		M0_UT_ASSERT(rc == 1);
		state = (*status)[0].srs_state;
		M0_UT_ASSERT(m0_fid_eq(&(*status)[0].srs_fid, svc_fid));
		M0_UT_ASSERT(M0_IN(state, (CM_STATUS_IDLE,
					   CM_STATUS_STARTED,
					   CM_STATUS_PAUSED,
					   CM_STATUS_FAILED)));
		M0_UT_ASSERT((*status)[0].srs_progress >= 0);

		if (M0_IN(state, (CM_STATUS_IDLE, CM_STATUS_PAUSED,
				  CM_STATUS_FAILED)))
			break;
		m0_free(*status);
	}
}

static void test_spiel_pool_repair(enum m0_repreb_type type)
{
	const struct m0_fid         pool_fid = (type == M0_REPREB_TYPE_SNS) ?
		M0_FID_TINIT(M0_CONF_POOL_TYPE.cot_ftype.ft_id, 1, 4) :
		M0_FID_TINIT(M0_CONF_POOL_TYPE.cot_ftype.ft_id, 1, 100);
	struct m0_fid               svc_fid = (type == M0_REPREB_TYPE_SNS) ?
		M0_FID_TINIT(M0_CONF_SERVICE_TYPE.cot_ftype.ft_id, 1, 9) :
		M0_FID_TINIT(M0_CONF_SERVICE_TYPE.cot_ftype.ft_id, 1, 11);
	struct m0_fid               pool_invalid_fid = M0_FID_TINIT(
				M0_CONF_POOL_TYPE.cot_ftype.ft_id, 1, 3);
	struct m0_fid               invalid_fid = M0_FID_TINIT(
				M0_CONF_SERVICE_TYPE.cot_ftype.ft_id, 1, 4);
	const struct m0_fid         io_disk = M0_FID_TINIT(
				M0_CONF_DRIVE_TYPE.cot_ftype.ft_id, 1, 78);
	struct m0_spiel_repreb_status *status = NULL;
	enum m0_cm_status           state;
	int                         rc;

	M0_ASSERT(M0_IN(type, (M0_REPREB_TYPE_SNS, M0_REPREB_TYPE_DIX)));
	spiel_ci_ut_init();
	m0_fi_enable("ready", "no_wait");
	m0_fi_enable("m0_ha_local_state_set", "no_ha");
	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_repair_start(&spiel, &invalid_fid) :
		m0_spiel_dix_repair_start(&spiel, &invalid_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_repair_start(&spiel, &pool_invalid_fid) :
		m0_spiel_dix_repair_start(&spiel, &pool_invalid_fid);
	M0_UT_ASSERT(rc == -ENOENT);

	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_repair_quiesce(&spiel, &invalid_fid) :
		m0_spiel_dix_repair_quiesce(&spiel, &invalid_fid);
	M0_UT_ASSERT(rc == -EINVAL);
	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_repair_abort(&spiel, &invalid_fid) :
		m0_spiel_dix_repair_abort(&spiel, &invalid_fid);
	M0_UT_ASSERT(rc == -EINVAL);
	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_repair_continue(&spiel, &invalid_fid) :
		m0_spiel_dix_repair_continue(&spiel, &invalid_fid);
	M0_UT_ASSERT(rc == -EINVAL);
	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_repair_status(&spiel, &invalid_fid, &status) :
		m0_spiel_dix_repair_status(&spiel, &invalid_fid, &status);
	M0_UT_ASSERT(rc == -EINVAL);
	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_repair_status(&spiel, &pool_fid, &status) :
		m0_spiel_dix_repair_status(&spiel, &pool_fid, &status);
	M0_UT_ASSERT(rc == 1);
	M0_UT_ASSERT(m0_fid_eq(&status[0].srs_fid, &svc_fid));
	M0_UT_ASSERT(status[0].srs_state == CM_STATUS_IDLE);
	M0_UT_ASSERT(status[0].srs_progress >= 0);
	m0_free0(&status);

	spiel_ci_ut_ha_state_set(&io_disk, M0_NC_FAILED);
	spiel_ci_ut_ha_state_set(&io_disk, M0_NC_REPAIR);
	spiel_repair_start(&pool_fid, &svc_fid, type);

	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_repair_abort(&spiel, &pool_fid) :
		m0_spiel_dix_repair_abort(&spiel, &pool_fid);
	M0_UT_ASSERT(rc == 0);
	wait_for_repair_rebalance(type, CM_OP_REPAIR, &status, &pool_fid, &svc_fid);
	state = status[0].srs_state;
	M0_UT_ASSERT(m0_fid_eq(&status[0].srs_fid, &svc_fid));
	M0_UT_ASSERT(state == CM_STATUS_IDLE);
	M0_UT_ASSERT(status[0].srs_progress > 0);
	m0_free0(&status);

	spiel_repair_start(&pool_fid, &svc_fid, type);

	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_repair_quiesce(&spiel, &pool_fid) :
		m0_spiel_dix_repair_quiesce(&spiel, &pool_fid);
	M0_UT_ASSERT(rc == 0);

	wait_for_repair_rebalance(type, CM_OP_REPAIR, &status, &pool_fid, &svc_fid);
	M0_UT_ASSERT(m0_fid_eq(&status[0].srs_fid, &svc_fid));
	M0_UT_ASSERT(status[0].srs_state == CM_STATUS_PAUSED);
	M0_UT_ASSERT(status[0].srs_progress >= 0);
	m0_free0(&status);

	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_repair_continue(&spiel, &pool_fid) :
		m0_spiel_dix_repair_continue(&spiel, &pool_fid);
	M0_UT_ASSERT(rc == 0);
	wait_for_repair_rebalance(type, CM_OP_REPAIR, &status, &pool_fid, &svc_fid);
	spiel_ci_ut_ha_state_set(&io_disk, M0_NC_REPAIRED);
	m0_free0(&status);
	m0_fi_disable("ready", "no_wait");
	m0_fi_disable("m0_ha_local_state_set", "no_ha");
	spiel_ci_ut_fini();
}

static void test_spiel_sns_repair(void)
{
	test_spiel_pool_repair(M0_REPREB_TYPE_SNS);
}

static void test_spiel_dix_repair(void)
{
	test_spiel_pool_repair(M0_REPREB_TYPE_DIX);
}

static void test_spiel_pool_rebalance(enum m0_repreb_type type)
{
	const struct m0_fid            pool_fid = (type == M0_REPREB_TYPE_SNS) ?
		M0_FID_TINIT(M0_CONF_POOL_TYPE.cot_ftype.ft_id, 1, 4) :
		M0_FID_TINIT(M0_CONF_POOL_TYPE.cot_ftype.ft_id, 1, 100);
	struct m0_fid                  svc_fid = (type == M0_REPREB_TYPE_SNS) ?
		M0_FID_TINIT(M0_CONF_SERVICE_TYPE.cot_ftype.ft_id, 1, 9) :
		M0_FID_TINIT(M0_CONF_SERVICE_TYPE.cot_ftype.ft_id, 1, 11);
	struct m0_fid                  pool_invalid_fid = M0_FID_TINIT(
				M0_CONF_POOL_TYPE.cot_ftype.ft_id, 1, 3);
	struct m0_fid                  invalid_fid = M0_FID_TINIT(
				M0_CONF_SERVICE_TYPE.cot_ftype.ft_id, 1, 4);
	const struct m0_fid            io_disk = M0_FID_TINIT(
				M0_CONF_DRIVE_TYPE.cot_ftype.ft_id, 1, 78);
	struct m0_spiel_repreb_status *status;
	enum m0_cm_status              state;
	int                            rc;

	M0_ASSERT(M0_IN(type, (M0_REPREB_TYPE_SNS, M0_REPREB_TYPE_DIX)));
	spiel_ci_ut_init();
	m0_fi_enable("ready", "no_wait");
	m0_fi_enable("m0_ha_local_state_set", "no_ha");
	spiel_ci_ut_ha_state_set(&io_disk, M0_NC_FAILED);
	spiel_ci_ut_ha_state_set(&io_disk, M0_NC_REPAIR);
	spiel_ci_ut_ha_state_set(&io_disk, M0_NC_REPAIRED);
	spiel_ci_ut_ha_state_set(&io_disk, M0_NC_REBALANCE);
	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_rebalance_start(&spiel, &invalid_fid) :
		m0_spiel_dix_rebalance_start(&spiel, &invalid_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_rebalance_start(&spiel, &pool_invalid_fid) :
		m0_spiel_dix_rebalance_start(&spiel, &pool_invalid_fid);
	M0_UT_ASSERT(rc == -ENOENT);

	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_rebalance_quiesce(&spiel, &invalid_fid) :
		m0_spiel_dix_rebalance_quiesce(&spiel, &invalid_fid);
	M0_UT_ASSERT(rc == -EINVAL);
	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_rebalance_continue(&spiel, &invalid_fid) :
		m0_spiel_dix_rebalance_continue(&spiel, &invalid_fid);
	M0_UT_ASSERT(rc == -EINVAL);
	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_rebalance_status(&spiel, &invalid_fid, &status) :
		m0_spiel_dix_rebalance_status(&spiel, &invalid_fid, &status);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_rebalance_status(&spiel, &pool_fid, &status) :
		m0_spiel_dix_rebalance_status(&spiel, &pool_fid, &status);
	M0_UT_ASSERT(rc == 1);
	M0_UT_ASSERT(m0_fid_eq(&status[0].srs_fid, &svc_fid));
	M0_UT_ASSERT(status[0].srs_state == CM_STATUS_IDLE);
	M0_UT_ASSERT(status[0].srs_progress >= 0);
	m0_free(status);

	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_rebalance_start(&spiel, &pool_fid) :
		m0_spiel_dix_rebalance_start(&spiel, &pool_fid);
	M0_UT_ASSERT(rc == 0);

	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_rebalance_status(&spiel, &pool_fid, &status) :
		m0_spiel_dix_rebalance_status(&spiel, &pool_fid, &status);
	M0_UT_ASSERT(rc == 1);
	state = status[0].srs_state;
	M0_UT_ASSERT(m0_fid_eq(&status[0].srs_fid, &svc_fid));
	M0_UT_ASSERT(M0_IN(state, (CM_STATUS_IDLE, CM_STATUS_STARTED,
				   CM_STATUS_FAILED)));
	M0_UT_ASSERT(status[0].srs_progress >= 0);
	m0_free(status);
	if (M0_IN(state, (CM_STATUS_IDLE, CM_STATUS_FAILED)))
		goto done;

	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_rebalance_quiesce(&spiel, &pool_fid) :
		m0_spiel_dix_rebalance_quiesce(&spiel, &pool_fid);
	M0_UT_ASSERT(rc == 0);

	wait_for_repair_rebalance(type, CM_OP_REBALANCE, &status, &pool_fid, &svc_fid);
	M0_UT_ASSERT(m0_fid_eq(&status[0].srs_fid, &svc_fid));
	M0_UT_ASSERT(status[0].srs_state == CM_STATUS_PAUSED);
	M0_UT_ASSERT(status[0].srs_progress >= 0);
	m0_free(status);

	rc = (type == M0_REPREB_TYPE_SNS) ?
		m0_spiel_sns_rebalance_continue(&spiel, &pool_fid) :
		m0_spiel_dix_rebalance_continue(&spiel, &pool_fid);
	M0_UT_ASSERT(rc == 0);

	wait_for_repair_rebalance(type, CM_OP_REBALANCE, &status, &pool_fid, &svc_fid);
	spiel_ci_ut_ha_state_set(&io_disk, M0_NC_ONLINE);
	m0_free(status);

done:
	m0_fi_disable("ready", "no_wait");
	m0_fi_disable("m0_ha_local_state_set", "no_ha");
	spiel_ci_ut_fini();
}

static void test_spiel_sns_rebalance(void)
{
	test_spiel_pool_rebalance(M0_REPREB_TYPE_SNS);
}

static void test_spiel_dix_rebalance(void)
{
	test_spiel_pool_rebalance(M0_REPREB_TYPE_DIX);
}

static int spiel_ci_tests_init()
{
	return m0_conf_ut_ast_thread_init();
}

static int spiel_ci_tests_fini()
{
	return m0_conf_ut_ast_thread_fini() ?: system("rm -rf ut_spiel.db/");
}

struct m0_ut_suite spiel_ci_ut = {
	.ts_name  = "spiel-ci-ut",
	.ts_init  = spiel_ci_tests_init,
	.ts_fini  = spiel_ci_tests_fini,
	.ts_tests = {
		{ "service-cmds", test_spiel_service_cmds },
		{ "process-cmds", test_spiel_process_cmds },
		{ "service-order", test_spiel_service_order },
		{ "process-services-list", test_spiel_process_services_list },
		{ "device-cmds", test_spiel_device_cmds },
		{ "stats", test_spiel_fs_stats },
		{ "pool-sns-repair", test_spiel_sns_repair },
		{ "pool-sns-rebalance", test_spiel_sns_rebalance },
		{ "pool-dix-repair", test_spiel_dix_repair },
		{ "pool-dix-rebalance", test_spiel_dix_rebalance },
		{ NULL, NULL }
	}
};

#undef M0_TRACE_SUBSYSTEM
