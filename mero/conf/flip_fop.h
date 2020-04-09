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
 * Original author: Mikhail Antropov <mikhail.v.antropov@xyratex.com>
 * Original creation date: 16-Mar-2015
 */

#pragma once

#ifndef __MERO_CONF_FLIP_FOP_H__
#define __MERO_CONF_FLIP_FOP_H__


#include "lib/buf.h"
#include "lib/types.h"
#include "xcode/xcode_attr.h"
#include "conf/fop.h"
#include "fid/fid_xc.h"  /* m0_fid_xc */
#include "fop/fop.h"     /* m0_fop */


/**
   @section bulkclientDFSconffop Generic conf flip fop.
 */
M0_INTERNAL bool m0_is_conf_flip_fop(const struct m0_fop *fop);
M0_INTERNAL bool m0_is_conf_flip_fop_rep(const struct m0_fop *fop);
M0_INTERNAL struct m0_fop_conf_flip *m0_conf_fop_to_flip_fop(
						const struct m0_fop *fop);
M0_INTERNAL struct m0_fop_conf_flip_rep *m0_conf_fop_to_flip_fop_rep(
						const struct m0_fop *fop);
/**
   @} bulkclientDFSconffop end group
*/

/**
 * @defgroup conf_fops FOPs for Data Operations
 *
 * This component contains the File Operation Packets (FOP) definitions
 * for following operation
 * - flip ConfD configuration to confd
 *
 * It describes the FOP formats along with brief description of the flow.
 *
 * Note: As authorization is carried on server, all request FOPs
 * contain uid and gid. For authentication, nid is included in every FOP.
 * This is to serve very primitive authentication for now.
 *
 * @{
 */

/**
 * @section CONF flip FOP Definitions
 */

/**
 * Reply FOP request.
 */
struct m0_fop_conf_flip_rep {
	/** Status code of operation. */
	int32_t cffr_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * FOP request.
 */
struct m0_fop_conf_flip {
	/** Previous version number */
	uint32_t cff_prev_version;
	/** New version number */
	uint32_t cff_next_version;
	/** Transaction ID */
	uint64_t cff_tx_id;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/* __MERO_CONF_FLIP_FOP_H__ */
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
