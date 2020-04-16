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
 * Original author: Yuriy Umanets <yuriy_umanets@xyratex.com>
 * Original creation date: 03/29/2011
 */

#pragma once

#ifndef __MERO_MDSERVICE_MD_FOMS_H__
#define __MERO_MDSERVICE_MD_FOMS_H__

#include "mdservice/md_fops_xc.h"

struct m0_fom;
struct m0_fop;
struct m0_fid;

struct m0_cob;
struct m0_cob_nskey;
struct m0_cob_oikey;

struct m0_fom_md {
	/** Generic m0_fom object. */
	struct m0_fom      fm_fom;
	/** FOL record fragment to be added for meta-data operations. */
	struct m0_fol_frag fm_fol_frag;
};

enum m0_md_fom_phases {
        M0_FOPH_MD_GENERIC = M0_FOPH_NR + 1
};

M0_INTERNAL int m0_md_fop_init(struct m0_fop *fop, struct m0_fom *fom);

/**
   Init request fom for all types of requests.
*/
M0_INTERNAL int m0_md_req_fom_create(struct m0_fop *fop, struct m0_fom **m,
				     struct m0_reqh *reqh);
M0_INTERNAL int m0_md_rep_fom_create(struct m0_fop *fop, struct m0_fom **m,
				     struct m0_reqh *reqh);

#endif /* __MERO_MDSERVICE_MD_FOMS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
