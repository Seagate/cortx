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
 * Original creation date: 21-Apr-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SSS
#include "lib/trace.h"

#include "lib/assert.h"
#include "lib/misc.h"
#include "lib/memory.h"
#include "lib/tlist.h"
#include "lib/finject.h"          /* M0_FI_ENABLED */
#include "conf/diter.h"
#include "conf/obj.h"
#include "conf/obj_ops.h"         /* M0_CONF_DIRNEXT */
#include "conf/helpers.h"         /* m0_conf_sdev_get */
#include "fop/fop.h"
#include "fop/fom_generic.h"
#include "pool/pool.h"
#include "pool/pool_machine.h"
#include "reqh/reqh.h"
#include "sss/device_fops.h"
#include "sss/device_foms.h"
#ifndef __KERNEL__
 #include "mero/setup.h"          /* m0_cs_storage_devs_get */
 #include "ioservice/storage_dev.h"
#endif

extern struct m0_reqh_service_type m0_ios_type;

static int sss_device_fom_create(struct m0_fop   *fop,
				 struct m0_fom  **out,
				 struct m0_reqh  *reqh);
static int sss_device_fom_tick(struct m0_fom *fom);
static void sss_device_fom_fini(struct m0_fom *fom);
static size_t sss_device_fom_home_locality(const struct m0_fom *fom);

/**
   @addtogroup DLDGRP_sss_device
   @{
 */

/**
 * Stages of device fom.
 *
 * All custom phases of device FOM are separated into two stages.
 *
 * One part of custom phases is executed outside of FOM local transaction
 * context (before M0_FOPH_TXN_INIT phase), the other part is executed as usual
 * for FOMs in context of local transaction.
 *
 * Separation is done to prevent dead-lock between two exclusively opened
 * BE transactions: one is in FOM local transaction context and the other one
 * created during adding stob domain in sss_device_stob_attach.
 *
 * @see sss_device_fom_phases_desc.
 */
enum sss_device_fom_stage {
	/**
	 * Stage incorporates AD stob domain and stob creation.
	 * Phases of this stage are executed before M0_FOPH_TXN_INIT phase.
	 */
	SSS_DEVICE_FOM_STAGE_STOB,
	/**
	 * Stage includes phases which works with pool machine
	 * and are executed as usual FOM-specific phases.
	 */
	SSS_DEVICE_FOM_STAGE_POOL_MACHINE
};

/**
 * Device commands fom
 */
struct m0_sss_dfom {
	/** Embedded fom. */
	struct m0_fom              ssm_fom;
	/** Current stage. */
	enum sss_device_fom_stage  ssm_stage;
	/** Confc context used to retrieve disk conf object */
	struct m0_confc_ctx        ssm_confc_ctx;
	/** Clink to wait on confc ctx completion */
	struct m0_clink            ssm_clink;
	/** Is used to iterate over m0_conf_sdev objects */
	struct m0_conf_diter       ssm_it;
	/**
	 * Storage device fid. Obtained from disk conf object.
	 * disk conf object -> ck_sdev -> sd_obj.co_id.
	 */
	struct m0_fid              ssm_fid;
	/** True iff the storage device belongs current IO service */
	bool                       ssm_native_device;
	/** device HA state retrieval context */
	struct {
		struct m0_mutex   chan_lock;
		struct m0_chan    chan;
		struct m0_clink   clink;
		struct m0_ha_nvec nvec;
		struct m0_ha_note note;
	}                          ssm_ha;
};

static struct m0_fom_ops sss_device_fom_ops = {
	.fo_tick          = sss_device_fom_tick,
	.fo_home_locality = sss_device_fom_home_locality,
	.fo_fini          = sss_device_fom_fini
};

const struct m0_fom_type_ops sss_device_fom_type_ops = {
	.fto_create = sss_device_fom_create
};

