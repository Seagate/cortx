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

#ifndef __MERO_MDSERVICE_FSYNC_FOMS_H__
#define __MERO_MDSERVICE_FSYNC_FOMS_H__

#include "fop/fop.h" /* m0_fom */

/**
 * Configuration of the fsync state machine. Defines the sm's phases
 * and additional information.
 */
M0_EXTERN struct m0_sm_conf m0_fsync_fom_conf;
extern struct m0_sm_state_descr m0_fsync_fom_phases[];

/**
 * Creates a FOM that can process fsync fop requests.
 * @param fop fsync fop request to be processed.
 * @param out output parameter pointing to the created fom.
 * @param reqh pointer to the request handler that will run the fom.
 * @return 0 if the fom was correctly created or an error code otherwise.
 */
M0_INTERNAL int m0_fsync_req_fom_create(struct m0_fop  *fop,
					struct m0_fom **out,
					struct m0_reqh *reqh);


#endif /* __MERO_MDSERVICE_FSYNC_FOMS_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
