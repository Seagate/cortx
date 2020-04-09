/* -*- c -*- */
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 03-Feb-2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "lib/misc.h"               /* M0_IN */
#include "xcode/xcode.h"            /* m0_xcode_union_add, M0_CONF_TYPE_MAX */
#include "conf/obj.h"
#include "conf/onwire.h"            /* m0_confx_obj_xc */
#include "conf/onwire_xc.h"         /* m0_confx_header_xc */

/**
 * @page conf DLD of configuration caching
 *
 * - @ref conf-ovw
 * - @ref conf-def
 * - @ref conf-req
 * - @ref conf-depends
 * - @ref conf-highlights
 * - @ref conf-fspec
 * - @ref conf-lspec
 * - @ref conf-conformance
 * - @ref conf-ut
 * - @ref conf-st
 * - @ref conf-O
 * - @ref conf-scalability
 * - @ref conf-ref
 * - @ref conf-impl-plan
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-ovw Overview
 *
 * Configuration is part of Mero cluster meta-data.  Configuration
 * client library (confc) provides API for accessing configuration
 * data.  Confc obtains configuration data from the configuration
 * server (confd) and caches this data in local memory.
 *
 * Confd tries to obtain requested configuration data from its own
 * cache. In case of cache miss, confd loads data from the
 * configuration database and updates the cache.
 *
 * Confd is a user-space service.  Confc library is implemented in
 * user space and in the kernel. Applications access configuration
 * data by linking with confc library and using its API.
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-def Definitions
 *
 * - @b Confc (configuration client library, configuration client):
 *   a library that provides configuration consumers with API to query
 *   Mero configuration.
 *
 * - @b Confd (configuration server): a management service that
 *   provides configuration clients with information obtained from
 *   configuration database.
 *
 * - Configuration @b consumer: any software that uses confc API to
 *   access Mero configuration.  Alternative name: @b application.
 *
 * - Configuration @b cache: configuration data stored in node’s
 *   memory.  Confc library maintains such a cache and provides
 *   configuration consumers with access to its data.  Confd also uses
 *   configuration cache for faster retrieval of information requested
 *   by configuration clients.
 *
 * - Configuration @b object: a data structure that contains
 *   configuration information. There are several types of
 *   configuration objects: filesystem, service, node, etc.
 *
 * - @b Identity of a configuration object is a pair of its type and
 *   identifier.
 *
 * - Configuration object is a @b stub if its status is not equal to
 *   M0_CS_READY.  Stubs contain no meaningful configuration data.
 *
 * - A configuration object is said to be @b pinned if its reference
 *   counter is nonzero; otherwise it is @b unpinned.
 *
 * - @b Relation: a pointer from one configuration object to another
 *   configuration object or to a collection of objects.  In former
 *   case it is @b one-to-one relation, in the latter case it is @b
 *   one-to-many relation.
 *
 * - @b Downlink: a relation whose destination is located further from
 *   the "root" configuration object than the origin.
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-req Requirements
 *
 * - @b r.conf.confc.kernel
 *   Confc library must be implemented for the kernel.
 * - @b r.conf.confc.user
 *   Confc library must be implemented for user space.
 * - @b r.conf.confc.async
 *   Confc library provides asynchronous interfaces for accessing
 *   configuration data.
 * - @b r.conf.cache.data-model
 *   The implementation should organize configuration information as
 *   outlined in section 4.1 of the HLD. The same data structures
 *   should be used for confc and confd caches, if possible.
 *   Configuration structures must be kept in memory.
 * - @b r.conf.cache.pinning
 *   Pinning of an object protects existence of this object in the cache.
 *   Pinned object can be moved from a stub condition to "ready".
 * - @b r.conf.cache.unique-objects
 *   Configuration cache must not contain multiple objects with the
 *   same identity.
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-depends Dependencies
 *
 * - We assume that the size of configuration database is very small
 *   compared with other meta-data.  Confd can take advantage of this
 *   assumption and load the entire contents of configuration database
 *   into the cache.
 *
 * - Mero database library ("db/db.h") should provide a user-space
 *   interface for creating in-memory databases. This interface will
 *   be used by confd and user-space confc.
 *
 *   See also `Writing In-Memory Berkeley DB Applications'
 *   [http://docs.oracle.com/cd/E17076_02/html/articles/inmemory/C/index.html].
 *
 * - m0_rpc_item_get() and m0_rpc_item_put() should be implemented.
 *
 *   Confc implementation schedules a state transition in
 *   ->rio_replied().  The data of ->ri_reply will be consumed only
 *   when the new state is being entered to.  The rpc item pointed to
 *   by ->ri_reply must not be freed (by rpc layer) until confc has
 *   consumed the data.  Thus the need for m0_rpc_item_get().
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-highlights Design Highlights
 *
 * - The application should not use relations of a configuration
 *   object to access other objects.
 *
 *   Rationale: relations may point to unpinned objects. Confc
 *   implementation may convert unpinned objects into stubs. The
 *   application shall not use stubs, since they contain no valid
 *   configuration data.
 *
 *   @see @ref conf-fspec-obj-private
 *
 * - The registry of cached configuration objects
 *   (m0_conf_cache::ca_registry) is not expected to be queried
 *   frequently. It makes sense to base its implementation on linked
 *   list data structure.
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-fspec Functional Specification
 *
 * - @subpage conf-fspec-obj
 * - @subpage confc-fspec
 * - @subpage conf-fspec-cache
 * - @subpage conf-fspec-preload
 * - @subpage conf-fspec-objops
 * - @subpage confd-fspec
 * - @subpage rconfc-fspec
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-lspec Logical Specification
 *
 * - @ref conf-lspec-comps
 * - @subpage confc-lspec
 * - @ref conf_dlspec_objops
 * - @ref conf_dlspec_cache
 * - @subpage rconfc-lspec
 * - @subpage confd-lspec-page
 * - @ref conf-lspec-state
 * - @ref conf-lspec-thread
 *
 * <!---------------------------------------------------------------->
 * @subsection conf-lspec-comps Components Overview
 *
 * Confc and confd maintain independent in-memory @ref
 * conf-fspec-cache "caches" of configuration data.
 *
 * Configuration cache can be @ref conf-fspec-preload "pre-loaded"
 * from an ASCII string.
 *
 * If a confc cache does not have enough data to fulfill a request of
 * configuration consumer, confc obtains the necessary data from the
 * confd and adds new configuration data to the cache.
 *
 * If a confd cache does not have enough data to fulfill a request of
 * confc, confd loads the necessary data from the configuration
 * database and updates the cache.
 *
 * <!---------------------------------------------------------------->
 * @subsection conf-lspec-state State Specification
 *
 * - @ref confc-lspec-state "States of a context state machine at confc side"
 * - @ref confd-lspec-state "States of a FOM at confd side"
 * - @ref conf-fspec-obj-enum-status
 * - @ref conf-fspec-obj-pinned
 *
 * <!---------------------------------------------------------------->
 * @subsection conf-lspec-thread Threading and Concurrency Model
 *
 * - @ref confc-lspec-thread "confc"
 * - @ref confd-lspec-thread "confd"
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-conformance Conformance
 *
 * - @b i.conf.confc.kernel
 *   The implementation of confc uses portable subset of Mero core
 *   API, which abstracts away the differences between kernel and
 *   user-space code.
 * - @b i.conf.confc.user
 *   Confc library is implemented for user space.
 * - @b i.conf.confc.async
 *   m0_confc_open() and m0_confc_readdir() are asynchronous calls.
 * - @b i.conf.cache.data-model
 *   Configuration information is organized as outlined in section 4.1
 *   of the HLD. One-to-many relationships are represented by
 *   m0_conf_dir objects.  The same data structures are used for both
 *   confc and confd.  Configuration structures are kept in memory.
 * - @b i.conf.cache.pinning
 *   Confc "pins" configuration object by incrementing its reference
 *   counter.  m0_confc_fini() asserts (M0_PRE()) that no objects are
 *   pinned when the cache is being destroyed.
 * - @b i.conf.cache.unique-objects
 *   Uniqueness of configuration object identities is achieved by
 *   using a registry of cached objects (m0_conf_cache::ca_registry).
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-ut Unit Tests
 *
 * Fault Injection mechanism (lib/finject.h) will be used to test
 * handling of "rare" errors (e.g., allocation errors) and to disable
 * some of external modules' functionality (e.g., to make m0_rpc_post()
 * a noop).
 *
 * @subsection conf-ut-common Infrastructure Test Suite
 *
 *     @test m0_conf_cache operations will be tested.
 *
 *     @test Path operations will be tested. This includes checking
 *           validity of various paths.
 *
 *     @test Object operations will be tested. This includes allocation,
 *           comparison with on-wire representation, stub enrichment.
 *
 *     @test m0_confstr_parse() will be tested.
 *
 * @subsection conf-ut-confc confc Test Suite
 *
 *     Test suite's init routine will create an "ast" thread (search
 *     for `ast_thread' in sm/ut/sm.c).  This thread will process ASTs
 *     as they are posted by confc functions.
 *
 *     @test path_walk() will be tested.
 *
 *     @test m0_confc_open*() and m0_confc_close() will be tested.
 *
 *     @test Cache operations will be tested. This includes
 *           cache_add(), object_enrich(), cache_grow(), and
 *           cache_preload().
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-scalability Scalability
 *
 * Current design imposes no restrictions on the size of configuration
 * cache.  If a configuration database is huge and the application is
 * keen to know every aspect of cluster configuration, confc cache may
 * eventually consume all available memory.  Confc will be unable to
 * allocate new objects, its state machines will end in S_FAILURE
 * state, and m0_confc_ctx_error() will return -ENOMEM.  The application
 * may opt to get rid of configuration cache by issuing m0_confc_fini().
 *
 * XXX @todo Implement cache eviction.
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-ref References
 *
 * - <a href="https://docs.google.com/a/seagate.com/document/d/12tbG9CeExDcCAs5H
 4rRgDeRRqAD0KGCp-W7ZWWXyzek/view">
 *   HLD of configuration caching</a>
 *
 * - <a href="https://docs.google.com/a/seagate.com/document/d/1pwDAxlghAlBGZ2zd
 mDeGPYoxblIDuKGmHystGwFHD-A/view">
 *   HLD of configuration.schema</a>
 *
 * - <a href="https://docs.google.com/a/seagate.com/document/d/1GkQJC82z7DqHBQR4
 Aeq-EfvEBjS9alZaR9-XU2QujEE/view">
 *   Configuration one-pager</a>
 */

