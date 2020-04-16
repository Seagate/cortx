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
 * Original creation date: 26-Jul-2012
 */

#include "conf/cache.h"
#include "conf/obj_ops.h"  /* m0_conf_obj_create */
#include "conf/preload.h"  /* m0_confstr_parse, m0_confx_free */
#include "conf/onwire.h"   /* m0_confx_obj, m0_confx */
#include "conf/dir.h"      /* m0_conf_dir_add */
#include "conf/ut/common.h"
#include "lib/buf.h"       /* m0_buf, M0_BUF_INITS */
#include "lib/errno.h"     /* ENOENT */
#include "lib/fs.h"        /* m0_file_read */
#include "lib/memory.h"    /* m0_free0 */
#include "ut/misc.h"       /* M0_UT_PATH */
#include "ut/ut.h"

static void test_obj_xtors(void)
{
	struct m0_conf_obj            *obj;
	const struct m0_conf_obj_type *t = NULL;

	while ((t = m0_conf_obj_type_next(t)) != NULL) {
		const struct m0_fid fid = M0_FID_TINIT(t->cot_ftype.ft_id,
						       1, 0);
		M0_ASSERT(m0_conf_fid_is_valid(&fid));
		obj = m0_conf_obj_create(&fid, &m0_conf_ut_cache);
		M0_UT_ASSERT(obj != NULL);

		m0_conf_cache_lock(&m0_conf_ut_cache);
		m0_conf_obj_delete(obj);
		m0_conf_cache_unlock(&m0_conf_ut_cache);
	}
}

static void
ut_conf_obj_create(const struct m0_fid *fid, struct m0_conf_obj **result)
{
	struct m0_conf_obj *obj;
	int                 rc;

	obj = m0_conf_cache_lookup(&m0_conf_ut_cache, fid);
	M0_UT_ASSERT(obj == NULL);

	obj = m0_conf_obj_create(fid, &m0_conf_ut_cache);
	M0_UT_ASSERT(obj != NULL);
	M0_UT_ASSERT(m0_fid_eq(&(obj)->co_id, fid));

	m0_conf_cache_lock(&m0_conf_ut_cache);
	rc = m0_conf_cache_add(&m0_conf_ut_cache, obj);
	m0_conf_cache_unlock(&m0_conf_ut_cache);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(m0_conf_cache_lookup(&m0_conf_ut_cache, fid) == obj);
	*result = obj;
}

static void ut_conf_obj_delete(struct m0_conf_obj *obj)
{
	m0_conf_cache_lock(&m0_conf_ut_cache);
	m0_conf_cache_del(&m0_conf_ut_cache, obj);
	m0_conf_cache_unlock(&m0_conf_ut_cache);

	M0_UT_ASSERT(m0_conf_cache_lookup(&m0_conf_ut_cache, &obj->co_id) ==
		     NULL);
}

static void test_dir_add_del(void)
{
	struct m0_conf_obj *dir;
	struct m0_conf_obj *obj;
	const struct m0_fid objid = M0_FID_TINIT('a', 8, 1);

	ut_conf_obj_create(&M0_FID_TINIT('D', 8, 1), &dir);
	M0_CONF_CAST(dir, m0_conf_dir)->cd_relfid = M0_FID_TINIT('/', 0, 999);
	M0_CONF_CAST(dir, m0_conf_dir)->cd_item_type = m0_conf_fid_type(&objid);

	ut_conf_obj_create(&objid, &obj);
	m0_conf_dir_add(M0_CONF_CAST(dir, m0_conf_dir), obj);
	m0_conf_dir_del(M0_CONF_CAST(dir, m0_conf_dir), obj);

	ut_conf_obj_delete(obj);
	ut_conf_obj_delete(dir);
}

static void test_cache(void)
{
	int                 rc;
	struct m0_conf_obj *obj;
	struct m0_fid       samples[] = {
		M0_FID_TINIT('p', ~0, 0),
		M0_FID_TINIT('D', 7, 3)
	};

	m0_forall(i, ARRAY_SIZE(samples),
		  ut_conf_obj_create(&samples[i], &obj), true);
	ut_conf_obj_delete(obj); /* delete the last object */

	/* Duplicated identity. */
	obj = m0_conf_obj_create(&samples[0], &m0_conf_ut_cache);
	M0_UT_ASSERT(obj != NULL);

	m0_conf_cache_lock(&m0_conf_ut_cache);
	rc = m0_conf_cache_add(&m0_conf_ut_cache, obj);
	m0_conf_cache_unlock(&m0_conf_ut_cache);
	M0_UT_ASSERT(rc == -EEXIST);

	m0_conf_cache_lock(&m0_conf_ut_cache);
	m0_conf_obj_delete(obj);
	m0_conf_cache_unlock(&m0_conf_ut_cache);
}

static void test_obj_find(void)
{
	int                 rc;
	const struct m0_fid id = M0_FID_TINIT('p', 1, 0);
	struct m0_conf_obj *p = NULL;
	struct m0_conf_obj *q = NULL;

	m0_conf_cache_lock(&m0_conf_ut_cache);

	rc = m0_conf_obj_find(&m0_conf_ut_cache, &id, &p);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(p != NULL);

	rc = m0_conf_obj_find(&m0_conf_ut_cache, &id, &q);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(q == p);

	rc = m0_conf_obj_find(&m0_conf_ut_cache, &M0_FID_TINIT('d', 1, 0), &q);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(q != p);

	m0_conf_cache_unlock(&m0_conf_ut_cache);
}

static void test_obj_fill(void)
{
	struct m0_confx    *enc;
	struct m0_conf_obj *obj;
	char               *confstr = NULL;
	int                 i;
	int                 rc;

	m0_confx_free(NULL); /* to make sure this can be done */

	rc = m0_file_read(M0_UT_PATH("conf.xc"), &confstr);
	M0_UT_ASSERT(rc == 0);
	rc = m0_confstr_parse(confstr, &enc);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(enc->cx_nr == M0_UT_CONF_NR_OBJS);
	m0_free0(&confstr);

	m0_conf_cache_lock(&m0_conf_ut_cache);
	for (i = 0; i < enc->cx_nr; ++i) {
		struct m0_confx_obj *xobj = M0_CONFX_AT(enc, i);

		rc = m0_conf_obj_find(&m0_conf_ut_cache, m0_conf_objx_fid(xobj),
				      &obj) ?:
			m0_conf_obj_fill(obj, xobj);
		M0_UT_ASSERT(rc == 0);
	}
	m0_conf_cache_unlock(&m0_conf_ut_cache);

	m0_confx_free(enc);
}

struct m0_ut_suite conf_ut = {
	.ts_name  = "conf-ut",
	.ts_init  = m0_conf_ut_cache_init,
	.ts_fini  = m0_conf_ut_cache_fini,
	.ts_tests = {
		{ "obj-xtors",   test_obj_xtors },
		{ "cache",       test_cache     },
		{ "obj-find",    test_obj_find  },
		{ "obj-fill",    test_obj_fill  },
		{ "dir-add-del", test_dir_add_del },
		{ NULL, NULL }
	}
};
