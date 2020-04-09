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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 * Original creation date: 19-Mar-2012
 */
#pragma once
#ifndef __MERO_CONF_CONFD_H__
#define __MERO_CONF_CONFD_H__

#include "conf/cache.h"
#include "reqh/reqh_service.h"

/**
 * @page confd-fspec Configuration Service (confd)
 *
 * XXX FIXME: confd documentation is outdated.
 *
 * Configuration service (confd) is designed to work as a part of
 * user-space configuration service, driven by request handler and
 * provides a "FOP-based" interface for accessing Mero
 * configuration information stored in configuration db. Confd is run
 * within the context of a request handler.
 *
 * Confd pre-loads configuration values fetched from configuration db
 * in memory-based data cache to speed up confc requests.
 *
 * - @ref confd-fspec-data
 * - @ref confd-fspec-sub
 *   - @ref confd-fspec-sub-setup
 * - @ref confd-fspec-cli
 * - @ref confd-fspec-recipes
 *   - @ref confd-fspec-recipe1
 * - @ref confd_dfspec "Detailed Functional Specification"
 *
 * <hr> <!------------------------------------------------------------>
 * @section confd-fspec-data Data Structures
 *
 * - m0_confd --- represents configuration service instance registered
 *   in request handler, stores structures to perform caching,
 *   accesses to configuration db and handles configuration data
 *   requests.
 *
 * - m0_confd_cache -- represents an efficient, high concurrency,
 *   in-memory cache over the underlying database.
 *
 *   Members:
 * - m0_confd_cache::ca_db is a database environment to access
 *   configuration db.
 * - m0_confd_cache::ca_cache is a registry of cached configuration
 *   objects.
 *
 * Confd receives multiple configuration requests from confcs. Each
 * request is a FOP containing a "path" of a requested configuration
 * value to be retrieved from the configuration db. Confcs and confds
 * use RPC layer as a transport to send FOPs.
 *
 * The following FOPs are defined for confd (see conf/onwire.h):
 * - m0_conf_fetch --- configuration request;
 * - m0_conf_fetch_resp --- Confd's response to m0_conf_fetch;
 * - m0_conf_update --- Update request;
 * - m0_conf_update_resp --- Confd's response to m0_conf_update;
 *
 * <hr> <!------------------------------------------------------------->
 * @section confd-fspec-sub  Subroutines
 *
 * - m0_confd_register()  - registers confd service in the system.
 * - m0_confd_unregister() - unregisters confd service.
 *
 * <!------------------------------------------------------------------>
 * @subsection confd-fspec-sub-setup Initialization and termination
 *
 * Confd is initiated and put into operation by request handler logic,
 * after mero is started. Confd service should be registered in
 * request handler with m0_confd_register() call, where it has to
 * initialise own data structures and FOPs used for communication.
 *
 * Initial configuration database is manually created prior to startup.
 * Confd assumes that:
 * - configuration db is created before confd started;
 * - the schema of configuration database conforms to the expected
 *   schema.
 *
 * The following errors may occur while using the configuration db:
 * - db is empty or is in an unrecognized format;
 * - db schema does not conform to
 *   <a href="https://docs.google.com/a/seagate.com/document/d/1pwDAxlghAlBGZ2zd
 mDeGPYoxblIDuKGmHystGwFHD-A/view">
 *   HLD of Mero’s configuration database schema</a>;
 * - key is not found.
 *
 * While initialization process, confd has to preload internal cache
 * of configuration objects with their configuration values. It loads
 * entire configuration db into memory-based structures. Pre-loading
 * details can be found in @ref confd-lspec.
 *
 * Initialised confd may be eventually terminated by m0_confd_unregister()
 * in which confd has to finalise own data structures and FOPs.
 *
 * After a confd instance is started it manages configuration
 * database, its own internal cache structures and incoming
 * FOP-requests.
 *
 * <!------------------------------------------------------------------>
 * @section confd-fspec-cli Command Usage
 *
 * To configure confd from console, standard options described in
 * @ref m0d in cs_help() function are used.
 *
 * <hr> <!------------------------------------------------------------->
 * @section confd-fspec-recipes  Recipes
 *
 * @subsection confd-fspec-recipe1 Typical interaction between confc and confd
 *
 * Client sends a m0_conf_fetch FOP request to confd;
 *
 * Configuration service processes confc requests in
 * m0_fom_ops::fo_tick() function of m0_conf_fetch FOP request and
 * sends m0_conf_fetch_resp FOP back.
 *
 * @see confd_dfspec
 */

/**
 * @defgroup confd_dfspec Configuration Service (confd)
 * @brief Detailed Functional Specification.
 *
 * @see @ref confd-fspec
 *
 * @{
 */

extern struct m0_reqh_service_type m0_confd_stype;
extern const struct m0_bob_type m0_confd_bob;

/** Configuration server. */
struct m0_confd {
	/** Generic service. */
	struct m0_reqh_service d_reqh;

	/** Lock to protect configuration cache. */
	struct m0_mutex        d_cache_lock;
	/**
	 * Configuration cache.
	 * This configuration cache is used from m0_reqh::rh_confc::cc_cache.
	 * m0_reqh::rh_confc::cc_cache is populated on conf setup during
	 * m0d startup.
	 */
	struct m0_conf_cache  *d_cache;

	/** Magic value == M0_CONFD_MAGIC. */
	uint64_t               d_magic;
};

M0_INTERNAL int m0_confd_register(void);
M0_INTERNAL void m0_confd_unregister(void);

M0_INTERNAL int m0_confd_service_to_filename(struct m0_reqh_service *service,
					     char                  **filename);
/**
 * Allocates conf cache and populates it using provided configuration string.
 */
M0_INTERNAL int m0_confd_cache_create(struct m0_conf_cache **out,
				      struct m0_mutex       *cache_lock,
				      const char            *confstr);

/** Finalises conf cache, releases memory. */
M0_INTERNAL void m0_confd_cache_destroy(struct m0_conf_cache *cache);

/** @} confd_dfspec */
#endif /* __MERO_CONF_CONFD_H__ */
