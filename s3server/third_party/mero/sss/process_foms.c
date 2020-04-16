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
 * Original author: Mikhail Antropov <mikhail.v.antropov@xyratex.com>
 * Original creation date: 23-Mar-2015
 */

/**
 * @page DLD-ss_process Process
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SSS
#include "lib/trace.h"

#include "lib/assert.h"
#include "lib/finject.h" /* M0_FI_ENABLED */
#include "lib/misc.h"
#include "lib/memory.h"
#include "be/domain.h"            /* m0_be_domain */
#include "be/alloc.h"             /* m0_be_alloc_stats */
#include "module/instance.h"      /* m0_get */
#include "conf/helpers.h"
#include "fop/fop.h"
#include "fop/fom_generic.h"
#include "reqh/reqh_service.h"
#include "spiel/spiel.h"
#include "rm/rm_service.h"        /* m0_rms_type */
#include "sss/process_fops.h"
#include "sss/process_foms.h"
#include "sss/ss_svc.h"
#ifndef __KERNEL__
 #include "module/instance.h"
 #include <unistd.h>
 #include <dlfcn.h>                 /* dlopen, dlsym */
 #include "mero/process_attr.h"
 #include "mero/setup.h"            /* m0_cs_storage_devs_get */
 #include "pool/pool_machine.h"     /* m0_pool_machine_state */
 #include "pool/pool.h"             /* m0_pooldev */
 #include "ioservice/storage_dev.h" /* m0_storage_dev_space */
#endif

static int ss_process_fom_create(struct m0_fop   *fop,
				 struct m0_fom  **out,
				 struct m0_reqh  *reqh);
static int ss_process_fom_tick(struct m0_fom *fom);
static int ss_process_fom_tick__init(struct m0_fom        *fom,
				     const struct m0_reqh *reqh);
static void ss_process_fom_fini(struct m0_fom *fom);
static void ss_process_fom_fini(struct m0_fom *fom);
static size_t ss_process_fom_home_locality(const struct m0_fom *fom);

enum ss_process_fom_phases {
	SS_PROCESS_FOM_INIT = M0_FOPH_NR + 1,
	SS_PROCESS_FOM_STOP,
	SS_PROCESS_FOM_RECONFIG_GET_DATA,
	SS_PROCESS_FOM_RECONFIG_DATA_WAIT,
	SS_PROCESS_FOM_RECONFIG,
	SS_PROCESS_FOM_HEALTH,
	SS_PROCESS_FOM_QUIESCE,
	SS_PROCESS_FOM_RUNNING_LIST,
	SS_PROCESS_FOM_LIB_LOAD
};

static struct m0_fom_ops ss_process_fom_ops = {
	.fo_tick          = ss_process_fom_tick,
	.fo_home_locality = ss_process_fom_home_locality,
	.fo_fini          = ss_process_fom_fini
};

const struct m0_fom_type_ops ss_process_fom_type_ops = {
	.fto_create = ss_process_fom_create
};

