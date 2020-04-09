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
 * Original creation date: 05/21/2013
 */

/**
 * @addtogroup hash
 * @{
 */

#include "lib/bob.h"	/* m0_bob_type */
#include "lib/hash.h"   /* m0_htable */
#include "lib/errno.h"  /* Include appropriate errno.h header. */
#include "lib/arith.h"	/* min64u() */
#include "lib/memory.h" /* M0_ALLOC_ARR() */
#include "lib/misc.h"	/* m0_forall() */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

static const struct m0_bob_type htable_bobtype;
M0_BOB_DEFINE(static, &htable_bobtype, m0_htable);

static const struct m0_bob_type htable_bobtype = {
	.bt_name         = "hashtable",
	.bt_magix_offset = offsetof(struct m0_htable, h_magic),
	.bt_magix        = M0_LIB_HASHLIST_MAGIC,
	.bt_check        = NULL,
};

static bool htable_invariant(const struct m0_htable *htable);

static void hbucket_init(const struct m0_ht_descr *d,
			 struct m0_hbucket        *bucket)
{
	M0_PRE(bucket != NULL);
	M0_PRE(d != NULL);

	m0_tlist_init(d->hd_tldescr, &bucket->hb_objects);
	m0_mutex_init(&bucket->hb_mutex);
}

static void hbucket_fini(const struct m0_ht_descr *d,
			 struct m0_hbucket        *bucket)
{
	M0_PRE(bucket != NULL);
	M0_PRE(d != NULL);

	m0_tlist_fini(d->hd_tldescr, &bucket->hb_objects);
	m0_mutex_fini(&bucket->hb_mutex);
}

static void *obj_key(const struct m0_ht_descr *hd, void *obj)
{
	return obj + hd->hd_key_offset;
}

static bool hbucket_invariant(const struct m0_ht_descr *desc,
			      const struct m0_hbucket  *bucket,
			      const struct m0_htable   *htable)
{
	uint64_t  index;
	void     *amb;

	index = bucket - htable->h_buckets;

	return
		bucket != NULL &&
		desc != NULL &&
		m0_hbucket_forall_ol(desc->hd_tldescr, amb, bucket,
				     index == desc->hd_hash_func(htable,
				     obj_key(desc, amb)));
}

static bool htable_invariant(const struct m0_htable *htable)
{
	return
		m0_htable_bob_check(htable) &&
		htable->h_bucket_nr >  0 &&
		htable->h_buckets   != NULL &&
		m0_forall(i, htable->h_bucket_nr,
			  hbucket_invariant(htable->h_descr,
					    &htable->h_buckets[i], htable));
}

M0_INTERNAL int m0_htable_init(const struct m0_ht_descr *d,
			       struct m0_htable         *htable,
			       uint64_t                  bucket_nr)
{
	uint64_t nr;

	M0_PRE(htable != NULL);
	M0_PRE(d != NULL);
	M0_PRE(bucket_nr > 0);

	m0_htable_bob_init(htable);

	htable->h_descr     = d;
	htable->h_bucket_nr = bucket_nr;
	M0_ALLOC_ARR(htable->h_buckets, htable->h_bucket_nr);
	if (htable->h_buckets == NULL)
		return M0_ERR(-ENOMEM);

	for (nr = 0; nr < htable->h_bucket_nr; ++nr)
		hbucket_init(d, &htable->h_buckets[nr]);
	M0_POST_EX(htable_invariant(htable));
	return 0;
}

M0_INTERNAL bool m0_htable_is_init(const struct m0_htable *htable)
{
	return htable_invariant(htable);
}

M0_INTERNAL void m0_htable_add(struct m0_htable *htable,
			       void             *amb)
{
	uint64_t bucket_id;

	M0_PRE_EX(htable_invariant(htable));
	M0_PRE(amb != NULL);
	M0_PRE(!m0_tlink_is_in(htable->h_descr->hd_tldescr, amb));

	bucket_id = htable->h_descr->hd_hash_func(htable,
			obj_key(htable->h_descr, amb));

	m0_tlist_add(htable->h_descr->hd_tldescr,
		     &htable->h_buckets[bucket_id].hb_objects, amb);
	M0_POST_EX(htable_invariant(htable));
	M0_POST(m0_tlink_is_in(htable->h_descr->hd_tldescr, amb));
}

