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

#pragma once

#ifndef __MERO_LIB_HASH_H__
#define __MERO_LIB_HASH_H__

#include "lib/types.h"
#include "lib/tlist.h"
#include "lib/mutex.h"

/**
 * @defgroup hash Hash table.
 *
 * Hash table module provides a simple hash implementation built on
 * top of typed lists. @see tlist.
 *
 * Often, lookup for objects stored in simple tlists proves to be expensive
 * owing to lack of any efficient arrangement of objects in tlist.
 * Hash table provides a simple way to distribute objects in hash using a
 * key-value mechanism, which enhances lookup time of objects.
 *
 * Hash table contains array of hash buckets which contain a simple tlist
 * of ambient objects.
 * Every object is supposed to provide a key, based on which its location
 * in hash table is decided. The caller is supposed to provide a hash function
 * which is used to calculate bucket id in which object will lie.
 *
 * Hash keys and ambient objects are generic (kept as void *) in hash code
 * so as to support any type of keys and objects.
 *
 * Users are encouraged to use the type-safe interfaces defined over hash
 * table using macros like M0_HT_DEFINE().
 *
 * Similar to tlists, hash table is a simple algorithmic module. It does not
 * deal with liveness or concurrency and other such issues.
 * Caller is supposed to control liveness and use proper synchronization
 * primitives to handle concurrency.
 *
 * A good hash function can ensure good distribution of objects
 * throughout the hash table, thus owing to efficient operation of hash.
 *
 * Consider a scenario with struct bar containing multiple objects of
 * struct foo.
 *
 * @code
 *
 * struct bar {
 *         ...
 *         // Hash table used to store multiple foo-s.
 *         struct m0_htable b_foohash;
 *         ...
 * };
 *
 * struct foo {
 *         // Magic to validate sanity of object.
 *         uint64_t        f_magic;
 *
 *         // A uint64_t Key used to find out appropriate bucket.
 *         uint64_t        f_hash_key;
 *
 *         ...
 *         // hash linkage to keep foo structures as part of some list
 *         // in hash table.
 *         struct m0_hlink f_link;
 * };
 *
 * - Define a hash function which will take care of distributing objects
 *   throughtout the hash buckets and the key retrieval and key equal
 *   functions.
 *
 *   uint64_t hash_func(const struct m0_htable *htable, const void *key)
 *   {
 *           const uint64_t *k = key;
 *           return *k % htable->h_bucket_nr;
 *   }
 *
 *   bool hash_key_eq(const void *key1, const void *key2)
 *   {
 *           const uint64_t *k1 = key1;
 *           const uint64_t *k2 = key2;
 *           return *k1 == *k2;
 *   }
 *
 * - Now define hash descriptor like this.
 *
 *   M0_HT_DESCR_DEFINE(foohash, "Hash of foo structures", static, struct foo,
 *                      f_link, f_magic, FOO_MAGIC, BAR_MAGIC, f_hash_key,
 *                      hash_func, hash_key_eq);
 *
 *   This will take care of defining tlist descriptor internally.
 *
 *   Similarly,
 *
 *   M0_HT_DEFINE(foohash, static, struct foo, uint64_t);
 *
 *   this will take care of defining tlist APIs internally.
 *
 * - Now initialize the m0_htable like
 *
 *   m0_htable_init(&foohash_tl, &bar->b_foohash, bucket_nr);
 *   OR
 *   foohash_htable_init(&bar->b_foohash, bucket_nr);
 *
 *   Note that this initializes a struct that you must have previously
 *   defined yourself.
 *
 *   For a freestanding m0_htable of type `foohash` do:
 *
 *   struct m0_htable my_foohash;
 *   foohash_htable_init(&my_foohash, bucket_nr);
 *
 * Now, foo objects can be added/removed to/from bar::b_foohash using
 * APIs like m0_htable_add() and m0_htable_del().
 *
 * Also, lookup through hash can be done using API like m0_htable_lookup().
 *
 * @endcode
 *
 * Macros like m0_hbucket_forall() and m0_htable_forall() can be used
 * to evaluate a certain expression for all objects in hashbucket/hashtable.
 *
 * m0_htable_for() and m0_htable_endfor() can be used to have a loop
 * over all objects in hashtable.
 *
 * @{
 */

struct m0_htable;
struct m0_hbucket;
struct m0_ht_descr;

/**
 * Represents a simple hash bucket.
 */
