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
 * Original creation date: 12-Feb-2016
 */

/**
 * @addtogroup conf_glob
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/glob.h"
#include "conf/obj_ops.h"  /* m0_conf_obj_ops */
#include "conf/dir.h"      /* m0_conf_dir_tl */
#include "lib/errno.h"     /* E2BIG */
#include "lib/string.h"    /* m0_vsnprintf */

/** Symbolic names for conf_glob_step() return values. */
enum conf_glob_res {
	/** Another conf_glob_step() can be made. */
	CONF_GLOB_CONT,
	/** Target object reached. Another conf_glob_step() can be made. */
	CONF_GLOB_TARGET,
	/** Conf DAG traversal completed. */
	CONF_GLOB_END
};

static int conf_glob_step(struct m0_conf_glob *glob,
			  const struct m0_conf_obj **target);
static int conf_glob_err(struct m0_conf_glob *glob, int errcode,
			 const struct m0_conf_obj *obj,
			 const struct m0_fid *path);

M0_INTERNAL void m0_conf__glob_init(struct m0_conf_glob *glob,
				    int flags, m0_conf_glob_errfunc_t errfunc,
				    const struct m0_conf_cache *cache,
				    const struct m0_conf_obj *origin,
				    const struct m0_fid *path)
{
	size_t i;

	M0_PRE(cache != NULL || origin != NULL);
	M0_PRE(ergo(origin != NULL && cache != NULL,
		    origin->co_cache == cache));
	M0_PRE(m0_conf_cache_is_locked(cache ?: origin->co_cache));

	for (i = 0; i < ARRAY_SIZE(glob->cg_path) && m0_fid_is_set(&path[i]);
	     ++i)
		glob->cg_path[i] = path[i];
	M0_ASSERT_INFO(IS_IN_ARRAY(i, glob->cg_path), "Path is too long,"
		       " consider increasing M0_CONF_PATH_MAX");
	glob->cg_path[i] = M0_FID0; /* terminate the path */

	glob->cg_cache = cache ?: origin->co_cache;
	glob->cg_origin = origin ?: m0_conf_cache_lookup(cache,
							 &M0_CONF_ROOT_FID);
	M0_ASSERT_INFO(glob->cg_origin != NULL, "No root object");
	M0_ASSERT_INFO(glob->cg_origin->co_status == M0_CS_READY,
		       "Conf object is not ready: "FID_F,
		       FID_P(&glob->cg_origin->co_id));

	glob->cg_flags = flags;
	glob->cg_errfunc = errfunc;
	glob->cg_depth = 0;
	glob->cg_down_p = true;
	M0_SET_ARR0(glob->cg_trace);
	glob->cg_trace[0] = glob->cg_origin;
#ifdef DEBUG
	glob->cg_debug_print = false;
#endif
}

M0_INTERNAL int m0_conf_glob(struct m0_conf_glob *glob, uint32_t nr,
			     const struct m0_conf_obj **objv)
{
	uint32_t filled = 0;
	int      rc;

	M0_ENTRY();
	M0_PRE(m0_conf_cache_is_locked(glob->cg_cache));
	while (filled < nr) {
		rc = conf_glob_step(glob, &objv[filled]);
		if (rc < 0)
			return M0_ERR(rc);
		if (rc == CONF_GLOB_END)
			return M0_RC(filled);
		if (rc == CONF_GLOB_TARGET)
			++filled;
		else
			M0_ASSERT(rc == CONF_GLOB_CONT);
	}
	return M0_RC(filled);
}

M0_INTERNAL char *
m0_conf_glob_error(const struct m0_conf_glob *glob, char *buf, size_t buflen)
{
	M0_PRE(_0C(glob->cg_errcode != 0) && _0C(glob->cg_errobj != NULL) &&
	       _0C(glob->cg_errpath != NULL));
	M0_PRE(buf != NULL && buflen > 0);

	if (glob->cg_errcode == -EPERM) {
		M0_ASSERT(glob->cg_errobj->co_status != M0_CS_READY);
		return m0_vsnprintf(buf, buflen, "Conf object is not ready: "
				    FID_F, FID_P(&glob->cg_errobj->co_id));
	}
	M0_ASSERT(glob->cg_errcode == -ENOENT);
	return m0_vsnprintf(buf, buflen, "Unreachable path: "FID_F"/"FID_F,
			    FID_P(&glob->cg_errobj->co_id),
			    FID_P(glob->cg_errpath));
}

