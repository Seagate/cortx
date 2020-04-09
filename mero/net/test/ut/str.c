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
 * Original creation date: 09/03/2012
 */

#include "net/test/str.h"
#include "lib/vec.h"       /* m0_bufvec */
#include "lib/memory.h"    /* m0_free0 */
#include "ut/ut.h"

enum {
	STR_BUF_LEN    = 0x100,
	STR_BUF_OFFSET = 42,
};

static void try_serialize(char *str)
{
	char		 buf[STR_BUF_LEN];
	void		*addr = buf;
	m0_bcount_t	 buf_len = STR_BUF_LEN;
	struct m0_bufvec bv = M0_BUFVEC_INIT_BUF(&addr, &buf_len);
	m0_bcount_t	 serialized_len;
	m0_bcount_t	 len;
	char		*str2;
	int		 str_len;
	int		 rc;

	serialized_len = m0_net_test_str_serialize(M0_NET_TEST_SERIALIZE,
						   &str, &bv, STR_BUF_OFFSET);
	M0_UT_ASSERT(serialized_len > 0);

	str2 = NULL;
	len = m0_net_test_str_serialize(M0_NET_TEST_DESERIALIZE,
					&str2, &bv, STR_BUF_OFFSET);
	M0_UT_ASSERT(len == serialized_len);

	str_len = strlen(str);
	rc = strncmp(str, str2, str_len + 1);
	M0_UT_ASSERT(rc == 0);
	m0_free0(&str2);
}

void m0_net_test_str_ut(void)
{
	try_serialize("");
	try_serialize("asdf");
	try_serialize("SGVsbG8sIHdvcmxkIQo=");
	try_serialize("0123456789!@#$%^&*()qwertyuiopasdfghjklzxcvbnm"
		      "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	try_serialize(__FILE__);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
