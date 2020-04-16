/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Yuriy Umanets <Yuriy_Umanets@xyratex.com>
 * Original creation date: 01/23/2010
 */

#include "lib/bitmap_xc.h"
#include "lib/buf_xc.h"
#include "lib/types_xc.h"
#include "lib/vec_xc.h"
#include "lib/types_xc.h"
#include "lib/ext_xc.h"
#include "lib/string_xc.h"
#include "lib/misc.h"       /* M0_EXPORTED */

M0_INTERNAL int libm0_init(void)
{
	extern int m0_node_uuid_init(void);
	return m0_node_uuid_init();
}
M0_EXPORTED(libm0_init);

M0_INTERNAL void libm0_fini(void)
{
}
M0_EXPORTED(libm0_fini);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
