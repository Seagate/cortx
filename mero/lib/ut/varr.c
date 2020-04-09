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
 * Original author: Anand Vidwansa <anand_vidwansa@xyratex.com>
 * Original creation date: 01/07/2013
 */

#include "lib/vec.h"
#include "lib/memory.h"
#include "lib/misc.h"		/* M0SET0() */
#include "lib/arith.h"		/* m0_rnd() */
#include "lib/ub.h"
#include "ut/ut.h"
#include "lib/time.h"
#include "lib/varr.h"
#include "lib/varr_private.h"   /* varr_cache */
#include "lib/misc.h"		/* M0_BITS() */
#include "lib/finject.h"	/* M0_FI_ENABLED */
#ifndef __KERNEL__
#include <limits.h>		/* CHAR_BIT */
#else
#include <linux/limits.h>
#endif

enum data_types {
	DT_ATOMIC_8,
	DT_ATOMIC_64,
	DT_POWTWO,
	DT_NON_POWTWO,
};

enum sizes_and_shifts {
	BUFF_SIZE           = 32,
	BUFF_SHIFT          = 5,
	DT_ATOMIC_8_SHIFT   = 0,
	DT_ATOMIC_64_SHIFT  = 3,
	DT_POWTWO_SHIFT     = 4,
	DT_NON_POWTWO_SHIFT = BUFF_SHIFT,
};

M0_BASSERT(sizeof (uint64_t) == M0_BITS(DT_ATOMIC_64_SHIFT));
M0_BASSERT(sizeof (uint8_t) == M0_BITS(DT_ATOMIC_8_SHIFT));

enum misc_params {
/* Following expression has been used to compute the maximum allowable tree
 * depth under given constraints. In actual computation, SYSTEM_MEMORY has been
 * assumed to be 1GB.
   max_depth = log(SYSTEM_MEMORY *
		(BUFF_SIZE - M0_VA_TNODEPTR_SIZE)/
		(BUFF_SIZE * M0_VA_TNODEPTR_SIZE*M0_VA_TNODE_NR) + 1))/
		log (BUFF_SIZE/M0_VA_TNODEPTR_SIZE) + 1;
*/
	MAX_DEPTH      = 11,
	MAX_OBJ_NR     = 26000,
	MAX_TEST_DEPTH = 6,
	MAX_BUFFERS    = 4096,
};

M0_BASSERT(BUFF_SIZE == M0_BITS(BUFF_SHIFT));
M0_BASSERT((int)M0_0VEC_ALIGN > (int)BUFF_SIZE);
M0_BASSERT(!(M0_0VEC_ALIGN & (M0_0VEC_ALIGN - 1)));

/* sizeof struct po2 is an integer power of two. */
struct po2 {
	uint64_t p_x;
	uint64_t p_y;
};
M0_BASSERT(sizeof (struct po2) == M0_BITS(DT_POWTWO_SHIFT));
/* sizeof struct non_po2 is not an integer power of two. */
struct non_po2 {
	uint16_t np_chksum;
	uint8_t  np_arr[15];
};
/* Test the case when object size and buffer size are same. */
M0_BASSERT(sizeof (struct non_po2) < BUFF_SIZE &&
	   sizeof (struct non_po2) > M0_BITS(BUFF_SHIFT - 1));

