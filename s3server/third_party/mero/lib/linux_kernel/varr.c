/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 12/17/2012
 */

#include "lib/varr.h"		/* m0_varr */
#include "lib/varr_private.h"
#include "lib/bob.h"		/* m0_bob_type */
#include "lib/types.h"		/* Includes appropriate types header. */
#include "lib/memory.h"		/* m0_alloc(), m0_free() */
#include <linux/pagemap.h>

M0_EXTERN void *m0_varr_buf_alloc(size_t bufsize)
{
	if (bufsize == PAGE_SIZE)
		return (void *)get_zeroed_page(GFP_KERNEL);
	else
		return m0_alloc(bufsize);
}

M0_EXTERN void m0_varr_buf_free(void *buf, size_t bufsize)
{
	M0_PRE(buf != NULL);
	if (bufsize == PAGE_SIZE)
		free_page((unsigned long)buf);
	else
		m0_free(buf);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
