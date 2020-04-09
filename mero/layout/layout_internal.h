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
 * Original author: Trupti Patil <trupti_patil@xyratex.com>
 * Original creation date: 01/11/2011
 */

#pragma once

#ifndef __MERO_LAYOUT_LAYOUT_INTERNAL_H__
#define __MERO_LAYOUT_LAYOUT_INTERNAL_H__

/**
 * @addtogroup layout
 * @{
 */

/* import */
struct m0_layout_domain;
struct m0_layout;
struct m0_layout_ops;
struct m0_layout_type;
struct m0_layout_enum;
struct m0_layout_enum_ops;
struct m0_layout_enum_type;
struct m0_striped_layout;
struct m0_layout_instance;
struct m0_layout_instance_ops;
struct m0_fid;

enum {
	/** Invalid layout id. */
	LID_NONE                   = 0,

	/** Flag used during table creation, using m0_table_init() */
	DEFAULT_DB_FLAG            = 0,

	/**
	 * Maximum limit on the number of COB entries those can be stored
	 * inline into the layouts table, while rest of those are stored into
	 * the cob_lists table.
	 */
	LDB_MAX_INLINE_COB_ENTRIES = 20,

	/*
	 * Simulation for m0_table_init() facing error in
	 * m0_layout_domain_init().
	 */
	L_TABLE_INIT_ERR           = -501,

	/*
	 * Simulation for lto_register() facing error in
	 * m0_layout_type_register().
	 */
	LTO_REG_ERR                = -502,

	/*
	 * Simulation for leto_register() facing error in
	 * m0_layout_enum_type_register().
	 */
	LETO_REG_ERR               = -503,

	/*
	 * Simulation for lo_decode() facing error in
	 * m0_layout_decode().
	 */
	LO_DECODE_ERR              = -504,

	/*
	 * Simulation for lo_encode() facing error in
	 * m0_layout_encode().
	 */
	LO_ENCODE_ERR              = -505,

	/*
	 * Simulation for m0_table_update() facing error in
	 * m0_layout_update().
	 */
	L_TABLE_UPDATE_ERR         = -506
};

M0_INTERNAL bool m0_layout__domain_invariant(const struct m0_layout_domain
					     *dom);
M0_INTERNAL bool m0_layout__allocated_invariant(const struct m0_layout *l);
M0_INTERNAL bool m0_layout__invariant(const struct m0_layout *l);
M0_INTERNAL bool m0_layout__enum_invariant(const struct m0_layout_enum *le);
M0_INTERNAL bool m0_layout__striped_allocated_invariant(const struct
							m0_striped_layout *s);
M0_INTERNAL bool m0_layout__striped_invariant(const struct m0_striped_layout
					      *stl);

M0_INTERNAL struct m0_layout *m0_layout__list_lookup(const struct
						     m0_layout_domain *dom,
						     uint64_t lid,
						     bool ref_increment);

M0_INTERNAL void m0_layout__init(struct m0_layout *l,
				 struct m0_layout_domain *dom,
				 uint64_t lid,
				 struct m0_layout_type *type,
				 const struct m0_layout_ops *ops);
M0_INTERNAL void m0_layout__fini(struct m0_layout *l);
M0_INTERNAL void m0_layout__populate(struct m0_layout *l, uint32_t user_count);
M0_INTERNAL void m0_layout__delete(struct m0_layout *l);

M0_INTERNAL void m0_layout__striped_init(struct m0_striped_layout *stl,
					 struct m0_layout_domain *dom,
					 uint64_t lid,
					 struct m0_layout_type *type,
					 const struct m0_layout_ops *ops);
M0_INTERNAL void m0_layout__striped_fini(struct m0_striped_layout *stl);
M0_INTERNAL void m0_layout__striped_populate(struct m0_striped_layout *str_l,
					     struct m0_layout_enum *e,
					     uint32_t user_count);
M0_INTERNAL void m0_layout__striped_delete(struct m0_striped_layout *stl);

M0_INTERNAL void m0_layout__enum_init(struct m0_layout_domain *dom,
				      struct m0_layout_enum *le,
				      struct m0_layout_enum_type *et,
				      const struct m0_layout_enum_ops *ops);
M0_INTERNAL void m0_layout__enum_fini(struct m0_layout_enum *le);

M0_INTERNAL void m0_layout__log(const char         *fn_name,
				const char         *err_msg,
				uint64_t            lid,
				int                 rc);

M0_INTERNAL m0_bcount_t m0_layout__enum_max_recsize(struct m0_layout_domain
						    *dom);

M0_INTERNAL void m0_layout__instance_init(struct m0_layout_instance *li,
					  const struct m0_fid *gfid,
					  struct m0_layout *l,
					  const struct m0_layout_instance_ops
					  *ops);
M0_INTERNAL void m0_layout__instance_fini(struct m0_layout_instance *li);
M0_INTERNAL bool m0_layout__instance_invariant(const struct m0_layout_instance
					       *li);

/** @} end group layout */

/* __MERO_LAYOUT_LAYOUT_INTERNAL_H__ */
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