/* Iterates over arrays with various sizes of objects of type 'dt'. */
#define test_iterate(varr, ele, dt, buff_nr)			        \
({								        \
	 struct m0_varr *__varr_   = (varr);			        \
	 typeof(ele)     __ele;					        \
	 uint64_t	 __buff_nr = (buff_nr);			        \
	 uint64_t        __nr;					        \
	 int		 __rc;					        \
	 uint64_t	 __step;				        \
	 uint32_t        __dt      = dt;			        \
								        \
	 M0_ASSERT(size_get(__dt) == sizeof __ele);		        \
	 M0_SET0(__varr_);					        \
	 __nr = array_len_compute(__buff_nr, __dt);		        \
	 __rc = m0_varr_init(__varr_, __nr, size_get(__dt), BUFF_SIZE); \
	 M0_ASSERT(__rc == 0);					        \
	 m0_varr_iter(__varr_, typeof(__ele), i, obj, 0,	        \
		      m0_varr_size(__varr_), 1) {		        \
		obj_init((void*)obj, i, __dt);			        \
	 } m0_varr_enditer;					        \
	 m0_varr_for (__varr_, typeof(__ele), i, obj) {		        \
		obj_sanity_check(obj, i, __dt);			        \
	 } m0_varr_endfor;					        \
	 for (__step = 2; __step <= __nr;	++__step) {		\
		 m0_varr_iter(__varr_, typeof(__ele), i, obj, 0,        \
			      m0_varr_size(__varr_), __step) {          \
			 obj_sanity_check((void *)obj, i, __dt);	\
		 } m0_varr_enditer;				        \
	 }							        \
	 m0_varr_fini(__varr_);				                \
})

/* Tests init and fini APIs for m0_varr. */
static void test_init(void);
/* Tests sanity of object and buffer sizes that get stored within
 * m0_varr. */
static void test_size(void);
/* Tests tree construction for complete trees of various depths, maximum
 * depth being max_depth. */
static void test_depth(uint32_t max_depth);
/* Tests contents of cache present in m0_varr. */
static void test_cache(void);
/* Iterates over array, for various input objects. */
static void test_ut_iterate(uint64_t nr);
static void obj_init(void *obj, uint64_t data, enum data_types dt);
static void obj_sanity_check(const void *obj, uint64_t data,
			     enum data_types dt);
static size_t size_get(enum data_types dt);
static uint16_t int_summation(uint8_t n);
uint64_t array_len_compute(uint64_t buff_nr, enum data_types dt);
uint32_t shift_get(enum data_types dt);
static void tree_sanity_check(const struct m0_varr *varr, uint32_t depth);

void test_varr(void)
{
	test_init();
	test_size();
	test_depth(MAX_TEST_DEPTH);
	test_cache();
	test_ut_iterate(MAX_OBJ_NR);
}

static void test_init(void)
{
	struct m0_varr varr;
	int	       rc;
	uint64_t       n;
	m0_time_t      seed;

	M0_SET0(&varr);
	rc = m0_varr_init(&varr, M0_VA_TNODE_NR, size_get(DT_POWTWO),
			  BUFF_SIZE);
	M0_UT_ASSERT(rc == 0);
	m0_varr_fini(&varr);

	/* Test fault injection. */
	seed = m0_time_now();
	n = m0_rnd(MAX_BUFFERS, &seed);
	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", n, 1);
	M0_SET0(&varr);
	rc = m0_varr_init(&varr, MAX_OBJ_NR, size_get(DT_POWTWO),
			  BUFF_SIZE);
	if (rc == 0)
		m0_varr_fini(&varr);
	m0_fi_disable("m0_alloc", "fail_allocation");
}

static void test_size(void)
{
	struct m0_varr  varr;
	int		rc;
	enum data_types dt;
	size_t		obj_size;

	for (dt = DT_ATOMIC_8; dt <= DT_NON_POWTWO; ++dt) {
		obj_size = size_get(dt);
		M0_SET0(&varr);
		/* Note that input buffer size has been deliberately given as
		 * non power of two */
		rc = m0_varr_init(&varr, M0_VA_TNODE_NR, obj_size,
				  M0_0VEC_ALIGN - 1);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(!(varr.va_obj_size & (varr.va_obj_size - 1)) &&
			     obj_size <= varr.va_obj_size	          &&
			     2 * obj_size > varr.va_obj_size);
		M0_UT_ASSERT(varr.va_bufsize == M0_0VEC_ALIGN);
		tree_sanity_check(&varr, 2);
		m0_varr_fini(&varr);
	}
}

