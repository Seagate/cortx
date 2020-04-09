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

#ifndef __MERO_LAYOUT_LINEAR_ENUM_H__
#define __MERO_LAYOUT_LINEAR_ENUM_H__

/**
 * @defgroup linear_enum Linear Enumeration Type.
 *
 * A layout with linear enumeration type stores a linear formula that is
 * used to enumerate all the component object identifiers.
 *
 * @{
 */

/* import */
#include "lib/arith.h" /* M0_IS_8ALIGNED */
#include "layout/layout.h"

/* export */
struct m0_layout_linear_enum;

/**
 * Attributes specific to Linear enumeration type.
 * These attributes are part of m0_layout_linear_enum which is in-memory layout
 * enumeration object and are stored in the Layout DB as well.
 *
 * @note This structure needs to be maintained as 8 bytes aligned.
 */
struct m0_layout_linear_attr {
	/** Number of elements present in the enumeration. */
	uint32_t   lla_nr;

	/** Constant A used in the linear equation A + idx * B. */
	uint32_t   lla_A;

	/** Constant B used in the linear equation A + idx * B. */
	uint32_t   lla_B;

	/** Padding to make the structure 8 bytes aligned. */
	uint32_t   lla_pad;
};
M0_BASSERT(M0_IS_8ALIGNED(sizeof(struct m0_layout_linear_attr)));

/** Extension of the generic m0_layout_enum for the linear enumeration type. */
struct m0_layout_linear_enum {
	/** Super class. */
	struct m0_layout_enum        lle_base;

	struct m0_layout_linear_attr lle_attr;

	uint64_t                     lla_magic;
};

/**
 * Allocates and builds linear enumeration object.
 * @post ergo(rc == 0, linear_invariant_internal(lin_enum))
 *
 * @note Enum object is not to be finalised explicitly by the user. It is
 * finalised internally through m0_layout__striped_fini().
 */
M0_INTERNAL int m0_linear_enum_build(struct m0_layout_domain *dom,
				     const struct m0_layout_linear_attr *attr,
				     struct m0_layout_linear_enum **out);

extern struct m0_layout_enum_type m0_linear_enum_type;

/** @} end group linear_enum */

/* __MERO_LAYOUT_LINEAR_ENUM_H__ */
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
