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
 * Original creation date: 13-Feb-2015
 */

#pragma once

#ifndef __MERO_CONF_LOAD_FOP_H__
#define __MERO_CONF_LOAD_FOP_H__


#include "lib/types.h"
#include "xcode/xcode_attr.h"
#include "lib/buf.h"
#include "lib/buf_xc.h"
#include "conf/fop.h"
#include "fid/fid_xc.h"         /* m0_fid_xc */
#include "fop/fop.h"            /* m0_fop */
#include "rpc/bulk.h"           /* m0_rpc_bulk */
#include "net/net_otw_types.h"  /* m0_net_buf_desc_data */

/**
   @section bulkclientDFSconffop Generic Conf Load FOP.
 */

M0_INTERNAL bool m0_is_conf_load_fop(const struct m0_fop *fop);
M0_INTERNAL bool m0_is_conf_load_fop_rep(const struct m0_fop *fop);
M0_INTERNAL struct m0_fop_conf_load *m0_conf_fop_to_load_fop(
						const struct m0_fop *fop);
M0_INTERNAL struct m0_fop_conf_load_rep *m0_conf_fop_to_load_fop_rep(
						const struct m0_fop *fop);
/**
   @} bulkclientDFSconffop end group
*/

/**
 * CalCulate segment size of bulk data
 */
M0_INTERNAL m0_bcount_t m0_conf_segment_size(struct m0_fop *fop);


/**
 * @defgroup conf_fops FOPs for Data Operations
 *
 * This component contains the File Operation Packets (FOP) definitions
 * for following operation
 * - transport configuration to confd
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
 * @section CONF LOAD FOP Definitions
 */

/**
 * Reply FOP request.
 */
struct m0_fop_conf_load_rep {
	/** Status code of operation. */
	int32_t  clfr_rc;
	/** Current version number */
	uint32_t clfr_version;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * FOP request.
 */
struct m0_fop_conf_load {
	/** New version number */
	uint32_t                    clf_version;
	/** Transaction ID */
	uint64_t                    clf_tx_id;
	/**
	 * Net buffer descriptor representing the m0_net_buffer containing
	 * the data buffer.
	 */
	struct m0_net_buf_desc_data clf_desc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#endif /* __MERO_CONF_LOAD_FOP_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
