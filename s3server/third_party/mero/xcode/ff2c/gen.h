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

#ifndef __MERO_XCODE_FF2C_GEN_H__
#define __MERO_XCODE_FF2C_GEN_H__

/**
   @addtogroup xcode
 */
/** @{ */

#include <stdio.h>                              /* FILE */

/* import */
struct ff2c_ff;

struct ff2c_gen_opt {
	const char *go_basename;
	const char *go_guardname;
	FILE       *go_out;
};

int ff2c_h_gen(const struct ff2c_ff *ff, const struct ff2c_gen_opt *opt);
int ff2c_c_gen(const struct ff2c_ff *ff, const struct ff2c_gen_opt *opt);

/** @} end of xcode group */

/* __MERO_XCODE_FF2C_GEN_H__ */
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
