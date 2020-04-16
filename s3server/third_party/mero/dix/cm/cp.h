/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 25/08/2016
 */

#pragma once

#ifndef __MERO_DIX_CM_CP_H__
#define __MERO_DIX_CM_CP_H__

#include "fid/fid.h"           /* m0_fid */
#include "fop/fom_long_lock.h" /* m0_long_lock_link */
#include "rpc/at.h"            /* m0_rpc_at_buf */
#include "cas/ctg_store.h"     /* m0_ctg_op */
#include "cm/cp.h"             /* m0_cm_cp */

/**
   @defgroup DIXCMCP DIX copy machine Copy packet
   @ingroup DIXCM

 */

/* Import */
struct m0_fom;
struct m0_cm;

extern const struct m0_cm_cp_ops m0_dix_cm_repair_cp_ops;
extern const struct m0_cm_cp_ops m0_dix_cm_rebalance_cp_ops;

/** DIX copy packet context. */
struct m0_dix_cm_cp {
	/** Base copy packet. */
	struct m0_cm_cp             dc_base;

	/** Catalog fid this copy packet is targeted to. */
	struct m0_fid               dc_ctg_fid;

	/**
	 * This is true if for the local/outgoing copy packet and false
	 * for incoming copy packet.
	 */
	bool                       dc_is_local;

	/**
	 * Catalogue operation for meta lookup and key/value insertion into
	 * ordinary catalogues.
	 */
	struct m0_ctg_op           dc_ctg_op;

	/** Last catalogue operation result code. */
	int                        dc_ctg_op_rc;

	/** Catalogue operation flags. */
	uint32_t                   dc_ctg_op_flags;

	/** Pointer to catalogue we are currently operating on. */
	struct m0_cas_ctg         *dc_ctg;

	/** Long lock link used to get write lock on ordinary catalogues. */
	struct m0_long_lock_link   dc_ctg_lock;
	/** Long lock link used to get read lock on meta catalogue. */
	struct m0_long_lock_link   dc_meta_lock;

	/** ADDB2 instrumentation for long lock. */
	struct m0_long_lock_addb2  dc_ctg_lock_addb2;
	/** ADDB2 instrumentation for meta long lock. */
	struct m0_long_lock_addb2  dc_meta_lock_addb2;

	/** Key/value transmission phase. */
	int                        dc_phase_transmit;

	/** Buffer for key. */
	struct m0_buf              dc_key;
	/** Buffer for value. */
	struct m0_buf              dc_val;
};

/** Key/value transmission phases. */
enum m0_dix_cm_phase_transmit {
	/** Key transmission phase. */
	DCM_PT_KEY = 0,
	/** Value transmission phase. */
	DCM_PT_VAL,
	/** Transmission phases number. */
	DCM_PT_NR
};

/**
 * Returns DIX copy packet context by embedded @cp context.
 *
 * @param cp Base copy packet.
 *
 * @ret   DIX CP context.
 */
M0_INTERNAL struct m0_dix_cm_cp *cp2dixcp(const struct m0_cm_cp *cp);

/**
 * Determines request handler locality for copy packet FOM.
 *
 * @param cp Base copy packet.
 *
 * @ret   Request handler locality.
 */
M0_INTERNAL uint64_t m0_dix_cm_cp_home_loc_helper(const struct m0_cm_cp *cp);

/**
 * Returns copy machine context by embedded @fom context.
 *
 * @param fom FOM context.
 *
 * @ret   Base CP context.
 */
M0_INTERNAL struct m0_cm *cpfom2cm(struct m0_fom *fom);

/** DIX copy packet invariant. */
M0_INTERNAL bool m0_dix_cm_cp_invariant(const struct m0_cm_cp *cp);

/**
 * Allocates DIX copy packet and returnes embedded base copy packet.
 *
 * @param cm Base copy machine.
 *
 * @ret Embedded base copy packet on success or NULL on failure.
 */
M0_INTERNAL struct m0_cm_cp *m0_dix_cm_cp_alloc(struct m0_cm *cm);

/**
 * Initialises DIX copy packet by embedded @cp context, allocates corresponding
 * aggregation group and sets up next phase of copy packet FOM.
 *
 * @param cp Base copy packet.
 *
 * @ret M0_FSO_AGAIN.
 */
M0_INTERNAL int m0_dix_cm_cp_init(struct m0_cm_cp *cp);

/**
 * Unlocks locked catalogues, sends reply and moves copy packet FOM to the final
 * state.
 *
 * @param cp Copy packet embedded into DIX CP.
 * @param ft Reply FOP type.
 *
 * @ret M0_FSO_WAIT.
 */
M0_INTERNAL int m0_dix_cm_cp_fail(struct m0_cm_cp    *cp,
				  struct m0_fop_type *ft);

/**
 * Does nothing but transition of copy packet FOM to M0_CCP_IO_WAIT state as
 * all data is aleady read by DIX iterator.
 *
 * @param cp Base copy packet.
 *
 * @ret M0_FSO_AGAIN.
 */
M0_INTERNAL int m0_dix_cm_cp_read(struct m0_cm_cp *cp);

