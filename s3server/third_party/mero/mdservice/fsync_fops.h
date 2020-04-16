/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED, A SEAGATE COMPANY
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
 * Original authors: James Morse   <james_morse@xyratex.com>,
 *                   Juan Gonzalez <juan_gonzalez@xyratex.com>,
 *                   Sining Wu     <sining_wu@xyratex.com>
 * Original creation date: 09-Apr-2014
 */

#pragma once

#ifndef __MERO_MDSERVICE_FSYNC_FOPS_H__
#define __MERO_MDSERVICE_FSYNC_FOPS_H__

#include "lib/types.h" /* uint32_t uin64_t ... */
#include "xcode/xcode_attr.h" /* M0_XCA_RECORD */
#include "fop/fom.h"
#include "be/tx.h"
#include "be/tx_xc.h"

/*
 * A fsync fop is sent to a mdservice/ioservice instance to guarantee all the
 * BE transactions up to certain point have been successfully committed.
 *
 * This code is not in md_fops.h because we need it to be reusable from the
 * ioservice. The rest of fops in md_fops.h must remain isolated from ioservice.
 */


/**
 * fsync request fop.
 */
struct m0_fop_fsync {
	/** Remote ID of the target transaction */
	struct m0_be_tx_remid   ff_be_remid;

	/** Store one of enum m0_fsync_mode */
	uint32_t                ff_fsync_mode;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

enum m0_fsync_mode {
	M0_FSYNC_MODE_PASSIVE,
	M0_FSYNC_MODE_ACTIVE,
};

/**
* fsync reply fop
*/
struct m0_fop_fsync_rep {
	/** return code for the fsync operation. 0 for success */
	int32_t                ffr_rc;

	/**
	 * Remote ID of the last logged transaction.
	 * May be later than requested.
	 */
	struct m0_be_tx_remid   ffr_be_remid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);


extern struct m0_fop_type m0_fop_fsync_mds_fopt;
extern struct m0_fop_type m0_fop_fsync_rep_fopt;

extern const struct m0_fom_type_ops m0_fsync_fom_ops;
extern const struct m0_fop_type_ops m0_fsync_fop_ops;

/**
 * Initializes the fsync fop type so the caller (typically the mdservice
 * or the ioservice) can use it.After success, the service will be able to
 * handle fsync fops.
 * @param svct The service type of the caller that is going to support fsync
 * fops.
 * @return 0 if the operation succeeded or a relevant error code otherwise.
 */
M0_INTERNAL int m0_mdservice_fsync_fop_init(struct m0_reqh_service_type * svct);

/**
 * Releases the fsync fop type and all the structures associated to it.
 * Must be invoked only once after m0_mdservice_fsync_fop_init has succeeded.
 */
M0_INTERNAL void m0_mdservice_fsync_fop_fini(void);

#endif /* __MERO_MDSERVICE_FSYNC_FOPS_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
