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
 * Original creation date: 05/08/2013
 */

/**
 * @addtogroup data_integrity
 * @{
 */
#undef M0_TRACE_SUBSYSTEM
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FILE

#include "lib/trace.h"
#include "stob/battr.h"
#include "file/di.h"
#include "file/file.h"
#include "lib/misc.h"
#include "lib/vec_xc.h"
#include "file/crc.c"

struct di_info {
	m0_bcount_t d_bsize; /**< Block size of the data. */
	uint32_t    d_bit_set_nr; /**< Number of elements in a group. */
	uint32_t    d_pos; /**< Start position of the element in group. */
	uint32_t    d_blks_nr; /**< Number of data blocks of size d_bsize. */
};

static m0_bcount_t current_pos(const struct di_info *info, int i)
{
	M0_PRE(info != NULL);

	return info->d_pos + i * info->d_bit_set_nr;
}

static uint64_t file_di_crc_mask(const struct m0_file *file);
static uint64_t file_di_crc_in_shift(const struct m0_file *file);
static uint64_t file_di_crc_out_shift(const struct m0_file *file);
static void	file_di_crc_sum(const struct m0_file *file,
			        const struct m0_indexvec *io_info,
			        const struct m0_bufvec *in_vec,
			        struct m0_bufvec *di_vec);
static bool	file_di_crc_check(const struct m0_file *file,
				  const struct m0_indexvec *io_info,
				  const struct m0_bufvec *in_vec,
				  const struct m0_bufvec *di_vec);
static void	file_checksum(void (*checksum)(const void *data,
					       m0_bcount_t bsize,
					       uint64_t *csum),
			      const struct m0_bufvec *in_vec,
			      const struct m0_indexvec *io_info,
			      struct di_info *di,
			      struct m0_bufvec *di_vec);
static bool	file_checksum_check(bool (*checksum)(const void *data,
						     m0_bcount_t bsize,
						     const uint64_t *csum),
				    const struct m0_bufvec *in_vec,
				    const struct m0_indexvec *io_info,
				    struct di_info *di,
				    const struct m0_bufvec *di_vec);

static void t10_ref_tag_compute(const struct m0_indexvec *io_info,
				struct di_info *di,
				struct m0_bufvec *di_vec);
static bool t10_ref_tag_check(const struct m0_indexvec *io_info,
			      struct di_info *di,
			      const struct m0_bufvec *di_vec);

static void file_di_info_setup(const struct m0_file *file,
			       const struct m0_indexvec *io_info,
			       struct di_info *di);

static struct m0_di_type file_di_crc = {
	.dt_name = "crc32-4k+t10-ref-tag",
};

static struct m0_di_type file_di_none_type = {
	.dt_name = "di-none",
};

static void file_di_none_sum(const struct m0_file *file,
			     const struct m0_indexvec *io_info,
			     const struct m0_bufvec *in_vec,
			     struct m0_bufvec *di_vec)
{
	return;
}

static bool file_di_none_check(const struct m0_file *file,
			       const struct m0_indexvec *io_info,
			       const struct m0_bufvec *in_vec,
			       const struct m0_bufvec *di_vec)
{
	return true;
}

static uint64_t file_di_none_mask(const struct m0_file *file)
{
	return 0;
}

static uint64_t file_di_none_in_shift(const struct m0_file *file)
{
	return 12;
}

static uint64_t file_di_none_out_shift(const struct m0_file *file)
{
	return 0;
}

static const struct m0_di_ops di_ops[M0_DI_NR] = {
	[M0_DI_NONE] = {
		.do_type      = &file_di_none_type,
		.do_mask      = file_di_none_mask,
		.do_in_shift  = file_di_none_in_shift,
		.do_out_shift = file_di_none_out_shift,
		.do_sum       = file_di_none_sum,
		.do_check     = file_di_none_check,
	},

	[M0_DI_CRC32_4K] = {
		.do_type      = &file_di_crc,
		.do_mask      = file_di_crc_mask,
		.do_in_shift  = file_di_crc_in_shift,
		.do_out_shift = file_di_crc_out_shift,
		.do_sum       = file_di_crc_sum,
		.do_check     = file_di_crc_check,
	},
};

static uint64_t file_di_crc_mask(const struct m0_file *file)
{
	return M0_BITS(M0_BI_CKSUM_CRC_32, M0_BI_REF_TAG);
}

static uint64_t file_di_crc_in_shift(const struct m0_file *file)
{
	return 12;
}

