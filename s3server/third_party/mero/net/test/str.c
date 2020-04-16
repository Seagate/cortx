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
 * Original creation date: 09/03/2012
 */

#ifndef __KERNEL__
#include <string.h>		/* strlen */
#else
#include <linux/string.h>	/* strlen */
#endif

#include "lib/memory.h"	/* m0_alloc */
#include "lib/misc.h"           /* ergo, ARRAY_SIZE */

#include "mero/magic.h"	/* M0_NET_TEST_STR_MAGIC */

#include "net/test/str.h"

/**
   @defgroup NetTestStrInternals Serialization of ASCIIZ string
   @ingroup NetTestInternals

   @see
   @ref net-test

   @{
 */

struct net_test_str_len {
	/** M0_NET_TEST_STR_MAGIC */
	uint64_t ntsl_magic;
	size_t   ntsl_len;
};

TYPE_DESCR(net_test_str_len) = {
	FIELD_DESCR(struct net_test_str_len, ntsl_magic),
	FIELD_DESCR(struct net_test_str_len, ntsl_len),
};

m0_bcount_t m0_net_test_str_serialize(enum m0_net_test_serialize_op op,
				      char **str,
				      struct m0_bufvec *bv,
				      m0_bcount_t bv_offset)
{
	struct net_test_str_len str_len;
	m0_bcount_t		len;
	m0_bcount_t		len_total;

	M0_PRE(op == M0_NET_TEST_SERIALIZE || op == M0_NET_TEST_DESERIALIZE);
	M0_PRE(str != NULL);
	M0_PRE(ergo(op == M0_NET_TEST_SERIALIZE, *str != NULL));

	if (op == M0_NET_TEST_SERIALIZE) {
		str_len.ntsl_len = strlen(*str) + 1;
		str_len.ntsl_magic = M0_NET_TEST_STR_MAGIC;
	}
	len = m0_net_test_serialize(op, &str_len,
				    USE_TYPE_DESCR(net_test_str_len),
				    bv, bv_offset);
	len_total = net_test_len_accumulate(0, len);
	if (len_total != 0) {
		if (op == M0_NET_TEST_DESERIALIZE) {
			if (str_len.ntsl_magic != M0_NET_TEST_STR_MAGIC)
				return 0;
			*str = m0_alloc(str_len.ntsl_len);
			if (*str == NULL)
				return 0;
		}
		len = m0_net_test_serialize_data(op, *str, str_len.ntsl_len,
						 true,
						 bv, bv_offset + len_total);
		len_total = net_test_len_accumulate(len_total, len);
	};

	return len_total;
}

/**
   @} end of NetTestStrInternals group
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
