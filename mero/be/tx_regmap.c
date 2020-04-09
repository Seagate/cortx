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
 * Original creation date: 17-Jun-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/tx_regmap.h"

#include "be/tx.h"
#include "be/io.h"

#include "lib/ext.h"    /* m0_ext */
#include "lib/errno.h"  /* ENOMEM */
#include "lib/memory.h" /* m0_alloc_nz */
#include "lib/assert.h" /* M0_POST */
#include "lib/misc.h"   /* M0_SET0 */
#include "lib/arith.h"  /* max_check */

/**
 * @addtogroup be
 *
 * @{
 */

/** @note don't forget to undefine this at the end of the file */
#define REGD_EXT(rd) (struct m0_ext) {                                      \
	.e_start = (m0_bindex_t)(rd)->rd_reg.br_addr,                       \
	.e_end   = (m0_bindex_t)(rd)->rd_reg.br_addr + (rd)->rd_reg.br_size \
}

M0_INTERNAL bool m0_be_reg_d__invariant(const struct m0_be_reg_d *rd)
{
	const struct m0_be_reg *reg = &rd->rd_reg;

	/**
	 * m0_be_reg__invariant() can't be used here because it checks
	 * m0_be_reg segment, and UT can test m0_be_reg_d-related
	 * structures without m0_be_seg initialization.
	 */
	return _0C(reg->br_addr != NULL) && _0C(reg->br_size > 0);
}

M0_INTERNAL bool m0_be_reg_d_is_in(const struct m0_be_reg_d *rd, void *ptr)
{
	M0_CASSERT(sizeof(m0_bindex_t) >= sizeof(ptr));
	return m0_ext_is_in(&REGD_EXT(rd), (m0_bindex_t) ptr);
}

static bool be_reg_d_are_overlapping(const struct m0_be_reg_d *rd1,
				     const struct m0_be_reg_d *rd2)
{
	return m0_ext_are_overlapping(&REGD_EXT(rd1), &REGD_EXT(rd2));
}

static bool be_reg_d_is_partof(const struct m0_be_reg_d *super,
			       const struct m0_be_reg_d *sub)
{
	return m0_ext_is_partof(&REGD_EXT(super), &REGD_EXT(sub));
}

/** Return address of the first byte inside the region. */
static void *be_reg_d_fb(const struct m0_be_reg_d *rd)
{
	return rd->rd_reg.br_addr;
}

/** Return address of the byte before be_reg_d_fb(rd). */
static void *be_reg_d_fb1(const struct m0_be_reg_d *rd)
{
	return (void *) ((uintptr_t) be_reg_d_fb(rd) - 1);
}

/** Return address of the last byte inside the region. */
static void *be_reg_d_lb(const struct m0_be_reg_d *rd)
{
	return rd->rd_reg.br_addr + rd->rd_reg.br_size - 1;
}

/** Return address of the byte after be_reg_d_lb(rd). */
static void *be_reg_d_lb1(const struct m0_be_reg_d *rd)
{
	return (void *) ((uintptr_t) be_reg_d_lb(rd) + 1);
}

static m0_bcount_t be_reg_d_size(const struct m0_be_reg_d *rd)
{
	return rd->rd_reg.br_size;
}

static void be_reg_d_sub_make(struct m0_be_reg_d *super,
                              struct m0_be_reg_d *sub)
{
	M0_ASSERT(be_reg_d_is_partof(super, sub));

	/* sub->rd_reg.br_addr and sub->rd_reg.br_size are already set */
	sub->rd_reg.br_seg = super->rd_reg.br_seg;
	sub->rd_buf = super->rd_buf + (be_reg_d_fb(sub) - be_reg_d_fb(super));
	sub->rd_gen_idx = super->rd_gen_idx;
}

static void be_reg_d_arr_insert2(void *arr[2], void *value)
{
	M0_PRE(arr[0] != NULL && arr[1] != NULL);

	if (value < arr[0]) {
		arr[1] = arr[0];
		arr[0] = value;
	} else if (arr[0] < value && value < arr[1]) {
		arr[1] = value;
	}
}

