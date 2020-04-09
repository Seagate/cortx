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
 * Original author: Madhavrao Vemuri <madhav_vemuri@xyratex.com>
 * Original creation date: 01/07/2013
 */

#include "lib/misc.h"

/**
   @addtogroup data_integrity

   @{
 */

static uint64_t md_crc32_cksum(void *data, uint64_t len, uint64_t *cksum);

/**
 * Table-driven implementaion of crc32.
 * CRC table is generated during first crc operation which contains the all
 * possible crc values for the byte of data. These values are used to
 * compute CRC for the all the bytes of data in the block of given length.
 *
 * This algorithm is implemented based on the guide at follwoing link,
 * @see http://www.repairfaq.org/filipg/LINK/F_crc_v3.html
 */

#define CRC_POLY	0x04C11DB7
#define CRC_WIDTH	32
#define CRC_SLICE_SIZE	8
#define CRC_TABLE_SIZE	256

uint32_t crc_table[CRC_TABLE_SIZE];
static bool is_table = false;

static void crc_mktable(void)
{
	int	 i;
	int	 j;
	uint32_t hibit = M0_BITS(CRC_WIDTH - 1);
	uint32_t crc;

	for (i = 0; i < CRC_TABLE_SIZE; i++) {
		crc = (uint32_t)i <<  (CRC_WIDTH - CRC_SLICE_SIZE);
		for(j = 0; j < CRC_SLICE_SIZE; j++) {
			crc <<= 1;
			if (crc &  hibit)
				crc ^= CRC_POLY;
		}
		crc_table[i] = crc;
	}
}

static uint32_t crc32(uint32_t crc, unsigned char const *data, m0_bcount_t len)
{
	M0_PRE(data != NULL);
	M0_PRE(len > 0);

	if (!is_table) {
		crc_mktable();
		is_table = true;
	}

	while (len--)
		crc = ((crc << CRC_SLICE_SIZE) | *data++) ^
			crc_table[crc >> (CRC_WIDTH - CRC_SLICE_SIZE) & 0xFF];

	return crc;
}

M0_INTERNAL void m0_crc32(const void *data, uint64_t len,
			  uint64_t *cksum)
{
	M0_PRE(data != NULL);
	M0_PRE(len > 0);
	M0_PRE(cksum != NULL);

	cksum[0] = crc32(~0, data, len);
}

M0_INTERNAL bool m0_crc32_chk(const void *data, uint64_t len,
			      const uint64_t *cksum)
{
	M0_PRE(data != NULL);
	M0_PRE(len > 0);
	M0_PRE(cksum != NULL);

	return cksum[0] == (uint64_t) crc32(~0, data, len);
}

static void md_crc32_cksum_set(void *data, uint64_t len, uint64_t *cksum)
{
	*cksum = md_crc32_cksum(data, len, cksum);
}

static uint64_t md_crc32_cksum(void *data, uint64_t len, uint64_t *cksum)
{
	uint64_t crc = ~0;
	uint64_t old_cksum = *cksum;

	*cksum = 0;
	crc = crc32(crc, data, len);
	*cksum = old_cksum;

	return crc;
}

static bool md_crc32_cksum_check(void *data, uint64_t len, uint64_t *cksum)
{
	return *cksum == md_crc32_cksum(data, len, cksum);
}

/** @} end of data_integrity */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
