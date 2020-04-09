/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 8-Jul-2016
 */

#pragma once

#ifndef __MERO_DIX_REQ_INTERNAL_H__
#define __MERO_DIX_REQ_INTERNAL_H__

/**
 * @defgroup dix
 *
 * @{
 */

#include "lib/buf.h"    /* m0_buf */
#include "lib/chan.h"   /* m0_chan */
#include "lib/vec.h"    /* m0_bufvec */
#include "lib/tlist.h"  /* m0_tlink */
#include "sm/sm.h"      /* m0_sm_ast */
#include "pool/pool.h"  /* m0_pool_nd_state */
#include "cas/client.h" /* m0_cas_req */

struct m0_dix_req;
struct m0_pool_version;
struct m0_dix_next_resultset;

struct m0_dix_item {
	struct m0_buf dxi_key;
	struct m0_buf dxi_val;
	/**
	 * Current parity group unit number for GET request. GET request is sent
	 * to target holding data unit at first. If this request fails, then
	 * iteration over parity units starts until request succeeds.
	 */
	uint64_t      dxi_pg_unit;
	/**
	 * Applicable only for index DELETE operation. Indicates that two-phase
	 * delete is necessary for the index.
	 */
	bool          dxi_del_phase2;
	int           dxi_rc;
};

struct m0_dix_cas_req {
	struct m0_dix_req *ds_parent;
	struct m0_cas_req  ds_creq;
	struct m0_clink    ds_clink;
	int                ds_rc;
};

struct m0_dix_idxop_req {
	struct m0_dix_idxop_ctx *dcr_ctx;
	uint32_t                 dcr_index_no;
	struct m0_pool_version  *dcr_pver;
	struct m0_dix_cas_req   *dcr_creqs;
	uint64_t                 dcr_creqs_nr;
	bool                     dcr_del_phase2;
};

struct m0_dix_idxop_ctx {
	struct m0_dix_idxop_req *dcd_idxop_reqs;
	uint32_t                 dcd_idxop_reqs_nr;
	uint64_t                 dcd_cas_reqs_nr;
	uint64_t                 dcd_completed_nr;
	struct m0_sm_ast         dcd_ast;
};

struct m0_dix_crop_attrs {
	/** Mapping to the index in user input vector. */
	uint64_t cra_item;
};

struct m0_dix_cas_rop {
	struct m0_dix_req        *crp_parent;
	struct m0_cas_req         crp_creq;
	/** CAS request flags (m0_cas_op_flags bitmask). */
	uint32_t                  crp_flags;
	struct m0_clink           crp_clink;
	uint32_t                  crp_sdev_idx;
	uint32_t                  crp_keys_nr;
	uint32_t                  crp_cur_key;
	struct m0_bufvec          crp_keys;
	struct m0_bufvec          crp_vals;
	/** Additional attributes for every key in crp_keys. */
	struct m0_dix_crop_attrs *crp_attrs;
	struct m0_tlink           crp_linkage;
	uint64_t                  crp_magix;
};

/**
 * Additional context linked with parity group units of record operation.
 * Note, that it contains a copy of some fields of pooldev from pool machine.
 * The copy is done to have a consistent view of pool machine during request
 * processing. A pool machine is locked only once during target devices
 * calculation and then copied information is used.
 */
struct m0_dix_pg_unit {
	/** Target storage device in a pool version. */
	uint32_t               dpu_tgt;

	/** Global storage device index. */
	uint32_t               dpu_sdev_idx;

	/** Storage device state. */
	enum m0_pool_nd_state  dpu_pd_state;

	/**
	 * Indicates whether this parity group is unavailable, e.g. the target
	 * for the unit has failed and not repaired yet.
	 */
	bool                   dpu_failed;
	bool                   dpu_is_spare;
	bool                   dpu_del_phase2;
};

struct m0_dix_rec_op {
	struct m0_dix_layout_iter dgp_iter;

	struct m0_buf             dgp_key;

	/**
	 * Current target for NEXT request. NEXT request doesn't use dgp_iter,
	 * because NEXT request is sent to all targets in a pool.
	 */
	uint32_t                  dgp_next_tgt;
	/** Mapping to the index in user input vector. */
	uint64_t                  dgp_item;
	struct m0_dix_pg_unit    *dgp_units;
	uint64_t                  dgp_units_nr;
	/**
	 * Total number of failed devices that are targets for record parity
	 * group units.
	 */
	uint32_t                  dgp_failed_devs_nr;
};

struct m0_dix_rop_ctx {
	struct m0_dix_rec_op   *dg_rec_ops;
	uint32_t                dg_rec_ops_nr;
	/** Array of targets number (P) size. */
	struct m0_dix_cas_rop **dg_target_rop;
	/** List of CAS requests, linked by m0_dix_cas_rop::crp_linkage. */
	struct m0_tl            dg_cas_reqs;
	uint64_t                dg_cas_reqs_nr;
	uint64_t                dg_completed_nr;
	/** Pool version where index under operation resides. */
	struct m0_pool_version *dg_pver;
	struct m0_sm_ast        dg_ast;
};

M0_TL_DESCR_DECLARE(cas_rop, M0_EXTERN);
M0_TL_DECLARE(cas_rop, M0_INTERNAL, struct m0_dix_cas_rop);

/**
 * Performs merge sorting of records retrieved for NEXT operation.
 *
 * Since NEXT operation queries all component catalogues for subsequent records,
 * then on completion received records shall be sorted, duplicates shall be
 * filtered and excessive records shall be thrown away from result.
 */
M0_INTERNAL int m0_dix_next_result_prepare(struct m0_dix_req *req);

/**
 * Initialise result set for NEXT operation.
 *
 * Result set is a context for merge-sorting records retrieved from all
 * component catalogues.
 *
 * This function is actually internal and exported only for UT purposes.
 * Normally it is called inside m0_dix_next_result_prepare().
 */
M0_INTERNAL int m0_dix_rs_init(struct m0_dix_next_resultset  *rs,
			       uint32_t                       start_keys_nr,
			       uint32_t                       sctx_nr);

/**
 * Finalises result set for NEXT operation.
 */
M0_INTERNAL void m0_dix_rs_fini(struct m0_dix_next_resultset *rs);

/** @} end of dix group */
#endif /* __MERO_DIX_REQ_INTERNAL_H__ */

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
