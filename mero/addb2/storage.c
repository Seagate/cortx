/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 31-Jan-2015
 */

/**
 * @addtogroup addb2
 *
 * Addb2 storage implementation
 * ----------------------------
 *
 * An addb2 trace is submitted to storage machine by calling
 * m0_addb2_storage_submit(). Traces are collected into "frames". struct frame
 * holds all the information necessary to submit a frame to the storage. A frame
 * contains up to FRAME_TRACE_MAX traces with total size in bytes not exceeding
 * FRAME_SIZE_MAX.
 *
 * Once the frame reaches the maximal size, it is submitted for write to the
 * stob (frame_try()) and moved to the in-flight list
 * (m0_addb2_storage::as_inflight). When IO completes, completion handler
 * frame_endio() is called, which invokes completion call-backs and moves the
 * frame to the idle list (m0_addb2_storage::as_idle).
 *
 * If the idle list is empty (all frames are in-flight), newly submitted traces
 * are added to a pending queue m0_addb2_srorage::as_queue).
 *
 * An intermediate list (m0_addb2_storage::as_pending) contains formed, but
 * still not submitted frames. It is used to avoid keepinf g the storage lock
 * over stob IO launching.
 *
 * On storage a frame starts with the header (m0_addb2_frame_header), which
 * contains frame size and offset of the previous frame header. This allows
 * frames to be scanned in either direction, starting from any valid frame.
 *
 * The header is followed by traces (m0_addb2_trace) one after another. The
 * number of traces in a frame is recorded in the frame header
 * (m0_addb2_frame_header::he_trace_nr).
 *
 * The storage machine maintains the current position, where the next frame will
 * be written (m0_addb2_storage::as_pos). When the position is too close to the
 * stob end, it is wrapped to the beginning (stor_update()).
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ADDB

#include "lib/misc.h"          /* ARRAY_SIZE, M0_FIELD_VALUE, M0_BITS */
#include "lib/vec.h"
#include "lib/chan.h"
#include "lib/trace.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/errno.h"         /* -ENOMEM */
#include "lib/tlist.h"
#include "lib/mutex.h"
#include "lib/memory.h"
#include "xcode/xcode.h"
#include "stob/stob.h"
#include "stob/domain.h"
#include "stob/io.h"
#include "addb2/addb2.h"
#include "addb2/storage.h"
#include "addb2/internal.h"
#include "addb2/addb2_xc.h"
#include "addb2/storage_xc.h"

enum {
	/**
	 * Maximal number of frames storage engine will submit to write
	 * concurrently.
	 */
	MAX_INFLIGHT = 8,
	/**
	 * Number of fragments in frame IO.
	 *
	 * Each frame IO update storage header at the beginning of the stob and
	 * also writes the frame itself in the stob.
	 */
	IO_FRAG = 2
};

/**
 * A frame is a collection of traces written to storage as a unit.
 */
struct frame {
	struct m0_addb2_storage      *f_stor;
	/**
	 * Header for this frame.
	 */
	struct m0_addb2_frame_header  f_header;
	/**
	 * Linkage into storage machine list. Either ->as_inflight or ->as_idle.
	 */
	struct m0_tlink               f_linkage;
	/**
	 * Traces, which are belong to this frame.
	 */
	struct m0_addb2_trace        *f_trace[FRAME_TRACE_MAX];
	/**
	 * Stob io structure used to submit this frame for write.
	 */
	struct m0_stob_io             f_io;
	/**
	 * A buffer, allocated together with the frame, where the frame is
	 * serialised.
	 */
	void                         *f_area;
	/**
	 * Array of sizes of stob extents into which the frame IO is directed.
	 *
	 * Frame IO goes into 2 extents: storage header at the beginning of the
	 * stob and frame proper.
	 */
	m0_bcount_t                   f_count[IO_FRAG];
	/**
	 * Array of starting indices of stob extents.
	 */
	m0_bindex_t                   f_index[IO_FRAG];
	/**
	 * IO buffers.
	 */
	void                         *f_buf[IO_FRAG];
	/**
	 * Clink used to handle stob io completion.
	 */
	struct m0_clink               f_clink;
	uint64_t                      f_magix;
};

/**
 * Addb2 storage machine.
 */
