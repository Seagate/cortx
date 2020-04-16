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
 * Original author: Anand Vidwansa <anand_vidwansa@xyratex.com>
 * Original creation date: 10/11/2011
 */

#pragma once

#ifndef __MERO_LIB_LINUX_KERNEL_VEC_H__
#define __MERO_LIB_LINUX_KERNEL_VEC_H__

#include "lib/types.h"

/**
   @addtogroup vec
   @{
*/

struct page;
struct m0_0vec;

/**
   Add a struct page to contents of m0_0vec structure.
   Struct page is kernel representation of a buffer.
   @note The m0_0vec struct should be allocated by user.

   @param zvec The m0_0vec struct to be initialized.
   @param pages Array of kernel pages.
   @param index The target object offset for page.
   @post ++zvec->z_cursor.bc_vc.vc_seg
 */
M0_INTERNAL int m0_0vec_page_add(struct m0_0vec *zvec, struct page *pg,
				 m0_bindex_t index);

/** @} end of vec group */

/* __MERO_LIB_LINUX_KERNEL_VEC_H__ */
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