/**
 * Initialises ctg operation and does lookup in meta catalogue.
 *
 * @param cp Base copy packet.
 *
 * @ret M0_FSO_AGAIN or M0_FSO_WAIT.
 */
M0_INTERNAL int m0_dix_cm_cp_xform(struct m0_cm_cp *cp);

/**
 * Initialises ctg operation and does insertion of key/value into catalogue.
 *
 * @param cp Base copy packet.
 *
 * @ret M0_FSO_AGAIN, M0_FSO_WAIT or negative error code.
 */
M0_INTERNAL int m0_dix_cm_cp_write(struct m0_cm_cp *cp);

/**
 * Locks catalogue for key/value insertion.
 *
 * @param cp Base copy packet.
 *
 * @ret M0_FSO_AGAIN, M0_FSO_WAIT or negative error code.
 */
M0_INTERNAL int m0_dix_cm_cp_write_pre(struct m0_cm_cp *cp);

/**
 * Checks I/O completion and sends reply.
 *
 * @param cp Base copy packet.
 * @param ft Reply FOP type.
 *
 * @ret M0_FSO_AGAIN, M0_FSO_WAIT or negative error code.
 */
M0_INTERNAL int m0_dix_cm_cp_io_wait(struct m0_cm_cp    *cp,
				     struct m0_fop_type *ft);

/**
 * Unconditionally transmits copy packet FOM into M0_CCP_SEND phase.
 * @note In DIX we do not care about sliding window and always ready to send.
 *
 * @param cp Base copy packet.
 *
 * @ret M0_FSO_AGAIN.
 */
M0_INTERNAL int m0_dix_cm_cp_sw_check(struct m0_cm_cp *cp);

/**
 * Allocates, initialises and sends the FOP which carries a DIX copy packet.
 *
 * @param cp Base copy packet embedded into DIX copy packet to be sent.
 * @param ft FOP type.
 *
 * @ret M0_FSO_AGAIN or M0_FSO_WAIT.
 */
M0_INTERNAL int m0_dix_cm_cp_send(struct m0_cm_cp *cp, struct m0_fop_type *ft);

/**
 * Transmits copy packet FOM into M0_CCP_FINI phase on success or into
 * M0_CCP_FINI on error.
 *
 * @param cp Base copy packet embedded into sent DIX copy packet.
 *
 * @ret M0_FSO_AGAIN or M0_FSO_WAIT.
 */
M0_INTERNAL int m0_dix_cm_cp_send_wait(struct m0_cm_cp *cp);

/**
 * Loads key or value depending on transmission phase using RPC AT buffers.
 *
 * @param cp Base copy packet.
 *
 * @ret M0_FSO_AGAIN, M0_FSO_WAIT or negative error code.
 */
M0_INTERNAL int m0_dix_cm_cp_recv_init(struct m0_cm_cp *cp);

/**
 * Manages key/value transmission phases and locks meta catalogue if key/value
 * are loaded.
 *
 * @param cp Base copy packet.
 *
 * @ret M0_FSO_AGAIN or M0_FSO_WAIT.
 */
M0_INTERNAL int m0_dix_cm_cp_recv_wait(struct m0_cm_cp *cp);

/**
 * Calculates read/write statistics.
 *
 * @param cp Base copy packet.
 */
M0_INTERNAL void m0_dix_cm_cp_complete(struct m0_cm_cp *cp);

/**
 * Removes embedded base copy packet from aggregation group (if such presented)
 * and frees DIX copy packet.
 *
 * @param cp Base copy packet.
 */
M0_INTERNAL void m0_dix_cm_cp_free(struct m0_cm_cp *cp);

/**
 * Wakes up pump FOM in case of local copy packet or finalises catalogues lock
 * links otherwise
 *
 * @param cp Base copy packet.
 *
 * @ret Always return 0 for now.
 */
M0_INTERNAL int m0_dix_cm_cp_fini(struct m0_cm_cp *cp);

/**
 * Fills target component catalogue fid of DIX copy packet.
 *
 * @param dix_cp   DIX copy packet to be sent.
 * @param cctg_fid Target component catalogue fid.
 */
M0_INTERNAL void m0_dix_cm_cp_tgt_info_fill(struct m0_dix_cm_cp *dix_cp,
					    const struct m0_fid *cctg_fid);

/**
 * Fills remote target info and initialises copy packet indices bitmap to all 1.
 *
 * @param dix_cp            DIX copy packet to be sent.
 * @param cctg_fid          Target component catalogue fid.
 * @param failed_unit_index Index of failed storage unit (not used for now).
 */
M0_INTERNAL void m0_dix_cm_cp_setup(struct m0_dix_cm_cp *dix_cp,
				    const struct m0_fid *cctg_fid,
				    uint64_t failed_unit_index);

/**
 * Duplicates DIX copy packet.
 *
 * @param[in]  src Base copy packet embedded into DIX copy packet to be copied.
 * @param[out] dst A copy of source copy packet.
 *
 * @ret 0 on success or negative error code.
 */
M0_INTERNAL int m0_dix_cm_cp_dup(struct m0_cm_cp *src, struct m0_cm_cp **dst);

/** @} DIXCMCP */
#endif /* __MERO_DIX_CM_CP_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
