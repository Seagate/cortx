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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 * Original creation date: 07-Dec-2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0D
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/finject.h"
#include "lib/string.h"           /* m0_strdup */
#include "mero/setup.h"           /* cs_args */
#include "mero/setup_internal.h"  /* cs_ad_stob_create */
#include "rpc/rpclib.h"           /* m0_rpc_client_ctx */
#include "conf/obj.h"
#include "conf/obj_ops.h"         /* M0_CONF_DIRNEXT */
#include "conf/confc.h"           /* m0_confc */
#include "conf/schema.h"          /* m0_conf_service_type */
#include "conf/dir.h"             /* m0_conf_dir_len */
#include "conf/diter.h"           /* m0_conf_diter_init */
#include "conf/helpers.h"         /* m0_confc_root_open */
#include "reqh/reqh_service.h"    /* m0_reqh_service_ctx */
#include "stob/linux.h"           /* m0_stob_linux_reopen */
#include "ioservice/storage_dev.h" /* m0_storage_dev_attach */
#include "ioservice/fid_convert.h" /* m0_fid_conf_sdev_device_id */

/* ----------------------------------------------------------------
 * Mero options
 * ---------------------------------------------------------------- */

/* Note: `s' is believed to be heap-allocated. */
static void option_add(struct cs_args *args, char *s)
{
	char **argv;

	M0_PRE(0 <= args->ca_argc && args->ca_argc <= args->ca_argc_max);
	if (args->ca_argc == args->ca_argc_max) {
		args->ca_argc_max = args->ca_argc_max == 0 ? 64 :
				    args->ca_argc_max * 2;
		argv = m0_alloc(sizeof(args->ca_argv[0]) * args->ca_argc_max);
		if (args->ca_argv != NULL) {
			memcpy(argv, args->ca_argv,
			       sizeof(args->ca_argv[0]) * args->ca_argc);
			m0_free(args->ca_argv);
		}
		args->ca_argv = argv;
	}
	args->ca_argv[args->ca_argc++] = s;
	M0_LOG(M0_DEBUG, "%02d %s", args->ca_argc, s);
}

static char *
strxdup(const char *addr)
{
	static const char  xpt[] = "lnet:";
	char		  *s;

	s = m0_alloc(strlen(addr) + sizeof(xpt));
	if (s != NULL)
		sprintf(s, "%s%s", xpt, addr);

	return s;
}

static void
service_options_add(struct cs_args *args, const struct m0_conf_service *svc)
{
	static const char *opts[] = {
		[M0_CST_MDS]     = "-G",
		[M0_CST_IOS]     = "-i",
		[M0_CST_CONFD]   = "",
		[M0_CST_RMS]     = "",
		[M0_CST_STATS]   = "-R",
		[M0_CST_HA]      = "",
		[M0_CST_SSS]     = "",
		[M0_CST_SNS_REP] = "",
		[M0_CST_SNS_REB] = "",
		[M0_CST_ADDB2]   = "",
		[M0_CST_CAS]     = "",
		[M0_CST_DIX_REP] = "",
		[M0_CST_DIX_REB] = "",
		[M0_CST_DS1]     = "",
		[M0_CST_DS2]     = "",
		[M0_CST_FIS]     = "",
		[M0_CST_FDMI]    = "",
		[M0_CST_BE]      = "",
		[M0_CST_M0T1FS]  = "",
		[M0_CST_CLOVIS]  = "",
		[M0_CST_ISCS]    = "",
	};
	int         i;
	const char *opt;

	if (svc->cs_endpoints == NULL)
		return;

	for (i = 0; svc->cs_endpoints[i] != NULL; ++i) {
		if (!IS_IN_ARRAY(svc->cs_type, opts)) {
			M0_LOG(M0_ERROR, "invalid service type %d, ignoring",
			       svc->cs_type);
			break;
		}
		opt = opts[svc->cs_type];
		if (opt == NULL)
			continue;
		option_add(args, m0_strdup(opt));
		option_add(args, strxdup(svc->cs_endpoints[i]));
	}
}