struct m0_sm_state_descr ss_process_fom_phases[] = {
	[SS_PROCESS_FOM_INIT]= {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "SS_FOM_INIT",
		.sd_allowed = M0_BITS(SS_PROCESS_FOM_STOP,
				      SS_PROCESS_FOM_RECONFIG_GET_DATA,
				      SS_PROCESS_FOM_HEALTH,
				      SS_PROCESS_FOM_QUIESCE,
				      SS_PROCESS_FOM_RUNNING_LIST,
				      SS_PROCESS_FOM_LIB_LOAD,
				      M0_FOPH_FAILURE),
	},
	[SS_PROCESS_FOM_STOP]= {
		.sd_name    = "SS_PROCESS_FOM_STOP",
		.sd_allowed = M0_BITS(M0_FOPH_SUCCESS, M0_FOPH_FAILURE),
	},
	[SS_PROCESS_FOM_RECONFIG_GET_DATA]= {
		.sd_name    = "SS_PROCESS_FOM_RECONFIG_GET_DATA",
		.sd_allowed = M0_BITS(SS_PROCESS_FOM_RECONFIG_DATA_WAIT,
				      M0_FOPH_FAILURE),
	},
	[SS_PROCESS_FOM_RECONFIG_DATA_WAIT] = {
		.sd_name    = "SS_PROCESS_FOM_RECONFIG_DATA_WAIT",
		.sd_allowed = M0_BITS(SS_PROCESS_FOM_RECONFIG),
	},
	[SS_PROCESS_FOM_RECONFIG]= {
		.sd_name    = "SS_PROCESS_FOM_RECONFIG",
		.sd_allowed = M0_BITS(M0_FOPH_SUCCESS, M0_FOPH_FAILURE),
	},
	[SS_PROCESS_FOM_HEALTH]= {
		.sd_name    = "SS_PROCESS_FOM_HEALTH",
		.sd_allowed = M0_BITS(M0_FOPH_SUCCESS, M0_FOPH_FAILURE),
	},
	[SS_PROCESS_FOM_QUIESCE]= {
		.sd_name    = "SS_PROCESS_FOM_QUIESCE",
		.sd_allowed = M0_BITS(M0_FOPH_SUCCESS, M0_FOPH_FAILURE),
	},
	[SS_PROCESS_FOM_RUNNING_LIST]= {
		.sd_name    = "SS_PROCESS_FOM_RUNNING_LIST",
		.sd_allowed = M0_BITS(M0_FOPH_SUCCESS, M0_FOPH_FAILURE),
	},
	[SS_PROCESS_FOM_LIB_LOAD]= {
		.sd_name    = "SS_PROCESS_FOM_LIB_LOAD",
		.sd_allowed = M0_BITS(M0_FOPH_SUCCESS, M0_FOPH_FAILURE),
	}
};

struct m0_sm_conf ss_process_fom_conf = {
	.scf_name      = "ss-process-fom-sm",
	.scf_nr_states = ARRAY_SIZE(ss_process_fom_phases),
	.scf_state     = ss_process_fom_phases
};

struct m0_sss_process_fom {
	struct m0_fom          spm_fom;
	struct m0_confc_ctx    spm_confc_ctx;
};

static int ss_process_fom_create(struct m0_fop   *fop,
				 struct m0_fom  **out,
				 struct m0_reqh  *reqh)
{
	int                        rc;
	struct m0_sss_process_fom *process_fom = NULL;
	struct m0_ss_process_req  *process_fop;
	struct m0_fop             *rfop = NULL;
	int                        cmd;

	M0_ENTRY();
	M0_PRE(fop != NULL);
	M0_PRE(out != NULL);
	M0_PRE(m0_ss_fop_is_process_req(fop));

	process_fop = m0_ss_fop_process_req(fop);
	cmd = process_fop->ssp_cmd;

	if (!M0_FI_ENABLED("fom_alloc_fail"))
		M0_ALLOC_PTR(process_fom);
	if (!M0_FI_ENABLED("fop_alloc_fail"))
		M0_ALLOC_PTR(rfop);
	if (process_fom == NULL || rfop == NULL)
		goto err;

	switch(cmd){
	case M0_PROCESS_STOP:
		m0_fop_init(rfop, &m0_fop_process_rep_fopt, NULL,
			    m0_ss_process_stop_fop_release);
		break;
	case M0_PROCESS_RUNNING_LIST:
		m0_fop_init(rfop, &m0_fop_process_svc_list_rep_fopt, NULL,
			    m0_fop_release);
		break;
	default:
		m0_fop_init(rfop, &m0_fop_process_rep_fopt, NULL,
			    m0_fop_release);
	}

	rc = M0_FI_ENABLED("fop_data_alloc_fail") ? M0_ERR(-ENOMEM) :
		m0_fop_data_alloc(rfop);
	if (rc != 0)
		goto err;