/*
 * The function returns the first left-most non-crossing sub-region from the
 * given @rd_arr array of (possibly crossing) regions which starts from @start
 * (if its value is within that sub-region).
 *
 * Example:
 *
 * rd_arr has 2 regions: R1 = (1, 5) and R2 = (4, 7) (regions (addr, size)).
 * Given regions:
 *  | R1 |
 *  +--+-+----+
 *     |  R2  |
 *  The function gives the following region depending on start:
 *  +-------+----------------+
 *  | start | (*addr, *size) |
 *  +-------+----------------+
 *  | 0 or 1|    (1, 3)      |
 *  |   2   |    (2, 2)      |
 *  |   3   |    (3, 1)      |
 *  |   4   |    (4, 2)      |
 *  |   5   |    (5, 1)      |
 *  +-------+----------------+
 *
 *  @param[in]  rd_arr array of reg_d
 *  @param[in]  nr     number of elements in rd_arr
 *  @param[in]  start  result region addr can't be less than start.
 *                     start address must be not greater than the end of
 *                     left-most region in the given @rd_arr[]
 *  @param[out] addr   result region addr
 *  @param[out] size   result region size
 */
static void be_reg_d_arr_first_subreg(struct m0_be_reg_d **rd_arr,
                                      int                  nr,
                                      void                *start,
                                      void               **addr,
                                      m0_bcount_t         *size)
{
	struct m0_be_reg_d *rd;
	void               *arr[2];
	void               *fb;
	void               *lb1;
	int                 i;

	arr[0] = NULL;
	arr[1] = NULL;
	for (i = 0; i < nr; ++i) {
		rd = rd_arr[i];
		if (rd != NULL) {
			fb  = be_reg_d_fb(rd);
			lb1 = be_reg_d_lb1(rd);
			fb = max_check(fb, start);
			M0_ASSERT(fb < lb1);
			if (arr[0] == NULL && arr[1] == NULL) {
				arr[0] = fb;
				arr[1] = lb1;
			} else {
				be_reg_d_arr_insert2(arr, fb);
				be_reg_d_arr_insert2(arr, lb1);
			}
		}
	}
	M0_ASSERT(arr[0] != NULL && arr[1] != NULL);
	M0_ASSERT(arr[0] != arr[1]);
	*addr = arr[0];
	*size = arr[1] - arr[0];
}

static bool be_rdt_contains(const struct m0_be_reg_d_tree *rdt,
			    const struct m0_be_reg_d      *rd)
{
	return &rdt->brt_r[0] <= rd && rd < &rdt->brt_r[rdt->brt_size];
}

#define ARRAY_ALLOC_NZ(arr, nr) ((arr) = m0_alloc_nz((nr) * sizeof ((arr)[0])))

M0_INTERNAL int m0_be_rdt_init(struct m0_be_reg_d_tree *rdt, size_t size_max)
{
	rdt->brt_size_max = size_max;
	ARRAY_ALLOC_NZ(rdt->brt_r, rdt->brt_size_max);
	if (rdt->brt_r == NULL)
		return M0_ERR(-ENOMEM);

	M0_POST(m0_be_rdt__invariant(rdt));
	return 0;
}

M0_INTERNAL void m0_be_rdt_fini(struct m0_be_reg_d_tree *rdt)
{
	M0_PRE(m0_be_rdt__invariant(rdt));
	m0_free(rdt->brt_r);
}

M0_INTERNAL bool m0_be_rdt__invariant(const struct m0_be_reg_d_tree *rdt)
{
	return _0C(rdt != NULL) &&
	       _0C(rdt->brt_r != NULL || rdt->brt_size == 0) &&
	       _0C(rdt->brt_size <= rdt->brt_size_max) &&
	       M0_CHECK_EX(m0_forall(i, rdt->brt_size,
				     m0_be_reg_d__invariant(&rdt->brt_r[i]))) &&
	       M0_CHECK_EX(m0_forall(i,
				     rdt->brt_size == 0 ? 0 : rdt->brt_size - 1,
				     _0C(rdt->brt_r[i].rd_reg.br_addr <
					 rdt->brt_r[i + 1].rd_reg.br_addr))) &&
	       M0_CHECK_EX(m0_forall(i,
				     rdt->brt_size == 0 ? 0 : rdt->brt_size - 1,
		     _0C(!be_reg_d_are_overlapping(&rdt->brt_r[i],
						   &rdt->brt_r[i + 1]))));
}

M0_INTERNAL size_t m0_be_rdt_size(const struct m0_be_reg_d_tree *rdt)
{
	return rdt->brt_size;
}