M0_UNUSED static void
node_options_add(struct cs_args *args, const struct m0_conf_node *node)
{
/**
 * @todo Node parameters cn_memsize and cn_flags options are not used currently.
 * Options '-m' and '-q' options are used for maximum RPC message size and
 * minimum length of TM receive queue.
 * If required, change the option names accordingly.
 */
/*
	char buf[64] = {0};

	option_add(args, m0_strdup("-m"));
	(void)snprintf(buf, ARRAY_SIZE(buf) - 1, "%u", node->cn_memsize);
	option_add(args, m0_strdup(buf));

	option_add(args, m0_strdup("-q"));
	(void)snprintf(buf, ARRAY_SIZE(buf) - 1, "%lu", node->cn_flags);
	option_add(args, m0_strdup(buf));
*/
}

static bool service_and_node(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE ||
	       m0_conf_obj_type(obj) == &M0_CONF_NODE_TYPE;
}

M0_INTERNAL int
cs_conf_to_args(struct cs_args *dest, struct m0_conf_root *root)
{
	struct m0_confc      *confc;
	struct m0_conf_diter  it;
	int                   rc;

	M0_ENTRY();

	confc = m0_confc_from_obj(&root->rt_obj);
	M0_ASSERT(confc != NULL);

	rc = m0_conf_diter_init(&it, confc, &root->rt_obj,
				M0_CONF_ROOT_NODES_FID,
				M0_CONF_NODE_PROCESSES_FID,
				M0_CONF_PROCESS_SERVICES_FID);
	if (rc != 0)
		return M0_ERR(rc);

	option_add(dest, m0_strdup("lt-m0d")); /* XXX Does the value matter? */
	while ((rc = m0_conf_diter_next_sync(&it, service_and_node)) ==
		M0_CONF_DIRNEXT) {
		struct m0_conf_obj *obj = m0_conf_diter_result(&it);
		if (m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE) {
			struct m0_conf_service *svc =
				M0_CONF_CAST(obj, m0_conf_service);
			service_options_add(dest, svc);
		} else if(m0_conf_obj_type(obj) == &M0_CONF_NODE_TYPE) {
			struct m0_conf_node *node =
				M0_CONF_CAST(obj, m0_conf_node);
			node_options_add(dest, node);
		}
	}
	m0_conf_diter_fini(&it);
	return M0_RC(rc);
}

static bool is_local_service(const struct m0_conf_obj *obj)
{
	const struct m0_conf_service *svc;
	const struct m0_conf_process *proc;
	struct m0_mero *cctx = m0_cs_ctx_get(m0_conf_obj2reqh(obj));
	const struct m0_fid *proc_fid = &cctx->cc_reqh_ctx.rc_fid;
	const char *local_ep;

	if (m0_conf_obj_type(obj) != &M0_CONF_SERVICE_TYPE)
		return false;
	svc = M0_CONF_CAST(obj, m0_conf_service);
	proc = M0_CONF_CAST(m0_conf_obj_grandparent(&svc->cs_obj),
			    m0_conf_process);
	local_ep = m0_rpc_machine_ep(m0_mero_to_rmach(cctx));
	M0_LOG(M0_DEBUG, "local_ep=%s pc_endpoint=%s type=%d process="FID_F
	       " service=" FID_F, local_ep, proc->pc_endpoint, svc->cs_type,
	       FID_P(&proc->pc_obj.co_id), FID_P(&svc->cs_obj.co_id));
	/*
	 * It is expected for subordinate m0d service to have endpoint equal to
	 * respective process' endpoint.
	 */
	M0_ASSERT_INFO(cctx->cc_mkfs || /* ignore mkfs run */
		       !m0_fid_eq(&proc->pc_obj.co_id, proc_fid) ||
		       m0_streq(local_ep, svc->cs_endpoints[0]),
		       "process=" FID_F " process_fid=" FID_F
		       " local_ep=%s pc_endpoint=%s cs_endpoints[0]=%s",
		       FID_P(&proc->pc_obj.co_id), FID_P(proc_fid), local_ep,
		       proc->pc_endpoint, svc->cs_endpoints[0]);
	return m0_fid_eq(&proc->pc_obj.co_id, proc_fid) &&
		/*
		 * Comparing fids is not enough, since two different processes
		 * (e.g., m0mkfs and m0d) may have the same fid but different
		 * endpoints.
		 *
		 * Start CAS in mkfs mode to create meta indices for DIX. See
		 * MERO-2793.
		 */
		(m0_streq(proc->pc_endpoint, local_ep) ||
		 (cctx->cc_mkfs && svc->cs_type == M0_CST_CAS));
}

