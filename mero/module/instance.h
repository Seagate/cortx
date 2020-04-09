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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 18-Jan-2014
 */
#pragma once
#ifndef __MERO_MODULE_INSTANCE_H__
#define __MERO_MODULE_INSTANCE_H__

#include "module/module.h"         /* m0_module */
#include "lib/bitmap.h"            /* m0_bitmap */
#include "lib/lockers.h"           /* M0_LOCKERS__DECLARE */
#include "stob/module.h"           /* m0_stob_module */
#include "fdmi/module.h"           /* m0_fdmi_module */
#include "ioservice/storage_dev.h" /* m0_storage_devs */
#include "ut/stob.h"               /* m0_ut_stob_module */
#include "mero/process_attr.h"     /* m0_proc_attr */

struct m0_be_domain;

/**
 * @addtogroup module
 *
 * @{
 */

/** Identifiers of standard modules. */
enum m0_module_id {
	M0_MODULE_NET,
	M0_MODULE_UT,
	M0_MODULE_ADDB2,
	M0_MODULE_LOCALITY,
	M0_MODULE_HA,
	M0_MODULE_PROCESSOR,
	M0_MODULE_POOL,
	M0_MODULE_ISC,
	/* XXX ... more to come ... */
	M0_MODULE_NR
};

/* XXX TODO: s/inst/instance/ */
M0_LOCKERS__DECLARE(M0_INTERNAL, m0_inst, m0, 16);

struct m0_ha;
struct m0_ha_module;
struct m0_ha_note_handler;

/**
 * m0 instance.
 *
 * All "global" variables are accessible from this struct.
 *
 * Each module belongs to exactly one instance.
 *
 * m0 instance is allocated and initialised by external user.
 * There are three use cases:
 *
 * - test creating one or more m0 instances (only one currently, but
 *   multiple instances could be very convenient for testing);
 *
 * - mero/setup creating a single m0 instance for m0d;
 *
 * - m0t1fs creating a single m0 instance for kernel.
 */
struct m0 {
	/**
	 * Generation counter, modified each time a module or
	 * dependency is added. Used to detect when initialisation
	 * should re-start.
	 */
	uint64_t                   i_dep_gen;
	/** Module representing this instance. */
	struct m0_module           i_self;
	/* *
	 * Module-specific data (e.g. addresses) of standard modules.
	 *
	 * @see m0_module_id
	 */
	void                      *i_moddata[M0_MODULE_NR];
	/**
	 * Non-standard modules (i.e. those not mentioned in m0_module_id)
	 * may store their data here.
	 */
	struct m0_inst_lockers     i_lockers;
	/**
	 * List of m0_param_source-s, linked through m0_param_source::ps_link.
	 *
	 * @see m0_param_source_add(), m0_param_source_del()
	 */
	struct m0_tl               i_param_sources;

	struct m0_ha_module       *i_ha_module;
	struct m0_ha              *i_ha;
	/* * Link to the HA. It's used in Mero code. */
	struct m0_ha_link         *i_ha_link;
	struct m0_ha_note_handler *i_note_handler;
	struct m0_ha_fvec_handler *i_fvec_handler;

	/*
	 * XXX TODO: Get rid of the fields below. Use ->i_moddata[] or
	 * ->i_lockers.
	 */
	struct m0_fdmi_module      i_fdmi_module;
	struct m0_stob_module      i_stob_module;
	struct m0_stob_ad_module   i_stob_ad_module;
	struct m0_ut_stob_module   i_ut_stob_module;
	struct m0_poolmach_state  *i_pool_module;
	struct m0_storage_devs     i_storage_devs;
	/**
	 * Is true iff ioservice performs all operations with specific storage
	 * device M0_SDEV_CID_DEFAULT regardless of the cob_fid values.
	 * Such behaviour is used only by tests and should be removed.
	 * @see cs_storage_devs_init()
	 * @see m0_fid_convert_cob2linuxstob()
	 */
	bool                       i_storage_is_fake;
	bool                       i_reqh_uses_ad_stob;
	bool                       i_disable_addb2_storage;
	/** Key for ioservice cob domain */
	unsigned                   i_ios_cdom_key;
	/** Key for mdservice cob domain */
	unsigned                   i_mds_cdom_key;
	/** Key for active rec domain */
	unsigned                   i_actrec_dom_key;
	/** Process attributes - memory limits and core mask */
	struct m0_proc_attr        i_proc_attr;
};

/**.
 * Returns current m0 instance.
 *
 * In the kernel, there is only one instance. It is returned.
 *
 * In user space, the instance is created by m0d early startup code
 * and stored in thread-local storage. This instance is inherited by
 * threads (i.e., when a thread is created it gets the instance of the
 * creator and stores it in its TLS).
 *
 * Theoretically, user space Mero can support multiple instances in
 * the same address space.
 *
 * @post retval != NULL
 */
M0_INTERNAL struct m0 *m0_get(void);

/**
 * Stores given pointer in thread-local storage.
 *
 * @pre instance != NULL
 */
M0_INTERNAL void m0_set(struct m0 *instance);

/** Levels of m0 instance. */
enum {
	/**
	 * m0_param_source_add(), m0_param_source_del(),
	 * m0_param_get() may not be used before m0::i_self enters
	 * M0_LEVEL_INST_PREPARE or after it leaves it.
	 */
	M0_LEVEL_INST_PREPARE,
	/**
	* Layer subsystem non-reloaded for reconfigure Mero.
	*/
	M0_LEVEL_INST_QUIESCE_SYSTEM,
	M0_LEVEL_INST_ONCE,
	/*
	 * XXX DELETEME after the removal of m0_init() function and
	 * subsystem[] array in mero/init.c file.
	 */
	M0_LEVEL_INST_SUBSYSTEMS,
	/**
	 * The "fully initialised" level, which the users of m0
	 * instance should m0_module_init() it to.
	 *
	 * M0_LEVEL_INST_READY depends on a particular set of modules,
	 * according to the use case (UT, m0t1fs, m0d, etc.).
	 */
	M0_LEVEL_INST_READY
};

/*
 *  m0
 * +------------------------------+
 * | M0_LEVEL_INST_READY          |
 * +------------------------------+
 * | M0_LEVEL_INST_SUBSYSTEMS     |
 * +------------------------------+
 * | M0_LEVEL_INST_ONCE           |
 * +------------------------------+
 * | M0_LEVEL_INST_QUIESCE_SYSTEM |
 * +------------------------------+
 * | M0_LEVEL_INST_PREPARE        |
 * +------------------------------+
 */
M0_INTERNAL void m0_instance_setup(struct m0 *instance);


/** @} module */
#endif /* __MERO_MODULE_INSTANCE_H__ */