struct m0_sm_state_descr sss_device_fom_phases_desc[] = {
	[SSS_DFOM_SWITCH]= {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "SSS_DFOM_SWITCH",
		.sd_allowed = M0_BITS(SSS_DFOM_SDEV_OPENING,
				      SSS_DFOM_ATTACH_POOL_MACHINE,
				      SSS_DFOM_DETACH_STOB,
				      SSS_DFOM_DETACH_POOL_MACHINE,
				      M0_FOPH_TXN_INIT,
				      SSS_DFOM_FORMAT,
				      M0_FOPH_FAILURE),
	},
	[SSS_DFOM_DISK_OPENING]= {
		.sd_name    = "SSS_DFOM_DISK_OPENING",
		.sd_allowed = M0_BITS(SSS_DFOM_DISK_HA_STATE_GET,
				      M0_FOPH_FAILURE),
	},
	[SSS_DFOM_DISK_HA_STATE_GET]= {
		.sd_name    = "SSS_DFOM_DISK_HA_STATE_GET",
		.sd_allowed = M0_BITS(SSS_DFOM_DISK_OPENED,
				      M0_FOPH_FAILURE),
	},
	[SSS_DFOM_DISK_OPENED]= {
		.sd_name    = "SSS_DFOM_DISK_OPENED",
		.sd_allowed = M0_BITS(SSS_DFOM_FS_OPENED, M0_FOPH_FAILURE),
	},
	[SSS_DFOM_FS_OPENED]= {
		.sd_name    = "SSS_DFOM_FS_OPENED",
		.sd_allowed = M0_BITS(SSS_DFOM_SDEV_ITER, M0_FOPH_FAILURE),
	},
	[SSS_DFOM_SDEV_ITER]= {
		.sd_name    = "SSS_DFOM_SDEV_ITER",
		.sd_allowed = M0_BITS(SSS_DFOM_SWITCH, M0_FOPH_FAILURE),
	},
	[SSS_DFOM_SDEV_OPENING]= {
		.sd_name    = "SSS_DFOM_SDEV_OPENING",
		.sd_allowed = M0_BITS(SSS_DFOM_ATTACH_STOB, M0_FOPH_FAILURE),
	},
	[SSS_DFOM_ATTACH_STOB]= {
		.sd_name    = "SSS_DFOM_ATTACH_STOB",
		.sd_allowed = M0_BITS(M0_FOPH_TXN_INIT),
	},
	[SSS_DFOM_ATTACH_POOL_MACHINE]= {
		.sd_name    = "SSS_DFOM_ATTACH_POOL_MACHINE",
		.sd_allowed = M0_BITS(M0_FOPH_SUCCESS, M0_FOPH_FAILURE),
	},
	[SSS_DFOM_DETACH_STOB]= {
		.sd_name    = "SSS_DFOM_DETACH_STOB",
		.sd_allowed = M0_BITS(SSS_DFOM_DETACH_STOB_WAIT),
	},
	[SSS_DFOM_DETACH_STOB_WAIT]= {
		.sd_name    = "SSS_DFOM_DETACH_STOB_WAIT",
		.sd_allowed = M0_BITS(M0_FOPH_TXN_INIT),
	},
	[SSS_DFOM_DETACH_POOL_MACHINE]= {
		.sd_name    = "SSS_DFOM_DETACH_POOL_MACHINE",
		.sd_allowed = M0_BITS(M0_FOPH_SUCCESS, M0_FOPH_FAILURE),
	},
	[SSS_DFOM_FORMAT]= {
		.sd_name    = "SSS_DFOM_FORMAT",
		.sd_allowed = M0_BITS(M0_FOPH_SUCCESS, M0_FOPH_FAILURE),
	},
};

const struct m0_sm_conf sss_device_fom_conf = {
	.scf_name      = "sss-device-fom-sm",
	.scf_nr_states = ARRAY_SIZE(sss_device_fom_phases_desc),
	.scf_state     = sss_device_fom_phases_desc
};

static int sss_device_fom_create(struct m0_fop   *fop,
				 struct m0_fom  **out,
				 struct m0_reqh  *reqh)
{
	struct m0_sss_dfom *dfom = NULL;
	struct m0_fop      *rep_fop = NULL;

	M0_ENTRY();
	M0_PRE(fop != NULL);
	M0_PRE(out != NULL);
	M0_PRE(m0_sss_fop_is_dev_req(fop));
	M0_PRE(reqh != NULL);

	if (!M0_FI_ENABLED("fom_alloc_fail"))
		M0_ALLOC_PTR(dfom);
	if (!M0_FI_ENABLED("fop_alloc_fail"))
		rep_fop = m0_fop_reply_alloc(fop, &m0_sss_fop_device_rep_fopt);
	if (dfom == NULL || rep_fop == NULL)
		goto err;

	m0_fom_init(&dfom->ssm_fom, &fop->f_type->ft_fom_type,
		    &sss_device_fom_ops, fop, rep_fop, reqh);

	*out = &dfom->ssm_fom;
	M0_LOG(M0_DEBUG, "fom %p", dfom);
	return M0_RC(0);

err:
	m0_free(rep_fop);
	m0_free(dfom);
	return M0_ERR(-ENOMEM);
}

static void sss_device_fom_fini(struct m0_fom *fom)
{
	struct m0_sss_dfom *dfom;

	M0_ENTRY();
	M0_LOG(M0_DEBUG, "fom %p", fom);
	dfom = container_of(fom, struct m0_sss_dfom, ssm_fom);
	m0_fom_fini(fom);
	m0_free(dfom);
	M0_LEAVE();
}

#ifndef __KERNEL__

