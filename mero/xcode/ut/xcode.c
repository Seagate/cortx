/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 28-Dec-2011
 */

#ifndef __KERNEL__
#include <stdio.h>                          /* printf */
#include <ctype.h>                          /* isspace */
#endif

#include "lib/memory.h"
#include "lib/vec.h"                        /* m0_bufvec */
#include "lib/misc.h"                       /* M0_SET0 */
#include "lib/arith.h"                      /* m0_rnd64 */
#include "lib/errno.h"                      /* ENOENT */
#include "lib/string.h"                     /* m0_streq */
#include "ut/ut.h"

#include "xcode/xcode.h"

struct foo {
	uint64_t f_x;
	uint64_t f_y;
};

typedef uint32_t tdef;

struct un {
	uint32_t u_tag;
	union {
		uint64_t u_x;
		char     u_y;
	} u;
};

enum { N = 6 };

struct ar {
	struct foo a_el[N];
};

struct top {
	struct foo t_foo;
	uint32_t   t_flag;
	struct v {
		uint32_t  v_nr;
		char     *v_data;
	} t_v;
	tdef              t_def;
	struct un         t_un;
	union {
		uint32_t *o_32;
		uint64_t *o_64;
	} t_opaq;
	struct ar  t_ar;
};

enum { CHILDREN_MAX = 16 };

struct static_xt {
	struct m0_xcode_type  xt;
	struct m0_xcode_field field[CHILDREN_MAX];
};

static struct static_xt xut_un = {
	.xt = {
		.xct_aggr   = M0_XA_UNION,
		.xct_name   = "un",
		.xct_sizeof = sizeof (struct un),
		.xct_nr     = 3
	}
};

static struct static_xt xut_tdef = {
	.xt = {
		.xct_aggr   = M0_XA_TYPEDEF,
		.xct_name   = "tdef",
		.xct_sizeof = sizeof (tdef),
		.xct_nr     = 1
	}
};

static struct static_xt xut_v = {
	.xt = {
		.xct_aggr   = M0_XA_SEQUENCE,
		.xct_name   = "v",
		.xct_sizeof = sizeof (struct v),
		.xct_nr     = 2
	}
};

static struct static_xt xut_foo = {
	.xt = {
		.xct_aggr   = M0_XA_RECORD,
		.xct_name   = "foo",
		.xct_sizeof = sizeof (struct foo),
		.xct_nr     = 2
	}
};

static struct static_xt xut_ar = {
	.xt = {
		.xct_aggr   = M0_XA_ARRAY,
		.xct_name   = "ar",
		.xct_sizeof = sizeof (struct ar),
		.xct_nr     = 1
	}
};

static struct static_xt xut_top = {
	.xt = {
		.xct_aggr   = M0_XA_RECORD,
		.xct_name   = "top",
		.xct_sizeof = sizeof (struct top),
		.xct_nr     = 7
	}
};

static char data[] = "Hello, world!\n";

static struct top T = {
	.t_foo  = {
		.f_x = 7,
		.f_y = 8
	},
	.t_flag = 0xF,
	.t_v    = {
		.v_nr   = sizeof data,
		.v_data = data
	},
	.t_un   = {
		.u_tag = 4
	},
	.t_opaq = {
		.o_32 = &T.t_v.v_nr
	},
	.t_ar   = {
		.a_el = {
			[0] = { 0, 1 },
			[1] = { 1, 1 },
			[2] = { 2, 1 },
			[3] = { 3, 1 },
			[4] = { 4, 1 },
			[5] = { 5, 1 }
		}
	}
};

static char                 ebuf[1000];
static m0_bcount_t          count = ARRAY_SIZE(ebuf);
static void                *vec = ebuf;
static struct m0_bufvec     bvec  = M0_BUFVEC_INIT_BUF(&vec, &count);
static struct m0_xcode_ctx  ctx;

static struct tdata {
	struct _foo {
		uint64_t f_x;
		uint64_t f_y;
	} __attribute__((packed)) t_foo;
	uint32_t __attribute__((packed)) t_flag;
	struct _v {
		uint32_t v_nr;
		char     v_data[sizeof data];
	} __attribute__((packed)) t_v;
	uint32_t __attribute__((packed)) t_def;
	struct t_un {
		uint32_t u_tag;
		char     u_y;
	} __attribute__((packed)) t_un;
	uint32_t __attribute__((packed)) t_opaq;
	struct t_ar {
		struct _foo1 {
			uint64_t f_x;
			uint64_t f_y;
		} a_el[N];
	} __attribute__((packed)) t_ar;
} __attribute__((packed)) TD;

M0_BASSERT(sizeof TD < sizeof ebuf);

static int failure;

static int opaq_type(const struct m0_xcode_obj *par,
		     const struct m0_xcode_type **out)
{
	struct top *t = par->xo_ptr;

	M0_UT_ASSERT(par->xo_type == &xut_top.xt);

	if (!failure) {
		*out = t->t_flag == 0xf ? &M0_XT_U32 : &M0_XT_U64;
		return 0;
	} else
		return -ENOENT;
}

