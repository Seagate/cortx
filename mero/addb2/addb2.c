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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 21-Dec-2014
 */


/**
 * @addtogroup addb2 ADDB.2
 *
 * IMPLEMENTATION
 * --------------
 *
 * addb2 machine (m0_addb2_mach) is a structure maintaining the "context" (see
 * addb2/addb2.h top-level comment) and build internal representation of
 * produced records, suitable for storage and network.
 *
 * Records and contexts are accumulated in m0_addb2_trace, which is an array of
 * 64-bit elements (m0_abbd2_trace::tr_body[]). This array contains context
 * manipulation operations (pushes and pops) and records in order they were
 * issued to the machine.
 *
 * After a machine is allocated and initialised (m0_addb2_init()), the following
 * operations can be executed against it:
 *
 *     * push (m0_addb2_push()): this operation adds a label to the current
 *       context, maintained in m0_addb2_mach::ma_label[] array. Maximal stack
 *       depth is hard-coded to M0_ADDB2_LABEL_MAX. A label is specified by a
 *       56-bit identifier (given as a 64-bit value with 8 highest bits zeroed)
 *       and a payload, consisting of a given number of 64-bit values. Number of
 *       values in the payload must not exceed VALUE_MAX_NR. In addition to
 *       being stored in the current context, the label is also serialised to
 *       the current trace buffer (m0_addb2_mach::ma_cur) as following:
 *
 *           - first, a 64-bit "tagged identifier" is added to the trace
 *             buffer. Lowest 56 bits of the tagged identifier are equal to the
 *             label identifier. Highest 8 bits are logical OR of the constant
 *             PUSH opcode and the number of values in the payload (this number
 *             always fits into 4 bits);
 *
 *           - then payload values are added one by one;
 *
 *     * pop (m0_addb2_pop()): this operation removes the top-level label from
 *       the current context and adds 64-bit constant POP to the current trace
 *       buffer;
 *
 *     * add (m0_addb2_add()): this operation adds a record. The record is
 *       delivered for online consumption (record_consume()) and added to the
 *       current trace buffer similarly to the push operation, except that DATA
 *       opcode is used instead of PUSH;
 *
 *     * sensor (m0_addb2_sensor_add()): this operation adds a sensor. The
 *       sensor is added to the list hanging off of the top-most context label
 *       (tentry::e_sensor) and is serialised to the trace buffer
 *       (sensor_place()) similarly to the push and add operations, except that
 *       SENSOR opcode is used. The record produced by the sensor is delivered
 *       to online CONSUMERS.
 *
 *       IMPLEMENTATION will read out and serialise the sensor when
 *
 *           - a new trace buffer and initialised and the sensor is still in
 *             context;
 *
 *           - the sensor goes of out context as result of a pop operation.
 *
 *       In either case the produced record is added to the buffer and delivered
 *       to online CONSUMERS.
 *
 * Trace buffer is parsed by m0_addb2_cursor_next().
 *
 * Incoming stream of context manipulations and records is parceled into
 * traces. Each trace is self-sufficient: it starts with a sequence of push
 * operations, recreating the context, which was current at the moment the trace
 * began (mach_buffer()).
 *
 * Traces are internally wrapped in "buffers" (struct buffer). A buffer can be
 * either the current buffer (m0_addb2_mach::ma_cur) to which new elements are
 * added, or belong to one of the two per-machine lists:
 *
 *     * busy list (m0_addb2_mach::ma_busy) is a list of buffers submitted for
 *       processing (network or storage transmission), but not yet processed;
 *
 *     * idle list (m0_addb2_mach:ma_idle) is a list of buffers ready to replace
 *       current buffer, when it becomes full.
 *
 * When current buffer becomes full it is submitted (pack()) by calling
 * m0_addb2_mach_ops::apo_submit(). If ->apo_submit() returns a positive value,
 * the buffer is assumed accepted for processing and moved to the busy list. If
 * ->apo_submit() returns anything else, the buffer is assumed to be processed
 * instantly and moved to the idle list. When processing of a buffer accepted
 * for processing is completed, m0_addb2_trace_done() should be called. This
 * moves the buffer from busy to idle list.
 *
 * Machine keeps the total number of allocated buffers between BUFFER_MIN and
 * BUFFER_MAX. If the maximum is reached, the warning is issued (mach_buffer())
 * and records will be lost.
 *
 * Concurrency
 * -----------
 *
 * PRODUCER should guarantee that calls to m0_addb2_{pop,push,add}() are
 * serialised. IMPLEMENTATION internally takes m0_addb2_mach::ma_lock when it
 * has to move a buffer from list to list, that is, when the current buffer
 * becomes full or a buffer processing completes. This lock is taken by
 * m0_addb2_trace_done().
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ADDB

#include "lib/finject.h"
#include "lib/errno.h"                       /* ENOMEM, EPROTO */
#include "lib/thread.h"                      /* m0_thread_tls */
#include "lib/arith.h"                       /* M0_CNT_DEC, M0_CNT_INC */
#include "lib/tlist.h"
#include "lib/trace.h"
#include "lib/memory.h"
#include "lib/mutex.h"
#include "lib/mutex.h"
#include "lib/locality.h"
#include "fid/fid.h"
#include "module/instance.h"                 /* m0_get */