/** Time complexity is O(1) */
static bool be_rdt_check_i(const struct m0_be_reg_d_tree *rdt,
			   void                          *addr,
			   size_t                         index)
{
	struct m0_be_reg_d *r    = rdt->brt_r;
	size_t              size = m0_be_rdt_size(rdt);

	return  (size == 0 && index == 0) ||
		(size != 0 &&
		 ((index == size && !m0_be_reg_d_is_in(&r[size - 1], addr) &&
		   addr > r[size - 1].rd_reg.br_addr) ||
		  (index == 0 && (m0_be_reg_d_is_in(&r[0], addr) ||
				  addr < r[0].rd_reg.br_addr)) ||
		  (index < size && m0_be_reg_d_is_in(&r[index], addr)) ||
		  (index < size && index > 0 &&
		   !m0_be_reg_d_is_in(&r[index - 1], addr) &&
		   addr < r[index].rd_reg.br_addr &&
		   addr > r[index - 1].rd_reg.br_addr)));
}

/** Time complexity is O(log(m0_be_rdt_size(rdt) + 1)) */
static size_t be_rdt_find_i(const struct m0_be_reg_d_tree *rdt, void *addr)
{
	size_t begin = 0;
	size_t mid;
	size_t end   = m0_be_rdt_size(rdt);
	size_t res   = m0_be_rdt_size(rdt) + 1;
	size_t size  = m0_be_rdt_size(rdt);

	while (begin + 1 < end) {
		mid = (begin + end) / 2;
		M0_ASSERT_INFO(mid > begin && mid < end,
			       "begin = %zu, mid = %zu, end = %zu",
			       begin, mid, end);
		if (rdt->brt_r[mid].rd_reg.br_addr < addr) {
			begin = mid;
		} else {
			end   = mid;
		}
	}
	res = be_rdt_check_i(rdt, addr, begin) ? begin : res;
	res = be_rdt_check_i(rdt, addr, end)   ? end   : res;

	M0_ASSERT_INFO(size == 0 || res <= size, "size = %zu, res = %zu",
		       size, res);
	M0_POST(m0_be_rdt__invariant(rdt));
	M0_POST(be_rdt_check_i(rdt, addr, res));
	return res;
}


M0_INTERNAL struct m0_be_reg_d *
m0_be_rdt_find(const struct m0_be_reg_d_tree *rdt, void *addr)
{
	struct m0_be_reg_d *rd;
	size_t		    i;

	M0_PRE(m0_be_rdt__invariant(rdt));

	i = be_rdt_find_i(rdt, addr);
	rd = i == rdt->brt_size ? NULL : &rdt->brt_r[i];

	M0_POST(ergo(rd != NULL, be_rdt_contains(rdt, rd)));
	return rd;
}

M0_INTERNAL struct m0_be_reg_d *
m0_be_rdt_next(const struct m0_be_reg_d_tree *rdt, struct m0_be_reg_d *prev)
{
	struct m0_be_reg_d *rd;

	M0_PRE(m0_be_rdt__invariant(rdt));
	M0_PRE(prev != NULL);
	M0_PRE(be_rdt_contains(rdt, prev));

	rd = prev == &rdt->brt_r[rdt->brt_size - 1] ? NULL : ++prev;

	M0_POST(ergo(rd != NULL, be_rdt_contains(rdt, rd)));
	return rd;
}

M0_INTERNAL void m0_be_rdt_ins(struct m0_be_reg_d_tree  *rdt,
			       const struct m0_be_reg_d *rd)
{
	size_t index;
	size_t i;

	M0_PRE(m0_be_rdt__invariant(rdt));
	M0_PRE(m0_be_rdt_size(rdt) < rdt->brt_size_max);
	M0_PRE(rd->rd_reg.br_size > 0);

	index = be_rdt_find_i(rdt, be_reg_d_fb(rd));
	++rdt->brt_size;
	for (i = rdt->brt_size - 1; i > index; --i)
		rdt->brt_r[i] = rdt->brt_r[i - 1];
	rdt->brt_r[index] = *rd;

	M0_POST(m0_be_rdt__invariant(rdt));
}

M0_INTERNAL struct m0_be_reg_d *m0_be_rdt_del(struct m0_be_reg_d_tree  *rdt,
					      const struct m0_be_reg_d *rd)
{
	size_t index;
	size_t i;

	M0_PRE(m0_be_rdt__invariant(rdt));
	M0_PRE(m0_be_rdt_size(rdt) > 0);

	index = be_rdt_find_i(rdt, be_reg_d_fb(rd));
	M0_ASSERT(m0_be_reg_eq(&rdt->brt_r[index].rd_reg, &rd->rd_reg));

	for (i = index; i + 1 < rdt->brt_size; ++i)
		rdt->brt_r[i] = rdt->brt_r[i + 1];
	--rdt->brt_size;

	M0_POST(m0_be_rdt__invariant(rdt));
	return index == m0_be_rdt_size(rdt) ? NULL : &rdt->brt_r[index];
}

