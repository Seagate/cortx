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
 * Original author: Huang Hua <hua_huang@xyratex.com>
 *		    Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 12/02/2010
 */

#pragma once

#ifndef __MERO_LIB_LINUX_KERNEL_RWLOCK_H__
#define __MERO_LIB_LINUX_KERNEL_RWLOCK_H__

#include <linux/rwsem.h>

/**
   @addtogroup rwlock

   <b>Linux kernel rwlock.</a>

   Linux kernel implementation is based on rw_semaphore (linux/rwsem.h).

   @{
 */

struct m0_rwlock {
	struct rw_semaphore rw_sem;
};

/** @} end of mutex group */

#endif /* __MERO_LIB_LINUX_KERNEL_RWLOCK_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