#include "addb2/addb2.h"
#include "addb2/internal.h"
#include "addb2/consumer.h"
#include "addb2/addb2_xc.h"
#include "addb2/storage.h"                   /* m0_addb2_frame_header */
#include "addb2/storage_xc.h"
#include "addb2/identifier.h"                /* M0_AVI_NODATA */

enum {
#ifndef __KERNEL__
	/**
	 * Trace buffer size (m0_addb2_trace::tr_body[]) in bytes.
	 *
	 * This should be small enough so that traces can be easily piggy-backed
	 * to outgoing RPC packets. This should be large enough so that storage
	 * (m0_addb2_storage) could build a large storage IO without having to
	 * wait for a very large number of traces.
	 */
	BUFFER_SIZE      = 64 * 1024,
	/**
	 * Minimal number of trace buffers allocated for a machine.
	 */
	BUFFER_MIN       = 4,
#else
	BUFFER_SIZE      = 4096,
	BUFFER_MIN       = 0,
#endif
	/** Must be enough to push 1 storage frame out. */
	BUFFER_MAX       = 2 * (FRAME_SIZE_MAX / BUFFER_SIZE + 1),
	/**
	 * Bit-mask identifying bits used to store a "tag", which is
	 * (opcode|payloadsize).
	 */
	TAG_MASK         = 0xff00000000000000ull,
	/**
	 * Amount of free space in a buffer after sensor placement.
	 *
	 * When a new buffer is created, no more than (BUFFER_SIZE -
	 * SENSOR_THRESHOLD) of it is used for sensors.
	 */
	SENSOR_THRESHOLD = 4 * BUFFER_SIZE / 5
};

/**
 * When define, a ->ma_name[] field is added to the m0_addb2_mach, where name of
 * the creator thread is stored.
 */
#define DEBUG_OWNERSHIP (1)

/**
 * Check that buffer still have free space after all labels are pushed.
 *
 * Arbitrary assume that a label has 2 payload elements on average.
 */
M0_BASSERT(BUFFER_SIZE > M0_ADDB2_LABEL_MAX * 3 * sizeof(uint64_t));

/**
 * Trace buffer that can be linked in a per-machine list.
 */
struct buffer {
	/** Linkage in a list. */
	struct m0_tlink            b_linkage;
	/** Trace buffer. */
	struct m0_addb2_trace_obj  b_trace;
	uint64_t                   b_magix;
};

/**
 * Label in the context stack.
 */
struct tentry {
	/**
	 * Payload area.
	 */
	uint64_t               e_value[VALUE_MAX_NR];
	/**
	 * List of sensors, associated with this label.
	 */
	struct m0_tl           e_sensor;
	/**
	 * Label "value" (i.e., identifier and payload).
	 *
	 * This always points to an element of mach->ma_rec.ar_label[].
	 */
	struct m0_addb2_value *e_recval;
};

/**
 * Addb2 machine.
 */
