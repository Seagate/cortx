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
 * Original creation date: 15-Aug-2016
 */

#pragma once

#ifndef __MERO_DIX_REQ_H__
#define __MERO_DIX_REQ_H__

/**
 * @addtogroup dix
 *
 * @{
 *
 * DIX requests allow to access and modify distributed indices. Requests are
 * executed in the context of distributed index client.
 *
 * Record operations logic
 * -----------------------
 * GET, PUT, DEL operations share a common logic:
 * - Retrieve index layout descriptor. There are 3 cases:
 *   * Layout descriptor is provided by user: nothing to do.
 *   * Layout id is provided by user: lookup descriptor in layout-descr index.
 *   * Only index fid is specified: lookup descriptor starting from root index.
 * - Create index layout instance (@ref m0_dix_linst) based on layout
 *   descriptor.
 * - Calculate numerical orders of storage devices in a pool (Dn, Dp1 ... Dpk,
 *   Ds1 ... Dsk) that are involved into operation using built layout instance.
 *   Devices Dn, Dp1 ... Dpk are used to store K+1 replicas of index
 *   records. Devices Ds1 .. Dsk only used in degraded mode (during index
 *   repair/rebalance). Numerical order of every pool device is stored in
 *   configuration (m0_conf_sdev::sd_dev_idx).
 * - For every device involved find CAS service serving it. This relationship is
 *   also stored in configuration database.
 * - For every element in input operation vector (record for put(), key for
 *   del(), etc.) execute corresponding CAS operation against:
 *     * Random CAS service if operation doesn't modify index (get).
 *     * All CAS services if operation carries modification (put, del).
 *   Operations are executed in parallel.
 * - Wait until all operations are finished and collect results.
 *
 * NEXT operation logic:
 * - Retrieve index layout descriptor as it's done for other record operations.
 * - For every storage device in a pool find CAS service serving it.
 * - Execute in parallel CAS requests of NEXT type for all component catalogues
 *   of the index (every pool storage device stores component catalogue).
 * - Wait replies from all CAS services and then merge-sort them by record keys.
 *   Leave only requested by user number of records, throwing away excess
 *   records.
 *
 * Execution context
 * -----------------
 * Client operations are executed in the context of state machine group
 * provided at initialisation. All DIX requests have asynchronous interface. It
 * is possible to run several requests in the context of one client
 * simultaneously even if the same state machine group is used.
 *
 * All DIX requests are asynchronous and user shall wait until request
 * reaches one of DIXREQ_FINAL, DIXREQ_FAILURE states.
 */

#include "fid/fid.h"           /* m0_fid */
#include "sm/sm.h"             /* m0_sm */
#include "pool/pool_machine.h" /* m0_poolmach_versions */
#include "dix/layout.h"        /* m0_dix_layout */
#include "dix/req_internal.h"  /* m0_dix_idxop_ctx */

/* Import */
struct m0_bufvec;
struct m0_dix_ldesc;
struct m0_dtx;
struct m0_dix_meta_req;
struct m0_dix_cli;
struct m0_cas_req;

enum m0_dix_req_state {
	DIXREQ_INVALID,
	DIXREQ_INIT,
	DIXREQ_LAYOUT_DISCOVERY,
	DIXREQ_LID_DISCOVERY,
	DIXREQ_DISCOVERY_DONE,
	DIXREQ_META_UPDATE,
	DIXREQ_INPROGRESS,
	DIXREQ_GET_RESEND,
	DIXREQ_DEL_PHASE2,
	DIXREQ_FINAL,
	DIXREQ_FAILURE,
	DIXREQ_NR
};

/** Distributed index descriptor: global identifier + layout. */
struct m0_dix {
	struct m0_fid        dd_fid;
	struct m0_dix_layout dd_layout;
};

/**
 * Sorting context for merge sorting NEXT results.
 *
 * There is exactly one sorting context per CAS reply carrying records from one
 * component catalogue. All records from CAS reply are loaded to 'sc_reps'
 * before sorting algorithm starts. Sorting context has current position that is
 * advanced during sorting algorithm.
 *
 * Sorting algorithm for every starting key requested in NEXT operation
 * basically do the following:
 * - In every sorting context find first record related to this starting key and
 *   sets current position to it.
 * - Goes through all sorting contexts looking at records at current position
 *   and find the record with minimal key among them.
 * - Add found record to a result set.
 * - Advances current position in all sorting contexts, so it points to the
 *   first record with a key bigger than the found one.
 */
