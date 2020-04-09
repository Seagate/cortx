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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 *                  Rajesh Bhalerao <Rajesh_Bhalerao@xyratex.com>
 * Original creation date: 07/05/2013
 */

#pragma once

#ifndef __MERO_STOB_BATTR_H__
#define __MERO_STOB_BATTR_H__

/**
 * Identifiers of stob block attributes
 */

enum m0_battr_id {
	/**
	 * 64-bit of T10 Data Integrity Field for sector size of 512B
	 * - Reference Tag: 4 bytes
	 * - Meta Tag: 2 bytes
	 * - Guard Tag: 2 bytes
	 */
	M0_BI_T10_DIF_512B,
	/**
	 * 128-bit of T10 Data Integrity Field for sector size of 4KiB
	 * - Reference Tag: ? bytes
	 * - Meta Tag: ? bytes
	 * - Guard Tag: ? bytes
	 */
	M0_BI_T10_DIF_4KB_0,
	M0_BI_T10_DIF_4KB_1,
	/**
	 * 64-bits of data block version, used by DTM
	 */
	M0_BI_VERNO,
	/**
	 * 32-bit CRC checksum
	 */
	M0_BI_CKSUM_CRC_32,
	/**
	 * 32-bit Fletcher-32 checksum
	 */
	M0_BI_CKSUM_FLETCHER_32,
	/**
	 * 64-bit Fletcher-64 checksum
	 */
	M0_BI_CKSUM_FLETCHER_64,
	/**
	 * 256-bit SHA-256 checksum
	 */
	M0_BI_CKSUM_SHA256_0,
	M0_BI_CKSUM_SHA256_1,
	M0_BI_CKSUM_SHA256_2,
	M0_BI_CKSUM_SHA256_3,
	/**
	 * 64-bit Reference Tag
	 */
	M0_BI_REF_TAG,
};

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
