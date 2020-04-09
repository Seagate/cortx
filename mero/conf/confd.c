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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 *                  Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 19-Mar-2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/confd.h"
#include "conf/obj_ops.h"  /* m0_conf_obj_find */
#include "conf/onwire.h"   /* m0_confx, m0_confx_obj */
#include "conf/preload.h"  /* m0_confx_free */
#include "conf/dir.h"      /* m0_conf_dir_tl */
#include "lib/errno.h"     /* ENOMEM */
#include "lib/memory.h"    /* M0_ALLOC_PTR */
#include "lib/string.h"    /* m0_strdup */
#include "lib/fs.h"        /* m0_file_read */
#include "mero/magic.h"    /* M0_CONFD_MAGIC */
#include "mero/setup.h"    /* m0_reqh_context */

/**
 * @page confd-lspec-page confd Internals
 *
 * XXX FIXME: confd documentation is outdated.
 *
 * - @ref confd-depends
 * - @ref confd-highlights
 * - @ref confd-lspec
 *   - @ref confd-lspec-state
 *   - @ref confd-lspec-long-lock
 *   - @ref confd-lspec-thread
 *   - @ref confd-lspec-numa
 * - @ref confd-conformance
 * - @ref confd-ut
 * - @ref confd-O
 * - @ref confd_dlspec "Detailed Logical Specification"
 *
 * @section confd-depends Dependencies
 *
 * Confd depends on the following subsystems:
 * - @ref rpc_service <!-- rpc/service.h -->
 * - @ref db  <!-- db/db.h -->
 * - @ref fom <!-- fop/fom.h -->
 * - @ref fop <!-- fop/fop.h -->
 * - @ref reqh <!-- reqh/reqh.h -->
 * - @ref m0d <!-- mero/setup.h -->
 * - m0_reqh_service_type_register()  <!--reqh/reqh_service.h -->
 * - m0_reqh_service_type_unregister() <!--reqh/reqh_service.h -->
 * - @ref m0_long_lock_API <!-- fop/fom_long_lock.h -->
 *
 * Most important functions, confd depends on, are listed above:
 * - RPC layer:
 *   - m0_rpc_reply_post() used to send FOP-based reply to Confc;
 * - DB layer:
 *   - m0_db_pair_setup() and m0_table_lookup() used to access
 *     configuration values stored in db.
 * - FOP, FOM, REQH:
 *   - m0_fom_block_at();
 *   - m0_fom_block_leave();
 *   - m0_fom_block_enter();
 *   - m0_long_read_lock();
 *   - m0_long_write_lock().
 * - Mero setup:
 *   - m0_cs_setup_env() configures Mero to use confd's environment.
 *
 * <hr> <!------------------------------------------------------------->
 * @section confd-highlights Design Highlights
 *
 * - User-space implementation.
 * - Provides a "FOP-based" interface for confc to access configuration
 *   information.
 * - Relies on request handler threading model and is driven by
 *   reqh. Request processing is based on FOM execution.
 * - Maintains its own configuration cache, implementation of which is
 *   common to confd and confc.
 * - Several confd state machines (FOMs) processing requests from
 *   configuration consumers can work with configuration cache
 *   concurrently.
 *
 * <hr> <!------------------------------------------------------------->
 * @section confd-lspec Logical Specification
 *
 * Confd service initialization is performed by request handler. To
 * allocate Confd service and its internal structures in memory
 * m0_confd_service_locate() is used.
 *
 * Confd service type is registered in `subsystem' data structure of
 * "mero/init.c", the following lines are added:
 * @code
 * struct init_fini_call subsystem[] = {
 *      ...
 *	{ &m0_confd_register, &m0_confd_unregister, "confd" },
 *      ...
 * };
 * @endcode
 *
 * Configuration cache pre-loading procedure traverses all tables of
 * configuration db. Since relations between neighbour levels only are
 * possible, tables of higher "levels of DAG" are processed first.
 * The following code example presents pre-loading in details:
 *
 * @code
 * conf_cache_preload (...)
 * {
 *    for each record in the "profiles" table do
 *      ... allocate and fill struct m0_conf_profile from p_prof
 *    endfor
 *
 *    for table in "file_systems", "services",
 *              "nodes", "nics", "storage_devices",
 *              in the order specified, do
 *       for each record in the table, do
 *         ... allocate and fill struct m0_conf_obj ...
 *         ... create DAG struct m0_conf_relation to appropriate conf object ...
 *       endfor
 *    end for
 * }
 * @endcode
 *
 * FOP operation vector, FOP type, and RPC item type have to be defined
 * for each FOP. The following structures are defined for m0_conf_fetch FOP:
 *
 * - struct m0_fop_type m0_conf_fetch_fopt --- defines FOP type;
 * - struct m0_fop_type_ops m0_conf_fetch_ops --- defines FOP
 *   operation vector;
 * - struct m0_rpc_item_type m0_rpc_item_type_fetch --- defines RPC
 *   item type.
 *
 * m0_fom_fetch_state() - called by reqh to handle incoming
 * confc requests. Implementation of this function processes all
 * FOP-FOM specific and m0_conf_fetch_resp phases:
 * @code
 * static int m0_fom_fetch_state(struct m0_fom *fom)
 *  {
 *       checks if FOM should transition into a generic/standard
 *       phase or FOP specific phase.
 *
 *       if (fom->fo_phase < FOPH_NR) {
 *               result = m0_fom_state_generic(fom);
 *       } else {
 *              ... process m0_conf_fetch_resp phase transitions ...
 *       }
 *  }
 * @endcode
 *
 * Request handler triggers user-defined functions to create FOMs for
 * processed FOPs. Service has to register FOM-initialization functions
 * for each FOP treated as a request:
 *   - m0_conf_fetch;
 *   - m0_conf_update;
 *
 * To do so, the appropriate structures and functions have to be
 * defined. For example the following used by m0_conf_fetch FOP:
 *
 * @code
 * static const struct m0_fom_type_ops fom_fetch_type_ops = {
 *       .fto_create = fetch_fom_create
 * };
 *
 * struct m0_fom_type m0_fom_fetch_mopt = {
 *       .ft_ops = &fom_fetch_type_ops
 * };
 *
 * static int fetch_fom_create(struct m0_fop *fop, struct m0_fom **m,
 *                             struct m0_reqh *reqh)
 * {
 *    1) allocate fom;
 *    2) m0_fom_init(fom, &m0_fom_ping_mopt, &fom_fetch_type_ops,
 *                   fop, NULL, reqh);
 *    3) *m = fom;
 * }
 * @endcode
 *
 * The implementation of m0_fom_fetch_state() needs the following
 * functions to be defined:
 *
 * - fetch_check_request(), update_check_request() - check incoming
 *     request and validates requested path of configuration objects.
 *
 * - fetch_next_state(), update_next_state() - transit FOM phases
 *   depending on the current phase and on the state of configuration
 *   objects.
 *
 * - obj_serialize() - serializes given object to FOP.
 *
 * - fetch_failure_handle(), update_failure_handle() - handle occurred
 *   errors.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confd-lspec-state State Specification
 *
 * Confd as a whole is not a state machine, phase processing is
 * implemented on basis of FOM of m0_conf_fetch, m0_conf_update FOPs.
 * After corresponding FOM went through a list of FOM specific phases
 * it transited into F_INITIAL phase.
 *
 * The number of state machine instances correspond to the number of
 * FOPs being processed in confd.
 *
 * m0_conf_fetch FOM state transition diagram:
 * @dot
 *  digraph conf_fetch_phase {
 *      node [fontsize=9];
 *      edge [fontsize=9];
 *      F_INITIAL [style=filled, fillcolor=lightgrey];
 *      F_SERIALISE;
 *      F_TERMINATE [style=filled, fillcolor=lightgrey];
 *      F_FAILURE [style=filled, fillcolor=lightgrey];
 *
 *      F_INITIAL -> F_SERIALISE [label=
 *      "m0_long_read_lock(m0_confd::d_cache::ca_rwlock)"];
 *
 *      F_SERIALISE -> F_TERMINATE [label = "success"];
 *      F_SERIALISE -> F_FAILURE [label = "failure"];
 *  }
 * @enddot
 *
 * - F_INITIAL
 *   In this phase, incoming FOM/FOP-related structures are being
 *   initialized and FOP-processing preconditions are being
 *   checked. Then, an attempt is made to obtain a read lock
 *   m0_confd::d_cache::ca_rwlock. When it's obtained then
 *   m0_long_lock logic transits FOM back into F_SERIALISE.
 *
 * - F_SERIALISE:
 *   Current design assumes that data is pre-loaded into configuration
 *   cache. In F_SERIALISE phase, m0_confd::d_cache::ca_rwlock lock has
 *   been already obtained as a read lock.
 *   m0_conf_fetch_resp FOP is being prepared for sending by looking
 *   up requested path in configuration cache and unlocking
 *   m0_confd::d_cache::ca_rwlock.  After that, m0_conf_fetch_resp FOP
 *   is sent with m0_rpc_reply_post().  fetch_next_state() transits FOM into
 *   F_TERMINATE. If incoming request consists of a path which is not
 *   in configuration cache, then the m0_conf_fetch FOM is
 *   transitioned to the F_FAILURE phase.
 *
 * - F_TERMINATE:
 *   In this phase, statistics values are being updated in
 *   m0_confd::d_stat. m0_confd::d_cache::ca_rwlock has to be
 *   unlocked.
 *
 * - F_FAILURE:
 *   In this phase, statistics values are being updated in
 *   m0_confd::d_stat.
 *   m0_conf_fetch_resp FOP with an empty configuration objects
 *   sequence and negative error code is sent with m0_rpc_reply_post().
 *   m0_confd::d_cache::ca_rwlock has to be unlocked.
 *
 *  @note m0_conf_stat FOM has a similar state diagram as
 *  m0_conf_fetch FOM does and hence is not illustrated here.
 *
 *  m0_conf_update FOM state transition diagram:
 * @dot
 *  digraph conf_update_phase {
 *      node [fontsize=9];
 *      edge [fontsize=9];
 *      U_INITIAL [style=filled, fillcolor=lightgrey];
 *      U_UPDATE;
 *      U_TERMINATE [style=filled, fillcolor=lightgrey];
 *      U_FAILURE [style=filled, fillcolor=lightgrey];
 *
 *      U_INITIAL -> U_UPDATE [label=
 *      "m0_long_write_lock(m0_confd::d_cache::ca_rwlock)"];
 *
 *      U_UPDATE -> U_TERMINATE [label = "success"];
 *      U_UPDATE -> U_FAILURE [label = "failure"];
 *  }
 * @enddot
 *
 * - U_INITIAL:
 *   In this phase, incoming FOM/FOP-related structures are being
 *   initialized and FOP-processing preconditions are being
 *   checked. Then, an attempt is made to obtain a write lock
 *   m0_confd::d_cache::ca_rwlock. When it's obtained then
 *   m0_long_lock logic transits FOM back into U_UPDATE.
 *
 * - U_UPDATE:
 *   In current phase, m0_confd::d_cache::ca_rwlock lock has been
 *   already obtained as a write lock. Then, configuration cache has
 *   to be updated and m0_confd::d_cache::ca_rwlock lock should be
 *   unlocked.  After that, m0_conf_update_resp FOP is sent with
 *   m0_rpc_reply_post(). update_next_state() transits FOM into
 *   U_TERMINATE.  If incoming request consists of a path which is not
 *   in configuration cache than the m0_conf_fetch FOM is transitioned
 *   to the U_FAILURE phase
 *
 * - U_TERMINATE:
 *   In this phase, statistics values are being updated in
 *   m0_confd::d_stat. m0_confd::d_cache::ca_rwlock has to be
 *   unlocked.
 *
 * - U_FAILURE:
 *   In this phase, statistics values are being updated in
 *   m0_confd::d_stat.
 *   m0_conf_update_resp FOP with an empty configuration objects
 *   sequence and negative error code is sent with m0_rpc_reply_post().
 *   m0_confd::d_cache::ca_rwlock has to be unlocked.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confd-lspec-long-lock Locking model
 *
 * Confd relies on a locking primitive integrated with FOM signaling
 * mechanism. The following interfaces are used:
 *
 * @code
 * bool m0_long_{read,write}_lock(struct m0_longlock *lock,
 *                                struct m0_fom *fom, int next_phase);
 * void m0_long_{read,write}_unlock(struct m0_longlock *lock);
 * bool m0_long_is_{read,write}_locked(struct m0_longlock *lock);
 * @endcode
 *
 * m0_long_{read,write}_lock() returns true iff the lock is
 * obtained. If the lock is not obtained (i.e. the return value is
 * false), the subroutine would have arranged to awaken the FOM at the
 * appropriate time to retry the acquisition of the lock.  It is
 * expected that the invoker will return M0_FSO_AGAIN from the state
 * function in this case.
 *
 * m0_long_is_{read,write}_locked() returns true iff the lock has been
 * obtained.
 *
 * The following code example shows how to perform a transition from
 * F_INITIAL to F_SERIALISE and obtain a lock:
 * @code
 * static int fom_fetch_state(struct m0_fom *fom)
 * {
 *      //...
 *      struct m0_long_lock_link *link;
 *      if (fom->fo_phase == F_INITIAL) {
 *              // Initialise things.
 *              // ...
 *              // Retreive long lock link from derived FOM object: link = ...;
 *              // and acquire the lock
 *              return M0_FOM_LONG_LOCK_RETURN(m0_long_read_lock(lock,
 *                                                               link,
 *                                                               F_SERIALISE));
 *	}
 *	//...
 * }
 * @endcode
 * @see fom-longlock <!-- @todo fom-longlock has to be defined in future -->
 *
 * <hr> <!------------------------------------------------------------>
 * @section confd-lspec-thread Threading and Concurrency Model
 *
 * Confd creates no threads of its own but instead is driven by the
 * request handler. All threading and concurrency is being performed
 * on the Request Handler side, registered in the system.  Incoming
 * FOPs handling, phase transitions, outgoing FOPs serialization,
 * error handling is done in callbacks called by reqh-component.
 *
 * Configuration service relies on rehq component threading model and
 * should not acquire any locks or be in any waiting states, except
 * listed below. Request processing should be performed in an
 * asynchronous-like manner. Only synchronous calls to configuration
 * DB are allowed which should be bracketed with m0_fom_block_{enter,leave}().
 *
 * Multiple concurrently executing FOMs share the same configuration
 * cache and db environment of confd, so access to them is
 * synchronized with the specialized m0_longlock read/write lock
 * designed for use in FOMs: the FOM does not busy-wait, but gets
 * blocked until lock acquisition can be retried. Simplistic
 * synchronization of the database and in-memory cache through means
 * of this read/writer lock (m0_confd::d_lock) is sufficient, as the
 * workload of confd is predominantly read-only.
 *
 * @subsection confd-lspec-numa NUMA Optimizations
 *
 * Multiple confd instances can run in the system, but no more than
 * one per request handler. Each confd has its own data-base back-end
 * and its own pre-loaded copy of data-base in memory.
 *
 * <hr> <!------------------------------------------------------------->
 * @section confd-conformance Conformance
 *
 * - @b i.conf.confd.user
 *   Confd is implemented in user space.
 * - @b i.conf.cache.data-model
 *   Configuration information is organized as outlined in section 4.1
 *   of the HLD. The same data structures are used for confc and
 *   confd.  Configuration structures are kept in memory.
 * - @b i.conf.cache.unique-objects
 *   A registry of cached objects (m0_conf_cache::ca_registry) is used
 *   to achieve uniqueness of configuration object identities.
 *
 * <hr> <!------------------------------------------------------------->
 * @section confd-ut Unit Tests
 *
 * @test obj_serialize() will be tested.
 * @test {fetch,update}_next_state() will be tested.
 *
 * @test Load predefined configuration object from configuration db.
 * Check its predefined value.
 *
 * @test Load predefined configuration directory from db. Check theirs
 * predefined values.
 *
 * @test Fetch non-existent configuration object from configuration db.
 *
 * <hr> <!------------------------------------------------------------->
 * @section confd-O Analysis
 *
 * Size of configuration cache, can be evaluated according to a number
 * of configuration objects in configuration db and is proportional to
 * the size of the database file
 *
 * Configuration request FOP (m0_conf_fetch) is executed in
 * approximately constant time (measured in disk I/O) because the
 * entire configuration db is cached in-memory and rarely would be
 * blocked by an update.
 *
 * @see confd_dlspec
 */