	rfop->f_item.ri_rmachine = m0_fop_rpc_machine(fop);

	m0_fom_init(&process_fom->spm_fom, &fop->f_type->ft_fom_type,
		    &ss_process_fom_ops, fop, rfop, reqh);

	*out = &process_fom->spm_fom;
	M0_LOG(M0_DEBUG, "fom %p", process_fom);
	return M0_RC(0);

err:
	m0_free(process_fom);
	m0_free(rfop);
	return M0_ERR(-ENOMEM);
}

static void ss_process_fom_fini(struct m0_fom *fom)
{
	struct m0_sss_process_fom *process_fom;

	M0_ENTRY();

	M0_LOG(M0_DEBUG, "fom %p", fom);
	m0_fom_fini(fom);
	process_fom = M0_AMB(process_fom, fom, spm_fom);
	m0_free(process_fom);

	M0_LEAVE();
}

static int ss_process_fom_tick__init(struct m0_fom        *fom,
				     const struct m0_reqh *reqh)
{
	static enum ss_fom_phases next_phase[] = {
		[M0_PROCESS_STOP]         = SS_PROCESS_FOM_STOP,
		[M0_PROCESS_RECONFIG]     = SS_PROCESS_FOM_RECONFIG_GET_DATA,
		[M0_PROCESS_HEALTH]       = SS_PROCESS_FOM_HEALTH,
		[M0_PROCESS_QUIESCE]      = SS_PROCESS_FOM_QUIESCE,
		[M0_PROCESS_RUNNING_LIST] = SS_PROCESS_FOM_RUNNING_LIST,
		[M0_PROCESS_LIB_LOAD]     = SS_PROCESS_FOM_LIB_LOAD,
	};
	int cmd;

	M0_ENTRY("fom %p, state %d", fom, m0_fom_phase(fom));
	M0_PRE(fom != NULL);

	cmd = m0_ss_fop_process_req(fom->fo_fop)->ssp_cmd;
	if (!IS_IN_ARRAY(cmd, next_phase))
		return M0_ERR(-ENOENT);
	m0_fom_phase_set(fom, next_phase[cmd]);
	return M0_RC(0);
}

static int ss_process_health(struct m0_reqh *reqh, int32_t *h)
{
	/* Do nothing special here for now.
	 * Maybe in future some checks will be added */

	M0_PRE(h != NULL);
	*h = M0_HEALTH_GOOD;
	return 0;
}

#ifdef __KERNEL__
static int ss_process_stats(struct m0_reqh *reqh M0_UNUSED,
			     struct m0_ss_process_rep *rep M0_UNUSED)
{
	return M0_ERR(-ENOSYS);
}
#else
extern struct m0_reqh_service_type m0_ios_type;

static struct m0_reqh_service *ss_ioservice_find(struct m0_reqh *reqh)
{
	struct m0_reqh_service_type *iot;

	M0_PRE(reqh != NULL);
	iot = m0_reqh_service_type_find(m0_ios_type.rst_name);
	if (iot != NULL)
		return m0_reqh_service_find(iot, reqh);
	return NULL;
}

M0_TL_DESCR_DECLARE(seg, M0_EXTERN);

static int ss_be_segs_stats_ingest(struct m0_be_domain      *dom,
				   struct m0_ss_process_rep *rep)
{
	struct m0_tl     *segs = &dom->bd_segs;
	struct m0_be_seg *bs;

	/* collect be segments stats */
	m0_tl_for(seg, segs, bs) {
		struct m0_be_allocator_stats stats = {0};

		m0_be_alloc_stats(&bs->bs_allocator, &stats);
		if (m0_addu64_will_overflow(rep->sspr_total_seg,
					    stats.bas_space_total))
			return M0_ERR(-EOVERFLOW);
		rep->sspr_total_seg += stats.bas_space_total;
		if (m0_addu64_will_overflow(rep->sspr_free_seg,
					    stats.bas_space_free))
			return M0_ERR(-EOVERFLOW);
		rep->sspr_free_seg  += stats.bas_space_free;
	} m0_tl_endfor;

