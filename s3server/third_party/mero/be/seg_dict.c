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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 *		    Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 24-Aug-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/seg_dict.h"

#include "lib/errno.h"        /* ENOMEM */
#include "lib/finject.h"      /* M0_FI_ENABLED */
#include "lib/string.h"       /* m0_streq */
#include "mero/magic.h"

#include "be/alloc.h"
#include "be/op.h"            /* M0_BE_OP_SYNC */
#include "be/seg.h"
#include "be/seg_internal.h"  /* m0_be_seg_hdr */
#include "be/tx.h"            /* M0_BE_TX_CAPTURE_PTR */

/**
 * @addtogroup be
 *
 * @{
 */

/* -------------------------------------------------------------------
 * Helpers
 */

struct be_seg_dict_keyval {
	char                   *dkv_key;
	/**
	 * Used to retrieve length of `dkv_key' without additional
	 * m0_be_get().
	 */
	size_t                  dkv_key_len;
	void                   *dkv_val;
	struct m0_be_list_link  dkv_link;
	uint64_t                dkv_magic;
};

M0_BE_LIST_DESCR_DEFINE(be_seg_dict, "be_seg_dict", static,
			struct be_seg_dict_keyval, dkv_link, dkv_magic,
			M0_BE_SEG_DICT_MAGIC, M0_BE_SEG_DICT_HEAD_MAGIC);
M0_BE_LIST_DEFINE(be_seg_dict, static, struct be_seg_dict_keyval);

static struct m0_be_list *be_seg_dict_get(const struct m0_be_seg *seg)
{
	return &((struct m0_be_seg_hdr *) seg->bs_addr)->bh_dict;
}

static void be_seg_dict_kv_get(const struct be_seg_dict_keyval *kv)
{
	/* GET kv */
	/* GET kv->dkv_key */
}

static void be_seg_dict_kv_put(const struct be_seg_dict_keyval *kv)
{
	/* PUT kv->dkv_key */
	/* PUT kv */
}

static struct be_seg_dict_keyval *
be_seg_dict_find(struct m0_be_seg *seg,
		 const char       *user_key,
		 bool (*compare)(const char *dict_key,
				 const char *user_key))
{
	struct m0_be_list         *dict = be_seg_dict_get(seg);
	struct be_seg_dict_keyval *kv;
	struct be_seg_dict_keyval *result = NULL;

	m0_be_list_for(be_seg_dict, dict, kv) {
		be_seg_dict_kv_get(kv);
		if (compare(kv->dkv_key, user_key))
			result = kv;
		be_seg_dict_kv_put(kv);
		if (result != NULL)
			break;
	} m0_be_list_endfor;

	return result;
}

static bool be_seg_dict_kv_eq(const char *dict_key, const char *user_key)
{
	return m0_streq(dict_key, user_key);
}

/* -------------------------------------------------------------------
 * Credits
 */

M0_INTERNAL void m0_be_seg_dict_insert_credit(struct m0_be_seg       *seg,
					      const char             *name,
					      struct m0_be_tx_credit *accum)
{
	struct be_seg_dict_keyval *kv;
	size_t                     buf_len = strlen(name) + 1;

	M0_BE_ALLOC_CREDIT_PTR(kv, seg, accum);
	M0_BE_ALLOC_CREDIT_ARR(name, buf_len, seg, accum);
	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT(1, buf_len));
	m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT_PTR(kv));
	be_seg_dict_be_list_credit(M0_BLO_TLINK_CREATE, 1, accum);
	be_seg_dict_be_list_credit(M0_BLO_ADD, 1, accum);
	/* Failure path. */
	M0_BE_FREE_CREDIT_PTR(kv, seg, accum);
}

M0_INTERNAL void m0_be_seg_dict_delete_credit(struct m0_be_seg       *seg,
					      const char             *name,
					      struct m0_be_tx_credit *accum)
{
	struct be_seg_dict_keyval *kv;
	size_t                     buf_len = strlen(name) + 1;