/**
 * @defgroup confd_dlspec confd Internals
 *
 * @see @ref conf, @ref confd-lspec "Logical Specification of confd"
 *
 * @{
 */

static int confd_allocate(struct m0_reqh_service **service,
			  const struct m0_reqh_service_type *stype);
static int confd_cache_preload(struct m0_conf_cache *cache, const char *str);

const struct m0_bob_type m0_confd_bob = {
	.bt_name         = "m0_confd",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_confd, d_magic),
	.bt_magix        = M0_CONFD_MAGIC
};

static const struct m0_reqh_service_type_ops confd_stype_ops = {
	.rsto_service_allocate = confd_allocate
};

struct m0_reqh_service_type m0_confd_stype = {
	.rst_name     = "M0_CST_CONFD",
	.rst_ops      = &confd_stype_ops,
	.rst_level    = M0_RS_LEVEL_NORMAL,
	.rst_typecode = M0_CST_CONFD,
};

M0_INTERNAL int m0_confd_register(void)
{
	return m0_reqh_service_type_register(&m0_confd_stype);
}

M0_INTERNAL void m0_confd_unregister(void)
{
	m0_reqh_service_type_unregister(&m0_confd_stype);
}

static int confd_start(struct m0_reqh_service *service);
static void confd_stop(struct m0_reqh_service *service);
static void confd_fini(struct m0_reqh_service *service);