struct m0_addb2_storage {
	struct m0_stob_domain             *as_stob_dom;
	/**
	 * Stob used to store traces.
	 *
	 * Storage engine acquires a reference this the stob in
	 * m0_addb2_storage_init(), releases the reference in
	 * m0_addb2_storage_fini().
	 */
	struct m0_stob                    *as_stob;
	/**
	 * Lock to serialise all machine operations.
	 */
	struct m0_mutex                    as_lock;
	/**
	 * Stob block size shift.
	 */
	unsigned                           as_bshift;
	/**
	 * Stob size, passed to m0_addb2_storage_init().
	 */
	m0_bcount_t                        as_size;
	/**
	 * Offset in the stob where next frame will be written.
	 */
	m0_bindex_t                        as_pos;
	/**
	 * Sequence number of the next frame to be written.
	 */
	uint64_t                           as_seqno;
	/**
	 * Offset in the stob of the previous frame written.
	 */
	m0_bindex_t                        as_prev_offset;
	/**
	 * Queue of trace buffer objects (linked through
	 * m0_addb2_trace_obj::to_linkage) still not placed in a frame.
	 */
	struct m0_tl                       as_queue;
	/**
	 * List of frames ready for population with traces.
	 *
	 * The head of this list is "current frame" (frame_cur()).
	 */
	struct m0_tl                       as_idle;
	/**
	 * The list of frames being written to stob.
	 */
	struct m0_tl                       as_inflight;
	/**
	 * Formed but still not launched frames.
	 */
	struct m0_tl                       as_pending;
	/**
	 * Pre-allocated frames. ->as_idle, ->as_pending and ->as_inflight
	 * contain elements of this array.
	 */
	struct frame                       as_prealloc[MAX_INFLIGHT + 1];
	const struct m0_addb2_storage_ops *as_ops;
	bool                               as_stopped;
	/**
	 * Opaque cookie passed to m0_addb2_storage_init() and returned by
	 * m0_addb2_storage_cookie().
	 */
	void                              *as_cookie;
	/**
	 * Small trace used to mark when the storage engine is opened.
	 */
	struct m0_addb2_trace_obj          as_marker;
	bool                               as_idled;
};

/**
 * List of frames in storage machine. A frame is either in idle
 * (m0_addb2_storage::as_idle) or in-flight (m0_addb2_storage::as_inflight).
 */
M0_TL_DESCR_DEFINE(frame, "addb2 frames",
		   static, struct frame, f_linkage, f_magix,
		   M0_ADDB2_FRAME_MAGIC, M0_ADDB2_FRAME_HEAD_MAGIC);
M0_TL_DEFINE(frame, static, struct frame);

static bool        trace_tryadd(struct m0_addb2_storage *stor,
				struct m0_addb2_trace *trace);
static bool        trace_fits  (const struct frame *frame,
				const struct m0_addb2_trace *trace);
static void        trace_add   (struct frame *frame,
				struct m0_addb2_trace *trace);

static int  frame_init     (struct frame *frame, struct m0_addb2_storage *stor);
static void frame_fini     (struct frame *frame);
static void frame_clear    (struct frame *frame);
static void frame_submit   (struct frame *frame);
static void frame_idle     (struct frame *frame);
static void frame_done     (struct frame *frame);
static int  frame_try      (struct frame *frame);
static void frame_io_pack  (struct frame *frame);
static void frame_io_open  (struct frame *frame);
static bool frame_invariant(const struct frame *frame);
static bool frame_endio    (struct m0_clink *link);

static struct frame *frame_cur   (const struct m0_addb2_storage *stor);

static bool        stor_invariant(const struct m0_addb2_storage *stor);
static void        stor_fini     (struct m0_addb2_storage *stor);
static void        stor_balance  (struct m0_addb2_storage *stor, int delta);
static void        stor_drain    (struct m0_addb2_storage *stor);
static void        stor_update   (struct m0_addb2_storage *stor,
				  const struct m0_addb2_frame_header *header);
static m0_bindex_t stor_round    (const struct m0_addb2_storage *stor,
				  m0_bindex_t index);
static bool        stor_rounded  (const struct m0_addb2_storage *stor,
				  m0_bindex_t index);

static const struct m0_format_tag frame_tag;

enum {
	M0_ADDB2_STOB_KEY     = 2,
};

M0_INTERNAL const uint64_t m0_addb2_stob_key = M0_ADDB2_STOB_KEY;