struct m0_hbucket {
	/**
	 * A lock to guard concurrent access to the list of objects.
	 */
	struct m0_mutex hb_mutex;
	/**
	 * List of objects which lie in same hash bucket.
	 * A single m0_tl_descr object would be used by all
	 * m0_hbucket::hb_objects lists in a single m0_hash object.
	 */
	struct m0_tl    hb_objects;
};

/**
 * A simple hash data structure which helps to avoid the linear search
 * of whole list of objects.
 */
struct m0_htable {
	/** Magic value. Holds M0_LIB_HASHLIST_MAGIC.  */
	uint64_t                  h_magic;

	/** Number of hash buckets used. */
	uint64_t                  h_bucket_nr;

	/**
	 * Array of hash buckets.
	 * Hash buckets are supposed to be indexed in increasing order of
	 * bucket id retrieved using hash function.
	 */
	struct m0_hbucket        *h_buckets;

	/** Associated hash table descriptor. */
	const struct m0_ht_descr *h_descr;
};

/**
 * Hash table descriptor. An instance of this type must be defined per
 * hash table type. Multiple instances of m0_htable can share a single
 * descriptor.
 * It keeps track of tlist descriptor, offset to key field and hash function.
 */
struct m0_ht_descr {
	/** tlist descriptor used for m0_hbucket::hb_objects tlist. */
	const struct m0_tl_descr *hd_tldescr;

	/** Offset to key field in ambient object. */
	size_t                    hd_key_offset;

	/** Hash function. Has to be provided by user. */
	uint64_t (*hd_hash_func) (const struct m0_htable *htable,
			          const void             *key);

	/**
	 * Key comparison routine. Since hash component supports
	 * custom made keys, the comparison routine has to be
	 * provided by user.
	 */
	bool (*hd_key_eq)        (const void *key1, const void *key2);
};

/**
 * Wrapper over m0_tlink to align with m0_htable.
 * Every ambient object which is supposed to be a member of hash table,
 * is supposed to have a m0_hlink member.
 */
struct m0_hlink {
	struct m0_tlink hl_link;
};

/**
 * Initializes a hashtable.
 * @param bucket_nr Number of buckets that will be housed in this m0_htable.
 * @param d tlist descriptor used for tlist in hash buckets.
 * @pre   htable != NULL &&
 *        bucket_nr > 0    &&
 *        d != NULL.
 * @post  htable->h_magic == M0_LIB_HASHLIST_MAGIC &&
 *        htable->h_bucket_nr > 0 &&
 *        htable->h_buckets != NULL.
 */
M0_INTERNAL int m0_htable_init(const struct m0_ht_descr *d,
			       struct m0_htable         *htable,
			       uint64_t                  bucket_nr);

/* Checks if hash-table is initialised. */
M0_INTERNAL bool m0_htable_is_init(const struct m0_htable *htable);

/**
 * Finalizes a struct m0_htable.
 * @pre  htable != NULL &&
 *       htable->h_magic == M0_LIB_HASHLIST_MAGIC &&
 *       htable->h_buckets != NULL.
 * @post htable->buckets == NULL &&
 *       htable->bucket_nr == 0.
 */
M0_INTERNAL void m0_htable_fini(struct m0_htable *htable);

/**
 * Adds an object to hash table.
 * The key must be set in object at specified location in order to
 * identify the bucket.
 * @pre htable != NULL &&
 *      amb    != NULL &&
 *      htable->h_buckets != NULL.
 */
M0_INTERNAL void m0_htable_add(struct m0_htable *htable,
			       void             *amb);
/**
 * Concurrent version of m0_htable_add.
 */
M0_INTERNAL void m0_htable_cc_add(struct m0_htable *htable,
				  void             *amb);
/**
 * Removes an object from hash table.
 * The key must be set in object at specified location in order to
 * identify the bucket.
 * @pre htable != NULL &&
 *      amb    != NULL &&
 *      htable->h_buckets != NULL.
 */
M0_INTERNAL void m0_htable_del(struct m0_htable *htable,
			       void             *amb);

/**
 * Concurrent version of m0_htable_del.
 */
M0_INTERNAL void m0_htable_cc_del(struct m0_htable *htable,
			          void             *amb);

/**
 * Looks up if given object is present in hash table based on input key.
 * Returns ambient object on successful lookup, returns NULL otherwise.
 * @pre d != NULL &&
 *      key != NULL &&
 *      htable != NULL &&
 *      htable->h_buckets != NULL.
 */
M0_INTERNAL void *m0_htable_lookup(const struct m0_htable *htable,
				   const void             *key);

/**
 * Concurrent version of m0_htable_lookup.
 */