static int xcode_init(void)
{
	xut_v.xt.xct_child[0] = (struct m0_xcode_field){
		.xf_name   = "v_nr",
		.xf_type   = &M0_XT_U32,
		.xf_offset = offsetof(struct v, v_nr)
	};
	xut_v.xt.xct_child[1] = (struct m0_xcode_field){
		.xf_name   = "v_data",
		.xf_type   = &M0_XT_U8,
		.xf_offset = offsetof(struct v, v_data)
	};

	xut_foo.xt.xct_child[0] = (struct m0_xcode_field){
		.xf_name   = "f_x",
		.xf_type   = &M0_XT_U64,
		.xf_offset = offsetof(struct foo, f_x)
	};
	xut_foo.xt.xct_child[1] = (struct m0_xcode_field){
		.xf_name   = "f_y",
		.xf_type   = &M0_XT_U64,
		.xf_offset = offsetof(struct foo, f_y)
	};

	xut_top.xt.xct_child[0] = (struct m0_xcode_field){
		.xf_name   = "t_foo",
		.xf_type   = &xut_foo.xt,
		.xf_offset = offsetof(struct top, t_foo)
	};
	xut_top.xt.xct_child[1] = (struct m0_xcode_field){
		.xf_name   = "t_flag",
		.xf_type   = &M0_XT_U32,
		.xf_offset = offsetof(struct top, t_flag)
	};
	xut_top.xt.xct_child[2] = (struct m0_xcode_field){
		.xf_name   = "t_v",
		.xf_type   = &xut_v.xt,
		.xf_offset = offsetof(struct top, t_v)
	};
	xut_top.xt.xct_child[3] = (struct m0_xcode_field){
		.xf_name   = "t_def",
		.xf_type   = &xut_tdef.xt,
		.xf_offset = offsetof(struct top, t_def)
	};
	xut_top.xt.xct_child[4] = (struct m0_xcode_field){
		.xf_name   = "t_un",
		.xf_type   = &xut_un.xt,
		.xf_offset = offsetof(struct top, t_un)
	};
	xut_top.xt.xct_child[5] = (struct m0_xcode_field){
		.xf_name   = "t_opaq",
		.xf_type   = &M0_XT_OPAQUE,
		.xf_opaque = opaq_type,
		.xf_offset = offsetof(struct top, t_opaq)
	};
	xut_top.xt.xct_child[6] = (struct m0_xcode_field){
		.xf_name   = "t_ar",
		.xf_type   = &xut_ar.xt,
		.xf_offset = offsetof(struct top, t_ar)
	};

	xut_tdef.xt.xct_child[0] = (struct m0_xcode_field){
		.xf_name   = "def",
		.xf_type   = &M0_XT_U32,
		.xf_offset = 0
	};

	xut_un.xt.xct_child[0] = (struct m0_xcode_field){
		.xf_name   = "u_tag",
		.xf_type   = &M0_XT_U32,
		.xf_offset = offsetof(struct un, u_tag)
	};
	xut_un.xt.xct_child[1] = (struct m0_xcode_field){
		.xf_name   = "u_x",
		.xf_type   = &M0_XT_U64,
		.xf_tag    = 1,
		.xf_offset = offsetof(struct un, u.u_x)
	};
	xut_un.xt.xct_child[2] = (struct m0_xcode_field){
		.xf_name   = "u_y",
		.xf_type   = &M0_XT_U8,
		.xf_tag    = 4,
		.xf_offset = offsetof(struct un, u.u_y)
	};

	xut_ar.xt.xct_child[0] = (struct m0_xcode_field){
		.xf_name   = "a_el",
		.xf_type   = &xut_foo.xt,
		.xf_offset = offsetof(struct ar, a_el),
		.xf_tag    = N,
	};

	TD.t_foo.f_x  =  T.t_foo.f_x;
	TD.t_foo.f_y  =  T.t_foo.f_y;
	TD.t_flag     =  T.t_flag;
	TD.t_v.v_nr   =  T.t_v.v_nr;
	M0_ASSERT(T.t_v.v_nr == ARRAY_SIZE(TD.t_v.v_data));
	memcpy(TD.t_v.v_data, T.t_v.v_data, T.t_v.v_nr);
	TD.t_def      =  T.t_def;
	TD.t_un.u_tag =  T.t_un.u_tag;
	TD.t_un.u_y   =  T.t_un.u.u_y;
	TD.t_opaq     = *T.t_opaq.o_32;
	memcpy(&TD.t_ar, &T.t_ar, sizeof T.t_ar);
	return 0;
}

#ifndef __KERNEL__
__attribute__((unused)) static void it_print(const struct m0_xcode_cursor *it)
{
	int i;
	const struct m0_xcode_cursor_frame *f;

	for (i = 0, f = &it->xcu_stack[0]; i < it->xcu_depth; ++i, ++f) {
		printf(".%s[%lu]",
		       f->s_obj.xo_type->xct_child[f->s_fieldno].xf_name,
		       f->s_elno);
	}
	printf(":%s ", m0_xcode_aggr_name[f->s_obj.xo_type->xct_aggr]);
	if (f->s_obj.xo_type->xct_aggr == M0_XA_ATOM) {
		switch (f->s_obj.xo_type->xct_atype) {
		case M0_XAT_VOID:
			printf("void");
			break;
		case M0_XAT_U8:
			printf("%c", *(char *)f->s_obj.xo_ptr);
			break;
		case M0_XAT_U32:
			printf("%x", *(uint32_t *)f->s_obj.xo_ptr);
			break;
		case M0_XAT_U64:
			printf("%x", (unsigned)*(uint64_t *)f->s_obj.xo_ptr);
			break;
		default:
			M0_IMPOSSIBLE("atom");
		}
	}
	printf("\n");
}
#endif

static void chk(struct m0_xcode_cursor *it, int depth,
		const struct m0_xcode_type *xt,
		void *addr, int fieldno, int elno,
		enum m0_xcode_cursor_flag flag)
{
	int                           rc;
	struct m0_xcode_obj          *obj;
	struct m0_xcode_cursor_frame *f;

	rc = m0_xcode_next(it);
	M0_UT_ASSERT(rc > 0);

	M0_UT_ASSERT(it->xcu_depth == depth);
	M0_UT_ASSERT(IS_IN_ARRAY(depth, it->xcu_stack));

	f   = m0_xcode_cursor_top(it);
	obj = &f->s_obj;

	M0_UT_ASSERT(obj->xo_type == xt);
	M0_UT_ASSERT(obj->xo_ptr  == addr);
	M0_UT_ASSERT(f->s_fieldno == fieldno);
	M0_UT_ASSERT(f->s_elno    == elno);
	M0_UT_ASSERT(f->s_flag    == flag);
}