/**
 * Confc callback. Is used when m0_confc_open or m0_confc_open_by_fid
 * finished asyn for wakeup this FOM and finialize Conc query
 */
static bool sss_dfom_confc_ctx_check_cb(struct m0_clink *clink)
{
	struct m0_sss_dfom  *dfom = container_of(clink, struct m0_sss_dfom,
						 ssm_clink);
	struct m0_confc_ctx *confc_ctx = &dfom->ssm_confc_ctx;

	if (m0_confc_ctx_is_completed(confc_ctx)) {
		m0_clink_del(clink);
		m0_clink_fini(clink);
		m0_fom_wakeup(&dfom->ssm_fom);
	}
	return true;
}

static inline uint32_t sss_dfom_device_cmd(struct m0_sss_dfom *dfom)
{
	return m0_sss_fop_to_dev_req(dfom->ssm_fom.fo_fop)->ssd_cmd;
}

/**
 * Select next phase depending on device command and current stage.
 * If command is unknown then return -ENOENT and fom state machine will
 * go to M0_FOPH_FAILURE phase.
 */
static void sss_device_fom_switch(struct m0_fom *fom)
{
	static const enum sss_device_fom_phases next_phase[][2] = {
		[M0_DEVICE_ATTACH] = { SSS_DFOM_SDEV_OPENING,
				       SSS_DFOM_ATTACH_POOL_MACHINE },
		[M0_DEVICE_DETACH] = { SSS_DFOM_DETACH_STOB,
				       SSS_DFOM_DETACH_POOL_MACHINE },
		[M0_DEVICE_FORMAT] = { M0_FOPH_TXN_INIT,
				       SSS_DFOM_FORMAT },
	};
	uint32_t            cmd;
	struct m0_sss_dfom *dfom = container_of(fom, struct m0_sss_dfom,
						ssm_fom);

	M0_ENTRY("fom %p, state %d", fom, m0_fom_phase(fom));
	M0_PRE(fom != NULL);
	M0_PRE(M0_IN(dfom->ssm_stage, (SSS_DEVICE_FOM_STAGE_STOB,
				       SSS_DEVICE_FOM_STAGE_POOL_MACHINE)));

	/* If non native sdev (by IO service) then ignore STOB stage */
	if (dfom->ssm_stage == SSS_DEVICE_FOM_STAGE_STOB &&
	    !dfom->ssm_native_device) {
		/*
		 * Foreign disk->sdev (i.e. not of current IO service)
		 * ==> do not proceed to STOB stage.
		 */
		m0_fom_phase_set(fom, M0_FOPH_TXN_INIT);
	} else {
		cmd = sss_dfom_device_cmd(dfom);
		M0_ASSERT(cmd < M0_DEVICE_CMDS_NR);
		m0_fom_phase_set(fom, next_phase[cmd][dfom->ssm_stage]);
	}
	++dfom->ssm_stage;
}

static int sss_confc_ctx_init(struct m0_sss_dfom *dfom)
{
	struct m0_confc_ctx *ctx = &dfom->ssm_confc_ctx;
	struct m0_confc     *confc = m0_reqh2confc(m0_fom_reqh(&dfom->ssm_fom));
	int                  rc;

	rc = m0_confc_ctx_init(ctx, confc);
	M0_POST(ergo(rc == 0, confc == ctx->fc_confc));
	return M0_RC(rc);
}

static int sss_device_fom_conf_obj_open(struct m0_sss_dfom  *dfom,
					struct m0_fid       *fid)
{
	struct m0_confc_ctx *ctx = &dfom->ssm_confc_ctx;
	int                  rc;

	rc = sss_confc_ctx_init(dfom);
	if (rc == 0) {
		m0_clink_init(&dfom->ssm_clink, sss_dfom_confc_ctx_check_cb);
		m0_clink_add_lock(&ctx->fc_mach.sm_chan, &dfom->ssm_clink);
		m0_confc_open_by_fid(ctx, fid);
	}
	return M0_RC(rc);
}

static int sss_device_fom_disk_opening(struct m0_fom *fom)
{
	struct m0_sss_dfom *dfom;
	struct m0_fid      *disk_fid;

	M0_ENTRY();
	dfom = container_of(fom, struct m0_sss_dfom, ssm_fom);
	disk_fid = &m0_sss_fop_to_dev_req(fom->fo_fop)->ssd_fid;
	return M0_RC(sss_device_fom_conf_obj_open(dfom, disk_fid));
}

