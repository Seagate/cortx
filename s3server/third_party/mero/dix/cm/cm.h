/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Sergey Shilov <sergey.shilov@seagate.com>
 * Original creation date: 08/08/2016
 */

#pragma once

#ifndef __MERO_DIX_CM_H__
#define __MERO_DIX_CM_H__


/**
  @page DIXCMDLD-fspec DIX copy machine functional specification

  - @ref DIXCMDLD-fspec-ds
  - @ref DIXCMDLD-fspec-if
  - @ref DIXCMDLD-fspec-usecases-repair
  - @ref DIXCMDLD-fspec-usecases-rebalance

  @section DIXCMDLD-fspec Functional Specification
  DIX copy machine provides infrastructure to perform tasks like repair and
  re-balance using parity de-clustering layout.

  @subsection DIXCMDLD-fspec-ds Data Structures
  - m0_dix_cm
    Represents DIX copy machine, this embeds generic struct m0_cm and DIX
    specific copy machine objects.

  - m0_dix_cm_iter
    Represents DIX copy machine data iterator.

  @subsection DIXCMDLD-fspec-if Interfaces
  Functions to register/deregister DIX copy machine type and corresponding reqh
  service:
  - m0_dix_cm_type_register()
  - m0_dix_cm_type_deregister()

  Functions to setup, control and fini DIX copy machine:
  - m0_dix_cm_prepare();
  - m0_dix_cm_stop();
  - m0_dix_cm_setup();
  - m0_dix_cm_start();
  - m0_dix_cm_fini();

  Functions to init/fini repair/rebalance trigger fops:
  - m0_dix_cm_repair_trigger_fop_init();
  - m0_dix_cm_repair_trigger_fop_fini();
  - m0_dix_cm_rebalance_trigger_fop_init();
  - m0_dix_cm_rebalance_trigger_fop_fini();

  @subsection DIXCMDLD-fspec-usecases-repair DIX repair recipes
  N/A

  @subsection DIXCMDLD-fspec-usecases-rebalance DIX re-balance recipes
  N/A
 */

/**
  @defgroup DIXCM DIX copy machine
  @ingroup CM

  DIX copy machine is a replicated state machine, which performs data
  restructuring and handles device, container, node, &c failures.
  @see The @ref DIXCMDLD

  @{
*/

/**
 * Operation that DIX copy machine is carrying out.
 */
#include "lib/chan.h"     /* m0_clink */
#include "cm/cm.h"
#include "cm/repreb/cm.h"
#include "dix/cm/iter.h"

#define GRP_END_MARK_ID (struct m0_cm_ag_id) {           \
	.ai_hi = M0_UINT128((uint64_t)-1, (uint64_t)-1), \
	.ai_lo = M0_UINT128((uint64_t)-1, (uint64_t)-1)  \
}
/* Import */
enum m0_pool_nd_state;
struct m0_pdclust_layout;

#define M0_DIX_CM_TYPE_DECLARE(cmtype, id, ops, name, typecode) \
M0_CM_TYPE_DECLARE(cmtype, id, ops, name, typecode);            \
M0_INTERNAL struct m0_dix_cm_type cmtype ## _dcmt = {		\
	.dct_base = &cmtype ## _cmt,                            \
}

/** DIX copy machine type. */
struct m0_dix_cm_type {
	struct m0_cm_type  *dct_base;
	struct m0_fom_type  dct_iter_fomt;
};

/** Read/write stats for DIX CM. */
struct m0_dix_cm_stats {
	uint64_t dcs_read_size;
	uint64_t dcs_write_size;
};

/** DIX copy machine context. */
struct m0_dix_cm {
	/* Base copy machine context. */
	struct m0_cm           dcm_base;

	/* DIX copy machine specific type. */
	struct m0_dix_cm_type *dcm_type;

	/** Operation that dix copy machine is going to execute. */
	enum m0_cm_op          dcm_op;

	/** DIX copy machine data iterator. */
	struct m0_dix_cm_iter  dcm_it;

	/**
	 * Start time for DIX copy machine. This is recorded when the ready fop
	 * arrives to the DIX copy machine replica.
	 */
	m0_time_t              dcm_start_time;

	/**
	 * Stop time for DIX copy machine. This is recorded when repair is
	 * completed.
	 */
	m0_time_t              dcm_stop_time;

	/** Key for locality data that store total read/write size. */
	int                    dcm_stats_key;

	/**
	 * Indicates whether CM iterator is in process of retrieving next
	 * value.
	 */
	bool                   dcm_iter_inprogress;

	/** Indicates whether current CP is under processing. */
	bool                   dcm_cp_in_progress;

	/**
	 * Clink to detect that all proxies completed their local pump FOM.
	 */
	struct m0_clink        dcm_proxies_completed;

	/** Number of processed records, used for progress counter. */
	int                    dcm_processed_nr;
	/**
	 * Number of overall records to be processed, used for progress
	 * counter.
	 */
	uint64_t               dcm_recs_nr;

	/** Magic denoted by M0_DIX_CM_MAGIC. */
	uint64_t               dcm_magic;
};

M0_EXTERN const struct m0_cm_cp_ops m0_dix_cm_repair_cp_ops;
M0_EXTERN const struct m0_cm_cp_ops m0_dix_cm_rebalance_cp_ops;
M0_EXTERN struct m0_cm_type     dix_repair_cmt;
M0_EXTERN struct m0_cm_type     dix_rebalance_cmt;
M0_EXTERN struct m0_dix_cm_type dix_repair_dcmt;
M0_EXTERN struct m0_dix_cm_type dix_rebalance_dcmt;

