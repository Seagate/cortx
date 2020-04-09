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
 * Original creation date: 25/07/2013
 */

#pragma once

#ifndef __MERO_FILE_DI_H__
#define __MERO_FILE_DI_H__

#include "lib/types.h"
#include "lib/vec.h"
#include "ioservice/io_fops.h"

struct m0_file;

/**
 * @defgroup data_integrity Data integrity using checksum
 *
 * Checksum for data blocks is computed based on checksum algorithm
 * selected from configuration.
 * Also checksum length is chosen based on this algorithm.
 *
 * A di data bufvec, consisting of a single buffer is added as m0_buf in
 * iofop and passed across the network.
 *
 * Data-integrity type represents a particular combination of data-integrity
 * methods applied to a file.
 *
 * i.e., data-integrity type prescribes a particular check-summing algorithm
 * with a particular block size, producing output of a particular size, plus,
 * possibly, a particular encryption algorithm, etc.
 *
 * Data integrity type and operations are initialized in m0_file.
 * Using do_sum(), checksum values are computed for each block of data and
 * using do_chk() checksum values are verified.
 *
 *  * @see https://docs.google.com/a/seagate.com/document/d/1reU_KtCmWRqHdX3nDkP
 pa9zQw5IvxZB1qT0ZKBnk8Uw/
 * @{
 * */
struct m0_di_type {
	/**
	 * Name can be something like "crc32-64K" or "t10-dif".
	 */
	const char *dt_name;
};

enum m0_di_checksum_len {
	M0_DI_CRC32_LEN = 1,
	M0_DI_ELEMENT_SIZE = 64,
};

enum m0_di_types {
	M0_DI_NONE,
	/** CRC32 checksum for block size data of 4k. */
	M0_DI_CRC32_4K,
	/** CRC32 checksum for block size data of 16k. */
	M0_DI_CRC32_64K,
	/** T10 tag for block size data of 4k. */
	M0_DI_T10_DIF,
	M0_DI_NR
};

enum {
	M0_DI_DEFAULT_TYPE =
#ifdef ENABLE_DATA_INTEGRITY
		M0_DI_CRC32_4K
#else
		M0_DI_NONE
#endif
};

struct m0_di_ops {
	const struct m0_di_type *do_type;
	/**
	 * Returns the mask of block attributes (stob/battr.h), used by this
	 * di type.
	 */
	uint64_t    (*do_mask)     (const struct m0_file *file);
	/**
	 * Shift (binary logarithm) of input block size.
	 *
	 * If this is ~0ULL, the algorithm applies to an entire file.
	 */
	uint64_t    (*do_in_shift) (const struct m0_file *file);
	/**
	 * Shift of output block size, that is, of number of bytes of di
	 * data (e.g., checksum) produced by this di type for each input block.
	 */
	uint64_t    (*do_out_shift)(const struct m0_file *file);
	/**
	 * Calculate di data for the input bufvec (which is not necessarily
	 * a multiple of input block size) and place the result in the
	 * appropriately sized output bufvec.
	 *
	 * Some parts of the output bufvec can be already filled by the caller,
	 * they should not be overwritten. For example, the application already
	 * calculated the t10-dif checksum, Mero only computes Reference and
	 * Application Tags.
	 * @param io_info contains offsets and sizes of data, used to compute
	 *		  tag values.
	 */
	void        (*do_sum)      (const struct m0_file *file,
				    const struct m0_indexvec *io_info,
				    const struct m0_bufvec *in,
				    struct m0_bufvec *out);
	/**
	 * Check that di data in output bufvec match the input bufvec.
	 * @param io_info contains offsets and sizes of data, used to compute
	 *		  tag values and compare with values in di data.
	 */
	bool        (*do_check)    (const struct m0_file *file,
				    const struct m0_indexvec *io_info,
				    const struct m0_bufvec *in,
				    const struct m0_bufvec *out);
};

/** Returns di ops for a given di_type. */
M0_INTERNAL const struct m0_di_ops *m0_di_ops_get(enum m0_di_types di_type);

/**
 * Computes the checksum for the region excluding checksum field and
 * sets this value in the checksum field.
 *
 * @param cksum_field Address of the checksum field
 */
M0_INTERNAL void m0_md_di_set(void *addr, m0_bcount_t nob,
			      uint64_t *cksum_field);

/**
 * Compares the checksum value in cksum_field with the computed checksum for
 * this region.
 *
 * @param cksum_field Address of the checksum field
 */
M0_INTERNAL bool m0_md_di_chk(void *addr, m0_bcount_t nob,
			      uint64_t *cksum_field);

#define M0_MD_DI_SET(obj, field)			   \
({							   \
	void *__obj = (obj);				   \
	m0_md_di_set(__obj, sizeof *(obj), &__obj->field); \
})

#define M0_MD_DI_CHK(obj, field)			   \
({							   \
	void *__obj = (obj);				   \
	m0_md_di_chk(__obj, sizeof *(obj), &__obj->field); \
})

/**
 * Computes crc32 checksum for data of length "len" and stores it in "cksum".
 *
 * @param data A block of data of size "len".
 * @param len Length of data.
 * @param cksum Checksum values to be computed are stored in it.
 */
M0_INTERNAL void m0_crc32(const void *data, uint64_t len,
			  uint64_t *cksum);

/**
 * Compares the crc32 checksum for data of length "len" with checksum values
 * in "cksum".
 *
 * @param data A block of data of size "len".
 * @param len Length of data.
 * @param cksum Checksum values to be verified are read from it..
 */
M0_INTERNAL bool m0_crc32_chk(const void *data, uint64_t len,
			      const uint64_t *cksum);

M0_INTERNAL m0_bcount_t m0_di_size_get(const struct m0_file *file,
				       const m0_bcount_t size);

/** @} end of data_integrity */
#endif /* __MERO_FILE_DI_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
