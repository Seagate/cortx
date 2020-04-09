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
 * Original creation date: 17-Mar-2015
 */

#pragma once

#ifndef __MERO_ADDB2_SYS_H__
#define __MERO_ADDB2_SYS_H__

/**
 * @defgroup addb2
 *
 * SYSTEM interface (detailed)
 * ---------------------------
 *
 * addb2 SYSTEM interface, defined and implemented in addb2/sys.[ch] provides a
 * "sys" object (m0_addb2_sys) that encapsulates a factory and a cache of addb2
 * machines, associated with network (m0_addb2_net) and storage
 * (m0_addb2_storage) back-ends.
 *
 * Sys objects are used to obtain fully functional addb2 machines. As addb2
 * PRODUCER interface is fully lockless, there should typically exist an addb2
 * machine per thread, which means that machines should be created very quickly.
 *
 * Sys object meets this requirement by keeping a cache of fully initialised
 * machines. New machine is requested by calling m0_addb2_sys_get() and returned
 * back by a call to m0_addb2_sys_put().
 *
 * An addb2 machine, produced by a sys object, has its trace processing
 * call-back (m0_addb2_mach_ops::apo_submit()) set to sys_submit(), which
 * dispatches record traces to the back-ends. Back-end processing is done via
 * asts (see sys_ast()), which allows ->apo_submit() to be called in any
 * context.
 *
 * Typically a running Mero process has two sys objects instantiated:
 *
 *     - a "fom" sys object for fom locality threads
 *       (m0_fom_domain::fd_addb2_sys). This cache contains an addb2 machine for
 *       each locality handler thread;
 *
 *     - a "global" sys instance used for all other threads. The pointer to the
 *       global sys instance is kept in m0_get()->i_moddata[M0_MODULE_ADDB2].
 *
 * Both sys objects are initialised early as part of m0_init(), but are not
 * fully functional until m0_reqh_addb2_init() is called. This function
 * associates both sys objects with a stob provided as a parameter and enables
 * back-end processing. Addb2 traces produced before m0_reqh_addb2_init() call
 * are queued in the sys objects (in m0_addb2_sys::sy_queue, up to a
 * configurable limit).
 *
 * Network back-end is used to process addb2 records when storage back-end is
 * not available. Currently this means that it is only relevant on a
 * client. Network back-end is activated by a call to
 * m0_addb2_sys_net_start_with() in m0t1fs_setup().
 *
 * @{
 */

/* import */
#include "lib/mutex.h"
#include "lib/types.h"
#include "lib/tlist.h"
#include "lib/semaphore.h"
#include "sm/sm.h"
struct m0_addb2_trace_obj;
struct m0_addb2_storage;
struct m0_addb2_net;
struct m0_thread;
struct m0_stob;

/* export */
struct m0_addb2_config;
struct m0_addb2_sys;

/**
 * Configuration of addb2 machine.
 *
 * This structure defines parameters of addb2 machines created by a sys object.
 */
struct m0_addb2_config {
	/**
	 * Trace buffer size. Currently ignored, the buffer size is hard-coded
	 * in addb2.c (BUFFER_SIZE).
	 */
	unsigned co_buffer_size;
	/**
	 * Minimal number of buffers in the machine. Currently ignored, the
	 * value is hard-coded in addb2.c (BUFFER_MIN).
	 */
	unsigned co_buffer_min;
	/**
	 * Maximal number of buffers in the machine. Currently ignored, the
	 * value is hard-coded in addb2.c (BUFFER_MAX).
	 */
	unsigned co_buffer_max;
	/**
	 * Maximal number of trace buffers that can be queued
	 * (m0_addb2_sys::sy_queue) in a sys object.
	 */
	unsigned co_queue_max;
	/**
	 * Minimal number of cached addb2 machines in a sys object.
	 */
	unsigned co_pool_min;
	/**
	 * Maximal number of cached addb2 machines in a sys object.
	 */
	unsigned co_pool_max;
};

/**
 * Initalises a sys object.
 *
 * The sys object will produce (m0_addb2_sys_get()) addb2 machines matching the
 * given configuration.
 */
int  m0_addb2_sys_init(struct m0_addb2_sys **sys,
		       const struct m0_addb2_config *conf);
void m0_addb2_sys_fini(struct m0_addb2_sys *sys);
/**
 * Gets addb2 machine.
 *
 * Either return cached addb2 machine or create new one, when the cache is
 * empty.
 */
struct m0_addb2_mach *m0_addb2_sys_get(struct m0_addb2_sys *sys);
/**
 * Returns the machine.
 *
 * The caller should not use the returned machine any longer. The machine is
 * either returned to the cache or destroyed. All context labels pushed by the
 * called on a machine obtained via m0_addb2_sys_get() should be popped before
 * the machine is returned.
 */
void m0_addb2_sys_put(struct m0_addb2_sys *sys, struct m0_addb2_mach *mach);

/**
 * Enables ASTs for back-end processing.
 */
void m0_addb2_sys_sm_start(struct m0_addb2_sys *sys);
/**
 * Disables ASTs for back-end processing.
 *
 * While processing is disabled, addb2 traces are accumulated in the sys object
 * queue (m0_addb2_sys:sy_queue). Once the queue overflows, further traces are
 * dropped on the floor.
 */
void m0_addb2_sys_sm_stop(struct m0_addb2_sys *sys);

/**
 * Enables network back-end processing.
 */
int  m0_addb2_sys_net_start(struct m0_addb2_sys *sys);
/**
 * Disables network back-end processing.
 */
void m0_addb2_sys_net_stop(struct m0_addb2_sys *sys);
/**
 * Enables network processing and initialises the list of outgoing services.
 *
 * "head" should contain a list of services (pools_common_svc_ctx_tlist).
 *
 * @see m0t1fs_setup().
 */
int  m0_addb2_sys_net_start_with(struct m0_addb2_sys *sys, struct m0_tl *head);
/**
 * Starts storage back-end processing.
 *
 * Location and key define the ADDB stob domain; mkfs and force rule
 * the domain's initialization process.
 * Size is the size of stob that stores traces.
 */
int  m0_addb2_sys_stor_start(struct m0_addb2_sys *sys, const char *location,
			     uint64_t key, bool mkfs, bool force,
			     m0_bcount_t size);
/**
 * Disables storage back-end processing.
 */
void m0_addb2_sys_stor_stop(struct m0_addb2_sys *sys);

/**
 * A function usable as m0_addb2_mach_ops::apo_submit() call-back.
 */
int m0_addb2_sys_submit(struct m0_addb2_sys *sys,
			struct m0_addb2_trace_obj *obj);
/**
 * Sets "sys" to use the same back-ends as "src".
 */
void m0_addb2_sys_attach(struct m0_addb2_sys *sys, struct m0_addb2_sys *src);
/**
 * Resets "sys" back-ends to NULL.
 */
void m0_addb2_sys_detach(struct m0_addb2_sys *sys);
/**
 * Registers an addb2 counter with the sys object.
 *
 * On the next occasion (i.e., sys AST execution), the counter will be added to
 * some locality. This function is useful to distribute global counters (not
 * logically bound to any locality) aming localities.
 */
void m0_addb2_sys_counter_add(struct m0_addb2_sys *sys,
			      struct m0_addb2_counter *counter, uint64_t id);


/** @} end of addb2 group */
#endif /* __MERO_ADDB2_SYS_H__ */

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