struct m0_addb2_mach {
	/**
	 * The current label stack context.
	 */
	struct tentry                   ma_label[M0_ADDB2_LABEL_MAX];
	/**
	 * The current buffer.
	 */
	struct buffer                  *ma_cur;
	/**
	 * The list of buffers ready to be used.
	 *
	 * A buffer gets to this list when it is newly allocated
	 * (buffer_alloc()).
	 *
	 * A buffer gets off this list when it becomes current (mach_buffer()).
	 */
	struct m0_tl                    ma_idle;
	/**
	 * Length of the idle list.
	 */
	m0_bcount_t                     ma_idle_nr;
	/**
	 * The list of buffers submitted for processing (by calling
	 * m0_addb2_mach_ops::apo_submit()).
	 *
	 * The current buffer gets to this list when it becomes full (pack()).
	 *
	 * A buffers gets off this list when m0_addb2_trace_done() is called
	 * against it.
	 */
	struct m0_tl                    ma_busy;
	/**
	 * Length of the busy list.
	 */
	m0_bcount_t                     ma_busy_nr;
	/**
	 * Greater than 0 iff an operation (add, push, pop, sensor) is currently
	 * underway for the machine.
	 *
	 * This is used to detect and suppress nested addb2 invocations.
	 */
	uint32_t                        ma_nesting;
	/**
	 * True when an attempt to stop the machine is underway.
	 */
	bool                            ma_stopping;
	const struct m0_addb2_mach_ops *ma_ops;
	/**
	 * Protects buffer lists.
	 *
	 * This lock is acquired relatively rarely, when a buffer becomes full,
	 * or when buffer processing completes.
	 */
	struct m0_mutex                 ma_lock;
	/**
	 * This machine as a source of addb2 records. Online CONSUMERS register
	 * with this.
	 *
	 * @see m0_addb2_mach_source().
	 */
	struct m0_addb2_source          ma_src;
	/**
	 * The "current record". This is used for delivery to online CONSUMERS.
	 *
	 * @note mach->ma_label[i].e_recval == &mach->ma_rec.ar_label[i]
	 */
	struct m0_addb2_record          ma_rec;
	/**
	 * Opaque cookie, passed to m0_addb2_mach_init() and returned by
	 * m0_addb2_cookie().
	 */
	void                           *ma_cookie;
	/**
	 * Semaphore upped when the machine becomes idle.
	 */
	struct m0_semaphore             ma_idlewait;
	/**
	 * Linkage into mach_tlist used by sys.c.
	 */
	struct m0_tlink                 ma_linkage;
	/**
	 * Time when current trace was last packed.
	 */
	m0_time_t                       ma_packed;
	/**
	 * The number of sensors to skip when creating a new buffer. This is
	 * used to limit the amount of buffer space consumed by sensors.
	 *
	 * @see mach_buffer(), SENSOR_THRESHOLD.
	 */
	unsigned                        ma_sensor_skip;
	uint64_t                        ma_magix;
#if DEBUG_OWNERSHIP
	char                            ma_name[100];
	char                            ma_last[100];
#endif
};

/**
 * List of traces. This is used by the network and storage components of addb2
 * to link traces into their internal queues.
 */
M0_TL_DESCR_DEFINE(tr, "addb2 traces",
		   M0_INTERNAL, struct m0_addb2_trace_obj, o_linkage, o_magix,
		   M0_ADDB2_TRACE_MAGIC, M0_ADDB2_TRACE_HEAD_MAGIC);
M0_TL_DEFINE(tr, M0_INTERNAL, struct m0_addb2_trace_obj);

/**
 * List of buffers. Used to link traces into m0_addb2_mach lists.
 */
M0_TL_DESCR_DEFINE(buf, "addb2 buffers",
		   static, struct buffer, b_linkage, b_magix,
		   M0_ADDB2_BUF_MAGIC, M0_ADDB2_BUF_HEAD_MAGIC);
M0_TL_DEFINE(buf, static, struct buffer);

/**
 * List of sensors, associated with a label.
 */
M0_TL_DESCR_DEFINE(sensor, "addb2 sensors",
		   static, struct m0_addb2_sensor, s_linkage, s_magix,
		   M0_ADDB2_SENSOR_MAGIC, M0_ADDB2_SENSOR_HEAD_MAGIC);
M0_TL_DEFINE(sensor, static, struct m0_addb2_sensor);

/**
 * List of addb2 machines, used to implement machine pool in addb2/sys.c
 */
M0_TL_DESCR_DEFINE(mach, "addb2 machines",
		   M0_INTERNAL, struct m0_addb2_mach, ma_linkage, ma_magix,
		   M0_ADDB2_MACH_MAGIC, M0_ADDB2_MACH_HEAD_MAGIC);
M0_TL_DEFINE(mach, M0_INTERNAL, struct m0_addb2_mach);

/**
 * Trace bytecodes.
 *
 * Bytecodes are used to encode operations, by tagging high bytes of
 * identifiers. See the top level comment in this file.
 */
enum {
	/**
	 * Push operation. Lowest 4 bits are number of 64-bit values in
	 * payload.
	 */
	PUSH   = 0x10,
	/** Pop of the top-most context label. */
	POP    = 0x0f,
	/**
	 * Add operation. Lowest 4 bits are number of 64-bit values in payload.
	 */
	DATA   = 0x20,
	/**
	 * Sensor operation. Lowest 4 bits are number of 64-bit values in
	 * payload.
	 */
	SENSOR = 0x30
};

static int buffer_alloc(struct m0_addb2_mach *mach);
static void buffer_fini(struct buffer *buffer);
static m0_bcount_t buffer_space(const struct buffer *buffer);
static void buffer_add(struct buffer *buf, uint64_t datum);
static struct buffer *mach_buffer(struct m0_addb2_mach *mach);
static struct tentry *mach_top(struct m0_addb2_mach *m);
static struct buffer *cur(struct m0_addb2_mach *mach, m0_bcount_t space);
static struct m0_addb2_mach *mach(void);
static void mach_put(struct m0_addb2_mach *m);
static void mach_idle(struct m0_addb2_mach *m);
static void add(struct m0_addb2_mach *mach, uint64_t id, int n,
		const uint64_t *value);
