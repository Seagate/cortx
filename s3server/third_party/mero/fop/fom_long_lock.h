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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 * Original creation date: 18-Jun-2012
 */

#pragma once

#ifndef __MERO_LONG_LOCK_H__
#define __MERO_LONG_LOCK_H__

/**
 * @page m0_long_lock-dld FOM long lock DLD
 * @section m0_long_lock-dld-ovw Overview
 * Long lock is a non-blocking synchronization construct which has to be used in
 * FOM state handlers m0_fom_ops::fo_tick(). Long lock provides support for a
 * reader-writer lock for FOMs.
 *
 * @section m0_long_lock-dld-func Functional specification
 * According to reasons, described in @ref m0_long_lock-dld-logic, application
 * of struct m0_long_lock requires to introduce special link structures
 * struct m0_long_lock_link: one structure per one acquiring long lock in the
 * FOM. It's required that individual FOMs allocate and initialize link
 * structures in their derived FOM data structures, and use this structures with
 * long lock interfaces. The following code example demonstrates the usage
 * example of the lock:
 *
 * @code
 * // Object encompassing FOM for some FOP type which
 * // typically contains necessary context data
 * struct fom_object_type {
 *	// Generic m0_fom object.
 *      struct m0_fom                    fp_gen;
 *      // ...
 *      // Long lock link structure, embedded into derived FOM object.
 *	struct m0_long_lock_link	 fp_link;
 *	// ...
 * };
 *
 * static int fom_tick(struct m0_fom *fom)
 * {
 *      //...
 *      struct fom_object_type	 *fom_obj;
 *      struct m0_long_lock_link *link;
 *
 *	// Retreive derived FOM object and long lock link.
 *	fom_obj = container_of(fom, struct fom_object_type, fp_gen);
 *	link = &fom_obj->fp_link;
 *	//...
 *	if (m0_fom_phase(fom) == PH_CURRENT) {
 *		// try to obtain a lock, when it's obtained FOM is
 *		// transitted into PH_GOT_LOCK
 *
 *		return M0_FOM_LONG_LOCK_RETURN(
 *		     m0_long_read_lock(lock, link, PH_GOT_LOCK));
 *	} else if (m0_fom_phase(fom) == PH_GOT_LOCK) {
 *		// ...
 *		m0_long_read_unlock(lock, link);
 *		// ...
 *	}
 *	//...
 * }
 * @endcode
 *
 * @see m0_long_lock_API
 *
 * @section m0_long_lock-dld-logic Logical specification
 * The lock is considered to be obtained when FOM transitions to the phase
 * specified by the next_phase parameter to the m0_long_{read|write}_lock
 * subroutine - the PH_GOT_LOCK value in the example above. Eventually, when
 * FOMs state handler does not need to read or write shared data structures,
 * access to which must be synchronized, the lock can be released with
 * m0_long_read_unlock() and m0_long_write_unlock() calls.
 *
 * The long lock uses special link structures struct m0_long_lock_link to track
 * the FOMs that actively own the lock (m0_long_lock::l_owners), and the FOMs
 * that are queued waiting to acquire the lock (m0_long_lock::l_waiters).  This
 * is dictated by the requirement that one FOM can obtain multiple long
 * locks. Holding multiple locks assumes that the same FOM can be presented in
 * an arbitary number of queues in different long_locks. Derived FOM objects
 * should contain a distinct link structure for each long lock used. The link
 * must be initialized with m0_long_lock_link_init() before use.
 *
 * To avoid starvation of writers in the face of a stream of incoming readers,
 * a queue of waiting for the long lock FOMs is maintained:
 *
 * - Long lock links store read/write flags.
 * - m0_long_read_lock() adds new links into the m0_long_lock::l_owners list if
 *   the lock is obtained for reading or unlocked, otherwise into the
 *   m0_long_lock::l_waiters.
 * - When a lock is released and there are no FOMs which own the lock, FOMs
 *   waiting for processing (if any) are processed in FIFO order. This avoid
 *   starvation and provides a degree of fairness.
 *
 * When some lock is being unlocked the work of selecting the next lock owner(s)
 * and waking them up is done in the unlock path in m0_long_read_unlock() and
 * m0_long_write_unlock() with respect to m0_long_lock::l_owners and
 * m0_long_lock::l_waiters lists. m0_fom_ready_remote() call is used to wake
 * FOMs up.
 *
 * @defgroup m0_long_lock_API FOM long lock API
 * @{
 * @see @ref m0_long_lock-dld
 *
 */

