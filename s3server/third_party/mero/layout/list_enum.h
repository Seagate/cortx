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

#pragma once

#ifndef __MERO_LAYOUT_LIST_ENUM_H__
#define __MERO_LAYOUT_LIST_ENUM_H__

/**
 * @defgroup list_enum List Enumeration Type.
 *
 * List Enumeration Type. A layout with list enumeration type lists all the
 * COB identifiers as a part of the layout itself.
 * @{
 */

/* import */
#include "lib/arith.h"     /* M0_IS_8ALIGNED */
#include "layout/layout.h"

struct m0_fid;

/* export */
struct m0_layout_list_enum;

/** Extension of the generic m0_layout_enum for the list enumeration type. */
struct m0_layout_list_enum {
	/** Super class. */
	struct m0_layout_enum   lle_base;

	/** Number of elements present in the enumeration. */
	uint32_t                lle_nr;

	/**
	 * Pointer to an array of COB identifiers for the component objects
	 * which are part of 'the layout this enum is assocaited with'.
	 * @todo In kernel any allocation over 4KB is not safe. Thus, this
	 * array can safely hold only upto 256 number of COB identifiers,
	 * (m0_fid being 16 bytes in size).
	 * This issue is to be addressed later.
	 */
	struct m0_fid          *lle_list_of_cobs;

	uint64_t                lle_magic;
};

/**
 * Allocates and builds list enumeration object.
 *
 * @param cob_list List of cob identifiers, allocated and supplied by the
 * user. User shall not free the memory allocated for this list but it will
 * be internally freed as a part of the enum object finalisation that
 * happens through layout finalisation.
 *
 * @post ergo(rc == 0, list_invariant_internal(lin_enum))
 *
 * @note Enum object is not to be finalised explicitly by the user. It is
 * finalised internally through m0_layout__striped_fini().
 */
M0_INTERNAL int m0_list_enum_build(struct m0_layout_domain *dom,
				   struct m0_fid *cob_list, uint32_t nr,
				   struct m0_layout_list_enum **out);

extern struct m0_layout_enum_type m0_list_enum_type;

/** @} end group list_enum */

/**
 * Following structure is part of the internal implementation. It is required to
 * be accessed by the UT as well. Hence, is placed here in the header file.
 *
 * Structure used to store cob entries inline into the layouts table - maximum
 * upto LDB_MAX_INLINE_COB_ENTRIES number of those.
 *
 * @note This structure needs to be maintained as 8 bytes aligned.
 */
struct cob_entries_header {
	/** Total number of COB Ids for the specific layout. */
	uint32_t  ces_nr;

	/** Padding to make the structure 8 bytes aligned. */
	uint32_t  ces_pad;

	/**
	 * Payload storing list of cob ids (struct m0_fid), max upto
	 * LDB_MAX_INLINE_COB_ENTRIES number of those.
	 */
	char      ces_cobs[0];
};
M0_BASSERT(M0_IS_8ALIGNED(sizeof(struct cob_entries_header)));

/* __MERO_LAYOUT_LIST_ENUM_H__ */
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