static bool sss_device_fom_conf_obj_ha_state_cb(struct m0_clink *link)
{
	struct m0_sss_dfom *dfom;

	M0_ENTRY();
	dfom = container_of(link, struct m0_sss_dfom, ssm_ha.clink);
	/*
	 * Here we just wake the fom up doing nothing else. It's not possible to
	 * finalise HA retrieval context right here as long as we are in the
	 * chan callback.
	 *
	 * Context finalisation as well as state acceptance are going to be done
	 * later. See sss_disk_info_use().
	 */
	m0_fom_wakeup(&dfom->ssm_fom);
	M0_LEAVE();
	return true;
}

static void sss_device_fom_ha_init(struct m0_sss_dfom  *dfom,
				   struct m0_fid       *fid)
{
	m0_mutex_init(&dfom->ssm_ha.chan_lock);
	m0_chan_init(&dfom->ssm_ha.chan, &dfom->ssm_ha.chan_lock);
	m0_clink_init(&dfom->ssm_ha.clink,
		      sss_device_fom_conf_obj_ha_state_cb);
	m0_clink_add_lock(&dfom->ssm_ha.chan, &dfom->ssm_ha.clink);

	dfom->ssm_ha.note.no_id    = *fid;
	dfom->ssm_ha.note.no_state = M0_NC_UNKNOWN;
	dfom->ssm_ha.nvec.nv_nr    = 1;
	dfom->ssm_ha.nvec.nv_note  = &dfom->ssm_ha.note;
}

static void sss_device_fom_ha_fini(struct m0_sss_dfom *dfom)
{
	m0_clink_del_lock(&dfom->ssm_ha.clink);
	m0_clink_fini(&dfom->ssm_ha.clink);
	m0_mutex_lock(&dfom->ssm_ha.chan_lock);
	m0_chan_fini(&dfom->ssm_ha.chan);
	m0_mutex_unlock(&dfom->ssm_ha.chan_lock);
	m0_mutex_fini(&dfom->ssm_ha.chan_lock);
}

static void sss_device_fom_ha_update(struct m0_sss_dfom *dfom)
{
	struct m0_conf_obj *obj = dfom->ssm_confc_ctx.fc_result;
	struct m0_sss_device_fop_rep *rep =
		m0_sss_fop_to_dev_rep(dfom->ssm_fom.fo_rep_fop);

	M0_PRE(obj != NULL);
	M0_PRE(m0_fid_eq(&obj->co_id, &dfom->ssm_ha.note.no_id));
	M0_PRE(dfom->ssm_ha.note.no_state != M0_NC_UNKNOWN);
	M0_PRE(rep->ssdp_ha_state == M0_NC_UNKNOWN);

	rep->ssdp_ha_state = obj->co_ha_state;
	m0_ha_state_accept(&dfom->ssm_ha.nvec, false);
	/*
	 * Now we need to make sure the state has been successfully applied to
	 * the object we are to deal with later. This implies the dfom operates
	 * with a confc registered as an HA client, i.e. m0_ha_client_add() was
	 * done for the confc instance.
	 */
	M0_POST(obj->co_ha_state == dfom->ssm_ha.note.no_state);
}

/**
 * Invokes device HA state retrieval using fid from request.
 */
static int sss_device_fom_disk_ha_state_get(struct m0_fom *fom)
{
	struct m0_sss_dfom *dfom;
	struct m0_fid      *disk_fid;
	int                 rc;

	M0_ENTRY();
	dfom = container_of(fom, struct m0_sss_dfom, ssm_fom);
	disk_fid = &m0_sss_fop_to_dev_req(fom->fo_fop)->ssd_fid;
	sss_device_fom_ha_init(dfom, disk_fid);
	rc = m0_ha_state_get(&dfom->ssm_ha.nvec, &dfom->ssm_ha.chan);
	if (rc != 0)
		sss_device_fom_ha_fini(dfom);
	return M0_RC(rc);
}

static int sss_disk_info_use(struct m0_sss_dfom *dfom)
{
	struct m0_confc_ctx *ctx = &dfom->ssm_confc_ctx;
	struct m0_conf_drive *disk;
	int                  rc;

	if (sss_dfom_device_cmd(dfom) == M0_DEVICE_ATTACH) {
		/* complete previous phase now with unlocked context */
		sss_device_fom_ha_fini(dfom);
		sss_device_fom_ha_update(dfom);
	}

	rc = m0_confc_ctx_error_lock(ctx);
	if (rc == 0) {
		disk = M0_CONF_CAST(m0_confc_ctx_result(ctx), m0_conf_drive);
		dfom->ssm_fid = disk->ck_sdev->sd_obj.co_id;
		m0_confc_close(&disk->ck_obj);
	}
	m0_confc_ctx_fini(ctx);
	return M0_RC(rc);
}

/**
 * Initialises custom Device fom fields using information from
 * m0_conf_drive object.
 */