	be_seg_dict_be_list_credit(M0_BLO_DEL, 1, accum);
	be_seg_dict_be_list_credit(M0_BLO_TLINK_DESTROY, 1, accum);
	M0_BE_FREE_CREDIT_ARR(name, buf_len, seg, accum);
	M0_BE_FREE_CREDIT_PTR(kv, seg, accum);
}

M0_INTERNAL void m0_be_seg_dict_create_credit(struct m0_be_seg       *seg,
					      struct m0_be_tx_credit *accum)
{
	be_seg_dict_be_list_credit(M0_BLO_CREATE, 1, accum);
}

M0_INTERNAL void m0_be_seg_dict_destroy_credit(struct m0_be_seg       *seg,
					       struct m0_be_tx_credit *accum)
{
	be_seg_dict_be_list_credit(M0_BLO_DESTROY, 1, accum);
}

/* -------------------------------------------------------------------
 * Operations
 */

M0_INTERNAL void m0_be_seg_dict_init(struct m0_be_seg *seg)
{
	M0_ENTRY("seg=%p", seg);
	M0_PRE(m0_be_seg__invariant(seg));

	/* NOOP, be/list doesn't have init */

	M0_LEAVE();
}

M0_INTERNAL void m0_be_seg_dict_fini(struct m0_be_seg *seg)
{
	M0_ENTRY("seg=%p", seg);
	M0_PRE(m0_be_seg__invariant(seg));

	/* NOOP, be/list doesn't have fini */

	M0_LEAVE();
}

M0_INTERNAL int m0_be_seg_dict_lookup(struct m0_be_seg  *seg,
				      const char        *name,
				      void             **out)
{
	struct be_seg_dict_keyval *kv;

	M0_ENTRY("seg=%p name='%s'", seg, name);
	kv = be_seg_dict_find(seg, name, be_seg_dict_kv_eq);
	if (kv != NULL) {
		be_seg_dict_kv_get(kv);
		*out = kv->dkv_val;
		be_seg_dict_kv_put(kv);
	}
	return kv == NULL ? M0_RC(-ENOENT) : M0_RC(0);
}

static bool be_seg_dict_kv_begin(const char *dict_key,
				 const char *user_key)
{
	return strcmp(dict_key, user_key) >= 0;
}

static bool be_seg_dict_kv_next(const char *dict_key,
				const char *user_key)
{
	return strcmp(dict_key, user_key) > 0;
}

static int be_seg_dict_iterate(struct m0_be_seg  *seg,
			       const char        *prefix,
			       const char        *start_key,
			       const char       **this_key,
			       void             **this_rec,
			       bool (*compare)(const char *dict_key,
					       const char *user_key))
{
	struct be_seg_dict_keyval *kv;
	int                     rc = -ENOENT;

	kv = be_seg_dict_find(seg, start_key, compare);
	if (kv) {
		be_seg_dict_kv_get(kv);
		if (strstr(kv->dkv_key, "M0_BE:") != NULL &&
		    strstr(kv->dkv_key, prefix) != NULL) {
			rc = 0;
			*this_key = kv->dkv_key;
			*this_rec = kv->dkv_val;
		}
		M0_LOG(M0_DEBUG, "rc=%d, dict_key='%s', prefix='%s'",
		       rc, kv->dkv_key, prefix);
		be_seg_dict_kv_put(kv);
	}
	return M0_RC(rc);
}


M0_INTERNAL int m0_be_seg_dict_begin(struct m0_be_seg *seg,
				     const char *start_key,
				     const char **this_key,
				     void **this_rec)
{
	return be_seg_dict_iterate(seg, start_key, start_key,
				   this_key, this_rec, be_seg_dict_kv_begin);
}

M0_INTERNAL int m0_be_seg_dict_next(struct m0_be_seg *seg,
				    const char *prefix,
				    const char *start_key,
				    const char **this_key,
				    void **this_rec)
{
	return be_seg_dict_iterate(seg, prefix, start_key,
				   this_key, this_rec, be_seg_dict_kv_next);
}

