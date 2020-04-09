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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 11-Mar-2015
 */

#pragma once

#ifndef __MERO_IOSERVICE_FID_CONVERT_H__
#define __MERO_IOSERVICE_FID_CONVERT_H__

#include "lib/types.h"          /* uint32_t */

/**
 * @defgroup fidconvert
 *
 * @verbatim
 *                     8 bits           24 bits             96 bits
 *                 +-----------------+-----------+---------------------------+
 *    GOB fid      |   GOB type id   |  zeroed   |                           |
 *                 +-----------------+-----------+---------------------------+
 *                                                            ||
 *                                                            \/
 *                 +-----------------+-----------+---------------------------+
 *    COB fid      |   COB type id   | device id |                           |
 *                 +-----------------+-----------+---------------------------+
 *                                        ||                  ||
 *                                        \/                  \/
 *    AD stob      +-----------------+-----------+---------------------------+
 *      fid        | AD stob type id | device id |                           |
 *                 +-----------------+-----------+---------------------------+
 *
 *                       8 bits                96 bits             24 bits
 *                 +-----------------+---------------------------+-----------+
 *     AD stob     | AD stob domain  |          zeroed           | device id |
 *   domain fid    |    type id      |                           |           |
 *                 +-----------------+---------------------------------------+
 *                                                      ||
 *                                                      \/
 *  AD stob domain +-----------------+---------------------------------------+
 *  backing store  |   linux stob    |                                       |
 *    stob fid     |     type id     |                                       |
 *                 +-----------------+---------------------------------------+
 *                       8 bits                     120 bits
 *                 +-----------------+---------------------------------------+
 *     Linux stob  | Linux stob dom  |                                       |
 *   domain fid    |    type id      |        FIXED DOM KEY                  |
 *                 +-----------------+---------------------------------------+
 * @endverbatim
 *
 * Note: ad stob backing store conversion is here because ad stob itself doesn't
 * have limitation on backing store stob fid, but ioservice has such limitation.
 *
 * @{
 */

struct m0_fid;
struct m0_stob_id;

enum {
	M0_FID_DEVICE_ID_BITS     = 24,
	M0_FID_DEVICE_ID_OFFSET   = 32,
	M0_FID_DEVICE_ID_MASK     = ((1ULL << M0_FID_DEVICE_ID_BITS) - 1) <<
				    M0_FID_DEVICE_ID_OFFSET,
	M0_FID_DEVICE_ID_MAX      = (1ULL << M0_FID_DEVICE_ID_BITS) - 1,
	M0_FID_GOB_CONTAINER_MASK = (1ULL << M0_FID_DEVICE_ID_OFFSET) - 1,
	M0_AD_STOB_LINUX_DOM_KEY  = 0xadf11e, /* AD file */
	/** Default cid for fake linuxstob storage. */
	M0_SDEV_CID_DEFAULT       = 1,
};

M0_INTERNAL void m0_fid_gob_make(struct m0_fid *gob_fid,
				 uint32_t       container,
				 uint64_t       key);

M0_INTERNAL void m0_fid_convert_gob2cob(const struct m0_fid *gob_fid,
					struct m0_fid       *cob_fid,
					uint32_t             device_id);
M0_INTERNAL void m0_fid_convert_cob2gob(const struct m0_fid *cob_fid,
					struct m0_fid       *gob_fid);
M0_INTERNAL void m0_fid_convert_cob2stob(const struct m0_fid *cob_fid,
					 struct m0_stob_id   *stob_id);

M0_INTERNAL void m0_fid_convert_cob2adstob(const struct m0_fid *cob_fid,
					   struct m0_stob_id   *stob_id);
M0_INTERNAL void m0_fid_convert_adstob2cob(const struct m0_stob_id *stob_id,
					   struct m0_fid           *cob_fid);
M0_INTERNAL void m0_fid_convert_stob2cob(const struct m0_stob_id   *stob_id,
					 struct m0_fid *cob_fid);
M0_INTERNAL void
m0_fid_convert_bstore2adstob(const struct m0_fid *bstore_fid,
			     struct m0_fid       *stob_domain_fid);
M0_INTERNAL void
m0_fid_convert_adstob2bstore(const struct m0_fid *stob_domain_fid,
			     struct m0_fid       *bstore_fid);

M0_INTERNAL uint32_t m0_fid_cob_device_id(const struct m0_fid *cob_fid);
M0_INTERNAL uint64_t m0_fid_conf_sdev_device_id(const struct m0_fid *sdev_fid);

M0_INTERNAL bool m0_fid_validate_gob(const struct m0_fid *gob_fid);
M0_INTERNAL bool m0_fid_validate_cob(const struct m0_fid *cob_fid);
M0_INTERNAL bool m0_fid_validate_adstob(const struct m0_stob_id *stob_id);
M0_INTERNAL bool m0_fid_validate_bstore(const struct m0_fid *bstore_fid);
M0_INTERNAL bool m0_fid_validate_linuxstob(const struct m0_stob_id *stob_id);

M0_INTERNAL uint32_t m0_fid__device_id_extract(const struct m0_fid *fid);

/** @} end of fidconvert group */
#endif /* __MERO_IOSERVICE_FID_CONVERT_H__ */

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
