/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Yuriy Umanets <yuriy_umanets@xyratex.com>
 * Original creation date: 04-Apr-2014
 */

#include "lib/errno.h"         /* EINVAL */
#include "lib/misc.h"          /* memcmp, strcmp */
#include "lib/string.h"        /* sscanf */
#include "lib/assert.h"        /* M0_PRE */
#include "fid/fid_xc.h"
#include "fid/fid.h"

/**
   @addtogroup md_fid

   @{
 */

/**
   Cob storage root. Not exposed to user.
 */
M0_INTERNAL const struct m0_fid M0_COB_ROOT_FID = {
	.f_container = 1ULL,
	.f_key       = 1ULL
};

M0_INTERNAL const char M0_COB_ROOT_NAME[] = "ROOT";

M0_INTERNAL const struct m0_fid M0_DOT_MERO_FID = {
	.f_container = 1ULL,
	.f_key       = 4ULL
};

M0_INTERNAL const char M0_DOT_MERO_NAME[] = ".mero";

M0_INTERNAL const struct m0_fid M0_DOT_MERO_FID_FID = {
	.f_container = 1ULL,
	.f_key       = 5ULL
};

M0_INTERNAL const char M0_DOT_MERO_FID_NAME[] = "fid";

/**
   Namespace root fid. Exposed to user.
*/
M0_INTERNAL const struct m0_fid M0_MDSERVICE_SLASH_FID = {
	.f_container = 1ULL,
	.f_key       = (1ULL << 16) - 1
};

M0_INTERNAL const struct m0_fid M0_MDSERVICE_START_FID = {
	.f_container = 1ULL,
	.f_key       = (1ULL << 16)
};

/** @} end of md_fid group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
