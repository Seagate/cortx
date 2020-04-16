/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 *                  Trupti Patil <trupti_patil@xyratex.com>
 * Original creation date: 07/09/2010
 */

#pragma once

#ifndef __MERO_LAYOUT_LAYOUT_H__
#define __MERO_LAYOUT_LAYOUT_H__

/**
 * @defgroup layout Layouts.
 *
 * @section layout-terminology Terminology
 * - Layout @n
 *   A 'layout' is an attribute of a file. It maps a file onto a set of network
 *   resources. viz. component objects.
 *
 * - Layout user count @n
 *   Layout user count is the number of users associated with a particular
 *   layout. For example, files using that layout, other composite layouts
 *   using that layout.
 *   - User count of a layout does not have any impact on the liveness of
 *     an in-memory representation of a layout.
 *   - A layout with non-zero user count can not be deleted from the layout DB.
 *   - A layout with zero user count may continue to reside in memory or in
 *     the layout DB.
 *
 * - Layout type @n
 *   A 'layout type' specifies how a file is stored in a collection of targets.
 *   It provides the <offset-in-gob> to <traget-idx, offset-in-target> mapping.
 *   For example, PDCLUST, RAID1, RAID5 are some types of layout, while
 *   COMPOSITE being another special layout type.
 *
 * - Enumeration @n
 *   An 'enumeration' provides <gfid, target-idx> to <cob-fid> mapping. Not all
 *   the layout types need an enumeration. For example, layouts with types
 *   composite, de-dup do not need an enumeration.
 *
 * - Enumeration type @n
 *   An 'enumeration type' determines how a collection of component object
 *   identifiers (cob-fid) is specified. For example, it may be specified as a
 *   list or by means of some linear formula.
 *
 * @section layout-types-supported Supported layout and enum types
 * - Layout types supported currently are:
 *   - PDCLUST @n
 *     This layout type applies parity declustering feature to the striping
 *     process. Parity declustering feature is to keep the rebuild overhead low
 *     by striping a file over more servers or drives than there are units in
 *     the parity group.
 *   - COMPOSITE @n
 *     This layout type partitions a file or a part of the file into
 *     various segments while each of those segment uses a different layout.
 *
 * - Enumeration types (also referred as 'enum types') supported currently are:
 *   - LINEAR @n
 *     A layout with LINEAR enumeration type uses a formula to enumerate all
 *     its component object identifiers.
 *   - LIST @n
 *     A layout with LIST enumeration type uses a list to enumerate all its
 *     component object identifiers.
 *
 * @section layout-managed-resources Layout Managed Resources
 * - A layout as well as a layout-id are resources (managed by the 'Mero
 *   Resource Manager').
 * - Layout being a resource, it can be cached by the clients and can be
 *   revoked when it is changed.
 * - Layout Id being a resource, a client can cache a range of layout ids that
 *   it uses to create new layouts without contacting the server.
 * - A layout can be assigned to a file both by the server and the client.
 *
 * @section layout-operations-sequence Sequence Of Layout Operation
 * The sequence of operation related to domain initialization/finalisation,
 * layout type and enum type registration and unregistration is as follows:
 * - Initialise m0_layout_domain object.
 * - Register layout types and enum types using
 *   m0_layout_standard_types_register().
 * - Perform various required operations on the in-memory layouts including
 *   the usage of m0_pdclust_build(), m0_layout_get(), m0_layout_put(),
 *   m0_layout_encode(), m0_layout_decode(), m0_layout_enum_nr(),
 *   m0_layout_enum_get().
 * - Perform various BE-related operations on the layouts, like:
 *   m0_layout_lookup(), m0_layout_add(), m0_layout_update(),
 *   m0_layout_delete().
 * - Perform various operations on layout instances including the usage of
 *   m0_layout_instance_build(), m0_layout_instance_fini() and the relevant
 *   instance type specific operations. (Creating a layout instance is a way
 *   of associating a layout with a particular user, for example a file.)
 * - Finalise all the layout instances.
 * - Finalise all the in-memory layouts. (The layouts can continue to exist in
 *   the layout DB, even if the resepctive layout types and enum types are to
 *   be unregistered and the domain is to be finalised.)
 * - Unregister layout types and enum types using
 *   m0_layout_all_types_unregister().
 * - Finalise m0_layout_domain object.
 *
 * @section layout-client-server-access Client Server Access to APIs
 * Regarding client/server access to various APIs from layout and layout-DB
 * modules:
 * - The APIs exported through layout.h are available both to the client and
 *   the server.
 * - the APIs exported through layout_db.h are available only to the server.
 *
 * @{
 */