static const struct m0_reqh_service_ops confd_ops = {
	.rso_start       = confd_start,
	.rso_start_async = m0_reqh_service_async_start_simple,
	.rso_stop        = confd_stop,
	.rso_fini        = confd_fini
};

/** Allocates and initialises confd service. */
static int confd_allocate(struct m0_reqh_service            **service,
			  const struct m0_reqh_service_type  *stype)
{
	struct m0_confd *confd;

	M0_ENTRY();
	M0_PRE(stype == &m0_confd_stype);

	M0_ALLOC_PTR(confd);
	if (confd == NULL)
		return M0_ERR(-ENOMEM);

	m0_bob_init(&m0_confd_bob, confd);
	*service = &confd->d_reqh;
	(*service)->rs_ops = &confd_ops;
	return M0_RC(0);
}

/** Finalises and deallocates confd service. */
static void confd_fini(struct m0_reqh_service *service)
{
	struct m0_confd *confd = bob_of(service, struct m0_confd, d_reqh,
					&m0_confd_bob);
	M0_ENTRY();

	m0_bob_fini(&m0_confd_bob, confd);
	m0_free(confd);

	M0_LEAVE();
}

M0_INTERNAL int m0_confd_cache_create(struct m0_conf_cache **out,
				      struct m0_mutex       *cache_lock,
				      const char            *confstr)
{
	int rc;

	M0_ENTRY();
	M0_PRE(confstr != NULL);

	M0_ALLOC_PTR(*out);
	if (*out == NULL)
		return M0_ERR(-ENOMEM);
	m0_conf_cache_init(*out, cache_lock);
	m0_conf_cache_lock(*out);
	rc = confd_cache_preload(*out, confstr);
	m0_conf_cache_unlock(*out);
	if (rc == 0)
		return M0_RC(0);
	m0_conf_cache_fini(*out);
	m0_free0(out);
	return M0_ERR(rc);
}