M0_INTERNAL bool m0_conf_obj_is_stub(const struct m0_conf_obj *obj)
{
	M0_PRE(M0_IN(obj->co_status,
		     (M0_CS_MISSING, M0_CS_LOADING, M0_CS_READY)));
	return obj->co_status != M0_CS_READY;
}

static const struct m0_conf_obj_type *obj_types[256];

M0_BASSERT(M0_CONF_OBJ_TYPE_MAX <= ARRAY_SIZE(obj_types));

void m0_conf_obj_type_register(const struct m0_conf_obj_type *otype)
{
	uint8_t id = otype->cot_ftype.ft_id;

	M0_PRE(IS_IN_ARRAY(id, obj_types));
	M0_PRE(obj_types[id] == NULL);

	m0_fid_type_register(&otype->cot_ftype);
	obj_types[id] = otype;

	if (otype->cot_xt != NULL) {
		otype->cot_xc_init();
		M0_PRE_EX(m0_xcode_type_flags(*otype->cot_xt,
					      M0_XCODE_TYPE_FLAG_DOM_CONF, 0,
					      M0_BITS(M0_XA_ATOM)));
		/* Onwire representation must start with the header. */
		M0_PRE((*otype->cot_xt)->xct_child[0].xf_type ==
		       m0_confx_header_xc);

		m0_xcode_union_add(m0_confx_obj_xc, otype->cot_branch,
				   *otype->cot_xt, id);
	}
	M0_POST(m0_forall(i, ARRAY_SIZE(obj_types),
	  ergo(obj_types[i] != NULL,
	       (obj_types[i]->cot_magic == otype->cot_magic) == (i == id))));
}