static int sss_device_fom_disk_opened(struct m0_fom *fom)
{
	struct m0_sss_dfom  *dfom = container_of(fom, struct m0_sss_dfom,
						 ssm_fom);
	struct m0_confc_ctx *ctx = &dfom->ssm_confc_ctx;
	struct m0_confc     *confc = m0_reqh2confc(m0_fom_reqh(fom));
	int                  rc;

	M0_ENTRY();
	M0_PRE(confc == ctx->fc_confc);

	rc = sss_disk_info_use(dfom) ?: sss_confc_ctx_init(dfom);
	if (rc == 0) {
		m0_clink_init(&dfom->ssm_clink, sss_dfom_confc_ctx_check_cb);
		m0_clink_add_lock(&ctx->fc_mach.sm_chan, &dfom->ssm_clink);
		m0_confc_open(ctx, confc->cc_root, M0_FID0);
	}
	return M0_RC(rc);
}

static bool _obj_is_sdev(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SDEV_TYPE;
}

static bool sss_device_find_has_svc(struct m0_fom *fom, struct m0_fid *svc_fid)
{
	struct m0_reqh         *reqh = m0_fom_reqh(fom);
	struct m0_reqh_service *svc = m0_reqh_service_lookup(reqh, svc_fid);

	return svc != NULL && svc->rs_type == &m0_ios_type;
}

/**
 * Conf iterator for search storage device parent - IO service
 * If storage device found then check "parent (IO service) has in
 * current reqh"
 */
static int sss_device_fom_sdev_iter(struct m0_fom *fom)
{
	struct m0_sss_dfom *dfom;
	struct m0_conf_obj *obj;
	struct m0_fid       svc_fid;
	int                 rc;

	M0_ENTRY();

	dfom = container_of(fom, struct m0_sss_dfom, ssm_fom);

	while ((rc = m0_conf_diter_next(&dfom->ssm_it, _obj_is_sdev)) ==
			M0_CONF_DIRNEXT) {
		obj = m0_conf_diter_result(&dfom->ssm_it);
		if (m0_fid_eq(&dfom->ssm_fid, &obj->co_id)) {
			svc_fid = m0_conf_obj_grandparent(obj)->co_id;
			dfom->ssm_native_device = sss_device_find_has_svc(
						&dfom->ssm_fom, &svc_fid);
			rc = M0_CONF_DIREND;
			break;
		}
	}

	/*
	 * If rc == M0_CONF_DIRMISS, then ios_start_disk_iter_cb() will be
	 * called once directory entry is loaded
	 */
	if (rc != M0_CONF_DIRMISS) {
		/* End of directory or error */
		m0_clink_del_lock(&dfom->ssm_clink);
		m0_clink_fini(&dfom->ssm_clink);
		m0_conf_diter_fini(&dfom->ssm_it);
		m0_confc_ctx_fini(&dfom->ssm_confc_ctx);
		m0_fom_phase_move(&dfom->ssm_fom, 0, SSS_DFOM_SWITCH);
	}
	return M0_RC(rc);
}

static bool sss_device_fom_sdev_iter_cb(struct m0_clink *clink)
{
	struct m0_sss_dfom *dfom = container_of(clink, struct m0_sss_dfom,
						ssm_clink);
	m0_fom_wakeup(&dfom->ssm_fom);
	return true;
}

static int sss_device_fom_root_info_use(struct m0_sss_dfom  *dfom,
					struct m0_conf_obj **root_obj)
{
	struct m0_confc_ctx *confc_ctx = &dfom->ssm_confc_ctx;
	int                  rc;

	rc = m0_confc_ctx_error_lock(confc_ctx);
	if (rc == 0)
		*root_obj = m0_confc_ctx_result(confc_ctx);
	return M0_RC(rc);
}

static int sss_device_fom_sdev_iter_init(struct m0_sss_dfom *dfom,
					 struct m0_conf_obj *root_obj)
{
	struct m0_confc *confc = m0_reqh2confc(m0_fom_reqh(&dfom->ssm_fom));
	int              rc;

	rc = m0_conf_diter_init(&dfom->ssm_it, confc, root_obj,
				M0_CONF_ROOT_NODES_FID,
				M0_CONF_NODE_PROCESSES_FID,
				M0_CONF_PROCESS_SERVICES_FID,
				M0_CONF_SERVICE_SDEVS_FID);
	m0_confc_close(root_obj);
	if (rc == 0) {
		m0_clink_init(&dfom->ssm_clink, sss_device_fom_sdev_iter_cb);
		m0_clink_add_lock(&dfom->ssm_it.di_wait, &dfom->ssm_clink);
		m0_fom_phase_move(&dfom->ssm_fom, 0, SSS_DFOM_SDEV_ITER);
	}
	return M0_RC(rc);
}

/**
 * Initialize custom Device fom fields using information from obtained
 * disk conf object.
 */