static void pack(struct m0_addb2_mach *mach);
static uint64_t tag(uint8_t code, uint64_t id);
static void sensor_place(struct m0_addb2_mach *m, struct m0_addb2_sensor *s);
static void record_consume(struct m0_addb2_mach *m,
			   uint64_t id, int n, const uint64_t *value);

/**
 * Depths of machine context stack.
 */
#define MACH_DEPTH(mach) (mach->ma_rec.ar_label_nr)

void m0_addb2_push(uint64_t id, int n, const uint64_t *value)
{
	struct m0_addb2_mach *m = mach();

	if (m != NULL) {
		struct tentry         *e;
		struct m0_addb2_value *v;

		M0_PRE(MACH_DEPTH(m) < ARRAY_SIZE(m->ma_label));
		M0_PRE(n <= ARRAY_SIZE(e->e_value));
		M0_PRE(!m->ma_stopping);
		/*
		 * Note that add() must go before MACH_DEPTH(mach)++, because
		 * add() might allocate a new trace buffer and mach_buffer()
		 * will place all stacked labels to the buffer.
		 */
		add(m, tag(PUSH | n, id), n, value);
		MACH_DEPTH(m)++;
		e = mach_top(m);
		v = e->e_recval;
		v->va_id = id;
		v->va_nr = n;
		M0_ASSERT(v->va_data == e->e_value);
		memcpy(e->e_value, value, n * sizeof *value);
		mach_put(m);
	}
}

void m0_addb2_pop(uint64_t id)
{
	struct m0_addb2_mach *m = mach();

	if (m != NULL) {
		struct tentry          *e = mach_top(m);
		struct m0_addb2_sensor *s;

		M0_PRE(MACH_DEPTH(m) > 0);
		M0_PRE(!m->ma_stopping);
		M0_ASSERT_INFO(e->e_recval->va_id == id,
			       "Want: %"PRIx64" got: %"PRIx64".",
			       e->e_recval->va_id, id);

		m0_tl_teardown(sensor, &e->e_sensor, s) {
			sensor_place(m, s);
			s->s_ops->so_fini(s);
		}
		sensor_tlist_fini(&e->e_sensor);
		add(m, tag(POP, 0), 0, NULL);
		/* decrease the depth *after* add(), see m0_addb2_push(). */
		-- MACH_DEPTH(m);
		mach_put(m);
	}
}

void m0_addb2_add(uint64_t id, int n, const uint64_t *value)
{
	struct m0_addb2_mach *m = mach();

	if (m != NULL) {
		M0_PRE(n <= ARRAY_SIZE(m->ma_label[0].e_value));
		M0_PRE(!m->ma_stopping);

		add(m, tag(DATA | n, id), n, value);
		record_consume(m, id, n, value);
		mach_put(m);
	}
}

void m0_addb2_sensor_add(struct m0_addb2_sensor *s, uint64_t id, unsigned nr,
			 int idx, const struct m0_addb2_sensor_ops *ops)
{
	struct m0_addb2_mach *m = mach();

	M0_PRE(M0_IS0(s));
	M0_PRE(nr <= VALUE_MAX_NR);

	if (m != NULL) {
		struct tentry *te = idx < 0 ? mach_top(m) : &m->ma_label[idx];

		M0_PRE(MACH_DEPTH(m) > 0);
		M0_PRE(ergo(idx >= 0, idx < MACH_DEPTH(m)));
		M0_PRE(!m->ma_stopping);

		s->s_id  = id;
		s->s_nr  = nr;
		s->s_ops = ops;
		sensor_tlink_init_at_tail(s, &te->e_sensor);
		sensor_place(m, s);
		mach_put(m);
	}
}

void m0_addb2_sensor_del(struct m0_addb2_sensor *s)
{
	struct m0_addb2_mach *m = mach();

	if (m != NULL) {
		M0_PRE(!m->ma_stopping);

		if (sensor_tlink_is_in(s)) {
			sensor_place(m, s);
			sensor_tlist_del(s);
		}
		sensor_tlink_fini(s);
		mach_put(m);
	}
}

