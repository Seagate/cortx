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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 *                  Maxim Medved <max.medved@seagate.com>
 * Original creation date: 17-Jun-2013
 */

#include "be/tx_regmap.h"

#include "ut/ut.h"		/* M0_UT_ASSERT */

#include "lib/types.h"          /* uint64_t */
#include "lib/arith.h"          /* m0_rnd64 */
#include "lib/misc.h"           /* M0_SET0 */
#include "lib/string.h"         /* memcpy */

#include "be/ut/helper.h"	/* m0_be_ut_seg */

/*
#define LOGD(...) printf(__VA_ARGS__)
*/
#define LOGD(...)

enum {
	BE_UT_RDT_SIZE	  = 0x10,
	BE_UT_RDT_R_SIZE  = 0x4,
	BE_UT_RDT_ITER	  = 0x10000,
};

struct be_ut_rdt_reg_d {
	struct m0_be_reg_d ur_rd;
	bool		   ur_inserted;
};

static struct be_ut_rdt_reg_d  be_ut_rdt_rd[BE_UT_RDT_SIZE];
static struct m0_be_reg_d_tree be_ut_rdt;

static bool be_ut_reg_d_is_equal(const struct m0_be_reg_d *rd1,
				 const struct m0_be_reg_d *rd2)
{
	return rd1->rd_reg.br_size == rd2->rd_reg.br_size &&
	       rd1->rd_reg.br_addr == rd2->rd_reg.br_addr;
}

static int be_ut_rdt_del_find(int index)
{
	int i;

	for (i = index + 1; i < BE_UT_RDT_SIZE; ++i) {
		if (be_ut_rdt_rd[i].ur_inserted)
			break;
	}
	return i;
}

static void be_ut_reg_d_tree_check(void)
{
	struct be_ut_rdt_reg_d *urd;
	struct m0_be_reg_d     *rd;
	int			i;
	size_t			size = 0;

	rd = m0_be_rdt_find(&be_ut_rdt, NULL);
	for (i = 0; i < BE_UT_RDT_SIZE; ++i) {
		urd = &be_ut_rdt_rd[i];
		if (urd->ur_inserted) {
			++size;
			M0_UT_ASSERT(be_ut_reg_d_is_equal(rd, &urd->ur_rd));
			rd = m0_be_rdt_next(&be_ut_rdt, rd);
		}
	}
	M0_UT_ASSERT(rd == NULL);
	M0_UT_ASSERT(size == m0_be_rdt_size(&be_ut_rdt));
}

void m0_be_ut_reg_d_tree(void)
{
	struct be_ut_rdt_reg_d *urd;
	struct m0_be_reg_d     *rd;
	uint64_t                seed = 0;
	m0_bcount_t		r_size;
	void		       *r_addr;
	int			rc;
	int			i;
	int			index;
	int			del_i;

	rc = m0_be_rdt_init(&be_ut_rdt, BE_UT_RDT_SIZE);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < BE_UT_RDT_SIZE; ++i) {
		r_size = m0_rnd64(&seed) % BE_UT_RDT_R_SIZE + 1;
		r_addr = (void *) (uintptr_t) (i * BE_UT_RDT_R_SIZE + 1);
		be_ut_rdt_rd[i] = (struct be_ut_rdt_reg_d) {
			.ur_rd = {
				.rd_reg = M0_BE_REG(NULL, r_size, r_addr),
			},
			.ur_inserted = false,
		};
	}
	be_ut_reg_d_tree_check();
	for (i = 0; i < BE_UT_RDT_ITER; ++i) {
		index = m0_rnd64(&seed) % BE_UT_RDT_SIZE;
		urd = &be_ut_rdt_rd[index];

		if (!urd->ur_inserted) {
			m0_be_rdt_ins(&be_ut_rdt, &urd->ur_rd);
		} else {
			del_i = be_ut_rdt_del_find(index);
			rd = m0_be_rdt_del(&be_ut_rdt, &urd->ur_rd);
			M0_UT_ASSERT(equi(del_i == BE_UT_RDT_SIZE, rd == NULL));
			M0_UT_ASSERT(ergo(del_i != BE_UT_RDT_SIZE,
					  be_ut_reg_d_is_equal(rd,
						&be_ut_rdt_rd[del_i].ur_rd)));
		}
		urd->ur_inserted = !urd->ur_inserted;

		be_ut_reg_d_tree_check();
	}
	m0_be_rdt_fini(&be_ut_rdt);
}

enum {
	BE_UT_REGMAP_ITER   = 0x10000,
	BE_UT_REGMAP_LEN    = 0x20,
	BE_UT_REGMAP_R_SIZE = 0x10,
};

/* [begin, end) */
struct be_ut_test_reg {
	m0_bcount_t bur_begin;
	m0_bcount_t bur_end;
	bool	    bur_do_insert;
	size_t	    bur_desired_nr;
	m0_bcount_t bur_desired_len;
};

struct be_ut_test_reg_suite {
	char		      *trs_name;
	struct be_ut_test_reg *trs_test;
	size_t		       trs_nr;
};

#define BUT_TR(begin, end, do_insert, desired_nr, desired_len)	\
	{							\
		.bur_begin = (begin),				\
		.bur_end = (end),				\
		.bur_do_insert = (do_insert),			\
		.bur_desired_nr = (desired_nr),			\
		.bur_desired_len = (desired_len),		\
	}