static int sss_device_fom_fs_opened(struct m0_fom *fom)
{
	struct m0_sss_dfom *dfom = container_of(fom, struct m0_sss_dfom,
						ssm_fom);
	struct m0_conf_obj *root_obj = NULL;
	int                 rc;

	M0_ENTRY();

	rc = sss_device_fom_root_info_use(dfom, &root_obj) ?:
	     sss_device_fom_sdev_iter_init(dfom, root_obj) ?:
	     sss_device_fom_sdev_iter(fom);
	if (!M0_IN(rc, (0, M0_CONF_DIRMISS, M0_CONF_DIREND)))
		m0_confc_ctx_fini(&dfom->ssm_confc_ctx);
	return M0_RC(rc);
}

static int sss_device_fom_sdev_opening(struct m0_fom *fom)
{
	struct m0_sss_dfom *dfom;

	M0_ENTRY();
	dfom = container_of(fom, struct m0_sss_dfom, ssm_fom);
	return M0_RC(sss_device_fom_conf_obj_open(dfom, &dfom->ssm_fid));
}

static int sss_device_stob_attach(struct m0_fom *fom)
{
	struct m0_sss_dfom     *dfom;
	struct m0_storage_devs *devs = m0_cs_storage_devs_get();
	struct m0_storage_dev  *dev;
	struct m0_storage_dev  *dev_new = NULL;
	struct m0_confc_ctx    *confc_ctx;
	struct m0_conf_sdev    *sdev;
	int                     rc;

	M0_ENTRY();
	dfom = container_of(fom, struct m0_sss_dfom, ssm_fom);
	confc_ctx = &dfom->ssm_confc_ctx;
	rc = m0_confc_ctx_error_lock(confc_ctx);
	if (rc != 0)
		goto out;
	sdev = M0_CONF_CAST(m0_confc_ctx_result(confc_ctx),
				    m0_conf_sdev);
	M0_LOG(M0_DEBUG, "sdev fid"FID_F"device index:%d",
			FID_P(&sdev->sd_obj.co_id), (int)sdev->sd_dev_idx);
	m0_storage_devs_lock(devs);
	dev = m0_storage_devs_find_by_cid(devs, sdev->sd_dev_idx);
	m0_storage_devs_unlock(devs);
	if (dev != NULL) {
		rc = M0_ERR(-EEXIST);
		goto confc_close;
	}
	/*
	 * Enclose domain creation into m0_fom_block_{enter,leave}()
	 * block since it is possibly long operation.
	 */
	m0_fom_block_enter(fom);
	rc = m0_storage_dev_new_by_conf(devs, sdev, false, &dev_new);
	m0_fom_block_leave(fom);
	if (rc == 0) {
		m0_storage_devs_lock(devs);
		/*
		 * There is a race window where the devs is unlocked above.
		 * But current fom is the only place where a storage device
		 * can be attached "on fly". Therefore, there is no user that
		 * would try to create and attach storage device with the
		 * same cid.
		 */
		m0_storage_dev_attach(dev_new, devs);
		m0_storage_devs_unlock(devs);
	}
confc_close:
	m0_confc_close(&sdev->sd_obj);
out:
	m0_confc_ctx_fini(&dfom->ssm_confc_ctx);
	return M0_RC(rc);
}

static int sss_device_pool_machine_attach(struct m0_fom *fom)
{
	struct m0_sss_dfom *dfom = container_of(fom, struct m0_sss_dfom,
						ssm_fom);
	M0_ENTRY();
	return M0_RC(m0_pool_device_state_update(m0_fom_reqh(&dfom->ssm_fom),
						 &fom->fo_tx.tx_betx,
						 &dfom->ssm_fid,
						 M0_PNDS_ONLINE));
}

static int sss_device_pool_machine_detach(struct m0_fom *fom)
{
	struct m0_sss_dfom *dfom = container_of(fom, struct m0_sss_dfom,
						ssm_fom);
	M0_ENTRY();
	return M0_RC(m0_pool_device_state_update(m0_fom_reqh(&dfom->ssm_fom),
						 &fom->fo_tx.tx_betx,
						 &dfom->ssm_fid,
						 M0_PNDS_OFFLINE));
}

/*
 * Find and detach m0_storage_dev object. If arm_wakeup is true the fom
 * is armed to wakeup when stob detach operation is completed.
 */
