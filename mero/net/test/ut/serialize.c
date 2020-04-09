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

#ifndef __KERNEL__
#include <limits.h>		/* CHAR_MAX */
#else
#include <linux/kernel.h>	/* INT_MIN */
#endif

#include "lib/types.h"		/* UINT32_MAX */

#include "lib/misc.h"		/* M0_SET0 */
#include "ut/ut.h"		/* M0_UT_ASSERT */
#include "lib/vec.h"		/* m0_bufvec */

#include "net/test/serialize.h"

enum {
	SERIALIZE_BUF_LEN = 0x100,
};

/* define limits for kernel mode */
/* max: should I add lib/limits.h? */
#ifdef __KERNEL__
#define CHAR_MIN (-(CHAR_MAX) - 1)
#define CHAR_MAX 127
#define UCHAR_MAX (~0)
#ifndef SHRT_MIN
#define SHRT_MIN SHORT_MIN
#endif
#ifndef SHRT_MAX
#define SHRT_MAX SHORT_MAX
#endif
#ifndef USHRT_MAX
#define USHRT_MAX USHORT_MAX
#endif
#endif

struct simple_struct {
	char		   ss_c;
	unsigned char	   ss_uc;
	short		   ss_s;
	unsigned short	   ss_us;
	int		   ss_i;
	unsigned int	   ss_ui;
	long		   ss_l;
	unsigned long	   ss_ul;
	long long	   ss_ll;
	unsigned long long ss_ull;
	int8_t		   ss_i8;
	uint8_t		   ss_u8;
	int16_t		   ss_i16;
	uint16_t	   ss_u16;
	int32_t		   ss_i32;
	uint32_t	   ss_u32;
	int64_t		   ss_i64;
	uint64_t	   ss_u64;
};

/* simple_struct_descr */
TYPE_DESCR(simple_struct) = {
	FIELD_DESCR(struct simple_struct, ss_c),
	FIELD_DESCR(struct simple_struct, ss_uc),
	FIELD_DESCR(struct simple_struct, ss_s),
	FIELD_DESCR(struct simple_struct, ss_us),
	FIELD_DESCR(struct simple_struct, ss_i),
	FIELD_DESCR(struct simple_struct, ss_ui),
	FIELD_DESCR(struct simple_struct, ss_l),
	FIELD_DESCR(struct simple_struct, ss_ul),
	FIELD_DESCR(struct simple_struct, ss_ll),
	FIELD_DESCR(struct simple_struct, ss_ull),
	FIELD_DESCR(struct simple_struct, ss_i8),
	FIELD_DESCR(struct simple_struct, ss_u8),
	FIELD_DESCR(struct simple_struct, ss_i16),
	FIELD_DESCR(struct simple_struct, ss_u16),
	FIELD_DESCR(struct simple_struct, ss_i32),
	FIELD_DESCR(struct simple_struct, ss_u32),
	FIELD_DESCR(struct simple_struct, ss_i64),
	FIELD_DESCR(struct simple_struct, ss_u64),
};

static m0_bcount_t simple_struct_serialize(enum m0_net_test_serialize_op op,
					   struct simple_struct *ss,
					   struct m0_bufvec *bv,
					   m0_bcount_t bv_offset)
{
	return m0_net_test_serialize(op, ss, USE_TYPE_DESCR(simple_struct),
				     bv, bv_offset);
}