#define BUT_TRA_NAME(name) be_ut_tra_##name
#define DEFINE_BUT_TRA(name, ...)					    \
	static struct be_ut_test_reg BUT_TRA_NAME(name)[] = { __VA_ARGS__ }

#define BUT_TRS(name)	{					\
	.trs_name = #name,					\
	.trs_test = BUT_TRA_NAME(name),				\
	.trs_nr	  = ARRAY_SIZE(BUT_TRA_NAME(name)),		\
}

/*
 * ins = insert
 * del = delete
 * adj = adjacent
 * cpy = copy
 * top = top
 * bot = bottom
 */
DEFINE_BUT_TRA(ins_simple, BUT_TR(1, 3, true, 1, 2));
DEFINE_BUT_TRA(ins_size1, BUT_TR(1, 2, true, 1, 1));

DEFINE_BUT_TRA(ins2, BUT_TR(1, 3, true, 1, 2), BUT_TR(4, 6, true, 2, 4));
DEFINE_BUT_TRA(ins2_size1, BUT_TR(1, 2, true, 1, 1), BUT_TR(2, 3, true, 2, 2));

DEFINE_BUT_TRA(ins2_adj, BUT_TR(1, 3, true, 1, 2), BUT_TR(3, 5, true, 2, 4));
DEFINE_BUT_TRA(ins2_adj_size1,
	       BUT_TR(1, 3, true, 1, 2), BUT_TR(3, 4, true, 2, 3));

DEFINE_BUT_TRA(ins_replace, BUT_TR(1, 3, true, 1, 2), BUT_TR(1, 3, true, 1, 2));
DEFINE_BUT_TRA(ins_cpy, BUT_TR(3, 8, true, 1, 5), BUT_TR(4, 7, true, 1, 5));
DEFINE_BUT_TRA(ins_cpy_top, BUT_TR(3, 8, true, 1, 5), BUT_TR(5, 8, true, 1, 5));
DEFINE_BUT_TRA(ins_cpy_bot, BUT_TR(3, 8, true, 1, 5), BUT_TR(3, 5, true, 1, 5));

DEFINE_BUT_TRA(del_none, BUT_TR(1, 3, false, 0, 0));
DEFINE_BUT_TRA(del_none_size1, BUT_TR(1, 2, false, 0, 0));

DEFINE_BUT_TRA(del_entire,
	       BUT_TR(1, 3, true, 1, 2), BUT_TR(1, 3, false, 0, 0));
DEFINE_BUT_TRA(del_entire_top,
	       BUT_TR(1, 3, true, 1, 2), BUT_TR(1, 4, false, 0, 0));
DEFINE_BUT_TRA(del_entire_bot,
	       BUT_TR(3, 5, true, 1, 2), BUT_TR(2, 5, false, 0, 0));
DEFINE_BUT_TRA(del_entire_both,
	       BUT_TR(3, 5, true, 1, 2), BUT_TR(2, 6, false, 0, 0));
DEFINE_BUT_TRA(del_nop,
	       BUT_TR(3, 8, true, 1, 5), BUT_TR(4, 7, false, 1, 5));

DEFINE_BUT_TRA(ins2_del1,
	       BUT_TR(3, 5, true, 1, 2), BUT_TR(6, 7, true, 2, 3),
	       BUT_TR(3, 5, false, 1, 1));
DEFINE_BUT_TRA(ins2_del2,
	       BUT_TR(3, 5, true, 1, 2), BUT_TR(6, 7, true, 2, 3),
	       BUT_TR(3, 5, false, 1, 1), BUT_TR(6, 7, false, 0, 0));
DEFINE_BUT_TRA(ins2_del_all,
	       BUT_TR(3, 5, true, 1, 2), BUT_TR(6, 7, true, 2, 3),
	       BUT_TR(3, 7, false, 0, 0));

DEFINE_BUT_TRA(del_cut_top1,
	       BUT_TR(3, 8, true, 1, 5), BUT_TR(5, 9, false, 1, 2));
DEFINE_BUT_TRA(del_cut_top0,
	       BUT_TR(3, 8, true, 1, 5), BUT_TR(5, 8, false, 1, 2));
DEFINE_BUT_TRA(del_cut_bot1,
	       BUT_TR(3, 8, true, 1, 5), BUT_TR(2, 5, false, 1, 3));
DEFINE_BUT_TRA(del_cut_bot0,
	       BUT_TR(3, 8, true, 1, 5), BUT_TR(3, 5, false, 1, 3));
DEFINE_BUT_TRA(del_cut_adj,
	       BUT_TR(3, 5, true, 1, 2), BUT_TR(5, 8, true, 2, 5),
	       BUT_TR(4, 6, false, 2, 3));
DEFINE_BUT_TRA(del_cut_entire_top,
	       BUT_TR(3, 5, true, 1, 2), BUT_TR(5, 8, true, 2, 5),
	       BUT_TR(4, 8, false, 1, 1));
DEFINE_BUT_TRA(del_cut_entire_bot,
	       BUT_TR(3, 5, true, 1, 2), BUT_TR(5, 8, true, 2, 5),
	       BUT_TR(3, 6, false, 1, 2));