M0_INTERNAL void m0_be_rdt_reset(struct m0_be_reg_d_tree *rdt)
{
	M0_PRE(m0_be_rdt__invariant(rdt));

	rdt->brt_size = 0;

	M0_POST(m0_be_rdt_size(rdt) == 0);
	M0_POST(m0_be_rdt__invariant(rdt));
}

M0_INTERNAL int
m0_be_regmap_init(struct m0_be_regmap           *rm,
                  const struct m0_be_regmap_ops *ops,
                  void                          *ops_data,
                  size_t                         size_max,
                  bool                           split_on_absorb)
{
	int rc;

	rc = m0_be_rdt_init(&rm->br_rdt, size_max);
	rm->br_ops = ops;
	rm->br_ops_data = ops_data;
	rm->br_split_on_absorb = split_on_absorb;

	M0_POST(ergo(rc == 0, m0_be_regmap__invariant(rm)));
	return M0_RC(rc);
}

M0_INTERNAL void m0_be_regmap_fini(struct m0_be_regmap *rm)
{
	M0_PRE(m0_be_regmap__invariant(rm));
	m0_be_rdt_fini(&rm->br_rdt);
}

M0_INTERNAL bool m0_be_regmap__invariant(const struct m0_be_regmap *rm)
{
	return _0C(rm != NULL) && m0_be_rdt__invariant(&rm->br_rdt);
}

static struct m0_be_reg_d *be_regmap_find_fb(struct m0_be_regmap *rm,
					     const struct m0_be_reg_d *rd)
{
	return m0_be_rdt_find(&rm->br_rdt, be_reg_d_fb(rd));
}

static void be_regmap_reg_d_cut(struct m0_be_regmap *rm,
				struct m0_be_reg_d *rd,
				m0_bcount_t cut_start,
				m0_bcount_t cut_end)
{
	struct m0_be_reg *r = &rd->rd_reg;

	M0_PRE(m0_be_reg_d__invariant(rd));
	M0_PRE(rd->rd_reg.br_size > cut_start + cut_end);

	rm->br_ops->rmo_cut(rm->br_ops_data, rd, cut_start, cut_end);

	r->br_size -= cut_start;
	r->br_addr += cut_start;

	r->br_size -= cut_end;

	M0_POST(m0_be_reg_d__invariant(rd));
}

/* rdi is splitted in 2 parts (if possible). rd is inserted between them. */
static void be_regmap_reg_d_split(struct m0_be_regmap *rm,
                                  struct m0_be_reg_d  *rdi,
                                  struct m0_be_reg_d  *rd,
                                  struct m0_be_reg_d  *rd_new)
{
	/*
	 * |                rdi                  |
	 * +---------------+------+--------------+
	 * | rdi_before_rd |  rd  | rdi_after_rd |
	 */
	m0_bcount_t rdi_before_rd = be_reg_d_fb(rd)   - be_reg_d_fb(rdi);
	m0_bcount_t rdi_after_rd  = be_reg_d_lb1(rdi) - be_reg_d_lb1(rd);

	M0_ASSERT(_0C(rdi_before_rd > 0) && _0C(rdi_after_rd > 0));

	rd_new->rd_reg = M0_BE_REG(NULL, rdi_after_rd, be_reg_d_lb1(rd));
	be_regmap_reg_d_cut(rm, rdi, 0, rdi_after_rd);
	/*
	 * |         rdi           |    |      rdi      | rd_new |
	 * +-------+-------+-------+ -> +-------+-------+--------+
	 * |       |  rd   |       |    |       |  rd   |        |
	 */
	rm->br_ops->rmo_split(rm->br_ops_data, rdi, rd_new);
}

M0_INTERNAL void m0_be_regmap_add(struct m0_be_regmap *rm,
				  struct m0_be_reg_d  *rd)
{
	struct m0_be_reg_d *rdi;
	struct m0_be_reg_d  rd_copy;
	struct m0_be_reg_d  rd_new;
	bool                copied = false;

	M0_PRE(m0_be_regmap__invariant(rm));
	M0_PRE(rd != NULL);
	M0_PRE(m0_be_reg_d__invariant(rd));

