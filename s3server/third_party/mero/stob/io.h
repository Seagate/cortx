/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 12-Mar-2014
 */

#pragma once

#ifndef __MERO_STOB_IO_H__
#define __MERO_STOB_IO_H__

#include "lib/vec.h"		/* m0_bufvec */
#include "lib/chan.h"		/* m0_chan */
#include "lib/mutex.h"		/* m0_mutex */
#include "xcode/xcode.h"          /* M0_XCA_ENUM */

/**
 * @defgroup stob Storage object
 *
 * @{
 */

/**
   @name adieu

   Asynchronous Direct Io Extensible User interface (adieu) for storage objects.

   <b>Overview</b>.

   adieu is an interface for a non-blocking (asynchronous) 0-copy (direct)
   vectored IO against storage objects.

   A user of this interface builds an IO operation description and queues it
   against a storage object. IO completion or failure notification is done by
   signalling a user supplied m0_chan. As usual, the user can either wait on
   the chan or register a call-back with it.

   adieu supports scatter-gather type of IO operations (that is, vectored on
   both input and output data).

   adieu can work both on local and remote storage objects. adieu IO operations
   are executed as part of a distributed transaction.

   <b>Functional specification.</b>

   Externally, adieu usage has the following phases:

       - m0_bufvec registration. Some types of storage objects require that
         buffers from which IO is done are registered with its IO sub-system
         (examples: RDMA). This step is optional, IO from unregistered buffers
         should also be possible (albeit might incur additional data-copy).

       - IO description creation. A IO operation description object m0_stob_io
         is initialised.

       - IO operation is queued by a call to m0_stob_io_launch(). It is
         guaranteed that on a successful return from this call, a chan embedded
         into IO operation data-structure will be eventually signalled.

       - An execution of a queued IO operation can be delayed for some time
         due to storage traffic control regulations, concurrency control,
         resource quotas or barriers.

       - An IO operation is executed, possibly by splitting it into
         implementation defined fragments. A user can request an "prefixed
         fragments execution" mode (m0_stob_io_flags::SIF_PREFIX) constraining
         execution concurrency as to guarantee that after execution completion
         (with success or failure) a storage is updated as if some possibly
         empty prefix of the IO operation executed successfully (this is similar
         to the failure mode of POSIX write call). When prefixed fragments
         execution mode is not requested, an implementation is free to execute
         fragments in any order and with any degree of concurrency. Prefixed
         fragments execution mode request has no effect on read-only IO
         operations.

       - When whole operation execution completes, a chan embedded into IO
         operation data-structure is signalled. It is guaranteed that no IO is
         outstanding at this moment and that adieu implementation won't touch
         either IO operation structure or associated data pages afterward.

       - After analyzing IO result codes, a user is free to either de-allocate
         IO operation structure by calling m0_stob_io_fini() or use it to queue
         another IO operation potentially against different object.

   <b>Ordering and barriers.</b>

   The only guarantee about relative order of IO operations state transitions is
   that execution of any updating operation submitted before
   m0_stob_io_opcode::SIO_BARRIER operation completes before any updating
   operation submitted after the barrier starts executing. For the purpose of
   this definition, an updating operation is an operation of any valid type
   different from SIO_READ (i.e., barriers are updating operations).

   A barrier operation completes when all operations submitted before it
   (including other barrier operations) complete.

   @warning Clarify the scope of a barrier: a single storage object, a storage
   object domain, a storage object type, all local storage objects or all
   objects in the system.

   <b>IO alignment and granularity.</b>

   Alignment is not "optimal IO size". This is a requirement rather than hint.

   Block sizes are needed for the following reasons:

       - to insulate stob IO layer from read-modify-write details;

       - to allow IO to the portions of objects inaccessible through the
         flat 64-bit byte-granularity name-space.

   @note the scheme is very simplistic, enforcing the same unit of
   alignment and granularity. Sophistication could be added as
   necessary.

   <b>Result codes.</b>

   In addition to filling in data pages with the data (in a case read
   operation), adieu supplies two status codes on IO completion:

       - <tt>m0_stob_io::si_rc</tt> is a return code of IO operation. 0 means
         success, any other possible value is negated errno;

       - <tt>m0_stob_io::si_count</tt> is a number of blocks (as defined by
         m0_stob_op::sop_block_shift()) successfully transferred between data
         pages and the storage object. When IO is executed in prefixed
         fragments mode, exactly <tt>m0_stob_io::si_count</tt> blocks of the
         storage object, starting from the offset
         <tt>m0_stob_io::si_stob.ov_index[0]</tt> were transferred.

   <b>Data ownership.</b>

   Data pages are owned by adieu implementation from the moment of call to
   m0_stob_io_launch() until the chan is signalled. adieu users must not
   inspect or modify data during that time. An implementation is free to modify
   the data temporarily, un-map pages, etc. An implementation must not touch
   the data at any other time.

   <b>Liveness rules.</b>

   m0_stob_io can be freed once it is owned by an adieu user (see data
   ownership). It has no explicit reference counting, a user must add its own
   should m0_stob_io be shared between multiple threads.

   The user must guarantee that the target storage object is pinned in memory
   while IO operation is owned by the implementation. An implementation is free
   to touch storage object while IO is in progress.

   Similarly, the user must pin the transaction and IO scope while m0_stob_io is
   owned by the implementation.

   <b>Concurrency.</b>

   When m0_stob_io is owned by a user, the user is responsible for concurrency
   control.

   Implementation guarantees that synchronous channel notification (through
   clink call-back) happens in the context not holding IO lock.

   At the moment there are two types of storage object supporting adieu:

       - Linux file system based one, using Linux libaio interfaces;

       - AD stob type implements adieu on top of underlying backing store
         storage object.

   <b>State.</b>
   @verbatim

                      (O)(X)
                       |  ^
                       |  |
     m0_stob_io_init() |  | m0_stob_io_fini()
                       |  |
                       V  |
                     SIS_IDLE
                       |  ^
                       |  |
   m0_stob_io_launch() |  | IO completion
                       |  |
                       V  |
                     SIS_BUSY

   @endverbatim

   @todo A natural way to extend this design is to introduce additional
   SIS_PREPARED state and to split IO operation submission into two stages: (i)
   "preparation" stage that is entered once "IO geometry" is known (i.e., once
   m0_vec of data pages and m0_vec storage objects are known) and (ii)
   "queueing" stage that is entered when in addition to IO geometry, actual data
   pages are allocated. The motivating example for this refinement is a data
   server handling read or write RPC from a client. The RPC contains enough
   information to build IO vectors, while data arrive later through RDMA. To
   avoid dead-locks, it is crucial to avoid dynamic resource allocations (first
   of all, memory allocations) in data path after resources are consumed by
   RDMA. To this end, IO operation must be completely set up and ready for
   queueing before RMDA starts, i.e., before data pages are available.

   @{
 */