static int stor_dom_init(struct m0_addb2_storage *stor, const char *location,
			 uint64_t key, bool mkfs, bool force)
{
	struct m0_stob_domain  *dom;
	const char             *str_cfg_init;
	int                     rc;
	int                     rc1 = 0;

	M0_ENTRY();

	str_cfg_init = "directio=true";
	stor->as_stob_dom = NULL;
	rc = m0_stob_domain_init(location, str_cfg_init, &dom);
	if (mkfs) {
		/* Found existing stob domain, kill it. */
		if ((rc == 0 && force) || !M0_IN(rc, (0, -ENOENT))) {
			rc1 = rc == 0 ?
				m0_stob_domain_destroy(dom) :
				m0_stob_domain_destroy_location(location);
			if (rc1 != 0)
				goto out;
		}
		if (rc != 0 || force)
			rc = m0_stob_domain_create(location, str_cfg_init, key,
						   NULL, &dom);
	}
	if (rc != 0)
		return M0_ERR(rc);
out:
	if (rc != 0)
		m0_stob_domain_fini(dom);
	rc = (rc1 == 0 ? rc : rc1);
	if (rc == 0)
		stor->as_stob_dom = dom;
	return M0_RC(rc);
}

static void stor_dom_fini(struct m0_addb2_storage *stor)
{
	m0_stob_domain_fini(stor->as_stob_dom);
}

static int stor_stob_init(struct m0_addb2_storage *stor, uint64_t key)
{
	struct m0_stob_domain *dom = stor->as_stob_dom;
	struct m0_stob        *stob;
	int                    rc;
	struct m0_stob_id      stob_id;

	M0_PRE(dom != NULL);
	stor->as_stob = NULL;

	m0_stob_id_make(0, key, &dom->sd_id, &stob_id);
	rc = m0_stob_find(&stob_id, &stob);
	if (rc == 0) {
		rc = m0_stob_locate(stob);
		if (rc == 0 && m0_stob_state_get(stob) != CSS_EXISTS)
			rc = m0_stob_create(stob, NULL, NULL);
		if (rc == 0)
			stor->as_stob = stob;
		else
			m0_stob_put(stob);
	}
	return M0_RC(rc);
}

static void stor_stob_fini(struct m0_addb2_storage *stor)
{
	m0_stob_put(stor->as_stob);
}

M0_INTERNAL struct m0_addb2_storage *
m0_addb2_storage_init(const char *location, uint64_t key, bool mkfs, bool force,
		      const struct m0_addb2_storage_ops *ops, m0_bcount_t size,
		      void *cookie)
{
	struct m0_addb2_storage     *stor;
	int                          rc;
	struct m0_addb2_frame_header h;
	int                          i;


	M0_PRE(size >= BSIZE + FRAME_SIZE_MAX);
	M0_CASSERT(sizeof(struct m0_addb2_frame_header) <= BSIZE);

	M0_ALLOC_PTR(stor);
	if (stor == NULL)
		return NULL;
	rc = stor_dom_init(stor, location, key, mkfs, force);
	if (rc != 0)
		goto cleanup_stor;
	rc = stor_stob_init(stor, m0_addb2_stob_key);
	if (rc != 0)
		goto cleanup_stob_domain;
	if (mkfs) {
		h = (struct m0_addb2_frame_header) {
			.he_seqno= 0,
			.he_offset = 0,
			.he_size = BSIZE
		};
	} else if (m0_addb2_storage_header(stor->as_stob, &h) == 0 &&
		 size != h.he_stob_size)
		goto cleanup_stob;

	m0_mutex_init(&stor->as_lock);
	stor->as_ops      = ops;
	stor->as_size     = size;
	stor->as_bshift   = m0_stob_block_shift(stor->as_stob);
	/*
	 * For disk format compatibility, make block size a constant
	 * independent of stob block size. Use 64KB.
	 */
	M0_ASSERT(stor->as_bshift <= BSHIFT);
	stor->as_cookie = cookie;
	M0_ASSERT(stor_rounded(stor, size));
	/*
	 * Initialized "h" lets stor_update() below set up the first
	 * frame.
	 */
	stor_update(stor, &h);
	tr_tlist_init(&stor->as_queue);
	frame_tlist_init(&stor->as_inflight);
	frame_tlist_init(&stor->as_idle);
	frame_tlist_init(&stor->as_pending);

	for (i = 0; i < ARRAY_SIZE(stor->as_prealloc); ++i) {
		rc = frame_init(&stor->as_prealloc[i], stor);
		if (rc != 0) {
			stor_fini(stor);
			return NULL;
		}
	}
	stor->as_marker = (struct m0_addb2_trace_obj) {
		.o_tr = {
			.tr_nr   = m0_addb2__dummy_payload_size,
			.tr_body = m0_addb2__dummy_payload
		}
	};
	m0_addb2_storage_submit(stor, &stor->as_marker);

	M0_POST(stor_invariant(stor));
	return stor;

cleanup_stob:
	stor_stob_fini(stor);
cleanup_stob_domain:
	stor_dom_fini(stor);
cleanup_stor:
	m0_free(stor);
	return NULL;
}