	return M0_RC(0);
}

static int ss_ios_stats_ingest(struct m0_ss_process_rep *rep)
{
	struct m0_storage_devs   *sds = m0_cs_storage_devs_get();
	struct m0_storage_dev    *dev;
	struct m0_storage_space   sp;

	/* XXX Remove this block when storage_dev supports linuxstobs. */
	if (sds == NULL)
		return M0_RC(0);

	/* collect sdevs stats */
	m0_tl_for(storage_dev, &sds->sds_devices, dev) {
		M0_SET0(&sp);
		/* Collect device stats for devices attached under dummy
		 * services in UT
		 */
		if (M0_FI_ENABLED("take_dsx_in_effect") &&
		    M0_IN(dev->isd_srv_type, (M0_CST_DS1, M0_CST_DS2)))
			goto consider_DS_in_ut;
		/* Skip device if it does not belong to ios. */
		if (dev->isd_srv_type != M0_CST_IOS)
			continue;
consider_DS_in_ut:
		m0_storage_dev_space(dev, &sp);
		/* any storage device must update total stats */
		if (m0_addu64_will_overflow(rep->sspr_total_disk,
					    sp.sds_total_size))
			return M0_ERR(-EOVERFLOW);
		rep->sspr_total_disk += sp.sds_total_size;
		/*
		 * skip the device that's not online.
		 * TODO: EOS-5495 - Device status is not recieved from hare,
		 * isd_ha_state is default value(M0_NC_UNKNOWN) because not
		 * updated by hare, ignore the device check if blocksize is
		 * non zero.
 		 * 'sp.sds_block_size == 0' will be removed once hare updates
 		 * the device status.
 		 */
		if (sp.sds_block_size == 0 && dev->isd_ha_state != M0_NC_ONLINE)
			continue;
		m0_bcount_t free_space;
		if (~((m0_bcount_t)0) / sp.sds_block_size <
				sp.sds_free_blocks)
			return M0_ERR(-EOVERFLOW);
		free_space = sp.sds_free_blocks * sp.sds_block_size;
		if (m0_addu64_will_overflow(rep->sspr_free_disk, free_space))
			return M0_ERR(-EOVERFLOW);
		rep->sspr_free_disk += free_space;
		m0_bcount_t avail_space = sp.sds_avail_blocks *
						sp.sds_block_size;
		if (m0_addu64_will_overflow(rep->sspr_avail_disk, avail_space))
			return M0_ERR(-EOVERFLOW);
		rep->sspr_avail_disk += avail_space;
	} m0_tl_endfor;

	return M0_RC(0);
}

static int ss_process_stats(struct m0_reqh           *reqh,
			    struct m0_ss_process_rep *rep)
{
	int rc;

	M0_ENTRY();
	rc = ss_be_segs_stats_ingest(reqh->rh_beseg->bs_domain, rep);
	/* see if ioservice is up and running */
	if (rc == 0 && ss_ioservice_find(reqh) != NULL)
		rc = ss_ios_stats_ingest(rep);
	return M0_RC(rc);
}
#endif

extern struct m0_reqh_service_type m0_rpc_service_type;

/**
 * Initiates process of stopping services found in M0_RST_STARTED state.
 *
 * Avoids stopping a number of vital services to sustain the ability to:
 *
 * - SSS - handle further incoming commands if any
 * - RPC - communicate with cluster nodes, including local communication
 * - RMS - handle resource requests, owner balancing, etc., that the controlled
 *         services depend on
 */