M0_INTERNAL void *m0_htable_cc_lookup(struct m0_htable *htable,
				      const void       *key);

/** Returns if m0_htable contains any objects. */
M0_INTERNAL bool m0_htable_is_empty(const struct m0_htable *htable);

/** Returns number of objects stored within m0_htable. */
M0_INTERNAL uint64_t m0_htable_size(const struct m0_htable *htable);

/** Locks the bucket to which the key belongs. */
M0_INTERNAL void m0_hbucket_lock(struct m0_htable *htable,
				 const void       *key);

/** Unlocks the bucket to which the key belongs. */
M0_INTERNAL void m0_hbucket_unlock(struct m0_htable *htable,
				   const void       *key);

/** Defines a hashtable descriptor. */
#define M0_HT_DESCR(name, amb_type, key_field, hash_func, key_eq)	\
{									\
	.hd_tldescr    = &name ## _tl,					\
	.hd_key_eq     = key_eq,					\
	.hd_hash_func  = hash_func,					\
	.hd_key_offset = offsetof(amb_type, key_field),			\
};

/** Defines a hashtable descriptor with given scope. */
#define M0_HT_DESCR_DEFINE(name, htname, scope, amb_type, amb_link_field,\
			   amb_magic_field, amb_magic, head_magic,	\
			   key_field, hash_func, key_eq)		\
									\
M0_BASSERT(sizeof(hash_func(NULL, &M0_FIELD_VALUE(amb_type, key_field))) > 0);\
M0_BASSERT(sizeof(key_eq(&M0_FIELD_VALUE(amb_type, key_field),		\
			 &M0_FIELD_VALUE(amb_type, key_field))) > 0);	\
									\
M0_TL_DESCR_DEFINE(name, htname, scope, amb_type, amb_link_field.hl_link,	\
		   amb_magic_field, amb_magic, head_magic);		\
									\
scope const struct m0_ht_descr name ## _ht = M0_HT_DESCR(name,		\
							 amb_type,	\
							 key_field,	\
	(uint64_t (*)(const struct m0_htable *, const void *))hash_func,\
	(bool (*)(const void *, const void *))key_eq)

/** Declares a hashtable descriptr with given scope. */
#define M0_HT_DESCR_DECLARE(name, scope)				\
scope const struct m0_ht_descr name ## _ht

/**
 * Declares all functions of hash table which accepts ambient type
 * and key type as input.
 */
#define M0_HT_DECLARE(name, scope, amb_type, key_type)			     \
									     \
M0_TL_DECLARE(name, scope, amb_type);                                        \
									     \
scope int name ## _htable_init(struct m0_htable *htable,		     \
			       uint64_t          bucket_nr);		     \
scope void name ## _htable_add(struct m0_htable *htable, amb_type *amb);     \
scope void name ## _htable_del(struct m0_htable *htable, amb_type *amb);     \
scope amb_type *name ## _htable_lookup(const struct m0_htable *htable,	     \
				       const key_type         *key);	     \
scope void name ## _htable_cc_add(struct m0_htable *htable, amb_type *amb);  \
scope void name ## _htable_cc_del(struct m0_htable *htable, amb_type *amb);  \
scope amb_type *name ## _htable_cc_lookup(struct m0_htable *htable,          \
				          const key_type   *key);            \
scope void name ## _hbucket_lock(struct m0_htable *htable,                   \
				 const key_type   *key);                     \
scope void name ## _hbucket_unlock(struct m0_htable *htable,                 \
				   const key_type *key);                     \
scope void name ## _htable_fini(struct m0_htable *htable);		     \
scope bool name ## _htable_is_empty(const struct m0_htable *htable);	     \
scope uint64_t name ## _htable_size(const struct m0_htable *htable);

/**
 * Defines all functions of hash table which accepts ambient type
 * and key type as input.
 */
#define M0_HT_DEFINE(name, scope, amb_type, key_type)			     \
									     \
M0_TL_DEFINE(name, scope, amb_type);					     \
									     \
scope __AUN int name ## _htable_init(struct m0_htable *htable,		     \
				     uint64_t          bucket_nr)	     \
{									     \
	return m0_htable_init(&name ## _ht, htable, bucket_nr);		     \
}									     \
									     \
scope __AUN void name ## _htable_add(struct m0_htable *htable,		     \
				     amb_type         *amb)		     \
{									     \
	m0_htable_add(htable, amb);					     \
}									     \
									     \
scope __AUN void name ## _htable_del(struct m0_htable *htable,		     \
			             amb_type         *amb)		     \
{									     \
	m0_htable_del(htable, amb);					     \
}									     \
									     \
