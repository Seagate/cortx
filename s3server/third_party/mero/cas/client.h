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
 * Original creation date: 08-Apr-2016
 */

#pragma once

#ifndef __MERO_CAS_CLIENT_H__
#define __MERO_CAS_CLIENT_H__

#include "lib/time.h" /* m0_time_t */
#include "lib/chan.h" /* m0_chan */
#include "fid/fid.h"  /* m0_fid */
#include "fop/fop.h"  /* m0_fop */
#include "cas/cas.h"  /* m0_cas_hint, m0_cas_id */

/* Imports */
struct m0_dtx;
struct m0_bufvec;
struct m0_rpc_session;

/**
 * @defgroup cas-client
 *
 * @{
 *
 * CAS client provides an interface to make queries to CAS service.
 * It's a thin layer that hides network communication details with CAS
 * service. CAS client doesn't provide distribution of requests over remote
 * CAS services, user explicitly specify destination of every request.
 *
 * Request to CAS service instance is represented by m0_cas_req structure.
 * General workflow with the request is:
 * - m0_cas_req_init(req, cas_rpc_session, sm_grp);
 * - Add clink to m0_cas_req::ccr_chan channel.
 * - Send specific request over RPC session.
 *   For example to request value by a single key:
 *   m0_cas_get(req, index, key);
 * - Wait asynchronously or synchronously until req->ccr_sm reaches
 *   CASREQ_FINAL or CASREQ_FAILURE state;
 * - Check return code of operation with m0_cas_req_generic_rc(req);
 * - If return code is 0, then obtain request-specific reply.
 *   M0_ASSERT(m0_cas_req_nr(req) == 1);
 *   m0_cas_get_rep(req, 0, rep);
 * - m0_cas_req_fini(req);
 *
 * Available requests:
 * - m0_cas_index_create()
 * - m0_cas_index_delete()
 * - m0_cas_index_lookup()
 * - m0_cas_index_list()
 * - m0_cas_put()
 * - m0_cas_get()
 * - m0_cas_next()
 * - m0_cas_del()
 *
 * If one of the functions above returns non-zero return code, then further
 * request processing is impossible and the request should be finalised using
 * m0_cas_req_fini(). All requests are vectorised. CAS client doesn't copy
 * values of vector items for index records, so they should be accessible until
 * the request is processed. It is not needed for requests made for operations
 * over indices.
 *
 * For every request there is corresponding function to obtain the result of
 * request execution (m0_cas_*_rep() family). After the request is processed,
 * return code can be obtained for operation against every input vector's item.
 * If m0_cas_req_generic_rc() returns 0 for the request, then number and order
 * of items obtained via m0_cas_*_rep() is guaranteed to be the same as in
 * original request. The exceptions are m0_cas_index_list_rep() and
 * m0_cas_next_rep() functions, which can return less records than
 * requested.
 *
 * Indices are uniquely identified by their FIDs, which are m0_fid values with
 * m0_cas_index_fid_type as a FID type. Note, that FID (0,0) is reserved by
 * implementation and shouldn't be used in requests (see m0_cas_meta_fid).
 *
 * In order to meet atomicity and durability requirements, modification requests
 * should be executed as part of a distributed transaction.
 *
 * Request has associated state machine executed in a state machine group
 * provided during initialisation. All requests should be invoked with state
 * machine group mutex held. There are helpers to lock/unlock this state machine
 * group (m0_cas_lock(), m0_cas_unlock()). All request state transitions are
 * done in the context of this state machine group.
 *
 * There is no restriction on the size of keys and values.
 *
 * @see m0_chan, m0_dtx
 */

/** Possible CAS request states. */
enum m0_cas_req_state {
	CASREQ_INVALID,
	CASREQ_INIT,
	CASREQ_SENT,
	CASREQ_FRAGM_SENT,
	CASREQ_ASSEMBLY,
	CASREQ_FINAL,
	CASREQ_FAILURE,
	CASREQ_NR
};