static bool is_ios(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE &&
		M0_CONF_CAST(obj, m0_conf_service)->cs_type == M0_CST_IOS;
}

static bool is_local_ios(const struct m0_conf_obj *obj)
{
	return is_ios(obj) && is_local_service(obj);
}

static bool is_device(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SDEV_TYPE;
}

static int cs_conf_storage_attach_by_srv(struct cs_stobs        *cs_stob,
					 struct m0_storage_devs *devs,
					 struct m0_fid          *svc_fid,
					 struct m0_confc        *confc,
					 bool                    force)
{
	struct m0_storage_dev *dev;
	struct m0_conf_obj    *svc_obj;
	struct m0_conf_sdev   *sdev;
	struct m0_stob        *stob;
	int                    rc;

	M0_ENTRY();

	if (svc_fid == NULL)
		return 0;

	rc = m0_confc_open_by_fid_sync(confc, svc_fid, &svc_obj);
	if (rc == 0) {
		struct m0_conf_diter    it;
		struct m0_conf_service *svc = M0_CONF_CAST(svc_obj,
							   m0_conf_service);
		uint32_t                dev_nr;
		struct m0_ha_note      *note;
		uint32_t                fail_devs = 0;

		/*
		 * Total number of devices under service is used to allocate
		 * note vector and notifications are sent only for devices
		 * which are failed with -ENOENT during attach.
		 */
		dev_nr = m0_conf_dir_len(svc->cs_sdevs);
		M0_ASSERT(dev_nr != 0);
		M0_ALLOC_ARR(note, dev_nr);
		if (note == NULL) {
			m0_confc_close(svc_obj);
			return M0_ERR(-ENOMEM);
		}
		rc = m0_conf_diter_init(&it, confc, svc_obj,
					M0_CONF_SERVICE_SDEVS_FID);
		if (rc != 0) {
			m0_free(note);
			m0_confc_close(svc_obj);
			return M0_ERR(rc);
		}

		while ((rc = m0_conf_diter_next_sync(&it, is_device)) ==
			M0_CONF_DIRNEXT) {
			sdev = M0_CONF_CAST(m0_conf_diter_result(&it),
					    m0_conf_sdev);
			M0_LOG(M0_DEBUG,
			       "sdev " FID_F " device index: %d "
			       "sdev.sd_filename: %s, "
			       "sdev.sd_size: %" PRIu64,
			       FID_P(&sdev->sd_obj.co_id), sdev->sd_dev_idx,
			       sdev->sd_filename, sdev->sd_size);

			M0_ASSERT(sdev->sd_dev_idx <= M0_FID_DEVICE_ID_MAX);
			if (sdev->sd_obj.co_ha_state == M0_NC_FAILED)
				continue;
			rc = m0_storage_dev_new_by_conf(devs, sdev, force, &dev);
			if (rc == -ENOENT) {
				M0_LOG(M0_DEBUG, "co_id="FID_F" path=%s rc=%d",
				       FID_P(&sdev->sd_obj.co_id),
				       sdev->sd_filename, rc);
				note[fail_devs].no_id = sdev->sd_obj.co_id;
				note[fail_devs].no_state = M0_NC_FAILED;
				M0_CNT_INC(fail_devs);
				continue;
			}
			if (rc != 0) {
				M0_LOG(M0_ERROR, "co_id="FID_F" path=%s rc=%d",
				       FID_P(&sdev->sd_obj.co_id),
				       sdev->sd_filename, rc);
				break;
			}
			m0_storage_dev_attach(dev, devs);
			stob = m0_storage_devs_find_by_cid(devs,
					    sdev->sd_dev_idx)->isd_stob;
			if (stob != NULL)
				m0_stob_linux_conf_sdev_associate(stob,
							   &sdev->sd_obj.co_id);

		}
		m0_conf_diter_fini(&it);
		if (fail_devs > 0) {
			struct m0_ha_nvec nvec;

			nvec.nv_nr = fail_devs;
			nvec.nv_note = note;
			m0_ha_local_state_set(&nvec);
		}
		m0_free(note);
	}
	m0_confc_close(svc_obj);

	return M0_RC(rc);
}