/* import */
#include "lib/types.h"  /* uint64_t */
#include "lib/tlist.h"  /* struct m0_tl */
#include "lib/mutex.h"  /* struct m0_mutex */
#include "lib/arith.h"  /* M0_IS_8ALIGNED */
#include "lib/refs.h"   /* struct m0_ref */

#include "fid/fid.h"    /* struct m0_fid */
#include "layout/layout_pver.h" /* m0_layout_init_by_pver() */

struct m0_bufvec_cursor;
struct m0_be_tx;

/* export */
struct m0_layout_domain;
struct m0_layout;
struct m0_layout_ops;
struct m0_layout_type;
struct m0_layout_type_ops;
struct m0_layout_enum;
struct m0_layout_enum_ops;
struct m0_layout_enum_type;
struct m0_layout_enum_type_ops;
struct m0_striped_layout;
struct m0_layout_instance;
struct m0_layout_instance_ops;
struct m0_layout_rec;
struct m0_pools_common;

/**
 * Operation on a layout record, performed through either m0_layout_decode()
 * or m0_layout_encode() routines.
 * M0_LXO_BUFFER_OP indicates that m0_layout_decode()/m0_layout_encode() has
 * to operate upon a buffer.
 */
enum m0_layout_xcode_op {
        M0_LXO_BUFFER_OP, /* Operate on a buffer. */
        M0_LXO_DB_LOOKUP, /* Lookup for layout from the DB. */
        M0_LXO_DB_ADD,    /* Add layout to the DB. */
        M0_LXO_DB_UPDATE, /* Update layout in the DB. */
        M0_LXO_DB_DELETE  /* Delete layout from the DB. */
};

enum {
	M0_LAYOUT_TYPE_MAX      = 32,
	M0_LAYOUT_ENUM_TYPE_MAX = 32
};

enum { M0_DEFAULT_LAYOUT_ID = 1 };

/**
 * Layout domain.
 * It includes a pointer to the primary database table "layouts" and some
 * related parameters. ld_type_data[] and ld_enum_data[] store pointers to
 * the auxiliary tables applicable, if any, for the various layout types and
 * enum types.
 */
struct m0_layout_domain {
	/** Layout types array. */
	struct m0_layout_type      *ld_type[M0_LAYOUT_TYPE_MAX];

	/** Enumeration types array. */
	struct m0_layout_enum_type *ld_enum[M0_LAYOUT_ENUM_TYPE_MAX];

	/** List of pointers for layout objects associated with this domain. */
	struct m0_tl                ld_layout_list;

	/** Layout type specific data. */
	void                       *ld_type_data[M0_LAYOUT_TYPE_MAX];

	/** Layout enum type specific data. */
	void                       *ld_enum_data[M0_LAYOUT_ENUM_TYPE_MAX];

	/** Maximum possible size for a record in the layouts table. */
	m0_bcount_t                 ld_max_recsize;

	/**
	 * Lock to protect an instance of m0_layout_domain, including all
	 * its members.
	 */
	struct m0_mutex             ld_lock;
};

/**
 * In-memory representation of a layout.
 */
struct m0_layout {
	/** Layout id. */
	uint64_t                     l_id;

	/** Layout type. */
	struct m0_layout_type       *l_type;

	/** Layout domain this layout object is part of. */
	struct m0_layout_domain     *l_dom;

	/** Reference counter for caching a layout. */
	struct m0_ref                l_ref;

	/**
	 * Layout user count, indicating how many users this layout has.
	 * For example files, other composite layouts using this layout.
	 * A layout can not be deleted from the layout DB as long as its
	 * user count is non-zero.
	 */
	uint32_t                     l_user_count;
	/**
	 * Lock to protect a m0_layout instance, including all its direct and
	 * indirect members.
	 */
	struct m0_mutex              l_lock;
	/** Layout operations vector. */
	const struct m0_layout_ops  *l_ops;
	/** Magic number set while m0_layout object is initialised. */
	uint64_t                     l_magic;
	/**
	 * Linkage used for maintaining list of the layout objects stored in
	 * the m0_layout_domain object.
	 */
	struct m0_tlink              l_list_linkage;
	/**
	 * A link to the in-memory copy of the pool version associated with
	 * this layout.
	 */
	struct m0_pool_version      *l_pver;
};