struct m0_dix_next_sort_ctx {
	struct m0_cas_req        *sc_creq;
	struct m0_cas_next_reply *sc_reps;
	uint32_t                  sc_reps_nr;
	bool                      sc_stop;
	bool                      sc_done;
	uint32_t                  sc_pos;
};

struct m0_dix_next_sort_ctx_arr {
	struct m0_dix_next_sort_ctx *sca_ctx;
	uint32_t                     sca_nr;
};

/**
 * Contains result records for particular starting key from NEXT request.
 *
 * This structure is filled as a result of merge sorting algorithm. There is
 * exactly one such structure for every starting key.
 */
struct m0_dix_next_results {
	struct m0_cas_next_reply  **drs_reps;
	uint32_t                    drs_nr;
	uint32_t                    drs_pos;
};

/**
 * Result set for NEXT operation.
 *
 * It's used as a context for merge sorting records retrieved from all component
 * catalogues.
 */
struct m0_dix_next_resultset {
	struct m0_dix_next_results      *nrs_res;
	uint32_t                         nrs_res_nr;
	struct m0_dix_next_sort_ctx_arr  nrs_sctx_arr;
};

enum dix_req_type {
	/** Create new index. */
	DIX_CREATE,
	/** Lookup component catalogues existence for an index. */
	DIX_CCTGS_LOOKUP,
	/** Delete an index. */
	DIX_DELETE,
	/** Given start keys, get records with the next keys from an index. */
	DIX_NEXT,
	/** Get records with the given keys from an index. */
	DIX_GET,
	/** Put given records in an index. */
	DIX_PUT,
	/** Delete records with the given keys from an index. */
	DIX_DEL
} M0_XCA_ENUM;

struct m0_dix_req {
	/** Request state machine. */
	struct m0_sm                  dr_sm;
	/** DIX client in which context request is executed. */
	struct m0_dix_cli            *dr_cli;
	/**
	 * Meta request that is used to:
	 * - Modify "layout" meta-index for DIX_CREATE, DIX_DELETE requests;
	 * - Retrieve layout from "layout" meta-index by index fid;
	 * - Retrieve layout descriptor from "layout-descr" index by layout id;
	 */
	struct m0_dix_meta_req       *dr_meta_req;
	/** Internal clink to wait for asynchronous operations completion. */
	struct m0_clink               dr_clink;
	/**
	 * Copy of distributed indices descriptors provided by user. It is used
	 * only by DIX_CREATE request to insert indices descriptors in "layout"
	 * meta-index since dr_indices may be changed during request life time.
	 */
	struct m0_dix                *dr_orig_indices;
	/**
	 * Array of indices to operate on. For index operations (DIX_CREATE,
	 * DIX_DELETE, DIX_CCTGS_LOOKUP) it is an array of indices to be
	 * created/deleted/looked up. For record operations (DIX_NEXT, DIX_GET,
	 * DIX_PUT, DIX_DEL) it's a single index.
	 */
	struct m0_dix                *dr_indices;
	/** Number of indices in dr_indices array. */
	uint32_t                      dr_indices_nr;
	/**
	 * Indicates whether request read/modify records in meta-index.
	 * It is set when request is executed against "root", "layout",
	 * "layout-descr" meta-indices.
	 */
	bool                          dr_is_meta;
	/**
	 * Array of request items contexts. For index operations the item is
	 * individual index, for record operations the item is individual
	 * record.
	 */
	struct m0_dix_item           *dr_items;
	/** Number of items in dr_items array. */
	uint64_t                      dr_items_nr;
	/**
	 * Pointer (not copy) to user supplied record keys for record operation.
	 */
	const struct m0_bufvec       *dr_keys;
	/**
	 * Pointer (not copy) to user supplied record values for PUT request.
	 */
	const struct m0_bufvec       *dr_vals;
	/** Distributed transaction, passed as is to underlying CAS requests. */
	struct m0_dtx                *dr_dtx;
	/** Index operation context, used only for index operations. */
	struct m0_dix_idxop_ctx       dr_idxop;
	/** Record operation context, used only for record operations. */
	struct m0_dix_rop_ctx        *dr_rop;
	/** AST posted to request state machine group on different events. */
	struct m0_sm_ast              dr_ast;
	/** DIX request type. */
	enum dix_req_type             dr_type;
	/** Result set for DIX_NEXT operation. */
	struct m0_dix_next_resultset  dr_rs;
	/**
	 * Array specifying how many records to retrieve for corresponding
	 * starting key in DIX_NEXT request.
	 */
	uint32_t                     *dr_recs_nr;
	/** Request flags bitmask of m0_cas_op_flags values. */
	uint32_t                      dr_flags;