struct m0_addb2_mach *
m0_addb2_mach_init(const struct m0_addb2_mach_ops *ops, void *cookie)
{
	struct m0_addb2_mach *mach;

	M0_ALLOC_PTR(mach);
	if (mach != NULL) {
		int i;

		mach->ma_ops = ops;
		mach->ma_cookie = cookie;
		mach->ma_packed = m0_time_now();
		m0_mutex_init(&mach->ma_lock);
		m0_semaphore_init(&mach->ma_idlewait, 0);
		buf_tlist_init(&mach->ma_idle);
		buf_tlist_init(&mach->ma_busy);
		m0_addb2_source_init(&mach->ma_src);
		mach_tlink_init(mach);
#if DEBUG_OWNERSHIP
		strcpy(mach->ma_name, m0_thread_self()->t_namebuf);
#endif
		for (i = 0; i < ARRAY_SIZE(mach->ma_rec.ar_label); ++i) {
			struct tentry         *t = &mach->ma_label[i];
			struct m0_addb2_value *v = &mach->ma_rec.ar_label[i];

			v->va_data = t->e_value;
			t->e_recval = v;
			sensor_tlist_init(&t->e_sensor);
		}
		for (i = 0; i < BUFFER_MIN; ++i) {
			if (buffer_alloc(mach) != 0) {
				m0_addb2_mach_fini(mach);
				mach = NULL;
				break;
			}
		}
	}
	return mach;
}

void m0_addb2_mach_fini(struct m0_addb2_mach *mach)
{
	struct buffer *buf;

	M0_PRE(MACH_DEPTH(mach) == 0);
	/*
	 * This lock-unlock is a barrier against concurrently finishing last
	 * m0_addb2_trace_done(), which signalled ->apo_idle().
	 *
	 * It *is* valid to acquire and finalise a lock in the same function in
	 * this particular case.
	 */
	m0_mutex_lock(&mach->ma_lock);
	m0_mutex_unlock(&mach->ma_lock);

	if (mach->ma_cur != NULL)
		buffer_fini(mach->ma_cur);
	m0_tl_teardown(buf, &mach->ma_idle, buf) {
		buffer_fini(buf);
	}
	m0_addb2_source_fini(&mach->ma_src);
	mach_tlink_fini(mach);
	buf_tlist_fini(&mach->ma_idle);
	buf_tlist_fini(&mach->ma_busy);
	m0_mutex_fini(&mach->ma_lock);
	m0_semaphore_fini(&mach->ma_idlewait);
	m0_free(mach);
}

void m0_addb2_force(m0_time_t delay)
{
	struct m0_addb2_mach *m = mach();

	if (m != NULL) {
		if (delay == 0 || m0_time_is_in_past(m0_time_add(m->ma_packed,
								 delay))) {
			pack(m);
		}
		mach_put(m);
	}
}

static int addb2_force_loc_cb(void *unused)
{
	m0_addb2_force(M0_TIME_IMMEDIATELY);
	return 0;
}

static void addb2_force_loc(struct m0_locality *loc)
{
	m0_locality_call(loc, &addb2_force_loc_cb, NULL);
}

void m0_addb2_force_all(void)
{
	struct m0_locality *loc;
	struct m0_locality *loc_first;
	uint64_t            i;

	m0_addb2_force(M0_TIME_IMMEDIATELY);
	addb2_force_loc(m0_locality0_get());
	i = 0;
	loc = m0_locality_get(0);
	loc_first = loc;
	do {
		addb2_force_loc(loc);
		loc = m0_locality_get(++i);
	} while (loc != loc_first);
}

void m0_addb2_mach_stop(struct m0_addb2_mach *mach)
{
	mach->ma_stopping = true;
	pack(mach);
	m0_mutex_lock(&mach->ma_lock);
	mach_idle(mach);
	m0_mutex_unlock(&mach->ma_lock);
}

void m0_addb2_mach_wait(struct m0_addb2_mach *mach)
{
	M0_PRE(mach->ma_stopping);
	m0_semaphore_down(&mach->ma_idlewait);
}

void *m0_addb2_mach_cookie(const struct m0_addb2_mach *mach)
{
	return mach->ma_cookie;
}

void m0_addb2_trace_done(const struct m0_addb2_trace *ctrace)
{
	/* can safely discard const, because we passed this trace to
	   ->apo_submit(). */
	struct m0_addb2_trace *trace = (struct m0_addb2_trace *)ctrace;
	struct buffer         *buf   = M0_AMB(buf, trace, b_trace.o_tr);
	struct m0_addb2_mach  *mach  = buf->b_trace.o_mach;

	if (mach != NULL) { /* mach == NULL for a trace from network. */
		m0_mutex_lock(&mach->ma_lock);
		M0_PRE_EX(buf_tlist_contains(&mach->ma_busy, buf));
		M0_CNT_DEC(mach->ma_busy_nr);
		if (mach->ma_busy_nr + mach->ma_idle_nr +
		    !!(mach->ma_cur != NULL) <= BUFFER_MIN) {
			buf_tlist_move(&mach->ma_idle, buf);
			M0_CNT_INC(mach->ma_idle_nr);
			buf->b_trace.o_tr.tr_nr = 0;
		} else {
			buf_tlist_del(buf);
			buffer_fini(buf);
		}
		if (mach->ma_stopping)
			mach_idle(mach);
		m0_mutex_unlock(&mach->ma_lock);
	}
}

