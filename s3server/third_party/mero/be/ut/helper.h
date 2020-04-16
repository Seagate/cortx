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
 * Original author: Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 5-Jun-2013
 */

#pragma once
#ifndef __MERO_BE_UT_HELPER_H__
#define __MERO_BE_UT_HELPER_H__

#include "lib/types.h"  /* bool */
#include "lib/buf.h"    /* m0_buf */
#include "lib/memory.h" /* M0_ALLOC_PTR */
#include "sm/sm.h"      /* m0_sm */

#include "be/domain.h"  /* m0_be_domain */
#include "be/seg.h"     /* m0_be_seg */
#include "be/seg0.h"    /* m0_be_0type */
#include "lib/bob.h"    /* M0_BOB_DEFINE */

enum {
	BE_UT_SEG_START_ADDR = 0x400000000000ULL,
	BE_UT_SEG_START_ID   = 42,
	BE_UT_LOG_ID         = BE_UT_SEG_START_ID - 2,
};

struct m0_be_ut_sm_group_thread;
struct m0_stob;

struct m0_be_ut_backend {
	struct m0_be_domain		  but_dom;
	/* XXX DELETEME
	 * Make sure that ->but_dom_cfg is not needed for m0_be_domain
	 * initialisation and delete it.
	 * See https://seagate.slack.com/archives/mero/p1424896669000401
	 */
	struct m0_be_domain_cfg		  but_dom_cfg;
	struct m0_be_ut_sm_group_thread **but_sgt;
	size_t				  but_sgt_size;
	struct m0_mutex			  but_sgt_lock;
	bool				  but_sm_groups_unlocked;
	char				 *but_stob_domain_location;
	uint64_t                          but_magix;
};

extern const struct m0_bob_type m0_ut_be_backend_bobtype;
M0_BOB_DECLARE(M0_INTERNAL, m0_be_ut_backend);

/*
 * Fill cfg with default configuration.
 * @note bec_reqh is not set here
 */
void m0_be_ut_backend_cfg_default(struct m0_be_domain_cfg *cfg);

void m0_be_ut_backend_init(struct m0_be_ut_backend *ut_be);
void m0_be_ut_backend_fini(struct m0_be_ut_backend *ut_be);

M0_INTERNAL int m0_be_ut_backend_init_cfg(struct m0_be_ut_backend *ut_be,
					  const struct m0_be_domain_cfg *cfg,
					  bool mkfs);

M0_INTERNAL void
m0_be_ut_backend_seg_add2(struct m0_be_ut_backend	   *ut_be,
			  m0_bcount_t			    size,
			  bool				    preallocate,
			  const char			   *stob_create_cfg,
			  struct m0_be_seg		  **out);
M0_INTERNAL void
m0_be_ut_backend_seg_add(struct m0_be_ut_backend	   *ut_be,
			 const struct m0_be_0type_seg_cfg  *seg_cfg,
			 struct m0_be_seg		  **out);
M0_INTERNAL void
m0_be_ut_backend_seg_del(struct m0_be_ut_backend	   *ut_be,
			 struct m0_be_seg		   *seg);

M0_INTERNAL void m0_be_ut_reqh_create(struct m0_reqh **pptr);
M0_INTERNAL void m0_be_ut_reqh_destroy(void);

struct m0_sm_group *
m0_be_ut_backend_sm_group_lookup(struct m0_be_ut_backend *ut_be);

/*
 * Runs asts for the current backend sm group.
 * Waits until at least all asts which were in queue before function call
 * are complete.
 */
M0_INTERNAL void
m0_be_ut_backend_sm_group_asts_run(struct m0_be_ut_backend *ut_be);

void m0_be_ut_backend_new_grp_lock_state_set(struct m0_be_ut_backend *ut_be,
					     bool unlocked_new);

void m0_be_ut_backend_thread_exit(struct m0_be_ut_backend *ut_be);

/* will work with single thread only */
void m0_be_ut_tx_init(struct m0_be_tx *tx, struct m0_be_ut_backend *ut_be);

struct m0_be_ut_seg {
	struct m0_be_seg	*bus_seg;
	struct m0_be_ut_backend *bus_backend;
};

void m0_be_ut_seg_init(struct m0_be_ut_seg *ut_seg,
		       struct m0_be_ut_backend *ut_be,
		       m0_bcount_t size);
void m0_be_ut_seg_fini(struct m0_be_ut_seg *ut_seg);
void m0_be_ut_seg_reload(struct m0_be_ut_seg *ut_seg);

M0_INTERNAL void *m0_be_ut_seg_allocate_addr(m0_bcount_t size);
M0_INTERNAL uint64_t m0_be_ut_seg_allocate_id(void);

/* m0_be_allocator_{init,create,open} */
void m0_be_ut_seg_allocator_init(struct m0_be_ut_seg *ut_seg,
				 struct m0_be_ut_backend *ut_be);
/* m0_be_allocator_{close,destroy,fini} */
void m0_be_ut_seg_allocator_fini(struct m0_be_ut_seg *ut_seg,
				 struct m0_be_ut_backend *ut_be);

M0_INTERNAL void m0_be_ut_alloc(struct m0_be_ut_backend  *ut_be,
				struct m0_be_ut_seg      *ut_seg,
				void                    **ptr,
				m0_bcount_t               size);
M0_INTERNAL void m0_be_ut_free(struct m0_be_ut_backend *ut_be,
			       struct m0_be_ut_seg     *ut_seg,
			       void                    *ptr);

#define M0_BE_UT_ALLOC_PTR(ut_be, ut_seg, ptr)                          \
		m0_be_ut_alloc((ut_be), (ut_seg),                       \
			       (void **) &(ptr), sizeof(*(ptr)))

#define M0_BE_UT_FREE_PTR(ut_be, ut_seg, ptr)                           \
		m0_be_ut_free((ut_be), (ut_seg), (ptr))

/**
 * Executes __action_func in a single transaction.
 * Uses __credit_func to get transaction credit.
 * @see m0_be_ut_alloc()/m0_be_ut_free() source code for examples.
 */
#define M0_BE_UT_TRANSACT(__ut_be, __tx, __cred, __credit_func, __action_func) \
	do {                                                            \
		struct m0_be_tx_credit  __cred   = {};                  \
		struct m0_be_tx        *__tx;                           \
		int                     __rc;                           \
									\
		M0_ALLOC_PTR(__tx);                                     \
		M0_ASSERT(__tx != NULL);                                \
		m0_be_ut_tx_init(__tx, (__ut_be));                      \
		__credit_func;                                          \
		m0_be_tx_prep(__tx, &__cred);                           \
		__rc = m0_be_tx_open_sync(__tx);                        \
		M0_ASSERT_INFO(__rc == 0, "__rc = %d", __rc);           \
		if (__rc == 0) {                                        \
			__action_func;                                  \
			m0_be_tx_close_sync(__tx);                      \
		}                                                       \
		m0_be_tx_fini(__tx);                                    \
		m0_free(__tx);                                          \
	} while (0)

#endif /* __MERO_BE_UT_HELPER_H__ */

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