M0_INTERNAL void m0_confd_cache_destroy(struct m0_conf_cache *cache)
{
	M0_ENTRY();
	m0_conf_cache_fini(cache);
	m0_free(cache);
	M0_LEAVE();
}

static int confd_start(struct m0_reqh_service *service)
{
	struct m0_confd *confd = bob_of(service, struct m0_confd, d_reqh,
					&m0_confd_bob);
	char            *path = NULL;
	char            *confstr = NULL;
	int              rc;

	M0_ENTRY();
	m0_mutex_init(&confd->d_cache_lock);
	rc = m0_confd_service_to_filename(service, &path) ?:
	     m0_file_read(path, &confstr) ?:
	     m0_confd_cache_create(&confd->d_cache, &confd->d_cache_lock,
				   confstr);
	m0_free(confstr);
	m0_free(path);
	if (rc != 0) {
		m0_mutex_fini(&confd->d_cache_lock);
		return M0_ERR(rc);
	}
	return M0_RC(0);
}

static void confd_stop(struct m0_reqh_service *service)
{
	struct m0_confd *confd = bob_of(service, struct m0_confd, d_reqh,
					&m0_confd_bob);
	M0_ENTRY();
	M0_PRE(confd->d_cache != NULL);
	m0_confd_cache_destroy(confd->d_cache);
	m0_mutex_fini(&confd->d_cache_lock);
	M0_LEAVE();
}