static int sss_device_stob_detach(struct m0_fom *fom, bool arm_wakeup)
{
	struct m0_sss_dfom     *dfom;
	struct m0_storage_devs *devs = m0_cs_storage_devs_get();
	struct m0_storage_dev  *dev;
	int                     rc;
	struct m0_confc        *confc = m0_reqh2confc(m0_fom_reqh(fom));
	struct m0_conf_sdev    *sdev;

	M0_ENTRY();
	dfom = container_of(fom, struct m0_sss_dfom, ssm_fom);
	rc = m0_conf_sdev_get(confc, &dfom->ssm_fid, &sdev);
	if (rc != 0)
		return M0_ERR(rc);
	M0_LOG(M0_DEBUG, "sdev="FID_F" dev_id=%u", FID_P(&sdev->sd_obj.co_id),
	       sdev->sd_dev_idx);
	m0_storage_devs_lock(devs);
	dev = m0_storage_devs_find_by_cid(devs, sdev->sd_dev_idx);
	if (dev == NULL)
		rc = M0_ERR(-ENOENT);
	else {
		if (arm_wakeup) {
			m0_chan_lock(&dev->isd_detached_chan);
			m0_fom_wait_on(fom, &dev->isd_detached_chan,
				       &fom->fo_cb);
			m0_chan_unlock(&dev->isd_detached_chan);
		}
		m0_storage_dev_detach(dev);
	}
	m0_storage_devs_unlock(devs);
	m0_confc_close(&sdev->sd_obj);
	return M0_RC(rc);
}

static int sss_device_format(struct m0_fom *fom)
{
	struct m0_sss_dfom     *dfom;
	struct m0_storage_devs *devs = m0_cs_storage_devs_get();
	struct m0_storage_dev  *dev;
	int                     rc;
	struct m0_confc        *confc = m0_reqh2confc(m0_fom_reqh(fom));
	struct m0_conf_sdev    *sdev;

	M0_ENTRY();
	dfom = container_of(fom, struct m0_sss_dfom, ssm_fom);
	rc = m0_conf_sdev_get(confc, &dfom->ssm_fid, &sdev);
	if (rc != 0)
		return M0_ERR(rc);
	M0_LOG(M0_DEBUG, "sdev="FID_F" dev_id=%u", FID_P(&sdev->sd_obj.co_id),
	       sdev->sd_dev_idx);
	m0_storage_devs_lock(devs);
	dev = m0_storage_devs_find_by_cid(devs, sdev->sd_dev_idx);
	/*
	 * Note. If device not attached yet then dev equal NULL.
	 */
	rc = m0_storage_dev_format(dev, sdev->sd_dev_idx);
	m0_storage_devs_unlock(devs);
	m0_confc_close(&sdev->sd_obj);
	return M0_RC(rc);
}

/**
 * Device command fom tick
 *
 * Besides derive custom phases, check M0_FOPH_TXN_INIT and
 * M0_FOPH_TXN_OPEN phase.
 *
 * If M0_FOPH_TXN_INIT phase expected and current stage is
 * SSS_DEVICE_FOM_STAGE_STOB then switch fom to first custom phase.
 */