static uint64_t file_di_crc_out_shift(const struct m0_file *file)
{
	return m0_no_of_bits_set(file_di_crc_mask(file));
}

static void file_di_crc_sum(const struct m0_file *file,
			    const struct m0_indexvec *io_info,
			    const struct m0_bufvec *in_vec,
			    struct m0_bufvec *di_vec)
{
	struct di_info di;

	M0_PRE(file != NULL);
	M0_PRE(in_vec != NULL);
	M0_PRE(di_vec != NULL);
	M0_PRE(io_info != NULL);

	file_di_info_setup(file, io_info, &di);

	file_checksum(&m0_crc32, in_vec, io_info, &di, di_vec);
	di.d_pos += M0_DI_CRC32_LEN;
	t10_ref_tag_compute(io_info, &di, di_vec);
}

static void file_di_info_setup(const struct m0_file *file,
			       const struct m0_indexvec *io_info,
			       struct di_info *di)
{
	di->d_pos = 0;
	di->d_bsize = M0_BITS(file->fi_di_ops->do_in_shift(file));
	di->d_bit_set_nr = m0_no_of_bits_set(
				file->fi_di_ops->do_mask(file));
	di->d_blks_nr = m0_vec_count(&io_info->iv_vec) / di->d_bsize;
}

static bool file_di_invariant(const struct m0_bufvec *in_vec,
			      const struct m0_bufvec *di_vec,
			      const struct m0_indexvec *io_info,
			      const struct di_info *di)
{
        struct m0_bufvec_cursor  data_cur;
        struct m0_bufvec_cursor  cksum_cur;

	m0_bufvec_cursor_init(&data_cur, in_vec);
        m0_bufvec_cursor_init(&cksum_cur, di_vec);

	return
		_0C(in_vec != NULL) &&
		_0C(di_vec != NULL) &&
		_0C(io_info != NULL) &&
		_0C(di != NULL) &&
		_0C(di->d_bsize > 0) &&
		_0C(di->d_bit_set_nr > 0) &&
		_0C(!m0_bufvec_cursor_move(&data_cur,
			(di->d_blks_nr - 1) * di->d_bsize)) &&
		_0C(!m0_bufvec_cursor_move(&cksum_cur,
		(current_pos(di, di->d_blks_nr) - 1) * M0_DI_ELEMENT_SIZE));
}

/*
 * For each data block in "in_vec" computes checksum using "checksum()" function
 * and store the result as the "di->d_pos"-th block attribute in "di_vec".
 *
 *  @param in_vec   Input data blocks.
 *  @param io_info  Data offset and count values.
 *  @param di	    Consist of position, number of elements in a group
 *		    and block size of the data.
 *  @param di_vec   Di data to be computed.
 */
static void file_checksum(void (*checksum)(const void *data, m0_bcount_t bsize,
					   uint64_t *csum),
			  const struct m0_bufvec *in_vec,
			  const struct m0_indexvec *io_info,
			  struct di_info *di,
			  struct m0_bufvec *di_vec)
{
	int			 i;
        struct m0_bufvec_cursor  data_cur;
        struct m0_bufvec_cursor  cksum_cur;
	uint8_t			*blk_data;
	uint64_t		*cksum;

	M0_ENTRY();
	M0_PRE(file_di_invariant(in_vec, di_vec, io_info, di));

	m0_bufvec_cursor_init(&data_cur, in_vec);
        m0_bufvec_cursor_init(&cksum_cur, di_vec);
	cksum = m0_bufvec_cursor_addr(&cksum_cur);
	for (i = 0; i < di->d_blks_nr; i++) {
		blk_data = m0_bufvec_cursor_addr(&data_cur);
		checksum(blk_data, di->d_bsize, cksum + current_pos(di, i));
		m0_bufvec_cursor_move(&data_cur, di->d_bsize);
	}

	M0_LEAVE();
}

static bool file_di_crc_check(const struct m0_file *file,
			      const struct m0_indexvec *io_info,
			      const struct m0_bufvec *in_vec,
			      const struct m0_bufvec *di_vec)
{
	struct di_info di;
	bool	       check;

	M0_PRE(file != NULL);
	M0_PRE(in_vec != NULL);
	M0_PRE(di_vec != NULL);
	M0_PRE(io_info != NULL);

	file_di_info_setup(file, io_info, &di);
	check = file_checksum_check(&m0_crc32_chk, in_vec, io_info, &di,
				    di_vec);
	if (check) {
		di.d_pos += M0_DI_CRC32_LEN;
		return t10_ref_tag_check(io_info, &di, di_vec);
	}

	return check;
}