struct m0_layout_ops {
	/**
	 * Finalises the type specific layout object. It involves finalising
	 * its enumeration object, if applicable.
	 *
	 * Dual to the sequence "lto_allocate() followed by the type specific
	 * populate operation", but the user is not expected to invoke this
	 * method explicitly. It is called implicitly when the last reference
	 * on the layout object is released.
	 * @see m0_layout_put().
	 */
	void        (*lo_fini)(struct m0_ref *ref);

	/**
	 * Finalises the layout object that is only allocated and not
	 * populated. Since it is not populated, it does not contain
	 * enumeration object.
	 * Dual to lto_allocate() when an allocated layout object can
	 * not be populated for some reason. In the other regular cases, dual
	 * to the sequence of "lto_allocate() followed by the type specific
	 * populate operation" is lo_fini().
	 */
	void        (*lo_delete)(struct m0_layout *l);

	/**
	 * Returns size of the record stored in the "layouts" (primary) table,
	 * for the specified layout. It includes the size required for
	 * storing the generic data, the layout type specific data and the enum
	 * type specific data.
	 *
	 * @invariant l->l_ops->lo_recsize(l)
	 *            <= l->l_type->lt_ops->lto_max_recsize(l->l_dom);
	 */
	m0_bcount_t (*lo_recsize)(const struct m0_layout *l);

	/**
	 * Allocates and builds a layout instance using the supplied layout.
	 * Increments a reference on the supplied layout.
	 */
	int         (*lo_instance_build)(struct m0_layout           *l,
					 const struct m0_fid        *fid,
					 struct m0_layout_instance **linst);

	/**
	 * Continues building the in-memory layout object from its
	 * representation either 'stored in the Layout DB' or 'received through
	 * the buffer'.
	 *
	 * @param op This enum parameter indicates what, if a DB operation is
	 * to be performed on the layout record and it could be LOOKUP if at
	 * all. If it is BUFFER_OP, then the layout is decoded from its
	 * representation received through the buffer.
	 *
	 * @pre M0_IN(op, (M0_LXO_DB_LOOKUP, M0_LXO_BUFFER_OP))
	 * @pre ergo(op == M0_LXO_DB_LOOKUP, tx != NULL)
	 * @post
	 * - ergo(rc == 0, pdclust_invariant(pl))
	 * - The cursor cur is advanced by the size of the data that is
	 *   read from it.
	 */
	int         (*lo_decode)(struct m0_layout *l,
				 struct m0_bufvec_cursor *cur,
				 enum m0_layout_xcode_op op,
				 struct m0_be_tx *tx,
				 uint32_t user_count);

	/**
	 * Continues to use the in-memory layout object and
	 * - Either adds/updates/deletes it to/from the Layout DB
	 * - Or converts it to a buffer.
	 * @param op This enum parameter indicates what is the DB operation
	 * to be performed on the layout record if at all and it could be one
	 * of ADD/UPDATE/DELETE. If it is BUFFER_OP, then the layout is stored
	 * in the buffer.
	 *
	 * @pre M0_IN(op, (M0_LXO_DB_ADD, M0_LXO_DB_UPDATE,
	 *                 M0_LXO_DB_DELETE, M0_LXO_BUFFER_OP))
	 * @pre ergo(op != M0_LXO_BUFFER_OP, tx != NULL)
	 */
	int         (*lo_encode)(struct m0_layout *l,
				 enum m0_layout_xcode_op op,
				 struct m0_be_tx *tx,
				 struct m0_bufvec_cursor *out);
};

/**
 * Structure specific to a layout type.
 * There is an instance of m0_layout_type for each one of the layout types.
 * For example, for PDCLUST and COMPOSITE layout types.
 * Any layout type can be registered with only one domain, at a time.
 */
struct m0_layout_type {
	/** Layout type name. */
	const char                      *lt_name;

	/** Layout type id. */
	uint32_t                         lt_id;

	/**
	 * Layout type reference count, indicating 'how many in-memory layout
	 * objects using this layout type' exist in 'the domain the layout type
	 * is registered with'.
	 */
	uint32_t                         lt_ref_count;