static struct be_ut_test_reg_suite be_ut_test_regs[] = {
	BUT_TRS(ins_simple),
	BUT_TRS(ins_size1),
	BUT_TRS(ins2),
	BUT_TRS(ins2_size1),
	BUT_TRS(ins2_adj),
	BUT_TRS(ins2_adj_size1),
	BUT_TRS(ins_replace),
	BUT_TRS(ins_cpy),
	BUT_TRS(ins_cpy_top),
	BUT_TRS(ins_cpy_bot),
	BUT_TRS(del_none),
	BUT_TRS(del_none_size1),
	BUT_TRS(del_entire),
	BUT_TRS(del_entire_top),
	BUT_TRS(del_entire_bot),
	BUT_TRS(del_entire_both),
	BUT_TRS(del_nop),
	BUT_TRS(ins2_del1),
	BUT_TRS(ins2_del2),
	BUT_TRS(ins2_del_all),
	BUT_TRS(del_cut_top1),
	BUT_TRS(del_cut_top0),
	BUT_TRS(del_cut_bot1),
	BUT_TRS(del_cut_bot0),
	BUT_TRS(del_cut_adj),
	BUT_TRS(del_cut_entire_top),
	BUT_TRS(del_cut_entire_bot),
};

#undef BUT_TRS
#undef DEFINE_BUT_TRA
#undef BUT_TRA_NAME
#undef BUT_TR

static struct m0_be_regmap be_ut_rm_regmap;
static const unsigned	   be_ut_rm_unused = ~0;
static unsigned		   be_ut_rm_data[BE_UT_REGMAP_LEN];
static unsigned		   be_ut_rm_reg[BE_UT_REGMAP_LEN];
static unsigned		   be_ut_rm_data_copy[BE_UT_REGMAP_LEN];
static unsigned		   be_ut_rm_iteration;
static void		  *be_ut_rm_ops_data = (void *) 42;

static void be_ut_rm_fill2(uintptr_t addr, m0_bcount_t size, unsigned value,
			   bool fill_reg)
{
	uintptr_t i;

	for (i = addr; i < addr + size; ++i) {
		M0_UT_ASSERT(0 <= i && i < BE_UT_REGMAP_LEN);
		M0_UT_ASSERT(equi(be_ut_rm_unused == be_ut_rm_data[i],
				  be_ut_rm_unused != value));
		M0_UT_ASSERT(ergo(fill_reg,
				  equi(be_ut_rm_unused == be_ut_rm_reg[i],
				       be_ut_rm_unused != value)));
		be_ut_rm_data[i] = value;
		if (fill_reg)
			be_ut_rm_reg[i] = value;
	}
}

static void be_ut_rm_fill(const struct m0_be_reg_d *rd, unsigned value,
			  bool fill_reg)
{
	M0_PRE(m0_be_reg_d__invariant(rd));
	be_ut_rm_fill2((uintptr_t) rd->rd_reg.br_addr,
		       rd->rd_reg.br_size, value, fill_reg);
}

static void be_ut_regmap_add(void *data, struct m0_be_reg_d *rd)
{
	M0_PRE(data == be_ut_rm_ops_data);
	be_ut_rm_fill(rd, be_ut_rm_iteration, true);
}

static void be_ut_regmap_del(void *data, const struct m0_be_reg_d *rd)
{
	M0_PRE(data == be_ut_rm_ops_data);
	be_ut_rm_fill(rd, be_ut_rm_unused, true);
}

static void be_ut_regmap_cpy(void *data, const struct m0_be_reg_d *super,
			     const struct m0_be_reg_d *rd)
{
	M0_PRE(data == be_ut_rm_ops_data);
	be_ut_rm_fill(rd, be_ut_rm_unused, false);
	be_ut_rm_fill(rd, be_ut_rm_iteration, false);
}

static void be_ut_regmap_cut(void *data, struct m0_be_reg_d *rd,
			    m0_bcount_t cut_at_start, m0_bcount_t cut_at_end)
{
	m0_bcount_t size;
	uintptr_t   addr;

	M0_PRE(m0_be_reg_d__invariant(rd));
	M0_PRE(data == be_ut_rm_ops_data);

	size = rd->rd_reg.br_size;
	addr = (uintptr_t) rd->rd_reg.br_addr;

	if (cut_at_start != 0)
		be_ut_rm_fill2(addr, cut_at_start, be_ut_rm_unused, true);

	if (cut_at_end != 0)
		be_ut_rm_fill2(addr + size - cut_at_end, cut_at_end,
			       be_ut_rm_unused, true);
}

static const struct m0_be_regmap_ops be_ut_regmap_ops = {
	.rmo_add = be_ut_regmap_add,
	.rmo_del = be_ut_regmap_del,
	.rmo_cpy = be_ut_regmap_cpy,
	.rmo_cut = be_ut_regmap_cut
};

static void be_ut_regmap_init(void)
{
	int rc;
	int i;

	M0_SET0(&be_ut_rm_regmap);
	rc = m0_be_regmap_init(&be_ut_rm_regmap, &be_ut_regmap_ops,
			       be_ut_rm_ops_data, BE_UT_REGMAP_ITER, true);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < BE_UT_REGMAP_LEN; ++i) {
		be_ut_rm_data[i] = be_ut_rm_unused;
		be_ut_rm_reg[i]	 = be_ut_rm_unused;
	}
	be_ut_rm_iteration = 0;
}

static void be_ut_regmap_fini(void)
{
	m0_be_regmap_fini(&be_ut_rm_regmap);
}

static void be_ut_regmap_data_copy(void)
{
	memcpy(&be_ut_rm_data_copy, &be_ut_rm_data, sizeof be_ut_rm_data_copy);
}

static void be_ut_regmap_print_d(unsigned d, int i)
{
	if (d == be_ut_rm_unused) {
		LOGD("%*s", 4, ".");
	} else {
		LOGD("%4.u", d);
	}
	LOGD("%c", i == BE_UT_REGMAP_LEN - 1 ? '\n' : ' ');
}