static size_t size_get(enum data_types dt)
{
	switch (dt) {
	case DT_ATOMIC_8:
		return sizeof (uint8_t);
	case DT_ATOMIC_64:
		return sizeof (uint64_t);
	case DT_POWTWO:
		return sizeof (struct po2);
	case DT_NON_POWTWO:
		return sizeof (struct non_po2);
	}
	return 0;

}

static void tree_sanity_check(const struct m0_varr *varr, uint32_t depth)
{
	uint32_t num_trees;
	uint64_t buff_nr;
	uint64_t num_accomodated;

	for (num_trees = 0; num_trees < M0_VA_TNODE_NR &&
		varr->va_tree[num_trees] != NULL; ++num_trees)
		/* do nothing */;

	/* Maximum possible leaf buffers for a given depth. */
	buff_nr = M0_BITS(varr->va_bufptr_nr_shift * (varr->va_depth - 2));
	buff_nr *= num_trees;
	M0_UT_ASSERT(varr->va_buff_nr <= buff_nr);
	/* Maximum number of objects that can accomodate in a given number of
	 * leaf-buffers. */
	num_accomodated = M0_BITS(varr->va_buf_shift - varr->va_obj_shift) *
		varr->va_buff_nr;
	M0_UT_ASSERT(num_accomodated >= varr->va_nr);
	M0_UT_ASSERT(num_accomodated - varr->va_nr <
		     M0_BITS(varr->va_buf_shift - varr->va_obj_shift));
	M0_UT_ASSERT(varr->va_depth == depth);
}

static void test_depth(uint32_t max_depth)
{
	struct   m0_varr varr;
	uint64_t nr;
	int      rc;
	uint32_t depth = 2;
	uint64_t buff_nr;

	M0_SET0(&varr);
	M0_UT_ASSERT(max_depth > 1);

	/* Test complete trees with various depths, maximum depth being
	 * max_depth. */
	for (buff_nr = M0_VA_TNODE_NR; depth <= max_depth;
	     buff_nr *= (BUFF_SIZE/M0_VA_TNODEPTR_SIZE), ++depth) {
		nr = buff_nr * M0_BITS(BUFF_SHIFT - DT_POWTWO_SHIFT);
		M0_SET0(&varr);
		rc = m0_varr_init(&varr, nr,
				  size_get(DT_POWTWO), BUFF_SIZE);
		M0_UT_ASSERT(rc == 0);
		tree_sanity_check(&varr, depth);
		m0_varr_fini(&varr);
	}
}

static void test_cache(void)
{
	struct m0_varr varr;
	uint32_t       i;
	uint32_t       obj_per_buff;
	int	       rc;
	void	      *arr_ele;

	M0_SET0(&varr);
	rc = m0_varr_init(&varr, MAX_OBJ_NR, size_get(DT_POWTWO), BUFF_SIZE);
	M0_UT_ASSERT(rc == 0);
	tree_sanity_check(&varr, 6);
	obj_per_buff = varr.va_bufsize/ varr.va_obj_size;
	for (i = 0; i < m0_varr_size(&varr); ++i) {
		arr_ele = m0_varr_ele_get(&varr, i);
		M0_UT_ASSERT(arr_ele == (void *)varr.va_cache->vc_buff +
			     (i % obj_per_buff) * varr.va_obj_size);
	}
	m0_varr_fini(&varr);
}

static uint16_t int_summation(uint8_t n)
{
	return (n + 1) * n / 2;
}

void  test_ut_iterate(uint64_t buff_nr)
{
	struct m0_varr varr;
	struct po2     obj_po2;
	struct non_po2 obj_non_po2;
	uint8_t        atomic8_obj;
	uint64_t       atomic64_obj;

	test_iterate(&varr, obj_po2,      DT_POWTWO,     buff_nr);
	test_iterate(&varr, obj_non_po2,  DT_NON_POWTWO, buff_nr);
	test_iterate(&varr, atomic8_obj,  DT_ATOMIC_8,   buff_nr);
	test_iterate(&varr, atomic64_obj, DT_ATOMIC_64,  buff_nr);
}