	/** Layout type operations vector. */
	const struct m0_layout_type_ops *lt_ops;
};

struct m0_layout_type_ops {
	/**
	 * Allocates layout type specific schema data.
	 * For example, comp_layout_ext_map table.
	 */
	int         (*lto_register)(struct m0_layout_domain *dom,
				    const struct m0_layout_type *lt);

	/** Deallocates layout type specific schema data. */
	void        (*lto_unregister)(struct m0_layout_domain *dom,
				      const struct m0_layout_type *lt);

	/**
	 * Returns the maximum possible size for the record stored in the
	 * "layouts" (primary) table for any layout. It includes the size
	 * required for storing the generic data, the layout type specific
	 * data and the enum type specific data.
	 */
	m0_bcount_t (*lto_max_recsize)(struct m0_layout_domain *dom);

	/**
	 * Allocates an instance of some layout-type specific data-type
	 * which embeds m0_layout and stores the resultant m0_layout object
	 * in the parameter out.
	 * @post ergo(result == 0, *out != NULL &&
	 *                        (*out)->l_ops != NULL &&
	 *                         m0_mutex_is_locked(&l->l_lock))
	 */
	int         (*lto_allocate)(struct m0_layout_domain *dom,
				    uint64_t lid,
				    struct m0_layout **out);
};

/** Layout enumeration. */
struct m0_layout_enum {
	/** Layout enumeration type. */
	struct m0_layout_enum_type      *le_type;

	/** Layout domain */
	struct m0_layout_domain         *le_dom;
	/**
	 * Flag indicating if this enum object is associated with any striped
	 * layout object. This flag is used in invariants only.
	 */
	bool                             le_sl_is_set;

	/** Striped layout object this enum is associated with. */
	struct m0_striped_layout        *le_sl;

	/** Enum operations vector. */
	const struct m0_layout_enum_ops *le_ops;

	/** Magic number set while m0_layout_enum object is initialised. */
	uint64_t                         le_magic;
};

struct m0_layout_enum_ops {
	/** Returns number of objects in the enumeration. */
	uint32_t    (*leo_nr)(const struct m0_layout_enum *e);

	/**
	 * Returns idx-th object in the enumeration.
	 * @pre idx < e->l_enum_ops->leo_nr(e)
	 */
	void        (*leo_get)(const struct m0_layout_enum *e, uint32_t idx,
			       const struct m0_fid *gfid, struct m0_fid *out);

	/**
	 * Returns size of the part of the layouts table record required to
	 * store enum details, for the specified enumeration object.
	 *
	 * @invariant e->le_ops->leo_recsize(e)
	 *            <= e->le_type->let_ops->leto_max_recsize();
	 */
	m0_bcount_t (*leo_recsize)(struct m0_layout_enum *e);

	/**
	 * Finalises the enum object.
	 *
	 * Dual to enum type specific build procedure but not to be invoked
	 * directly by the user in regular course of action since enum object
	 * is finalised internally as a part finalising layout object.
	 * This interface is required to be used by an external user in cases
	 * where layout build operation fails and the user (for example m0t1fs)
	 * needs to get rid of the enumeration object created prior to
	 * attempting the layout build operation.
	 */
	void        (*leo_fini)(struct m0_layout_enum *e);

	/**
	 * Finalises the enum object that is only allocated and not
	 * populated.
	 */
	void        (*leo_delete)(struct m0_layout_enum *e);

	/**
	 * Continues building the in-memory layout object, the enum part of it
	 * specifically, either from the buffer or from the DB.
	 *
	 * @param op This enum parameter indicates what if a DB operation is
	 *           to be performed on the layout record and it could be
	 *           LOOKUP if at all. If it is BUFFER_OP, then the layout is
	 *           decoded from its representation received through the
	 *           buffer.
	 * @pre M0_IN(op, (M0_LXO_DB_LOOKUP, M0_LXO_BUFFER_OP))
	 * @pre ergo(op == M0_LXO_DB_LOOKUP, tx != NULL)
	 * @post The cursor cur is advanced by the size of the data that is
	 * read from it.
	 */
	int         (*leo_decode)(struct m0_layout_enum *e,
				  struct m0_bufvec_cursor *cur,
				  enum m0_layout_xcode_op op,
				  struct m0_be_tx *tx,
				  struct m0_striped_layout *stl);

