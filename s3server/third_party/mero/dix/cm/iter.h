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
 * Original creation date: 23-Aug-2016
 */

#pragma once

#ifndef __MERO_DIX_CM_ITER_H__
#define __MERO_DIX_CM_ITER_H__

#include "fop/fom.h"       /* m0_fom */
#include "lib/buf.h"       /* m0_buf */
#include "fid/fid.h"       /* m0_fid */
#include "sm/sm.h"         /* m0_sm_ast */
#include "dix/layout.h"    /* m0_dix_ldesc */
#include "cas/ctg_store.h" /* m0_ctg_op */

/**
 * @addtogroup dix
 *
 * @{
 */

/* Import */
struct m0_dix_cm_type;
struct m0_reqh;

/** DIX copy machine data iterator. */
struct m0_dix_cm_iter {
	/**
	 * Iterator state machine (FOM).
	 * FOM is used instead of simple SM because catalogue store is
	 * FOM-oriented.
	 */
	struct m0_fom              di_fom;

	/** Current component catalogue fid. */
	struct m0_fid              di_cctg_fid;

	/** Previous component catalogue fid. */
	struct m0_fid              di_prev_cctg_fid;

	/**
	 * Flag indicating that meta catalogue was modified while iterator was
	 * busy processing records. It is used to detect changes in meta btree
	 * and re-seek catalogue address. Iterator doesn't assume that address
	 * of current catalogue remains the same after meta btree changes.
	 */
	bool                       di_meta_modified;

	/** Clink which tracks meta catalogue modifications. */
	struct m0_clink            di_meta_clink;

	/** Current component catalogue. */
	struct m0_cas_ctg         *di_cctg;

	/**
	 * Number of processed records on scope of current component catalogue.
	 */
	uint64_t                   di_cctg_processed_recs_nr;

	/**
	 * Number of overall processed records.
	 */
	uint64_t                   di_processed_recs_nr;

	/**
	 * Layout of the distributed index to which current component catalogue
	 * belongs.
	 */
	struct m0_dix_ldesc        di_ldesc;

	/** Current key in the index. */
	struct m0_buf              di_key;

	/** Current value in the index. */
	struct m0_buf              di_val;

	/** Previous key in the index. */
	struct m0_buf              di_prev_key;

	/**
	 * Catalogue operation to iterate over records in catalogue-index
	 * catalogue.
	 */
	struct m0_ctg_op           di_ctidx_op;

	/**
	 * Catalogue operation to iterate over keys in the current catalogue.
	 */
	struct m0_ctg_op           di_ctg_op;

	/**
	 * Catalogue operation to delete key/value in case of re-balance.
	 */
	struct m0_ctg_op           di_ctg_del_op;

	/**
	 * Result of delete key/value operation in case of re-balance.
	 */
	int                        di_ctg_del_op_rc;

	/**
	 * Long lock link used to get read/write locks on ordinary catalogues.
	 */
	struct m0_long_lock_link   di_lock_link;

	/**
	 * Long lock link used to get read lock on meta catalogues
	 * (catalogue-index, meta).
	 */
	struct m0_long_lock_link   di_meta_lock_link;

	/**
	 * Long lock link used to get catalogue store "delete" lock.
	 * See m0_ctg_del_lock().
	 */
	struct m0_long_lock_link   di_del_lock_link;

	/** ADDB2 instrumentation for long lock. */
	struct m0_long_lock_addb2  di_lock_addb2;

	/** ADDB2 instrumentation for meta long lock. */
	struct m0_long_lock_addb2  di_meta_lock_addb2;

	/** ADDB2 instrumentation for del lock. */
	struct m0_long_lock_addb2  di_del_lock_addb2;

	/** AST to post 'stop' event to FOM. */
	struct m0_sm_ast           di_ast;

	/** Iterator stopping was requested by the user. */
	bool                       di_stop;

	/**
	 * Channel indicating that next key/value pair is retrieved (or EOF is
	 * reached).
	 */
	struct m0_chan             di_completed;

