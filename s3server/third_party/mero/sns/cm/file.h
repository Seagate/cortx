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
 * Original author: Subhash Arya <subhash_arya@xyratex.com>
 *                  Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 09/12/2012
 */

#pragma once

#ifndef __MERO_SNS_CM_FILE_H__
#define __MERO_SNS_CM_FILE_H__

/**
   @defgroup SNSCMFILE SNS copy machine file context
   @ingroup SNSCM

   @{
 */

enum m0_sns_cm_fctx_state {
	M0_SCFS_INIT = 0,
	M0_SCFS_LOCK_WAIT,
	M0_SCFS_LOCKED,
	M0_SCFS_ATTR_FETCH,
	M0_SCFS_ATTR_FETCHED,
	M0_SCFS_LAYOUT_FETCH,
	M0_SCFS_LAYOUT_FETCHED,
	M0_SCFS_FINI,
	M0_SCFS_NR
};

/**
 * Holds the rm file lock context of a file that is being repaired/rebalanced.
 * @todo Use the same object to hold layout context.
 */
struct m0_sns_cm_file_ctx {
	/** Holds M0_SNS_CM_FILE_CTX_MAGIC. */
	uint64_t                    sf_magic;

	/** Resource manager file lock for this file. */
	struct m0_file              sf_file;

	/**
	 * The global file identifier for this file. This would be used
	 * as a "key" to the m0_sns_cm::sc_file_ctx and hence cannot
	 * be a pointer.
	 */
	struct m0_fid               sf_fid;

	struct m0_cob_attr          sf_attr;

	uint64_t                    sf_max_group;

	/** Poolmach for this file. */
	struct m0_poolmach         *sf_pm;

	struct m0_pooldev          *sf_pd;

	struct m0_layout           *sf_layout;

	/** pdclust instance for a particular GOB. */
	struct m0_pdclust_instance *sf_pi;

	struct m0_mutex             sf_lock;

	/** Linkage into m0_sns_cm::sc_file_ctx. */
	struct m0_hlink             sf_sc_link;

	/** An owner for maintaining this file's lock. */
	struct m0_rm_owner          sf_owner;

	/** Remote portal for requesting resource from creditor. */
	struct m0_rm_remote         sf_creditor;

	/** Request to borrow resource (file lock) from creditor. */
	struct m0_rm_incoming       sf_rin;

	/** Back pointer to the sns copy machine. */
	struct m0_sns_cm           *sf_scm;

	/**
	 * Count of aggregation groups that would be processed for this fid.
	 * When all the aggregagtion groups have been processed, the
	 * file lock can be released.
	 */
	uint64_t                    sf_ag_nr;

	/**
	 * Holds the reference count for this object. The reference is
	 * incremented explicitly every time the file lock is acquired.
	 * The reference is decremented by calling m0_sns_cm_file_unlock().
	 * The m0_sns_cm_file_ctx object is freed (and file is unlocked) whenever
	 * the reference count equals zero.
	 */
	struct m0_ref               sf_ref;

	struct m0_sm                sf_sm;

	struct m0_sm_group         *sf_group;

	struct m0_sm_ast            sf_attr_ast;

	struct m0_sm_ast            sf_fini_ast;

	struct m0_clink             sf_fini_clink;

	/** Index of the ioservice last visited to get file attributes. */
	uint64_t                    sf_nr_ios_visited;
	int                         sf_rc;
};

/** Allocates and initialises new m0_sns_cm_file_ctx object. */
M0_INTERNAL int m0_sns_cm_fctx_init(struct m0_sns_cm *scm,
				    const struct m0_fid *fid,
				    struct m0_sns_cm_file_ctx **sc_fctx);

M0_INTERNAL void m0_sns_cm_fctx_fini(struct m0_sns_cm_file_ctx *fctx);

/*
 * Invokes async file lock api and adds the given fctx object to
 * m0_sns_cm::sc_file_ctx hash table. Its the responsibility of
 * the caller to wait for the lock on rm's incoming channel where
 * the state changes are announced.
 * @see m0_sns_cm_file_lock_wait()
 * @ret -EAGAIN.
 */
