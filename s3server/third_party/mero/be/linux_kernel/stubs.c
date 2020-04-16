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
 * Original author: Trupti Patil <trupti_patil@xyratex.com>
 * Original creation date: 13-Sep-2013
 */

#include "lib/errno.h"       /* EINVAL */
#include "lib/memory.h"      /* m0_alloc() */
#include "be/domain.h"       /* m0_be_domain_cfg */
#include "be/tx.h"           /* m0_be_tx_state */
#include "be/seg_internal.h" /* m0_be_seg_hdr */

#include "be/ut/helper.h" /* m0_be_ut_backend, m0_be_ut_seg */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

struct m0_be_btree;
struct m0_be_btree_kv_ops;
struct m0_be_btree_cursor;

/* fake segment header. */
static struct m0_be_seg_hdr khdr;

M0_INTERNAL void m0_be_alloc(struct m0_be_allocator *a,
			     struct m0_be_tx *tx,
			     struct m0_be_op *op,
			     void **ptr,
			     m0_bcount_t size)
{
	*ptr = m0_alloc(size);
}

M0_INTERNAL void m0_be_alloc_aligned(struct m0_be_allocator *a,
				     struct m0_be_tx *tx,
				     struct m0_be_op *op,
				     void **ptr,
				     m0_bcount_t size,
				     unsigned shift,
				     uint64_t zonemask)
{
	m0_be_alloc(a, tx, op, ptr, size);
}

M0_INTERNAL void m0_be_allocator_credit(struct m0_be_allocator *a,
					enum m0_be_allocator_op optype,
					m0_bcount_t size,
					unsigned shift,
					struct m0_be_tx_credit *accum)
{
}

M0_INTERNAL void m0_be_free(struct m0_be_allocator *a,
			    struct m0_be_tx *tx,
			    struct m0_be_op *op,
			    void *ptr)
{
	m0_free(ptr);
}

M0_INTERNAL void m0_be_free_aligned(struct m0_be_allocator *a,
				    struct m0_be_tx *tx,
				    struct m0_be_op *op,
				    void *ptr)
{
	m0_be_free(a, tx, op, ptr);
}

M0_INTERNAL void m0_be_op_init(struct m0_be_op *op)
{
	op->bo_sm.sm_state = M0_BOS_INIT;
}

M0_INTERNAL void m0_be_op_fini(struct m0_be_op *op)
{
}

M0_INTERNAL void m0_be_op_wait(struct m0_be_op *op)
{
}

M0_INTERNAL void m0_be_op_active(struct m0_be_op *op)
{
}

M0_INTERNAL void m0_be_op_done(struct m0_be_op *op)
{
}

M0_INTERNAL bool m0_be_op_is_done(struct m0_be_op *op)
{
	return true;
}

M0_INTERNAL struct m0_be_allocator *m0_be_seg_allocator(struct m0_be_seg *seg)
{
	return NULL;
}

M0_INTERNAL void m0_be_seg_init(struct m0_be_seg *seg,
				struct m0_stob *stob,
				struct m0_be_domain *dom,
				uint64_t seg_id)
{
	seg->bs_stob   = stob;
	seg->bs_domain = dom;
	seg->bs_id     = seg_id;
}

M0_INTERNAL void m0_be_seg_fini(struct m0_be_seg *seg)
{
}

M0_INTERNAL int m0_be_seg_open(struct m0_be_seg *seg)
{
	seg->bs_size = sizeof khdr;
	seg->bs_addr = &khdr;
	seg->bs_state = M0_BSS_OPENED;
	return 0;
}

M0_INTERNAL void m0_be_seg_close(struct m0_be_seg *seg)
{
}

M0_INTERNAL bool m0_be_seg_contains(const struct m0_be_seg *seg,
				    const void *addr)
{
	return true;
}

M0_INTERNAL void m0_be_tx_init(struct m0_be_tx     *tx,
			       uint64_t             tid,
			       struct m0_be_domain *dom,
			       struct m0_sm_group  *sm_group,
			       m0_be_tx_cb_t        persistent,
			       m0_be_tx_cb_t        discarded,
			       void               (*filler)(struct m0_be_tx *tx,
							    void *payload),
			       void                *datum)
{
	tx->t_sm.sm_state = M0_BTS_PREPARE;
}

M0_INTERNAL void m0_be_tx_fini(struct m0_be_tx *tx)
{
}

M0_INTERNAL void m0_be_tx_put(struct m0_be_tx *tx)
{
}