	rdi = be_regmap_find_fb(rm, rd);
	if (rdi != NULL &&
	    be_reg_d_fb(rdi) < be_reg_d_fb(rd) &&
	    be_reg_d_lb(rdi) > be_reg_d_lb(rd)) {
		/* old region completely absorbs the new */
		if (rm->br_split_on_absorb) {
			be_regmap_reg_d_split(rm, rdi, rd, &rd_new);
			m0_be_rdt_ins(&rm->br_rdt, &rd_new);
		} else {
			rm->br_ops->rmo_cpy(rm->br_ops_data, rdi, rd);
			copied = true;
		}
	}
	if (!copied) {
		m0_be_regmap_del(rm, rd);
		rd_copy = *rd;
		rm->br_ops->rmo_add(rm->br_ops_data, &rd_copy);
		m0_be_rdt_ins(&rm->br_rdt, &rd_copy);
	}

	M0_POST(m0_be_regmap__invariant(rm));
}

M0_INTERNAL void m0_be_regmap_del(struct m0_be_regmap      *rm,
				  const struct m0_be_reg_d *rd)
{
	struct m0_be_reg_d *rdi;
	m0_bcount_t         cut;

	M0_PRE(m0_be_regmap__invariant(rm));
	M0_PRE(rd != NULL);
	M0_PRE(m0_be_reg_d__invariant(rd));

	/* first intersection */
	rdi = be_regmap_find_fb(rm, rd);
	if (rdi != NULL && m0_be_reg_d_is_in(rdi, be_reg_d_fb(rd)) &&
	    !m0_be_reg_d_is_in(rdi, be_reg_d_lb1(rd)) &&
	    !be_reg_d_is_partof(rd, rdi)) {
		cut = be_reg_d_size(rdi);
		cut -= be_reg_d_fb(rd) - be_reg_d_fb(rdi);
		be_regmap_reg_d_cut(rm, rdi, 0, cut);
		rdi = m0_be_rdt_next(&rm->br_rdt, rdi);
	}
	/* delete all completely covered regions */
	while (rdi != NULL && be_reg_d_is_partof(rd, rdi)) {
		rm->br_ops->rmo_del(rm->br_ops_data, rdi);
		rdi = m0_be_rdt_del(&rm->br_rdt, rdi);
	}
	/* last intersection */
	if (rdi != NULL && !m0_be_reg_d_is_in(rdi, be_reg_d_fb1(rd)) &&
	    m0_be_reg_d_is_in(rdi, be_reg_d_lb(rd))) {
		cut = be_reg_d_size(rdi);
		cut -= be_reg_d_lb(rdi) - be_reg_d_lb(rd);
		be_regmap_reg_d_cut(rm, rdi, cut, 0);
	}
}

M0_INTERNAL struct m0_be_reg_d *m0_be_regmap_first(struct m0_be_regmap *rm)
{
	return m0_be_rdt_find(&rm->br_rdt, NULL);
}

M0_INTERNAL struct m0_be_reg_d *m0_be_regmap_next(struct m0_be_regmap *rm,
						  struct m0_be_reg_d  *prev)
{
	return m0_be_rdt_next(&rm->br_rdt, prev);
}

M0_INTERNAL size_t m0_be_regmap_size(const struct m0_be_regmap *rm)
{
	M0_PRE(m0_be_regmap__invariant(rm));
	return m0_be_rdt_size(&rm->br_rdt);
}

M0_INTERNAL void m0_be_regmap_reset(struct m0_be_regmap *rm)
{
	m0_be_rdt_reset(&rm->br_rdt);
}

#undef REGD_EXT

static const struct m0_be_regmap_ops be_reg_area_ops_data_copy;
static const struct m0_be_regmap_ops be_reg_area_ops_data_nocopy;

static const struct m0_be_regmap_ops *be_reg_area_ops[] = {
	[M0_BE_REG_AREA_DATA_COPY] = &be_reg_area_ops_data_copy,
	[M0_BE_REG_AREA_DATA_NOCOPY] = &be_reg_area_ops_data_nocopy,
};

M0_INTERNAL int m0_be_reg_area_init(struct m0_be_reg_area        *ra,
				    const struct m0_be_tx_credit *prepared,
                                    enum m0_be_reg_area_type      type)
{
	int rc;

	M0_PRE(M0_IN(type, (M0_BE_REG_AREA_DATA_COPY,
	                    M0_BE_REG_AREA_DATA_NOCOPY)));

	*ra = (struct m0_be_reg_area){
		.bra_type     = type,
		.bra_prepared = *prepared,
	};