/**
 * Registers DIX copy machine type and its corresponding request handler
 * service type.
 */
M0_INTERNAL int m0_dix_cm_type_register(void);
/**
 * De-registers DIX copy machine type and its corresponding request handler
 * service type.
 */
M0_INTERNAL void m0_dix_cm_type_deregister(void);

/**
 * Returns DIX copy machine context by embedded @cm context.
 *
 * @param cm Base copy machine context.
 *
 * @ret   DIX CM context.
 */
M0_INTERNAL struct m0_dix_cm *cm2dix(struct m0_cm *cm);

/**
 * Prepares DIX copy machine to operate where @cm is embedded.
 *
 * @param cm Base copy machine context.
 *
 * @ret   Always success for now.
 */
M0_INTERNAL int m0_dix_cm_prepare(struct m0_cm *cm);

/**
 * Stops DIX copy machine where @cm is embedded.
 *
 * @param cm Base copy machine context.
 */
M0_INTERNAL void m0_dix_cm_stop(struct m0_cm *cm);

/**
 * Does nothing.
 *
 * @ret Always success.
 */
M0_INTERNAL int m0_dix_cm_setup(struct m0_cm *cm);

/**
 * Starts DIX copy machine by embedded @cm context.
 *
 * @param cm      Base copy machine context.
 *
 * @ret   0       On success.
 * @ret   -ENOMEM On out of memory.
 * @ret   -errno  On error.
 */
M0_INTERNAL int m0_dix_cm_start(struct m0_cm *cm);

/**
 * Finalises DIX copy machine by embedded @cm context.
 *
 * @param cm Base copy machine context.
 */
M0_INTERNAL void m0_dix_cm_fini(struct m0_cm *cm);

/**
 * Finalises DIX copy machine service.
 *
 * @param service DIX copy machine service to be finalised.
 */
M0_INTERNAL void m0_dix_cm_svc_fini(struct m0_reqh_service *service);

/**
 * Returns state of DIX repair process with respect to @gfid.
 *
 * @param gfid Input global fid for which DIX repair state has to
 *             be retrieved.
 * @param reqh Parent request handler object.
 *
 * @pre   m0_fid_is_valid(gfid) && reqh != NULL.
 *
 * @ret   1 if DIX repair has not started at all.
 * @ret   2 if DIX repair has started but not completed for @gfid.
 * @ret   3 if DIX repair has started and completed for @gfid.
 */
M0_INTERNAL enum dix_repair_state
m0_dix_cm_fid_repair_done(struct m0_fid *gfid, struct m0_reqh *reqh,
			  enum m0_pool_nd_state device_state);

/** Initialises DIX repair trigger fops. */
M0_INTERNAL void m0_dix_cm_repair_trigger_fop_init(void);
/** Finalises DIX repair trigger fops. */
M0_INTERNAL void m0_dix_cm_repair_trigger_fop_fini(void);

/** Initialises DIX re-balance trigger fops. */
M0_INTERNAL void m0_dix_cm_rebalance_trigger_fop_init(void);
/** Finalises DIX re-balance trigger fops. */
M0_INTERNAL void m0_dix_cm_rebalance_trigger_fop_fini(void);

/** Initialises DIX repair sliding-window fop. */
M0_INTERNAL void m0_dix_repair_sw_onwire_fop_init(void);
/** Finalises DIX repair sliding-window fop. */
M0_INTERNAL void m0_dix_repair_sw_onwire_fop_fini(void);

/** Initialises DIX re-balance sliding-window fop. */
M0_INTERNAL void m0_dix_rebalance_sw_onwire_fop_init(void);
/** Finalises DIX re-balance sliding-window fop. */
M0_INTERNAL void m0_dix_rebalance_sw_onwire_fop_fini(void);

/**
 * Checks whether there is enough space to continue DIX repair/re-balance
 * process.
 *
 * @ret Always true for now.
 */
M0_INTERNAL int m0_dix_get_space_for(struct m0_cm             *cm,
				     const struct m0_cm_ag_id *id,
				     size_t                   *count);

/**
 * Gets next key/value using DIX CM iterator and prepares DIX copy packet to be
 * sent. Function returns M0_FSO_WAIT, M0_FSO_AGAIN or negative error code.
 *
 * @param cm Base copy machine context.
 * @param cp Base copy packet embedded into DIX copy packet.
 */
M0_INTERNAL int m0_dix_cm_data_next(struct m0_cm *cm, struct m0_cm_cp *cp);

/**
 * Function always returns -ENOENT as incoming aggregation group list is not
 * populated in case of DIX repair/re-balance, because local CM doesn't know how
 * many copy packets it will receive.
 */
M0_INTERNAL int m0_dix_cm_ag_next(struct m0_cm             *cm,
				  const struct m0_cm_ag_id *id_curr,
				  struct m0_cm_ag_id       *id_next);

/**
 * Returns true if remote replica identified by @ctx has CAS type.
 *
 * @param cm    Base copy machine context.
 * @param ctx   Remote replica.
 *
 * @ret   true  If service has CAS type.
 * @ret   false Otherwise.
 */
M0_INTERNAL bool m0_dix_is_peer(struct m0_cm               *cm,
				struct m0_reqh_service_ctx *ctx);

/**
 * Returns request handler context corresponding to DIX CM.
 *
 * @param dcm DIX copy machine.
 *
 * @ret   Request handler context.
 */
M0_INTERNAL struct m0_reqh *m0_dix_cm2reqh(const struct m0_dix_cm *dcm);

/** @} DIXCM */
#endif /* __MERO_DIX_CM_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
