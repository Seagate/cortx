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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 04/28/2010
 */

#pragma once

#ifndef __MERO_STOB_STOB_H__
#define __MERO_STOB_STOB_H__

#include "lib/atomic.h"
#include "lib/types.h"         /* m0_uint128 */
#include "lib/vec.h"
#include "lib/chan.h"
#include "lib/rwlock.h"
#include "lib/tlist.h"
#include "fid/fid.h"
#include "sm/sm.h"
#include "stob/cache.h"		/* m0_stob_cache */
#include "fid/fid_xc.h"
#include "be/extmap.h"          /* m0_be_emap_seg */

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

/* import */
struct m0_dtx;
struct m0_chan;
struct m0_indexvec;
struct m0_io_scope;

struct m0_be_tx_credit;
struct m0_be_seg;
struct m0_dtx;

/**
   @defgroup stob Storage objects

   Storage object is a fundamental abstraction of M0. Storage objects offer a
   linear address space for data and may have redundancy and may have integrity
   data.

   There are multiple types of storage objects, used for various purposes and
   providing various extensions of the basic storage object interface described
   below. Specifically, containers for data and meta-data are implemented as
   special types of storage objects.

   @see stoblinux
   @{
 */

struct m0_stob_type_ops;
struct m0_stob_domain_ops;
struct m0_stob_domain;
struct m0_stob_ops;
struct m0_stob;
struct m0_stob_io;

/**
 * m0_stob state specifying its relationship with the underlying storage object.
 * @todo add M0_ prefix.
 */
enum m0_stob_state {
	/**
	 * The state or existence of the underlying storage object are not
	 * known. m0_stob can be used as a placeholder in storage object
	 * identifiers name-space in this state.
	 */
	CSS_UNKNOWN,
	/** The underlying storage object is known to exist. */
	CSS_EXISTS,
	/** The underlying storage object is known to not exist. */
	CSS_NOENT,
	/** The underlying storage object has been requested to be deleted. */
	CSS_DELETE,
};

/**
 * Identifier of a storage object.
 *
 * @see m0_stob, m0_stob_find(), m0_stob_lookup().
 */
struct m0_stob_id {
	/* Stob domain fid. @see m0_stob_domain, m0_stob */
	struct m0_fid si_domain_fid;
	/* Stob fid. @see m0_stob */
	struct m0_fid si_fid;
} M0_XCA_RECORD M0_XCA_DOMAIN(be|rpc);

#define STOB_ID_F  "{" FID_F "," FID_F "}"
#define STOB_ID_P(si)  FID_P(&(si)->si_domain_fid), FID_P(&(si)->si_fid)

/**
 * In-memory representation of a storage object.
 *
 * <b>Description</b>.
 *
 * m0_stob serves multiple purposes:
 *
 *     - it acts as a placeholder in storage object identifiers name-space. For
 *       example, locks can be taken on it;
 *
 *     - it acts as a handle for the underlying storage object. IO and meta-data
 *       operations can be directed to the storage object by calling functions
 *       on m0_stob;
 *
 *     - it caches certain storage object attributes in memory.
 *
 * Accordingly, m0_stob can be in one of the states described by enum
 * m0_stob_state. Compare these m0_stob roles with the socket interface (bind,
 * connect, etc.)
 *
 * Maintenance of stob lifetime is responsibility of stob implementation.
 *
 * Stob operations vectors (so_ops) can differ for stobs within the same
 * domain. Consequently, so_ops is assigned by stob creation interface and
 * depends on stob type implementation.
 *
 * Stob id is unique within a domain but not across cluster.
 * User chooses ids and is responsible for the uniqeness within their domain.
 * Stob id is represented by struct m0_stob_id and consists of:
 *
 *     - Stob domain fid;
 *     - Stob fid.
 *
 * Stob fid has the following structure:
 *
 * @verbatim
 *           8 bits                            120 bits
 *   +----------------------+----------------------------------------------+
 *   |     stob type id     |                  stob key                    |
 *   +----------------------+----------------------------------------------+
 * @endverbatim
 *
 * Note: stob type id is not the same as stob domain type id. It's made to make
 * possible to distinguish stob fids from stob domain fids.
 *
 * <b>Reference counting semantics</b>.
 *
 * m0_stob_find(), m0_stob_find_by_key(), m0_stob_lookup() and
 * m0_stob_lookup_by_key() always acquire reference to a m0_stob implicitly.
 * m0_stob_get() and m0_stob_put() work with references explicity. The rest
 * interface functions don't change reference count of an m0_stob.
 */
