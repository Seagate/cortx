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
 * Original author: Sergey Shilov <sergey.shilov@seagate.com>,
 *                  Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 10/17/2016
 */

#pragma once

#ifndef __MERO_FOP_FOM_INTERPOSE_H__
#define __MERO_FOP_FOM_INTERPOSE_H__

#include "fop/fom.h"

struct m0_fom_interpose_ops;

/**
 * A structure to modify the behaviour of a fom dynamically.
 *
 * Fom interposition allows fom phase transition logic, implemented in
 * m0_fom_ops::fo_tick() function, to be dynamically adjusted.
 *
 * This can be used to implement monitoring, profiling or structural
 * relationships between foms (e.g., master-slave).
 *
 * Multiple levels of interception can be applied to the same fom.
 */
struct m0_fom_interpose {
	struct m0_fom_ops                  fi_shim;
	const struct m0_fom_ops           *fi_orig;
	const struct m0_fom_interpose_ops *fi_ops;
};

/**
 * Functions to be executed around interposed fom tick.
 */
struct m0_fom_interpose_ops {
	/**
	 * Functions from this array are executed before the original fom tick
	 * is executed.
	 *
	 * A pre-function can return either normal tick return value (i.e.,
	 * FSO_AGAIN or FSO_WAIT) or a special INTERPOSE_CONT value. In the
	 * former case, the returned value is immediately returned as the tick
	 * result. Otherwise, original fom tick function is called.
	 */
	int (*io_pre [64])(struct m0_fom *fom, struct m0_fom_interpose *proxy);
	/**
	 * Functions from this array are executed after the original fom tick is
	 * executed.
	 *
	 * Post-functions take the result of the original tick as an additional
	 * parameter. Whatever is returned by the post-function is returned as
	 * the result of the tick.
	 */
	int (*io_post[64])(struct m0_fom *fom, struct m0_fom_interpose *proxy,
			   int result);
};

/**
 * A structure to implement the master-slave fom execution on top of
 * interposition.
 */
struct m0_fom_thralldom {
	/** Master fom, which calls slave fom and waits for its completion. */
	struct m0_fom           *ft_master;
	/**
	 * A structure which modifies the behaviour of a slave fom so that it
	 * wakes up master fom when finish phase is reached.
	 */
	struct m0_fom_interpose  ft_fief;
	/** Called when slave fom reaches finish state. */
	void                   (*ft_end)(struct m0_fom_thralldom *thrall,
					 struct m0_fom           *serf);
};

/**
 * Activates the interposition by substitution of original fom tick function
 * with interposition tick.
 */
M0_INTERNAL void m0_fom_interpose_enter(struct m0_fom           *fom,
					struct m0_fom_interpose *proxy);

/** Disables the interposition by restoring of original fom tick function. */
M0_INTERNAL void m0_fom_interpose_leave(struct m0_fom           *fom,
					struct m0_fom_interpose *proxy);


/**
 * Arranges for the master to be woken up once the serf reaches
 * M0_FOM_PHASE_FINISH, should be called by master fom.
 * It is up to the caller to make sure the serf is actually executing and that
 * the master goes to sleep.
 *
 * @note It is not required to initialise thrall context before call, all
 * context preparation is done inside of this function.
 */
M0_INTERNAL void m0_fom_enthrall(struct m0_fom *master, struct m0_fom *serf,
				 struct m0_fom_thralldom *thrall,
				 void (*end)(struct m0_fom_thralldom *thrall,
					     struct m0_fom           *serf));

#endif /* __MERO_FOP_FOM_INTERPOSE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
