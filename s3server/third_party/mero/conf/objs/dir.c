/* -*- c -*- */
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
 * Original creation date: 30-Aug-2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/objs/common.h"

static bool dir_check(const void *bob)
{
	const struct m0_conf_dir *self = bob;
	const struct m0_conf_obj *self_obj = &self->cd_obj;

	M0_PRE(m0_conf_obj_type(&self->cd_obj) == &M0_CONF_DIR_TYPE);

	return m0_conf_obj_is_stub(self_obj) ||
		_0C(self_obj->co_parent != NULL);
}

M0_CONF__BOB_DEFINE(m0_conf_dir, M0_CONF_DIR_MAGIC, dir_check);
M0_CONF__INVARIANT_DEFINE(dir_invariant, m0_conf_dir);

static int dir_decode(struct m0_conf_obj        *dest M0_UNUSED,
		      const struct m0_confx_obj *src M0_UNUSED)
{
	M0_IMPOSSIBLE("m0_conf_dir is not supposed to be decoded");
	return -1;
}

static int dir_encode(struct m0_confx_obj      *dest M0_UNUSED,
		      const struct m0_conf_obj *src M0_UNUSED)
{
	M0_IMPOSSIBLE("m0_conf_dir is not supposed to be encoded");
	return -1;
}

static bool dir_match(const struct m0_conf_obj  *cached M0_UNUSED,
		      const struct m0_confx_obj *flat M0_UNUSED)
{
	M0_IMPOSSIBLE("m0_conf_dir should not be compared with m0_confx_obj");
	return false;
}

static bool
belongs(const struct m0_conf_obj *entry, const struct m0_conf_dir *dir)
{
	return  m0_conf_obj_type(entry) == dir->cd_item_type &&
		entry->co_parent == &dir->cd_obj;
}

/**
 * Precondition for m0_conf_obj_ops::coo_readdir().
 *
 * @param dir     The 1st argument of ->coo_readdir(), typecasted.
 * @param entry   The 2nd argument of ->coo_readdir(), dereferenced
 *                before the function is called (*pptr).
 *
 * @see m0_conf_obj_ops::coo_readdir()
 */
static bool
readdir_pre(const struct m0_conf_dir *dir, const struct m0_conf_obj *entry)
{
	return  _0C(dir->cd_obj.co_status == M0_CS_READY) &&
		_0C(dir->cd_obj.co_nrefs > 0) &&
		_0C(ergo(entry != NULL, m0_conf_obj_invariant(entry))) &&
		_0C(ergo(entry != NULL, belongs(entry, dir))) &&
		_0C(ergo(entry != NULL, entry->co_nrefs > 0));
}

/**
 * Postcondition for m0_conf_obj_ops::coo_readdir().
 *
 * @param retval  The value returned by ->coo_readdir().
 * @param dir     The 1st argument of ->coo_readdir(), typecasted.
 * @param entry   The 2nd argument of ->coo_readdir(), dereferenced
 *                after the function is called (*pptr).
 *
 * @see m0_conf_obj_ops::coo_readdir()
 */
static bool readdir_post(int retval, const struct m0_conf_dir *dir,
			 const struct m0_conf_obj *entry)
{
	return  _0C(M0_IN(retval, (M0_CONF_DIREND, M0_CONF_DIRNEXT,
				   M0_CONF_DIRMISS))) &&
		_0C((retval == M0_CONF_DIREND) == (entry == NULL)) &&
		_0C(ergo(entry != NULL, m0_conf_obj_invariant(entry))) &&
		_0C(ergo(entry != NULL, belongs(entry, dir))) &&
		_0C(ergo(entry != NULL,
			 (retval == M0_CONF_DIRNEXT) == (entry->co_nrefs > 0)));
}

static int dir_readdir(const struct m0_conf_obj *dir, struct m0_conf_obj **pptr)
{
	struct m0_conf_obj *next;
	int                 ret;
	struct m0_conf_dir *d = M0_CONF_CAST(dir, m0_conf_dir);
	struct m0_conf_obj *prev = *pptr;

	M0_ENTRY();
	M0_PRE(readdir_pre(d, prev));

	if (prev == NULL) {
		next = m0_conf_dir_tlist_head(&d->cd_items);
	} else {
		next = m0_conf_dir_tlist_next(&d->cd_items, prev);
		m0_conf_obj_put(prev);
		*pptr = NULL;
	}

	if (next == NULL) {
		ret = M0_CONF_DIREND;
	} else if (next->co_status == M0_CS_READY) {
		m0_conf_obj_get(next);
		*pptr = next;
		ret = M0_CONF_DIRNEXT;
	} else {
		*pptr = next; /* let the caller know which object is missing */
		ret = M0_CONF_DIRMISS;
	}

	M0_POST(readdir_post(ret, d, *pptr));
	M0_LEAVE("retval=%d", ret);
	return ret;
}

static int dir_lookup(const struct m0_conf_obj *parent,
		      const struct m0_fid *name, struct m0_conf_obj **out)
{
	struct m0_conf_dir *x = M0_CONF_CAST(parent, m0_conf_dir);
	struct m0_conf_obj *obj;

	M0_PRE(parent->co_status == M0_CS_READY);

	obj = m0_tl_find(m0_conf_dir, item, &x->cd_items,
			 m0_fid_eq(&item->co_id, name));
	if (obj == NULL)
		return M0_ERR_INFO(-ENOENT, FID_F": No elem="FID_F" in dir="
				   FID_F, FID_P(&parent->co_parent->co_id),
				   FID_P(name), FID_P(&parent->co_id));
	*out = obj;
	M0_POST(m0_conf_obj_invariant(*out));
	return 0;
}

static void dir_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_obj *item;
	struct m0_conf_dir *x = M0_CONF_CAST(obj, m0_conf_dir);

	m0_tl_teardown(m0_conf_dir, &x->cd_items, item) {
		/* `item' is deleted by m0_conf_cache_fini(). */
	}
	m0_conf_dir_tlist_fini(&x->cd_items);
	m0_conf_dir_bob_fini(x);
	m0_free(x);
}

static const struct m0_conf_obj_ops dir_ops = {
	.coo_invariant = dir_invariant,
	.coo_decode    = dir_decode,
	.coo_encode    = dir_encode,
	.coo_match     = dir_match,
	.coo_lookup    = dir_lookup,
	.coo_readdir   = dir_readdir,
	.coo_downlinks = NULL,
	.coo_delete    = dir_delete
};

static struct m0_conf_obj *dir_create(void)
{
	struct m0_conf_dir *x;
	struct m0_conf_obj *ret;

	M0_ALLOC_PTR(x);
	if (x == NULL)
		return NULL;
	m0_conf_dir_bob_init(x);

	/* Initialise concrete fields. */
	m0_conf_dir_tlist_init(&x->cd_items);

	ret = &x->cd_obj;
	ret->co_ops = &dir_ops;
	return ret;
}

const struct m0_conf_obj_type M0_CONF_DIR_TYPE = {
	.cot_ftype  = {
		.ft_id   = M0_CONF__DIR_FT_ID,
		.ft_name = "conf_dir"
	},
	.cot_create = &dir_create,
	.cot_magic  = M0_CONF_DIR_MAGIC
};

const struct m0_fid_type M0_CONF_RELFID_TYPE = {
	.ft_id   = '/',
	.ft_name = "conf relation"
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