void m0_conf_obj_type_unregister(const struct m0_conf_obj_type *otype)
{
	uint8_t id = otype->cot_ftype.ft_id;

	M0_PRE(IS_IN_ARRAY(id, obj_types));
	M0_PRE(obj_types[id] == otype);
	m0_fid_type_unregister(&otype->cot_ftype);
	obj_types[id] = NULL;
}

M0_INTERNAL const struct m0_conf_obj_type *
m0_conf_obj_type_next(const struct m0_conf_obj_type *otype)
{
	int idx;

	idx = otype == NULL ? 0 : otype->cot_ftype.ft_id + 1;
	for (; idx < ARRAY_SIZE(obj_types); ++idx) {
		if (obj_types[idx] != NULL)
			return obj_types[idx];
	}
	return NULL;
}

const struct m0_conf_obj_type *m0_conf_obj_type(const struct m0_conf_obj *obj)
{
	return m0_conf_fid_type(&obj->co_id);
}

const struct m0_conf_obj_type *m0_conf_fid_type(const struct m0_fid *fid)
{
	uint8_t id = m0_fid_type_getfid(fid)->ft_id;

	M0_PRE(IS_IN_ARRAY(id, obj_types));
	M0_PRE(obj_types[id] != NULL);

	return obj_types[id];
}