	/** Channel guard for di_completed. */
	struct m0_mutex            di_ch_guard;

	/** Target devices where lost data should be repaired. */
	uint64_t                  *di_tgts;

	/** Current target device where lost data should be repaired. */
	uint64_t                   di_tgts_cur;

	/** Number of target devices where lost data should be repaired. */
	uint64_t                   di_tgts_nr;

	/** Minimal threshold in bytes for transmission using bulk. */
	m0_bcount_t                di_cutoff;
};

/**
 * Registers DIX CM iterator FOM type.
 *
 * @param dcmt DIX CM iterator type.
 */
M0_INTERNAL void m0_dix_cm_iter_type_register(struct m0_dix_cm_type *dcmt);

/**
 * Starts DIX CM iterator by queueing of corresponding FOM for execution.
 * Function always returns success for now.
 *
 * @param iter       DIX CM iterator.
 * @param dcmt       DIX CM iterator type.
 * @param reqh       Corresponding request handler.
 * @param rpc_cutoff Threshold in bytes for transmission using bulk.
 *
 * @ret 0 On success.
 */
M0_INTERNAL int m0_dix_cm_iter_start(struct m0_dix_cm_iter *iter,
				     struct m0_dix_cm_type *dcmt,
				     struct m0_reqh        *reqh,
				     m0_bcount_t            rpc_cutoff);

/**
 * Wakes up DIX CM iterator FOM to move the iterator to the next record.
 * Than key/value can be retrieved using @m0_dix_cm_iter_get() function.
 *
 * @param iter DIX CM iterator.
 *
 * @see m0_dix_cm_iter_get()
 */
M0_INTERNAL void m0_dix_cm_iter_next(struct m0_dix_cm_iter *iter);

/**
 * Gets current key/value and remote device id for which retrieved key/value are
 * targeted to.
 * @note Key/value buffers are copied inside of this function, caller is
 *       responsible for their deallocation.
 *
 * @param[in]  iter    DIX CM iterator.
 * @param[in]  key     Buffer which data pointer will be set to key.
 * @param[in]  val     Buffer which data pointer will be set to value.
 * @param[out] sdev_id Remote device id.
 *
 * @ret 0 on success.
 * @ret -ENODATA if all local catalogues are processed.
 * @ret Other error code on iterator failure.
 *
 * @see m0_dix_cm_iter_next()
 */
M0_INTERNAL int m0_dix_cm_iter_get(struct m0_dix_cm_iter *iter,
				   struct m0_buf         *key,
				   struct m0_buf         *val,
				   uint32_t              *sdev_id);

/**
 * Tells DIX CM iterator to stop and waits for the final state of its FOM.
 * Please note that no external lock should be held before calling this
 * function, because it may wait and block. Otherwise, deadlock may appear
 * on that external lock.
 * @param iter DIX CM iterator.
 *
 * @see m0_dix_cm_iter_start()
 */
M0_INTERNAL void m0_dix_cm_iter_stop(struct m0_dix_cm_iter *iter);

/**
 * Gets fid of component catalogue that is currently under processing and number
 * of processed records of this component catalogue.
 *
 * @param[in]  iter              DIX CM iterator.
 * @param[out] cctg_fid          Current component catalogue fid.
 * @param[out] cctg_proc_recs_nr Number of processed records.
 */
M0_INTERNAL
void m0_dix_cm_iter_cur_pos(struct m0_dix_cm_iter *iter,
			    struct m0_fid         *cctg_fid,
			    uint64_t              *cctg_proc_recs_nr);

/**
 * Gets current number of overall processed records by iterator @iter.
 *
 * @param[in]  iter         DIX CM iterator.
 * @param[out] proc_recs_nr Number of overall processed records.
 */
M0_INTERNAL
void m0_dix_cm_iter_processed_num(struct m0_dix_cm_iter *iter,
				  uint64_t              *proc_recs_nr);

/** @} end of dix group */
#endif /* __MERO_DIX_CM_ITER_H__ */

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
