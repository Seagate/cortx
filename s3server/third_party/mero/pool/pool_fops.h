/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Huang Hua <Hua_Huang@xyratex.com>
 * Original creation date: 06/19/2013
 */

#pragma once

#ifndef __MERO_POOL_POOL_FOPS_H__
#define __MERO_POOL_POOL_FOPS_H__

#include "lib/types.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"
#include "xcode/xcode_attr.h"

extern struct m0_fop_type m0_fop_poolmach_query_fopt;
extern struct m0_fop_type m0_fop_poolmach_query_rep_fopt;
extern struct m0_fop_type m0_fop_poolmach_set_fopt;
extern struct m0_fop_type m0_fop_poolmach_set_rep_fopt;

M0_INTERNAL void m0_poolmach_fop_fini(void);
M0_INTERNAL int m0_poolmach_fop_init(void);

struct m0_fop_poolmach_set_rep {
	int32_t  fps_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_fop_poolmach_dev_info {
	uint32_t                    fpi_nr;
	struct m0_fop_poolmach_dev *fpi_dev;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

struct m0_fop_poolmach_dev {
	uint32_t      fpd_state;
	struct m0_fid fpd_fid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_fop_poolmach_set {
	uint32_t                        fps_type;
	struct m0_fop_poolmach_dev_info fps_dev_info;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_fop_poolmach_query_rep {
	int32_t                         fqr_rc;
	struct m0_fop_poolmach_dev_info fqr_dev_info;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_fop_poolmach_dev_idx {
	uint32_t       fpx_nr;
	struct m0_fid *fpx_fid;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

struct m0_fop_poolmach_query {
	uint32_t                       fpq_type;
	struct m0_fop_poolmach_dev_idx fpq_dev_idx;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#endif /* __MERO_POOL_POOL_FOPS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
