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

/**
 * @addtogroup conf_walk
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/walk.h"
#include "conf/cache.h"    /* m0_conf_cache_is_locked */
#include "conf/obj_ops.h"  /* m0_conf_obj_invariant */
#include "conf/dir.h"      /* m0_conf_dir_tl */

struct conf_walker {
	struct m0_conf_obj *cw_origin;
	int               (*cw_fn)(struct m0_conf_obj *obj, void *args);
	struct m0_conf_obj *cw_cur;
	bool                cw_down_p;
};

static int conf_walker_down(struct conf_walker *w, void *args);
static int conf_walker_up(struct conf_walker *w);
static struct m0_conf_obj *conf_downlink_first(const struct m0_conf_obj *obj);
static struct m0_conf_obj *conf_downlink_next(const struct m0_conf_obj *parent,
					      const struct m0_conf_obj *child);

M0_INTERNAL int m0_conf_walk(int (*fn)(struct m0_conf_obj *obj, void *args),
			     struct m0_conf_obj *origin, void *args)
{
	struct conf_walker w = {
		.cw_origin = origin,
		.cw_fn     = fn,
		.cw_cur    = origin,
		.cw_down_p = true
	};
	int rc;

	M0_PRE(m0_conf_cache_is_locked(origin->co_cache));
	M0_PRE(m0_conf_obj_invariant(origin));
	while (1) {
		rc = w.cw_down_p ? conf_walker_down(&w, args) :
			conf_walker_up(&w);
		if (rc < 0 || rc == M0_CW_STOP)
			return rc;
		M0_ASSERT(rc == M0_CW_CONTINUE);
	}
}

static void conf_walker_turn(struct conf_walker *w)
{
	w->cw_down_p = !w->cw_down_p;
}

static int conf_walker_down(struct conf_walker *w, void *args)
{
	struct m0_conf_obj *obj = w->cw_cur;
	int                 rc;

	M0_PRE(w->cw_down_p);
	M0_PRE(m0_conf_obj_invariant(obj));

	rc = w->cw_fn(obj, args);
	if (rc < 0 || rc == M0_CW_STOP)
		return M0_RC(rc);
	if (rc == M0_CW_SKIP_SIBLINGS) {
		if (obj->co_parent == NULL)
			M0_ASSERT(m0_conf_obj_type(obj) == &M0_CONF_ROOT_TYPE);
		else
			w->cw_cur = obj->co_parent;
		conf_walker_turn(w);
		return M0_RC(M0_CW_CONTINUE);
	}
	if (m0_conf_obj_is_stub(obj) || rc == M0_CW_SKIP_SUBTREE) {
		conf_walker_turn(w);
		return M0_RC(M0_CW_CONTINUE);
	}
	M0_ASSERT(rc == M0_CW_CONTINUE);
	obj = conf_downlink_first(obj);
	if (obj == NULL)
		conf_walker_turn(w);
	else
		w->cw_cur = obj;
	return M0_RC(M0_CW_CONTINUE);
}

static int conf_walker_up(struct conf_walker *w)
{
	struct m0_conf_obj *parent;

	M0_PRE(!w->cw_down_p);
	M0_PRE(m0_conf_obj_invariant(w->cw_cur));

	if (w->cw_cur == w->cw_origin)
		return M0_RC(M0_CW_STOP); /* end of traversal */

	parent = w->cw_cur->co_parent;
	w->cw_cur = conf_downlink_next(parent, w->cw_cur);
	if (w->cw_cur == NULL)
		w->cw_cur = parent; /* no more downlinks; go up */
	else
		conf_walker_turn(w); /* found unvisited downlink; go down */
	return M0_RC(M0_CW_CONTINUE);
}

static struct m0_conf_obj *conf_downlink_first(const struct m0_conf_obj *obj)
{
	const struct m0_fid **rels;
	struct m0_conf_obj   *x;
	int                   rc;

	if (m0_conf_obj_type(obj) == &M0_CONF_DIR_TYPE)
		return m0_conf_dir_tlist_head(
			&M0_CONF_CAST(obj, m0_conf_dir)->cd_items);
	rels = obj->co_ops->coo_downlinks(obj);
	if (*rels == NULL)
	    return NULL;
	rc = obj->co_ops->coo_lookup(obj, *rels, &x);
	M0_ASSERT(rc == 0); /* downlinks are guaranteed to be valid */
	return x;
}

static struct m0_conf_obj *conf_downlink_next(const struct m0_conf_obj *parent,
					      const struct m0_conf_obj *child)
{
	const struct m0_fid **rels;
	struct m0_conf_obj   *x;
	bool                  child_found_p = false;
	int                   rc;

	if (m0_conf_obj_type(parent) == &M0_CONF_DIR_TYPE)
		return m0_conf_dir_tlist_next(
			&M0_CONF_CAST(parent, m0_conf_dir)->cd_items, child);
	for (rels = parent->co_ops->coo_downlinks(parent); *rels != NULL;
	     ++rels) {
		rc = parent->co_ops->coo_lookup(parent, *rels, &x);
		M0_ASSERT(rc == 0); /* downlinks are guaranteed to be valid */
		if (child_found_p)
			return x;
		if (x == child)
			child_found_p = true;
	}
	M0_ASSERT_INFO(child_found_p, "No downlink from "FID_F" to "FID_F,
		       FID_P(&parent->co_id), FID_P(&child->co_id));
	return NULL;
}

#undef M0_TRACE_SUBSYSTEM
/** @} conf_walk */