static void be_ut_regmap_data_cmp(const struct m0_be_reg_d *r,
				  unsigned desired,
				  bool nop)
{
	int i;

	LOGD("save: ");
	for (i = 0; i < BE_UT_REGMAP_LEN; ++i)
		be_ut_regmap_print_d(be_ut_rm_data_copy[i], i);
	LOGD("curr: ");
	for (i = 0; i < BE_UT_REGMAP_LEN; ++i)
		be_ut_regmap_print_d(be_ut_rm_data[i], i);
	LOGD("reg : ");
	for (i = 0; i < BE_UT_REGMAP_LEN; ++i)
		be_ut_regmap_print_d(be_ut_rm_reg[i], i);
	LOGD("    : ");
	for (i = 0; i < BE_UT_REGMAP_LEN; ++i)
		be_ut_regmap_print_d(i % 10, i);

	for (i = 0; i < BE_UT_REGMAP_LEN; ++i) {
		M0_UT_ASSERT(ergo(nop,
				  be_ut_rm_data[i] == be_ut_rm_data_copy[i]));
		if (m0_be_reg_d_is_in(r, (void *) (uintptr_t) i)) {
			M0_UT_ASSERT(ergo(!nop, be_ut_rm_data[i] == desired));
		} else {
			M0_UT_ASSERT(ergo(!nop, be_ut_rm_data[i] ==
					  be_ut_rm_data_copy[i]));
		}
	}
}

/* check regmap size - number of regions in the regmap */
static void be_ut_regmap_size_length_check(size_t desired_size,
					   m0_bcount_t desired_length,
					   bool do_check)
{
	struct m0_be_reg_d *rd;
	size_t		    size = 0;
	m0_bcount_t	    length = 0;
	size_t		    size1 = 0;
	m0_bcount_t	    length1 = 0;
	int		    i;

	for (rd = m0_be_regmap_first(&be_ut_rm_regmap); rd != NULL;
	     rd = m0_be_regmap_next(&be_ut_rm_regmap, rd)) {
		++size;
		length += rd->rd_reg.br_size;
	}
	M0_UT_ASSERT(ergo(do_check, size == desired_size));
	M0_UT_ASSERT(ergo(do_check, length == desired_length));
	M0_UT_ASSERT(size == m0_be_regmap_size(&be_ut_rm_regmap));
	for (i = 0; i < BE_UT_REGMAP_LEN; ++i) {
		length1 += be_ut_rm_reg[i] != be_ut_rm_unused;
		size1 += (i == 0 ? be_ut_rm_unused : be_ut_rm_reg[i - 1]) !=
			 be_ut_rm_reg[i] && be_ut_rm_reg[i] != be_ut_rm_unused;
	}
	M0_UT_ASSERT(length1 == length);
	M0_UT_ASSERT(size1 == size);
}

/*
 * There is one special case when m0_be_regmap_del() is no-op: when a new region
 * is completely covered by existing and existing is larger than new at the
 * beginning and at the end. For example, if new region is [5, 10) and in the
 * tree there is a region [3, 12), then delete is no-op because delete operation
 * will need additional credit m0_be_tx_credit.tc_reg_nr for the regions [3, 5)
 * and [10, 12).
 */
static bool be_ut_regmap_nop(m0_bcount_t begin, m0_bcount_t end, bool do_insert)
{
	unsigned value;
	int	 i;

	if (do_insert)
		return false;
	if (begin == 0 || end + 1 == BE_UT_REGMAP_LEN)
		return false;

	value = be_ut_rm_reg[begin];
	for (i = begin; i < end; ++i) {
		if (be_ut_rm_reg[i] != value)
			return false;
	}
	if (be_ut_rm_reg[begin - 1] != value || be_ut_rm_reg[end] != value)
		return false;
	return true;
}

/* do operation with range [begin, end) */
static void be_ut_regmap_do(m0_bcount_t begin, m0_bcount_t end, bool do_insert)
{
	static unsigned long gen_idx = 0;
	struct m0_be_reg_d   rd;
	unsigned	     desired;
	bool		     nop;

	M0_PRE(0 <= begin && begin < BE_UT_REGMAP_LEN);
	M0_PRE(0 <= end   && end   <= BE_UT_REGMAP_LEN);
	M0_PRE(begin <= end);

	++be_ut_rm_iteration;

	LOGD("\n%s [%lu, %lu), iteration = %u\n",
	       do_insert ? "ins" : "del", begin, end, be_ut_rm_iteration);

	/** XXX TODO check other fields of m0_be_reg_d */
	rd = (struct m0_be_reg_d ) {
		.rd_reg     = M0_BE_REG(NULL, end - begin, (void *) begin),
		.rd_gen_idx = ++gen_idx,
	};

	be_ut_regmap_data_copy();
	nop = be_ut_regmap_nop(begin, end, do_insert);

	if (do_insert)
		m0_be_regmap_add(&be_ut_rm_regmap, &rd);
	else
		m0_be_regmap_del(&be_ut_rm_regmap, &rd);

	desired = do_insert == 1 ? be_ut_rm_iteration : be_ut_rm_unused;
	be_ut_regmap_data_cmp(&rd, desired, nop);
	be_ut_regmap_size_length_check(0, 0, false);
}

