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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 06/28/2012
 */

#pragma once

#ifndef __MERO_NET_TEST_SERIALIZE_H__
#define __MERO_NET_TEST_SERIALIZE_H__

#include "lib/types.h"	/* m0_bcount_t */
#include "lib/vec.h"	/* m0_buvfec */

/**
   @defgroup NetTestSerializeDFS Serialization
   @ingroup NetTestDFS

   @see
   @ref net-test

   @{
 */

/** Operation type. @see m0_net_test_serialize(). */
enum m0_net_test_serialize_op {
	M0_NET_TEST_SERIALIZE,  /**< Serialize operation. */
	M0_NET_TEST_DESERIALIZE, /**< Deserialize operation. */
};

/** Field description. @see m0_net_test_serialize(). */
struct m0_net_test_descr {
	size_t ntd_offset;	/**< Data offset in structure */
	size_t ntd_length;	/**< Data length in structure */
	bool   ntd_plain_data;	/**< Do not care about endiannes */
};

#define TYPE_DESCR(type_name) \
	static const struct m0_net_test_descr type_name ## _descr[]

#define USE_TYPE_DESCR(type_name) \
	type_name ## _descr, ARRAY_SIZE(type_name ## _descr)

#define FIELD_SIZE(type, field) (sizeof ((type *) 0)->field)

/** Recommended to use in field declaration order */
#define FIELD_DESCR(type, field) {					\
	.ntd_offset	= offsetof(type, field),			\
	.ntd_length	= FIELD_SIZE(type, field),			\
	.ntd_plain_data	= false,					\
}

#define FIELD_DESCR_PLAIN(type, field) {				\
	.ntd_offset	= offsetof(type, field),			\
	.ntd_length	= FIELD_SIZE(type, field),			\
	.ntd_plain_data	= true,						\
}

/**
   Serialize or deserialize data.
   @pre data_len > 0
   @pre plain_data || data_len == 1 || data_len == 2 ||
		      data_len == 4 || data_len == 8
   @see m0_net_test_serialize().
 */
m0_bcount_t m0_net_test_serialize_data(enum m0_net_test_serialize_op op,
				       void *data,
				       m0_bcount_t data_len,
				       bool plain_data,
				       struct m0_bufvec *bv,
				       m0_bcount_t bv_offset);

/**
   Serialize or deserialize data structure with the given description.
   @param op Operation. Can be M0_NET_TEST_SERIALIZE or M0_NET_TEST_DESERIALIZE.
   @param obj Pointer to data structure.
   @param descr Array of data field descriptions.
   @param descr_nr Described fields number in descr.
   @param bv m0_bufvec. Can be NULL - in this case bv_offset is ignored.
   @param bv_offset Offset in bv.
   @return 0 No free space in buffer or descr_nr == 0.
   @return >0 Number of bytes read/written/will be written to/from buffer.
   @pre op == M0_NET_TEST_SERIALIZE || op == M0_NET_TEST_DESERIALIZE
   @pre obj != NULL
   @pre descr != NULL
   @note m0_net_test_descr.ntd_length can't be > 8 if
   m0_net_test_descr.ntd_plain_data is true.
 */
m0_bcount_t m0_net_test_serialize(enum m0_net_test_serialize_op op,
				  void *obj,
				  const struct m0_net_test_descr descr[],
				  size_t descr_nr,
				  struct m0_bufvec *bv,
				  m0_bcount_t bv_offset);

/**
   Get new len_total value after serializing part of data.
   @see @ref net-test-fspec-usecases-serialize
   @param accumulator Total serialized length before serializing current
		      part of data.
   @param addend Length of serialized current part of data.
		 0 means serializing failed.
   @return 0 if addend == 0
	   accumulator + addend otherwise.
 */
static inline m0_bcount_t net_test_len_accumulate(m0_bcount_t accumulator,
						  m0_bcount_t addend)
{
	return addend == 0 ? 0 : accumulator + addend;
}

/**
   @} end of NetTestSerializeDFS group
 */

#endif /*  __MERO_NET_TEST_SERIALIZE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