#include "fop/fop.h"
#include "lib/list.h"
#include "lib/chan.h"
#include "lib/mutex.h"
#include "lib/bob.h"

/**
 * Long lock states.
 */
enum m0_long_lock_state {
	M0_LONG_LOCK_UNLOCKED,
	M0_LONG_LOCK_RD_LOCKED,
	M0_LONG_LOCK_WR_LOCKED
};

/**
 * Type of long lock link, requesting the lock
 */
enum m0_long_lock_type {
	M0_LONG_LOCK_READER,
	M0_LONG_LOCK_WRITER
};

struct m0_long_lock_addb2 {
	/** Time when lock has taken. */
	m0_time_t la_taken;
	/** Indicates whether user has to wait for lock. */
	bool      la_waiting;
	/** Duration of waiting. */
	m0_time_t la_wait;
};

/**
 * Long lock link, special structure used to link multiple owners and waiters of
 * the long lock.
 */
struct m0_long_lock_link {
	/** FOM, which obtains the lock */
	struct m0_fom             *lll_fom;
	/** Linkage to struct m0_long_lock::l_{owners,waiters} list */
	struct m0_tlink            lll_lock_linkage;
	/** magic number. M0_LONG_LOCK_LINK_MAGIX */
	uint64_t                   lll_magix;
	/** Type of long lock, requested by the lll_fom */
	enum m0_long_lock_type     lll_lock_type;
	/** ADDB2 information for link */
	struct m0_long_lock_addb2 *lll_addb2;
};

/**
 * Long lock structure.
 */
struct m0_long_lock {
	/** List of long lock links, which has obtained the lock */
	struct m0_tl            l_owners;
	/** List of long lock links, which waiting for the lock */
	struct m0_tl            l_waiters;
	/** Mutex used to protect the structure from concurrent access. */
	struct m0_mutex         l_lock;
	/** State of the lock */
	enum m0_long_lock_state l_state;
	/** Magic number. M0_LONG_LOCK_MAGIX */
	uint64_t                l_magix;
};

//#ifdef __KERNEL__
#if 1 /* XXX */
#define M0_BE_LONG_LOCK_PAD (264 + 88)
#else
#define M0_BE_LONG_LOCK_PAD (136 + 40)
#endif

/**
 * Persistent long lock structure.
 */
struct m0_be_long_lock {
	union {
		struct m0_long_lock llock;
		char                pad[M0_BE_LONG_LOCK_PAD];
	} bll_u;
} M0_XCA_BLOB M0_XCA_DOMAIN(be);
M0_BASSERT(sizeof(struct m0_long_lock) <=
	   sizeof(M0_FIELD_VALUE(struct m0_be_long_lock, bll_u.pad)));

/**
 * A macros to request a long lock from a fom phase transition function. The
 * value of macros should be returned from the phase transition function. The
 * fom transitions into next_phase when the lock is acquired:
 * - M0_FSO_AGAIN when the lock is acquired immediately;
 * - M0_FSO_WAIT when the lock will be acquired after a wait.
 */
#define M0_FOM_LONG_LOCK_RETURN(rc) ((rc) ? M0_FSO_AGAIN : M0_FSO_WAIT)

/**
 * @post lock->l_state == M0_LONG_LOCK_UNLOCKED
 * @post m0_mutex_is_not_locked(&lock->l_lock)
 */
M0_INTERNAL void m0_long_lock_init(struct m0_long_lock *lock);

/**
 * @pre !m0_long_is_read_locked(lock, *)
 * @pre !m0_long_is_write_locked(lock, *)
 */
M0_INTERNAL void m0_long_lock_fini(struct m0_long_lock *lock);