static int ss_process_quiesce(struct m0_reqh *reqh)
{
	struct m0_reqh_service *svc;

	M0_PRE(reqh != NULL);
	M0_PRE(M0_IN(m0_reqh_state_get(reqh), (M0_REQH_ST_NORMAL,
					       M0_REQH_ST_DRAIN,
					       M0_REQH_ST_SVCS_STOP)));

	m0_tl_for(m0_reqh_svc, &reqh->rh_services, svc) {
		M0_LOG(M0_DEBUG, "type:%d level:%d [%s] name:%s",
		       svc->rs_type->rst_typecode, svc->rs_type->rst_level,
		       svc->rs_type->rst_keep_alive ? "+" : "-",
		       svc->rs_type->rst_name);
		/* Do not stop vital services like SSS, RPC, HA link, etc. */
		if (svc->rs_type->rst_keep_alive)
			continue;
		if (m0_reqh_service_state_get(svc) == M0_RST_STARTED)
			m0_reqh_service_prepare_to_stop(svc);
	} m0_tl_endfor;

	return M0_RC(0);
}

#ifndef __KERNEL__
static int ss_fop_process_svc_to_buf(struct m0_reqh_service *svc,
				     struct m0_buf          *buf)
{
	struct m0_ss_process_svc_item *reply_svc;

	buf->b_nob = sizeof(struct m0_fid) + strlen(svc->rs_type->rst_name) + 1;
	buf->b_addr = m0_alloc(buf->b_nob);
	if (buf->b_addr == NULL)
		return M0_ERR(-ENOMEM);

	reply_svc = (struct m0_ss_process_svc_item *)buf->b_addr;
	reply_svc->ssps_fid = svc->rs_service_fid;
	strcpy(reply_svc->ssps_name, svc->rs_type->rst_name);

	return M0_RC(0);
}

static int
ss_fop_process_svc_list_fill(struct m0_ss_process_svc_list_rep *fop,
			     struct m0_reqh                    *reqh)
{
	struct m0_reqh_service *svc;
	int                     i;
	int                     rc;

	M0_PRE(reqh != NULL);
	M0_PRE(M0_IN(m0_reqh_state_get(reqh), (M0_REQH_ST_NORMAL,
					       M0_REQH_ST_DRAIN,
					       M0_REQH_ST_SVCS_STOP)));
	m0_rwlock_read_lock(&reqh->rh_rwlock);
	i = m0_tl_reduce(m0_reqh_svc, svc, &reqh->rh_services, 0,
		 + (m0_reqh_service_state_get(svc) == M0_RST_STARTED ? 1 : 0));

	fop->sspr_services.ab_count = i;
	M0_ALLOC_ARR(fop->sspr_services.ab_elems, i);
	if (fop->sspr_services.ab_elems == NULL)
		return M0_RC(-ENOMEM);

	i = 0;
	rc = 0;
	m0_tl_for(m0_reqh_svc, &reqh->rh_services, svc) {
		if (m0_reqh_service_state_get(svc) == M0_RST_STARTED) {
			rc = ss_fop_process_svc_to_buf(svc,
					&fop->sspr_services.ab_elems[i]);
			if (rc != 0)
				break;
			i++;
		}
	} m0_tl_endfor;
	m0_rwlock_read_unlock(&reqh->rh_rwlock);
	return M0_RC(rc);
}
#else
static int
ss_fop_process_svc_list_fill(struct m0_ss_process_svc_list_rep *fop,
			     struct m0_reqh                    *reqh)
{
	return M0_RC(0);
}
#endif /* __KERNEL__ */

static void ss_process_confc_ctx_arm(struct m0_sss_process_fom *pfom)
{
	struct m0_chan *chan;

	chan = &pfom->spm_confc_ctx.fc_mach.sm_chan;
	m0_chan_lock(chan);
	m0_fom_wait_on(&pfom->spm_fom, chan, &pfom->spm_fom.fo_cb);
	m0_chan_unlock(chan);
}

