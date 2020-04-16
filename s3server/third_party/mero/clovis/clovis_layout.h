/* -*- C -*- */
/*
 * COPYRIGHT 2017 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Authors: Sining Wu       <sining.wu@seagate.com>
 *	    Pratik Shinde   <pratik.shinde@seagate.com>
 *	    Vishwas Bhat    <vishwas.bhat@seagate.com>
 * Original creation date: 05-May-2017
 */

#pragma once

#ifndef __MERO_CLOVIS_CLOVIS_LAYOUT_H__
#define __MERO_CLOVIS_CLOVIS_LAYOUT_H__

#include "clovis.h"
#include "clovis_internal.h"
#include "lib/types.h"

/**
 * A few notes on current version of layout and composite layout implementation.
 *
 * Notations: an object with default de-clustered parity layout is simply called
 * object, while an object with composite/capture layout is called
 * composite/capture object.
 *
 * (1) An object has an assocaited layout (m0_clovis_object::ob_layout). we
 *     didn't associate layout with entity as not all entity needs layout
 *     information and dix index keeps and maintains the layout details itself.
 *
 * (2) Currently, mero/clovis stores layout id and pool version as object
 *     attributes in IO or MD services. Layout API is NOT changing this for
 *     'normal' objects. Composite or capture objects store extra layout
 *     information in global layout index and layout specific indices (such
 *     as composite extent indices.). This method is chosen because:
 *       - If storing all layouts in global layout index, it implicitly create
 *         a tight binding between dix and applications, but not all
 *         applications need dix.
 *       - Changes to current object creation/deletion is minor.
 *       - If an object stores de-clustered parity layout in global layout index
 *         as well, creating/deleting an object includes one more round of rpc
 *         process to cas services.
 *     Note: the chosen method above is a temporary solution and is subject to
 *     change in the future when all object layouts are stored in dix.
 *
 * (3) m0_clovis_obj_op() for IO is not changed to include layout information as
 *     an application like HSM may change an object's layout from default
 *     de-clustered parity layout to composite layout and this change is
 *     hidden from other applications. Other applications issue IO ops like
 *     before. So clovis needs a way to tell the layout type of an object and
 *     retrieve details accrodingly.
 *
 *     Clovis makes use of layout id attribute and stores the layout type in
 *     the highest 16 bits of layout id.
 *
 * (4) m0_clovis_entity_{create|delete}() APIs are not changed. An object with
 *     default de-clustered parity layout is first created, if the apaplication
 *     wants to set the object to different layout such as composite layout,
 *     it does so by issuing a M0_CLOVIS_EO_LAYOUT_SET op.
 *
 * (5) 2 new ops M0_CLOVIS_EO_LAYOUT_{GET|SET} together with
 *     m0_clovis_layout_op() API are provided to allocated and initialised
 *     layout op. A few helper APIs are also provided to allocate/free layout.
 *
 * (6) Composite extents are stored in internally created indices and
 *     m0_clovis_composite_layer_idx() is provided to return the index.
 *     Applications can query it using normal index ops. Clovis also
 *     provide with some helper APIs to transform layer index's key/value
 *     to internal representations.
 *
 * (7) Current implementation has a few synchronous calls(points):
 *       - Layout is not retrieved from global layout index util an IO op
 *         is initialised for composite or capture object. Retrieving
 *         layout inside m0_clovis_obj_op() is a synchronous request.
 *       - Paticularly for composite layout, Clovis retrieves extents as well,
 *         which is also a set of synchronous NEXT queries.
 *       - When constructing IO op for a composite layer (sub object), pool
 *         version and layout id of sub object need to be known to build
 *         layout instance. These object attributes again are retrieved
 *         synchronously from services.
 *
 *    A solution to remove these synchronous calls is to implement
 *    a execution plan which forms a DAG representing the execution orders
 *    of steps. This requires re-write/organise many places in clovis and
 *    will leave for the next version.
 */

struct m0_clovis_layout;
struct m0_clovis_op_layout;

/**
 * Clovis makes use of layout id attribute and stores the layout type in
 * the highest 16 bits of layout id.
 */