/**
 * Obtains given lock for reading for given fom. Taking recursive read-lock is
 * not permitted. If the lock is not obtained the invoking FOM should wait; it
 * will eventually be awoken when the lock has been obtained, and will be
 * transitioned to the next_phase state.
 *
 * @param link - Long lock link associated with the FOM
 *		 which has to obtain the lock.
 *
 * @pre link->lll_fom != NULL
 * @pre !m0_long_is_read_locked(lock, link)
 * @pre !m0_tlink_is_in(&link->lll_lock_linkage)
 * @post m0_fom_phase(fom) == next_phase
 *
 * @return true iff the lock is taken.
 */
M0_INTERNAL bool m0_long_read_lock(struct m0_long_lock *lock,
				   struct m0_long_lock_link *link,
				   int next_phase);

/**
 * Obtains given lock for writing for given fom. Taking recursive write-lock is
 * not permitted. If the lock is not obtained the invoking FOM should wait; it
 * will eventually be awoken when the lock has been obtained, and will be
 * transitioned to the next_phase state.
 *
 * @param link - Long lock link associated with the FOM
 *		 which has to obtain the lock.
 *
 * @pre link->lll_fom != NULL
 * @pre !m0_long_is_write_locked(lock, fom)
 * @pre !m0_tlink_is_in(&link->lll_lock_linkage)
 * @post m0_fom_phase(fom) == next_phase
 *
 * @return true iff the lock is taken.
 */
M0_INTERNAL bool m0_long_write_lock(struct m0_long_lock *lock,
				    struct m0_long_lock_link *link,
				    int next_phase);

/**
 * Takes write or read long term lock, depending on the value of the "write"
 * parameter.
 */
M0_INTERNAL bool m0_long_lock(struct m0_long_lock *lock, bool write,
			      struct m0_long_lock_link *link,
			      int next_phase);

/**
 * Unlocks given read-lock.
 *
 * @param link - Long lock link associated with the FOM
 *		 which has to obtain the lock.
 *
 * @pre m0_long_is_read_locked(lock, link);
 * @pre m0_fom_group_is_locked(lock->lll_fom)
 * @post !m0_long_is_read_locked(lock, link);
 */
M0_INTERNAL void m0_long_read_unlock(struct m0_long_lock *lock,
				     struct m0_long_lock_link *link);

/**
 * Unlocks given write-lock.
 *
 * @param link - Long lock link associated with the FOM
 *		 which has to obtain the lock.
 *
 * @pre m0_long_is_write_locked(lock, link);
 * @pre m0_fom_group_is_locked(lock->lll_fom)
 * @post !m0_long_is_write_locked(lock, link);
 */
M0_INTERNAL void m0_long_write_unlock(struct m0_long_lock *lock,
				      struct m0_long_lock_link *link);

/**
 * Unlocks read or write lock.
 */
M0_INTERNAL void m0_long_unlock(struct m0_long_lock *lock,
				struct m0_long_lock_link *link);

/**
 * @return true iff the lock is taken as a read-lock by the given fom.
 */
M0_INTERNAL bool m0_long_is_read_locked(struct m0_long_lock *lock,
					const struct m0_fom *fom);

/**
 * @return true iff the lock is taken as a write-lock by the given fom.
 */
M0_INTERNAL bool m0_long_is_write_locked(struct m0_long_lock *lock,
					 const struct m0_fom *fom);

/**
 * Initialize long lock link object with given fom.
 *
 * addb2 optional argument which is used to collect counters on lock wait/hold
 * times by the link
 * @pre fom != NULL
 */
M0_INTERNAL void m0_long_lock_link_init(struct m0_long_lock_link *link,
					struct m0_fom *fom,
					struct m0_long_lock_addb2 *addb2);

/**
 * Finalize long lock link object.
 *
 * @pre !m0_lll_tlink_is_in(link)
 */
M0_INTERNAL void m0_long_lock_link_fini(struct m0_long_lock_link *link);

M0_BOB_DECLARE(M0_EXTERN, m0_long_lock);
M0_BOB_DECLARE(M0_EXTERN, m0_long_lock_link);

/**
 * Initializes bob-type for m0_long_lock and m0_long_lock_link. Should be called
 * once, during system initialisation.
 */
M0_INTERNAL void m0_fom_ll_global_init(void);


/** @} end of m0_long_lock group */

#endif /* __MERO_LONG_LOCK_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