M0_INTERNAL void m0_htable_del(struct m0_htable *htable,
			       void             *amb)
{
	M0_PRE_EX(htable_invariant(htable));
	M0_PRE(amb != NULL);

	m0_tlist_del(htable->h_descr->hd_tldescr, amb);

	M0_POST_EX(htable_invariant(htable));
	M0_POST(!m0_tlink_is_in(htable->h_descr->hd_tldescr, amb));
}

M0_INTERNAL void *m0_htable_lookup(const struct m0_htable *htable,
				   const void             *key)
{
	void     *scan;
	uint64_t  bucket_id;

	M0_PRE_EX(htable_invariant(htable));

	bucket_id = htable->h_descr->hd_hash_func(htable, key);

	m0_tlist_for(htable->h_descr->hd_tldescr,
		     &htable->h_buckets[bucket_id].hb_objects, scan) {
		if (htable->h_descr->hd_key_eq(obj_key(htable->h_descr, scan),
					       key))
			break;
	} m0_tlist_endfor;

	return scan;
}

M0_INTERNAL void m0_htable_cc_add(struct m0_htable *htable,
		                  void             *amb)
{
	M0_PRE(amb != NULL);

	m0_hbucket_lock(htable, obj_key(htable->h_descr, amb));
	m0_htable_add(htable, amb);
	m0_hbucket_unlock(htable, obj_key(htable->h_descr, amb));
}

M0_INTERNAL void m0_htable_cc_del(struct m0_htable *htable,
		                  void             *amb)
{
	M0_PRE(amb != NULL);

	m0_hbucket_lock(htable, obj_key(htable->h_descr, amb));
	m0_htable_del(htable, amb);
	m0_hbucket_unlock(htable, obj_key(htable->h_descr, amb));
}

M0_INTERNAL void *m0_htable_cc_lookup(struct m0_htable *htable,
		                      const void  *key)
{
	void *obj;

	M0_PRE(key != NULL);

	m0_hbucket_lock(htable, key);
	obj = m0_htable_lookup(htable, key);
	m0_hbucket_unlock(htable, key);
	return obj;
}

M0_INTERNAL void m0_hbucket_lock(struct m0_htable *htable,
				 const void       *key)
{
	uint64_t bucket_id;

	M0_PRE_EX(htable_invariant(htable));

	bucket_id = htable->h_descr->hd_hash_func(htable, key);
	m0_mutex_lock(&htable->h_buckets[bucket_id].hb_mutex);
}

M0_INTERNAL void m0_hbucket_unlock(struct m0_htable *htable,
				   const void       *key)
{
	uint64_t bucket_id;

	M0_PRE_EX(htable_invariant(htable));

	bucket_id = htable->h_descr->hd_hash_func(htable, key);
	m0_mutex_unlock(&htable->h_buckets[bucket_id].hb_mutex);
}

M0_INTERNAL void m0_htable_fini(struct m0_htable *htable)
{
	uint64_t nr;

	M0_PRE_EX(htable_invariant(htable));

	for (nr = 0; nr < htable->h_bucket_nr; ++nr)
		hbucket_fini(htable->h_descr, &htable->h_buckets[nr]);
	m0_free(htable->h_buckets);
	m0_htable_bob_fini(htable);
	htable->h_buckets   = NULL;
	htable->h_bucket_nr = 0;
	htable->h_descr     = NULL;
}

M0_INTERNAL bool m0_htable_is_empty(const struct m0_htable *htable)
{
	uint64_t nr;

	M0_PRE_EX(htable_invariant(htable));

	for (nr = 0; nr < htable->h_bucket_nr; ++nr) {
		if (!m0_tlist_is_empty(htable->h_descr->hd_tldescr,
				&htable->h_buckets[nr].hb_objects))
			break;
	}
	return nr == htable->h_bucket_nr;
}

M0_INTERNAL uint64_t m0_htable_size(const struct m0_htable *htable)
{
	uint64_t nr;
	uint64_t len = 0;

	M0_PRE_EX(htable_invariant(htable));

	for (nr = 0; nr < htable->h_bucket_nr; ++nr)
		len += m0_tlist_length(htable->h_descr->hd_tldescr,
				&htable->h_buckets[nr].hb_objects);
	return len;
}

M0_INTERNAL uint64_t m0_hash(uint64_t x)
{
	uint64_t y;

	y = x;
	y <<= 18;
	x -= y;
	y <<= 33;
	x -= y;
	y <<= 3;
	x += y;
	y <<= 3;
	x -= y;
	y <<= 4;
	x += y;
	y <<= 2;

	return x + y;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of hash */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