static void xcode_cursor_test(void)
{
	int                    i;
	struct m0_xcode_cursor it;

	M0_SET0(&it);

	it.xcu_stack[0].s_obj.xo_type = &xut_top.xt;
	it.xcu_stack[0].s_obj.xo_ptr  = &T;

	chk(&it, 0, &xut_top.xt, &T, 0, 0, M0_XCODE_CURSOR_PRE);
	chk(&it, 1, &xut_foo.xt, &T.t_foo, 0, 0, M0_XCODE_CURSOR_PRE);
	chk(&it, 2, &M0_XT_U64, &T.t_foo.f_x, 0, 0, M0_XCODE_CURSOR_PRE);
	chk(&it, 2, &M0_XT_U64, &T.t_foo.f_x, 0, 0, M0_XCODE_CURSOR_POST);
	chk(&it, 1, &xut_foo.xt, &T.t_foo, 0, 0, M0_XCODE_CURSOR_IN);
	chk(&it, 2, &M0_XT_U64, &T.t_foo.f_y, 0, 0, M0_XCODE_CURSOR_PRE);
	chk(&it, 2, &M0_XT_U64, &T.t_foo.f_y, 0, 0, M0_XCODE_CURSOR_POST);
	chk(&it, 1, &xut_foo.xt, &T.t_foo, 1, 0, M0_XCODE_CURSOR_IN);
	chk(&it, 1, &xut_foo.xt, &T.t_foo, 2, 0, M0_XCODE_CURSOR_POST);
	chk(&it, 0, &xut_top.xt, &T, 0, 0, M0_XCODE_CURSOR_IN);
	chk(&it, 1, &M0_XT_U32, &T.t_flag, 0, 0, M0_XCODE_CURSOR_PRE);
	chk(&it, 1, &M0_XT_U32, &T.t_flag, 0, 0, M0_XCODE_CURSOR_POST);
	chk(&it, 0, &xut_top.xt, &T, 1, 0, M0_XCODE_CURSOR_IN);
	chk(&it, 1, &xut_v.xt, &T.t_v, 0, 0, M0_XCODE_CURSOR_PRE);
	chk(&it, 2, &M0_XT_U32, &T.t_v.v_nr, 0, 0, M0_XCODE_CURSOR_PRE);
	chk(&it, 2, &M0_XT_U32, &T.t_v.v_nr, 0, 0, M0_XCODE_CURSOR_POST);
	chk(&it, 1, &xut_v.xt, &T.t_v, 0, 0, M0_XCODE_CURSOR_IN);
	for (i = 0; i < ARRAY_SIZE(data); ++i) {
		chk(&it, 2, &M0_XT_U8,
		    &T.t_v.v_data[i], 0, 0, M0_XCODE_CURSOR_PRE);
		chk(&it, 2, &M0_XT_U8,
		    &T.t_v.v_data[i], 0, 0, M0_XCODE_CURSOR_POST);
		M0_UT_ASSERT(*(char *)it.xcu_stack[2].s_obj.xo_ptr == data[i]);
		chk(&it, 1, &xut_v.xt, &T.t_v, 1, i, M0_XCODE_CURSOR_IN);
	}
	chk(&it, 1, &xut_v.xt, &T.t_v, 2, 0, M0_XCODE_CURSOR_POST);
	chk(&it, 0, &xut_top.xt, &T, 2, 0, M0_XCODE_CURSOR_IN);
	chk(&it, 1, &xut_tdef.xt, &T.t_def, 0, 0, M0_XCODE_CURSOR_PRE);
	chk(&it, 2, &M0_XT_U32, &T.t_def, 0, 0, M0_XCODE_CURSOR_PRE);
	chk(&it, 2, &M0_XT_U32, &T.t_def, 0, 0, M0_XCODE_CURSOR_POST);
	chk(&it, 1, &xut_tdef.xt, &T.t_def, 0, 0, M0_XCODE_CURSOR_IN);
	chk(&it, 1, &xut_tdef.xt, &T.t_def, 1, 0, M0_XCODE_CURSOR_POST);
	chk(&it, 0, &xut_top.xt, &T, 3, 0, M0_XCODE_CURSOR_IN);
	chk(&it, 1, &xut_un.xt, &T.t_un, 0, 0, M0_XCODE_CURSOR_PRE);
	chk(&it, 2, &M0_XT_U32, &T.t_un.u_tag, 0, 0, M0_XCODE_CURSOR_PRE);
	chk(&it, 2, &M0_XT_U32, &T.t_un.u_tag, 0, 0, M0_XCODE_CURSOR_POST);
	chk(&it, 1, &xut_un.xt, &T.t_un, 0, 0, M0_XCODE_CURSOR_IN);
	chk(&it, 2, &M0_XT_U8, &T.t_un.u.u_y, 0, 0, M0_XCODE_CURSOR_PRE);
	chk(&it, 2, &M0_XT_U8, &T.t_un.u.u_y, 0, 0, M0_XCODE_CURSOR_POST);
	chk(&it, 1, &xut_un.xt, &T.t_un, 2, 0, M0_XCODE_CURSOR_IN);
	chk(&it, 1, &xut_un.xt, &T.t_un, 3, 0, M0_XCODE_CURSOR_POST);
	chk(&it, 0, &xut_top.xt, &T, 4, 0, M0_XCODE_CURSOR_IN);
	chk(&it, 1, &M0_XT_U32, T.t_opaq.o_32, 0, 0, M0_XCODE_CURSOR_PRE);
	chk(&it, 1, &M0_XT_U32, T.t_opaq.o_32, 0, 0, M0_XCODE_CURSOR_POST);
	chk(&it, 0, &xut_top.xt, &T, 5, 0, M0_XCODE_CURSOR_IN);
	chk(&it, 1, &xut_ar.xt, &T.t_ar, 0, 0, M0_XCODE_CURSOR_PRE);
	for (i = 0; i < N; ++i) {
		struct foo *el = &T.t_ar.a_el[i];

		chk(&it, 2, &xut_foo.xt, el, 0, 0, M0_XCODE_CURSOR_PRE);
		chk(&it, 3, &M0_XT_U64, &el->f_x, 0, 0, M0_XCODE_CURSOR_PRE);
		chk(&it, 3, &M0_XT_U64, &el->f_x, 0, 0, M0_XCODE_CURSOR_POST);
		chk(&it, 2, &xut_foo.xt, el, 0, 0, M0_XCODE_CURSOR_IN);
		chk(&it, 3, &M0_XT_U64, &el->f_y, 0, 0, M0_XCODE_CURSOR_PRE);
		chk(&it, 3, &M0_XT_U64, &el->f_y, 0, 0, M0_XCODE_CURSOR_POST);
		chk(&it, 2, &xut_foo.xt, el, 1, 0, M0_XCODE_CURSOR_IN);
		chk(&it, 2, &xut_foo.xt, el, 2, 0, M0_XCODE_CURSOR_POST);
		chk(&it, 1, &xut_ar.xt, &T.t_ar, 0, i, M0_XCODE_CURSOR_IN);
	}
	chk(&it, 1, &xut_ar.xt, &T.t_ar, 2, 0, M0_XCODE_CURSOR_POST);
	chk(&it, 0, &xut_top.xt, &T, 6, 0, M0_XCODE_CURSOR_IN);
	chk(&it, 0, &xut_top.xt, &T, 7, 0, M0_XCODE_CURSOR_POST);

	M0_UT_ASSERT(m0_xcode_next(&it) == 0);
}

static void xcode_length_test(void)
{
	int result;

	m0_xcode_ctx_init(&ctx, &(struct m0_xcode_obj){ &xut_top.xt, &T });
	result = m0_xcode_length(&ctx);
	M0_UT_ASSERT(result == sizeof TD);
}

static void xcode_encode_test(void)
{
	int result;

	m0_xcode_ctx_init(&ctx, &(struct m0_xcode_obj){ &xut_top.xt, &T });
	m0_bufvec_cursor_init(&ctx.xcx_buf, &bvec);
	result = m0_xcode_encode(&ctx);
	M0_UT_ASSERT(result == 0);

	M0_UT_ASSERT(memcmp(&TD, ebuf, sizeof TD) == 0);
}

static void xcode_opaque_test(void)
{
	int result;

	failure = 1;
	m0_xcode_ctx_init(&ctx, &(struct m0_xcode_obj){ &xut_top.xt, &T });
	result = m0_xcode_length(&ctx);
	M0_UT_ASSERT(result == -ENOENT);
	failure = 0;
}

