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
 * Original author: Amit Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 10/13/2011
 */

#include <linux/module.h>

/*
  This file contains dummy init() and fini() routines for modules, that are
  not yet ported to kernel.

  Once the module compiles successfully for kernel mode, dummy routines from
  this file should be removed.
 */

M0_INTERNAL int m0_linux_stobs_init(void)
{
	return 0;
}

M0_INTERNAL void m0_linux_stobs_fini(void)
{
}

M0_INTERNAL int sim_global_init(void)
{
	return 0;
}

M0_INTERNAL void sim_global_fini(void)
{
}

M0_INTERNAL int m0_timers_init(void)
{
	return 0;
}

M0_INTERNAL void m0_timers_fini(void)
{
}
