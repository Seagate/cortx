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

#pragma once

#ifndef __MERO_ADDB2_ADDB2_H__
#define __MERO_ADDB2_ADDB2_H__

/**
 * @defgroup addb2 ADDB.2
 *
 * Temporary note
 * --------------
 *
 * addb2 is a new implementation of addb sub-system. It has simpler external
 * interface, simpler and smaller implementation. For the time being, addb and
 * addb2 co-exist. Both versions send data over the network and store it
 * separately. Gradually, existing addb users will be converted to addb2. Once
 * the conversion is finished, addb implementation will be removed and addb2
 * renamed to addb throughout. Then, this note will also be removed.
 *
 * Overview
 * --------
 *
 * "ADDB" stands for "Analytic and Diagnostic Data-Base". Purpose of addb2
 * sub-system is to collect information about system behaviour for online and
 * offline analysis. For the purpose of addb2 design description, the following
 * components are identified:
 *
 *     * PRODUCER is a part of system that produces addb2 records to describe
 *       its behaviour. Mero sub-system becomes a PRODUCER after it is
 *       instrumented with addb2 calls. addb2 is itself a PRODUCER;
 *
 *     * CONSUMER is a part of system that is interested in learning about
 *       system behaviour through addb2. There are two classes of CONSUMERS:
 *
 *         - online (or synchronous) CONSUMER gets addb2 records as soon as they
 *           are produced,
 *
 *         - offline (or asynchronous) CONSUMER gets addb2 records possibly much
 *           later;
 *
 *     * IMPLEMENTATION is the addb2 sub-system itself (the code is in addb2/
 *       sub-directory). IMPLEMENTATION accepts records from PRODUCERS and
 *       delivers them to CONSUMERS;
 *
 *     * SYSTEM is a code outside of addb2, which configures addb2 and specifies
 *       how addb2 records flow across the network and where they are stored for
 *       offline consumption.
 *
 * All addb2 records have the same structure: a record (struct m0_addb2_record)
 * is a "measurement", tagged with a set of "labels". Measurements and labels
 * have the same structure, described by struct m0_addb2_value: a 56-bit
 * identifier, plus a time-stamp, plus a (variable up to 15, possibly 0) number
 * of 64-bit data items, called "payload". To give a fictitious, but realistic
 * example of a record:
 *
 * @verbatim
 *                |  IDENTIFIER      TIMESTAMP     PAYLOAD[0]    PAYLOAD[1] ...
 *    ------------+------------------------------------------------------------
 *    MEASUREMENT |  IO_WAIT_TIME     851263.911   5117216ns
 *    LABEL-6     |  DEVICE_FID       849265.721   7             200
 *    LABEL-5     |  FOP_OBJ_FID      842124.732   10            23213
 *    LABEL-4     |  CLIENT_ID        842124.732   404
 *    LABEL-3     |  FOP_OP_CODE      842124.732   COB_READ
 *    LABEL-2     |  SERVICE           15216.215   IOSERVICE
 *    LABEL-1     |  CPU_CORE            521.100   3
 *    LABEL-0     |  NODE_ID             100.321   20
 * @endverbatim
 *
 * Labels describe the context, in which the measurement occurred. In this case
 * the context is a read operation on a device with the fid (7, 200), which is
 * part of a read operation on a cob with fid (10, 23213), sent by a client 404
 * to the ioservice on the server 20 and handled on the 3rd core.
 *
 * The measurement itself is read operation latency: 5117216ns.
 *
 * Time-stamps (in nanoseconds) record the time when the measurement was taken
 * or the context was entered.
 *
 * Measurements can be produced in two ways:
 *
 *     - one-time measurements, like above, explicitly added by m0_addb2_add()
 *       call;
 *
 *     - continuous measurements, produced by "sensors" (m0_addb2_sensor).
 *       IMPLEMENTATION periodically reads the sensors associated with the
 *       context.
 *
 * Examples (sans time-stamps) of records produced by a sensor would be
 *
 * @verbatim
 *                |  IDENTIFIER      PAYLOAD[0]    PAYLOAD[1]    PAYLOAD[2]
 *    ------------+---------------------------------------------------------
 *    MEASUREMENT |  FREE_MEMORY     16GB
 *    LABEL-0     |  NODE_ID         20
 * @endverbatim
 *
 * and
 *
 * @verbatim
 *                |  IDENTIFIER      PAYLOAD[0]    PAYLOAD[1]    PAYLOAD[2]
 *    ------------+---------------------------------------------------------
 *    MEASUREMENT |  IO_SIZES        4KB           10MB          4MB
 *    LABEL-0     |  DEVICE_FID      7             200
 *    LABEL-1     |  NODE_ID         20
 * @endverbatim
 *
 * In the latter case, data items of IO_SIZES measurement are minimal (4KB),
 * maximal (10MB) and average (4MB) IO sizes for the given device.
 *
 * IMPLEMENTATION is not interested in semantics of records. It is up to
 * PRODUCERS and CONSUMERS to agree on common values of identifiers and meaning
 * of payloads.
 *
 * PRODUCER interface
 * ------------------
 *
 * PRODUCER interface is used to submit addb2 records for consumption. An
 * important feature of this interface is a method used to compose contexts
 * (i.e., label sets) for records. This method must be
 *
 *     - flexible: contexts with variable number of labels with different
 *       payloads should be possible;
 *
 *     - modular: sub-systems should be able to contribute to the context
 *       without global coordination. For example, in the IO_WAIT_TIME example
 *       above, DEVICE_FID is added by the stob sub-system, which has no
 *       knowledge about ioservice.
 *
 * The interface is based on a notion of addb2 "machine" (m0_addb2_mach), which
 * is a locus, where context is incrementally built. A machine keeps track of
 * the current context, maintained as a stack. New label can be added to a
 * context by a call to m0_addb2_push() and the top-most label can be removed by
 * a call to m0_addb2_pop().
 *
 * A call to m0_addb2_add() adds a one-time measurement in the current context.
 *
 * A call to m0_addb2_sensor_add() adds sensor in the current context and
 * associates the sensor with the top label in the context. IMPLEMENTATION
 * periodically queries added sensors. A sensor is automatically de-activated
 * when the label the sensor is associated with is popped from the stack.
 *
 * An addb2 machine cannot, typically, be concurrently used, because concurrent
 * stack manipulations would make context meaningless.
 *
 * CONSUMER interface
 * ------------------
 *
 * CONSUMER interface is described in addb2/consumer.h
 *
 * SYSTEM interface
 * ----------------
 *
 * SYSTEM initialises addb2 machines, associates them with threads. It also
 * initialises network (see addb2/net.h) and storage (see addb2/storage.h)
 * components of IMPLEMENTATION.
 *
 * External entry-points of PRODUCER interface (m0_addb2_push(), m0_addb2_pop(),
 * m0_addb2_add(), m0_addb2_sensor_add(), etc.) all operate on the "current"
 * addb2 machine. IMPLEMENTATION assumes that the pointer to this machine is
 * stored in m0_thread_tls()->tls_addb2_mach (see addb2/addb2.c:mach()). If this
 * pointer is NULL, no operation is performed. It is up to SYSTEM to setup this
 * pointer in some or all threads.
 *
 * @note Currently, this pointer is set for all threads except for some
 * light-weight short-living threads like ones used to implement soft timers.
 *
 * @{
 */