static void simple_struct_test(char		   c,
			       unsigned char	   uc,
			       short		   s,
			       unsigned short	   us,
			       int		   i,
			       unsigned int	   ui,
			       long		   l,
			       unsigned long	   ul,
			       long long	   ll,
			       unsigned long long ull,
			       int8_t		   i8,
			       uint8_t		   u8,
			       int16_t		   i16,
			       uint16_t		   u16,
			       int32_t		   i32,
			       uint32_t		   u32,
			       int64_t		   i64,
			       uint64_t		   u64)
{
	m0_bcount_t	     ss_serialized_len;
	m0_bcount_t	     rc_bcount;
	char		     buf[SERIALIZE_BUF_LEN];
	void		    *addr = buf;
	m0_bcount_t	     len = SERIALIZE_BUF_LEN;
	struct m0_bufvec     bv = M0_BUFVEC_INIT_BUF(&addr, &len);
	struct simple_struct ss = {
		.ss_c   = c,
		.ss_uc  = uc,
		.ss_s   = s,
		.ss_us  = us,
		.ss_i   = i,
		.ss_ui  = ui,
		.ss_l   = l,
		.ss_ul  = ul,
		.ss_ll  = ll,
		.ss_ull = ull,
		.ss_i8  = i8,
		.ss_u8  = u8,
		.ss_i16 = i16,
		.ss_u16 = u16,
		.ss_i32 = i32,
		.ss_u32 = u32,
		.ss_i64 = i64,
		.ss_u64 = u64,
	};

	/* length of structure test */
	ss_serialized_len = simple_struct_serialize(M0_NET_TEST_SERIALIZE, &ss,
						    NULL, 0);
	M0_UT_ASSERT(ss_serialized_len > 0);

	/* simple encode-decode test */
	M0_SET_ARR0(buf);
	rc_bcount = simple_struct_serialize(M0_NET_TEST_SERIALIZE, &ss, &bv, 0);
	M0_UT_ASSERT(rc_bcount == ss_serialized_len);

	M0_SET0(&ss);

	rc_bcount = simple_struct_serialize(M0_NET_TEST_DESERIALIZE, &ss,
					    &bv, 0);
	M0_UT_ASSERT(rc_bcount == ss_serialized_len);
	M0_UT_ASSERT(ss.ss_c   == c);
	M0_UT_ASSERT(ss.ss_uc  == uc);
	M0_UT_ASSERT(ss.ss_s   == s);
	M0_UT_ASSERT(ss.ss_us  == us);
	M0_UT_ASSERT(ss.ss_i   == i);
	M0_UT_ASSERT(ss.ss_ui  == ui);
	M0_UT_ASSERT(ss.ss_l   == l);
	M0_UT_ASSERT(ss.ss_ul  == ul);
	M0_UT_ASSERT(ss.ss_ll  == ll);
	M0_UT_ASSERT(ss.ss_ull == ull);
	M0_UT_ASSERT(ss.ss_i8  == i8);
	M0_UT_ASSERT(ss.ss_u8  == u8);
	M0_UT_ASSERT(ss.ss_i16 == i16);
	M0_UT_ASSERT(ss.ss_u16 == u16);
	M0_UT_ASSERT(ss.ss_i32 == i32);
	M0_UT_ASSERT(ss.ss_u32 == u32);
	M0_UT_ASSERT(ss.ss_i64 == i64);
	M0_UT_ASSERT(ss.ss_u64 == u64);

	/* failure test */
	len = 64;
	rc_bcount = simple_struct_serialize(M0_NET_TEST_SERIALIZE, &ss, &bv, 0);
	M0_UT_ASSERT(rc_bcount == 0);
}

void m0_net_test_serialize_ut(void)
{
	/* zero values test */
	simple_struct_test(0, 0, 0, 0, 0, 0, 0, 0,
			   0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	/* one values test */
	simple_struct_test(1, 1, 1, 1, 1, 1, 1, 1,
			   1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
	/* -1 and 1 values test */
	simple_struct_test(-1, 1, -1, 1, -1, 1, -1, 1,
			   -1, 1, -1, 1, -1, 1, -1, 1, -1, 1);
	/* 42 value test */
	simple_struct_test(42, 42, 42, 42, 42, 42, 42, 42,
			   42, 42, 42, 42, 42, 42, 42, 42, 42, 42);
	/* min values test */
	simple_struct_test(CHAR_MIN, 0, SHRT_MIN, 0,
			   INT_MIN, 0, LONG_MIN, 0,
			   LLONG_MIN, 0, INT8_MIN, 0,
			   INT16_MIN, 0, INT32_MIN, 0, INT64_MIN, 0);
	/* max values test */
	simple_struct_test(CHAR_MAX, UCHAR_MAX, SHRT_MAX, USHRT_MAX,
			   INT_MAX, UINT_MAX, LONG_MAX, ULONG_MAX,
			   LLONG_MAX, ULLONG_MAX, INT8_MAX, UINT8_MAX,
			   INT16_MAX, UINT16_MAX, INT32_MAX, UINT32_MAX,
			   INT64_MAX, UINT64_MAX);
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