M0_INTERNAL int m0_sns_cm_file_lock(struct m0_sns_cm *scm,
				    const struct m0_fid *fid,
				    struct m0_sns_cm_file_ctx **out);
/**
 * Returns -EAGAIN until the rm file lock is acquired. The given
 * fom waits on the the rm incoming channel till the file lock is acquired.
 * @ret 0 When file lock is acquired successfully.
 * @ret -EFAULT     When file lock acquisition fails.
 * @ret -EAGAIN When waiting for the file lock to be acquired.
 */
M0_INTERNAL int
m0_sns_cm_file_lock_wait(struct m0_sns_cm_file_ctx *fctx,
			 struct m0_fom *fom);

/**
 * Decrements the reference on the m0_sns_cm_file_ctx object.
 * When the count reaches null, m0_file_unlock() is invoked and
 * the m0_sns_cm_file_ctx_object is freed.
 * @see m0_sns_cm_file_unlock()
 */
M0_INTERNAL void m0_sns_cm_file_unlock(struct m0_sns_cm *scm,
				       struct m0_fid *fid);

/**
 * Looks up the m0_sns_cm::sc_file_ctx hash table and returns the
 * m0_sns_cm_file_ctx object for the passed global fid.
 * Returns NULL if the object is not present in the hash table.
 */
M0_INTERNAL struct m0_sns_cm_file_ctx *
m0_sns_cm_fctx_locate(struct m0_sns_cm *scm, struct m0_fid *fid);

M0_INTERNAL  struct m0_sns_cm_file_ctx *
m0_sns_cm_fctx_get(struct m0_sns_cm *scm, const struct m0_cm_ag_id *id);

M0_INTERNAL void m0_sns_cm_fctx_put(struct m0_sns_cm *scm,
                                    const struct m0_cm_ag_id *id);

M0_INTERNAL void m0_sns_cm_fctx_cleanup(struct m0_sns_cm *scm);
M0_INTERNAL void m0_sns_cm_flock_resource_set(struct m0_sns_cm *scm);

M0_INTERNAL int
m0_sns_cm_file_attr_and_layout(struct m0_sns_cm_file_ctx *fctx);

M0_INTERNAL void
m0_sns_cm_file_attr_and_layout_wait(struct m0_sns_cm_file_ctx *fctx,
				    struct m0_fom *fom);

M0_INTERNAL int m0_sns_cm_fctx_state_get(struct m0_sns_cm_file_ctx *fctx);

M0_INTERNAL struct m0_pool_version *
m0_sns_cm_pool_version_get(struct m0_sns_cm_file_ctx *fctx);

M0_INTERNAL void m0_sns_cm_file_fwd_map(struct m0_sns_cm_file_ctx *fctx,
					const struct m0_pdclust_src_addr *sa,
					struct m0_pdclust_tgt_addr *ta);

M0_INTERNAL void m0_sns_cm_file_bwd_map(struct m0_sns_cm_file_ctx *fctx,
					const struct m0_pdclust_tgt_addr *ta,
					struct m0_pdclust_src_addr *sa);

M0_INTERNAL void m0_sns_cm_fctx_lock(struct m0_sns_cm_file_ctx *fctx);
M0_INTERNAL void m0_sns_cm_fctx_unlock(struct m0_sns_cm_file_ctx *fctx);

M0_INTERNAL uint64_t m0_sns_cm_file_data_units(struct m0_sns_cm_file_ctx *fctx);
M0_INTERNAL bool m0_sns_cm_file_unit_is_EOF(struct m0_pdclust_layout *pl,
					    uint64_t nr_max_data_units,
					    uint64_t group, uint32_t unit);

M0_HT_DESCR_DECLARE(m0_scmfctx, M0_EXTERN);
M0_HT_DECLARE(m0_scmfctx, M0_EXTERN, struct m0_sns_cm_file_ctx,
              struct m0_fid);

/** @} SNSCMFILE */

#endif /* __MERO_SNS_CM_FILE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */

