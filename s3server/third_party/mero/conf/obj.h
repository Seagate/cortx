/* -*- c -*- */
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Authors:         Andriy Tkachuk <andriy.tkachuk@seagate.com>
 *
 * Original creation date: 30-Jan-2012
 */
#pragma once
#ifndef __MERO_CONF_OBJ_H__
#define __MERO_CONF_OBJ_H__

#include "conf/schema.h"    /* m0_conf_service_type */
#include "ha/note.h"        /* m0_ha_obj_state */
#include "layout/pdclust.h" /* m0_pdclust_attr */
#include "lib/protocol.h"   /* m0_protocol_id */
#include "lib/bob.h"
#include "fid/fid.h"          /* m0_fid */
#include "conf/schema.h"      /* m0_conf_service_type */
#include "fdmi/filter.h"      /* m0_fdmi_filter */

struct m0_conf_obj_ops;
struct m0_confx_obj;
struct m0_xcode_type;

/**
 * @page conf-fspec-obj Configuration Objects
 *
 * - @ref conf-fspec-obj-data
 * - @ref conf-fspec-obj-enum
 *   - @ref conf-fspec-obj-enum-status
 * - @ref conf-fspec-obj-pinned
 * - @ref conf-fspec-obj-private
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-fspec-obj-data Data Structures
 *
 * There are different kinds of configuration data: configuration of
 * filesystems, services, nodes, storage devices, etc.  Configuration data is
 * contained in configuration objects of which there are following predefined
 * types:
 *
 * - m0_conf_dir (a container for configuration objects),
 * - m0_conf_root
 * - m0_conf_profile,
 * - m0_conf_pool,
 * - m0_conf_pver,
 * - m0_conf_objv,
 * - m0_conf_node,
 * - m0_conf_process,
 * - m0_conf_service,
 * - m0_conf_sdev,
 * - m0_conf_site,
 * - m0_conf_rack,
 * - m0_conf_enclosure,
 * - m0_conf_controller,
 * - m0_conf_drive,
 * - m0_conf_fdmi_flt_grp,
 * - m0_conf_fdmi_filter.
 *
 * Some attributes are applicable to any type of configuration object.
 * Such common attributes are put together into m0_conf_obj structure,
 * which is embedded into concrete configuration objects.
 *
 * DAG of configuration objects:
 *
 * @dot
 * /// Solid line denotes "is parent of" relation,
 * /// dashed line --- "refers to" relation;
 * /// "normal" (black) arrow --- one-to-one relation,
 * /// "onormal" (white) arrow --- one-to-many relation.
 *
 * digraph mero_conf_schema {
 *   edge [arrowhead=onormal];  // most relations are "one-to-many"
 *
 *   root -> "node" [label=nodes];
 *   root -> site [label=sites];
 *   root -> pool [label=pools];
 *   root -> profile [label="profiles\n(clients only)"];
 *   root -> fdmi_flt_grp [label=fdmi_flt_grps];
 *   fdmi_flt_grp -> fdmi_filter [label=filters];
 *
 *   "node" -> process [label=processes];
 *   process -> service [label=services];
 *   service -> sdev [label=sdevs];
 *   site -> rack [label=racks];
 *   rack -> enclosure [label=encls];
 *   enclosure -> controller [label=ctrls];
 *   controller -> drive [label=drives];
 *
 *   profile [label="profile\n(for clients)"];
 *   profile -> pool [style=dashed];
 *
 *   pool -> "pool version" [label=pvers];
 *   "pool version" -> "site-v" [label=sitevs];
 *   "site-v" -> "rack-v" [label=sitevs];
 *   "rack-v" -> "enclosure-v" [label=children];
 *   "enclosure-v" -> "controller-v" [label=children];
 *   "controller-v" -> "drive-v" [label=children];
 *
 *   edge [dir=back, weight=0, style=dashed, arrowhead=normal];
 *   "node" -> controller;
 *   site -> "site-v";
 *   rack -> "rack-v";
 *   enclosure -> "enclosure-v";
 *   controller -> "controller-v";
 *   drive -> "drive-v";
 *   sdev -> drive [dir=both];
 * }
 * @enddot
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-fspec-obj-enum Enumerations
 *
 * - m0_conf_status --- readiness of object's configuration data.
 *
 * @subsection conf-fspec-obj-enum-status Configuration Object Status
 *
 * A configuration object exists in one of three states:
 *   - M0_CS_MISSING --- configuration data is absent and is not being
 *     retrieved; the object is a stub.
 *   - M0_CS_LOADING --- retrieval of configuration is in progress; the
 *     object is a stub.
 *   - M0_CS_READY --- configuration is available; this is a
 *     fully-fledged object, not a stub.
 *
 * These values make up @ref m0_conf_status enumeration.
 *
 * Status field of a configuration object -- m0_conf_obj::co_status --
 * is accessed and modified by object's owner (confc or confd).
 * Initial status is M0_CS_MISSING. Possible transitions are shown on
 * the diagram below:
 *
 * @dot
 * digraph obj_status {
 *     M0_CS_MISSING -> M0_CS_LOADING [label="loading started"];
 *     M0_CS_LOADING -> M0_CS_MISSING [label="loading failed"];
 *     M0_CS_LOADING -> M0_CS_READY [label="loading succeeded"];
 *     M0_CS_MISSING -> M0_CS_READY [label=
 *  "configuration data is filled\nby some loading operation"];
 * }
 * @enddot
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-fspec-obj-pinned Pinned Objects
 *
 * If object's reference counter -- m0_conf_obj::co_nrefs -- is
 * non-zero, the object is said to be @em pinned. Stubs cannot be
 * pinned, only M0_CS_READY objects can.  Object's reference counter
 * is used by confc and confd implementations and is not supposed to
 * be accessed by a configuration consumer.
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-fspec-obj-private Private Fields
 *
 * Only those fields of concrete configuration objects that follow
 * "configuration data (for the application)" comment should ever be
 * accessed by the application.  The rest of fields are private
 * property of confc and confd implementations.
 *
 * @see @ref conf_dfspec_obj "Detailed Functional Specification"
 */

/**
 * @defgroup conf_dfspec_obj Configuration Objects
 * @brief Detailed Functional Specification.
 *
 * @see @ref conf, @ref conf-fspec-obj "Functional Specification"
 *
 * @{
 */

/**
 * Status of configuration object.
 * Configuration object is a stub unless its status is M0_CS_READY.
 *
 * @see m0_conf_obj_is_stub()
 */
enum m0_conf_status {
	M0_CS_MISSING, /**< Configuration is absent; no retrieval in progress.*/
	M0_CS_LOADING, /**< Retrieval of configuration is in progress. */
	M0_CS_READY    /**< Configuration is available. */
};

/**
 * Generic configuration object.
 *
 * The fields of struct m0_conf_obj are common to all configuration
 * objects.  m0_conf_obj is embedded into each concrete configuration
 * object.
 */
struct m0_conf_obj {
	/** Unique object identifier. */
	struct m0_fid                 co_id;

	enum m0_conf_status           co_status;

	const struct m0_conf_obj_ops *co_ops;

	/**
	 * Pointer to the parent object.
	 *
	 * For objects that may have several parents (e.g., m0_conf_node) this
	 * points to the object itself.
	 *
	 * @note  Do not use obj->co_parent->co_parent chain of pointers.
	 *        m0_conf_obj_grandparent() is a safer alternative.
	 */
	struct m0_conf_obj           *co_parent;

	/**
	 * Reference counter.
	 * The object is "pinned" if this value is non-zero.
	 *
	 * @see m0_conf_obj_get(), m0_conf_obj_put()
	 */
	uint64_t                      co_nrefs;

	/**
	 * Channel on which following events are announced:
	 * - configuration loading completed;
	 * - object unpinned.
	 */
	struct m0_chan                co_chan;

	/** State of this configuration object, as reported by HA. */
	enum m0_ha_obj_state          co_ha_state;

	/**
	 * Channel on which .co_ha_state changes are announced
	 * (M0_CS_READY objects only).
	 * Protected by configuration cache lock (.co_cache->ca_lock).
	 */
	struct m0_chan                co_ha_chan;

	/** Configuration cache this object belongs to. */
	struct m0_conf_cache         *co_cache;

	/** Linkage to m0_conf_cache::ca_registry. */
	struct m0_tlink               co_cache_link;

	/** Linkage to m0_conf_dir::cd_items. */
	struct m0_tlink               co_dir_link;

	/**
	 * Generic magic.
	 *
	 * All configuration objects have the same value of
	 * ->co_gen_magic.
	 *
	 * This magic value is used for list operations.
	 */
	uint64_t                      co_gen_magic;

	/**
	 * Concrete magic.
	 *
	 * Different concrete object types have different values of
	 * ->co_con_magic.
	 *
	 * This magic value is used for generic-to-concrete casting
	 * (see M0_CONF_CAST()).
	 */
	uint64_t                      co_con_magic;

	/**
	 * Whether m0_conf_cache_gc() should garbage-collect this object.
	 *
	 * @note  Whoever sets this flag is responsible for calling
	 *        m0_conf_cache_gc(), otherwise m0_conf_cache_clean()
	 *        will fail.
	 */
	bool                          co_deleted;

	/**
	 * Human-friendly name of the resource represented by this object.
	 * To be used in error messages.
	 */
	const char                   *co_name;

	/*
	 * XXX TODO: Introduce .co_extra_attrs field --- optional array
	 * of {name, value} pairs of strings.
	 *
	 * Fixed set of conf objects' attributes is expensive to change.
	 * To add new attribute to a conf object type, one has to modify:
	 * - conf/obj.h and conf.onwire.h data structures,
	 * - conf/objs/ *.c files,
	 * - Spiel,
	 * - m0confgen utility,
	 * - Halon code.
	 *
	 * m0_conf_obj::co_extra_attrs will let us experiment with new
	 * sets of attributes without code changes. This may prove useful
	 * for testing, debugging, and maintenance.
	 */
};

struct m0_conf_obj_type {
	const struct m0_fid_type    cot_ftype;
	struct m0_conf_obj       *(*cot_create)(void);
	uint64_t                    cot_magic;
	/**
	 * xcode type of m0_confx_foo. Double indirect, because m0_confx_foo_xc
	 * is not a constant (its address is).
	 */
	struct m0_xcode_type      **cot_xt;
	/**
	 * Name of the field in m0_confx_obj_xc union. This union xcode type is
	 * dynamically built as new conf object types are registered.
	 */
	const char                 *cot_branch;
	void                      (*cot_xc_init)(void);
};

void m0_conf_obj_type_register(const struct m0_conf_obj_type *otype);
void m0_conf_obj_type_unregister(const struct m0_conf_obj_type *otype);

const struct m0_conf_obj_type *m0_conf_obj_type(const struct m0_conf_obj *obj);
const struct m0_conf_obj_type *m0_conf_fid_type(const struct m0_fid *id);
const struct m0_fid           *m0_conf_objx_fid(const struct m0_confx_obj *obj);
const struct m0_conf_obj_type *
m0_conf_objx_type(const struct m0_confx_obj *obj);

bool m0_conf_fid_is_valid(const struct m0_fid *fid);

/**
 * Returns true iff obj->co_status != M0_CS_READY.
 *
 * @pre M0_IN(obj->co_status, (M0_CS_MISSING, M0_CS_LOADING, M0_CS_READY))
 */
M0_INTERNAL bool m0_conf_obj_is_stub(const struct m0_conf_obj *obj);

struct m0_conf_obj *m0_conf_obj_grandparent(const struct m0_conf_obj *obj);

enum { M0_CONF_OBJ_TYPE_MAX = 256 };

/* ------------------------------------------------------------------
 * Concrete configuration objects
 * ------------------------------------------------------------------ */

/** Directory object --- a container for configuration objects. */
struct m0_conf_dir {
	struct m0_conf_obj             cd_obj;
	/** List of m0_conf_obj-s, linked through m0_conf_obj::co_dir_link. */
	struct m0_tl                   cd_items;
	/** Type of items. */
	const struct m0_conf_obj_type *cd_item_type;
	/** "Relation" represented by this directory. */
	struct m0_fid                  cd_relfid;
};

/** Top-level configuration object. */
struct m0_conf_root {
	/**
	 * ->rt_obj.co_parent == NULL: God the Father has no parent.
	 */
	struct m0_conf_obj    rt_obj;
	struct m0_conf_dir   *rt_nodes;
	struct m0_conf_dir   *rt_sites;
	struct m0_conf_dir   *rt_pools;
	struct m0_conf_dir   *rt_profiles;
	/**
	 * FDMI filter groups.
	 *
	 * @todo Halon support is yet to be implemented; see HALON-730.
	 */
	struct m0_conf_dir   *rt_fdmi_flt_grps;
/* configuration data (for the application) */
	/**
	 * Version of the configuration database.
	 * Incremented after every configuration database update.
	 *
	 * @note Value 0 is reserved and must not be used.
	 */
	uint64_t              rt_verno;
	/** Any fid. Reserved for future use. */
	struct m0_fid         rt_rootfid;
	/** Meta-data pool. */
	struct m0_fid         rt_mdpool;  // XXX-MULTIPOOLS: DELETEME
	/**
	 * Distributed index meta-data pool version.
	 *
	 * If there are no CAS services defined in the configuration,
	 * the value is M0_FID0.
	 *
	 * @see "Index meta-data" section in dix/client.h for more information.
	 */
	struct m0_fid         rt_imeta_pver;
	/** Meta-data redundancy. */
	uint32_t              rt_mdredundancy;
	/**
	 * Extra parameters.
	 * NULL-terminated array of C strings.
	 * XXX @todo Make it an array of name-value pairs (attributes).
	 */
	const char          **rt_params;
	struct m0_protocol_id rt_protocol;
};

/**
 * Profile represents a particular subset of pools.
 *
 * Profiles are only used by Mero clients.  Mero services ignore
 * m0_conf_profile objects, accessing pools via m0_conf_root::rt_pools.
 */
struct m0_conf_profile {
	struct m0_conf_obj cp_obj;
/* configuration data (for the application) */
	struct m0_fid_arr  cp_pools; /**< Pools in this profile. */
};

/**
 * Pools are used to partition hardware and software resources
 * (entities) --- devices, services, etc.
 */
struct m0_conf_pool {
	struct m0_conf_obj  pl_obj;
	/**
	 * Directory of actual and formulaic pool versions.
	 *
	 * @note Virtual pool versions are not linked to the conf graph
	 *       (though they are added to m0_conf_cache).
	 */
	struct m0_conf_dir *pl_pvers;
/* configuration data (for the application) */
	/**
	 * Policy to be used for pool version selection.
	 *
	 * @see m0_pver_policy_code
	 *
	 * XXX @todo Replace with
	 * m0_conf_root::rt_pool_selection_policy.
	 */
	uint32_t            pl_pver_policy;
};

/**
 * Levels of a m0_conf_pver_subtree.
 *
 * Currently 5 levels are supported:
 * [0] = sites, [1] = racks, [2] = enclosures, [3] = controllers, [4] = drives.
 */
enum {
	M0_CONF_PVER_LVL_SITES = 0,
	M0_CONF_PVER_LVL_RACKS,
	M0_CONF_PVER_LVL_ENCLS,
	M0_CONF_PVER_LVL_CTRLS,
	M0_CONF_PVER_LVL_DRIVES,
	M0_CONF_PVER_HEIGHT
};

/** Actual or virtual pool version. */
struct m0_conf_pver_subtree {
	/**
	 * Number of failed entities at the corresponding level,
	 * a.k.a. recd vector ("recd" is acronym of "racks, enclosures,
	 * controllers, drives"; `site` object did not exist when the
	 * field was christened.).
	 *
	 * This attribute is not used for virtual pool versions.
	 */
	uint32_t               pvs_recd[M0_CONF_PVER_HEIGHT];
	struct m0_conf_dir    *pvs_sitevs;
/* configuration data (for the application) */
	/** Layout attributes. */
	struct m0_pdclust_attr pvs_attr;
	/**
	 * Tolerance constraint.
	 *
	 * @see DLD-failure-domains-def
	 */
	uint32_t               pvs_tolerance[M0_CONF_PVER_HEIGHT];
};

/** Formulaic pool version. */
struct m0_conf_pver_formulaic {
	/** Cluster-unique identifier of this formulaic pver. */
	uint32_t      pvf_id;
	/**
	 * Fid of the base pool version.
	 *
	 * Base pver is the "actual" pver, which a "virtual" pver is modelled
	 * after.
	 * @see m0_conf_virtual_pver_create()
	 */
	struct m0_fid pvf_base;
	/**
	 * Allowance vector --- the number of objects excluded from the
	 * corresponding level of base pver subtree.
	 *
	 * Difference between allowance and tolerance: allowance is for
	 * writing, i.e. we will create new pool versions until that
	 * number of failures is reached. Tolerance is for reading.
	 *
	 * @pre  at least one of the numbers != 0
	 */
	uint32_t      pvf_allowance[M0_CONF_PVER_HEIGHT];
};

/** Kinds of m0_conf_pvers. */
enum m0_conf_pver_kind {
	M0_CONF_PVER_ACTUAL,
	M0_CONF_PVER_FORMULAIC,
	/*
	 * Virtual pvers exist in local conf cache only, they are not
	 * mentioned in the conf database.
	 *
	 * Virtual pvers are not linked to the conf DAG.
	 */
	M0_CONF_PVER_VIRTUAL
};

/**
 * Pool version.
 * These objects are used to track changes in pool membership.
 */
struct m0_conf_pver {
	struct m0_conf_obj     pv_obj;
/* configuration data (for the application) */
	enum m0_conf_pver_kind pv_kind;
	union {
		struct m0_conf_pver_subtree   subtree;
		struct m0_conf_pver_formulaic formulaic;
	} pv_u;
};

/** Element of m0_conf_pver_subtree. */
struct m0_conf_objv {
	struct m0_conf_obj  cv_obj;
	struct m0_conf_dir *cv_children;
	/**
	 * Ordinal number of this objv in m0_conf_pver subtree.
	 * The value is assigned by conf_pver_enumerate().
	 */
	int                 cv_ix;
/* configuration data (for the application) */
	/**
	 * Real entity, which this objv refers to.
	 *
	 * Depending on the level of objv in m0_conf_pver_subtree,
	 * .cv_real may point at m0_conf_{site,rack,enclosure,controller,drive}.
	 */
	struct m0_conf_obj *cv_real;
};

struct m0_conf_node {
	struct m0_conf_obj   cn_obj;
	struct m0_conf_dir  *cn_processes;
/* configuration data (for the application) */
	/** Memory size in MB. */
	uint32_t             cn_memsize;
	/** Number of processors. */
	uint32_t             cn_nr_cpu;
	/** Last known state. See m0_cfg_state_bit. */
	uint64_t             cn_last_state;
	/** Property flags. See m0_cfg_flag_bit. */
	uint64_t             cn_flags;
};

/**
 * Process configuration.
 *
 * @see m0_processors_online(), m0_proc_attr
 */
struct m0_conf_process {
	struct m0_conf_obj  pc_obj;
	struct m0_conf_dir *pc_services;
/* configuration data (for the application) */
	struct m0_bitmap    pc_cores; /**< Available cores (mask). */
	uint64_t            pc_memlimit_as;
	uint64_t            pc_memlimit_rss;
	uint64_t            pc_memlimit_stack;
	uint64_t            pc_memlimit_memlock;
	/** @todo Use an array to support several network interfaces. */
	const char         *pc_endpoint;
};

/** Service configuration (within a process). */
struct m0_conf_service {
	struct m0_conf_obj        cs_obj;
	struct m0_conf_dir       *cs_sdevs;
/* configuration data (for the application) */
	enum m0_conf_service_type cs_type;
	/**
	 * Endpoints from which this service is reachable.
	 * (Several network interfaces ==> several endpoints.)
	 *
	 * NULL-terminated array of C strings.
	 */
	const char              **cs_endpoints;
	/**
	 * Service parameters.
	 * NULL-terminated array of C strings.
	 * XXX @todo Make it an array of name-value pairs (attributes).
	 */
	const char              **cs_params;
};

/** Storage device. */
struct m0_conf_sdev {
	struct m0_conf_obj   sd_obj;
/* configuration data (for the application) */
	/**
	 * Fid of the corresponding drive object.
	 *
	 * @note This can be M0_FID0 if no drive object is defined
	 *       for this sdev. This can happen, for example,
	 *       when the drive is failed, taken away, but not
	 *       replaced yet.
	 *
	 * @todo XXX DELETEME
	 */
	struct m0_fid        sd_drive;
	/**
	 * Device index.
	 * The value should be unique and belong [0, P) range, where P
	 * is a total number of devices under CAS/IOS services in the
	 * filesystem.
	 */
	uint32_t             sd_dev_idx;
	/** Interface type. See m0_cfg_storage_device_interface_type. */
	uint32_t             sd_iface;
	/** Media type. See m0_cfg_storage_device_media_type. */
	uint32_t             sd_media;
	/** Block size in bytes. */
	uint32_t             sd_bsize;
	/** Size in bytes. */
	uint64_t             sd_size;
	/** Last known state.  See m0_cfg_state_bit. */
	uint64_t             sd_last_state;
	/** Property flags.  See m0_cfg_flag_bit. */
	uint64_t             sd_flags;
	/** Filename in host OS. */
	const char          *sd_filename;
};

/** Site (data center). */
struct m0_conf_site {
	struct m0_conf_obj    ct_obj;
	/** The racks located at this site. */
	struct m0_conf_dir   *ct_racks;
/* configuration data (for the application) */
	/**
	 * Pool versions this site is part of.
	 * NULL-terminated array.
	 */
	struct m0_conf_pver **ct_pvers;
};

/** Hardware resource --- rack. */
struct m0_conf_rack {
	struct m0_conf_obj    cr_obj;
	/** Enclosures on this rack. */
	struct m0_conf_dir   *cr_encls;
/* configuration data (for the application) */
	/**
	 * Pool versions this rack is part of.
	 * NULL-terminated array.
	 */
	struct m0_conf_pver **cr_pvers;
};

/** Hardware resource --- enclosure. */
struct m0_conf_enclosure {
	struct m0_conf_obj    ce_obj;
	/** Controllers in this enclosure. */
	struct m0_conf_dir   *ce_ctrls;
/* configuration data (for the application) */
	/**
	 * Pool versions this enclosure is part of.
	 * NULL-terminated array.
	 */
	struct m0_conf_pver **ce_pvers;
};

/** Hardware resource --- controller. */
struct m0_conf_controller {
	struct m0_conf_obj    cc_obj;
	/** The node this controller is associated with. */
	struct m0_conf_node  *cc_node;
	/** Storage drives attached to this controller. */
	struct m0_conf_dir   *cc_drives;
/* configuration data (for the application) */
	/**
	 * Pool versions this controller is part of.
	 * NULL-terminated array.
	 */
	struct m0_conf_pver **cc_pvers;
};

struct m0_conf_drive {
	struct m0_conf_obj    ck_obj;
/* configuration data (for the application) */
	/** Storage device associated with this drive. */
	struct m0_conf_sdev  *ck_sdev;
	/**
	 * Pool versions this drive is part of.
	 * NULL-terminated array.
	 */
	struct m0_conf_pver **ck_pvers;
};

/** Filters group. All filters in group match record type they work with. */
struct m0_conf_fdmi_flt_grp {
	struct m0_conf_obj  ffg_obj;
	/* Type of the record that filters of this group support. */
	uint32_t            ffg_rec_type;
	/* Directory of filters for this filters group. */
	struct m0_conf_dir *ffg_filters;
};

struct m0_conf_fdmi_filter {
	struct m0_conf_obj     ff_obj;
	struct m0_fid          ff_filter_id;
	struct m0_fdmi_filter  ff_filter;
	/** The node this filter is hosted at. */
	struct m0_conf_node   *ff_node;
	/**
	 * Endpoints which are interested in FDMI records matched with
	 * this filter.
	 *
	 * NULL-terminated array of C strings.
	 * @note Currently only one endpoint can be in this array.
	 */
	const char           **ff_endpoints;
	struct m0_tlink        ff_linkage;
	uint64_t               ff_magic;
};

/* ------------------------------------------------------------------
 * Cast
 * ------------------------------------------------------------------ */

/**
 * Casts m0_conf_obj to the ambient concrete configuration object.
 *
 * @param ptr   Pointer to m0_conf_obj member.
 * @param type  Type of concrete configuration object (without `struct').
 *
 * Example:
 * @code
 * struct m0_conf_service *svc = M0_CONF_CAST(svc_obj, m0_conf_service);
 * @endcode
 */
#define M0_CONF_CAST(ptr, type) \
	bob_of(ptr, struct type, type ## _cast_field, &type ## _bob)

#define m0_conf_dir_cast_field        cd_obj
#define m0_conf_root_cast_field       rt_obj
#define m0_conf_profile_cast_field    cp_obj
#define m0_conf_pool_cast_field       pl_obj
#define m0_conf_pver_cast_field       pv_obj
#define m0_conf_objv_cast_field       cv_obj
#define m0_conf_node_cast_field       cn_obj
#define m0_conf_process_cast_field    pc_obj
#define m0_conf_service_cast_field    cs_obj
#define m0_conf_sdev_cast_field       sd_obj
#define m0_conf_site_cast_field       ct_obj
#define m0_conf_rack_cast_field       cr_obj
#define m0_conf_enclosure_cast_field  ce_obj
#define m0_conf_controller_cast_field cc_obj
#define m0_conf_drive_cast_field      ck_obj
#define m0_conf_fdmi_filter_cast_field  ff_obj
#define m0_conf_fdmi_flt_grp_cast_field ffg_obj

#define M0_CONF_OBJ_TYPES                       \
	X_CONF(root,         ROOT,         't') \
	X_CONF(dir,          DIR,          'D') \
	X_CONF(profile,      PROFILE,      'p') \
	X_CONF(pool,         POOL,         'o') \
	X_CONF(pver,         PVER,         'v') \
	X_CONF(objv,         OBJV,         'j') \
	X_CONF(node,         NODE,         'n') \
	X_CONF(process,      PROCESS,      'r') \
	X_CONF(service,      SERVICE,      's') \
	X_CONF(sdev,         SDEV,         'd') \
	X_CONF(site,         SITE,         'S') \
	X_CONF(rack,         RACK,         'a') \
	X_CONF(enclosure,    ENCLOSURE,    'e') \
	X_CONF(controller,   CONTROLLER,   'c') \
	X_CONF(drive,        DRIVE,        'k') \
	X_CONF(fdmi_flt_grp, FDMI_FLT_GRP, 'g') \
	X_CONF(fdmi_filter,  FDMI_FILTER,  'l')

#define X_CONF(name, NAME, _)                                   \
extern const struct m0_bob_type m0_conf_ ## name ## _bob;       \
extern const struct m0_conf_obj_type M0_CONF_ ## NAME ## _TYPE;

M0_CONF_OBJ_TYPES
#undef X_CONF

/*
 * Relation fids. They represent parent-child relations between
 * conf objects.
 */
#define M0_CONF_REL_FIDS               \
	X_CONF(ANY,                -1) \
				       \
	X_CONF(ROOT_NODES,          1) \
	X_CONF(ROOT_SITES,          2) \
	X_CONF(ROOT_POOLS,          3) \
	X_CONF(ROOT_PROFILES,       4) \
	X_CONF(ROOT_FDMI_FLT_GRPS,  5) \
	X_CONF(FDMI_FGRP_FILTERS,   6) \
				       \
	X_CONF(POOL_PVERS,          7) \
	X_CONF(PVER_SITEVS,         8) \
	X_CONF(SITEV_RACKVS,        9) \
	X_CONF(RACKV_ENCLVS,       10) \
	X_CONF(ENCLV_CTRLVS,       11) \
	X_CONF(CTRLV_DRIVEVS,      12) \
				       \
	X_CONF(NODE_PROCESSES,     13) \
	X_CONF(PROCESS_SERVICES,   14) \
	X_CONF(SERVICE_SDEVS,      15) \
				       \
	X_CONF(SITE_RACKS,         16) \
	X_CONF(RACK_ENCLS,         17) \
	X_CONF(ENCLOSURE_CTRLS,    18) \
	X_CONF(CONTROLLER_DRIVES,  19)

#define X_CONF(name, _) \
extern const struct m0_fid M0_CONF_ ## name ## _FID;

M0_CONF_REL_FIDS
#undef X_CONF
extern const struct m0_fid_type M0_CONF_RELFID_TYPE;

/**
 * Root configuration object FID.
 * The same in any configuration database.
 */
extern const struct m0_fid M0_CONF_ROOT_FID;

/**
 * Iterates over registered conf object types.
 *
 * To start iteration call m0_conf_obj_type_next(NULL). Returns NULL when the
 * iteration is over.
 *
 * @code
 * const struct m0_conf_obj_type *t = NULL;
 *
 * while ((t = m0_conf_obj_type_next(t)) != NULL) {
 *         ... do something with t ...
 * }
 * @endcode
 */
M0_INTERNAL const struct m0_conf_obj_type *
m0_conf_obj_type_next(const struct m0_conf_obj_type *otype);

M0_INTERNAL int m0_conf_obj_init(void);
M0_INTERNAL void m0_conf_obj_fini(void);

M0_INTERNAL void m0_conf_child_adopt(struct m0_conf_obj *parent,
				     struct m0_conf_obj *child);

/** @} conf_dfspec_obj */
#endif /* __MERO_CONF_OBJ_H__ */
