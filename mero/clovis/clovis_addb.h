/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original authors: Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Revision:         Pratik Shinde   <pratik.shinde@seagate.com>
 * Original creation date: 15-Sep-2014
 */

#pragma once

#ifndef __MERO_CLOVIS_CLOVIS_ADDB_H__
#define __MERO_CLOVIS_CLOVIS_ADDB_H__

#include "addb2/addb2.h"
#include "addb2/identifier.h"
#include "clovis/clovis_internal_xc.h"
#include "xcode/xcode_attr.h"

/**
   @addtogroup clovis
   @{
 */

enum m0_avi_clovis_labels {
	M0_AVI_CLOVIS_SM_OP = M0_AVI_CLOVIS_RANGE_START + 1,
	M0_AVI_CLOVIS_SM_OP_COUNTER,
	M0_AVI_CLOVIS_SM_OP_COUNTER_END = M0_AVI_CLOVIS_SM_OP_COUNTER + 0x100,

	M0_AVI_CLOVIS_TO_DIX,

	M0_AVI_CLOVIS_COB_REQ,
	M0_AVI_CLOVIS_TO_COB_REQ,
	M0_AVI_CLOVIS_COB_REQ_TO_RPC,
	M0_AVI_CLOVIS_TO_IOO,
	M0_AVI_CLOVIS_IOO_TO_RPC,
	M0_AVI_CLOVIS_BULK_TO_RPC,

	M0_AVI_CLOVIS_OP_ATTR_ENTITY_ID,
	M0_AVI_CLOVIS_OP_ATTR_CODE,

	M0_AVI_CLOVIS_IOO_ATTR_BUFS_NR,
	M0_AVI_CLOVIS_IOO_ATTR_BUF_SIZE,
	M0_AVI_CLOVIS_IOO_ATTR_PAGE_SIZE,
	M0_AVI_CLOVIS_IOO_ATTR_BUFS_ALIGNED,
	M0_AVI_CLOVIS_IOO_ATTR_RMW,

	M0_AVI_CLOVIS_IOO_REQ,
	M0_AVI_CLOVIS_IOO_REQ_COUNTER,
	M0_AVI_CLOVIS_IOO_REQ_COUNTER_END = M0_AVI_CLOVIS_IOO_REQ_COUNTER + 0x100,
} M0_XCA_ENUM;

/** @} */ /* end of clovis group */

#endif /* __MERO_CLOVIS_CLOVIS_ADDB_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