M0_INTERNAL void m0_addb2_storage_fini(struct m0_addb2_storage *stor)
{
	m0_mutex_lock(&stor->as_lock);
	M0_PRE(stor_invariant(stor));
	m0_mutex_unlock(&stor->as_lock);
	stor_fini(stor);
}

M0_INTERNAL void *m0_addb2_storage_cookie(const struct m0_addb2_storage *stor)
{
	return stor->as_cookie;
}

M0_INTERNAL void m0_addb2_storage_stop(struct m0_addb2_storage *stor)
{
	m0_mutex_lock(&stor->as_lock);
	M0_PRE(stor_invariant(stor));
	M0_PRE(!stor->as_stopped);
	stor->as_stopped = true;
	stor_balance(stor, 0);
	M0_POST(stor_invariant(stor));
	m0_mutex_unlock(&stor->as_lock);
}

M0_INTERNAL int m0_addb2_storage_submit(struct m0_addb2_storage *stor,
					struct m0_addb2_trace_obj *obj)
{
	m0_mutex_lock(&stor->as_lock);
	M0_PRE(stor_invariant(stor));
	M0_PRE(!stor->as_stopped);
	tr_tlink_init_at_tail(obj, &stor->as_queue);
	stor_balance(stor, 0);
	M0_POST(stor_invariant(stor));
	m0_mutex_unlock(&stor->as_lock);
	return +1;
}

M0_INTERNAL bool
m0_addb2_storage__is_not_locked(const struct m0_addb2_storage *stor)
{
	return m0_mutex_is_not_locked(&stor->as_lock);
}

static void stor_fini(struct m0_addb2_storage *stor)
{
	struct frame *frame;

	/*
	 * This lock-unlock is a barrier against concurrently finishing last
	 * frame_endio(), which signalled ->sto_idle().
	 *
	 * It *is* valid to acquire and finalise a lock in the same function in
	 * this particular case.
	 */
	m0_mutex_lock(&stor->as_lock);
	m0_mutex_unlock(&stor->as_lock);

	m0_tl_teardown(frame, &stor->as_idle, frame) {
		frame_fini(frame);
	}
	frame_tlist_fini(&stor->as_idle);
	frame_tlist_fini(&stor->as_inflight);
	frame_tlist_fini(&stor->as_pending);
	tr_tlist_fini(&stor->as_queue);
	m0_mutex_fini(&stor->as_lock);
	stor_stob_fini(stor);
	stor_dom_fini(stor);
	m0_free(stor);
}

/**
 * Attempts to append some traces into a frames and to submit full frames.
 *
 * "delta" parameter is 1 when called from frame_endio() and 0 otherwise.
 */
static void stor_balance(struct m0_addb2_storage *stor, int delta)
{
	struct m0_addb2_trace_obj *obj = tr_tlist_head(&stor->as_queue);
	bool                       force = false;

	while (obj != NULL && trace_tryadd(stor, &obj->o_tr)) {
		tr_tlist_del(obj);
		force |= obj->o_force;
		obj = tr_tlist_head(&stor->as_queue);
	}
	if (obj == NULL && (stor->as_stopped || force)) {
		struct frame *frame = frame_cur(stor);

		if (frame != NULL && frame->f_header.he_trace_nr > 0)
			frame_submit(frame);
		stor_drain(stor);
		/*
		 * "delta" accounts for the frame for which IO completion is
		 * executed. This frame cannot be moved to the idle list before
		 * calling stor_balance(), because then there will be a
		 * situation when inflight and pending lists are empty, storage
		 * lock is released (in frame_endio()->...->frame_try()) and
		 * frame_endio() is still running---racing with stor_fini().
		 */
		if (frame_tlist_length(&stor->as_inflight) == delta &&
		    stor->as_stopped && !stor->as_idled &&
		    stor->as_ops->sto_idle != NULL) {
			stor->as_idled = true;
			stor->as_ops->sto_idle(stor);
		}
	}
	stor_drain(stor);
}