static void conf_glob_turn(struct m0_conf_glob *glob)
{
	glob->cg_down_p = !glob->cg_down_p;
}

static int
conf_glob_down(struct m0_conf_glob *glob, const struct m0_conf_obj **target)
{
	uint32_t                 *depth = &glob->cg_depth;
	const struct m0_fid      *elem = &glob->cg_path[*depth];
	const struct m0_conf_obj *obj = glob->cg_trace[*depth];
	struct m0_conf_obj       *x;
	int                       rc;

	M0_ENTRY("depth=%"PRIu32" obj="FID_F" elem="FID_F,
		 *depth, FID_P(&obj->co_id), FID_P(elem));
	M0_PRE(glob->cg_down_p);
	M0_PRE(obj->co_status == M0_CS_READY);

	if (!m0_fid_is_set(elem)) { /* end of path? */
		/* target object reached */
		conf_glob_turn(glob);
		*target = obj;
		return M0_RC(CONF_GLOB_TARGET);
	}
	M0_CNT_INC(*depth);
	M0_ASSERT(IS_IN_ARRAY(*depth, glob->cg_path));
	M0_ASSERT(glob->cg_trace[*depth] == NULL);

	if (m0_fid_eq(elem, &M0_CONF_ANY_FID)) {
		const struct m0_conf_dir *dir = M0_CONF_CAST(obj, m0_conf_dir);
		if (m0_conf_dir_tlist_is_empty(&dir->cd_items)) {
			/* empty directory ==> detour */
			conf_glob_turn(glob);
			M0_CNT_DEC(*depth);
			return M0_RC(CONF_GLOB_CONT);
		}
		obj = glob->cg_trace[*depth] =
			m0_conf_dir_tlist_head(&dir->cd_items);
		if (obj->co_status == M0_CS_READY)
			return M0_RC(CONF_GLOB_CONT); /* continue descent */
		/* stub ==> stop or detour */
		conf_glob_turn(glob);
		return M0_RC(conf_glob_err(glob, -EPERM, obj, elem));
	} else {
		rc = obj->co_ops->coo_lookup(obj, elem, &x);
		if (rc != 0)
			M0_ASSERT(rc == -ENOENT); /* no such object */
		else if (x->co_status != M0_CS_READY)
			rc = -EPERM; /* stub */

		if (rc == 0) {
			glob->cg_trace[*depth] = x;
			return M0_RC(CONF_GLOB_CONT); /* continue descent */
		}
		/* stop or detour */
		conf_glob_turn(glob);
		M0_CNT_DEC(*depth);
		return M0_RC(conf_glob_err(glob, rc, rc == -ENOENT ? obj : x,
					   elem));
	}
}