static int sss_device_fom_tick(struct m0_fom *fom)
{
	struct m0_sss_device_fop_rep *rep;
	struct m0_sss_dfom *dfom = container_of(fom,
				struct m0_sss_dfom, ssm_fom);

	M0_ENTRY("fom %p, state %d", fom, m0_fom_phase(fom));
	M0_PRE(fom != NULL);

	/* first handle generic phase */
	if (m0_fom_phase(fom) < M0_FOPH_NR) {
		if (m0_fom_phase(fom) == M0_FOPH_TXN_INIT) {
			/* If stage "work with stob" then goto custom phases */
			if (dfom->ssm_stage == SSS_DEVICE_FOM_STAGE_STOB) {
				m0_fom_phase_move(fom, 0,
						  SSS_DFOM_DISK_OPENING);
				return M0_FSO_AGAIN;
			}
		}
		return m0_fom_tick_generic(fom);
	}

	rep = m0_sss_fop_to_dev_rep(fom->fo_rep_fop);

	switch (m0_fom_phase(fom)) {

	case SSS_DFOM_DISK_OPENING:
		rep->ssdp_rc = sss_device_fom_disk_opening(fom);
		m0_fom_phase_moveif(fom, rep->ssdp_rc,
				    SSS_DFOM_DISK_HA_STATE_GET,
				    M0_FOPH_FAILURE);
		return rep->ssdp_rc == 0 ? M0_FSO_WAIT : M0_FSO_AGAIN;

	case SSS_DFOM_DISK_HA_STATE_GET:
		if (sss_dfom_device_cmd(dfom) != M0_DEVICE_ATTACH) {
			m0_fom_phase_set(fom, SSS_DFOM_DISK_OPENED);
			return M0_FSO_AGAIN;
		}
		rep->ssdp_rc = sss_device_fom_disk_ha_state_get(fom);
		m0_fom_phase_moveif(fom, rep->ssdp_rc,
				    SSS_DFOM_DISK_OPENED, M0_FOPH_FAILURE);
		return rep->ssdp_rc == 0 ? M0_FSO_WAIT : M0_FSO_AGAIN;

	case SSS_DFOM_DISK_OPENED:
		rep->ssdp_rc = sss_device_fom_disk_opened(fom);
		m0_fom_phase_moveif(fom, rep->ssdp_rc,
				    SSS_DFOM_FS_OPENED, M0_FOPH_FAILURE);
		return rep->ssdp_rc == 0 ? M0_FSO_WAIT : M0_FSO_AGAIN;

	case SSS_DFOM_FS_OPENED:
		rep->ssdp_rc = sss_device_fom_fs_opened(fom);
		if (!M0_IN(rep->ssdp_rc, (M0_CONF_DIRMISS, M0_CONF_DIREND)))
			m0_fom_phase_move(fom, rep->ssdp_rc, M0_FOPH_FAILURE);
		return rep->ssdp_rc == M0_CONF_DIRMISS ? M0_FSO_WAIT :
							 M0_FSO_AGAIN;

	case SSS_DFOM_SDEV_ITER:
		rep->ssdp_rc = sss_device_fom_sdev_iter(fom);
		if (!M0_IN(rep->ssdp_rc, (M0_CONF_DIRMISS, M0_CONF_DIREND)))
			m0_fom_phase_move(fom, rep->ssdp_rc, M0_FOPH_FAILURE);
		return rep->ssdp_rc == M0_CONF_DIRMISS ? M0_FSO_WAIT :
							 M0_FSO_AGAIN;

	case SSS_DFOM_SWITCH:
		sss_device_fom_switch(fom);
		return M0_FSO_AGAIN;

	case SSS_DFOM_SDEV_OPENING:
		rep->ssdp_rc = sss_device_fom_sdev_opening(fom);
		m0_fom_phase_moveif(fom, rep->ssdp_rc,
				    SSS_DFOM_ATTACH_STOB, M0_FOPH_FAILURE);
		return rep->ssdp_rc == 0 ? M0_FSO_WAIT : M0_FSO_AGAIN;

	case SSS_DFOM_ATTACH_STOB:
		rep->ssdp_rc = sss_device_stob_attach(fom);
		m0_fom_phase_move(fom, 0, M0_FOPH_TXN_INIT);
		return M0_FSO_AGAIN;

	case SSS_DFOM_ATTACH_POOL_MACHINE:
		rep->ssdp_rc = sss_device_pool_machine_attach(fom);
		if (rep->ssdp_rc != 0)
			sss_device_stob_detach(fom, false);
		m0_fom_phase_moveif(fom, rep->ssdp_rc, M0_FOPH_SUCCESS,
				    M0_FOPH_FAILURE);
		return M0_FSO_AGAIN;

	case SSS_DFOM_DETACH_STOB:
		/* calls m0_fom_wait_on() inside */
		rep->ssdp_rc = sss_device_stob_detach(fom, true);
		m0_fom_phase_move(fom, 0, SSS_DFOM_DETACH_STOB_WAIT);
		return rep->ssdp_rc == 0 ? M0_FSO_WAIT : M0_FSO_AGAIN;

	case SSS_DFOM_DETACH_STOB_WAIT:
		m0_fom_phase_move(fom, 0, M0_FOPH_TXN_INIT);
		return M0_FSO_AGAIN;

	case SSS_DFOM_DETACH_POOL_MACHINE:
		rep->ssdp_rc = sss_device_pool_machine_detach(fom);
		m0_fom_phase_moveif(fom, rep->ssdp_rc, M0_FOPH_SUCCESS,
				    M0_FOPH_FAILURE);
		return M0_FSO_AGAIN;

	case SSS_DFOM_FORMAT:
		rep->ssdp_rc = sss_device_format(fom);
		m0_fom_phase_moveif(fom, rep->ssdp_rc, M0_FOPH_SUCCESS,
				    M0_FOPH_FAILURE);
		return M0_FSO_AGAIN;

	default:
		M0_IMPOSSIBLE("Invalid phase");
	}
	return M0_FSO_AGAIN;
}
#else
static int sss_device_fom_tick(struct m0_fom *fom)
{
	struct m0_sss_device_fop_rep *rep;

	if (m0_fom_phase(fom) < M0_FOPH_NR)
		return m0_fom_tick_generic(fom);

	rep = m0_sss_fop_to_dev_rep(fom->fo_rep_fop);
	rep->ssdp_rc = M0_ERR(-ENOENT);
	m0_fom_phase_move(fom, rep->ssdp_rc, M0_FOPH_FAILURE);
	return M0_FSO_AGAIN;
}
#endif

static size_t sss_device_fom_home_locality(const struct m0_fom *fom)
{
	return 1;
}

/** @} */

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