/* import */
#include "lib/types.h"
#include "lib/time.h"
#include "lib/tlist.h"
#include "lib/misc.h"               /* ARRAY_SIZE */
#include "xcode/xcode_attr.h"

/* export */
struct m0_addb2_mach;
struct m0_addb2_sensor;
struct m0_addb2_trace;
struct m0_addb2_trace_obj;

/**
 * Adds a label to the current context.
 *
 * @param id    - label identifier
 * @param n     - number of 64-bit values in label payload
 * @param value - payload
 *
 * Possible uses:
 *
 * @code
 * m0_addb2_add(FOP_FID, 2, fid);
 * @endcode
 *
 * @pre (id & 0xff00000000000000ull) == 0
 */
void m0_addb2_push(uint64_t id, int n, const uint64_t *value);

/**
 * Removes the top-most label in the current context stack.
 *
 * @param id - label identifier
 *
 * @pre "id" must the identifier of the top-most label.
 * @pre (id & 0xff00000000000000ull) == 0
 */
void m0_addb2_pop (uint64_t id);

/**
 * Adds one-time measurement in the current context.
 *
 * @param id    - measurement identifier
 * @param n     - number of 64-bit values in measurement payload
 * @param value - payload
 *
 * @pre (id & 0xff00000000000000ull) == 0
 */