static void decode(struct m0_xcode_obj *obj)
{
	int result;

	m0_xcode_ctx_init(&ctx, &(struct m0_xcode_obj){ &xut_top.xt, NULL });
	ctx.xcx_alloc = m0_xcode_alloc;
	m0_bufvec_cursor_init(&ctx.xcx_buf, &bvec);

	result = m0_xcode_decode(&ctx);
	M0_UT_ASSERT(result == 0);

	*obj = ctx.xcx_it.xcu_stack[0].s_obj;
}

static void xcode_decode_test(void)
{
	struct m0_xcode_obj decoded;
	struct top *TT;

	decode(&decoded);
	TT = decoded.xo_ptr;
	M0_UT_ASSERT( TT != NULL);
	M0_UT_ASSERT( TT->t_foo.f_x    ==  T.t_foo.f_x);
	M0_UT_ASSERT( TT->t_foo.f_y    ==  T.t_foo.f_y);
	M0_UT_ASSERT( TT->t_flag       ==  T.t_flag);
	M0_UT_ASSERT( TT->t_v.v_nr     ==  T.t_v.v_nr);
	M0_UT_ASSERT(memcmp(TT->t_v.v_data, T.t_v.v_data, T.t_v.v_nr) == 0);
	M0_UT_ASSERT( TT->t_def        ==  T.t_def);
	M0_UT_ASSERT( TT->t_un.u_tag   ==  T.t_un.u_tag);
	M0_UT_ASSERT( TT->t_un.u.u_y   ==  T.t_un.u.u_y);
	M0_UT_ASSERT(*TT->t_opaq.o_32  == *T.t_opaq.o_32);
	M0_UT_ASSERT(memcmp(TT->t_ar.a_el, T.t_ar.a_el, sizeof T.t_ar) == 0);

	m0_xcode_free_obj(&decoded);
}

enum {
	FSIZE = sizeof(uint64_t) + sizeof(uint64_t)
};

static char             foo_buf[FSIZE];
static void            *foo_addr  = foo_buf;
static m0_bcount_t      foo_count = ARRAY_SIZE(foo_buf);
static struct m0_bufvec foo_bvec  = M0_BUFVEC_INIT_BUF(&foo_addr, &foo_count);

static int foo_length(struct m0_xcode_ctx *ctx, const void *obj)
{
	return ARRAY_SIZE(foo_buf);
}

static void foo_xor(char *buf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(foo_buf); ++i)
		buf[i] ^= 42;
}

static int foo_encode(struct m0_xcode_ctx *ctx, const void *obj)
{
	struct m0_bufvec_cursor cur;

	m0_bufvec_cursor_init(&cur, &foo_bvec);
	memcpy(foo_buf, obj, sizeof(struct foo));
	foo_xor(foo_buf);
	return m0_bufvec_cursor_copy(&ctx->xcx_buf, &cur, FSIZE) != FSIZE ?
		-EPROTO : 0;
}

static int foo_decode(struct m0_xcode_ctx *ctx, void *obj)
{
	struct m0_bufvec_cursor cur;

	m0_bufvec_cursor_init(&cur, &foo_bvec);
	if (m0_bufvec_cursor_copy(&cur, &ctx->xcx_buf, FSIZE) == FSIZE) {
		foo_xor(foo_buf);
		memcpy(obj, foo_buf, sizeof(struct foo));
		return 0;
	} else
		return -EPROTO;
}

static const struct m0_xcode_type_ops foo_ops = {
	.xto_length = foo_length,
	.xto_encode = foo_encode,
	.xto_decode = foo_decode
};

static void xcode_nonstandard_test(void)
{
	int result;
	int i;

	xut_foo.xt.xct_ops = &foo_ops;

	m0_xcode_ctx_init(&ctx, &(struct m0_xcode_obj){ &xut_top.xt, &T });
	result = m0_xcode_length(&ctx);
	M0_UT_ASSERT(result == sizeof TD);

	m0_xcode_ctx_init(&ctx, &(struct m0_xcode_obj){ &xut_top.xt, &T });
	m0_bufvec_cursor_init(&ctx.xcx_buf, &bvec);
	result = m0_xcode_encode(&ctx);
	M0_UT_ASSERT(result == 0);

	foo_xor(ebuf);
	for (i = 0; i < N; ++i)
		foo_xor((void *)&((struct tdata *)ebuf)->t_ar.a_el[i]);
	M0_UT_ASSERT(memcmp(&TD, ebuf, sizeof TD) == 0);
	foo_xor(ebuf);
	for (i = 0; i < N; ++i)
		foo_xor((void *)&((struct tdata *)ebuf)->t_ar.a_el[i]);
	xcode_decode_test();
	xut_foo.xt.xct_ops = NULL;
}

static void xcode_cmp_test(void)
{
	struct m0_xcode_obj obj0;
	struct m0_xcode_obj obj1;
	struct top *t0;
	struct top *t1;
	int    cmp;

	xcode_encode_test();

	decode(&obj0);
	decode(&obj1);

	t0 = obj0.xo_ptr;
	t1 = obj1.xo_ptr;

	cmp = m0_xcode_cmp(&obj0, &obj0);
	M0_UT_ASSERT(cmp == 0);

	cmp = m0_xcode_cmp(&obj0, &obj1);
	M0_UT_ASSERT(cmp == 0);

	cmp = m0_xcode_cmp(&obj1, &obj0);
	M0_UT_ASSERT(cmp == 0);

	t1->t_foo.f_x--;
	cmp = m0_xcode_cmp(&obj0, &obj1);
	M0_UT_ASSERT(cmp > 0);
	cmp = m0_xcode_cmp(&obj1, &obj0);
	M0_UT_ASSERT(cmp < 0);

	t1->t_foo.f_x++;
	cmp = m0_xcode_cmp(&obj0, &obj1);
	M0_UT_ASSERT(cmp == 0);

	t1->t_v.v_data[0] = 'J';
	cmp = m0_xcode_cmp(&obj0, &obj1);
	M0_UT_ASSERT(cmp < 0);
	t1->t_v.v_data[0] = t0->t_v.v_data[0];

	t1->t_v.v_nr++;
	cmp = m0_xcode_cmp(&obj0, &obj1);
	M0_UT_ASSERT(cmp < 0);
	cmp = m0_xcode_cmp(&obj1, &obj0);
	M0_UT_ASSERT(cmp > 0);
	t1->t_v.v_nr--;

	m0_xcode_free_obj(&obj0);
	m0_xcode_free_obj(&obj1);
}

static int custom_read(const struct m0_xcode_cursor *it,
		       struct m0_xcode_obj *obj, const char *str)
{
	static const char pattern[] = "!EXPECTED!";

	M0_UT_ASSERT(obj->xo_type == &xut_v.xt);
	return strncmp(str, pattern, ARRAY_SIZE(pattern) - 1) == 0 ?
		ARRAY_SIZE(pattern) - 1 : -EPERM;
}

