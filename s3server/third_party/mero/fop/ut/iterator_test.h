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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 * Original creation date: 01/05/2011
 */

#pragma once

#ifndef __MERO_FOP_UT_ITERATOR_TEST_H__
#define __MERO_FOP_UT_ITERATOR_TEST_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"

struct m0_fop_seg {
	uint64_t fs_offset;
	uint64_t fs_count;
} M0_XCA_RECORD;

struct m0_fop_vec {
	uint32_t           fv_count;
	struct m0_fop_seg *fv_seg;
} M0_XCA_SEQUENCE;

struct m0_fop_optfid {
	struct m0_fid fo_fid;
	m0_void_t     fo_none;
} M0_XCA_RECORD;

struct m0_fop_recursive1 {
	struct m0_fid        fr_fid;
	struct m0_fop_vec    fr_seq;
	struct m0_fop_optfid fr_unn;
} M0_XCA_RECORD;

struct m0_fop_recursive2 {
	struct m0_fid            fr_fid;
	struct m0_fop_recursive1 fr_seq;
} M0_XCA_RECORD;

struct m0_fop_iterator_test {
	struct m0_fid            fit_fid;
	struct m0_fop_vec        fit_vec;
	struct m0_fop_optfid     fit_opt0;
	struct m0_fop_optfid     fit_opt1;
	struct m0_fop_optfid     fit_topt;
	struct m0_fop_recursive2 fit_rec;
} M0_XCA_RECORD;

#endif /* __MERO_FOP_UT_ITERATOR_TEST_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