	/**
	 * Continues to use the in-memory layout object, the enum part of it
	 * specifically and either 'stores it in the Layout DB' or 'converts
	 * it to a buffer'.
	 *
	 * @param op This enum parameter indicates what is the DB operation to
	 *           be performed on the layout record if at all and it could
	 *           be one of ADD/UPDATE/DELETE. If it is BUFFER_OP, then the
	 *           layout is converted into a buffer.
	 *
	 * @pre M0_IN(op, (M0_LXO_DB_ADD, M0_LXO_DB_UPDATE,
	 *                 M0_LXO_DB_DELETE, M0_LXO_BUFFER_OP))
	 * @pre ergo(op != M0_LXO_BUFFER_OP, tx != NULL)
	 */
	int         (*leo_encode)(const struct m0_layout_enum *le,
				  enum m0_layout_xcode_op op,
				  struct m0_be_tx *tx,
				  struct m0_bufvec_cursor *out);
};

/**
 * Finalises the enum object.
 * Dual to enum type specific build procedure.
 * The user will not invoke this API explicitly if the enum is used as a part
 * of some layout object. Layout finalisation will take care of enum
 * finalisation in that case. This API is expected to be used only in case
 * the enum could not be made part of any layout for some reason.
 * @see m0_layout_put()
 */
M0_INTERNAL void m0_layout_enum_fini(struct m0_layout_enum *le);

/**
 * Structure specific to a layout enumeration type.
 * There is an instance of m0_layout_enum_type for each one of enumeration
 * types. For example, for LINEAR and LIST enumeration types.
 * Any enumeration type can be registered with only one domain, at a time.
 */
struct m0_layout_enum_type {
	/** Layout enumeration type name. */
	const char                           *let_name;

	/** Layout enumeration type id. */
	uint32_t                              let_id;

	/**
	 * Enum type reference count, indicating 'how many in-memory enum
	 * objects using this enum type' exist in 'the domain the enum type is
	 * registered with'.
	 */
	uint32_t                              let_ref_count;

	/** Layout enumeration type operations vector. */
	const struct m0_layout_enum_type_ops *let_ops;
};

struct m0_layout_enum_type_ops {
	/**
	 * Allocates enumeration type specific schema data.
	 * For example, cob_lists table.
	 */
	int         (*leto_register)(struct m0_layout_domain *dom,
				     const struct m0_layout_enum_type *et);

	/** Deallocates enumeration type specific schema data. */
	void        (*leto_unregister)(struct m0_layout_domain *dom,
				       const struct m0_layout_enum_type *et);

	/**
	 * Returns applicable max record size for the part of the layouts
	 * table record, required to store enum details.
	 */
	m0_bcount_t (*leto_max_recsize)(void);

	/**
	 * Allocates and builds an instance of some enum-type specific
	 * data-type which embeds m0_layout_enum and stores the resultant
	 * m0_layout_enum object in the parameter out.
	 * @post ergo(rc == 0, *out != NULL && (*out)->le_ops != NULL)
	 */
	int         (*leto_allocate)(struct m0_layout_domain *dom,
				     struct m0_layout_enum **out);
};

/** Layout using enumeration. */
struct m0_striped_layout {
	/** Super class. */
	struct m0_layout       sl_base;

	/** Layout enumeration. */
	struct m0_layout_enum *sl_enum;
};

/**
 * Layout instance for a particular file.
 *
 * On a client, this structure is embedded in m0t1fs inode.
 */
struct m0_layout_instance {
	/** (Global) fid of the file this instance is associated with. */
	struct m0_fid                        li_gfid;

	/** Layout used for the file referred by li_gfid. */
	struct m0_layout                    *li_l;

	/** Layout operations vector. */
	const struct m0_layout_instance_ops *li_ops;

	/** Magic number set while m0_layout_instance object is initialised. */
	uint64_t                             li_magic;
};

struct m0_layout_instance_ops {
	/**
	 * Finalises the type specifc layout instance object.
	 *
	 * Releases a reference on the layout object that was obtained through
	 * the layout instance type specific build method, referred by
	 * l->l_ops->lo_instance_build(), for example pdclust_instance_build().
	 */
	void (*lio_fini)(struct m0_layout_instance *li);

	/**
	 * Returns enum object embedded in the layout referred by
	 * the layout instance.
	 */
	struct m0_layout_enum *
		(*lio_to_enum)(const struct m0_layout_instance *li);
};

