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

#ifndef __MERO_M0T1FS_M0T1FS_IOCTL_H__
#define __MERO_M0T1FS_M0T1FS_IOCTL_H__

#include <linux/ioctl.h>

#define M0_M0T1FS_IOC_MAGIC (0xAA)

/* ioctl(fs, M0_M0T1FS_FWAIT) */
#define M0_M0T1FS_FWAIT _IO(M0_M0T1FS_IOC_MAGIC, 'w')

#endif /* __MERO_M0T1FS_M0T1FS_IOCTL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
