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
 * Original author: Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original creation date: 08/17/2011
 */

#pragma once

#ifndef __MERO_CONSOLE_IT_H__
#define __MERO_CONSOLE_IT_H__

#include "fop/fop.h"     /* m0_fop_field_type */
#include "xcode/xcode.h" /* m0_xcode_type */

/**
   @addtogroup console_it
   @{
 */

enum m0_cons_data_process_type {
	CONS_IT_INPUT,
	CONS_IT_OUTPUT,
	CONS_IT_SHOW
};

/**
 * @struct m0_cons_atom_ops
 *
 * @brief operation to get value of ATOM type (i.e. CHAR, U64 etc).
 */
struct m0_cons_atom_ops {
	void (*catom_val_get)(const struct m0_xcode_type *xct,
			      const char *name, void *data);
	void (*catom_val_set)(const struct m0_xcode_type *xct,
			      const char *name, void *data);
	void (*catom_val_show)(const struct m0_xcode_type *xct,
			       const char *name, void *data);
};

/**
 * @brief Iterate over FOP fields and prints the names.
 *
 * @param fop fop object.
 */
M0_INTERNAL int m0_cons_fop_fields_show(struct m0_fop *fop);

/**
 * @brief Helper function for FOP input
 *
 * @param fop fop object.
 */
M0_INTERNAL int m0_cons_fop_obj_input(struct m0_fop *fop);

/**
 * @brief Helper function for FOP output.
 *
 * @param fop fop object.
 */
M0_INTERNAL int m0_cons_fop_obj_output(struct m0_fop *fop);

/** @} end of console_it */

/* __MERO_CONSOLE_IT_H__ */
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