#define M0_CLOVIS_OBJ_LAYOUT_ID(lid)  (lid & 0x0000ffffffffffff)
#define M0_CLOVIS_OBJ_LAYOUT_TYPE(lid) ((lid & 0xffff000000000000) >> 48UL)
#define M0_CLOVIS_OBJ_LAYOUT_MAKE_LID(lid, type) \
	((uint64_t)lid | ((uint64_t)type << 48UL))

extern const struct m0_clovis_op_layout_ops clovis_op_layout_composite_ops;
extern const struct m0_clovis_layout_ops clovis_layout_composite_ops;
extern struct m0_fid composite_extent_rd_idx_fid;
extern struct m0_fid composite_extent_wr_idx_fid;

struct m0_clovis_capture_layout {
	struct m0_clovis_layout cl_layout;
	struct m0_uint128       cl_orig_id;
	struct m0_fid           cl_pver;
	uint64_t                cl_lid;
};

struct m0_clovis_pdclust_layout {
	struct m0_clovis_layout pl_layout;

	struct m0_fid           pl_pver;
	uint64_t                pl_lid;
	struct m0_fid           pl_fid;
};

struct m0_clovis_composite_extent {
	struct m0_uint128 ce_id;
	m0_bindex_t       ce_off;
	m0_bcount_t       ce_len;

	struct m0_tlink   ce_tlink;
	uint64_t          ce_tlink_magic;
};

struct m0_clovis_composite_layer {
	struct m0_uint128 ccr_subobj;
	uint64_t          ccr_lid;

	int               ccr_priority;
	struct m0_tl      ccr_rd_exts;
	struct m0_tl      ccr_wr_exts;

	struct m0_mutex   ccr_lock;
	struct m0_tlink   ccr_tlink;
	uint64_t          ccr_tlink_magic;
};

/**
 * In-memory representation of an composite layout.
 */
struct m0_clovis_composite_layout {
	struct m0_clovis_layout ccl_layout;
	uint64_t                ccl_nr_layers;
	struct m0_tl            ccl_layers;
	struct m0_mutex         ccl_lock;
};

/**
 * Data structures for generic layout operation.
 */
struct m0_clovis_op_layout_ops {
	int (*olo_launch) (struct m0_clovis_op_layout *ol);
	/** Output layout details to user. */
	int (*olo_copy_to_app)(struct m0_clovis_layout *to, void *data);
	/** Input layout details from user. */
	int (*olo_copy_from_app)(struct m0_clovis_layout *from, void *data);
};

struct m0_clovis_op_layout {
	struct m0_clovis_op_common            ol_oc;
	uint64_t                              ol_magic;

	struct m0_sm_group                   *ol_sm_grp;
	struct m0_clovis_ast_rc               ol_ar;

	struct m0_clovis_layout              *ol_layout;
	/** The entity to be queried for its layout. */
	struct m0_clovis_entity              *ol_entity;

	const struct m0_clovis_op_layout_ops *ol_ops;
};

extern const struct m0_bob_type ol_bobtype;

struct m0_clovis_op_composite_io {
	struct m0_clovis_op_obj   oci_oo;
	uint64_t                  oci_magic;

	int                       oci_nr_sub_ops;
	struct m0_clovis_op     **oci_sub_ops;

	int                       oci_nr_replied;
};

M0_INTERNAL int m0_clovis__layout_op_launch(struct m0_clovis_op_layout *ol);

M0_INTERNAL int m0_clovis__layout_get(struct m0_clovis_layout *layout);
M0_INTERNAL void m0_clovis__layout_put(struct m0_clovis_layout *layout);

M0_INTERNAL int m0_clovis__composite_container_init(struct m0_clovis *cinst);

M0_INTERNAL int m0_clovis__obj_layout_send(struct m0_clovis_obj *obj,
					   struct m0_clovis_op_layout *ol);

M0_INTERNAL int m0_clovis__dix_layout_get_sync(struct m0_clovis_obj *obj,
					       struct m0_dix_layout *dlayout);
M0_INTERNAL struct m0_dix_cli *ent_dixc(const struct m0_clovis_entity *ent);
M0_INTERNAL struct m0_dix_cli *ol_dixc(const struct m0_clovis_op_layout *ol);

#endif /* __MERO_CLOVIS_CLOVIS_LAYOUT_H__ */

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
