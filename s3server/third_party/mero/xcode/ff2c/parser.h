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
 * Original creation date: 30-Dec-2011
 */

#pragma once

#ifndef __MERO_XCODE_FF2C_PARSER_H__
#define __MERO_XCODE_FF2C_PARSER_H__

/**
   @addtogroup xcode

   <b>ff2c. Parser.</b>

   Recursive-descent parser.
 */
/** @{ */

#include "xcode/ff2c/lex.h"

/* export */
struct ff2c_term;

enum ff2c_term_type {
	FNT_FF = 1,
	FNT_REQUIRE,
	FNT_DECLARATION,
	FNT_ATOMIC,
	FNT_COMPOUND,
	FNT_TYPENAME,
	FNT_TAG,
	FNT_ESCAPE,
	FNT_NR
};
extern const char *ff2c_term_type_name[];

struct ff2c_term {
	enum ff2c_term_type  fn_type;
	struct ff2c_term    *fn_parent;
	struct ff2c_term    *fn_head;
	struct ff2c_term    *fn_tail;
	struct ff2c_term    *fn_next;
	struct ff2c_token    fn_tok;
};

int ff2c_parse(struct ff2c_context *ctx, struct ff2c_term **out);
void ff2c_term_fini(struct ff2c_term *term);


/** @} end of xcode group */

/* __MERO_XCODE_FF2C_PARSER_H__ */
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
