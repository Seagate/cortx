/* -*- C -*- */
/*
 * COPYRIGHT 2018 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original authors: Ajay Nair       <ajay.nair@seagate.com>
 *                   Ujjwal Lanjewar <ujjwal.lanjewar@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 05-July-2017
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"             /* M0_LOG */

#include "ut/ut.h"                 /* M0_UT_ASSERT */
#include "lib/finject.h"
#include "lib/tlist.h"
#include "lib/hash.h"
#include "lib/memory.h"

#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"

#include "helpers/ufid.h"
#include "helpers/ufid.c"


struct m0_ut_suite ut_suite_ufid;
static struct m0_clovis dummy_m0c;
static struct m0_ufid_generator dummy_ufid_gr;

enum {
 	UFID_ERR_RESERVED = 1,
	UFID_ERR_PROC_ID,
	UFID_ERR_SALT,
	UFID_ERR_GEN_ID,
	UFID_ERR_SEQ_ID,
};

#define EXPECT_EQ(a, b) M0_UT_ASSERT(a == b)
#define EXPECT_NE(a, b) M0_UT_ASSERT(a != b)

#define UFID_RESERVED_TEST (60818UL)
#define UFID_RESERVED_TEST_HI \
	(UFID_RESERVED_TEST << (64 - M0_UFID_RESERVED_BITS))

static void id128_to_ufid(struct m0_uint128 *id128,
			  struct m0_ufid *ufid)
{
	uint64_t genid_hi;
	uint64_t genid_lo;
	uint64_t salt;
	uint64_t proc_id;
	uint64_t seq_id;
	uint64_t id_lo;
	uint64_t id_hi;
	uint64_t reserved;

	id_hi = id128->u_hi;
	id_lo = id128->u_lo;

	seq_id = id_lo & M0_UFID_SEQID_MASK;
	id_lo >>= M0_UFID_SEQID_BITS;
	proc_id = id_lo & M0_UFID_PROCID_MASK;
	id_lo >>= M0_UFID_PROCID_BITS;
	genid_lo = id_lo & M0_UFID_GENID_LO_MASK;

	genid_hi = id_hi & M0_UFID_GENID_HI_MASK;
	id_hi >>= M0_UFID_GENID_HI_BITS;
	salt = id_hi & M0_UFID_SALT_MASK;
	id_hi >>= M0_UFID_SALT_BITS;
	reserved = id_hi & M0_UFID_RESERVED_MASK;

	ufid->uf_seq_id = seq_id;
	ufid->uf_proc_id = proc_id;
	ufid->uf_gen_id = (genid_hi << M0_UFID_GENID_HI_BITS) | genid_lo;
	ufid->uf_salt = salt;
	ufid->uf_reserved = reserved;
}

static int ufid_validate(struct m0_uint128 *id128)
{
	struct m0_ufid ufid;
	struct m0_ufid current_ufid;

	id128_to_ufid(id128, &ufid);
	current_ufid = dummy_ufid_gr.ufg_ufid_cur;

	/* gen ID should always be increasing */
	if (ufid.uf_gen_id == 0 || ufid.uf_gen_id < current_ufid.uf_gen_id)
    		return UFID_ERR_GEN_ID;

	/* seq ID should increase for same gen ID */
	if (ufid.uf_gen_id == current_ufid.uf_gen_id &&
	    ufid.uf_seq_id <= current_ufid.uf_seq_id)
		return UFID_ERR_SEQ_ID;

	/* Salt ID should remain same for the process */
	  if (current_ufid.uf_salt != 0 && ufid.uf_salt != current_ufid.uf_salt)
		return UFID_ERR_SALT;

	  /* Process Id should remain constant till the process dies */
	if (current_ufid.uf_proc_id != 0 &&
	    ufid.uf_proc_id != current_ufid.uf_proc_id)
		return UFID_ERR_PROC_ID;

	  /* Mero bits should not be altered */
	  if (ufid.uf_reserved != 0 && ufid.uf_reserved != UFID_RESERVED_TEST)
		return UFID_ERR_RESERVED;

	return 0;
}
#define UFID_VALIDATE(id) EXPECT_EQ(ufid_validate(id), 0)

struct ut_ufid {
	uint64_t          u_magic;
	struct m0_hlink   u_link;

	struct m0_uint128 u_id128;
};