/**
 * If there is enough space, adds the trace to the current frame. If the frame
 * becomes full, submits it to write.
 */
static bool trace_tryadd(struct m0_addb2_storage *stor,
			 struct m0_addb2_trace *trace)
{
	struct frame *fr = frame_cur(stor);

	if (fr != NULL) {
		if (!trace_fits(fr, trace)) {
			frame_submit(fr);
			fr = frame_cur(stor);
			if (fr == NULL)
				return false;
		}
		trace_add(fr, trace);
		return true;
	} else
		return false;
}

/**
 * True iff the frame has enough space for the trace.
 */
static bool trace_fits(const struct frame *frame,
		       const struct m0_addb2_trace *trace)
{
	return  frame->f_header.he_trace_nr < FRAME_TRACE_MAX &&
		stor_round(frame->f_stor, frame->f_header.he_size +
			   m0_addb2_trace_size(trace)) <= FRAME_SIZE_MAX;
}

/**
 * Adds the trace to the frame.
 */
static void trace_add(struct frame *frame, struct m0_addb2_trace *trace)
{
	M0_PRE(trace_fits(frame, trace));

	frame->f_trace[frame->f_header.he_trace_nr ++] = trace;
	frame->f_header.he_size += m0_addb2_trace_size(trace);
}

/**
 * Initialises a frame.
 */
static int frame_init(struct frame *frame, struct m0_addb2_storage *stor)
{
	int result;

	frame->f_stor = stor;
	/*
	 * Allocate a buffer to contain both the storage header and the frame.
	 */
	frame->f_area = m0_alloc_aligned(BSIZE + FRAME_SIZE_MAX, BSHIFT);
	if (frame->f_area != NULL) {
		struct m0_stob_io *io = &frame->f_io;

		frame_clear(frame);
		m0_stob_io_init(io);
		frame->f_count[0] = BSIZE;
		frame->f_index[0] = 0;
		frame->f_buf[0] = frame->f_area;
		frame->f_buf[1] = frame->f_area + BSIZE;
		io->si_opcode = SIO_WRITE;
		io->si_user   = (struct m0_bufvec) {
			.ov_vec = {
				.v_nr    = ARRAY_SIZE(frame->f_count),
				.v_count = frame->f_count
			},
			.ov_buf = frame->f_buf
		};
		io->si_stob = (struct m0_indexvec) {
			.iv_vec = {
				.v_nr    = ARRAY_SIZE(frame->f_count),
				.v_count = frame->f_count
			},
			.iv_index = frame->f_index
		};
		m0_clink_init(&frame->f_clink, &frame_endio);
		m0_clink_add_lock(&io->si_wait, &frame->f_clink);
		frame_tlink_init_at_tail(frame, &stor->as_idle);
		result = 0;
	} else
		result = M0_ERR(-ENOMEM);
	return M0_RC(result);
}

static void frame_fini(struct frame *frame)
{
	if (frame->f_area != NULL) {
		frame_tlist_remove(frame);
		frame_tlink_fini(frame);
		m0_clink_del_lock(&frame->f_clink);
		m0_clink_fini(&frame->f_clink);
		m0_stob_io_fini(&frame->f_io);
		m0_free_aligned(frame->f_area, BSIZE + FRAME_SIZE_MAX, BSHIFT);
	}
}

/**
 * "Submits" the frame, by moving it to the pending list.
 */
static void frame_submit(struct frame *frame)
{
	struct m0_addb2_frame_header *h    = &frame->f_header;
	struct m0_addb2_storage      *stor = frame->f_stor;

	frame_tlist_move_tail(&stor->as_pending, frame);
	h->he_size        = stor_round(stor, h->he_size);
	h->he_seqno       = stor->as_seqno;
	h->he_offset      = stor->as_pos;
	h->he_prev_offset = stor->as_prev_offset;
	stor_update(stor, h);
}

/**
 * Submits pending frames.
 */
static void stor_drain(struct m0_addb2_storage *stor)
{
	struct frame *frame;

	/* Cannot used m0_tl_for() because of concurrent frame insertions. */
	while ((frame = frame_tlist_head(&stor->as_pending)) != NULL) {
		if (frame_try(frame) != 0) {
			frame_done(frame);
			/* Not much else we can do here. */
			frame_idle(frame);
		}
	}
}

