/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 18-Mar-2015
 */


/**
 * @addtogroup xcode
 *
 * @{
 */

#define _XT(x)
#define _TI(x)
#define _EN(x)
#define _FI(x) M0_INTERNAL void x(void);
#define _FF(x) M0_INTERNAL void x(void);
#include "xcode/xlist.h"
#undef __MERO_XCODE_XLIST_H__
#undef _XT
#undef _TI
#undef _EN
#undef _FI
#undef _FF

int m0_xcode_init(void)
{
#define _XT(x)
#define _TI(x)
#define _EN(x)
#define _FI(x) x();
#define _FF(x)
#include "xcode/xlist.h"
#undef __MERO_XCODE_XLIST_H__
#undef _XT
#undef _TI
#undef _EN
#undef _FI
#undef _FF
	return 0;
}

void m0_xcode_fini(void)
{
#define _XT(x)
#define _TI(x)
#define _EN(x)
#define _FI(x)
#define _FF(x) x();
#include "xcode/xlist.h"
#undef _XT
#undef _TI
#undef _EN
#undef _FI
#undef _FF
}

/** @} end of xcode group */

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