static const struct m0_xcode_type_ops read_ops = {
	.xto_read = &custom_read
};

#define OBJ(xt, ptr) (&(struct m0_xcode_obj){ .xo_type = (xt), .xo_ptr = (ptr) })

static void literal(const char *input, const char *output)
{
	struct v            *V;
	struct m0_xcode_obj *obj;
	char                *buf;
	int                  result;

	output = output ?: input;
	buf = m0_alloc(strlen(input) + 1);
	M0_UT_ASSERT(buf != NULL);
	M0_ALLOC_PTR(V);
	obj = OBJ(&xut_v.xt, V);
	result = m0_xcode_read(obj, input);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(V->v_nr == strlen(output) - 2);
	M0_UT_ASSERT(strncmp(V->v_data, output + 1, V->v_nr) == 0);
	result = m0_xcode_print(obj, buf, strlen(input) + 1);
	M0_UT_ASSERT(result == strlen(output));
	M0_UT_ASSERT(strcmp(output, buf) == 0);
	m0_free(buf);
	m0_xcode_free_obj(obj);
}

static void xcode_read_test(void)
{
	int         result;
	int         i;
	struct foo  F;
	struct un   U;
	struct v   *V;
	struct ar   A;
	struct top *_T;
	struct top  _Tmp;

	M0_SET0(&F);
	result = m0_xcode_read(OBJ(&xut_foo.xt, &F), "(10, 0xff)");
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(F.f_x == 10);
	M0_UT_ASSERT(F.f_y == 0xff);

	M0_SET0(&F);
	result = m0_xcode_read(OBJ(&xut_foo.xt, &F), " ( 10 , 0xff ) ");
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(F.f_x == 10);
	M0_UT_ASSERT(F.f_y == 0xff);

	M0_SET0(&F);
	result = m0_xcode_read(OBJ(&xut_foo.xt, &F), "(10,010)");
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(F.f_x == 10);
	M0_UT_ASSERT(F.f_y == 8);

	M0_SET0(&F);
	result = m0_xcode_read(OBJ(&xut_foo.xt, &F), " ( 10 , 0xff ) rest");
	M0_UT_ASSERT(result == -EINVAL);

	M0_SET0(&F);
	result = m0_xcode_read(OBJ(&xut_foo.xt, &F), "(10,)");
	M0_UT_ASSERT(result == -EPROTO);

	M0_SET0(&F);
	result = m0_xcode_read(OBJ(&xut_foo.xt, &F), "(10 12)");
	M0_UT_ASSERT(result == -EPROTO);

	M0_SET0(&F);
	result = m0_xcode_read(OBJ(&xut_foo.xt, &F), "()");
	M0_UT_ASSERT(result == -EPROTO);

	M0_SET0(&F);
	result = m0_xcode_read(OBJ(&xut_foo.xt, &F), "");
	M0_UT_ASSERT(result == -EPROTO);

	M0_SET0(&U);
	result = m0_xcode_read(OBJ(&xut_un.xt, &U), "{1| 42}");
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(U.u_tag == 1);
	M0_UT_ASSERT(U.u.u_x == 42);

	M0_SET0(&U);
	result = m0_xcode_read(OBJ(&xut_un.xt, &U), "{4| 8}");
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(U.u_tag == 4);
	M0_UT_ASSERT(U.u.u_y == 8);

	M0_SET0(&U);
	result = m0_xcode_read(OBJ(&xut_un.xt, &U), "{3| 0}");
	M0_UT_ASSERT(result == -EPROTO);

	M0_SET0(&U);
	result = m0_xcode_read(OBJ(&xut_un.xt, &U), "{3}");
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(U.u_tag == 3);

	M0_ALLOC_PTR(V);
	result = m0_xcode_read(OBJ(&xut_v.xt, V), "[0]");
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(V->v_nr == 0);
	m0_xcode_free_obj(OBJ(&xut_v.xt, V));

	M0_ALLOC_PTR(V);
	result = m0_xcode_read(OBJ(&xut_v.xt, V), "[1: 42]");
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(V->v_nr == 1);
	M0_UT_ASSERT(V->v_data[0] == 42);
	m0_xcode_free_obj(OBJ(&xut_v.xt, V));

	M0_ALLOC_PTR(V);
	result = m0_xcode_read(OBJ(&xut_v.xt, V), "[3: 42, 43, 44]");
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(V->v_nr == 3);
	M0_UT_ASSERT(V->v_data[0] == 42);
	M0_UT_ASSERT(V->v_data[1] == 43);
	M0_UT_ASSERT(V->v_data[2] == 44);
	m0_xcode_free_obj(OBJ(&xut_v.xt, V));

	M0_SET0(&A);
	result = m0_xcode_read(OBJ(&xut_ar.xt, &A), "<>");
	M0_UT_ASSERT(result == -EPROTO);

	M0_SET0(&A);
	result = m0_xcode_read(OBJ(&xut_ar.xt, &A), "<6>");
	M0_UT_ASSERT(result == -EPROTO);

	M0_SET0(&A);
	result = m0_xcode_read(OBJ(&xut_ar.xt, &A), "<(2, 2)>");
	M0_UT_ASSERT(result == -EPROTO);

#define ARS "<(0, 0x1), (0, 0x1), (0, 0x1), (0, 0x1), (0, 0x1), (0, 0x1)>"
	M0_SET0(&A);
	result = m0_xcode_read(OBJ(&xut_ar.xt, &A), ARS);
	M0_UT_ASSERT(result == 0);


/* MERO-1396 <- */
	literal("\"/dev/disk/by-id/wwn-0x5000c50078c12486\"", NULL);
	literal("[0x15:0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,"
		"0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30]",
		"\"000000000000000000000\"");
	literal("[0x16:0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,"
		"0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30]",
		"\"0000000000000000000000\"");
/* -!> */
	literal("\"a\"", NULL);
	literal("\"abcdef\"", NULL);
	literal("\"\"", NULL);

	M0_ALLOC_PTR(V);
	result = m0_xcode_read(OBJ(&xut_v.xt, V), "\"");
	M0_UT_ASSERT(result == -EPROTO);
	m0_xcode_free_obj(OBJ(&xut_v.xt, V));

	M0_ALLOC_PTR(_T);
	result = m0_xcode_read(OBJ(&xut_top.xt, _T),
			 "((1, 2), 8, [4: 1, 2, 3, 4], 4, {1| 42}, 7," ARS ")");
	M0_UT_ASSERT(result == 0);
	M0_SET0(&_Tmp);
	_Tmp.t_foo.f_x = 1;
	_Tmp.t_foo.f_y = 2;
	_Tmp.t_flag = 8;
	_Tmp.t_v.v_nr = 4;
	_Tmp.t_v.v_data = _T->t_v.v_data;
	_Tmp.t_def = 4;
	_Tmp.t_un.u_tag = 1;
	_Tmp.t_un.u.u_x = 42;
	_Tmp.t_opaq.o_32 = _T->t_opaq.o_32;
	for (i = 0; i < N; ++i) {
		_Tmp.t_ar.a_el[i].f_x = 0;
		_Tmp.t_ar.a_el[i].f_y = 1;
	}
	M0_UT_ASSERT(memcmp(_T, &_Tmp, sizeof(struct top)) == 0);
	m0_xcode_free_obj(OBJ(&xut_top.xt, _T));

	/* Test custom reader. */
	M0_ALLOC_PTR(_T);
	xut_v.xt.xct_ops = &read_ops;
	result = m0_xcode_read(OBJ(&xut_top.xt, _T),
			    "((1, 2), 8, ^!EXPECTED!, 4, {1| 42}, 7," ARS ")");
	M0_UT_ASSERT(result == 0);
	m0_xcode_free_obj(OBJ(&xut_top.xt, _T));

	M0_ALLOC_PTR(_T);
	result = m0_xcode_read(OBJ(&xut_top.xt, _T), "((1, 2), 8, ^WRONG ...");
	M0_UT_ASSERT(result == -EPERM);
	m0_xcode_free_obj(OBJ(&xut_top.xt, _T));

	M0_ALLOC_PTR(_T);
	result = m0_xcode_read(OBJ(&xut_top.xt, _T),
			"((1, 2), 8, [4: 1, 2, 3, 4], 4, {1| 42}, 7," ARS ")");
	M0_UT_ASSERT(result == 0);
	m0_xcode_free_obj(OBJ(&xut_top.xt, _T));
	xut_v.xt.xct_ops = NULL;
}