bool m0_conf_fid_is_valid(const struct m0_fid *fid)
{
	return	m0_fid_is_valid(fid) &&
		obj_types[m0_fid_type_getfid(fid)->ft_id] != NULL;
}

struct m0_conf_obj *m0_conf_obj_grandparent(const struct m0_conf_obj *obj)
{
	struct m0_conf_obj *result;

	M0_PRE(!m0_conf_obj_is_stub(obj));
	M0_PRE(m0_conf_obj_type(obj->co_parent) == &M0_CONF_DIR_TYPE);

	result = obj->co_parent->co_parent;

	M0_POST(!m0_conf_obj_is_stub(result));
	return result;
}

M0_INTERNAL int m0_conf_obj_init(void)
{
	m0_xcode_union_init(m0_confx_obj_xc, "m0_confx_obj",
			    "xo_type", M0_CONF_OBJ_TYPE_MAX);
	m0_confx_obj_xc->xct_flags = M0_XCODE_TYPE_FLAG_DOM_CONF |
				     M0_XCODE_TYPE_FLAG_DOM_RPC;
#define X_CONF(_, NAME, ...) \
	m0_conf_obj_type_register(&M0_CONF_ ## NAME ## _TYPE);

	M0_CONF_OBJ_TYPES
#undef X_CONF
	m0_xcode_union_close(m0_confx_obj_xc);
	m0_fid_type_register(&M0_CONF_RELFID_TYPE);
	return 0;
}

M0_INTERNAL void m0_conf_obj_fini(void)
{
#define X_CONF(_, NAME, ...) \
	m0_conf_obj_type_unregister(&M0_CONF_ ## NAME ## _TYPE);

	M0_CONF_OBJ_TYPES
#undef X_CONF
	m0_fid_type_unregister(&M0_CONF_RELFID_TYPE);
	m0_xcode_union_fini(m0_confx_obj_xc);
}

M0_INTERNAL void
m0_conf_child_adopt(struct m0_conf_obj *parent, struct m0_conf_obj *child)
{
	/* Root cannot be a child, because it is the topmost object. */
	M0_PRE(m0_conf_obj_type(child) != &M0_CONF_ROOT_TYPE);
	M0_PRE(child->co_cache == parent->co_cache);

	if (child->co_parent != child)
		child->co_parent = parent;
}

#undef M0_TRACE_SUBSYSTEM