/**
 * Representation of a request to remote CAS service.
 */
struct m0_cas_req {
	/** CAS request state machine. */
	struct m0_sm            ccr_sm;

	/* Private fields. */

	/** RPC session with remote CAS service. */
	struct m0_rpc_session  *ccr_sess;
	/** FOP carrying CAS request. */
	struct m0_fop          *ccr_fop;
	/**
	 * FOP to assemble key/values that weren't fit
	 * in the first server reply.
	 */
	struct m0_fop           ccr_asmbl_fop;
	/** Maximum number of reply records. */
	uint64_t                ccr_max_replies_nr;
	/** Pointer to currently executing CAS operation. */
	struct m0_cas_op       *ccr_req_op;
	/** Outgoing fop type. */
	struct m0_fop_type     *ccr_ftype;
	/** Shows whether operation is done over catalogues. */
	bool                    ccr_is_meta;
	/** Final reply. */
	struct m0_cas_rep       ccr_reply;
	/** Original vector of records. */
	struct m0_cas_recv      ccr_rec_orig;
	/** Number of currently sent records. */
	uint64_t                ccr_sent_recs_nr;
	/** RPC reply item. */
	struct m0_rpc_item     *ccr_reply_item;
	/** AST to post RPC-related events. */
	struct m0_sm_ast        ccr_replied_ast;
	/** AST to move sm to failure state. */
	struct m0_sm_ast        ccr_failure_ast;
	/** Requested keys. */
	const struct m0_bufvec *ccr_keys;
	/**
	 * Key indices from original request that are present in assemble
	 * request. It's used only to assemble "GET" request.
	 */
	uint64_t               *ccr_asmbl_ikeys;
	/* Returned tx REMID inforation from service to update FSYNC records. */
	struct m0_be_tx_remid  ccr_remid;
};

/**
 * Reply from CAS service for individual requested record.
 * Format of requested record depends on the request. For example, in case
 * of m0_cas_index_create() it is individual index to create.
 */
struct m0_cas_rec_reply {
	/** Return code of requested record execution. */
	int                crr_rc;
	/**
	 * Hint to speed up access to the indices in case of
	 * m0_cas_index_*() or to the records in case of
	 * m0_cas_records_*() functions.
	 *
	 * May be specified in further CAS client requests.
	 * @note: Not used for now.
	 */
	struct m0_cas_hint crr_hint;
};

/**
 * Information about single index retrieved as a result of
 * m0_cas_index_list() request.
 */
struct m0_cas_ilist_reply {
	/** Return code. -ENOENT means that end of meta-index is reached. */
	int                clr_rc;
	/** Index FID. It's guaranteed to be set if rc == 0. */
	struct m0_fid      clr_fid;
	/** Index hint. Not used for now. */
	struct m0_cas_hint clr_hint;
};

/**
 * Single value retrieved by m0_cas_get() request.
 */
struct m0_cas_get_reply {
	int           cge_rc;
	/** Retrieved value. Undefined if cge_rc != 0. */
	struct m0_buf cge_val;
};

/**
 * Single record retrieved by m0_cas_next() request.
 */
struct m0_cas_next_reply {
	/** Return code. -ENOENT means that end of index is reached. */
	int                cnp_rc;
	/** Record key. Set if rc == 0. */
	struct m0_buf      cnp_key;
	/** Record hint. Not used for now. */
	struct m0_cas_hint cnp_hint;
	/** Record value. Set if rc == 0. */
	struct m0_buf      cnp_val;
};

/**
 * Initialises CAS client request.
 *
 * Initialisation should be done always before sending specific request.
 * RPC session 'sess' to CAS service should be already established.
 *
 * @pre M0_IS0(req)
 */
M0_INTERNAL void m0_cas_req_init(struct m0_cas_req     *req,
				 struct m0_rpc_session *sess,
				 struct m0_sm_group    *grp);

