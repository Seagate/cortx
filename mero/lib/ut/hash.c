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
 * Original creation date: 05/28/2013
 */

#include "lib/bob.h"	/* m0_bob_type */
#include "lib/hash.h"   /* m0_htable */
#include "lib/errno.h"  /* Include appropriate errno.h header. */
#include "mero/magic.h"
#include "ut/ut.h"	/* M0_UT_ASSERT() */

/*
 * Once upon a time, there was a hash in a bar, which consists of
 * number of foos!
 * And all foos shared their keys in order to enroll into the hash.
 */
struct bar {
	/* Holds BAR_MAGIC. */
	uint64_t           b_magic;
	int                b_rc;
	struct m0_htable b_hash;
};

struct foo {
	/* Holds FOO_MAGIC. */
	uint64_t        f_magic;
	uint64_t        f_hkey;
	int             f_subject;
	struct m0_hlink f_hlink;
};

enum {
	BUCKET_NR = 8,
	FOO_NR    = 19,
	BAR_MAGIC = 0xa817115ad15ababaULL,
	FOO_MAGIC = 0x911ea3a7096a96e5ULL,
};

static struct foo foos[FOO_NR];
static struct bar thebar;

static uint64_t hash_func(const struct m0_htable *htable, const void *k)
{
	const uint64_t *key  = k;

	return *key % htable->h_bucket_nr;
}

static bool key_eq(const void *key1, const void *key2)
{
	const uint64_t *k1 = key1;
	const uint64_t *k2 = key2;

	return *k1 == *k2;
}

M0_HT_DESCR_DEFINE(foohash, "Hash of fops", static, struct foo,
		   f_hlink, f_magic, FOO_MAGIC, BAR_MAGIC,
		   f_hkey, hash_func, key_eq);

M0_HT_DEFINE(foohash, static, struct foo, uint64_t);

void test_hashtable(void)
{
	int                i;
	int                rc;
	uint64_t           key;
	struct foo        *f;
	struct m0_hbucket *hb;

	for (i = 0; i < FOO_NR; ++i) {
		foos[i].f_magic = FOO_MAGIC;
		foos[i].f_hkey  = i;
		foos[i].f_subject = 0;
		m0_tlink_init(&foohash_tl, &foos[i]);
	}

	thebar.b_magic = BAR_MAGIC;
	thebar.b_rc    = 0;
	rc = foohash_htable_init(&thebar.b_hash, BUCKET_NR);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(thebar.b_hash.h_magic == M0_LIB_HASHLIST_MAGIC);
	M0_UT_ASSERT(thebar.b_hash.h_bucket_nr == BUCKET_NR);
	M0_UT_ASSERT(thebar.b_hash.h_buckets != NULL);

	foohash_htable_add(&thebar.b_hash, &foos[0]);
	M0_UT_ASSERT(foohash_htable_size(&thebar.b_hash) == 1);
	M0_UT_ASSERT(!foohash_htable_is_empty(&thebar.b_hash));
	key = 0;
	M0_UT_ASSERT(foohash_htable_lookup(&thebar.b_hash, &key) == &foos[0]);
	key = 1;
	M0_UT_ASSERT(foohash_htable_lookup(&thebar.b_hash, &key) == NULL);

	M0_UT_ASSERT(!m0_tlist_is_empty(&foohash_tl, &thebar.b_hash.
				        h_buckets[0].hb_objects));

	foohash_htable_del(&thebar.b_hash, &foos[0]);
	M0_UT_ASSERT(foohash_htable_is_empty(&thebar.b_hash));
	M0_UT_ASSERT(foohash_htable_size(&thebar.b_hash) == 0);
	M0_UT_ASSERT(foohash_htable_lookup(&thebar.b_hash, &foos[0].f_hkey) ==
		     NULL);

	for (i = 0; i < FOO_NR; ++i) {
		foohash_htable_add(&thebar.b_hash, &foos[i]);
		M0_UT_ASSERT(m0_tlink_is_in(&foohash_tl, &foos[i]));
	}
	M0_UT_ASSERT(foohash_htable_size(&thebar.b_hash) == FOO_NR);

	for (i = 0; i < BUCKET_NR; ++i) {
		hb = &thebar.b_hash.h_buckets[i];
		M0_UT_ASSERT(!m0_tlist_is_empty(&foohash_tl, &hb->hb_objects));
		M0_UT_ASSERT(m0_hbucket_forall(foohash, f, hb,
			     f->f_hkey % BUCKET_NR == i));
	}
	M0_UT_ASSERT(m0_htable_forall(foohash, f, &thebar.b_hash,
		     f->f_subject == 0));

	m0_htable_for(foohash, f, &thebar.b_hash) {
		f->f_subject = 1;
	} m0_htable_endfor;

	M0_UT_ASSERT(m0_htable_forall(foohash, f, &thebar.b_hash,
		     f->f_subject == 1));

	for (i = 0; i < FOO_NR; ++i) {
		foohash_htable_del(&thebar.b_hash, &foos[i]);
		M0_UT_ASSERT(foohash_htable_size(&thebar.b_hash) ==
			     FOO_NR - (i + 1));
		M0_UT_ASSERT(foohash_htable_lookup(&thebar.b_hash,
					&foos[i].f_hkey) == NULL);
		M0_UT_ASSERT(!m0_tlink_is_in(&foohash_tl, &foos[i]));
	}
	M0_UT_ASSERT(foohash_htable_size(&thebar.b_hash) == 0);
	M0_UT_ASSERT(foohash_htable_is_empty(&thebar.b_hash));

	foohash_htable_fini(&thebar.b_hash);
	M0_UT_ASSERT(thebar.b_hash.h_buckets   == NULL);
	M0_UT_ASSERT(thebar.b_hash.h_bucket_nr == 0);
	M0_UT_ASSERT(thebar.b_hash.h_magic     == 0);
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