struct m0_stob {
	const struct m0_stob_ops *so_ops;
	struct m0_stob_domain	 *so_domain;
	struct m0_stob_id         so_id;
	enum m0_stob_state	  so_state;
	uint64_t		  so_ref;
	/** Channel where drop in ref count for the stob is signalled. */
	struct m0_chan            so_ref_chan;
	/* so_ref_chan protection. */
	struct m0_mutex           so_ref_mutex;
	struct m0_tlink		  so_cache_linkage;
	uint64_t		  so_cache_magic;
	void			 *so_private;
};

/** Stob operations vector. */
struct m0_stob_ops {
	/**
	 * Called when the last reference on the object is released or
	 * stob is evicted from the cache.
	 *
	 * XXX This method is called under exclusive mode stob cache lock.
	 * @see m0_stob_put()
	 */
	void (*sop_fini)(struct m0_stob *stob);
	/** @see m0_stob_destroy_credit() */
	void (*sop_destroy_credit)(struct m0_stob *stob,
				   struct m0_be_tx_credit *accum);
	/** @see m0_stob_destroy() */
	int (*sop_destroy)(struct m0_stob *stob, struct m0_dtx *dtx);
	/** @see m0_stob_punch_credit() */
	int (*sop_punch_credit)(struct m0_stob *stob,
				struct m0_indexvec *want,
				struct m0_indexvec *got,
				struct m0_be_tx_credit *accum);
	/** @see m0_stob_punch() */
	int (*sop_punch)(struct m0_stob *stob, struct m0_indexvec *range,
			 struct m0_dtx *dtx);
	/** @see m0_stob_io_init() */
	int  (*sop_io_init)(struct m0_stob *stob, struct m0_stob_io *io);
	/** @see m0_stob_block_shift() */
	uint32_t (*sop_block_shift)(struct m0_stob *stob);
	/** @see m0_stob_fd() */
	int (*sop_fd)(struct m0_stob *stob);
};

/**
 * Returns an in-memory representation for a stob with a given key,
 * creating the former if necessary.
 *
 * Resulting m0_stob can be in any state. m0_stob_find() neither fetches the
 * object attributes from the storage nor checks for object's existence. This
 * function is used to create a placeholder on which other functions
 * (m0_stob_locate(), m0_stob_create(), locking functions, etc.) can be called.
 *
 * On success, this function acquires a reference on the returned object.
 *
 * @post equi(rc == 0, *out != NULL)
 */
M0_INTERNAL int m0_stob_find(const struct m0_stob_id *id, struct m0_stob **out);
M0_INTERNAL int m0_stob_find_by_key(struct m0_stob_domain *dom,
				    const struct m0_fid *stob_fid,
				    struct m0_stob **out);

/** The same as m0_stob_find() but without m0_stob allocation. */
M0_INTERNAL int m0_stob_lookup(const struct m0_stob_id *id,
			       struct m0_stob **out);
M0_INTERNAL int m0_stob_lookup_by_key(struct m0_stob_domain *dom,
				      const struct m0_fid *stob_fid,
				      struct m0_stob **out);

/**
 * Locates the stob on the storage, fetching its attributes.
 *
 * @pre stob->so_ref > 0
 * @pre stob->so_state == CSS_UNKNOWN
 * @post ergo(rc == 0, M0_IN(m0_stob_state_get(stob), (CSS_EXISTS, CSS_NOENT)))
 */
M0_INTERNAL int m0_stob_locate(struct m0_stob *stob);

/** Calculates BE tx credit for m0_stob_create(). */
M0_INTERNAL void m0_stob_create_credit(struct m0_stob_domain *dom,
				       struct m0_be_tx_credit *accum);
/**
 * Creates a storate object.
 *
 * @param stob Previously allocated in-memory object.
 *
 * @pre stob->so_state != CSS_UNKNOWN
 * @pre stob->so_ref > 0
 * @post ergo(rc == 0, m0_stob_state_get(stob) == CSS_EXISTS))
 */
M0_INTERNAL int m0_stob_create(struct m0_stob *stob,
			       struct m0_dtx *dtx,
			       const char *str_cfg);