	/*
	 * ra->bra_prepared.tc_reg_nr is multiplied by 2 because
	 * it's possible to have number of used regions greater than
	 * a number of captured regions due to generation index accounting.
	 */
	rc = m0_be_regmap_init(&ra->bra_map, be_reg_area_ops[type], ra,
			       ra->bra_prepared.tc_reg_nr == 0 ? 0 :
			       ra->bra_prepared.tc_reg_nr * 2 - 1, true);
	if (rc == 0 && type == M0_BE_REG_AREA_DATA_COPY) {
		ARRAY_ALLOC_NZ(ra->bra_area, ra->bra_prepared.tc_reg_size);
		if (ra->bra_area == NULL) {
			m0_be_regmap_fini(&ra->bra_map);
			rc = -ENOMEM;
		}
	}

	/*
	 * Invariant should work even if m0_be_reg_area_init()
	 * was not successful.
	 */
	if (rc != 0)
		M0_SET0(ra);

	M0_POST(ergo(rc == 0, m0_be_reg_area__invariant(ra)));
	return M0_RC(rc);
}

#undef ARRAY_ALLOC_NZ

M0_INTERNAL void m0_be_reg_area_fini(struct m0_be_reg_area *ra)
{
	M0_PRE(m0_be_reg_area__invariant(ra));
	m0_be_regmap_fini(&ra->bra_map);
	m0_free(ra->bra_area);
}

M0_INTERNAL bool m0_be_reg_area__invariant(const struct m0_be_reg_area *ra)
{
	return m0_be_regmap__invariant(&ra->bra_map) &&
	       _0C(ra->bra_area_used <= ra->bra_prepared.tc_reg_size) &&
	       _0C(m0_be_tx_credit_le(&ra->bra_captured, &ra->bra_prepared));
}

M0_INTERNAL void m0_be_reg_area_used(struct m0_be_reg_area  *ra,
				     struct m0_be_tx_credit *used)
{
	struct m0_be_reg_d *rd;

	M0_PRE(m0_be_reg_area__invariant(ra));

	M0_SET0(used);
	for (rd = m0_be_regmap_first(&ra->bra_map); rd != NULL;
	     rd = m0_be_regmap_next(&ra->bra_map, rd))
		m0_be_tx_credit_add(used, &M0_BE_REG_D_CREDIT(rd));
}

M0_INTERNAL void m0_be_reg_area_prepared(struct m0_be_reg_area  *ra,
					 struct m0_be_tx_credit *prepared)
{
	M0_PRE(m0_be_reg_area__invariant(ra));

	*prepared = ra->bra_prepared;
}

M0_INTERNAL void m0_be_reg_area_captured(struct m0_be_reg_area  *ra,
					 struct m0_be_tx_credit *captured)
{
	M0_PRE(m0_be_reg_area__invariant(ra));

	*captured = ra->bra_captured;
}

static void be_reg_d_cpy(void *dst, const struct m0_be_reg_d *rd)
{
	memcpy(dst, rd->rd_reg.br_addr, rd->rd_reg.br_size);
}

static void *be_reg_area_alloc(struct m0_be_reg_area *ra, m0_bcount_t size)
{
	void *ptr;

	ptr = ra->bra_area + ra->bra_area_used;
	ra->bra_area_used += size;
	M0_POST(ra->bra_area_used <= ra->bra_prepared.tc_reg_size);
	return ptr;
}

static void be_reg_area_add_copy(void *data, struct m0_be_reg_d *rd)
{
	struct m0_be_reg_area *ra = data;

	M0_PRE(m0_be_reg_d__invariant(rd));
	M0_PRE(rd->rd_buf == NULL);

	rd->rd_buf = be_reg_area_alloc(ra, rd->rd_reg.br_size);
	be_reg_d_cpy(rd->rd_buf, rd);
}

static void be_reg_area_add(void *data, struct m0_be_reg_d *rd)
{
	M0_PRE(rd->rd_buf != NULL);
}

static void be_reg_area_del(void *data, const struct m0_be_reg_d *rd)
{
	/* do nothing */
}

static void be_reg_area_cpy_copy(void                     *data,
				 const struct m0_be_reg_d *super,
				 const struct m0_be_reg_d *rd)
{
	m0_bcount_t rd_offset;

	M0_PRE(m0_be_reg_d__invariant(rd));
	M0_PRE(be_reg_d_is_partof(super, rd));
	M0_PRE(super->rd_buf != NULL);
	M0_PRE(rd->rd_buf == NULL);

	rd_offset = rd->rd_reg.br_addr - super->rd_reg.br_addr;
	be_reg_d_cpy(super->rd_buf + rd_offset, rd);
}

