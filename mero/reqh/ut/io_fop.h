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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 10/29/2011
 */

#pragma once

#ifndef __MERO_STOB_IO_FOP_H__
#define __MERO_STOB_IO_FOP_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"

struct stob_io_fop_fid {
	uint64_t f_seq;
	uint64_t f_oid;
} M0_XCA_RECORD;

struct m0_fi_value {
	uint32_t fi_count;
	uint8_t *fi_buf;
} M0_XCA_SEQUENCE;

struct m0_stob_io_write {
	struct stob_io_fop_fid fiw_object;
	struct m0_fi_value     fiw_value;
} M0_XCA_RECORD;

struct m0_stob_io_write_rep {
	int32_t  fiwr_rc;
	uint32_t fiwr_count;
} M0_XCA_RECORD;

struct m0_stob_io_read {
	struct stob_io_fop_fid fir_object;
} M0_XCA_RECORD;

struct m0_stob_io_read_rep {
	int32_t            firr_rc;
	uint32_t           firr_count;
	struct m0_fi_value firr_value;
} M0_XCA_RECORD;

struct m0_stob_io_create {
	struct stob_io_fop_fid fic_object;
} M0_XCA_RECORD;

struct m0_stob_io_create_rep {
	int32_t ficr_rc;
} M0_XCA_RECORD;

extern struct m0_fop_type m0_stob_io_create_fopt;
extern struct m0_fop_type m0_stob_io_read_fopt;
extern struct m0_fop_type m0_stob_io_write_fopt;

extern struct m0_fop_type m0_stob_io_create_rep_fopt;
extern struct m0_fop_type m0_stob_io_read_rep_fopt;
extern struct m0_fop_type m0_stob_io_write_rep_fopt;

void m0_stob_io_fop_init(void);
void m0_stob_io_fop_fini(void);

#endif /* !__MERO_STOB_IO_FOP_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