static uint64_t ufid_hash(const struct m0_htable *htable,
			  const struct m0_uint128 *id128)
{
	const uint64_t  seq_id = id128->u_lo & M0_UFID_SEQID_MASK;
	return seq_id % htable->h_bucket_nr;
}

static int ufid_hash_eq(const struct m0_uint128 *id1,
			const struct m0_uint128 *id2)
{
	return m0_uint128_eq(id1, id2);
}

M0_HT_DESCR_DEFINE(ufid, "Hash of UFIDs",
		   static, struct ut_ufid,
		   u_link, u_magic, 0x43, 0x67, u_id128,
		   ufid_hash, ufid_hash_eq);
M0_HT_DEFINE(ufid, static, struct ut_ufid, struct m0_uint128);
static struct m0_htable ufid_tracker;

static void ut_ufid_seq_id_refresh(void)
{
	int                       i;
	int                       nr_tests;
	struct ut_ufid           *new_ufid;
	struct ut_ufid           *found_ufid;
	struct ut_ufid           *ufid;
	struct m0_ufid_generator *gr = &dummy_ufid_gr;

	ufid_htable_init(&ufid_tracker, 1024);

#ifdef M0_ASSERT_EX_ON
	/*
	 * Hash table invariant checks take too long if they are on. Reduce
	 * the number of tests for sequence id overflow test.
	 */
	nr_tests = 10 * 1024;
	gr->ufg_ufid_cur.uf_seq_id = M0_UFID_SEQID_MAX - 1024;
#else
	nr_tests = M0_UFID_SEQID_MAX + 1024;
#endif
	for (i = 0; i < nr_tests; i++) {
		ufid_seq_id_refresh(gr, 1);

		M0_ALLOC_PTR(new_ufid);
		M0_UT_ASSERT(new_ufid != NULL);
		ufid_to_id128(&gr->ufg_ufid_cur, &new_ufid->u_id128);

		found_ufid = m0_htable_lookup(&ufid_tracker,
					      &new_ufid->u_id128);
		M0_UT_ASSERT(found_ufid == NULL);
		m0_tlink_init(&ufid_tl, new_ufid);
		ufid_htable_add(&ufid_tracker, new_ufid);
	}

	m0_htable_for(ufid, ufid, &ufid_tracker) {
		m0_htable_del(&ufid_tracker, ufid);
		m0_free(ufid);
	}
	m0_htable_endfor;

	ufid_htable_fini(&ufid_tracker);
}

static void ut_ufid_salt_refresh(void)
{
	int                       i;
	int                       nr_tests = 100;
	uint32_t                  prev_salt;
	uint32_t                  curr_salt;
	struct m0_ufid_generator *gr = &dummy_ufid_gr;

	ufid_salt_refresh(gr);
	prev_salt = gr->ufg_ufid_cur.uf_salt;
	for (i = 0; i < nr_tests; i++) {
		ufid_salt_refresh(gr);
		curr_salt = gr->ufg_ufid_cur.uf_salt;
		EXPECT_NE(curr_salt, prev_salt);
		prev_salt = curr_salt;
	}
}

static void ut_ufid_proc_id_refresh(void)
{
	struct m0_ufid_generator *gr = &dummy_ufid_gr;

	/* Basic case. */
	EXPECT_EQ(ufid_proc_id_refresh(gr), 0);

	/* Process ID warn bit set should return warning. */
	m0_fi_enable_once("ufid_proc_id_refresh", "proc_id_warn");
	EXPECT_EQ(ufid_proc_id_refresh(gr), 0);

	/* Process ID overflow detection. */
	m0_fi_enable_once("ufid_proc_id_refresh", "proc_id_overflow");
	EXPECT_EQ(ufid_proc_id_refresh(gr), -EOVERFLOW);
}

static void ut_ufid_generation_id_refresh(void)
{
	struct m0_ufid_generator *gr = &dummy_ufid_gr;

	/* Basic case. */
	EXPECT_EQ(ufid_generation_id_refresh(gr), 0);

	/* Refresh should fail after retry is exhausted. */
	m0_fi_enable_once("ufid_generation_id_refresh",
			  "retries_exhausted");
	EXPECT_EQ(ufid_generation_id_refresh(gr), -ETIME);

	/* Refresh should fail if clock is set earlier than base time. */
	m0_fi_enable_once("ufid_generation_id_refresh",
			  "clock_lt_base_ts");
	EXPECT_EQ(ufid_generation_id_refresh(gr), -ETIME);

	/* Refresh should fail for large clock skew */
	m0_fi_enable_once("ufid_generation_id_refresh",
			  "clock_skew");
  	EXPECT_EQ(ufid_generation_id_refresh(gr), -ETIME);
}