void m0_be_ut_regmap_simple(void)
{
	struct be_ut_test_reg_suite *burs;
	struct be_ut_test_reg	    *bur;
	int			     i;
	int			     j;


	for (i = 0; i < ARRAY_SIZE(be_ut_test_regs); ++i) {
		be_ut_regmap_init();
		burs = &be_ut_test_regs[i];
		be_ut_regmap_size_length_check(0, 0, true);
		LOGD("\ntesting %s suite\n", burs->trs_name);
		for (j = 0; j < burs->trs_nr; ++j) {
			bur = &burs->trs_test[j];
			be_ut_regmap_do(bur->bur_begin, bur->bur_end,
					bur->bur_do_insert);
			be_ut_regmap_size_length_check(bur->bur_desired_nr,
						       bur->bur_desired_len,
						       true);
		}
		be_ut_regmap_fini();
	}
}

void m0_be_ut_regmap_random(void)
{
	m0_bcount_t begin;
	m0_bcount_t end;
	unsigned    i;
	uint64_t    seed = 0;
	int	    do_insert;

	be_ut_regmap_init();
	for (i = 0; i < BE_UT_REGMAP_ITER; ++i) {
		begin = m0_rnd64(&seed) % (BE_UT_REGMAP_LEN -
					 BE_UT_REGMAP_R_SIZE - 1) + 1;
		end = begin + m0_rnd64(&seed) % BE_UT_REGMAP_R_SIZE + 1;
		do_insert = m0_rnd64(&seed) % 2;
		be_ut_regmap_do(begin, end, do_insert != 0);
	}
	be_ut_regmap_fini();
}

enum {
	BE_UT_RA_SEG_SIZE = 0x10000,
	BE_UT_RA_TEST_NR  = 0x100,
	BE_UT_RA_R_SIZE	  = 0x10,
	BE_UT_RA_SIZE	  = 0x20,
	BE_UT_RA_ITER	  = 0x10000,
};

static struct m0_be_reg_area  be_ut_ra_reg_area;
static struct m0_be_seg      *be_ut_ra_seg;
static uint64_t               be_ut_ra_rand_seed;
static char		      be_ut_ra_save[BE_UT_RA_SIZE];
static char		      be_ut_ra_data[BE_UT_RA_SIZE];
static char		      be_ut_ra_reg[BE_UT_RA_SIZE];

static m0_bindex_t be_ut_reg_area_addr2offs(void *addr)
{
	return addr - be_ut_ra_seg->bs_addr - be_ut_ra_seg->bs_reserved;
}

static void *be_ut_reg_area_offs2addr(m0_bindex_t offs)
{
	return (void *) (uintptr_t) be_ut_ra_seg->bs_addr +
	       be_ut_ra_seg->bs_reserved + offs;
}

static void be_ut_reg_area_reset(bool reset_save)
{
	int i;

	for (i = 0; i < BE_UT_RA_SIZE; ++i) {
		be_ut_ra_data[i] = 0;
		if (reset_save)
			be_ut_ra_save[i] = 0;
	}
}

static void be_ut_reg_area_init(m0_bindex_t nr)
{
	int rc;

	rc = m0_be_reg_area_init(&be_ut_ra_reg_area,
				 &M0_BE_TX_CREDIT(nr * 2, nr * BE_UT_RA_R_SIZE),
				 M0_BE_REG_AREA_DATA_COPY);
	M0_UT_ASSERT(rc == 0);
	be_ut_reg_area_reset(true);
}

static void be_ut_reg_area_fini(void)
{
	m0_be_reg_area_fini(&be_ut_ra_reg_area);
}

static void be_ut_reg_area_fill(struct m0_be_reg_d *rd)
{
	uintptr_t  begin = be_ut_reg_area_addr2offs(rd->rd_reg.br_addr);
	uintptr_t  end	 = begin + rd->rd_reg.br_size;

	M0_PRE(0 <= begin && begin <  BE_UT_RA_SIZE);
	M0_PRE(0 <= end   && end   <= BE_UT_RA_SIZE);
	M0_PRE(begin <= end);

	memcpy(&be_ut_ra_data[begin], rd->rd_buf, end - begin);
}

/* XXX copy-paste from be_ut_reg_area_fill() */
static void be_ut_reg_area_fill_save(struct m0_be_reg_d *rd)
{
	uintptr_t  begin = be_ut_reg_area_addr2offs(rd->rd_reg.br_addr);
	uintptr_t  end	 = begin + rd->rd_reg.br_size;

	M0_PRE(0 <= begin && begin <  BE_UT_RA_SIZE);
	M0_PRE(0 <= end   && end   <= BE_UT_RA_SIZE);
	M0_PRE(begin <= end);

	memcpy(&be_ut_ra_save[begin], rd->rd_reg.br_addr, end - begin);
}

static void be_ut_reg_area_get(void)
{
	struct m0_be_reg_d *rd;

	be_ut_reg_area_reset(false);
	for (rd = m0_be_reg_area_first(&be_ut_ra_reg_area); rd != NULL;
	     rd = m0_be_reg_area_next(&be_ut_ra_reg_area, rd)) {
		be_ut_reg_area_fill(rd);
	}
}

static void be_ut_reg_area_size_length_check(size_t desired_size,
					     m0_bcount_t desired_length,
					     bool do_check)
{
	struct m0_be_tx_credit used;

	m0_be_reg_area_used(&be_ut_ra_reg_area, &used);
	M0_UT_ASSERT(ergo(do_check, used.tc_reg_nr == desired_size));
	M0_UT_ASSERT(ergo(do_check, used.tc_reg_size == desired_length));
}