static int conf_glob_up(struct m0_conf_glob *glob)
{
	uint32_t                 *depth = &glob->cg_depth;
	const struct m0_fid      *elem;
	const struct m0_conf_dir *dir;
	const struct m0_conf_obj *obj;

	M0_ENTRY("depth=%"PRIu32, *depth);
	M0_PRE(!glob->cg_down_p);

	if (*depth == 0)
		return M0_RC(CONF_GLOB_END); /* end of traversal */

	elem = &glob->cg_path[*depth - 1];
	if (elem->f_container != M0_CONF_ANY_FID.f_container)
		/*
		 * All fids in M0_CONF_REL_FIDS have the same .f_container
		 * part (0x2f00000000000000); see the use of
		 * M0_CONF_REL_FIDS in conf/objs/common.c.
		 *
		 * `elem' is not in M0_CONF_REL_FIDS ==>
		 * `elem' is a fid of a conf object.
		 *
		 * Since conf cache cannot have two conf
		 * objects with the same fid, it would be
		 * pointless for conf_glob_up() to raise
		 * above this object --- no other path
		 * withinin the conf graph would lead
		 * to `elem' conf object.
		 */
		return M0_RC(CONF_GLOB_END); /* end of traversal */
	if (!m0_fid_eq(elem, &M0_CONF_ANY_FID)) {
		glob->cg_trace[*depth] = NULL;
		M0_CNT_DEC(*depth);
		return M0_RC(CONF_GLOB_CONT); /* continue ascent */
	}
	dir = M0_CONF_CAST(glob->cg_trace[*depth - 1], m0_conf_dir);
	obj = glob->cg_trace[*depth] =
		m0_conf_dir_tlist_next(&dir->cd_items, glob->cg_trace[*depth]);
	if (obj == NULL) {
		/* no more items in this dir */
		M0_CNT_DEC(*depth);
		return M0_RC(CONF_GLOB_CONT); /* continue ascent */
	}
	/* found a sibling */
	if (obj->co_status == M0_CS_READY) {
		conf_glob_turn(glob);
		return M0_RC(CONF_GLOB_CONT); /* descend */
	}
	/* stub ==> stop or try again (with another sibling) */
	return M0_RC(conf_glob_err(glob, -EPERM, obj, elem));
}

#ifdef DEBUG
static void conf_glob_step_pre(const struct m0_conf_glob *glob)
{
	if (glob->cg_debug_print) {
		char     marker;
		uint32_t i;

		for (i = 0; m0_fid_is_set(&glob->cg_path[i]); ++i) {
			marker = i == glob->cg_depth ?
				"^v"[!!glob->cg_down_p] : ' ';
			if (glob->cg_trace[i] == NULL)
				M0_LOG(M0_DEBUG, "%c%u "FID_F" NULL", marker,
				       i, FID_P(&glob->cg_path[i]));
			else
				M0_LOG(M0_DEBUG, "%c%u "FID_F" "FID_F, marker,
				       i, FID_P(&glob->cg_path[i]),
				       FID_P(&glob->cg_trace[i]->co_id));
		}
	}
}

static void conf_glob_step_post(const struct m0_conf_glob *glob, int retval,
				const struct m0_conf_obj *obj)
{
	if (glob->cg_debug_print) {
		if (retval == CONF_GLOB_TARGET)
			M0_LOG(M0_DEBUG, "==> %c%u "FID_F,
			       "^v"[!!glob->cg_down_p], glob->cg_depth,
			       FID_P(&obj->co_id));
		else
			M0_LOG(M0_DEBUG, "==> %c%u",
			       "^v"[!!glob->cg_down_p], glob->cg_depth);
	}
}
#else
#  define conf_glob_step_pre(...)
#  define conf_glob_step_post(...)
#endif

static int
conf_glob_step(struct m0_conf_glob *glob, const struct m0_conf_obj **target)
{
	int rc;

	conf_glob_step_pre(glob);
	rc = glob->cg_down_p ? conf_glob_down(glob, target) :
		conf_glob_up(glob);
	conf_glob_step_post(glob, rc, *target);
	return rc;
}

static int conf_glob_err(struct m0_conf_glob *glob, int errcode,
			 const struct m0_conf_obj *obj,
			 const struct m0_fid *path)
{
	int rc;

	M0_PRE(errcode < 0);

	/* Store error context for m0_conf_glob_error() to use. */
	glob->cg_errcode = errcode;
	glob->cg_errobj = obj;
	glob->cg_errpath = path;

	rc = glob->cg_errfunc == NULL ? 0 :
		glob->cg_errfunc(errcode, obj, path);
	return (rc == 0 && glob->cg_flags & M0_CONF_GLOB_ERR) ? errcode : rc;
}

#undef M0_TRACE_SUBSYSTEM
/** @} conf_glob */