	/** Datum used to update clovis SYNC records. */
	void                         *dr_sync_datum;
};

/**
 * Single value retrieved by m0_dix_get() request.
 */
struct m0_dix_get_reply {
	int           dgr_rc;
	/** Retrieved value. Undefined if dgr_rc != 0. */
	struct m0_buf dgr_val;
};

/**
 * One record retrieved by m0_dix_next() request.
 * @see m0_dix_next_rep().
 */
struct m0_dix_next_reply {
	/** Record key. */
	struct m0_buf dnr_key;
	/** Record value. */
	struct m0_buf dnr_val;
};

/** Initialises DIX request. */
M0_INTERNAL void m0_dix_req_init(struct m0_dix_req  *req,
				 struct m0_dix_cli  *cli,
				 struct m0_sm_group *grp);

/**
 * Initialises DIX request operating with meta-indices.
 * This function shall be used instead of m0_dix_req_init() if the request is
 * the record operation over the meta-index ("root", "layout" or
 * "layout-descr").
 */
M0_INTERNAL void m0_dix_mreq_init(struct m0_dix_req  *req,
				  struct m0_dix_cli  *cli,
				  struct m0_sm_group *grp);

/**
 * Locks state machine group that is used by DIX request.
 */
M0_INTERNAL void m0_dix_req_lock(struct m0_dix_req *req);

/**
 * Unlocks state machine group that is used by DIX request.
 */
M0_INTERNAL void m0_dix_req_unlock(struct m0_dix_req *req);

/**
 * Checks whether DIX request state machine group is locked.
 */
M0_INTERNAL bool m0_dix_req_is_locked(const struct m0_dix_req *req);

/**
 * Blocks until DIX request reaches one of specified states or specified timeout
 * is elapsed.
 *
 * 'states' is a bit-mask of m0_dix_req_state values built with M0_BITS() macro.
 */
M0_INTERNAL int m0_dix_req_wait(struct m0_dix_req *req, uint64_t states,
				m0_time_t to);

/**
 * Creates distributed indices.
 *
 * This request registers provided indices in "layout" meta-index and then, if
 * COF_CROW flag is not specified, creates component catalogues for indices.
 *
 * User can specify for every index to be created layout ID or full layout
 * descriptor. Layout IDs are checked for existence internally.
 *
 * If req->dr_is_meta is set, then provided indices are not registered in
 * "layout" meta-index, only component catalogues are created.
 *
 * @pre m0_forall(i, indices_nr,
 *                indices[i].dd_layout.dl_type != DIX_LTYPE_UNKNOWN)
 * @pre ergo(req->dr_is_meta, dix_id_layouts_nr(req) == 0)
 * @pre M0_IN(flags, (0, COF_CROW))
 */
M0_INTERNAL int m0_dix_create(struct m0_dix_req   *req,
			      const struct m0_dix *indices,
			      uint32_t             indices_nr,
			      struct m0_dtx       *dtx,
			      uint32_t             flags);

/**
 * Checks whether all component catalogues exist for the given indices.
 * Returns error if any component catalogue isn't accessible (e.g. disk where it
 * resides has failed) or doesn't exist. It doesn't make sense to call this
 * function for distributed indices with CROW policy, since some component
 * catalogues may be not created yet.
 */
M0_INTERNAL int m0_dix_cctgs_lookup(struct m0_dix_req   *req,
				    const struct m0_dix *indices,
				    uint32_t             indices_nr);