/**
 * Finalises CAS client request.
 *
 * It's not allowed to call it when request is in progress.
 * Once one of request functions is called, user should wait until SM reaches
 * CASREQ_FINAL or CASREQ_FAILURE state. There is no way to cancel current
 * operation.
 *
 * @pre M0_IN(req->ccr_sm.sm_state,
 *	      (CASREQ_INIT, CASREQ_FINAL, CASREQ_FAILURE))
 * @pre m0_cas_req_is_locked(req)
 * @post M0_IS0(req)
 */
M0_INTERNAL void m0_cas_req_fini(struct m0_cas_req *req);

/**
 * The same as m0_cas_req_fini(), but takes CAS request lock internally.
 *
 * @pre M0_IN(req->ccr_sm.sm_state,
 *	      (CASREQ_INIT, CASREQ_FINAL, CASREQ_FAILURE))
 * @post M0_IS0(req)
 */
M0_INTERNAL void m0_cas_req_fini_lock(struct m0_cas_req *req);

/**
 * Locks state machine group that is used by CAS request.
 */
M0_INTERNAL void m0_cas_req_lock(struct m0_cas_req *req);

/**
 * Unlocks state machine group that is used by CAS request.
 */
M0_INTERNAL void m0_cas_req_unlock(struct m0_cas_req *req);

/**
 * Checks whether CAS request state machine group is locked.
 */
M0_INTERNAL bool m0_cas_req_is_locked(const struct m0_cas_req *req);

/**
 * Gets request execution return code.
 *
 * Should be called only after request processing is finished, i.e.
 * CAS request is in CASREQ_FINAL or CASREQ_FAILURE state.
 *
 * This return code is generic in sense that it doesn't take into account
 * return codes of operations on individual request records. These codes
 * can be obtained via m0_cas_*_rep() functions.
 */
M0_INTERNAL int m0_cas_req_generic_rc(const struct m0_cas_req *req);

/**
 * Returns the number of results.
 *
 * For the following requests it's guaranteed that if
 * m0_cas_req_generic_rc(req) == 0, then m0_cas_req_nr() equals to the number of
 * requested items:
 * - m0_cas_index_create()
 * - m0_cas_index_lookup()
 * - m0_cas_index_delete()
 * - m0_cas_put()
 * - m0_cas_get()
 * - m0_cas_del()
 */
M0_INTERNAL uint64_t m0_cas_req_nr(const struct m0_cas_req *req);

/**
 * Synchronously waits until CAS request reaches a desired state.
 *
 * The 'states' argument is a bitmask based on m0_cas_req_state.
 * M0_BITS() macro should be used to build a bitmask.
 *
 * @pre m0_cas_req_is_locked(req)
 * @param to absolute timeout to wait.
 */
M0_INTERNAL int m0_cas_req_wait(struct m0_cas_req *req, uint64_t states,
				m0_time_t to);

/**
 * Creates new indices.
 *
 * It is not needed to keep CAS ids array accessible until request is processed.
 *
 * @pre m0_cas_req_is_locked(req)
 * @see m0_cas_index_create_rep()
 */
M0_INTERNAL int m0_cas_index_create(struct m0_cas_req      *req,
				    const struct m0_cas_id *cids,
				    uint64_t                cids_nr,
				    struct m0_dtx          *dtx);

/**
 * Gets execution result of m0_cas_index_create() request.
 *
 * @pre idx < m0_cas_req_nr(req)
 */
M0_INTERNAL void m0_cas_index_create_rep(const struct m0_cas_req *req,
					 uint64_t                 idx,
					 struct m0_cas_rec_reply *rep);

/**
 * Delete indices with all records they contain.
 *
 * It is not needed to keep CAS ids array accessible until request is processed.
 * Flag @ref m0_cas_op_flags::COF_CROW can be set in flags to be a no-op if the
 * catalogue to be deleted does not exist.
 *
 * @pre m0_cas_req_is_locked(req)
 * @pre (flags & ~(COF_CROW | COF_DEL_LOCK)) == 0
 * @see m0_cas_index_delete_rep()
 */