struct m0_addb2_source *m0_addb2_mach_source(struct m0_addb2_mach *m)
{
	return &m->ma_src;
}

M0_INTERNAL m0_bcount_t m0_addb2_trace_size(const struct m0_addb2_trace *trace)
{
	return sizeof trace->tr_nr + trace->tr_nr * sizeof trace->tr_body[0];
}

/* Consumer cursor */

void m0_addb2_cursor_init(struct m0_addb2_cursor *cur,
			  const struct m0_addb2_trace *trace)
{
	M0_PRE(M0_IS0(cur));

	cur->cu_trace = trace;
	m0_addb2_source_init(&cur->cu_src);
}

void m0_addb2_cursor_fini(struct m0_addb2_cursor *cur)
{
	m0_addb2_source_fini(&cur->cu_src);
}

int m0_addb2_cursor_next(struct m0_addb2_cursor *cur)
{
	struct m0_addb2_record *r = &cur->cu_rec;

	while (cur->cu_pos < cur->cu_trace->tr_nr) {
		uint64_t *addr  = &cur->cu_trace->tr_body[cur->cu_pos];
		uint64_t  datum = addr[0];
		m0_time_t time  = addr[1];
		uint64_t  tag   = datum >> (64 - 8);
		uint8_t   nr    = tag & 0xf;

		M0_CASSERT(VALUE_MAX_NR == 0xf);

		datum &= ~TAG_MASK;
		addr += 2;
		switch (tag) {
		case PUSH ... PUSH + VALUE_MAX_NR:
			if (r->ar_label_nr < ARRAY_SIZE(r->ar_label)) {
				r->ar_label[r->ar_label_nr ++] =
					(struct m0_addb2_value) {
					.va_id   = datum,
					.va_time = time,
					.va_nr   = nr,
					.va_data = addr
				};
				cur->cu_pos += nr + 2;
				continue;
			} else
				M0_LOG(M0_NOTICE, "Too many labels.");
			break;
		case POP:
			if (r->ar_label_nr > 0) {
				-- r->ar_label_nr;
				cur->cu_pos += 2;
				continue;
			} else {
				/**
				   WARN!
				   =====
				   Code below is inproper way of fixing
				   ADDB2 Underflow bug, which causes addb2
				   iterator to stop. Leaving initial code
				   as a reference here. Still this change is
				   needed in master branch for now as far as
				   it unblocks Mero and S3 team.

				 -       } else
				 -             M0_LOG(M0_NOTICE, "Underflow.");
				 +       } else {
				 +             M0_LOG(M0_WARN, "Underflow.");
				 +             return 0;
				 +       }

				   Negative implications from this patch:
				    - Some addb2 pages with be lost.
				      On the real HW during performance testing
				      it can be 2-3% of records.
				 */
				M0_LOG(M0_WARN, "Underflow.");
				return 0;
			}
			break;
		case DATA ... DATA + VALUE_MAX_NR:
		case SENSOR ... SENSOR + VALUE_MAX_NR:
			r->ar_val = (struct m0_addb2_value) {
				.va_id   = datum,
				.va_time = time,
				.va_nr   = nr,
				.va_data = addr
			};
			cur->cu_pos += nr + 2;
			if (datum == M0_AVI_NODATA) /* skip internal record */
				continue;
			m0_addb2_consume(&cur->cu_src, r);
			return +1;
		default:
			M0_LOG(M0_NOTICE, "Opcode: %"PRIx64".", datum);
		}
		return M0_ERR(-EPROTO);
	}
	return 0;
}

M0_INTERNAL struct m0_addb2_module *m0_addb2_module_get(void)
{
	return m0_get()->i_moddata[M0_MODULE_ADDB2];
}

int m0_addb2_module_init(void)
{
	struct m0_addb2_module *am;

	M0_ALLOC_PTR(am);
	if (am != NULL) {
		m0_get()->i_moddata[M0_MODULE_ADDB2] = am;
		m0_addb2__dummy_payload[0] = tag(DATA | 0, M0_AVI_NODATA);
		return 0;
	} else
		return M0_ERR(-ENOMEM);
}

void m0_addb2_module_fini(void)
{
	m0_free(m0_addb2_module_get());
}

/**
 * Returns current buffer with at least "space" bytes free.
 */
static struct buffer *cur(struct m0_addb2_mach *mach, m0_bcount_t space)
{
	struct buffer *buf = mach_buffer(mach);

	M0_PRE(space <= BUFFER_SIZE);

	if (buf != NULL && buffer_space(buf) < space)
		pack(mach);
	return mach_buffer(mach);
}

/**
 * Gets a trace buffer.
 *
 * If there is no current buffer, tries to use an idle buffer, if any. Allocates
 * a new buffer if there are no idle buffers.
 */