M0_INTERNAL void m0_be_tx_prep(struct m0_be_tx *tx,
			       const struct m0_be_tx_credit *credit)
{
}

M0_INTERNAL void m0_be_tx_payload_prep(struct m0_be_tx *tx, m0_bcount_t size)
{
}

M0_INTERNAL void m0_be_tx_open(struct m0_be_tx *tx)
{
	tx->t_sm.sm_state = M0_BTS_ACTIVE;
}

M0_INTERNAL void
m0_be_tx_capture(struct m0_be_tx *tx, const struct m0_be_reg *reg)
{
}

M0_INTERNAL void m0_be_tx_close(struct m0_be_tx *tx)
{
	tx->t_sm.sm_state = M0_BTS_DONE;
}

M0_INTERNAL int m0_be_tx_open_sync(struct m0_be_tx *tx)
{
	m0_be_tx_open(tx);
	return 0;
}

M0_INTERNAL void m0_be_tx_close_sync(struct m0_be_tx *tx)
{
	m0_be_tx_close(tx);
}

M0_INTERNAL enum m0_be_tx_state m0_be_tx_state(const struct m0_be_tx *tx)
{
	return tx->t_sm.sm_state;
}

M0_INTERNAL int m0_be_tx_timedwait(struct m0_be_tx *tx, uint64_t states,
				   m0_time_t deadline)
{
	return 0;
}

M0_INTERNAL bool m0_be_seg__invariant(const struct m0_be_seg *seg)
{
	return true;
}

/* UT stubs */
void m0_be_ut_backend_cfg_default(struct m0_be_domain_cfg *cfg)
{
}

M0_INTERNAL int m0_be_ut_backend_init_cfg(struct m0_be_ut_backend *ut_be,
					  const struct m0_be_domain_cfg *cfg,
					  bool mkfs)
{
	return 0;
}

void m0_be_ut_backend_init(struct m0_be_ut_backend *ut_be)
{
}

void m0_be_ut_backend_fini(struct m0_be_ut_backend *ut_be)
{
}

void m0_be_ut_seg_allocator_init(struct m0_be_ut_seg *ut_seg,
				 struct m0_be_ut_backend *ut_be)
{
}

void m0_be_ut_seg_allocator_fini(struct m0_be_ut_seg *ut_seg,
				 struct m0_be_ut_backend *ut_be)
{
}

void m0_be_ut_seg_init(struct m0_be_ut_seg *ut_seg,
		       struct m0_be_ut_backend *ut_be,
		       m0_bcount_t size)
{
	M0_ALLOC_PTR(ut_seg->bus_seg);
	M0_ASSERT(ut_seg->bus_seg != NULL);
	m0_be_seg_init(ut_seg->bus_seg, NULL, &ut_be->but_dom,
		       M0_BE_SEG_FAKE_ID);
	m0_be_seg_open(ut_seg->bus_seg);
}

void m0_be_ut_seg_fini(struct m0_be_ut_seg *ut_seg)
{
	m0_free(ut_seg->bus_seg);
}

void m0_be_ut_seg_reload(struct m0_be_ut_seg *ut_seg)
{
}

struct m0_sm_group *
m0_be_ut_backend_sm_group_lookup(struct m0_be_ut_backend *ut_be)
{
	return NULL;
}

void m0_be_ut_backend_thread_exit(struct m0_be_ut_backend *ut_be)
{
}

void m0_be_ut_tx_init(struct m0_be_tx *tx, struct m0_be_ut_backend *ut_be)
{
}

M0_INTERNAL struct m0_be_seg *m0_be_domain_seg0_get(struct m0_be_domain *dom)
{
	return NULL;
}

int m0_be_tx_fol_add(struct m0_be_tx *tx, struct m0_fol_rec *rec)
{
	return M0_ERR(-EINVAL);
}

M0_INTERNAL void m0_be_ut_alloc(struct m0_be_ut_backend *ut_be,
				struct m0_be_ut_seg *ut_seg,
				void **ptr,
				m0_bcount_t size)
{
	m0_be_alloc(NULL, NULL, NULL, ptr, size);
}

M0_INTERNAL void m0_be_ut_free(struct m0_be_ut_backend *ut_be,
			       struct m0_be_ut_seg *ut_seg,
			       void *ptr)
{
	m0_be_free(NULL, NULL, NULL, ptr);
}

#undef M0_TRACE_SUBSYSTEM