static bool ss_process_confc_ctx_completed(struct m0_fom *fom)
{
	struct m0_sss_process_fom *pfom;

	pfom = M0_AMB(pfom, fom, spm_fom);
	if (!m0_confc_ctx_is_completed_lock(&pfom->spm_confc_ctx)) {
		ss_process_confc_ctx_arm(pfom);
		return false;
	}
	return true;
}

static int ss_process_reconfig_data_get(struct m0_fom *fom)
{
	struct m0_sss_process_fom *pfom = M0_AMB(pfom, fom, spm_fom);
	struct m0_reqh            *reqh = m0_fom_reqh(fom);
	struct m0_ss_process_req  *req;
	int                        rc;

	M0_ENTRY();

	rc = m0_confc_ctx_init(&pfom->spm_confc_ctx, m0_reqh2confc(reqh));
	if (rc == 0) {
		ss_process_confc_ctx_arm(pfom);
		req = m0_ss_fop_process_req(fom->fo_fop);
		m0_confc_open_by_fid(&pfom->spm_confc_ctx, &req->ssp_id);
	}
	return M0_RC(rc);
}

static int ss_process_reconfig(struct m0_fom *fom)
{
#ifndef __KERNEL__
	int                        rc;
	struct m0_sss_process_fom *pfom;
	struct m0_conf_process    *process;
	int                        pid = getpid();
	struct m0_proc_attr       *proc_attr;
	struct m0_confc_ctx       *confc_ctx;

	M0_ENTRY();

	proc_attr = &m0_get()->i_proc_attr;
	pfom      = M0_AMB(pfom, fom, spm_fom);
	confc_ctx = &pfom->spm_confc_ctx;
	rc = m0_confc_ctx_error_lock(confc_ctx);
	if (rc == 0)
		process = M0_CONF_CAST(m0_confc_ctx_result(confc_ctx),
				       m0_conf_process);
	m0_confc_ctx_fini(&pfom->spm_confc_ctx);

	if (rc != 0)
		return M0_ERR(rc);

	if (process != NULL) {
		*proc_attr = (struct m0_proc_attr){
			.pca_memlimit_as = process->pc_memlimit_as,
			.pca_memlimit_rss = process->pc_memlimit_rss,
			.pca_memlimit_stack = process->pc_memlimit_stack,
			.pca_memlimit_memlock = process->pc_memlimit_memlock,
		};
		rc = m0_bitmap_init(&proc_attr->pca_core_mask,
				    process->pc_cores.b_nr);
		if (rc == 0)
			m0_bitmap_copy(&proc_attr->pca_core_mask,
				       &process->pc_cores);
	} else {
		M0_SET0(proc_attr);
	}

	m0_confc_close(&process->pc_obj);

	if (!M0_FI_ENABLED("unit_test"))
		kill(pid, SIGUSR1);

	return M0_RC(rc);
#else
	return M0_ERR(-ENOENT);
#endif
}

static int ss_process_lib_load(struct m0_fom *fom)
{
#if !defined(__KERNEL__)
	struct m0_sss_process_fom *pfom    = M0_AMB(pfom, fom, spm_fom);
	struct m0_ss_process_req  *req     = m0_ss_fop_process_req(fom->fo_fop);
	char                      *libname = req->ssp_param.b_addr;
	char                      *last    = &libname[req->ssp_param.b_nob - 1];
	void                      *handle;

	M0_ENTRY();
	/*
	 * Space-terminated strings are allowed, because m0console is called
	 * from a shell and a typical shell cannot handle NUL character.
	 */
	if (*last == ' ')
		*last = 0;
	/* Check that the libname is NUL-terminated. */
	if (*last != 0)
		return M0_ERR(-EPROTO);
	handle = dlopen(libname, RTLD_NOW | RTLD_GLOBAL);
	if (handle != NULL) {
		void (*initcall)(void) = dlsym(handle, "mero_lib_init");
		if (initcall != NULL)
			(*initcall)();
		return M0_RC(0);
	} else
		return M0_ERR_INFO(-EINVAL, "libname: %s", libname);
#else
	/** @todo implement kernel module loading. */
	return M0_ERR(-ENOSYS);
#endif
}