static void be_ut_reg_area_check(bool do_insert, struct m0_be_reg_d *rd)
{
	struct m0_be_reg_d *rdi;
	int		    cmp;
	m0_bindex_t	    begin;
	m0_bindex_t	    end;
	m0_bindex_t	    ibegin;
	m0_bindex_t	    iend;

	if (do_insert) {
		be_ut_reg_area_fill_save(rd);
	} else {
		begin = be_ut_reg_area_addr2offs(rd->rd_reg.br_addr);
		end   = begin + rd->rd_reg.br_size;
		for (rdi = m0_be_reg_area_first(&be_ut_ra_reg_area);
		     rdi != NULL;
		     rdi = m0_be_reg_area_next(&be_ut_ra_reg_area, rdi)) {
			ibegin = be_ut_reg_area_addr2offs(rdi->rd_reg.br_addr);
			iend   = ibegin + rdi->rd_reg.br_size;
			if (ibegin < begin && end < iend)
				break;
		}
		if (rdi == NULL) {
			begin = be_ut_reg_area_addr2offs(rd->rd_reg.br_addr);
			bzero(&be_ut_ra_save[begin], rd->rd_reg.br_size);
		}
	}
	cmp = memcmp(be_ut_ra_save, be_ut_ra_data,
		     ARRAY_SIZE(be_ut_ra_save));
	M0_UT_ASSERT(cmp == 0);
}

static void be_ut_reg_area_rand(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(be_ut_ra_reg); ++i)
		be_ut_ra_reg[i] = m0_rnd64(&be_ut_ra_rand_seed) % 0xFF + 1;
}

/*
 * insert = capture
 * delete = uncapture
 */
/* do operation with range [begin, end) */
/* XXX copy-paste from be_ut_regmap_do() */
static void be_ut_reg_area_do(m0_bcount_t begin, m0_bcount_t end,
			      bool do_insert)
{
	struct m0_be_reg_d rd;

	M0_PRE(0 <= begin && begin < BE_UT_RA_SIZE);
	M0_PRE(0 <= end   && end   <= BE_UT_RA_SIZE);
	M0_PRE(begin <= end);

	memcpy(be_ut_ra_save, be_ut_ra_data, sizeof be_ut_ra_save);

	LOGD("\n%s [%lu, %lu)",
	     do_insert ? "capture" : "uncapture", begin, end);

	/** XXX TODO check other fields of m0_be_reg_d */
	rd = (struct m0_be_reg_d ) {
		.rd_reg = M0_BE_REG(be_ut_ra_seg, end - begin,
				    be_ut_reg_area_offs2addr(begin)),
	};

	if (do_insert) {
		be_ut_reg_area_rand();
		memcpy(rd.rd_reg.br_addr, be_ut_ra_reg, rd.rd_reg.br_size);
		m0_be_reg_area_capture(&be_ut_ra_reg_area, &rd);
	} else {
		m0_be_reg_area_uncapture(&be_ut_ra_reg_area, &rd);
	}

	be_ut_reg_area_get();
	be_ut_reg_area_check(do_insert, &rd);
}

/* XXX FIXME copy-paste from m0_be_ut_regmap_simple */
void m0_be_ut_reg_area_simple(void)
{
	struct be_ut_test_reg_suite *burs;
	struct be_ut_test_reg	    *bur;
	struct m0_be_ut_seg	     ut_seg;
	m0_bindex_t		     nr;
	int			     i;
	int			     j;

	m0_be_ut_seg_init(&ut_seg, NULL, BE_UT_RA_SEG_SIZE);
	be_ut_ra_seg = ut_seg.bus_seg;

	be_ut_ra_rand_seed = 0;
	for (i = 0; i < ARRAY_SIZE(be_ut_test_regs); ++i) {
		burs = &be_ut_test_regs[i];

		nr = 0;
		for (j = 0; j < burs->trs_nr; ++j)
			nr += burs->trs_test[j].bur_do_insert;
		be_ut_reg_area_init(nr);

		be_ut_reg_area_size_length_check(0, 0, true);
		LOGD("\ntesting %s suite\n", burs->trs_name);
		for (j = 0; j < burs->trs_nr; ++j) {
			bur = &burs->trs_test[j];
			be_ut_reg_area_do(bur->bur_begin, bur->bur_end,
					  bur->bur_do_insert);
			be_ut_reg_area_size_length_check(bur->bur_desired_nr,
							 bur->bur_desired_len,
							 true);
		}
		be_ut_reg_area_fini();
	}

	m0_be_ut_seg_fini(&ut_seg);
}

/* XXX FIXME copy-paste from m0_be_ut_regmap_random */
void m0_be_ut_reg_area_random(void)
{
	struct m0_be_ut_seg ut_seg;
	m0_bcount_t	    begin;
	m0_bcount_t	    end;
	unsigned	    i;
	uint64_t            seed = 0;
	int		    do_insert;

	m0_be_ut_seg_init(&ut_seg, NULL, BE_UT_RA_SEG_SIZE);
	be_ut_ra_seg = ut_seg.bus_seg;

	be_ut_reg_area_init(BE_UT_RA_ITER);
	for (i = 0; i < BE_UT_RA_ITER; ++i) {
		begin = m0_rnd64(&seed) % (BE_UT_REGMAP_LEN -
					 BE_UT_REGMAP_R_SIZE - 1) + 1;
		end = begin + m0_rnd64(&seed) % BE_UT_REGMAP_R_SIZE + 1;
		do_insert = m0_rnd64(&seed) % 2;
		be_ut_reg_area_do(begin, end, do_insert != 0);
		be_ut_reg_area_size_length_check(0, 0, false);
	}
	be_ut_reg_area_fini();
	m0_be_ut_seg_fini(&ut_seg);
}