static int confd_cache_preload(struct m0_conf_cache *cache, const char *str)
{
	struct m0_confx    *enc;
	int                 i;
	int                 rc;
	struct m0_conf_obj *obj;

	M0_ENTRY();
	M0_PRE(m0_mutex_is_locked(cache->ca_lock));

	rc = m0_confstr_parse(str, &enc);
	if (rc != 0)
		return M0_ERR(rc);

	for (i = 0; i < enc->cx_nr && rc == 0; ++i) {
		struct m0_conf_obj        *obj;
		const struct m0_confx_obj *xobj = M0_CONFX_AT(enc, i);

		rc = m0_conf_obj_find(cache, m0_conf_objx_fid(xobj), &obj) ?:
			m0_conf_obj_fill(obj, xobj);
	}

	/* Check for orphan configuration object. */
	m0_tl_for(m0_conf_cache, &cache->ca_registry, obj) {
		M0_ASSERT_INFO(((m0_conf_obj_type(obj)== &M0_CONF_ROOT_TYPE) ||
				obj->co_parent != NULL ||
				m0_tlink_is_in(&m0_conf_dir_tl, obj)),
			       FID_F" is not part of configuration tree.",
			       FID_P(&obj->co_id));
	} m0_tlist_endfor;

	/*
	 * Now having cache updated, reset version number to M0_CONF_VER_UNKNOWN
	 * to let confd update it properly somewhat later when processing next
	 * configuration read request. See confx_populate().
	 */
	cache->ca_ver = M0_CONF_VER_UNKNOWN;
	m0_confx_free(enc);
	return M0_RC(rc);
}

static bool nil(const char *s)
{
	return s == NULL || *s == '\0';
}

M0_INTERNAL int m0_confd_service_to_filename(struct m0_reqh_service *service,
					     char                  **dbpath)
{
	M0_ENTRY();

	*dbpath = NULL;
	if (m0_buf_is_set(&service->rs_ss_param))
		*dbpath = m0_buf_strdup(&service->rs_ss_param);
	if (nil(*dbpath)) {
		m0_free(*dbpath);
		*dbpath = m0_strdup((char*)service->rs_reqh_ctx->rc_confdb);
	}

	if (*dbpath == NULL)
		return M0_ERR(-ENOMEM);
	if (**dbpath == '\0')
		return M0_ERR(-EPROTO);
	return M0_RC(0);
}

/** @} confd_dlspec */
#undef M0_TRACE_SUBSYSTEM
