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
 * Original author: Mikhail Antropov <mikhail.antropov@xyratex.com>
 * Original creation date: 09/25/2015
 */

#pragma once

#ifndef __MERO_FID_FID_LIST_H__
#define __MERO_FID_FID_LIST_H__

/**
   @defgroup fid File identifier

   @{
 */

#include "fid/fid.h"
#include "lib/tlist.h"

/*
 * Item description FIDs list m0_tl
*/
struct m0_fid_item {
	struct m0_fid   i_fid;
	struct m0_tlink i_link;
	uint64_t        i_magic;
};

M0_TL_DESCR_DECLARE(m0_fids, M0_EXTERN);
M0_TL_DECLARE(m0_fids, M0_INTERNAL, struct m0_fid_item);


/** @} end of fid group */
#endif /* __MERO_FID_FID_LIST_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