void m0_addb2_add (uint64_t id, int n, const uint64_t *value);

/**
 * Helper macro for m0_addb2_add().
 *
 * Possible uses:
 *
 * @code
 * M0_ADDB2_ADD(IO_COMPLETION, io->i_byte_count, io->i_rc, m0_time_now());
 * M0_ADDB2_ADD(RARE_BRANCH_TAKEN);
 * @endcode
 */
#define M0_ADDB2_ADD(id, ...)						\
	m0_addb2_add(id, ARRAY_SIZE(((uint64_t[]){ __VA_ARGS__ })),	\
		     (const uint64_t[]){ __VA_ARGS__ })

/**
 * Helper macro for m0_addb2_add().
 *
 * @see M0_ADDB2_ADD().
 */
#define M0_ADDB2_PUSH(id, ...)						\
	m0_addb2_push(id, ARRAY_SIZE(((uint64_t[]){ __VA_ARGS__ })),	\
		      (const uint64_t[]){ __VA_ARGS__ })

/**
 * Helper to use structures as payload.
 *
 * Use as:
 *
 * @code
 * m0_addb2_push(ID, M0_ADDB2_OBJ(foo));
 * m0_addb2_add(ID, M0_ADDB2_OBJ(foo));
 * @endcode
 *
 */
#define M0_ADDB2_OBJ(obj) ((sizeof *(obj))/sizeof(uint64_t)), (uint64_t *)(obj)

/**
 * Executes a statement within temporary context.
 */
