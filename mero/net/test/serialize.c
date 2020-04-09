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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 06/28/2012
 */

#include "lib/misc.h"		/* M0_SET0 */
#include "lib/memory.h"		/* m0_alloc */
#include "lib/errno.h"		/* ENOMEM */
#include "lib/byteorder.h"	/* m0_byteorder_cpu_to_le16 */

#include "net/test/serialize.h"

/**
   @defgroup NetTestSerializeInternals Serialization
   @ingroup NetTestInternals

   @see
   @ref net-test

   @{
 */

/** Environment have LP64 data model */
M0_BASSERT(sizeof(long)   == 8);
M0_BASSERT(sizeof(void *) == 8);
M0_BASSERT(sizeof(int)    == 4);

static void net_test_serialize_cpu_to_le(char *d, char *s, m0_bcount_t len)
{
	if (len == 1) {
		*d = *s;
	} else if (len == 2) {
		* (uint16_t *) d = m0_byteorder_cpu_to_le16(* (uint16_t *) s);
	} else if (len == 4) {
		* (uint32_t *) d = m0_byteorder_cpu_to_le32(* (uint32_t *) s);
	} else if (len == 8) {
		* (uint64_t *) d = m0_byteorder_cpu_to_le64(* (uint64_t *) s);
	} else {
		M0_IMPOSSIBLE("len isn't a power of 2");
	}

}

static void net_test_serialize_le_to_cpu(char *d, char *s, m0_bcount_t len)
{
	if (len == 1) {
		*d = *s;
	} else if (len == 2) {
		* (uint16_t *) d = m0_byteorder_le16_to_cpu(* (uint16_t *) s);
	} else if (len == 4) {
		* (uint32_t *) d = m0_byteorder_le32_to_cpu(* (uint32_t *) s);
	} else if (len == 8) {
		* (uint64_t *) d = m0_byteorder_le64_to_cpu(* (uint64_t *) s);
	} else {
		M0_IMPOSSIBLE("len isn't a power of 2");
	}

}

/**
   Convert data to little endian representation.
   @pre len == 1 || len == 2 || len == 4 || len == 8
 */
static void net_test_serialize_reorder(enum m0_net_test_serialize_op op,
				       char *buf,
				       char *data,
				       m0_bcount_t len)
{
	M0_PRE(len == 1 || len == 2 || len == 4 || len == 8);

	if (op == M0_NET_TEST_SERIALIZE)
		net_test_serialize_cpu_to_le(buf, data, len);
	else
		net_test_serialize_le_to_cpu(data, buf, len);
}

/**
   Serialize/deserialize object field to buffer.
   Converts field to little-endian representation while serializing and
   reads field as little-endian from buffer while deserializing.
   @note bv_length is cached version of m0_vec_count(&bv->ov_vec).
   This value is ignored if bv == NULL.
   @pre data_len > 0
   @pre plain_data || data_len == 1 || data_len == 2 || data_len == 4 ||
	data_len == 8
   @see m0_net_test_serialize().
 */
static m0_bcount_t net_test_serialize_data(enum m0_net_test_serialize_op op,
					   void *data,
					   m0_bcount_t data_len,
					   bool plain_data,
					   struct m0_bufvec *bv,
					   m0_bcount_t bv_offset,
					   m0_bcount_t bv_length)
{
	struct m0_bufvec_cursor bv_cur;
	struct m0_bufvec_cursor data_cur;
	char			buf[8];
	void		       *data_addr = plain_data ? data : buf;
	struct m0_bufvec	data_bv = M0_BUFVEC_INIT_BUF(&data_addr,
							     &data_len);
	m0_bcount_t		copied;
	bool			end_reached;

	M0_PRE(data_len > 0);
	M0_PRE(plain_data || data_len == 1 || data_len == 2 ||
	       data_len == 4 || data_len == 8);

	/* if buffer is NULL and operation is 'serialize' then return size */
	if (bv == NULL)
		return op == M0_NET_TEST_SERIALIZE ? data_len : 0;
	/* if buffer is not large enough then return 0 */
	if (bv_offset + data_len > bv_length)
		return 0;

	/*
	   Take care about endianness.
	   Store all endian-dependent data in little-endian format.
	 */
	if (!plain_data && op == M0_NET_TEST_SERIALIZE)
		net_test_serialize_reorder(op, buf, data, data_len);

	/* initialize cursors and copy data */
	m0_bufvec_cursor_init(&bv_cur, bv);
	end_reached = m0_bufvec_cursor_move(&bv_cur, bv_offset);
	M0_ASSERT(!end_reached);

	m0_bufvec_cursor_init(&data_cur, &data_bv);

	if (op == M0_NET_TEST_SERIALIZE)
		copied = m0_bufvec_cursor_copy(&bv_cur, &data_cur, data_len);
	else
		copied = m0_bufvec_cursor_copy(&data_cur, &bv_cur, data_len);
	M0_ASSERT(copied == data_len);

	/*
	   Take care about endianness.
	   Read all endian-dependent data from little-endian buffer.
	 */
	if (!plain_data && op == M0_NET_TEST_DESERIALIZE)
		net_test_serialize_reorder(op, buf, data, data_len);

	return data_len;
}

m0_bcount_t m0_net_test_serialize_data(enum m0_net_test_serialize_op op,
				       void *data,
				       m0_bcount_t data_len,
				       bool plain_data,
				       struct m0_bufvec *bv,
				       m0_bcount_t bv_offset)
{
	m0_bcount_t bv_length = bv == NULL ? 0 : m0_vec_count(&bv->ov_vec);

	return net_test_serialize_data(op, data, data_len, plain_data,
				   bv, bv_offset, bv_length);
}

m0_bcount_t m0_net_test_serialize(enum m0_net_test_serialize_op op,
				  void *obj,
				  const struct m0_net_test_descr descr[],
				  size_t descr_nr,
				  struct m0_bufvec *bv,
				  m0_bcount_t bv_offset)
{
	size_t				i;
	const struct m0_net_test_descr *d_i;
	void			       *addr;
	m0_bcount_t			len_total = 0;
	m0_bcount_t			len;
	m0_bcount_t			bv_length;

	M0_PRE(op == M0_NET_TEST_SERIALIZE || op == M0_NET_TEST_DESERIALIZE);
	M0_PRE(obj != NULL);
	M0_PRE(descr != NULL);

	bv_length = bv == NULL ? 0 : m0_vec_count(&bv->ov_vec);

	for (i = 0; i < descr_nr; ++i) {
		d_i = &descr[i];
		addr = &((char *) obj)[d_i->ntd_offset];
		len = net_test_serialize_data(op, addr, d_i->ntd_length,
					      d_i->ntd_plain_data,
					      bv, bv_offset + len_total,
					      bv_length);
		len_total = net_test_len_accumulate(len_total, len);
		if (len_total == 0)
			break;
	}
	return len_total;
}

/**
   @} end of NetTestSerializeInternals group
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