/**
 * Returns enum object embedded in the layout referred by the layout instance.
 */
M0_INTERNAL struct m0_layout_enum *m0_layout_instance_to_enum(const struct
							      m0_layout_instance
							      *li);

/**
 * Allocates and builds a layout instance using the supplied layout;
 * Acquires an additional reference on the layout pointed by 'l'.
 * @post ergo(rc == 0, m0_ref_read(&l->l_ref) > 1)
 *
 * Dual to m0_layout_instance_fini()
 */
M0_INTERNAL int m0_layout_instance_build(struct m0_layout *l,
					 const struct m0_fid *fid,
					 struct m0_layout_instance **out);

/**
 * Finalises the layout instance object; releases reference on the layout
 * that was obtained through m0_layout_instance_build().
 *
 * Dual to m0_layout_instance_build()
 */
M0_INTERNAL void m0_layout_instance_fini(struct m0_layout_instance *li);

/**
 * layouts table.
 * Key is uint64_t, value obtained from m0_layout::l_id.
 * @note This structure needs to be maintained as 8 bytes aligned.
 */
struct m0_layout_rec {
	/**
	 * Layout type id.
	 * Value obtained from m0_layout_type::lt_id.
	 */
	uint32_t  lr_lt_id;

	/**
	 * Layout user count, indicating number of users for this layout.
	 * Value obtained from m0_layout::l_user_count.
	 */
	uint32_t  lr_user_count;

	/**
	 * Layout type specific payload.
	 * Contains attributes specific to the applicable layout type and/or
	 * applicable to the enumeration type, if applicable.
	 */
	char      lr_data[0];
};
M0_BASSERT(M0_IS_8ALIGNED(sizeof(struct m0_layout_rec)));

M0_INTERNAL int m0_layouts_init(void);
M0_INTERNAL void m0_layouts_fini(void);

/**
 * Initialises layout domain - Initialises arrays to hold the objects for
 * layout types and enum types.
 */
M0_INTERNAL int m0_layout_domain_init(struct m0_layout_domain *dom);

/**
 * Finalises the layout domain.
 * Dual to m0_layout_domain_init().
 * @pre All the layout types and enum types should be unregistered.
 */
M0_INTERNAL void m0_layout_domain_fini(struct m0_layout_domain *dom);

/**
   Release all layouts in the domain.
 */
M0_INTERNAL void m0_layout_domain_cleanup(struct m0_layout_domain *dom);

/** Registers all the standard layout types and enum types. */
M0_INTERNAL int m0_layout_standard_types_register(struct m0_layout_domain *dom);

/** Unrgisters all the standard layout types and enum types. */
M0_INTERNAL void m0_layout_standard_types_unregister(struct m0_layout_domain
						     *dom);

/**
 * Registers a new layout type with the layout types maintained by
 * m0_layout_domain::ld_type[] and initialises layout type specific tables,
 * if applicable.
 */
M0_INTERNAL int m0_layout_type_register(struct m0_layout_domain *dom,
					struct m0_layout_type *lt);

/**
 * Unregisters a layout type from the layout types maintained by
 * m0_layout_domain::ld_type[] and finalises layout type specific tables,
 * if applicable.
 */
M0_INTERNAL void m0_layout_type_unregister(struct m0_layout_domain *dom,
					   struct m0_layout_type *lt);

/**
 * Registers a new enumeration type with the enumeration types
 * maintained by m0_layout_domain::ld_enum[] and initialises enum type
 * specific tables, if applicable.
 */
M0_INTERNAL int m0_layout_enum_type_register(struct m0_layout_domain *dom,
					     struct m0_layout_enum_type *et);

/**
 * Unregisters an enumeration type from the enumeration types
 * maintained by m0_layout_domain::ld_enum[] and finalises enum type
 * specific tables, if applicable.
 */
M0_INTERNAL void m0_layout_enum_type_unregister(struct m0_layout_domain *dom,
						struct m0_layout_enum_type *et);

/**
 * Adds the layout to domain list.
 */
M0_INTERNAL void m0_layout_add(struct m0_layout_domain *dom, struct m0_layout *l);