struct m0_stob;
struct m0_stob_domain;
struct m0_be_tx_credit;

/**
   Type of a storage object IO operation.

   @todo implement barriers.
 */
enum m0_stob_io_opcode {
	SIO_INVALID,
	SIO_READ,
	SIO_WRITE,
	SIO_BARRIER,
	SIO_SYNC
};

/**
   State of adieu IO operation.
 */
enum m0_stob_io_state {
	/** State used to detect un-initialised m0_stob_io. */
	SIS_ZERO = 0,
	/**
	    User owns m0_stob_io and data pages. No IO is ongoing.
	 */
	SIS_IDLE,
	/**
	   Operation has been queued for execution by a call to
	   m0_stob_io_prepare().
	 */
	SIS_PREPARED,
	/**
	   Operation has been queued for execution by a call to
	   m0_stob_io_launch(), but hasn't yet been completed. adieu owns
	   m0_stob_io and data pages.
	 */
	SIS_BUSY,
};

/**
   Flags controlling the execution of IO operation.
 */
enum m0_stob_io_flags {
	/**
	   Execute operation in "prefixed fragments" mode.

	   It is called "prefixed" because in this mode it is guaranteed that
	   some initial part of the operation is executed. For example, when
	   writing N blocks at offset X, it is guaranteed that when operation
	   completes, blocks in the extent [X, X+M] are written to. When
	   operation completed successfully, M == N, otherwise, M might be less
	   than N. That is, here "prefix" means the same as in "string prefix"
	   (http://en.wikipedia.org/wiki/Prefix_(computer_science) ), because
	   [X, X+M] is a prefix of [X, X+N] when M <= N.
	 */
	SIF_PREFIX	 = (1 << 0),
};

/**
   Asynchronous direct IO operation against a storage object.
 */