#ifndef __KERNEL__
static void xcode_print_test(void)
{
	char buf[300];
	const char *s0;
	const char *s1;
	int         rc;
	char        data[] = { 1, 2, 3, 4 };
	uint64_t    o64    = 7;
	struct top  T = (struct top){
		.t_foo  = { 1, 2 },
		.t_flag = 8,
		.t_v    = { .v_nr = 4, .v_data = data },
		.t_def  = 4,
		.t_un   = { .u_tag = 1, .u = { .u_x = 42 }},
		.t_opaq = { .o_64 = &o64 },
		.t_ar   = {
			.a_el = {
				{ 0, 1 },
				{ 0, 1 },
				{ 0, 1 },
				{ 0, 1 },
				{ 0, 1 },
				{ 0, 1 }
			}
		}
	};
	struct top *V;

	rc = m0_xcode_print(OBJ(&xut_top.xt, &T), buf, ARRAY_SIZE(buf));
	M0_UT_ASSERT(rc == strlen(buf));
	for (s0 = buf, s1 = ""
		     "((0x1, 0x2),"
		     " 0x8,"
		     " [0x4: 0x1, 0x2, 0x3, 0x4],"
		     " 0x4,"
		     " {0x1| 0x2a},"
		     " 0x7, " ARS ")";
	     *s0 != 0 && *s1 != 0; ++s0, ++s1) {
		while (isspace(*s0))
			++s0;
		while (isspace(*s1))
			++s1;
		M0_UT_ASSERT(*s0 == *s1);
	}
	M0_UT_ASSERT(*s0 == 0);
	M0_UT_ASSERT(*s1 == 0);

	/* Read back printed obj */
	M0_ALLOC_PTR(V);
	rc = m0_xcode_read(OBJ(&xut_top.xt, V), buf);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_xcode_cmp(
			OBJ(&xut_top.xt, V), OBJ(&xut_top.xt, &T)) == 0);
	m0_xcode_free_obj(OBJ(&xut_top.xt, V));

	rc = m0_xcode_print(OBJ(&xut_top.xt, &T), NULL, 0);
	M0_UT_ASSERT(rc == strlen(buf));
}
#endif

static void xcode_find_test(void)
{
	struct m0_xcode_obj top = { &xut_top.xt, &T };
	void               *place;
	int                 result;

	result = m0_xcode_find(&top, &xut_top.xt, &place);
	M0_UT_ASSERT(result == 0 && place == &T);

	result = m0_xcode_find(&top, &xut_foo.xt, &place);
	M0_UT_ASSERT(result == 0 && place == &T.t_foo);

	result = m0_xcode_find(&top, &xut_v.xt, &place);
	M0_UT_ASSERT(result == 0 && place == &T.t_v);

	result = m0_xcode_find(&top, &M0_XT_U64, &place);
	M0_UT_ASSERT(result == 0 && place == &T.t_foo.f_x);

	result = m0_xcode_find(&top, &M0_XT_U32, &place);
	M0_UT_ASSERT(result == 0 && place == &T.t_flag);

	result = m0_xcode_find(&top, &M0_XT_VOID, &place);
	M0_UT_ASSERT(result == -ENOENT);
}

#define __ENUM_ONLY
#include "test_gccxml_simple.h"
#include "test_gccxml_simple_xc.h"

static void xcode_enum_gccxml(void)
{
	M0_UT_ASSERT(m0_streq(m0_xc_testenum_enum.xe_name, "testenum"));
	M0_UT_ASSERT(m0_xc_testenum_enum.xe_nr == 4);
	M0_UT_ASSERT(m0_xc_testenum_enum.xe_maxlen == 5);
	M0_UT_ASSERT(m0_xc_testenum_enum.xe_val[1].xev_val == 1);
	M0_UT_ASSERT(m0_streq(m0_xc_testenum_enum.xe_val[1].xev_name, "TE_1"));

	M0_UT_ASSERT(m0_streq(m0_xc_testbitmask_enum.xe_name, "testbitmask"));
	M0_UT_ASSERT(m0_xc_testbitmask_enum.xe_nr == 5);
	M0_UT_ASSERT(m0_xc_testbitmask_enum.xe_val[1].xev_val == M0_BITS(6));
	M0_UT_ASSERT(m0_streq(m0_xc_testbitmask_enum.xe_val[1].xev_name,
			      "BM_SIX"));
}