/* backend UT reg area merge. R == region */
enum {
	BE_UT_RA_MERGE_SEG_SIZE	   = 0x10000,
	BE_UT_RA_MERGE_R_SIZE_MIN  = 1,
	BE_UT_RA_MERGE_R_SIZE_MAX  = 4,
	BE_UT_RA_MERGE_R_SPACE_MIN = 0,
	BE_UT_RA_MERGE_R_SPACE_MAX = 2,
	BE_UT_RA_MERGE_R_NR_MAX	   = 5,
	BE_UT_RA_MERGE_SIZE_TOTAL  = 16,
	BE_UT_RA_MERGE_NR	   = 0x100,
	BE_UT_RA_MERGE_ITER	   = 0x100,
};

/* #define BE_UT_RA_MERGE_DEBUG */

static struct m0_be_seg	*be_ut_ra_merge_seg;
static uint64_t          be_ut_ra_merge_seed;
static unsigned char	 be_ut_ra_merge_pre[BE_UT_RA_MERGE_SIZE_TOTAL];
static unsigned char	 be_ut_ra_merged[BE_UT_RA_MERGE_SIZE_TOTAL];
static unsigned char	 be_ut_ra_merge_post[BE_UT_RA_MERGE_SIZE_TOTAL];

/* get random number in range [min, max] */
static unsigned be_ut_reg_area_merge_rand(unsigned min, unsigned max)
{
	return m0_rnd64(&be_ut_ra_merge_seed) % (max - min + 1) + min;
}

static m0_bindex_t be_ut_reg_area_merge_addr2offs(void *addr)
{
	return (uintptr_t) addr - be_ut_ra_merge_seg->bs_reserved -
	       (uintptr_t) be_ut_ra_merge_seg->bs_addr;
}

static void *be_ut_reg_area_merge_offs2addr(m0_bindex_t offs)
{
	return be_ut_ra_merge_seg->bs_addr +
	       be_ut_ra_merge_seg->bs_reserved + offs;
}

static void be_ut_reg_area_merge_ra_add(struct m0_be_reg_area *ra,
					m0_bindex_t begin,
					m0_bcount_t size,
					void *buf)
{
	void *addr = be_ut_reg_area_merge_offs2addr(begin);

	memcpy(addr, buf, size);
	struct m0_be_reg_d rd = {
		.rd_reg = M0_BE_REG(be_ut_ra_merge_seg, size, addr),
		.rd_buf = NULL,
	};
	m0_be_reg_area_capture(ra, &rd);
}

/* this function is the most tricky part of reg_area merge UT */
static void be_ut_reg_area_merge_rand_ra(struct m0_be_reg_area *ra)
{
	static unsigned char random[BE_UT_RA_MERGE_SIZE_TOTAL];
	m0_bindex_t	     pos = 0;
	m0_bindex_t	     begin;
	m0_bcount_t	     size;
	int		     i;
	int		     nr;

	pos = 0;
	nr = 0;
	while (pos < BE_UT_RA_MERGE_SIZE_TOTAL &&
	       nr < BE_UT_RA_MERGE_R_NR_MAX) {
		begin = pos;
		size = be_ut_reg_area_merge_rand(BE_UT_RA_MERGE_R_SIZE_MIN,
						 BE_UT_RA_MERGE_R_SIZE_MAX);
		for (i = 0; i < size; ++i)
			random[i] = be_ut_reg_area_merge_rand(1, 0xFF);
		if (begin + size < BE_UT_RA_MERGE_SIZE_TOTAL)
			be_ut_reg_area_merge_ra_add(ra, begin, size, random);

		pos += size;
		/* skip some space after the region */
		size = be_ut_reg_area_merge_rand(BE_UT_RA_MERGE_R_SPACE_MIN,
						 BE_UT_RA_MERGE_R_SPACE_MAX);
		pos += size;
		++nr;
	}
}

static void be_ut_reg_area_arr_copy(unsigned char *arr,
				    m0_bcount_t arr_size,
				    struct m0_be_reg_area *ra,
				    bool clear_arr)
{
	struct m0_be_reg_d *rd;
	m0_bindex_t	    index;
	m0_bcount_t	    size;
	int		    i;

	if (clear_arr)
		memset(arr, 0, arr_size);

	M0_BE_REG_AREA_FORALL(ra, rd) {
		index = be_ut_reg_area_merge_addr2offs(rd->rd_reg.br_addr);
		size  = rd->rd_reg.br_size;
		M0_UT_ASSERT(index >= 0);
		M0_UT_ASSERT(size > 0);
		/* it might be integer overflow in (index + size) expression */
		M0_UT_ASSERT(index	  < arr_size);
		M0_UT_ASSERT(size	  < arr_size);
		M0_UT_ASSERT(index + size < arr_size);
		memcpy(&arr[index], rd->rd_buf, size);
		for (i = 0; i < size; ++i)
			M0_UT_ASSERT(arr[index + i] != 0);
	};
}

