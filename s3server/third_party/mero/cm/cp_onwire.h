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
 * Original author: Anup Barve <anup_barve@xyratex.com>
 * Original creation date: 02/15/2013
 */

#pragma once

#ifndef __MERO_CM_CP_ONWIRE_H__
#define __MERO_CM_CP_ONWIRE_H__

#include "lib/bitmap.h"            /* m0_bitmap */
#include "lib/bitmap_xc.h"         /* m0_bitmap_xc */
#include "ioservice/io_fops.h"     /* m0_io_descs */
#include "ioservice/io_fops_xc.h"  /* m0_io_descs_xc */
#include "cm/ag.h"                 /* m0_cm_ag_id */
#include "cm/ag_xc.h"              /* m0_cm_ag_id_xc */

/** Onwire copy packet structure. */
struct m0_cpx {
	/** Copy packet priority. */
	uint32_t                  cpx_prio;

	/**
	 * Aggregation group id corresponding to an aggregation group,
	 * to which the copy packet belongs.
	 */
	struct m0_cm_ag_id        cpx_ag_id;

	/** Global index of this copy packet in aggregation group. */
	uint64_t                  cpx_ag_cp_idx;

	/** Bitmap representing the accumulator information. */
	struct m0_bitmap_onwire   cpx_bm;

	/** Network buffer descriptors corresponding to copy packet data. */
	struct m0_io_descs        cpx_desc;

	/**
	 * Epoch of the copy packet, the same as that from the copy machine
	 * which this copy packet belongs to.
	 */
	m0_time_t                 cpx_epoch;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Onwire copy packet reply. */
struct m0_cpx_reply {
	int32_t           cr_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#endif /*  __MERO_CM_CP_ONWIRE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