struct m0_stob_io {
	enum m0_stob_io_opcode      si_opcode;
	/**
	   Flags with which this IO operation is queued.
	 */
	enum m0_stob_io_flags       si_flags;
	/**
	   Where data are located in the user address space.

	   @note buffer sizes in m0_stob_io::si_user.ov_vec.v_count[]
	   are in block size units (as determined by
	   m0_stob_op::sop_block_shift). Buffer addresses in
	   m0_stob_io::si_user.ov_buf[] must be shifted block-shift bits
	   to the left.
	 */
	struct m0_bufvec            si_user;
	/**
	   Where data are located in the storage object name-space.

	   Segments in si_stob must be non-overlapping and go in increasing
	   offset order.

	   @note extent sizes in m0_stob_io::si_stob.iv_vec.v_count[] and extent
	   offsets in m0_stob_io::si_stob.ov_index[] are in block size units (as
	   determined by m0_stob_op::sop_block_shift).
	 */
	struct m0_indexvec          si_stob;
	/**
	   Channel where IO operation completion is signalled.

	   @note alternatively a channel embedded in every state machine can be
	   used.
	 */
	struct m0_chan              si_wait;
	struct m0_mutex             si_mutex; /**< si_wait chan protection */

	/* The fields below are modified only by an adieu implementation. */

	/**
	   Storage object this operation is against.
	 */
	struct m0_stob             *si_obj;
	/** operation vector */
	const struct m0_stob_io_op *si_op;
	/**
	   Result code.

	   This field is valid after IO completion has been signalled.
	 */
	int32_t                     si_rc;
	/**
	   Number of blocks transferred between data pages and storage object.

	   This field is valid after IO completion has been signalled.
	 */
	m0_bcount_t                 si_count;
	/**
	   State of IO operation. See state diagram for adieu. State transition
	   from SIS_BUSY to SIS_IDLE is asynchronous for adieu user.
	 */
	enum m0_stob_io_state       si_state;
	/**
	   Distributed transaction this IO operation is part of.

	   This field is owned by the adieu implementation.
	 */
	struct m0_dtx              *si_tx;
	/**
	   IO scope (resource accounting group) this IO operation is a part of.
	 */
	struct m0_io_scope         *si_scope;
	/**
	   Pointer to implementation private data associated with the IO
	   operation.

	   This pointer is initialized when m0_stob_io is queued for the first
	   time. When IO completes, the memory allocated by implementation is
	   not immediately freed (the implementation is still guaranteed to
	   never touch this memory while m0_stob_io is owned by a user).

	   @see m0_stob_io::si_stob_magic
	 */
	void                       *si_stob_private;
	/**
	   Stob type magic used to detect when m0_stob_io::si_stob_private can
	   be re-used.

	   This field is set to the value of m0_stob_type::st_magic when
	   m0_stob_io::si_stob_private is allocated. When the same m0_stob_io is
	   used to queue IO again, the magic is compared against type magic of
	   the target storage object. If magic differs (meaning that previous IO
	   was against an object of different type), implementation private data
	   at m0_stob_io::si_stob_private are freed and new private data are
	   allocated. Otherwise, old private data are re-used.

	   @see m0_stob_io::si_stob_private

	   @note magic number is used instead of a pointer to a storage object
	   or storage object class, to avoid pinning them for undefined amount
	   of time.

	   @note currently it is equal to stob type id
	 */
	uint32_t                    si_stob_magic;
	/** FOL record part representing operations on storage object. */
	struct m0_fol_frag         *si_fol_frag;
	/**
	   A sequence of block attributes.
	   Each element of this sequence is an array of N 64-bit values, where
	   N is the number of block attributes used for this stob (i.e., number
	   of 1-s in binary mask passed to ->sop_battr_set()).

	   For write, elements of this sequence are associated with the
	   matching written blocks.

	   For read, the sequence is populated with the block attributes of
	   read blocks.
	 */
	struct m0_bufvec            si_battr;
	/** IO launch time. */
	m0_time_t                   si_start;
	uint64_t                    si_id;
};

struct m0_stob_io_op {
	/**
	   Called by m0_stob_io_fini() to finalize implementation resources.

	   Also called when the same m0_stob_io is re-used for a different type
	   of IO.

	   @see m0_stob_io_private_fini().
	 */
	void (*sio_fini)(struct m0_stob_io *io);
	/**
	   Called by m0_stob_io_launch() to queue IO operation.

	   @note This method releases lock before successful returning.

	   @pre io->si_state == SIS_BUSY
	   @post ergo(result != 0, io->si_state == SIS_IDLE)
	 */
	int  (*sio_launch)(struct m0_stob_io *io);
	/**
	   Called by m0_stob_io_prepare() to capture metadata accroding to
	   internal logic.

	   @pre io->si_state == SIS_PREPARED
	   @post ergo(result != 0, io->si_state == SIS_BUSY)
	 */
	int  (*sio_prepare)(struct m0_stob_io *io);
};