static int ss_process_fom_tail(struct m0_fom *fom, int rc)
{
	/*
	 * fop data can be m0_ss_process_rep or m0_ss_process_svc_list_rep, but
	 * we only care that the first field is the error code, which is always
	 * the case for replies, see the assertion in m0_fop_type_init().
	 */
	struct m0_ss_process_rep *rep = m0_fop_data(fom->fo_rep_fop);

	rep->sspr_rc = rc;
	m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
	return M0_FSO_AGAIN;
}

static int ss_process_fom_tick(struct m0_fom *fom)
{
	struct m0_reqh                    *reqh;
	struct m0_ss_process_rep          *rep;
	struct m0_ss_process_svc_list_rep *rep_list;
	int                                rc;

	M0_ENTRY("fom %p, state %d", fom, m0_fom_phase(fom));
	M0_PRE(fom != NULL);

	if (m0_fom_phase(fom) < M0_FOPH_NR)
		return m0_fom_tick_generic(fom);

	reqh = m0_fom_reqh(fom);
	rep = m0_fop_data(fom->fo_rep_fop);
	switch (m0_fom_phase(fom)) {
	case SS_PROCESS_FOM_INIT:
		rc = ss_process_fom_tick__init(fom, reqh);
		if (rc != 0)
			return ss_process_fom_tail(fom, rc);
		else
			return M0_FSO_AGAIN;
	case SS_PROCESS_FOM_STOP:
		/**
		 * Do nothing here.
		 * Signal SIGQUIT is sent from m0_ss_process_stop_fop_release
		 * when FOP is freed.
		 */
		return ss_process_fom_tail(fom,
#ifndef __KERNEL__
					   0
#else
					   M0_ERR(-ENOSYS)
#endif
					   );
	case SS_PROCESS_FOM_HEALTH:
		return ss_process_fom_tail(fom,
				   ss_process_health(reqh, &rep->sspr_health) ?:
				   ss_process_stats(reqh, rep));

	case SS_PROCESS_FOM_QUIESCE:
		return ss_process_fom_tail(fom, ss_process_quiesce(reqh));
	case SS_PROCESS_FOM_RECONFIG_GET_DATA:
		rep->sspr_rc = ss_process_reconfig_data_get(fom);
		m0_fom_phase_moveif(fom, rep->sspr_rc,
				    SS_PROCESS_FOM_RECONFIG_DATA_WAIT,
				    M0_FOPH_FAILURE);
		return rep->sspr_rc == 0 ? M0_FSO_WAIT : M0_FSO_AGAIN;
	case SS_PROCESS_FOM_RECONFIG_DATA_WAIT:
		if (ss_process_confc_ctx_completed(fom)) {
			m0_fom_phase_set(fom, SS_PROCESS_FOM_RECONFIG);
			return M0_FSO_AGAIN;
		}
		return M0_FSO_WAIT;
	case SS_PROCESS_FOM_RECONFIG:
		return ss_process_fom_tail(fom, ss_process_reconfig(fom));
	case SS_PROCESS_FOM_RUNNING_LIST:
		rep_list = m0_ss_fop_process_svc_list_rep(fom->fo_rep_fop);
		return ss_process_fom_tail(fom,
				   ss_fop_process_svc_list_fill(rep_list,
								reqh));
	case SS_PROCESS_FOM_LIB_LOAD:
		return ss_process_fom_tail(fom, ss_process_lib_load(fom));
	default:
		M0_IMPOSSIBLE("Invalid phase");
	}
	return M0_FSO_AGAIN;
}

static size_t ss_process_fom_home_locality(const struct m0_fom *fom)
{
	return 1;
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
