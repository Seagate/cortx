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
 * Original author: Sergey Shilov <sergey.shilov@seagate.com>
 * Original creation date: 26-July-2016
 */

#pragma once

#ifndef __MERO_DIX_FID_CONVERT_H__
#define __MERO_DIX_FID_CONVERT_H__

#include "lib/types.h"          /* uint32_t */

/**
 * @addtogroup dix
 *
 *  Fids of component catalogues are build in the same manner as it is done for
 *  cobs: component catalogue fid is a combination of distributed index fid with
 *  'component catalogue' type and target storage id.
 *
 * @verbatim
 *                     8 bits           24 bits             96 bits
 *                 +-----------------+-----------+---------------------------+
 *    DIX fid      |   DIX type id   |  zeroed   |                           |
 *                 +-----------------+-----------+---------------------------+
 *                                                            ||
 *                                                            \/
 *                 +-----------------+-----------+---------------------------+
 *    CCTG fid     |   CCTG type id  | device id |                           |
 *                 +-----------------+-----------+---------------------------+
 * @endverbatim
 *
 * @{
 */

struct m0_fid;

enum {
	M0_DIX_FID_DEVICE_ID_BITS     = 24,
	M0_DIX_FID_DEVICE_ID_OFFSET   = 32,
	M0_DIX_FID_DEVICE_ID_MASK     = ((1ULL << M0_DIX_FID_DEVICE_ID_BITS) - 1)
					<< M0_DIX_FID_DEVICE_ID_OFFSET,
	M0_DIX_FID_DEVICE_ID_MAX      = (1ULL << M0_DIX_FID_DEVICE_ID_BITS) - 1,
	M0_DIX_FID_DIX_CONTAINER_MASK = (1ULL << M0_DIX_FID_DEVICE_ID_OFFSET)
					- 1,
};

M0_INTERNAL void m0_dix_fid_dix_make(struct m0_fid *dix_fid,
				     uint32_t       container,
				     uint64_t       key);

M0_INTERNAL void m0_dix_fid_convert_dix2cctg(const struct m0_fid *dix_fid,
					     struct m0_fid       *cctg_fid,
					     uint32_t             device_id);
M0_INTERNAL void m0_dix_fid_convert_cctg2dix(const struct m0_fid *cctg_fid,
					     struct m0_fid       *dix_fid);

M0_INTERNAL uint32_t m0_dix_fid_cctg_device_id(const struct m0_fid *cctg_fid);

M0_INTERNAL bool m0_dix_fid_validate_dix(const struct m0_fid *dix_fid);
M0_INTERNAL bool m0_dix_fid_validate_cctg(const struct m0_fid *cctg_fid);

M0_INTERNAL uint32_t m0_dix_fid__device_id_extract(const struct m0_fid *fid);

/** @} end of dix group */
#endif /* __MERO_DIX_FID_CONVERT_H__ */

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