/**
 * Updates the current position after the frame with the given header has been
 * written.
 */
static void stor_update(struct m0_addb2_storage *stor,
			const struct m0_addb2_frame_header *header)
{
	stor->as_seqno       = header->he_seqno + 1;
	stor->as_prev_offset = header->he_offset;
	stor->as_pos         = header->he_offset + header->he_size;
	if (stor->as_size - stor->as_pos < FRAME_SIZE_MAX)
		stor->as_pos = BSIZE; /* wrap around */
}

/**
 * Attempts to submit the frame for write.
 *
 * Copy the frame into frame->f_area, first header, then traces one by
 * one. Submit stob_io.
 *
 * This function releases and re-acquired storage lock.
 */
static int frame_try(struct frame *frame)
{

#define PLACE(cur, obj, count)				\
do {							\
	M0_PRE(M0_IS_8ALIGNED(cur));			\
	size_t __nob = (count) * sizeof *(obj);	\
	memcpy((cur), (obj), __nob);			\
	(cur) += __nob;				\
	M0_POST(M0_IS_8ALIGNED(cur));			\
} while (0)

	struct m0_addb2_frame_header   *h    = &frame->f_header;
	struct m0_addb2_storage        *stor = frame->f_stor;
	void                           *cur;
	int                             result;
	int                             i;

	M0_PRE(!frame_tlist_contains(&stor->as_inflight, frame));
	M0_PRE( frame_tlist_contains(&stor->as_pending,  frame));
	M0_ASSERT(stor_rounded(stor, h->he_offset));

	frame_tlist_move(&stor->as_inflight, frame);
	m0_mutex_unlock(&stor->as_lock);
	m0_format_header_pack(&h->he_header, &frame_tag);
	h->he_time = m0_time_now();
	m0_format_footer_update(h);
	frame->f_count[1] = h->he_size;
	frame->f_index[1] = h->he_offset;
	cur = frame->f_buf[0];
	PLACE(cur, h, 1);
	cur = frame->f_buf[1];
	PLACE(cur, h, 1);
	for (i = 0; i < h->he_trace_nr; ++i) {
		struct m0_addb2_trace *tr = frame->f_trace[i];

		PLACE(cur, &tr->tr_nr, 1);
		PLACE(cur, tr->tr_body, tr->tr_nr);
	}
	frame_io_pack(frame);
	frame->f_io.si_obj = NULL;
	result = m0_stob_io_prepare_and_launch(&frame->f_io, stor->as_stob,
					       NULL, NULL);
	if (result != 0) {
		frame_io_open(frame);
		M0_LOG(M0_ERROR, "Failed to launch: %i.", M0_ERR(result));
	}
	m0_mutex_lock(&stor->as_lock);
	return M0_RC(result);
#undef PLACE
}

/** Returns current frame. */
static struct frame *frame_cur(const struct m0_addb2_storage *stor)
{
	return frame_tlist_head(&stor->as_idle);
}

/**
 * Moves the frame to the idle list.
 *
 * This is done after IO completes (frame_endio()) or fails to start
 * (frame_submit()).
 */
static void frame_idle(struct frame *frame)
{
	frame_clear(frame);
	frame_tlist_move_tail(&frame->f_stor->as_idle, frame);
}

/**
 * Re-initialises the frame.
 */
static void frame_clear(struct frame *frame)
{
	struct m0_addb2_frame_header *h = &frame->f_header;

	M0_SET0(&frame->f_trace);
	*h = (typeof (*h)) {
		.he_size      = sizeof *h,
		.he_stob_size = frame->f_stor->as_size,
		.he_fid       = *m0_stob_fid_get(frame->f_stor->as_stob),
		.he_magix     = M0_ADDB2_FRAME_HEADER_MAGIX
	};
}

/**
 * Write completion handler.
 *
 * Invokes call-backs and moves the frame to the idle list. Submits more frames
 * if possible.
 */
static bool frame_endio(struct m0_clink *link)
{
	struct frame *frame = container_of(link, struct frame, f_clink);
	struct m0_addb2_storage *stor = frame->f_stor;

	/* Call back outside of the mutex to avoid deadlock. */
	if (stor->as_ops->sto_commit != NULL)
		stor->as_ops->sto_commit(stor, &frame->f_header);
	frame_done(frame);
	m0_mutex_lock(&stor->as_lock);
	M0_PRE(stor_invariant(stor));
	stor_balance(stor, 1);
	frame_io_open(frame);
	frame_idle(frame);
	M0_POST(stor_invariant(stor));
	m0_mutex_unlock(&stor->as_lock);
	return true;
}

