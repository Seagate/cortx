/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED, A SEAGATE COMPANY
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
 * Original authors: James Morse   <james_morse@xyratex.com>,
 *                   Juan Gonzalez <juan_gonzalez@xyratex.com>,
 *                   Sining Wu     <sining_wu@xyratex.com>
 * Original creation date: 07-May-2014
 */

#pragma once

#ifndef __MERO_M0T1FS_IOCTL_H__
#define __MERO_M0T1FS_IOCTL_H__

#include <linux/version.h>    /* LINUX_VERSION_CODE */
#include <linux/ioctl.h>      /* include before m0t1fs/m0t1fs_ioctl.h */

#include "m0t1fs/m0t1fs_ioctl.h"

#ifdef __KERNEL__
/**
 * Handles the ioctl targeted to m0t1fs.
 * @param filp File associated to the file descriptor used when creating
 * the ioctl.
 * @param cmd Code of the operation requested by the ioctl.
 * @param arg Argument passed from user space. It can be a pointer.
 * @return -ENOTTY if m0t1fs does not support the requested operation.
 * Otherwise, the value returned by the operation.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
M0_INTERNAL long m0t1fs_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg);
#else
M0_INTERNAL int m0t1fs_ioctl(struct inode *inode, struct file *filp,
			     unsigned int cmd, unsigned long arg);
#endif
#endif /* __KERNEL__ */

#endif /* __MERO_M0T1FS_IOCTL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