/**
 * Destroys distributed indices.
 *
 * For every provided index destroys all its component catalogues (along with
 * data) and unregisters it from "layout" index.
 *
 * If indices were created with COF_CROW, then it is user responsibility to set
 * COF_CROW for delete operation also. Otherwise, execution errors are possible.
 *
 * @pre M0_IN(flags, (0, COF_CROW))
 */
M0_INTERNAL int m0_dix_delete(struct m0_dix_req   *req,
			      const struct m0_dix *indices,
			      uint64_t             indices_nr,
			      struct m0_dtx       *dtx,
			      uint32_t             flags);

/**
 * Inserts records to a distributed index.
 *
 * 'Keys' and 'vals' buffer vectors are managed by user and shall be accessible
 * until the request completion.
 *
 * @pre keys->ov_vec.v_nr > 0
 * @pre keys->ov_vec.v_nr == vals->ov_vec.v_nr
 * @pre flags & ~(COF_OVERWRITE | COF_CROW | COF_SYNC_WAIT)) == 0
 */
M0_INTERNAL int m0_dix_put(struct m0_dix_req      *req,
			   const struct m0_dix    *index,
			   const struct m0_bufvec *keys,
			   const struct m0_bufvec *vals,
			   struct m0_dtx          *dtx,
			   uint32_t                flags);

/**
 * Gets values for provided keys from distributed index.
 *
 * 'Keys' buffer vector is managed by user and shall be accessible until the
 * request completion.
 *
 * Retrieved values on request completion can be retrieved via m0_dix_get_rep().
 *
 * @pre keys->ov_vec.v_nr > 0
 */
M0_INTERNAL int m0_dix_get(struct m0_dix_req      *req,
			   const struct m0_dix    *index,
			   const struct m0_bufvec *keys);

/**
 * Gets record value for i-th key retrieved by m0_dix_get().
 *
 * Function fills rep->dgr_val. Value data buffer is deallocated in
 * m0_dix_req_fini(), unless m0_dix_get_rep_mlock() is called by user.
 *
 * @pre m0_dix_generic_rc(req) == 0
 */
M0_INTERNAL void m0_dix_get_rep(const struct m0_dix_req *req,
				uint64_t                 idx,
				struct m0_dix_get_reply *rep);

/**
 * Deletes records from distributed index.
 *
 *'Keys' buffer vector is managed by user and shall be accessible until the
 * request completion.
 *
 * @pre keys->ov_vec.v_nr > 0
 * @pre (flags & ~(COF_SYNC_WAIT)) == 0
 */
M0_INTERNAL int m0_dix_del(struct m0_dix_req      *req,
			   const struct m0_dix    *index,
			   const struct m0_bufvec *keys,
			   struct m0_dtx          *dtx,
			   uint32_t                flags);

/**
 * Gets next 'recs_nr[i]' records for each i-th key in 'start_keys'.
 *
 * Size of recs_nr array should be >= start_keys->ov_vec.v_nr.
 *
 * Requested record ranges may overlap. Duplicates in a result are not filtered.
 *
 * Records with 'start_keys' keys are also accounted and included in the result
 * unless COF_EXCLUDE_START_KEY flag is specified.
 *
 * In order to start iteration from the first record user may specify 1-byte
 * zero start key and specify COF_SLANT flag. COF_SLANT flag is also useful if
 * start key may be not found in the index. In this case iteration starts with
 * the smallest key following the start key.
 *
 * 'Flags' argument is a bitmask of m0_cas_op_flags values.
 *
 * @pre keys_nr != 0
 * @pre (flags & ~(COF_SLANT | COF_EXCLUDE_START_KEY)) == 0
 */
M0_INTERNAL int m0_dix_next(struct m0_dix_req      *req,
			    const struct m0_dix    *index,
			    const struct m0_bufvec *start_keys,
			    const uint32_t         *recs_nr,
			    uint32_t                flags);

/**
 * Gets 'val_idx'-th value retrieved for 'key_idx'-th key as a result of
 * m0_dix_next() request.
 *
 * Function doesn't copy key/value data buffers, only assign pointers to
 * received buffers. They are deallocated in m0_dix_req_fini(), unless
 * m0_dix_next_rep_mlock() is called by user.
 *
 * @pre m0_dix_item_rc(req, key_idx) == 0
 */