/* XXX copy-paste from be_reg_area_cpy_copy() */
static void be_reg_area_cpy(void                     *data,
			    const struct m0_be_reg_d *super,
			    const struct m0_be_reg_d *rd)
{
	m0_bcount_t rd_offset;

	M0_PRE(m0_be_reg_d__invariant(rd));
	M0_PRE(be_reg_d_is_partof(super, rd));
	M0_PRE(super->rd_buf != NULL);
	M0_PRE(rd->rd_buf != NULL);

	rd_offset = rd->rd_reg.br_addr - super->rd_reg.br_addr;
	memcpy(super->rd_buf + rd_offset, rd->rd_buf, rd->rd_reg.br_size);
}

static void be_reg_area_cut(void               *data,
			    struct m0_be_reg_d *rd,
			    m0_bcount_t         cut_at_start,
			    m0_bcount_t         cut_at_end)
{
	rd->rd_buf += cut_at_start;
}

static void be_reg_area_split(void               *data,
                              struct m0_be_reg_d *rd,
                              struct m0_be_reg_d *rd_new)
{
	/*
	 * |         rd          |
	 * +---------+-----------+
	 * |   rd    |   rd_new  |   <-- rd->rd_reg is already split
	 *                               this function does the rest
	 */
	rd_new->rd_reg.br_seg = rd->rd_reg.br_seg;
	rd_new->rd_buf        = rd->rd_buf + rd->rd_reg.br_size;
	rd_new->rd_gen_idx    = rd->rd_gen_idx;
}

static const struct m0_be_regmap_ops be_reg_area_ops_data_copy = {
	.rmo_add   = be_reg_area_add_copy,
	.rmo_del   = be_reg_area_del,
	.rmo_cpy   = be_reg_area_cpy_copy,
	.rmo_cut   = be_reg_area_cut,
	.rmo_split = be_reg_area_split,
};

static const struct m0_be_regmap_ops be_reg_area_ops_data_nocopy = {
	.rmo_add   = be_reg_area_add,
	.rmo_del   = be_reg_area_del,
	.rmo_cpy   = be_reg_area_cpy,
	.rmo_cut   = be_reg_area_cut,
	.rmo_split = be_reg_area_split,
};

M0_INTERNAL void m0_be_reg_area_capture(struct m0_be_reg_area *ra,
					struct m0_be_reg_d    *rd)
{
	struct m0_be_tx_credit *captured = &ra->bra_captured;
	struct m0_be_tx_credit *prepared = &ra->bra_prepared;
	m0_bcount_t             reg_size = rd->rd_reg.br_size;

	M0_PRE(m0_be_reg_area__invariant(ra));
	M0_PRE(m0_be_reg_d__invariant(rd));

	m0_be_tx_credit_add(captured, &M0_BE_REG_D_CREDIT(rd));

	M0_ASSERT_INFO(m0_be_tx_credit_le(captured, prepared),
	               "There is not enough credits for capturing: "
	               "captured="
	               BETXCR_F" prepared="BETXCR_F" region_size=%lu",
	               BETXCR_P(captured), BETXCR_P(prepared), reg_size);

	m0_be_regmap_add(&ra->bra_map, rd);

	M0_POST(m0_be_reg_d__invariant(rd));
	M0_POST(m0_be_reg_area__invariant(ra));
}

M0_INTERNAL void m0_be_reg_area_uncapture(struct m0_be_reg_area    *ra,
					  const struct m0_be_reg_d *rd)
{
	M0_PRE(m0_be_reg_area__invariant(ra));
	M0_PRE(m0_be_reg_d__invariant(rd));

	m0_be_regmap_del(&ra->bra_map, rd);

	M0_POST(m0_be_reg_d__invariant(rd));
	M0_POST(m0_be_reg_area__invariant(ra));
}

M0_INTERNAL void m0_be_reg_area_merge_in(struct m0_be_reg_area *ra,
					 struct m0_be_reg_area *src)
{
	struct m0_be_reg_d *rd;

	M0_BE_REG_AREA_FORALL(src, rd) {
		m0_be_reg_area_capture(ra, rd);
	}
}

M0_INTERNAL void m0_be_reg_area_reset(struct m0_be_reg_area *ra)
{
	M0_PRE(m0_be_reg_area__invariant(ra));

	m0_be_regmap_reset(&ra->bra_map);
	ra->bra_area_used = 0;
	M0_SET0(&ra->bra_captured);

	M0_POST(m0_be_reg_area__invariant(ra));
}

M0_INTERNAL void m0_be_reg_area_optimize(struct m0_be_reg_area *ra)
{
	/* to be implemented. */
}