M0_INTERNAL int m0_be_seg_dict_insert(struct m0_be_seg *seg,
				      struct m0_be_tx  *tx,
				      const char       *name,
				      void             *value)
{
	struct m0_be_list         *dict = be_seg_dict_get(seg);
	struct be_seg_dict_keyval *kv;
	struct be_seg_dict_keyval *next;
	size_t                     buf_len = strlen(name) + 1;
	int                        rc = 0;

	M0_ENTRY("seg=%p name='%s'", seg, name);
	M0_PRE(m0_be_seg__invariant(seg));

	if (M0_FI_ENABLED("dict_insert_fail"))
		return M0_RC(-ENOENT);

	/* Check for collision */
	next = be_seg_dict_find(seg, name, be_seg_dict_kv_begin);
	if (next != NULL) {
		be_seg_dict_kv_get(next);
		if (m0_streq(next->dkv_key, name))
			rc = M0_RC(-EEXIST);
		be_seg_dict_kv_put(next);
		if (rc != 0)
			return rc;
	}

	/* Create new entry */

	M0_BE_ALLOC_PTR_SYNC(kv, seg, tx);
	if (kv == NULL)
		return M0_ERR(-ENOMEM);
	/* GET kv */
	M0_BE_ALLOC_ARR_SYNC(kv->dkv_key, buf_len, seg, tx);
	if (kv->dkv_key == NULL) {
		M0_BE_OP_SYNC(op, m0_be_free(m0_be_seg_allocator(seg),
					     tx, &op, kv));
		rc = M0_ERR(-ENOMEM);
	}
	if (rc == 0) {
		/* GET kv->dkv_key */
		strncpy(kv->dkv_key, name, buf_len);
		M0_ASSERT(kv->dkv_key[buf_len - 1] == '\0');
		kv->dkv_key_len = buf_len;
		kv->dkv_val = value;
		M0_BE_TX_CAPTURE_ARR(seg, tx, kv->dkv_key, buf_len);
		M0_BE_TX_CAPTURE_PTR(seg, tx, kv);
		be_seg_dict_be_tlink_create(kv, tx);
		if (next == NULL)
			be_seg_dict_be_list_add_tail(dict, tx, kv);
		else
			be_seg_dict_be_list_add_before(dict, tx, next, kv);
		/* PUT kv->dkv_key */
	}
	/* PUT kv */

	M0_POST(m0_be_seg__invariant(seg));
	return M0_RC(rc);
}

M0_INTERNAL int m0_be_seg_dict_delete(struct m0_be_seg *seg,
				      struct m0_be_tx  *tx,
				      const char       *name)
{
	struct m0_be_list         *dict = be_seg_dict_get(seg);
	struct be_seg_dict_keyval *kv;

	M0_ENTRY("seg=%p name='%s'", seg, name);
	M0_PRE(m0_be_seg__invariant(seg));

	kv = be_seg_dict_find(seg, name, be_seg_dict_kv_eq);
	if (kv == NULL)
		return M0_ERR(-ENOENT);

	be_seg_dict_be_list_del(dict, tx, kv);
	be_seg_dict_be_tlink_destroy(kv, tx);
	be_seg_dict_kv_get(kv);
	M0_BE_OP_SYNC(op, m0_be_free(m0_be_seg_allocator(seg), tx, &op,
				     kv->dkv_key));
	M0_BE_OP_SYNC(op, m0_be_free(m0_be_seg_allocator(seg), tx, &op, kv));
	be_seg_dict_kv_put(kv);

	M0_POST(m0_be_seg__invariant(seg));
	return M0_RC(0);
}

M0_INTERNAL void m0_be_seg_dict_create(struct m0_be_seg *seg,
				       struct m0_be_tx  *tx)
{
	struct m0_be_list *dict = be_seg_dict_get(seg);

	M0_ENTRY("seg=%p", seg);
	M0_PRE(m0_be_seg__invariant(seg));

	be_seg_dict_be_list_create(dict, tx);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_seg_dict_destroy(struct m0_be_seg *seg,
					struct m0_be_tx  *tx)
{
	struct m0_be_list *dict = be_seg_dict_get(seg);

	M0_ENTRY("seg=%p", seg);
	M0_PRE(m0_be_seg__invariant(seg));

	be_seg_dict_be_list_destroy(dict, tx);
	M0_LEAVE();
}

/** @} end of be group */
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