static void ut_m0_ufid_next(void)
{
	struct m0_uint128         id128;
	struct m0_ufid_generator *gr = &dummy_ufid_gr;

	M0_SET0(&id128);

	/* Base cases*/
	EXPECT_EQ(m0_ufid_next(gr, 1, &id128), 0);
	UFID_VALIDATE(&id128);

	/* Call again */
	EXPECT_EQ(m0_ufid_next(gr, 1, &id128), 0);
	UFID_VALIDATE(&id128);

	/* Get range */
	EXPECT_EQ(m0_ufid_next(gr, 100, &id128), 0);
	UFID_VALIDATE(&id128);

	EXPECT_EQ(m0_ufid_next(gr, M0_UFID_SEQID_MAX, &id128), 0);
	UFID_VALIDATE(&id128);

	/* num_seq is 0 */
	EXPECT_EQ(m0_ufid_next(gr, 0, &id128), -EINVAL);

	/* Greater than 2^20 */
	EXPECT_EQ(m0_ufid_next(gr, M0_UFID_SEQID_MAX + 1, &id128), -EINVAL);

	/* id is higer than 91 bits set */
	id128 = M0_UINT128(UFID_RESERVED_TEST_HI, 0);
	EXPECT_EQ(m0_ufid_next(gr, 1, &id128), 0);
	UFID_VALIDATE(&id128);
}

static void ut_m0_ufid_new(void)
{
	struct m0_uint128         id128;
	struct m0_ufid_generator *gr = &dummy_ufid_gr;
	struct m0_ufid            ufid;

	M0_SET0(&id128);

	/* Basic test */
	EXPECT_EQ(m0_ufid_new(gr, 1, 0, &id128), 0);
	UFID_VALIDATE(&id128);
	EXPECT_EQ(m0_ufid_new(gr, 100, 100, &id128), 0);
	UFID_VALIDATE(&id128);

	/* Greater than 2^20 */
	EXPECT_EQ(m0_ufid_new(gr, 1, M0_UFID_SEQID_MAX, &id128), 0);
	UFID_VALIDATE(&id128);

	/* Negative Cases. */
	/* Requested no. of FIDs should be between 1 and UFID_SEQID_MAX */
	EXPECT_EQ(m0_ufid_new(gr, 0, 100, &id128), -EINVAL);
	EXPECT_EQ(m0_ufid_new(gr, M0_UFID_SEQID_MAX + 1, 100, &id128),
		  -EINVAL);

	/* Check rollover. After rollover sequence ID should be 0 */
	EXPECT_EQ(m0_ufid_new(gr, 1, M0_UFID_SEQID_MAX, &id128), 0);
	id128_to_ufid(&id128, &ufid);
	EXPECT_EQ(ufid.uf_seq_id, 0);
}

M0_INTERNAL int ut_ufid_init(void)
{
	dummy_m0c.m0c_process_fid.f_container = 0x7200000000000000;
	dummy_m0c.m0c_process_fid.f_key       = 0x1;
	m0_ufid_init(&dummy_m0c, &dummy_ufid_gr);

	return 0;
}

M0_INTERNAL int ut_ufid_fini(void)
{
	m0_ufid_fini(&dummy_ufid_gr);
	return 0;
}

struct m0_ut_suite ut_suite_ufid = {
	.ts_name = "helpers-ufid-ut",
	.ts_init = ut_ufid_init,
	.ts_fini = ut_ufid_fini,
	.ts_tests = {
		{ "m0_ufid_new",
			&ut_m0_ufid_new},
		{ "m0_ufid_next",
			&ut_m0_ufid_next},
		{ "ufid_generation_id_refresh",
			&ut_ufid_generation_id_refresh},
		{ "ufid_proc_id_refresh",
			&ut_ufid_proc_id_refresh},
		{ "ufid_salt_refresh",
			&ut_ufid_salt_refresh},
		{ "ufid_seq_id_refresh",
			&ut_ufid_seq_id_refresh},
		{ NULL, NULL },
	}
};

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
