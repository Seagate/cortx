/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Valery V. Vorotyntsev <valery.vorotyntsev@seagate.com>
 * Original creation date: 28-Jun-2016
 */

#include "conf/walk.h"
#include "conf/ut/common.h"  /* m0_conf_ut_cache_from_file */
#include "ut/misc.h"         /* M0_UT_PATH */
#include "ut/ut.h"

static int conf_ut_count_nondirs(struct m0_conf_obj *obj, void *args)
{
	if (m0_conf_obj_type(obj) != &M0_CONF_DIR_TYPE)
		++*(unsigned *)args;
	return M0_CW_CONTINUE;
}

static void test_conf_walk(void)
{
	struct m0_conf_cache *cache = &m0_conf_ut_cache;
	struct m0_conf_obj   *root;
	unsigned              n = 0;
	int                   rc;

	m0_conf_ut_cache_from_file(cache, M0_UT_PATH("conf.xc"));
	m0_conf_cache_lock(cache);
	root = m0_conf_cache_lookup(cache, &M0_CONF_ROOT_FID);
	M0_UT_ASSERT(root != NULL);
	rc = m0_conf_walk(conf_ut_count_nondirs, root, &n);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(n == M0_UT_CONF_NR_OBJS);
	m0_conf_cache_unlock(cache);

	/* XXX TODO: add more tests */
}

struct m0_ut_suite conf_walk_ut = {
	.ts_name  = "conf-walk-ut",
	.ts_init  = m0_conf_ut_cache_init,
	.ts_fini  = m0_conf_ut_cache_fini,
	.ts_tests = {
		{ "walk", test_conf_walk },
		{ NULL, NULL }
	}
};