/** Calculates BE tx credit for m0_stob_destroy(). */
M0_INTERNAL void m0_stob_destroy_credit(struct m0_stob *stob,
					struct m0_be_tx_credit *accum);

/**
 * Marks stob state as CSS_DELETE to indicate that it has been requested to be
 * deleted.
 * Once a stob's state is marked as CSS_DELETE:
 * - If stob_ref > 1, the stob-delete FOM is made to wait on so_ref_chan channel
 * - This waiting channel is signalled when the stob_ref drops to 1, e.g. by
 *   SNS copy-machine FOM when it is done with the copy operation
 * - Copy-machine FOMs which may be created after 'stob-delete FOM is made to
 *   wait for so_ref drop to 1', exit right-away by releasing the so_ref
 * - The ongoing copy-machine FOMs proceed as usual
 * - Any copy-machine FOM when releases stob_ref, checks if the so_ref now is 1
 *   and if there is any cob-delete FOM waiting on the so_ref_chan channel. If
 *   yes, it signals the so_ref_chan channel
 */
M0_INTERNAL void m0_stob_delete_mark(struct m0_stob *stob);

/*
 * Destroys stob.
 *
 * @pre stob->so_state != CSS_UNKNOWN
 * @pre stob->so_ref == 1
 */
M0_INTERNAL int m0_stob_destroy(struct m0_stob *stob, struct m0_dtx *dtx);

/**
 * Calculates BE tx credit for m0_stob_punch operation.
 * The method of credits calculation is similar to m0_stob_destroy_credit
 * operation, except for punch_credit, the credits are calculated only for the
 * range of segments provided by user.
 */
M0_INTERNAL int m0_stob_punch_credit(struct m0_stob *stob,
				     struct m0_indexvec *want,
				     struct m0_indexvec *got,
				     struct m0_be_tx_credit *accum);

/** Punches a hole within a  stob at specified 'range'  */
M0_INTERNAL int m0_stob_punch(struct m0_stob *stob,
			      struct m0_indexvec *range,
			      struct m0_dtx *dtx);

/**
 * Returns a power of two, which determines alignment required for the user
 * buffers of stob IO requests against this object and IO granularity.
 */
M0_INTERNAL uint32_t m0_stob_block_shift(struct m0_stob *stob);

/**
 * Acquires an additional reference on the stob.
 *
 * @pre stob->so_ref > 1
 */
M0_INTERNAL void m0_stob_get(struct m0_stob *stob);

/**
 * Releases reference to the stob.
 *
 * When the last reference is released, the object can be either returned to
 * the cache or immediately freed at the storage object domain discretion.
 * A cached object can be freed at any time.
 */
M0_INTERNAL void m0_stob_put(struct m0_stob *stob);

/** Returns stob domain the stob belongs to. */
M0_INTERNAL struct m0_stob_domain *m0_stob_dom_get(struct m0_stob *stob);

/**
 * Returns stob state.
 * @see m0_stob_state
 */
M0_INTERNAL enum m0_stob_state m0_stob_state_get(struct m0_stob *stob);

/** Returns stob domain id. */
M0_INTERNAL uint64_t m0_stob_dom_id_get(struct m0_stob *stob);
/** Returns stob id. */
M0_INTERNAL const struct m0_stob_id *m0_stob_id_get(struct m0_stob *stob);
/** Returns stob fid. */
M0_INTERNAL const struct m0_fid *m0_stob_fid_get(struct m0_stob *stob);
/** Returns stob domain id from a stob id. */
M0_INTERNAL uint64_t m0_stob_id_dom_id_get(const struct m0_stob_id *stob_id);
/** Makes stob id from domain id and stob key. */
M0_INTERNAL void m0_stob_id_make(uint64_t container,
				 uint64_t key,
				 const struct m0_fid *dom_id,
				 struct m0_stob_id *stob_id);

M0_INTERNAL bool m0_stob_id_eq(const struct m0_stob_id *stob_id0,
                               const struct m0_stob_id *stob_id1);

/**
 * Returns file descriptor if stob implementation implies an underlying
 * file object.
 */
M0_INTERNAL int m0_stob_fd(struct m0_stob *stob);

M0_INTERNAL int m0_stob_mod_init(void);
M0_INTERNAL void m0_stob_mod_fini(void);

/** @} end group stob */
#endif /* __MERO_STOB_STOB_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
