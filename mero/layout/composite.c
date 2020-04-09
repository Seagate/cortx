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
 * Original creation date: 11/16/2011
 */

/**
 * @addtogroup composite
 * @{
 */

#include "lib/memory.h" /* M0_ALLOC_PTR() */

#include "layout/layout_internal.h"
#include "layout/composite.h"

struct composite_schema_data {
	/** Table to store extent maps for all the composite layouts. */
	struct m0_be_emap csd_comp_layout_ext_map;
};

/** Prefix for comp_layout_ext_map table. */
struct layout_prefix {
	/**
	 * Layout id for the composite layout.
	 * Value is same as m0_layout::l_id.
	 */
	uint64_t lp_l_id;

	/**
	 * Filler since prefix is a 128 bit field.
	 * Currently un-used.
	 */
	uint64_t lp_filler;
};

/*
 * @post A composite type of layout object is created. User is expected to
 * add a reference on the layout object as required and is expected to release
 * the reference when done with the usage. The layout is finalised when it is
 * the last reference being released.
 */
M0_INTERNAL void m0_composite_build(struct m0_layout_domain *dom,
				    uint64_t lid,
				    struct m0_tl *sub_layouts,
				    struct m0_composite_layout **out)
{
}

/** Implementation of lo_fini for COMPOSITE layout type. */
static void composite_fini(struct m0_ref *ref)
{
}

/* Implementation of lto_allocate for COMPOSITE layout type. */
static int composite_allocate(struct m0_layout_domain *dom,
			      uint64_t lid,
			      struct m0_layout **out)
{
	return 0;
}

/** Implementation of lo_delete for COMPOSITE layout type. */
static void composite_delete(struct m0_layout *l)
{
}

/** Implementation of lo_recsize() for COMPOSITE layout type. */
static m0_bcount_t composite_recsize(const struct m0_layout *l)
{
	return 0;
}

/**
 * Implementation of lto_register for COMPOSITE layout type.
 *
 * Initialises table specifically required for COMPOSITE layout type.
 */
static int composite_register(struct m0_layout_domain *dom,
			      const struct m0_layout_type *lt)
{
	/*
	@code
	struct composite_schema_data *csd;

	M0_ALLOC_PTR(csd);

	Initialise csd->csd_comp_layout_ext_map table.

	dom->ld_type_data[lt->lt_id] = csd;
	@endcode
	*/
	return 0;
}

/**
 * Implementation of lto_unregister for COMPOSITE layout type.
 *
 * Finalises table specifically required for COMPOSITE layout type.
 */
static void composite_unregister(struct m0_layout_domain *dom,
				 const struct m0_layout_type *lt)
{
	/*
	@code
	Finalise
	dom->ld_type_data[lt->lt_id]->csd_comp_layout_ext_map
	table.

	dom->ld_type_data[lt->lt_id] = NULL;
	@endcode
	*/
}

/** Implementation of lto_max_recsize() for COMPOSITE layout type. */
static m0_bcount_t composite_max_recsize(struct m0_layout_domain *dom)
{
	return 0;
}

static const struct m0_layout_ops composite_ops;

/**
 * Implementation of lo_decode() for composite layout type.
 *
 * Continues to build the in-memory layout object from its representation
 * either 'stored in the Layout DB' or 'received through the buffer'.
 *
 * @param op This enum parameter indicates what if a DB operation is to be
 * performed on the layout record and it could be LOOKUP if at all.
 * If it is BUFFER_OP, then the layout is decoded from its representation
 * received through the buffer.
 */
static int composite_decode(struct m0_layout *l,
			    struct m0_bufvec_cursor *cur,
			    enum m0_layout_xcode_op op,
			    struct m0_be_tx *tx,
			    uint32_t user_count)
{
	/*
	@code
	struct m0_composite_layout *cl;

	M0_PRE(M0_IN(op, (M0_LXO_DB_LOOKUP, M0_LXO_BUFFER_OP));

	M0_ALLOC_PTR(cl);

	m0_layout__init(dom, &cl->cl_base, lid,
			&m0_composite_layout_type, &composite_ops);

	if (op == M0_LXO_DB_LOOKUP) {
		Read all the segments from the comp_layout_ext_map table,
		belonging to composite layout with layout id 'lid' and store
		them in the cl->cl_sub_layouts.

	} else {
		Parse the sub-layout information from the buffer pointed by
		cur and store it in cl->cl_sub_layouts.
	}

	*out = &cl->cl_base;
	@endcode
	*/

	return 0;
}

/**
 * Implementation of lo_encode() for composite layout type.
 *
 * Continues to use the in-memory layout object and either 'stores it in the
 * Layout DB' or 'converts it to a buffer'.
 *
 * @param op This enum parameter indicates what is the DB operation to be
 * performed on the layout record if at all and it could be one of
 * ADD/UPDATE/DELETE. If it is BUFFER_OP, then the layout is stored in the
 * buffer.
 */
static int composite_encode(struct m0_layout *l,
			    enum m0_layout_xcode_op op,
			    struct m0_be_tx *tx,
			    struct m0_bufvec_cursor *out)
{
	/*
	@code

	M0_PRE(M0_IN(op, (M0_LXO_DB_ADD, M0_LXO_DB_UPDATE,
			  M0_LXO_DB_DELETE, M0_LXO_BUFFER_OP)));

	if ((op == M0_LXO_DB_ADD) || (op == M0_LXO_DB_UPDATE) ||
            (op == M0_LXO_DB_DELETE)) {
		Form records for the cob_lists table by using data from the
		m0_layout object l and depending on the value of op,
		insert/update/delete those records to/from the cob_lists table.
	} else {
		Store composite layout type specific fields like information
		about the sub-layouts, into the buffer by referring it from
		m0_layout object l.
	}

	@endcode
	*/

	return 0;
}

static const struct m0_layout_ops composite_ops = {
	.lo_fini    = composite_fini,
	.lo_delete  = composite_delete,
	.lo_recsize = composite_recsize,
	.lo_decode  = composite_decode,
	.lo_encode  = composite_encode
};

static const struct m0_layout_type_ops composite_type_ops = {
	.lto_register    = composite_register,
	.lto_unregister  = composite_unregister,
	.lto_max_recsize = composite_max_recsize,
	.lto_allocate    = composite_allocate
};


const struct m0_layout_type m0_composite_layout_type = {
	.lt_name = "composite",
	.lt_id   = 1,
	.lt_ops  = &composite_type_ops
};

/** @} end group composite */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