/**
 * Returns the layout object if it exists in memory by incrementing a reference
 * on it, else returns NULL.
 * This interface does not attempt to read the layout from the layout database.
 *
 * @post ergo(@ret != NULL, m0_ref_read(l->l_ref) > 1)
 *
 * @note This API is required specifically on the client in the absence of
 * layout DB APIs, m0_layout_lookup() to be specific.
 */
M0_INTERNAL struct m0_layout *m0_layout_find(struct m0_layout_domain *dom,
					     uint64_t lid);

/**
 * Find optimal layout id by pver and buffersize. Note, that returned lid
 * is _not_ pver+lid hash (client-side style id) but rather server-side
 * one (currently in range 1-14).
 *
 * If buffer is too small then returns the first very lid. If too large then
 * returns the layout id describing biggest possible unit_size.
 *
 * Always returns valid layout id.
 */
M0_INTERNAL uint64_t m0_layout_find_by_buffsize(struct m0_layout_domain *dom,
						struct m0_fid *pver,
						size_t buffsize);
/**
 * Acquires an additional reference on the layout object.
 * @see m0_layout_put()
 * @see m0_layout_find()
 */
M0_INTERNAL void m0_layout_get(struct m0_layout *l);

/**
 * Releases a reference on the layout object.
 * If it is the last reference being released, then it removes the layout
 * entry from the layout list maintained in the layout domain and then
 * finalises the layout along with finalising its enumeration object, if
 * applicable.
 * @see m0_layout_get()
 * @see m0_layout_find()
 */
M0_INTERNAL void m0_layout_put(struct m0_layout *l);

/**
 * Increments layout user count.
 * This API shall be used by the user to associate a specific layout with some
 * user of that layout, for example, while creating 'a file using that layout'.
 */
M0_INTERNAL void m0_layout_user_count_inc(struct m0_layout *l);

/**
 * Decrements layout user count.
 * This API shall be used by the user to dissociate a layout from some user of
 * that layout, for example, while deleting 'a file using that layout'.
 */
M0_INTERNAL void m0_layout_user_count_dec(struct m0_layout *l);

/**
 * This method
 * - Either continues to build an in-memory layout object from its
 *   representation 'stored in the Layout DB'
 * - Or builds an in-memory layout object from its representation 'received
 *   through a buffer'.
 *
 * Two use cases of m0_layout_decode()
 * - Server decodes an on-disk layout record by reading it from the Layout
 *   DB, into an in-memory layout structure, using m0_layout_lookup() which
 *   internally calls m0_layout_decode().
 * - Client decodes a buffer received over the network, into an in-memory
 *   layout structure, using m0_layout_decode().
 *
 * @param cur Cursor pointing to a buffer containing serialised representation
 * of the layout. Regarding the size of the buffer:
 * - In case m0_layout_decode() is called through m0_layout_add(), then the
 *   buffer should be containing all the data that is read specifically from
 *   the layouts table. It means its size needs to be at the most the size
 *   returned by m0_layout_max_recsize().
 * - In case m0_layout_decode() is called by some other caller, then the
 *   buffer should be containing all the data belonging to the specific layout.
 *   It may include data that spans over tables other than layouts as well. It
 *   means its size may need to be even more than the one returned by
 *   m0_layout_max_recsize(). For example, in case of LIST enumeration type,
 *   the buffer needs to contain the data that is stored in the cob_lists table.
 *
 * @param op This enum parameter indicates what is the DB operation to be
 * performed on the layout record. It could be LOOKUP if at all a DB operation.
 * If it is BUFFER_OP, then the layout is decoded from its representation
 * received through the buffer.
 *
 * @pre
 * - m0_layout__allocated_invariant(l) implying:
 *   - m0_ref_read(l->l_ref) == 1 and
 *   - m0_mutex_is_locked(&l->l_lock)
 * - The buffer pointed by cur contains serialised representation of the whole
 *   layout in case op is M0_LXO_BUFFER_OP. It contains the data for the
 *   layout read from the primary table viz. "layouts" in case op is
 *   M0_LXO_DB_LOOKUP.
 *
 * @post Layout object is fully built (along with enumeration object being
 * built if applicable) along with its ref count being intialised to 1. User
 * needs to explicitly release this reference so as to delete this in-memory
 * layout.
 * - ergo(rc == 0, m0_layout__invariant(l))
 * - ergo(rc != 0, m0_layout__allocated_invariant(l)
 * - m0_mutex_is_locked(&l->l_lock)
 * - The cursor cur is advanced by the size of the data that is read from it.
 * - ergo(rc == 0, m0_ref_read(l->l_ref) == 1)
 *
 * @see m0_layout_put()
 */