static void xcode_enum_print(void)
{
	char buf[30];
#define CHECK(v)							\
m0_streq(m0_xcode_enum_print(&m0_xc_testenum_enum, v, NULL), #v)
	M0_UT_ASSERT(CHECK(TE_0));
	M0_UT_ASSERT(CHECK(TE_1));
	M0_UT_ASSERT(CHECK(TE_33));
	M0_UT_ASSERT(CHECK(TE_5));
#undef CHECK
	M0_UT_ASSERT(strncmp(m0_xcode_enum_print(&m0_xc_testenum_enum,
						 77, NULL),
			     "Invalid", 7) == 0);
	M0_UT_ASSERT(strncmp(m0_xcode_enum_print(&m0_xc_testenum_enum,
						 0x4d, buf),
			     "4d", 2) == 0);
}

static void enum_read_check(const char *name, int nr, uint64_t v, int rc)
{
	uint64_t read = 0;

	M0_UT_ASSERT(m0_xcode_enum_read(&m0_xc_testenum_enum,
					name, nr, &read) == rc);
	M0_UT_ASSERT(ergo(rc == 0, read == v));
}

static void xcode_enum_read(void)
{
#define C(v) (enum_read_check(#v, strlen(#v), v, 0))
	C(TE_0);
	C(TE_1);
	C(TE_33);
	C(TE_5);
#undef C
	enum_read_check("4d", 2, 0x4d, 0);
	enum_read_check("4d ", 3, 0, -EPROTO);
	enum_read_check("4dZ", 3, 0, -EPROTO);
	enum_read_check("", 0, 0, 0);
	enum_read_check("TE_", 3, 0, -EPROTO);
	enum_read_check("TE_02", 4, 0, 0);
}

static void bitmask_print_check(uint64_t mask,
				int nr, bool ok, const char *out)
{
	char buf[256] = {};
	int  result;

	M0_UT_ASSERT(nr < ARRAY_SIZE(buf));
	result = m0_xcode_bitmask_print(&m0_xc_testbitmask_enum,
					mask, buf, nr);
	M0_UT_ASSERT(ergo(!ok, result > nr));
	M0_UT_ASSERT(ergo(ok, m0_streq(buf, out)));
}

static void xcode_bitmask_print(void)
{
	const struct test {
		uint64_t    t_mask;
		int         t_nr;
		bool        t_ok;
		const char *t_out;
	} test[] = {
	{ 0,                           1,       true,                    "" },
	{ BM_ZERO,                    10,       true,             "BM_ZERO" },
	{  BM_SIX,                    10,       true,              "BM_SIX" },
	{ BM_FOUR,                    10,       true,             "BM_FOUR" },
	{ BM_NINE,                    10,       true,             "BM_NINE" },
	{ BM_FIVE,                    10,       true,             "BM_FIVE" },
	{ BM_ZERO|BM_FOUR,            20,       true,     "BM_ZERO|BM_FOUR" },
	{ BM_FOUR|BM_ZERO,            20,       true,     "BM_ZERO|BM_FOUR" },
	{ BM_ZERO|BM_FOUR,            10,      false,                    "" },
	{ 1024,                       10,       true,                 "400" },
	{ 1024|BM_ZERO,               20,       true,         "BM_ZERO|400" },

	};
	int i;

	for (i = 0; i < ARRAY_SIZE(test); ++i) {
		const struct test *t = &test[i];

		bitmask_print_check(t->t_mask, t->t_nr, t->t_ok, t->t_out);
	}
}

static void bitmask_read_check(uint64_t mask, int nr, bool ok, const char *buf)
{
	uint64_t val;
	int      result;

	if (nr == -1)
		nr = strlen(buf);
	result = m0_xcode_bitmask_read(&m0_xc_testbitmask_enum, buf, nr, &val);
	M0_UT_ASSERT(ok == (result == 0));
	M0_UT_ASSERT(!ok == (result == -EPROTO));
	M0_UT_ASSERT(ergo(ok, val == mask));
}

static void xcode_bitmask_read(void)
{
	const struct test {
		uint64_t    t_mask;
		int         t_nr;
		bool        t_ok;
		const char *t_out;
	} test[] = {
	{ 0,                           0,   true,                        "" },
	{ BM_ZERO,                    -1,   true,                 "BM_ZERO" },
	{  BM_SIX,                    -1,   true,                  "BM_SIX" },
	{ BM_FOUR,                    -1,   true,                 "BM_FOUR" },
	{ BM_NINE,                    -1,   true,                 "BM_NINE" },
	{ BM_FIVE,                    -1,   true,                 "BM_FIVE" },
	{ BM_ZERO|BM_FOUR,            -1,   true,         "BM_ZERO|BM_FOUR" },
	{ BM_FOUR|BM_ZERO,            -1,   true,         "BM_FOUR|BM_ZERO" },
	{ BM_FOUR|BM_ZERO|BM_NINE,    -1,   true, "BM_FOUR|BM_NINE|BM_ZERO" },
	{ BM_FOUR|BM_NINE|0x400,      -1,   true,     "BM_FOUR|BM_NINE|400" },
	{ 0,                          -1,  false,    "BM_FOUR|BM_NINE|400Z" },
	{ 0,                          -1,  false,       "BM_FOUR|BM_NINE|Z" },
	{ 0,                          -1,  false,       "ZZZZZZZZZZZZZZZZZ" },
	{ BM_FOUR|BM_NINE|0x400,      -1,   true,    "BM_FOUR|BM_NINE|400|" },
	{ 0,                          -1,  false,       "BM_FOUR|BM_NINE||" },
	{ BM_FOUR|BM_NINE,            -1,   true,        "BM_FOUR|BM_NINE|" },
	{ 0,                          -1,  false,      "|BM_FOUR|BM_NINE||" },
	{ 0,                          -1,  false,                     "|||" },
	{ 0,                          -1,  false,                      "||" },
	{ 0,                          -1,  false,                       "|" },
	{ 0,                          -1,  false,                      "|0" },
	{ 0,                          -1,  false,                   "|0|0|" },
	{ 0,                          -1,  false,                     "|0|" },
	{ 0,                          -1,   true,                     "0|0" },
	{ 1,                          -1,   true,                      "1|" },
	{ BM_FOUR|BM_NINE|0x4400,     -1,   true,"BM_FOUR|4000|BM_NINE|400" },

	};
	int i;

	for (i = 0; i < ARRAY_SIZE(test); ++i) {
		const struct test *t = &test[i];

		bitmask_read_check(t->t_mask, t->t_nr, t->t_ok, t->t_out);
	}
}

static void xcode_enum_loop(void)
{
	char     buf[300];
	int      i;
	uint64_t val;
	uint64_t val1;
	uint64_t seed = (uint64_t)buf;
	int      result;

	for (i = 0; i < 2000; ++i) {
		const char *out;
		val = m0_rnd64(&seed);

		out = m0_xcode_enum_print(&m0_xc_testenum_enum, val, buf);
		result = m0_xcode_enum_read(&m0_xc_testenum_enum,
					    out, strlen(out), &val1);
		M0_UT_ASSERT(result == 0);
		M0_UT_ASSERT(val == val1);
		result = m0_xcode_bitmask_print(&m0_xc_testbitmask_enum,
					     val, buf, sizeof buf);
		M0_UT_ASSERT(result > 0);
		result = m0_xcode_bitmask_read(&m0_xc_testbitmask_enum,
					       buf, strlen(buf), &val1);
		M0_UT_ASSERT(result == 0);
		M0_UT_ASSERT(val == val1);
	}
}

static void xcode_enum_field(void)
{
	struct enumfield ef = {
		.ef_0    = 1,
		.ef_enum = TE_5,
		.ef_bitm = BM_ZERO|BM_NINE,
		.ef_1    = 2
	};
	struct enumfield    buf;
	struct m0_xcode_obj obj = {
		.xo_type = enumfield_xc,
		.xo_ptr  = &buf
	};
	int result;

	/* Initialise enumfield_xc. */
	m0_xc_xcode_ut_test_gccxml_simple_init();

	result = m0_xcode_read(&obj, "(1, @TE_5, @BM_ZERO|BM_NINE, 2)");
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(memcmp(&buf, &ef, sizeof ef) == 0);
}

static int ecount;
static void t_count(struct m0_xcode_type *xt, void *data)
{
	++ecount;
}

static void f_count(struct m0_xcode_type *xt,
		    struct m0_xcode_field *field, void *data)
{
	++ecount;
}

static void xcode_iterate(void)
{
	ecount = 0;
	m0_xcode_type_iterate(&xut_top.xt, &t_count, NULL, NULL);
	/* top + foo + uint32_t + v + tdef + un + opaq + ar +
	   foo:uint64_t + foo:uint64_t + v_nr + v_data + tdef:uint32_t +
	   un:u_tag + un:u_x + un:u_y + ar:e_al + foo:uint64_t +
	   foo:uint64_t */
	M0_UT_ASSERT(ecount == 19);
	ecount = 0;
	m0_xcode_type_iterate(&xut_top.xt, NULL, &f_count, NULL);
	/* Same as above sans top. */
	M0_UT_ASSERT(ecount == 18);
}

static void flagset(struct m0_xcode_type *xt, uint32_t flags)
{
	xt->xct_flags = flags;
}

struct pair {
	const struct m0_xcode_type *p_src;
	      struct m0_xcode_type  p_dst;
} builtins[] = {
	{ &M0_XT_U8,     {} },
	{ &M0_XT_U32,    {} },
	{ &M0_XT_U64,    {} },
	{ &M0_XT_OPAQUE, {} }
};

static void fieldset(struct m0_xcode_type *xt,
		     struct m0_xcode_field *field,
		     void *unused)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(builtins); ++i) {
		if (field->xf_type == builtins[i].p_src) {
			builtins[i].p_dst = *builtins[i].p_src;
			field->xf_type = &builtins[i].p_dst;
			break;
		}
	}
}