static struct buffer *mach_buffer(struct m0_addb2_mach *mach)
{
	if (mach->ma_cur == NULL) {
		m0_mutex_lock(&mach->ma_lock);
		if (mach->ma_idle_nr == 0) {
			if (mach->ma_busy_nr <= BUFFER_MAX)
				buffer_alloc(mach);
			else
				M0_LOG(M0_NOTICE, "Too many ADDB2 buffers.");
		}
		mach->ma_cur = buf_tlist_pop(&mach->ma_idle);
		/*
		 * Initialise the new current buffer: re-create current context
		 * by re-applying push operations and reading associated
		 * sensors.
		 */
		if (mach->ma_cur != NULL) {
			int i;
			int n = 0;

			M0_CNT_DEC(mach->ma_idle_nr);
			for (i = 0; i < MACH_DEPTH(mach) && n >= 0; ++i) {
				struct tentry          *e = &mach->ma_label[i];
				struct m0_addb2_value  *v = e->e_recval;
				struct m0_addb2_sensor *s;

				add(mach, tag(PUSH | v->va_nr, v->va_id),
				    v->va_nr, v->va_data);
				m0_tl_for(sensor, &e->e_sensor, s) {
					if (buffer_space(mach->ma_cur) <
					    SENSOR_THRESHOLD) {
						/*
						 * Too much space in the buffer
						 * is occupied by the
						 * sensors. Skip the rest of the
						 * sensors, they will be added
						 * to the next buffer.
						 */
						mach->ma_sensor_skip = n;
						n = -1;
						break;
					}
					if (n >= mach->ma_sensor_skip)
						sensor_place(mach, s);
					++n;
				} m0_tl_endfor;
			}
			if (n >= 0)
				/*
				 * Placed all the sensors, start from the
				 * beginning.
				 */
				mach->ma_sensor_skip = 0;
			mach->ma_cur->b_trace.o_force = false;
		}
		m0_mutex_unlock(&mach->ma_lock);
	}
	return mach->ma_cur;
}

/**
 * "Surrogate" current machine call-back used by UT.
 */
struct m0_addb2_mach *(*m0_addb2__mach)(void) = NULL;

/**
 * Returns current addb2 machine.
 *
 * Returned machine has m0_addb2_mach::ma_nesting elevated. mach_put() should be
 * called to release the machine.
 */
static struct m0_addb2_mach *mach(void)
{
	struct m0_thread_tls *tls  = m0_thread_tls();
	struct m0_addb2_mach *mach = tls != NULL ? tls->tls_addb2_mach : NULL;

	if (M0_FI_ENABLED("surrogate-mach") && m0_addb2__mach != NULL)
		mach = m0_addb2__mach();

	if (mach != NULL) {
		if (mach->ma_nesting > 0) {
			/* This is a normal situation, e.g., allocation of a new
			   buffer triggers m0_addb2_add() call in m0_alloc(). */
			M0_LOG(M0_DEBUG, "Recursive ADDB2 invocation.");
			mach = NULL;
		} else if (mach->ma_stopping)
			mach = NULL;
		else {
			++ mach->ma_nesting;
#if DEBUG_OWNERSHIP
			strcpy(mach->ma_last, m0_thread_self()->t_namebuf);
#endif
		}
	}
	M0_POST(ergo(mach != NULL, mach->ma_nesting == 1));
	return mach;
}

/**
 * Decreases the nesting counter of the machine, returned previously by mach().
 */
static void mach_put(struct m0_addb2_mach *m)
{
	M0_PRE(m->ma_nesting > 0);
	-- m->ma_nesting;
}

/**
 * Checks and signals if the machine is idle.
 */
static void mach_idle(struct m0_addb2_mach *m)
{
	if (m->ma_busy_nr == 0) {
		if (m->ma_ops->apo_idle != NULL)
			m->ma_ops->apo_idle(m);
		m0_semaphore_up(&m->ma_idlewait);
	}
}

/**
 * Adds a value (identifier and payload) to the current trace buffer.
 *
 * New buffer is allocated if there is no enough space in the current one.
 */
static void add(struct m0_addb2_mach *mach,
		uint64_t id, int n, const uint64_t *value)
{
	m0_time_t      now = m0_time_now();
	struct buffer *buf = cur(mach,
				 sizeof id + sizeof now + n * sizeof value[0]);

	if (buf != NULL) {
		buffer_add(buf, id);
		buffer_add(buf, now);
		while (n-- > 0)
			buffer_add(buf, *value++);
	}
}

/**
 * Adds a 64-bit value to a trace buffer.
 *
 * The buffer is assumed to have enough free space.
 */
static void buffer_add(struct buffer *buf, uint64_t datum)
{
	struct m0_addb2_trace *tr = &buf->b_trace.o_tr;

	M0_PRE(buffer_space(buf) >= sizeof datum);
	tr->tr_body[tr->tr_nr ++] = datum;
}

