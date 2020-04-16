/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */
#pragma once

#ifndef __MERO_FDMI_FDMI_FOPS_H__
#define __MERO_FDMI_FDMI_FOPS_H__

#include "lib/types.h"
#include "xcode/xcode_attr.h"
#include "lib/buf.h"
#include "lib/buf_xc.h"
#include "fid/fid_xc.h"   /* m0_fid_xc */
#include "lib/types_xc.h" /* m0_uint128_xc */

extern struct m0_fop_type m0_fop_fdmi_rec_not_fopt;
extern struct m0_fop_type m0_fop_fdmi_rec_not_rep_fopt;
extern struct m0_fop_type m0_fop_fdmi_rec_release_fopt;
extern struct m0_fop_type m0_fop_fdmi_rec_release_rep_fopt;

/**
   @addtogroup fdmi_sd_int
   @{
*/

/**
 * FDMI record type (XCODE specific adaptation of @ref m0_fdmi_rec_type_id)
 *
 * Intention is to eliminate situations with
 * unknown data size at transcoding
 *
 * @see enum m0_fdmi_rec_type_id
 */
typedef uint32_t m0_fdmi_rec_type_id_t;


/** Matched FDMI filters list. Enumerates filters' ids */
struct m0_fdmi_flt_id_arr {
	/** Number of matched filters */
	uint32_t        fmf_count;

	/** Array of filter ids */
	struct m0_fid  *fmf_flt_id;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
 * FDMI record body record notification FOP is sent with
 */
struct m0_fop_fdmi_record {
	/** FDMI record ID. Should be unique within FDMI source dock instance */
	struct m0_uint128          fr_rec_id;

	/** FDMI record type, see @ref m0_fdmi_rec_type_id */
	uint32_t                   fr_rec_type;

	/** FDMI record data. Structure depends on record type */
	struct m0_buf              fr_payload;

	/** List of matched filter IDs */
	struct m0_fdmi_flt_id_arr  fr_matched_flts;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * FDMI record notification reply body
 */
/** @todo Q: Make it generic reply? */
struct m0_fop_fdmi_record_reply {
	m0_fdmi_rec_type_id_t frn_frt;   /**< FDMI record type */
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * FDMI record release request body
 */
/** @todo Q: Is it possible release FOP to be ONEWAY? */
struct m0_fop_fdmi_rec_release {
	struct m0_uint128     frr_frid;  /**< FDMI record id to release */
	m0_fdmi_rec_type_id_t frr_frt;   /**< FDMI record type */
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * FDMI record release reply
 */
struct m0_fop_fdmi_rec_release_reply {
	int frrr_rc;                  /**< release request result */
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);


M0_INTERNAL int m0_fdms_fop_init(void);
M0_INTERNAL void m0_fdms_fop_fini(void);

/** @} addtogroup fdmi_sd_int */

#endif /* __MERO_FDMI_FDMI_FOPS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