M0_INTERNAL int m0_cas_index_delete(struct m0_cas_req      *req,
				    const struct m0_cas_id *cids,
				    uint64_t                cids_nr,
				    struct m0_dtx          *dtx,
				    uint32_t                flags);

/**
 * Gets execution result of m0_cas_index_delete() request.
 *
 * @pre idx < m0_cas_req_nr(req)
 */
M0_INTERNAL void m0_cas_index_delete_rep(const struct m0_cas_req *req,
					 uint64_t                 idx,
					 struct m0_cas_rec_reply *rep);

/**
 * Checks whether indices with given identifiers exist.
 *
 * It is not needed to keep CAS ids array accessible until request is processed.
 *
 * @pre m0_cas_req_is_locked(req)
 * @see m0_cas_index_lookup_rep()
 */
M0_INTERNAL int m0_cas_index_lookup(struct m0_cas_req      *req,
				    const struct m0_cas_id *cids,
				    uint64_t                cids_nr);

/**
 * Gets execution result of m0_cas_index_lookup() request.
 *
 * Return code meaning in m0_cas_rec_reply:
 *   0 - index exists;
 *   -ENOENT - index doesn't exist;
 *   other - some error during request processing.
 *
 * @pre idx < m0_cas_req_nr(req)
 */
M0_INTERNAL void m0_cas_index_lookup_rep(const struct m0_cas_req *req,
					 uint64_t                 idx,
					 struct m0_cas_rec_reply *rep);

/**
 * Gets identifiers of the next 'indices_nr' indices starting with index having
 * 'start_fid' identifier.
 *
 * Zero FID (0,0) (see m0_cas_meta_fid) can be used as a 'start_fid' in order
 * to start iteration from the first index. Note, that zero FID is also included
 * in the result.
 *
 * @pre m0_cas_req_is_locked(req)
 * @pre start_fid != NULL
 * @see m0_cas_index_list_rep()
 */
M0_INTERNAL int m0_cas_index_list(struct m0_cas_req   *req,
				  const struct m0_fid *start_fid,
				  uint32_t             indices_nr,
				  uint32_t             flags);
/**
 * Gets execution result of m0_cas_index_list() request.
 *
 * The fact that there are no more indices to return on CAS service side (end of
 * list is reached) is denoted by special reply record with -ENOENT return code.
 *
 * @pre idx < m0_cas_req_nr(req)
 */
M0_INTERNAL void m0_cas_index_list_rep(struct m0_cas_req         *req,
				       uint32_t                   idx,
				       struct m0_cas_ilist_reply *rep);

/**
 * Inserts records to the index.
 *
 * Keys and values buffers (m0_bufvec::ov_vec[i]) should be accessible until
 * request is processed. Also, it's user responsibility to manage these buffers
 * after request is processed.
 *
 * 'Flags' argument is a bitmask of m0_cas_op_flags values. COF_CREATE and
 * COF_OVERWRITE flags can't be specified together.
 *
 * @pre !(flags & COF_CREATE) || !(flags & COF_OVERWRITE)
 * @pre (flags & ~(COF_CREATE | COF_OVERWRITE | COF_CROW)) == 0
 * @pre m0_cas_req_is_locked(req)
 * @see m0_cas_put_rep()
 */
M0_INTERNAL int m0_cas_put(struct m0_cas_req      *req,
			   struct m0_cas_id       *index,
			   const struct m0_bufvec *keys,
			   const struct m0_bufvec *values,
			   struct m0_dtx          *dtx,
			   uint32_t                flags);

/**
 * Gets execution result of m0_cas_put() request.
 *
 * @pre idx < m0_cas_req_nr(req)
 */
M0_INTERNAL void m0_cas_put_rep(struct m0_cas_req       *req,
				uint64_t                 idx,
				struct m0_cas_rec_reply *rep);