/**
 * Ships the current trace buffer off to processing.
 */
static void pack(struct m0_addb2_mach *m)
{
	if (m->ma_cur != NULL) {
		struct m0_addb2_trace_obj *o = &m->ma_cur->b_trace;
		bool                       wait;

		m0_mutex_lock(&m->ma_lock);
		o->o_force = m->ma_stopping;
		/*
		 * The buffer is on the busy list until m0_addb2_trace_done() is
		 * called.
		 */
		buf_tlist_add_tail(&m->ma_busy, m->ma_cur);
		M0_CNT_INC(m->ma_busy_nr);
		m0_mutex_unlock(&m->ma_lock);
		wait = m->ma_ops->apo_submit(m, o) > 0;
		m0_mutex_lock(&m->ma_lock);
		if (!wait) {
			/*
			 * The buffer was processed "instantly", can be re-used
			 * outright.
			 */
			buf_tlist_move(&m->ma_idle, m->ma_cur);
			M0_CNT_DEC(m->ma_busy_nr);
			M0_CNT_INC(m->ma_idle_nr);
			o->o_tr.tr_nr = 0;
		}
		m->ma_cur = NULL;
		m0_mutex_unlock(&m->ma_lock);
	}
	m->ma_packed = m0_time_now();
}

/**
 * Returns the number of free bytes in the buffer.
 */
static m0_bcount_t buffer_space(const struct buffer *buffer)
{
	m0_bcount_t used = buffer->b_trace.o_tr.tr_nr *
		sizeof buffer->b_trace.o_tr.tr_body[0];
	M0_PRE(BUFFER_SIZE >= used);
	return BUFFER_SIZE - used;
}

/**
 * Allocates new trace buffer.
 */
static int buffer_alloc(struct m0_addb2_mach *mach)
{
	struct buffer *buf;
	void          *area;

	M0_ALLOC_PTR(buf);
	area = m0_alloc(BUFFER_SIZE);
	if (buf != NULL && area != NULL) {
		buf->b_trace.o_tr.tr_body = area;
		buf->b_trace.o_mach = mach;
		buf_tlink_init_at_tail(buf, &mach->ma_idle);
		M0_CNT_INC(mach->ma_idle_nr);
		return 0;
	} else {
		M0_LOG(M0_NOTICE, "Cannot allocate ADDB2 buffer.");
		m0_free(buf);
		m0_free(area);
		return M0_ERR(-ENOMEM);
	}
}

static void buffer_fini(struct buffer *buf)
{
	buf_tlink_fini(buf);
	m0_free(buf->b_trace.o_tr.tr_body);
	m0_free(buf);
}

/**
 * Places the tag in the high 8-bits of the identifier.
 */
static uint64_t tag(uint8_t code, uint64_t id)
{
	M0_PRE((id & TAG_MASK) == 0);
	return id | (((uint64_t)code) << (64 - 8));
}

/**
 * Returns the top-most label in the context.
 */
static struct tentry *mach_top(struct m0_addb2_mach *m)
{
	M0_PRE(MACH_DEPTH(m) > 0);
	return &m->ma_label[MACH_DEPTH(m) - 1];
}

/**
 * Reads the sensor measurement and adds it to the current trace buffer.
 */
static void sensor_place(struct m0_addb2_mach *m, struct m0_addb2_sensor *s)
{
	int nr = s->s_nr;

	M0_PRE(s != NULL);
	M0_PRE(nr <= VALUE_MAX_NR);

	{
		uint64_t area[nr]; /* VLA! */

		s->s_ops->so_snapshot(s, area);
		add(m, tag(SENSOR | nr, s->s_id), nr, area);
		record_consume(m, s->s_id, nr, area);
	}
}

/**
 * Delivers a record (identifier, payload and the current context) to the online
 * CONSUMERS.
 */
static void record_consume(struct m0_addb2_mach *m,
			   uint64_t id, int n, const uint64_t *value)
{
	m->ma_rec.ar_val = (struct m0_addb2_value) {
		.va_id   = id,
		.va_nr   = n,
		.va_data = value
	};
	m0_addb2_consume(&m->ma_src, &m->ma_rec);
}

M0_INTERNAL uint64_t m0_addb2__dummy_payload[1] = {};

M0_INTERNAL uint64_t m0_addb2__dummy_payload_size =
	ARRAY_SIZE(m0_addb2__dummy_payload);

M0_INTERNAL void m0_addb2__mach_print(const struct m0_addb2_mach *m)
{
#if DEBUG_OWNERSHIP
	const char *orig = m->ma_name;
	const char *last = m->ma_last;
	M0_LOG(M0_FATAL, "mach: %p \"%s\" \"%s\".", m, orig, last);
#else
	M0_LOG(M0_FATAL, "mach: %p.", m);
#endif
}

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