#define M0_ADDB2_IN(id, stmnt, ...)		\
do {						\
	M0_ADDB2_PUSH(id , ##__VA_ARGS__);	\
	(stmnt);				\
	m0_addb2_pop(id);			\
} while (0)

/**
 * Sensor operations invoked by IMPLEMENTATION.
 */
struct m0_addb2_sensor_ops {
	/**
	 * IMPLEMENTATION calls this periodically to read sensor measurements.
	 *
	 * "area" points to a buffer, where ->so_snapshot() should write out its
	 * payload, no more than m0_addb2_sensor::s_nr elements.
	 */
	void (*so_snapshot)(struct m0_addb2_sensor *s, uint64_t *area);
	/**
	 * Invoked by IMPLEMENTATION when the sensor goes out of context, i.e.,
	 * when the label, which was top-most when the sensor was added, is
	 * popped.
	 */
	void (*so_fini)(struct m0_addb2_sensor *s);
};

/**
 * A sensor is "always ready" measurement that IMPLEMENTATION periodically reads
 * out to produce records for CONSUMERS.
 */
struct m0_addb2_sensor {
	const struct m0_addb2_sensor_ops *s_ops;
	/** Linkage in a list of all sensors associated with a given label. */
	struct m0_tlink                   s_linkage;
	/** Sensor identifier. */
	uint64_t                          s_id;
	/** Number of 64-bit elements in sensor payload. */
	unsigned                          s_nr;
	uint64_t                          s_magix;
};

/**
 * Adds a sensor to the current context. This sensor will be periodically
 * queried (at the IMPLEMENTATION discretion) until it goes out of context.
 *
 * "idx" is used to specify to which label in the context to add the sensor. -1
 * means the topmost label, otherwise it is the level of label in the context
 * (starting from 0).
 *
 * @pre context stack must be non-empty.
 */
void m0_addb2_sensor_add(struct m0_addb2_sensor *s, uint64_t id, unsigned nr,
			 int idx, const struct m0_addb2_sensor_ops *ops);
/**
 * Deletes a sensor.
 */
void m0_addb2_sensor_del(struct m0_addb2_sensor *s);

/**
 * Forces current addb2 machine to send its collection of records for
 * processing (to storage or network), iff it was last sent packing more than
 * given delay ago.
 */
void m0_addb2_force(m0_time_t delay);

void m0_addb2_force_all(void);

/**
 * Machine operations vector provided by SYSTEM.
 *
 * @see fop/fom.c:addb2_ops.
 */
struct m0_addb2_mach_ops {
	/**
	 * Invoked by IMPLEMENTATION to send a collection of records to network
	 * or storage (at SYSTEM discretion).
	 *
	 * @retval +ve - the trace was accepted for
	 * processing. m0_addb2_trace_done() must be called against this trace
	 * eventually.
	 *
	 * @retval 0 - the trace was "processed instantly", IMPLEMENTATION is
	 * free to re-use trace after return. Submission errors are included in
	 * this case.
	 *
	 * see fop/fom.c:loc_addb2_submit().
	 */
	int (*apo_submit)(struct m0_addb2_mach *mach,
			  struct m0_addb2_trace_obj *tobj);
	/**
	 * Invoked by IMPLEMENTATION to notify SYSTEM that stopped machine
	 * completed its operation and can be finalised.
	 */
	void (*apo_idle)(struct m0_addb2_mach *mach);
};

/**
 * Notifies the IMPLEMENTATION that the trace has been processed (i.e., sent
 * across network or stored on storage).
 *
 * This is invoked internally from addb2 network and storage components.
 */
void m0_addb2_trace_done(const struct m0_addb2_trace *trace);

/**
 * Allocates and initialises an addb2 machine.
 *
 * The machine initially has an empty context stack and is immediately ready to
 * accept m0_addb2_push() and m0_addb2_add() calls. Whenever new record is added
 * to the machine it is immediately delivered to online CONSUMERS.
 *
 * The machine accumulates context state and records internally. When amount of
 * accumulated data reaches some threshold, the machine invokes
 * m0_addb2_mach_ops::apo_submit() to submit record data to network or storage
 * (as defined by SYSTEM).
 *
 * @param cookie - an opaque pointer, returned by m0_addb2_mach_cookie().
 */
struct m0_addb2_mach *m0_addb2_mach_init(const struct m0_addb2_mach_ops *ops,
					 void *cookie);
/**
 * Finalises a machine.
 *
 * @pre The machine must be idle.
 */
void m0_addb2_mach_fini(struct m0_addb2_mach *mach);

/**
 * Notifies IMPLEMENTATION that SYSTEM wants to shut the machine down.
 *
 * Once stopped, the machine stops accepting new records and context changes
 * (appropriate functions become no-ops) and submits all pending record data.
 *
 * When all pending record data have been processed, the machine becomes idle
 * and m0_addb2_mach_ops::apo_idle() is invoked. An idle machine can be
 * finalised.
 */
void m0_addb2_mach_stop(struct m0_addb2_mach *mach);

/**
 * Waits until stopped machine becomes idle.
 */
void m0_addb2_mach_wait(struct m0_addb2_mach *mach);

/**
 * Returns cookie passed to m0_addb2_mach_init().
 *
 * This can be used to associate a machine with some ambient SYSTEM object.
 */
void *m0_addb2_mach_cookie(const struct m0_addb2_mach *mach);

int m0_addb2_module_init(void);
void m0_addb2_module_fini(void);

/* Internal interface. */

/**
 * A trace is a representation of a sequence of addb2 records produced in the
 * same machine.
 */
struct m0_addb2_trace {
	uint64_t  tr_nr;
	uint64_t *tr_body;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
 * A wrapper object used to pass traces to network and storage components.
 */
struct m0_addb2_trace_obj {
	/** Trace itself. */
	struct m0_addb2_trace o_tr;
	/** Linkage into networking or storage queues. */
	struct m0_tlink       o_linkage;
	/** Pointer to the machine in which the trace was generated. */
	struct m0_addb2_mach *o_mach;
	/**
	 * Completion call-back.
	 *
	 * This is called by the processing (storage or network) after the trace
	 * has been processed (see frame_done()).
	 *
	 * @note If this is not defined, the standard call-back
	 * m0_addb2_trace_done() is invoked.
	 */
	void                (*o_done)(struct m0_addb2_trace_obj *obj);
	/** Push this trace through to the consumers as soon as possible,
	    bypassing caches. */
	bool                  o_force;
	uint64_t              o_magix;
};

/** @} end of addb2 group */
#endif /* __MERO_ADDB2_ADDB2_H__ */

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
