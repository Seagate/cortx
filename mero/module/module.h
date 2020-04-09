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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 8-Jan-2014
 */

#pragma once
#ifndef __MERO_MODULE_MODULE_H__
#define __MERO_MODULE_MODULE_H__

struct m0_module;

/**
 * @defgroup module
 *
 * A @em module is part of Mero, which can be initialised or finalised.
 * A module can correspond to a software module (e.g., lib/thread, reqh)
 * or to a run-time entity (reqh instance, service, stob domain, etc.).
 *
 * A module defines an ordered list of @em levels, corresponding to the
 * states of module initialisation. E.g., a reqh instance module might have
 * following levels:
 *
 * 0. m0_reqh structure initialised
 * 1. locality handler threads started
 * 2. BE started
 * 3. layout domain initialised
 * 4. ready to start services
 *
 * With a level are associated entry and exit functions, executed when
 * the level is entered and left.
 *
 * There are @em dependencies between (module, level) pairs.
 * A dependency
 *
 * @verbatim
 *         (m0, l0) -> (m1, l1)
 * @endverbatim
 *
 * means that the module m1 must be at least at the level l1 before the
 * module m0 can enter the level l0. For example there can be a dependency
 *
 * @verbatim
 *         (reqh_instance, locality_handler_threads_started) -> \
 *                 (lib_thread, initialised)
 * @endverbatim
 *
 * specifying that threading subsystem should be initialised before reqh
 * can start threads.
 *
 * It's important to understand that dependencies exist between (module,
 * level) pairs rather than between modules. Because of this, a potential
 * dependency cycle can be broken by introducing an additional level,
 * instead of a more expensive operation of introducing an additional
 * module.
 *
 * The collection of modules is not fixed. An entry function of a module
 * can add more modules and dependencies to the system. For example, one of
 * m0d levels fetches configuration information from confc. The list of
 * services to be run on this m0d is obtained from confc, and modules,
 * corresponding to the services are added, with appropriate dependencies.
 * After this m0d initialisation continues, taking new modules and
 * dependencies into account.
 *
 * Dependencies organize modules in an acyclic graph, which is not
 * necessarily a tree. Flexible initialisation is achieved by adding
 * special nodes in this graph. E.g., a unit test can introduce a module,
 * depending on subsystems that the test needs. To prepare for the test,
 * this module is initialised, starting subsystems. Another well-known
 * nodes in this graph are m0t1fs_instance and m0d_instance: first is a
 * module representing a m0t1fs mount point, depending on all subsystems
 * that m0t1fs needs. The latter represents an m0d process.
 *
 * As an example of a long(ish) chain of dependencies consider:
 *
 * 0. network starts
 * 1. rpc starts
 * 2. reqh starts
 * 3. confc starts
 * 4. reqh connects to confd
 * 5. confc is populated from confd
 * 6. stob identifier of seg0 is obtained from confc
 * 7. BE starts
 * 8. dtm starts
 * 9. reqh starts (other) services
 *
 * @{
 */

enum {
	M0_MODLEV_NONE = -1,
	M0_MODLEV_MAX  = 64,
	M0_MODDEP_MAX  = 128
};

/** Module level. */
struct m0_modlev {
	const char *ml_name;
	/**
	 * Entry function, executed before entering the level, after
	 * all dependencies are satisfied.
	 */
	int       (*ml_enter)(struct m0_module *module);
	/** Leave function, executed before leaving the level. */
	void      (*ml_leave)(struct m0_module *module);
};

/**
 * Dependency between a (module, level) pair.
 *
 * An { .md_other = m1, md_src = l0, md_dst = l1 } element in m0.dep[]
 * means that the module m1 must be at least at the level l1 before m0
 * can enter the level l0.
 */
struct m0_moddep {
	struct m0_module *md_other;
	int               md_src;
	int               md_dst;
};

#define M0_MODDEP_INIT(other, src, dst) \
	{ .md_other = (other), .md_src = (src), .md_dst = (dst) }

/**
 * Module.
 *
 * A module has an array of levels (m0_module::m_level, with entry and
 * exit functions), an array of dependencies (m0_module::m_dep) and an
 * array of inverse dependencies (m0_module::m_inv).
 *
 * If there is a (m0, l0) -> (m1, l1) dependency, then there is an
 * { .md_other = m1, md_src = l0, md_dst = l1 } element in m0.m_dep[] and an
 * { .md_other = m0, md_src = l0, md_dst = l1 } element in m1.m_inv[].
 */
struct m0_module {
	const char             *m_name;
	struct m0              *m_m0;
	/**
	 * Current level.
	 *
	 * This value is equal to M0_MODLEV_NONE iff the module is not
	 * initialised.
	 */
	int                     m_cur;
	/**
	 * Array of levels.
	 *
	 * @note The first entry of this list is never used, because
	 *       level 0 can be neither entered, nor left.
	 */
	const struct m0_modlev *m_level;
	int                     m_level_nr;
	/**
	 * ->m_level_nrefs[i] is equal to the number of dependencies that
	 * are currently relying on level i to be reached.  The level may
	 * not downgrade if this number is nonzero.
	 */
	unsigned                m_level_nrefs[M0_MODLEV_MAX];
	/** Array of dependencies. */
	struct m0_moddep        m_dep[M0_MODDEP_MAX];
	unsigned                m_dep_nr;
	/** Array of inverse dependencies. */
	struct m0_moddep        m_inv[M0_MODDEP_MAX];
	unsigned                m_inv_nr;
};

/** "Module factory". */
struct m0_module_type {
	/** Human-readable name (useful for debugging). */
	const char         *mt_name;
	/**
	 * Allocates and configures a module.
	 *
	 * Usually an ambient structure is allocated, which m0_module is
	 * embedded into. Then module levels are assigned and dependencies
	 * added. Module-specific data (e.g., the address of module) may be
	 * stored in m0::i_moddata[] or m0::i_lockers.
	 *
	 * @note  It is caller's responsibility to free any memory allocated
	 *        by the implementation and clear the corresponding cell in
	 *        m0::i_moddata[] or m0::i_lockers.
	 */
	struct m0_module *(*mt_create)(struct m0 *instance);
};

/**
 * Bring module at least to the given level.
 *
 * This function is not self-cleaning: even if it fails,
 * m0_module_fini() should be called.
 */
M0_INTERNAL int m0_module_init(struct m0_module *module, int level);

/** Downgrade the module to the given level. */
M0_INTERNAL void m0_module_fini(struct m0_module *module, int level);

/** Creates (m0, l0) -> (m1, l1) dependency. */
M0_INTERNAL void m0_module_dep_add(struct m0_module *m0, int l0,
				   struct m0_module *m1, int l1);

/** Performs initial configuration of m0_module fields. */
M0_INTERNAL void m0_module_setup(struct m0_module *module, const char *name,
				 const struct m0_modlev *level, int level_nr,
				 struct m0 *instance);

/** @} module */
#endif /* __MERO_MODULE_MODULE_H__ */

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