/*
 * For each data block in "in_vec" computes checksum using "checksum()"
 * compare the result with the "di->d_pos"-th block attribute in
 * "di_vec".
 *
 *  @param in_vec   Input data blocks.
 *  @param io_info  Data offset and count values.
 *  @param di	    Consist of position, number of elements in a group
 *		    and block size of the data.
 *  @param di_vec   Di data to be verified.
 */
static bool file_checksum_check(bool (*checksum)(const void *data,
						 m0_bcount_t bsize,
						 const uint64_t *csum),
				const struct m0_bufvec *in_vec,
				const struct m0_indexvec *io_info,
				struct di_info *di,
				const struct m0_bufvec *di_vec)
{
	int			 i;
        struct m0_bufvec_cursor  data_cur;
        struct m0_bufvec_cursor  cksum_cur;
	uint8_t			*blk_data;
	uint64_t		*cksum;

	M0_ENTRY();
	M0_PRE(file_di_invariant(in_vec, di_vec, io_info, di));

	m0_bufvec_cursor_init(&data_cur, in_vec);
        m0_bufvec_cursor_init(&cksum_cur, di_vec);
	cksum = m0_bufvec_cursor_addr(&cksum_cur);
	for (i = 0; i < di->d_blks_nr; i++) {
		blk_data = m0_bufvec_cursor_addr(&data_cur);
		if (!checksum(blk_data, di->d_bsize,
			      cksum + current_pos(di, i)))
			return M0_RC(false);
		m0_bufvec_cursor_move(&data_cur, di->d_bsize);
	}

	return M0_RC(true);
}

static void t10_ref_tag_compute(const struct m0_indexvec *io_info,
				struct di_info *di,
				struct m0_bufvec *di_vec)
{
        struct m0_bufvec_cursor cksum_cur;
	uint64_t	       *cksum;
	int			i;

	M0_ENTRY();

	m0_bufvec_cursor_init(&cksum_cur, di_vec);
	cksum = m0_bufvec_cursor_addr(&cksum_cur);
	for (i = 0; i < io_info->iv_vec.v_nr; i++)
		cksum[current_pos(di, i)] = io_info->iv_index[i] +
					    io_info->iv_vec.v_count[i];
	M0_LEAVE();
}

static bool t10_ref_tag_check(const struct m0_indexvec *io_info,
			      struct di_info *di,
			      const struct m0_bufvec *di_vec)
{
        struct m0_bufvec_cursor cksum_cur;
	uint64_t	       *cksum;
	int			i;

	M0_ENTRY();

        m0_bufvec_cursor_init(&cksum_cur, di_vec);
	cksum = m0_bufvec_cursor_addr(&cksum_cur);
	for (i = 0; i < io_info->iv_vec.v_nr; i++) {
		m0_bcount_t cur_pos = current_pos(di,  i);

		if (cksum[cur_pos] != io_info->iv_index[i] +
				      io_info->iv_vec.v_count[i]) {
			M0_LOG(M0_ERROR,"Segment no: %d TAG value is %d \n", i,
					(int)cksum[cur_pos]);
			return M0_RC(false);
		}
	}

	return M0_RC(true);
}

M0_INTERNAL const struct m0_di_ops *m0_di_ops_get(enum m0_di_types di_type)
{
	M0_PRE(IS_IN_ARRAY(di_type, di_ops));

	return &di_ops[di_type];
}

M0_INTERNAL void m0_md_di_set(void *addr, m0_bcount_t nob,
			      uint64_t *cksum_field)
{
	md_crc32_cksum_set(addr, nob, cksum_field);
}

M0_INTERNAL bool m0_md_di_chk(void *addr, m0_bcount_t nob,
			      uint64_t *cksum_field)
{
	return md_crc32_cksum_check(addr, nob, cksum_field);
}

M0_INTERNAL m0_bcount_t m0_di_size_get(const struct m0_file *file,
				       const m0_bcount_t size)
{
	M0_PRE(file != NULL);

	return file->fi_di_ops->do_out_shift(file) *
	       (size / M0_BITS(file->fi_di_ops->do_in_shift(file))) *
	       M0_DI_ELEMENT_SIZE;
}

#undef M0_TRACE_SUBSYSTEM
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