static void fieldclear(struct m0_xcode_type *xt,
		       struct m0_xcode_field *field,
		       void *unused)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(builtins); ++i) {
		if (field->xf_type == &builtins[i].p_dst) {
			field->xf_type = builtins[i].p_src;
			break;
		}
	}
}

static void xcode_flags(void)
{
	/*
	 * First, go through type tree and replace all built-in types (which
	 * are const objects) with local replicas.
	 */
	m0_xcode_type_iterate(&xut_top.xt, NULL, &fieldset, (void *)0);

	M0_UT_ASSERT(!m0_xcode_type_flags(&xut_top.xt, 1, 0, 0));
	M0_UT_ASSERT( m0_xcode_type_flags(&xut_top.xt, 0, 0, 0));
	M0_UT_ASSERT( m0_xcode_type_flags(&xut_top.xt, 0, 0xffffffff, 0));
	xut_top.xt.xct_flags = 1;
	M0_UT_ASSERT(!m0_xcode_type_flags(&xut_top.xt, 0, 0xffffffff, 0));
	M0_UT_ASSERT(!m0_xcode_type_flags(&xut_top.xt, 0, 1, 0));
	M0_UT_ASSERT( m0_xcode_type_flags(&xut_top.xt, 0, 0, 0));
	xut_top.xt.xct_flags = 0;
	xut_foo.xt.xct_flags = 1;
	M0_UT_ASSERT(!m0_xcode_type_flags(&xut_top.xt, 0, 0xffffffff, 0));
	M0_UT_ASSERT(!m0_xcode_type_flags(&xut_top.xt, 0, 1, 0));
	M0_UT_ASSERT( m0_xcode_type_flags(&xut_top.xt, 0, 0, 0));
	xut_foo.xt.xct_flags = 0;
	m0_xcode_type_iterate(&xut_top.xt, (void *)&flagset, NULL, (void *)5);
	M0_UT_ASSERT( m0_xcode_type_flags(&xut_top.xt, 1, 0, 0));
	M0_UT_ASSERT( m0_xcode_type_flags(&xut_top.xt, 1, 2, 0));
	M0_UT_ASSERT( m0_xcode_type_flags(&xut_top.xt, 4, 2, 0));
	m0_xcode_type_iterate(&xut_top.xt, (void *)&flagset, NULL, (void *)0);
	m0_xcode_type_iterate(&xut_top.xt, NULL, &fieldclear, (void *)0);
}

/*
 * Stub function, it's not meant to be used anywhere, it's defined to calm down
 * linker, which throws an "undefined reference to `m0_package_cred_get'"
 * otherwise.
 */
int m0_package_cred_get(const struct m0_xcode_obj *par,
			const struct m0_xcode_type **out)
{
	return 0;
}

struct m0_ut_suite xcode_ut = {
	.ts_name = "xcode-ut",
	.ts_init = xcode_init,
	.ts_fini = NULL,
	.ts_tests = {
		{ "xcode-cursor", xcode_cursor_test },
		{ "xcode-length", xcode_length_test },
		{ "xcode-encode", xcode_encode_test },
		{ "xcode-opaque", xcode_opaque_test },
		{ "xcode-decode", xcode_decode_test },
		{ "xcode-nonstandard", xcode_nonstandard_test },
		{ "xcode-cmp",    xcode_cmp_test },
		{ "xcode-read",   xcode_read_test },
#ifndef __KERNEL__
		{ "xcode-print",  xcode_print_test },
#endif
		{ "xcode-find",   xcode_find_test },

		{ "xcode-enum-gccxml",    xcode_enum_gccxml,       "Nikita" },
		{ "xcode-enum-print",     xcode_enum_print,        "Nikita" },
		{ "xcode-enum-read",      xcode_enum_read,         "Nikita" },
		{ "xcode-bitmask-print",  xcode_bitmask_print,     "Nikita" },
		{ "xcode-bitmask-read",   xcode_bitmask_read,      "Nikita" },
		{ "xcode-enum-loop",      xcode_enum_loop,         "Nikita" },
		{ "xcode-enum-field",     xcode_enum_field,        "Nikita" },
		{ "xcode-iterate",        xcode_iterate,           "Nikita" },
		{ "xcode-flags",          xcode_flags,             "Nikita" },
		{ NULL, NULL }
	}
};
M0_EXPORTED(xcode_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