M0_INTERNAL void m0_dix_next_rep(const struct m0_dix_req  *req,
				 uint64_t                  key_idx,
				 uint64_t                  val_idx,
				 struct m0_dix_next_reply *rep);

/**
 * Returns number of values retrieved for 'key_idx'-th key.
 *
 * If number of values is less than requested, then end of index is reached.
 *
 * @pre m0_dix_item_rc(req, key_idx) == 0
 */
M0_INTERNAL uint32_t m0_dix_next_rep_nr(const struct m0_dix_req *req,
					uint64_t                 key_idx);

/**
 * Prevents deallocation of key/value buffers during request finalisation.
 *
 * Applicable only for NEXT request. For 'key_idx'-th start key and 'val_idx'-th
 * record value retrieved for it key/value buffers are locked in memory and user
 * is responsible to deallocate them.
 *
 * @see m0_dix_next_rep()
 */
M0_INTERNAL void m0_dix_next_rep_mlock(struct m0_dix_req *req,
				       uint32_t           key_idx,
				       uint32_t           val_idx);

/**
 * Returns generic return code for the operation.
 *
 * If the generic return code is negative, then the whole request has failed.
 * Otherwise, the user should check return codes for the individual items in
 * operation vector via m0_dix_item_rc().
 *
 * @pre M0_IN(req->dr_sm.sm_state, (DIXREQ_FINAL, DIXREQ_FAILURE))
 */
M0_INTERNAL int m0_dix_generic_rc(const struct m0_dix_req *req);

/**
 * Returns execution result for the 'idx'-th item in the input vector.
 *
 * @pre m0_dix_generic_rc(req) == 0
 * @pre idx < m0_dix_req_nr(req)
 */
M0_INTERNAL int m0_dix_item_rc(const struct m0_dix_req *req,
			       uint64_t                 idx);

/**
 * Returns the return value of results.
 */
M0_INTERNAL int m0_dix_req_rc(const struct m0_dix_req *req);

/**
 * Returns the number of results.
 *
 * It's guaranteed that if m0_dix_req_generic_rc(req) == 0, then m0_dix_req_nr()
 * equals to the number of requested items (indices to create/lookup/delete,
 * records to insert/get/delete, etc.).
 */
M0_INTERNAL uint64_t m0_dix_req_nr(const struct m0_dix_req *req);

/**
 * Prevents deallocation of key/value buffers during request finalisation.
 *
 * Applicable only for GET request. Retrieved 'idx'-th record value buffer is
 * locked in memory and user is responsible to deallocate it.
 *
 * @see m0_dix_get_rep()
 */
M0_INTERNAL void m0_dix_get_rep_mlock(struct m0_dix_req *req, uint64_t idx);

/**
 * Finalises DIX request.
 *
 * @pre M0_IN(req->dr_sm.sm_state, (DIXREQ_FINAL, DIXREQ_FAILURE))
 * @pre m0_dix_req_is_locked()
 */
M0_INTERNAL void m0_dix_req_fini(struct m0_dix_req *req);

/**
 * The same as m0_dix_req_fini(), but takes SM group lock internally.
 *
 * @pre M0_IN(req->dr_sm.sm_state, (DIXREQ_FINAL, DIXREQ_FAILURE))
 * @pre !m0_dix_req_is_locked()
 */
M0_INTERNAL void m0_dix_req_fini_lock(struct m0_dix_req *req);

/**
 * Sets copy of layout descriptor 'desc' as 'dix' layout.
 */
M0_INTERNAL int m0_dix_desc_set(struct m0_dix             *dix,
				const struct m0_dix_ldesc *desc);

/**
 * Copy distributed index descriptor 'src' to 'dst'.
 */
M0_INTERNAL int m0_dix_copy(struct m0_dix *dst, const struct m0_dix *src);

/**
 * Finalises distributed index descriptor.
 *
 * Deallocates memory allocated for 'dix' fields, but not for 'dix' itself.
 */
M0_INTERNAL void m0_dix_fini(struct m0_dix *dix);

M0_INTERNAL int  m0_dix_sm_conf_init(void);
M0_INTERNAL void m0_dix_sm_conf_fini(void);

/** @} end of dix group */

#endif /* __MERO_DIX_REQ_H__ */

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