/**
   @post io->si_state == SIS_IDLE
 */
M0_INTERNAL void m0_stob_io_init(struct m0_stob_io *io);

/**
   @pre io->si_state == SIS_IDLE
 */
M0_INTERNAL void m0_stob_io_fini(struct m0_stob_io *io);

/**
   Calculates BE tx credit for stob I/O operation.

   @param io Defines operation. Only some fields should be filled. At least
   m0_stob_io::si_opcode, m0_stob_io::si_flags and m0_stob_io::si_stob.

   @note Only SIO_WRITE opcode is supported at the moment.
 */
M0_INTERNAL void m0_stob_io_credit(const struct m0_stob_io *io,
				   const struct m0_stob_domain *dom,
				   struct m0_be_tx_credit *accum);

/**
   @pre obj->so_state == CSS_EXISTS
   @pre m0_chan_has_waiters(&io->si_wait)
   @pre io->si_state == SIS_PREPARED
   @pre io->si_opcode != SIO_INVALID
   @pre m0_vec_count(&io->si_user.ov_vec) == m0_vec_count(&io->si_stob.ov_vec)
   @pre m0_stob_io_user_is_valid(&io->si_user)
   @pre m0_stob_io_stob_is_valid(&io->si_stob)

   @post ergo(result != 0, io->si_state == SIS_IDLE)

   @note IO can be already completed by the time m0_stob_io_launch()
   finishes. Because of this no post-conditions for io->si_state are imposed in
   the successful return case.
 */
M0_INTERNAL int m0_stob_io_launch(struct m0_stob_io *io, struct m0_stob *obj,
				  struct m0_dtx *tx, struct m0_io_scope *scope);

/**
   @pre obj->so_state == CSS_EXISTS
   @pre m0_chan_has_waiters(&io->si_wait)
   @pre io->si_state == SIS_IDLE
   @pre io->si_opcode != SIO_INVALID
   @pre m0_vec_count(&io->si_user.ov_vec) == m0_vec_count(&io->si_stob.ov_vec)
   @pre m0_stob_io_user_is_valid(&io->si_user)
   @pre m0_stob_io_stob_is_valid(&io->si_stob)

   @post ergo(result != 0, io->si_state == SIS_IDLE)
 */
M0_INTERNAL int m0_stob_io_prepare(struct m0_stob_io *io,
				   struct m0_stob *obj,
				   struct m0_dtx *tx,
				   struct m0_io_scope *scope);

/**
   @see m0_stob_io_prepare() and m0_stob_io_launch().
 */
M0_INTERNAL int m0_stob_io_prepare_and_launch(struct m0_stob_io *io,
					      struct m0_stob *obj,
					      struct m0_dtx *tx,
					      struct m0_io_scope *scope);

/**
   Returns true if user is a valid vector of user IO buffers.
 */
M0_INTERNAL bool m0_stob_io_user_is_valid(const struct m0_bufvec *user);

/**
   Returns true if stob is a valid vector of target IO extents.
 */
M0_INTERNAL bool m0_stob_io_stob_is_valid(const struct m0_indexvec *stob);

/**
   Reads or writes bufvector to stob

   @pre stob != NULL
   @pre bufvec != NULL
   @pre M0_IN(op_code, (SIO_READ, SIO_WRITE))
 */
M0_INTERNAL int m0_stob_io_bufvec_launch(struct m0_stob   *stob,
					 struct m0_bufvec *bufvec,
					 int               op_code,
					 m0_bindex_t       offset);


/**
   Scales buffer address into block-sized units.

   @see m0_stob_addr_open()
 */
M0_INTERNAL void *m0_stob_addr_pack(const void *buf, uint32_t shift);

/**
   Scales buffer address back from block-sized units.

   @see m0_stob_addr_pack()
 */
M0_INTERNAL void *m0_stob_addr_open(const void *buf, uint32_t shift);

/**
 * Sorts index vecs from stob. It also move buffer vecs while sorting.
 *
 * @param stob storage object from which index vecs needs to sort.
 */
M0_INTERNAL void m0_stob_iovec_sort(struct m0_stob_io *stob);

/**
 * On success, ensures that io->si_stob_private is setup to launch IO against
 * the object "obj".
 */
M0_INTERNAL int m0_stob_io_private_setup(struct m0_stob_io *io,
					 struct m0_stob *obj);
/** @} end of stob group */
#endif /* __MERO_STOB_IO_H__ */

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