M0_INTERNAL int m0_layout_decode(struct m0_layout *l,
				 struct m0_bufvec_cursor *cur,
				 enum m0_layout_xcode_op op,
				 struct m0_be_tx *tx);

/**
 * This method uses an in-memory layout object and
 * - Either adds/updates/deletes it to/from the Layout DB
 * - Or converts it to a buffer.
 *
 * Two use cases of m0_layout_encode()
 * - Server encodes an in-memory layout object into a buffer using
 *   m0_layout_encode(), so as to send it to the client.
 * - Server encodes an in-memory layout object using one of m0_layout_add(),
 *   m0_layout_update() or m0_layout_delete() and adds/updates/deletes
 *   it to or from the Layout DB.
 *
 * @param op This enum parameter indicates what is the DB operation to be
 * performed on the layout record if at all a DB operation which could be
 * one of ADD/UPDATE/DELETE. If it is BUFFER_OP, then the layout is stored
 * in the buffer provided by the caller.
 *
 * @param out Cursor pointing to a buffer. Regarding the size of the buffer:
 * - In case m0_layout_encode() is called through m0_layout_add()|
 *   m0_layout_update()|m0_layout_delete(), then the buffer size should be
 *   large enough to contain the data that is to be written specifically to
 *   the layouts table. It means it needs to be at the most the size returned
 *   by m0_layout_max_recsize().
 * - In case m0_layout_encode() is called by some other caller, then the
 *   buffer size should be large enough to contain all the data belonging to
 *   the specific layout. It means the size required may even be more than
 *   the one returned by m0_layout_max_recsize(). For example, in case of LIST
 *   enumeration type, some data goes into table other than layouts, viz.
 *   cob_lists table.
 *
 * @pre
 * - m0_layout__invariant(l)
 * - m0_mutex_is_locked(&l->l_lock)
 * @post
 * - If op is is either for M0_LXO_DB_<ADD|UPDATE|DELETE>, the respective DB
 *   operation is continued.
 * - If op is M0_LXO_BUFFER_OP, the buffer contains the serialised
 *   representation of the whole layout.
 */
M0_INTERNAL int m0_layout_encode(struct m0_layout *l,
				 enum m0_layout_xcode_op op,
				 struct m0_be_tx *tx,
				 struct m0_bufvec_cursor *out);

/**
 * Returns maximum possible size for a record in the layouts table (without
 * considering the data in the tables other than layouts), from what is
 * maintained in the m0_layout_domain object.
 */
M0_INTERNAL m0_bcount_t m0_layout_max_recsize(const struct m0_layout_domain
					      *dom);

/** Returns m0_striped_layout object for the specified m0_layout object. */
M0_INTERNAL struct m0_striped_layout *m0_layout_to_striped(const struct
							   m0_layout *l);

/**
 * Returns m0_layout_enum object for the specified m0_striped_layout
 * object.
 */
M0_INTERNAL struct m0_layout_enum *m0_striped_layout_to_enum(const struct
							     m0_striped_layout
							     *stl);

M0_INTERNAL struct m0_layout_enum *m0_layout_to_enum(const struct m0_layout *l);

/** Returns number of objects in the enumeration. */
M0_INTERNAL uint32_t m0_layout_enum_nr(const struct m0_layout_enum *e);


/**
 * Returns idx-th object in the layout enumeration.
 *
 * Beware that this function is currently incorrect and may return unexpected
 * results. The recommanded way to convert an object fid to a component fid is
 * the function m0_poolmach_gob2cob in pool/poolmachine.h
 *
 * [ref] https://jira.xyratex.com/browse/MERO-1834
 *
 * @see m0_poolmach_gob2cob()
 */
M0_INTERNAL void m0_layout_enum_get(const struct m0_layout_enum *e,
				    uint32_t idx,
				    const struct m0_fid *gfid,
				    struct m0_fid *out);

/** Returns the target index, given target fid. */
M0_INTERNAL uint32_t m0_layout_enum_find(const struct m0_layout_enum *e,
					 const struct m0_fid *gfid,
					 const struct m0_fid *target);

/** @} end group layout */

/* __MERO_LAYOUT_LAYOUT_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