static void be_ut_reg_area_merge_pre(struct m0_be_reg_area *ra)
{
#if BE_UT_RA_MERGE_DEBUG
	int i;
#endif

	be_ut_reg_area_arr_copy(be_ut_ra_merge_pre,
				ARRAY_SIZE(be_ut_ra_merge_pre), ra, true);
#if BE_UT_RA_MERGE_DEBUG
	printf("merge_pre:  ");
	for (i = 0; i < ARRAY_SIZE(be_ut_ra_merge_pre); ++i)
		printf("%4.u", (unsigned) be_ut_ra_merge_pre[i]);
	printf("\n");
#endif
}

static void be_ut_reg_area_merge_in(struct m0_be_reg_area *ra)
{
	int i;

	memcpy(be_ut_ra_merged, be_ut_ra_merge_pre,
	       ARRAY_SIZE(be_ut_ra_merged));
	be_ut_reg_area_arr_copy(be_ut_ra_merged, ARRAY_SIZE(be_ut_ra_merged),
				ra, false);
#if BE_UT_RA_MERGE_DEBUG
	printf("merged:     ");
	for (i = 0; i < ARRAY_SIZE(be_ut_ra_merged); ++i)
		printf("%4.u", (unsigned) be_ut_ra_merged[i]);
	printf("\n");
#endif
	for (i = 0; i < ARRAY_SIZE(be_ut_ra_merged); ++i) {
		M0_UT_ASSERT(ergo(be_ut_ra_merge_pre[i] != 0,
				  be_ut_ra_merged[i] != 0));
	}
}

static void be_ut_reg_area_merge_post(struct m0_be_reg_area *ra)
{
	int cmp;
#if BE_UT_RA_MERGE_DEBUG
	int i;
#endif

	be_ut_reg_area_arr_copy(be_ut_ra_merge_post,
				ARRAY_SIZE(be_ut_ra_merge_post), ra, true);
#if BE_UT_RA_MERGE_DEBUG
	printf("merge_post: ");
	for (i = 0; i < ARRAY_SIZE(be_ut_ra_merge_post); ++i)
		printf("%4.u", (unsigned) be_ut_ra_merge_post[i]);
	printf("\n\n");
#endif
	/*
	 * check if arrays merge is equal to merge using
	 * m0_be_reg_area_merge_in().
	 */
	cmp = memcmp(be_ut_ra_merged, be_ut_ra_merge_post,
		     ARRAY_SIZE(be_ut_ra_merged));
	M0_UT_ASSERT(cmp == 0);
}

void m0_be_ut_reg_area_merge(void)
{
	static struct m0_be_reg_area ra;
	static struct m0_be_reg_area mra[BE_UT_RA_MERGE_NR]; /* merge ra */
	struct m0_be_tx_credit	     prepared_ra = {};
	struct m0_be_tx_credit	     prepared_mra;
	struct m0_be_ut_seg	     ut_seg;
#if BE_UT_RA_MERGE_DEBUG
	unsigned char		     mra_arr[BE_UT_RA_MERGE_SIZE_TOTAL];
	int			     k;
#endif
	int			     i;
	int			     j;
	int			     rc;

	m0_be_ut_seg_init(&ut_seg, NULL, BE_UT_RA_MERGE_SEG_SIZE);
	be_ut_ra_merge_seg = ut_seg.bus_seg;

	prepared_mra = M0_BE_TX_CREDIT(BE_UT_RA_MERGE_R_NR_MAX,
				       BE_UT_RA_MERGE_SIZE_TOTAL);
	m0_be_tx_credit_mac(&prepared_ra, &prepared_mra, BE_UT_RA_MERGE_NR);

	rc = m0_be_reg_area_init(&ra, &prepared_ra, M0_BE_REG_AREA_DATA_NOCOPY);
	M0_UT_ASSERT(rc == 0);
	for (i = 0; i < ARRAY_SIZE(mra); ++i) {
		m0_be_reg_area_init(&mra[i], &prepared_mra,
				    M0_BE_REG_AREA_DATA_COPY);
		M0_UT_ASSERT(rc == 0);
	}

	be_ut_ra_merge_seed = 0;
	for (j = 0; j < BE_UT_RA_MERGE_ITER; ++j) {
		/* reset all data structures */
		m0_be_reg_area_reset(&ra);
		for (i = 0; i < ARRAY_SIZE(mra); ++i)
			m0_be_reg_area_reset(&mra[i]);
		/* create random reg_areas */
		for (i = 0; i < ARRAY_SIZE(mra); ++i)
			be_ut_reg_area_merge_rand_ra(&mra[i]);
		/* merge it with ra */
		for (i = 0; i < ARRAY_SIZE(mra); ++i) {
#if BE_UT_RA_MERGE_DEBUG
			printf("i = %d\n", i);
			be_ut_reg_area_arr_copy(mra_arr, ARRAY_SIZE(mra_arr),
						&mra[i], true);
			printf("mra_arr:    ");
			for (k = 0; k < ARRAY_SIZE(mra_arr); ++k)
				printf("%4.u", (unsigned) mra_arr[k]);
			printf("\n");
#endif
			be_ut_reg_area_merge_pre(&ra);
			m0_be_reg_area_merge_in(&ra, &mra[i]);
			be_ut_reg_area_merge_in(&mra[i]);
			be_ut_reg_area_merge_post(&ra);
		}
	}

	m0_be_reg_area_fini(&ra);
	for (i = 0; i < ARRAY_SIZE(mra); ++i)
		m0_be_reg_area_fini(&mra[i]);

	m0_be_ut_seg_fini(&ut_seg);
}

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
