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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 11-Jun-2013
 */

#pragma once

#ifndef __MERO_FOP_FOM_SIMPLE_H__
#define __MERO_FOP_FOM_SIMPLE_H__

/**
 * @defgroup fom
 *
 * A simple helper fom type to execute a user specified phase transition
 * function.
 *
 * "Simple fom" is pre-configured fom type to execute user-supplied code in fom
 * context.
 *
 * @{
 */

#include "fop/fom.h"

/**
 * Simple fom executes m0_fom_simple::si_tick() in each tick.
 *
 * Simplest m0_fom_simple::si_tick() is created by calling m0_fom_simple_post()
 * with NULL "conf" parameter. Such call initialises a fom with the following
 * trivial state machine:
 *
 * @verbatim
 *    +-----+
 *    |     |
 *    V     |
 *   INIT---+
 *    |
 *    |
 *    V
 *  FINISH
 * @endverbatim
 *
 * The useful work is done by m0_fom_simple::si_tick() function (initialised to
 * the "tick" argument of m0_fom_simple_post()) in INIT state. To terminate the
 * fom, m0_fom_simple::si_tick() should return a negative value.
 *
 * Typical m0_fom_simple::si_tick() function in such case would look like:
 *
 * @code
 * int foo_tick(struct m0_fom *fom, struct foo *f, int *__unused)
 * {
 *         switch (foo->f_subphase) {
 *         case SUBPHASE_0:
 *                 do_something_0(f);
 *                 foo->f_subphase = SUBPHASE_1;
 *                 return ready ? M0_FSO_AGAIN : M0_FSO_WAIT;
 *         case SUBPHASE_1:
 *                 do_something_1(f);
 *                 foo->f_subphase++;
 *                 return M0_FSO_WAIT;
 *         ...
 *         case SUBPHASE_N:
 *                 // terminate simple fom
 *                 return -1;
 *         }
 * }
 * @endcode
 *
 * That is, m0_fom_simple::si_tick() is very similar to m0_fom_ops::fo_tick(),
 * except that it gets an additional "data" parameter, originally passed by the
 * user to m0_fom_simple_post() and uses some fom-specific "subphase" instead of
 * fom phase.
 *
 * Passing a non-NULL conf to m0_fom_simple_post() creates a "semisimple" fom
 * with the user-supplied configuration. Such fom can use usual fom phases to
 * keep track of it current state. For a semisimple fom, current phase is passed
 * in "phase" parameter to m0_fom_simple::si_tick().
 *
 * A typical semisimple m0_fom_simple::si_tick() looks like:
 * @code
 * int foo_tick(struct m0_fom *fom, struct foo *f, int *phase)
 * {
 *         switch (*phase) {
 *         case PHASE_0:
 *                 do_something_0(f);
 *                 *phase = PHASE_1;
 *                 return ready ? M0_FSO_AGAIN : M0_FSO_WAIT;
 *         case PHASE_1:
 *                 do_something_1(f);
 *                 (*phase)++;
 *                 return M0_FSO_WAIT;
 *         ...
 *         case PHASE_N:
 *                 return -1;
 *         }
 * }
 * @endcode
 *
 * Note that the type of the second parameter can be different from void *, see
 * M0_FOM_SIMPLE_POST().
 */
struct m0_fom_simple {
	struct m0_fom      si_fom;
	int         (*si_tick)(struct m0_fom *fom, void *data, int *phase);
	/** User provided data, passed to ->si_tick(). */
	void              *si_data;
	/** User supplied locality. */
	size_t             si_locality;
	/** Embedded fom type for "semisimple" fom. */
	struct m0_fom_type si_type;
	/** Cleanup function pointer called by fom_simple_fini() */
	void             (*si_free)(struct m0_fom_simple *sfom);
};

enum {
	/**
	 * Pass this as "locality" argument to m0_fom_simple_post() to bind the
	 * fom to the current locality.
	 */
	M0_FOM_SIMPLE_HERE = 0xbedabedabedabeda
};

/**
 * Queues a simple fom.
 */
M0_INTERNAL void m0_fom_simple_post(struct m0_fom_simple *simpleton,
				    struct m0_reqh *reqh,
				    struct m0_sm_conf *conf,
				    int (*tick)(struct m0_fom *, void *, int *),
				    void (*free)(struct m0_fom_simple *sfom),
				    void *data, size_t locality);
/**
 * Starts an army of "nr" simple foms, queued to localities 0 .. (nr - 1).
 *
 * A fom thus created can query its locality in m0_fom_simple::si_locality.
 */
M0_INTERNAL void m0_fom_simple_hoard(struct m0_fom_simple *cat, size_t nr,
				     struct m0_reqh *reqh,
				     struct m0_sm_conf *conf,
				     int (*tick)(struct m0_fom *, void *,
						 int *),
				     void (*free)(struct m0_fom_simple *sfom),
				     void *data);

/**
 * Wrapper around m0_fom_simple_post() supporting flexible typing of "data".
 */
#define M0_FOM_SIMPLE_POST(s, r, c, t, f, d, l)			\
({									\
	/* check that "t" and "d" match. */				\
	(void)(sizeof((t)(NULL, (d), 0)));				\
	m0_fom_simple_post((s), (r), (c),				\
			   (int (*)(struct m0_fom *, void *, int *))(t),\
			   (void (*)(struct m0_fom_simple *))(f),       \
			   (void *)(d), (l));				\
})

M0_INTERNAL int m0_fom_simples_init(void);
M0_INTERNAL void m0_fom_simples_fini(void);

/** @} end of fom group */

#endif /* __MERO_FOP_FOM_SIMPLE_H__ */

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
