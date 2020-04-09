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
 * Original author       : Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original creation date: 08/03/2011
 * Revision              : Manish Honap <Manish_Honap@xyratex.com>
 * Revision date         : 07/31/2012
 */

#pragma once

#ifndef __MERO_CONSOLE_FOP_H__
#define __MERO_CONSOLE_FOP_H__

#include "lib/types.h"
#include "fop/fop.h"
#include "xcode/xcode_attr.h"

/**
 * Failure fops should be defined by not yet existing "failure" module. For the
 * time being, it makes sense to put them in cm/ or console/. ioservice is not
 * directly responsible for handling failures, it is intersected by copy-machine
 * (cm).
 * <b>Console fop formats</b>
 */

struct m0_cons_fop_fid {
	uint64_t cons_seq;
	uint64_t cons_oid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_cons_fop_buf {
	uint32_t  cons_size;
	uint8_t  *cons_buf;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

struct m0_cons_fop_test {
	uint8_t                cons_test_type;
	uint64_t               cons_test_id;
	struct m0_cons_fop_fid cons_id;
	struct m0_cons_fop_buf cons_test_buf;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 *  Device failure notification fop
 *  - id          : Console id.
 *  - notify_type : Device failure notification.
 *  - dev_id      : Device ID (Disk ID in case disk failure).
 *  - dev_name    : Device name (In case of disk it could be volume name).
 */
struct m0_cons_fop_device {
	struct m0_cons_fop_fid cons_id;
	uint32_t               cons_notify_type;
	uint64_t               cons_dev_id;
	struct m0_cons_fop_buf cons_dev_name;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 *  Reply fop to the notification fop
 *  - notify_type : Notification type of request fop.
 *  - return      : Opcode of request fop.
 */
struct m0_cons_fop_reply {
	int32_t  cons_return;
	uint32_t cons_notify_type;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * Init console FOP
 */
M0_INTERNAL int m0_console_fop_init(void);

/**
 * Fini console FOP
 */
M0_INTERNAL void m0_console_fop_fini(void);

extern struct m0_fop_type m0_cons_fop_device_fopt;
extern struct m0_fop_type m0_cons_fop_reply_fopt;
extern struct m0_fop_type m0_cons_fop_test_fopt;

/* __MERO_CONSOLE_FOP_H__ */
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
