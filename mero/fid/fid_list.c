/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER
#include "fid/fid_list.h"
#include "mero/magic.h"

/**
   @addtogroup fid

   @{
 */

M0_TL_DESCR_DEFINE(m0_fids, "m0_fid list", M0_INTERNAL,
		   struct m0_fid_item, i_link, i_magic,
		   M0_FID_MAGIC, M0_FID_HEAD_MAGIC);

M0_TL_DEFINE(m0_fids, M0_INTERNAL, struct m0_fid_item);

#undef M0_TRACE_SUBSYSTEM

/** @} end of fid group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