scope __AUN amb_type *name ## _htable_lookup(const struct m0_htable *htable, \
					     const key_type         *key)    \
{									     \
	return m0_htable_lookup(htable, key);				     \
}									     \
									     \
scope __AUN void name ## _hbucket_lock(struct m0_htable *htable,             \
				       const  key_type  *key)                \
{                                                                            \
	m0_hbucket_lock(htable, key);                                        \
}                                                                            \
                                                                             \
scope __AUN void name ## _hbucket_unlock(struct m0_htable *htable,           \
					 const  key_type  *key)              \
{                                                                            \
	m0_hbucket_unlock(htable, key);                                      \
}                                                                            \
                                                                             \
scope __AUN void name ## _htable_cc_add(struct m0_htable *htable,            \
		                        amb_type         *amb)               \
{                                                                            \
	m0_htable_cc_add(htable, amb);                                       \
}                                                                            \
                                                                             \
scope __AUN void name ## _htable_cc_del(struct m0_htable *htable,            \
		                        amb_type          *amb)              \
{                                                                            \
	m0_htable_cc_del(htable, amb);                                       \
}                                                                            \
                                                                             \
scope __AUN amb_type * name ## _htable_cc_lookup(struct m0_htable *htable,   \
		                                 const key_type   *key)      \
{                                                                            \
	return m0_htable_cc_lookup(htable, key);                             \
}                                                                            \
									     \
scope __AUN void name ## _htable_fini(struct m0_htable *htable)		     \
{									     \
	m0_htable_fini(htable);						     \
}									     \
									     \
scope __AUN bool name ## _htable_is_empty(const struct m0_htable *htable)    \
{									     \
	return m0_htable_is_empty(htable);				     \
}									     \
									     \
scope __AUN uint64_t name ## _htable_size(const struct m0_htable *htable)    \
{									     \
	return m0_htable_size(htable);					     \
}									     \

/**
 * Iterates over the members of a m0_hbucket and performs given operation
 * for all of them.
 */
#define m0_hbucket_forall(name, var, bucket, ...)			    \
({									    \
	typeof (bucket) __bucket = (bucket);				    \
									    \
	m0_tl_forall(name, var, &__bucket->hb_objects, ({ __VA_ARGS__ ; }));\
})

/**
 * Iterates over all hashbuckets and invokes m0_hbucket_forall() for all
 * buckets.
 */
#define m0_htable_forall(name, var, htable, ...)			    \
({									    \
	uint64_t cnt;							    \
	typeof (htable) ht = (htable);					    \
									    \
	for (cnt = 0; cnt < ht->h_bucket_nr; ++cnt)	{		    \
		if (!(m0_hbucket_forall(name, var, &ht->h_buckets[cnt],     \
					 ({ __VA_ARGS__ ; }))))	            \
			break;						    \
	}								    \
	cnt == ht->h_bucket_nr;					            \
})

/**
 * An open ended version of loop over all objects in all hash buckets
 * in a m0_htable.
 * This loop has to be closed using m0_htable_endfor macro.
 */
#define m0_htable_for(name, var, htable)				    \
({									    \
	uint64_t __cnt;							    \
	typeof (htable) ht = (htable);					    \
									    \
	for (__cnt = 0; __cnt < ht->h_bucket_nr; ++__cnt) {		    \
		m0_tl_for(name, &ht->h_buckets[__cnt].hb_objects, var)

#define m0_htable_endfor m0_tl_endfor; }; })

/**
 * Open ended version of loop over all ambient objects in a given
 * hash bucket.
 * The loop has to be closed using m0_hbucket_endfor;
 */
#define m0_hbucket_for(descr, var, bucket)				    \
({									    \
	m0_tlist_for (descr, &bucket->hb_objects, var)

#define m0_hbucket_endfor m0_tlist_endfor; })

/**
 * A loop which uses open ended version of hash bucket.
 * This can be used for invariant checking.
 */
#define m0_hbucket_forall_ol(descr, var, bucket, ...)			    \
({									    \
	typeof(descr) d = descr;					    \
	m0_hbucket_for (d, var, bucket) {				    \
		if (!({ __VA_ARGS__; }))				    \
			break;						    \
	} m0_hbucket_endfor;						    \
	var == NULL;							    \
})

/** Hash based on multiplicative cache. */
M0_INTERNAL uint64_t m0_hash(uint64_t x);

/** @} end of hash */

#endif /* __MERO_LIB_HASH_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