/**
 * Prepares the frame for stob io submission.
 */
static void frame_io_pack(struct frame *frame)
{
	unsigned shift = frame->f_stor->as_bshift;
	int      i;

	for (i = 0; i < IO_FRAG; ++i) {
		frame->f_buf[i] = m0_stob_addr_pack(frame->f_buf[i], shift);
		frame->f_count[i] >>= shift;
		frame->f_index[i] >>= shift;
	}
}

/**
 * Unpacks the frame after io completion or io submission failure.
 */
static void frame_io_open(struct frame *frame)
{
	unsigned shift = frame->f_stor->as_bshift;
	int      i;

	for (i = 0; i < IO_FRAG; ++i) {
		frame->f_buf[i] = m0_stob_addr_open(frame->f_buf[i], shift);
		frame->f_count[i] <<= shift;
		frame->f_index[i] <<= shift;
	}
}

/**
 * Invokes frame completion call-backs.
 */
static void frame_done(struct frame *frame)
{
	struct m0_addb2_storage *stor = frame->f_stor;
	int                      i;

	for (i = 0; i < frame->f_header.he_trace_nr; ++i) {
		struct m0_addb2_trace     *trace = frame->f_trace[i];
		struct m0_addb2_trace_obj *obj;

		obj = M0_AMB(obj, frame->f_trace[i], o_tr);
		if (stor->as_ops->sto_done != NULL)
			stor->as_ops->sto_done(stor, obj);
		if (obj->o_done != NULL)
			obj->o_done(obj);
		else
			m0_addb2_trace_done(trace);
	}
}

static m0_bindex_t stor_round(const struct m0_addb2_storage *stor,
			      m0_bindex_t index)
{
	return m0_round_up(index, BSIZE);
}

static bool stor_rounded(const struct m0_addb2_storage *stor, m0_bindex_t index)
{
	return stor_round(stor, index) == index;
}

static bool frame_invariant(const struct frame *frame)
{
	const struct m0_addb2_frame_header *h    = &frame->f_header;
	const struct m0_addb2_storage      *stor = frame->f_stor;

	return  _0C(frame_tlink_is_in(frame)) &&
		_0C(frame_tlist_contains(&stor->as_idle,     frame) ||
		    frame_tlist_contains(&stor->as_pending,  frame) ||
		    frame_tlist_contains(&stor->as_inflight, frame)) &&
		_0C(ergo(frame_tlist_contains(&stor->as_inflight, frame) ||
			 frame_tlist_contains(&stor->as_pending,  frame),
			 h->he_size > 0 && h->he_trace_nr > 0 &&
			 stor_rounded(stor, h->he_offset) &&
			 stor_rounded(stor, h->he_size))) &&
		_0C(ergo(frame_tlist_contains(&stor->as_idle, frame) &&
			 h->he_trace_nr > 0, frame == frame_cur(stor))) &&
		_0C(h->he_trace_nr <= ARRAY_SIZE(frame->f_trace)) &&
		_0C(h->he_offset <= stor->as_size - FRAME_SIZE_MAX) &&
		_0C(h->he_size <= FRAME_SIZE_MAX);
}

static bool stor_invariant(const struct m0_addb2_storage *stor)
{
	return  _0C(frame_tlist_length(&stor->as_idle) +
		    frame_tlist_length(&stor->as_pending) +
		    frame_tlist_length(&stor->as_inflight) ==
		    ARRAY_SIZE(stor->as_prealloc)) &&
		_0C(stor_rounded(stor, BSIZE)) &&
		_0C(stor->as_pos <= stor->as_size - FRAME_SIZE_MAX) &&
		m0_forall(i, ARRAY_SIZE(stor->as_prealloc),
			  frame_invariant(&stor->as_prealloc[i]));
}

static const struct m0_format_tag frame_tag = {
	.ot_version       = M0_ADDB2_FRAME_HEADER_FORMAT_VERSION,
	.ot_type          = M0_FORMAT_TYPE_ADDB2_FRAME_HEADER,
	.ot_footer_offset = offsetof(struct m0_addb2_frame_header, he_footer)
};

#undef M0_TRACE_SUBSYSTEM

/** @} end of addb2 group */

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
