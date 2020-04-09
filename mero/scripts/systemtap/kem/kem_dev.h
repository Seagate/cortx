/* -*- C -*- */
/*
 * COPYRIGHT 2019 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Malezhin <maxim.malezhin@seagate.com>
 * Original creation date: 5-Aug-2019
 */

#pragma once

#ifndef __MERO_SCRIPTS_SYSTEMTAP_KEM_KEM_DEV_H__
#define __MERO_SCRIPTS_SYSTEMTAP_KEM_KEM_DEV_H__

#include <linux/types.h>
#include <linux/cdev.h>
#include "kem.h"

#define KEMD_BUFFER_SIZE (4*1024)
#define KEMD_READ_PORTION 20

/*
 * Major number 60 has been chosen as it's number for local/experimental
 * devices. Refer to Linux Kernel documentation for details:
 *
 * https://www.kernel.org/doc/Documentation/admin-guide/devices.txt
 */
#define KEMD_MAJOR 60
#define KEMD_MINOR 0

/**
 * @defgroup kem_dev KEM device and ring buffer
 *
 * @{
 */

struct kem_rb {
	struct ke_msg *kr_buf;
	unsigned int   kr_size;
	unsigned int   kr_read_idx;
	unsigned int   kr_write_idx;
	atomic_t       kr_written;
	unsigned int   kr_logged;
	unsigned int   kr_occurred;
};

struct kem_dev {
	struct cdev    kd_cdev;
	atomic_t       kd_busy;
	int            kd_num;
	struct kem_rb *kd_rb;
};

/** @} end of kem_dev group */

#endif /* __MERO_SCRIPTS_SYSTEMTAP_KEM_KEM_DEV_H__ */

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