/* XXX copy-paste from mero/setup.c:pver_is_actual() */
static bool cs_conf_storage_pver_is_actual(const struct m0_conf_obj *obj)
{
	/**
	 * @todo XXX filter only actual pool versions till formulaic
	 *           pool version creation in place.
	 */
	return m0_conf_obj_type(obj) == &M0_CONF_PVER_TYPE &&
		M0_CONF_CAST(obj, m0_conf_pver)->pv_kind == M0_CONF_PVER_ACTUAL;
}

static int cs_conf_storage_is_n1_k0_s0(struct m0_confc *confc)
{
	struct m0_conf_root  *root = NULL;
	struct m0_conf_diter  it;
	struct m0_conf_pver  *pver_obj;
	int                   rc;
	bool                  result = false;

	M0_ENTRY();
	rc = m0_confc_root_open(confc, &root);
	M0_LOG(M0_DEBUG, "m0_confc_root_open: rc=%d", rc);
	if (rc == 0) {
		confc = m0_confc_from_obj(&root->rt_obj);
		rc = m0_conf_diter_init(&it, confc, &root->rt_obj,
					M0_CONF_ROOT_POOLS_FID,
					M0_CONF_POOL_PVERS_FID);
		M0_LOG(M0_DEBUG, "m0_conf_diter_init rc %d", rc);
	}
	while (rc == 0 &&
	       m0_conf_diter_next_sync(&it, cs_conf_storage_pver_is_actual) ==
	       M0_CONF_DIRNEXT) {
		pver_obj = M0_CONF_CAST(m0_conf_diter_result(&it),
					m0_conf_pver);
		if (pver_obj->pv_u.subtree.pvs_attr.pa_N == 1 &&
		    pver_obj->pv_u.subtree.pvs_attr.pa_K == 0) {
			result = true;
			break;
		}
	}
	if (rc == 0)
		m0_conf_diter_fini(&it);
	if (root != NULL)
		m0_confc_close(&root->rt_obj);
	return M0_RC(!!result);
}

M0_INTERNAL int cs_conf_storage_init(struct cs_stobs        *stob,
				     struct m0_storage_devs *devs,
				     bool                    force)
{
	int                     rc;
	struct m0_mero         *cctx;
	struct m0_reqh_context *rctx;
	struct m0_confc        *confc;
	struct m0_fid           tmp_fid;
	struct m0_fid          *svc_fid  = NULL;
	struct m0_conf_obj     *proc;

	M0_ENTRY();

	rctx = container_of(stob, struct m0_reqh_context, rc_stob);
	cctx = container_of(rctx, struct m0_mero, cc_reqh_ctx);
	confc = m0_mero2confc(cctx);

	if (cs_conf_storage_is_n1_k0_s0(confc))
		m0_storage_devs_locks_disable(devs);
	if (rctx->rc_services[M0_CST_DS1] != NULL) { /* setup for tests */
		svc_fid = &rctx->rc_service_fids[M0_CST_DS1];
	} else {
		struct m0_conf_diter  it;
		struct m0_fid        *proc_fid = &rctx->rc_fid;

		M0_ASSERT(m0_fid_is_set(proc_fid));

		M0_LOG(M0_DEBUG, FID_F, FID_P(proc_fid));
		rc = m0_confc_open_by_fid_sync(confc, proc_fid, &proc);
		if (rc != 0)
			return M0_ERR(rc);

		rc = m0_conf_diter_init(&it, confc, proc,
					M0_CONF_PROCESS_SERVICES_FID);
		if (rc != 0)
			return M0_ERR(rc);

		while ((rc = m0_conf_diter_next_sync(&it, is_ios)) ==
		       M0_CONF_DIRNEXT) {
			struct m0_conf_obj *obj = m0_conf_diter_result(&it);
			/*
			 * Copy of fid is needed because the conf cache can
			 * be invalidated after m0_confc_close() invocation
			 * rendering any pointer as invalid too.
			 */
			tmp_fid = obj->co_id;
			svc_fid = &tmp_fid;
			M0_LOG(M0_DEBUG, "obj->co_id: "FID_F, FID_P(svc_fid));
		}
		m0_conf_diter_fini(&it);
		m0_confc_close(proc);
	}
	if (svc_fid != NULL)
		M0_LOG(M0_DEBUG, "svc_fid: "FID_F, FID_P(svc_fid));

	/*
	 * XXX A kludge.
	 * See m0_storage_dev_attach() comment in cs_storage_devs_init().
	 */
	m0_storage_devs_lock(devs);
	rc = cs_conf_storage_attach_by_srv(stob, devs, svc_fid, confc, force);
	m0_storage_devs_unlock(devs);
	return M0_RC(rc);
}