M0_INTERNAL struct m0_be_reg_d *
m0_be_reg_area_first(struct m0_be_reg_area *ra)
{
	return m0_be_regmap_first(&ra->bra_map);
}

M0_INTERNAL struct m0_be_reg_d *
m0_be_reg_area_next(struct m0_be_reg_area *ra, struct m0_be_reg_d *prev)
{
	return m0_be_regmap_next(&ra->bra_map, prev);
}

M0_INTERNAL int
m0_be_reg_area_merger_init(struct m0_be_reg_area_merger *brm,
                           int                           reg_area_nr_max)
{
	int rc;

	brm->brm_reg_area_nr_max = reg_area_nr_max;
	brm->brm_reg_area_nr     = 0;

	M0_ALLOC_ARR(brm->brm_reg_areas, brm->brm_reg_area_nr_max);
	M0_ALLOC_ARR(brm->brm_pos,       brm->brm_reg_area_nr_max);
	if (brm->brm_reg_areas == NULL || brm->brm_pos == NULL) {
		m0_free(brm->brm_reg_areas);
		m0_free(brm->brm_pos);
		rc = -ENOMEM;
	} else {
		rc = 0;
	}
	return rc;
}

M0_INTERNAL void m0_be_reg_area_merger_fini(struct m0_be_reg_area_merger *brm)
{
	m0_free(brm->brm_pos);
	m0_free(brm->brm_reg_areas);
}

M0_INTERNAL void m0_be_reg_area_merger_reset(struct m0_be_reg_area_merger *brm)
{
	brm->brm_reg_area_nr = 0;
}

M0_INTERNAL void m0_be_reg_area_merger_add(struct m0_be_reg_area_merger *brm,
                                           struct m0_be_reg_area        *ra)
{
	M0_PRE(brm->brm_reg_area_nr < brm->brm_reg_area_nr_max);

	brm->brm_reg_areas[brm->brm_reg_area_nr++] = ra;
}

static void be_reg_area_merger_max_gen_idx(struct m0_be_reg_area_merger *brm,
                                           void                         *addr,
                                           m0_bcount_t                   size,
                                           struct m0_be_reg_d           *rd_new)
{
	struct m0_be_reg_d *rd;
	int                 i;
	int                 max_i;

	*rd_new = M0_BE_REG_D(M0_BE_REG(NULL, size, addr), NULL);
	max_i = -1;
	for (i = 0; i < brm->brm_reg_area_nr; ++i) {
		rd = brm->brm_pos[i];
		M0_ASSERT(rd == NULL ||
			  be_reg_d_is_partof(rd, rd_new) ||
		          !be_reg_d_are_overlapping(rd, rd_new));
		M0_ASSERT(ergo(rd != NULL && max_i != -1,
		               rd->rd_gen_idx != rd_new->rd_gen_idx));
		if (rd != NULL && be_reg_d_are_overlapping(rd, rd_new) &&
		    (max_i == -1 || rd_new->rd_gen_idx < rd->rd_gen_idx)) {
			max_i = i;
			rd_new->rd_gen_idx = rd->rd_gen_idx;
		}
	}
	M0_ASSERT(max_i != -1);
	be_reg_d_sub_make(brm->brm_pos[max_i], rd_new);
}

M0_INTERNAL void
m0_be_reg_area_merger_merge_to(struct m0_be_reg_area_merger *brm,
                               struct m0_be_reg_area        *ra)
{
	struct m0_be_reg_d *rd;
	struct m0_be_reg_d  rd_new;
	m0_bcount_t         size;
	void               *addr;
	int                 i;

	for (i = 0; i < brm->brm_reg_area_nr; ++i)
		brm->brm_pos[i] = m0_be_reg_area_first(brm->brm_reg_areas[i]);
	addr = NULL;
	while (m0_exists(j, brm->brm_reg_area_nr, brm->brm_pos[j] != NULL)) {
		be_reg_d_arr_first_subreg(brm->brm_pos, brm->brm_reg_area_nr,
		                          addr, &addr, &size);
		be_reg_area_merger_max_gen_idx(brm, addr, size, &rd_new);
		m0_be_reg_area_capture(ra, &rd_new);
		addr += size;
		/* pass by all stale regions by @addr */
		for (i = 0; i < brm->brm_reg_area_nr; ++i) {
			rd = brm->brm_pos[i];
			if (rd != NULL && be_reg_d_lb1(rd) == addr) {
				rd = m0_be_reg_area_next(
				        brm->brm_reg_areas[i], rd);
				brm->brm_pos[i] = rd;
			}
		}
	}
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of be group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