void test_ub_iterate(void)
{
	uint32_t  depth = 2;
	uint64_t  buff_nr;
	m0_time_t seed;
	int	  i;

	/* Testing for various leaf buffers. */
	for (i = 0; i < 100; ++i) {
		seed = m0_time_now();
		buff_nr = m0_rnd(MAX_BUFFERS, &seed);
		test_ut_iterate(buff_nr);
	}

	/* Testing complete tree.  */
	for (buff_nr = M0_VA_TNODE_NR; depth <= MAX_DEPTH - 1;
	     buff_nr *= (BUFF_SIZE/M0_VA_TNODEPTR_SIZE), ++depth) {
		test_ut_iterate(buff_nr);
	}
}

uint64_t array_len_compute(uint64_t buff_nr, enum data_types dt)
{
	uint32_t shift = shift_get(dt);

	return buff_nr * M0_BITS(BUFF_SHIFT - shift);
}

uint32_t shift_get(enum data_types dt)
{
	switch (dt) {
	case DT_ATOMIC_8:
		return DT_ATOMIC_8_SHIFT;
	case DT_ATOMIC_64:
		return DT_ATOMIC_64_SHIFT;
	case DT_POWTWO:
		return DT_POWTWO_SHIFT;
	case DT_NON_POWTWO:
		return DT_NON_POWTWO_SHIFT;
	}
	return 0;
}

static void obj_init(void *obj, uint64_t data, enum data_types dt)
{
	int	        i;
	struct po2     *obj_po2;
	struct non_po2 *obj_non_po2;

	switch (dt) {
	case DT_ATOMIC_8:
		*(uint8_t *)obj = data % UINT8_MAX;
		break;
	case DT_ATOMIC_64:
		*(uint64_t *)obj = data;
		break;
	case DT_POWTWO:
		obj_po2 = (struct po2 *)obj;
		obj_po2->p_x = data;
		obj_po2->p_y = data;
		break;
	case DT_NON_POWTWO:
		obj_non_po2 = (struct non_po2 *)obj;
		obj_non_po2->np_chksum = 0;
		for (i = 0; i < ARRAY_SIZE(obj_non_po2->np_arr);
		     ++i) {
			obj_non_po2->np_arr[i]  = (data % UINT8_MAX) * i;
			obj_non_po2->np_chksum += (data % UINT8_MAX) * i;
		}
		break;
	}
}

static void obj_sanity_check(const void *obj, uint64_t data,
			     enum data_types dt)
{
	struct po2     obj_po2;
	struct non_po2 obj_non_po2;

	switch (dt) {
	case DT_ATOMIC_8:
		M0_ASSERT( *(uint8_t *)obj == data % UINT8_MAX);
		break;
	case DT_ATOMIC_64:
		M0_ASSERT( *(uint64_t *)obj == data);
		break;
	case DT_POWTWO:
		obj_po2 = *(struct po2 *)obj;
		M0_ASSERT(obj_po2.p_x == data);
		M0_ASSERT(obj_po2.p_y == data);
		break;
	case DT_NON_POWTWO:
		obj_non_po2 = *(struct non_po2 *)obj;
		M0_ASSERT(obj_non_po2.np_chksum == (data % UINT8_MAX)*
			  int_summation(ARRAY_SIZE(obj_non_po2.np_arr) -
						   1));
	}
}
enum {
	UB_ITER = 1,
};

static void varr_ub(int i)
{
	test_ub_iterate();
}

struct m0_ub_set m0_varr_ub = {
	.us_name = "varr-ub",
	.us_init = NULL,
	.us_fini = NULL,
	.us_run  = {
		{ .ub_name = "varr",
		  .ub_iter = UB_ITER,
		  .ub_round = varr_ub },
		{.ub_name = NULL }
	}
};