M0_INTERNAL int cs_conf_services_init(struct m0_mero *cctx)
{
	int                        rc;
	struct m0_conf_diter       it;
	struct m0_conf_root       *root;
	struct m0_reqh_context    *rctx;
	struct m0_confc           *confc;

	M0_ENTRY();

	rctx = &cctx->cc_reqh_ctx;
	rctx->rc_nr_services = 0;
	confc = m0_mero2confc(cctx);
	rc = m0_confc_root_open(confc, &root);
	if (rc != 0)
		return M0_ERR_INFO(rc, "conf root open fail");
	rc = M0_FI_ENABLED("diter_fail") ? -ENOMEM :
		m0_conf_diter_init(&it, confc, &root->rt_obj,
				   M0_CONF_ROOT_NODES_FID,
				   M0_CONF_NODE_PROCESSES_FID,
				   M0_CONF_PROCESS_SERVICES_FID);
	if (rc != 0)
		goto fs_close;
	while ((rc = m0_conf_diter_next_sync(&it, is_local_service)) ==
		M0_CONF_DIRNEXT) {
		struct m0_conf_obj     *obj = m0_conf_diter_result(&it);
		struct m0_conf_service *svc =
			M0_CONF_CAST(obj, m0_conf_service);
		char                   *svc_type_name =
			m0_strdup(m0_conf_service_type2str(svc->cs_type));

		M0_LOG(M0_DEBUG, "service:%s fid:" FID_F, svc_type_name,
		       FID_P(&svc->cs_obj.co_id));
		M0_ASSERT(rctx->rc_nr_services < M0_CST_NR);
		if (svc_type_name == NULL) {
			int i;
			rc = M0_ERR(-ENOMEM);
			for (i = 0; i < rctx->rc_nr_services; ++i)
				m0_free(rctx->rc_services[i]);
			break;
		}
		M0_ASSERT_INFO(rctx->rc_services[svc->cs_type] == NULL,
			       "registering " FID_F " service type=%d when "
			       FID_F " for the type is already registered",
			       FID_P(&svc->cs_obj.co_id), svc->cs_type,
			       FID_P(&rctx->rc_service_fids[svc->cs_type]));
		rctx->rc_services[svc->cs_type] = svc_type_name;
		rctx->rc_service_fids[svc->cs_type] = svc->cs_obj.co_id;
		M0_CNT_INC(rctx->rc_nr_services);
	}
	m0_conf_diter_fini(&it);
fs_close:
	m0_confc_close(&root->rt_obj);
	return M0_RC(rc);
}

M0_INTERNAL int cs_conf_device_reopen(struct m0_poolmach *pm,
				      struct cs_stobs *stob,
				      uint32_t dev_id)
{
	struct m0_mero         *cctx;
	struct m0_reqh_context *rctx;
	struct m0_confc        *confc;
	int                     rc;
	struct m0_fid           fid;
	struct m0_conf_sdev    *sdev;
	struct m0_stob_id       stob_id;
	struct m0_conf_service *svc;

	M0_ENTRY();

	rctx = container_of(stob, struct m0_reqh_context, rc_stob);
	cctx = container_of(rctx, struct m0_mero, cc_reqh_ctx);
	confc = m0_mero2confc(cctx);
	fid = pm->pm_state->pst_devices_array[dev_id].pd_id;

	rc = m0_conf_sdev_get(confc, &fid, &sdev);
	if (rc != 0)
		return M0_ERR(rc);

	svc = M0_CONF_CAST(m0_conf_obj_grandparent(&sdev->sd_obj),
			   m0_conf_service);
	if (is_local_ios(&svc->cs_obj)) {
		M0_LOG(M0_DEBUG, "sdev size:%ld path:%s FID:"FID_F,
		       sdev->sd_size, sdev->sd_filename,
		       FID_P(&sdev->sd_obj.co_id));
		m0_stob_id_make(0, dev_id, &stob->s_sdom->sd_id, &stob_id);
		rc = m0_stob_linux_reopen(&stob_id, sdev->sd_filename);
	}
	m0_confc_close(&sdev->sd_obj);
	return M0_RC(rc);
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