/**
 * Prevents deallocation of key/value buffers during request finalisation.
 *
 * Applicable only for GET, NEXT requests.
 * In case of GET request m0_cas_get_reply::cge_val::b_addr pointer, obtained by
 * corresponding 'idx' will be locked in memory and user is responsible to
 * deallocate it afterwards.
 * In case of NEXT request both key and value buffers in m0_cas_next_reply
 * buffer are locked and user is responsible to deallocate them.
 *
 * @pre idx < m0_cas_req_nr(req)
 * @see m0_cas_get_rep(), m0_cas_next_rep()
 */
M0_INTERNAL void m0_cas_rep_mlock(const struct m0_cas_req *req,
				  uint64_t                 idx);

/**
 * Gets values for provided keys.
 *
 * Keys buffers (m0_bufvec::ov_vec[i]) should be accessible until request is
 * processed.
 *
 * @pre m0_cas_req_is_locked(req)
 * @see m0_cas_get_rep()
 */
M0_INTERNAL int m0_cas_get(struct m0_cas_req      *req,
			   struct m0_cas_id       *index,
			   const struct m0_bufvec *keys);

/**
 * Gets execution result of m0_cas_get() request.
 *
 * Function fills rep->cge_val. Value data buffer is deallocated in
 * m0_cas_req_fini(), unless m0_cas_rep_mlock() is called by user.
 *
 * @pre idx < m0_cas_req_nr(req)
 */
M0_INTERNAL void m0_cas_get_rep(const struct m0_cas_req *req,
				uint64_t                 idx,
				struct m0_cas_get_reply *rep);

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
 * @pre start_keys.ov_vec.v_nr > 0
 * @pre m0_forall(i, start_keys.ov_vec.v_nr, start_keys.ov_buf[i] != NULL)
 * @pre	M0_PRE((flags & ~(COF_SLANT | COF_EXCLUDE_START_KEY)) == 0)
 * @pre m0_cas_req_is_locked(req)
 * @see m0_cas_next_rep()
 */
M0_INTERNAL int m0_cas_next(struct m0_cas_req *req,
			    struct m0_cas_id  *index,
			    struct m0_bufvec  *start_keys,
			    uint32_t          *recs_nr,
			    uint32_t           flags);

/**
 * Gets execution result of m0_cas_next() request.
 *
 * The fact that there are no more records to return on CAS service side (end of
 * index is reached) is denoted by special reply record with -ENOENT return
 * code.
 *
 * Function doesn't copy key/value data buffers, only assign pointers to
 * received buffers. They are deallocated in m0_cas_req_fini(), unless
 * m0_cas_rep_mlock() is called by user.
 *
 * @pre idx < m0_cas_req_nr(req)
 */
M0_INTERNAL void m0_cas_next_rep(const struct m0_cas_req  *req,
				 uint32_t                  idx,
				 struct m0_cas_next_reply *rep);

/**
 * Deletes records with given keys from the index.
 *
 * Keys buffers (m0_bufvec::ov_vec[i]) should be accessible until request is
 * processed.
 *
 * 'Flags' argument is a bitmask of m0_cas_op_flags values. COF_DEL_LOCK is the
 * only possible flag for now.
 *
 * @pre m0_cas_req_is_locked(req)
 * @pre M0_IN(flags, (0, COF_DEL_LOCK))
 * @see m0_cas_del_rep()
 */
M0_INTERNAL int m0_cas_del(struct m0_cas_req *req,
			   struct m0_cas_id  *index,
			   struct m0_bufvec  *keys,
			   struct m0_dtx     *dtx,
			   uint32_t           flags);

/**
 * Gets execution result of m0_cas_del() request.
 *
 * @pre idx < m0_cas_req_nr(req)
 */
M0_INTERNAL void m0_cas_del_rep(struct m0_cas_req       *req,
				uint64_t                 idx,
				struct m0_cas_rec_reply *rep);

M0_INTERNAL int  m0_cas_sm_conf_init(void);
M0_INTERNAL void m0_cas_sm_conf_fini(void);

/** @} end of cas-client group */
#endif /* __MERO_CAS_CLIENT_H__ */

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
