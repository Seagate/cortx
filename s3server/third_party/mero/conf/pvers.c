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
 * Original author: Rajanikant Chirmade <rajanikant.chirmade@seagate.com>
 *                  Valery V. Vorotyntsev <valery.vorotyntsev@seagate.com>
 * Authors:         Andriy Tkachuk <andriy.tkachuk@seagate.com>
 * Original creation date: 16-Jun-2016
 */

/**
 * @addtogroup conf-pvers
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/pvers.h"
#include "conf/cache.h"        /* m0_conf_cache_lock */
#include "conf/obj_ops.h"      /* m0_conf_dir_tl */
#include "conf/walk.h"
#include "conf/glob.h"
#include "fd/fd.h"             /* m0_fd_tolerance_check */
#include "conf/objs/common.h"  /* m0_conf_dir_new */
#include "lib/combinations.h"  /* m0_combination_index */
#include "lib/memory.h"        /* M0_ALLOC_ARR */

#define CONF_PVER_VECTOR_LOG(owner, fid, vector)                       \
	M0_LOG(M0_DEBUG, owner"="FID_F" "#vector"=[%u %u %u %u %u]", fid, \
	       vector[M0_CONF_PVER_LVL_SITES],                         \
	       vector[M0_CONF_PVER_LVL_RACKS],                         \
	       vector[M0_CONF_PVER_LVL_ENCLS],                         \
	       vector[M0_CONF_PVER_LVL_CTRLS],                         \
	       vector[M0_CONF_PVER_LVL_DRIVES])

/** Array of int values. */
struct arr_int {
	uint32_t ai_count;
	int     *ai_elems;
};

struct arr_int_pos {
	struct arr_int ap_arr;
	uint32_t       ap_pos;
};

static int conf_pver_formulate(const struct m0_conf_pver *fpver,
			       struct m0_conf_pver **out);
static int conf_pver_formulaic_base(const struct m0_conf_pver *fpver,
				    struct m0_conf_pver **out);
static int conf_pver_objvs_count(struct m0_conf_pver *base, uint32_t *out);
static int conf_pver_virtual_create(const struct m0_fid *fid,
				    struct m0_conf_pver *base,
				    const uint32_t *allowance,
				    struct arr_int *failed,
				    struct m0_conf_pver **out);
static int conf_pver_failures_cid(struct m0_conf_pver *base, uint64_t *out);
static void conf_pver_subtree_delete(struct m0_conf_obj *obj);
static int conf_pver_recd_build(struct m0_conf_obj *obj, void *args);

static int conf_pver_formulaic_find_locked(uint32_t fpver_id,
					   const struct m0_conf_root *root,
					   const struct m0_conf_pver **out)
{
	enum { CONF_GLOB_BATCH = 16 }; /* arbitrary number */
	struct m0_conf_glob        glob;
	const struct m0_conf_obj  *objs[CONF_GLOB_BATCH];
	const struct m0_conf_pver *pver;
	int                        i;
	int                        rc;

	m0_conf_glob_init(&glob, M0_CONF_GLOB_ERR, NULL, NULL, &root->rt_obj,
			  M0_CONF_ROOT_POOLS_FID, M0_CONF_ANY_FID,
			  M0_CONF_POOL_PVERS_FID, M0_CONF_ANY_FID);
	while ((rc = m0_conf_glob(&glob, ARRAY_SIZE(objs), objs)) > 0) {
		for (i = 0; i < rc; ++i) {
			pver = M0_CONF_CAST(objs[i], m0_conf_pver);
			if (pver->pv_kind == M0_CONF_PVER_FORMULAIC &&
			    pver->pv_u.formulaic.pvf_id == fpver_id) {
				*out = pver;
				return M0_RC(0);
			}
		}
	}
	return rc == 0 ? M0_ERR_INFO(-ENOENT, "Formulaic pver with id=%u"
				     " is missing", fpver_id) : M0_ERR(rc);
}

static int conf_pver_base_recd_update(const struct m0_conf_pool *pool)
{
	const struct m0_conf_obj *obj;
	struct m0_conf_pver      *pver;
	int                       rc = 0;

	m0_tl_for (m0_conf_dir, &pool->pl_pvers->cd_items, obj) {
		pver = M0_CONF_CAST(obj, m0_conf_pver);
		if (pver->pv_kind == M0_CONF_PVER_ACTUAL) {
			M0_SET_ARR0(pver->pv_u.subtree.pvs_recd);
			rc = m0_conf_walk(conf_pver_recd_build, &pver->pv_obj,
					  pver->pv_u.subtree.pvs_recd);
			break;
		}
	} m0_tl_endfor;
	return M0_RC(rc);
}

static int conf_pver_find_locked(const struct m0_conf_pool *pool,
				 const struct m0_fid *pver_to_skip,
				 struct m0_conf_pver **out)
{
	const struct m0_conf_obj *obj;
	struct m0_conf_pver      *pver;
	int                       rc;

	M0_ENTRY("pool="FID_F, FID_P(&pool->pl_obj.co_id));

	rc = conf_pver_base_recd_update(pool);
	if (rc != 0)
		return M0_ERR_INFO(rc, "Recd update failed for pool "FID_F,
			   FID_P(&pool->pl_obj.co_id));
	m0_tl_for (m0_conf_dir, &pool->pl_pvers->cd_items, obj) {
		pver = M0_CONF_CAST(obj, m0_conf_pver);
		M0_LOG(M0_DEBUG, "pver="FID_F, FID_P(&pver->pv_obj.co_id));
		if (pver_to_skip != NULL &&
		    m0_fid_eq(&pver->pv_obj.co_id, pver_to_skip)) {
			M0_LOG(M0_INFO, "Skipping "FID_F, FID_P(pver_to_skip));
			continue;
		}
		if (!m0_conf_pver_is_clean(pver))
			continue;
		if (pver->pv_kind == M0_CONF_PVER_ACTUAL) {
			*out = pver;
			return M0_RC(0);
		}
		return M0_RC(conf_pver_formulate(pver, out));
	} m0_tl_endfor;
	return M0_ERR_INFO(-ENOENT, "No suitable pver is found at pool "FID_F,
			   FID_P(&pool->pl_obj.co_id));
}

static int conf_pver_find_by_fid_locked(const struct m0_fid *fid,
					const struct m0_conf_root *root,
					struct m0_conf_pver **out)
{
	struct m0_conf_obj        *obj;
	enum m0_conf_pver_kind     kind;
	uint64_t                   container;
	uint64_t                   key;
	const struct m0_conf_pver *fpver;
	struct m0_conf_pver       *base = NULL;
	uint32_t                   nr_total;
	struct arr_int             failed;
	int                        rc;

	M0_PRE(m0_conf_fid_type(fid) == &M0_CONF_PVER_TYPE);

	obj = m0_conf_cache_lookup(root->rt_obj.co_cache, fid);
	if (obj != NULL) {
		*out = M0_CONF_CAST(obj, m0_conf_pver);
		M0_ASSERT((*out)->pv_kind != M0_CONF_PVER_FORMULAIC);
		return M0_RC(0);
	}
	rc = m0_conf_pver_fid_read(fid, &kind, &container, &key);
	if (rc != 0)
		return M0_ERR(rc);
	if (kind == M0_CONF_PVER_ACTUAL)
		return M0_ERR_INFO(-ENOENT, "Actual pver is missing: "FID_F,
				   FID_P(fid));
	M0_ASSERT(kind == M0_CONF_PVER_VIRTUAL);
	rc = conf_pver_formulaic_find_locked(container, root, &fpver) ?:
	     conf_pver_formulaic_base(fpver, &base) ?:
	     conf_pver_objvs_count(base, &nr_total);
	if (rc != 0)
		return M0_ERR(rc);
	failed.ai_count = m0_reduce(i, M0_CONF_PVER_HEIGHT, 0,
				    + fpver->pv_u.formulaic.pvf_allowance[i]);
	M0_ASSERT(failed.ai_count > 0);
	M0_ALLOC_ARR(failed.ai_elems, failed.ai_count);
	if (failed.ai_elems == NULL)
		return M0_ERR(-ENOMEM);
	m0_combination_inverse(key, nr_total, failed.ai_count, failed.ai_elems);
	rc = conf_pver_virtual_create(
		fid, base, fpver->pv_u.formulaic.pvf_allowance, &failed, out);
	m0_free(failed.ai_elems);
	return M0_RC(rc);
}

M0_INTERNAL int m0_conf_pver_find(const struct m0_conf_pool *pool,
				  const struct m0_fid *pver_to_skip,
				  struct m0_conf_pver **out)
{
	int rc;

	m0_conf_cache_lock(pool->pl_obj.co_cache);
	rc = conf_pver_find_locked(pool, pver_to_skip, out);
	m0_conf_cache_unlock(pool->pl_obj.co_cache);
	return rc;
}

M0_INTERNAL int m0_conf_pver_find_by_fid(const struct m0_fid *fid,
					 const struct m0_conf_root *root,
					 struct m0_conf_pver **out)
{
	int rc;

	m0_conf_cache_lock(root->rt_obj.co_cache);
	rc = conf_pver_find_by_fid_locked(fid, root, out);
	m0_conf_cache_unlock(root->rt_obj.co_cache);
	return rc;
}

M0_INTERNAL int m0_conf_pver_formulaic_find(uint32_t fpver_id,
					    const struct m0_conf_root *root,
					    const struct m0_conf_pver **out)
{
	int rc;

	m0_conf_cache_lock(root->rt_obj.co_cache);
	rc = conf_pver_formulaic_find_locked(fpver_id, root, out);
	m0_conf_cache_unlock(root->rt_obj.co_cache);
	return rc;
}

M0_INTERNAL int
m0_conf_pver_formulaic_from_virtual(const struct m0_conf_pver *virtual,
				    const struct m0_conf_root *root,
				    const struct m0_conf_pver **out)
{
	uint64_t cont;

	M0_ENTRY("virtual="FID_F, FID_P(&virtual->pv_obj.co_id));
	M0_PRE(virtual->pv_kind == M0_CONF_PVER_VIRTUAL);

	return M0_RC(m0_conf_pver_fid_read(&virtual->pv_obj.co_id, NULL, &cont,
					   NULL) ?:
		     m0_conf_pver_formulaic_find(
			     /* formulaic pver id; see m0_conf_pver_fid() */
			     cont & 0xffffffff,
			     root, out));
}

/** Tries to find base pver of given formulaic pver in the conf cache. */
static int conf_pver_formulaic_base(const struct m0_conf_pver *fpver,
				    struct m0_conf_pver **out)
{
	const struct m0_conf_obj *base;

	M0_PRE(fpver->pv_kind == M0_CONF_PVER_FORMULAIC);

	base = m0_conf_cache_lookup(fpver->pv_obj.co_cache,
				    &fpver->pv_u.formulaic.pvf_base);
	if (base == NULL)
		return M0_ERR_INFO(-ENOENT, "Base "FID_F" of formulaic pver "
				   FID_F" is missing",
				   FID_P(&fpver->pv_u.formulaic.pvf_base),
				   FID_P(&fpver->pv_obj.co_id));
	*out = M0_CONF_CAST(base, m0_conf_pver);
	M0_POST((*out)->pv_kind == M0_CONF_PVER_ACTUAL);
	return M0_RC(0);
}

M0_INTERNAL bool m0_conf_pver_is_clean(const struct m0_conf_pver *pver)
{
	struct m0_conf_pver *base = NULL;
	const uint32_t      *recd;
	const uint32_t      *allowance;

	M0_ENTRY("pver=%p "FID_F, pver, FID_P(&pver->pv_obj.co_id));

	if (pver->pv_kind == M0_CONF_PVER_ACTUAL) {
		recd = pver->pv_u.subtree.pvs_recd;
		CONF_PVER_VECTOR_LOG("actual", FID_P(&pver->pv_obj.co_id),
				     recd);
		return M0_RC(M0_IS0(&pver->pv_u.subtree.pvs_recd));
	}
	M0_ASSERT(pver->pv_kind == M0_CONF_PVER_FORMULAIC);
	if (conf_pver_formulaic_base(pver, &base) != 0)
		return M0_RC(false);
	recd = base->pv_u.subtree.pvs_recd;
	allowance = pver->pv_u.formulaic.pvf_allowance;
	CONF_PVER_VECTOR_LOG("fpver", FID_P(&pver->pv_obj.co_id), allowance);
	CONF_PVER_VECTOR_LOG("base", FID_P(&base->pv_obj.co_id), recd);
	return M0_RC(m0_forall(i, M0_CONF_PVER_HEIGHT,
			       recd[i] == allowance[i]));
}

enum { CONF_PVER_FID_MASK = 0x003fffffffffffffULL };

M0_INTERNAL struct m0_fid
m0_conf_pver_fid(enum m0_conf_pver_kind kind, uint64_t container, uint64_t key)
{
	/*
	 * Actual/formulaic pver fid
	 * -------------------------
	 *   .f_container:
	 *   - 1-byte type id (0x76);
	 *   - 2-bit kind (actual=0, formulaic=1);
	 *   - 54-bit anything.
	 *
	 *   .f_key:
	 *   - 8-byte anything.
	 *
	 * Virtual pver fid
	 * ----------------
	 *   .f_container:
	 *   - 1-byte type id (0x76);
	 *   - 2-bit kind (2);
	 *   - 22-bit unused (zeros);
	 *   - 4-byte formulaic pver id.
	 *
	 *   .f_key:
	 *   - 8-byte index of combination of failed devices in the ordered
	 *     sequence of pver's devices.
	 */
	if (kind == M0_CONF_PVER_VIRTUAL) {
		/* formulaic pver id is 4 bytes */
		M0_PRE((container & ~0xffffffff) == 0);
	} else {
		M0_PRE(kind < M0_CONF_PVER_VIRTUAL);
		M0_PRE((container & ~CONF_PVER_FID_MASK) == 0);
	}
	/*
	 * XXX TODO: Introduce M0_CONF_FID() macro.
	 *
	 * Use M0_CONF_FID(PVER, container, key) instead of
	 * M0_FID_TINIT('v', container, key) or
	 * M0_FID_TINIT(M0_CONF_PVER_TYPE.cot_ftype.ft_id, container, key).
	 */
	return M0_FID_TINIT(M0_CONF_PVER_TYPE.cot_ftype.ft_id,
			    (uint64_t)kind << 54 | container, key);
}

M0_INTERNAL int m0_conf_pver_fid_read(const struct m0_fid *fid,
				      enum m0_conf_pver_kind *kind,
				      uint64_t *container, uint64_t *key)
{
	enum m0_conf_pver_kind _kind;
	uint64_t               _container;

	M0_PRE(m0_conf_fid_type(fid) == &M0_CONF_PVER_TYPE);

	_kind = fid->f_container >> (64 - 8 - 2) & 3;
	_container = fid->f_container & CONF_PVER_FID_MASK;
	if (_kind > M0_CONF_PVER_VIRTUAL ||
	    (_kind == M0_CONF_PVER_VIRTUAL && (_container & ~0xffffffff) != 0))
		return M0_ERR_INFO(-EINVAL, "Invalid pver fid: "FID_F,
				   FID_P(fid));
	if (kind != NULL)
		*kind = _kind;
	if (container != NULL)
		*container = _container;
	if (key != NULL)
		*key = fid->f_key;
	return 0;
}

/** Generates new fid for a m0_conf_objv of virtual pver subtree. */
static struct m0_fid conf_objv_virtual_fid(struct m0_conf_cache *cache)
{
	M0_PRE(m0_conf_cache_is_locked(cache));
	/*
	 * .f_container:
	 * - 1-byte type id (0x6a);
	 * - 1-bit "virtual?" flag (1);
	 * - 55-bit unused (zeros).
	 *
	 * .f_key:
	 * - 8-byte value of m0_conf_cache::ca_fid_counter.
	 */
	return M0_FID_TINIT(M0_CONF_OBJV_TYPE.cot_ftype.ft_id,
			    1ULL << (64 - 8 - 1),
			    cache->ca_fid_counter++);
}

M0_INTERNAL unsigned m0_conf_pver_level(const struct m0_conf_obj *obj)
{
	const struct m0_conf_obj_type *t = m0_conf_obj_type(obj);

	if (t == &M0_CONF_OBJV_TYPE) {
		/* We may not access m0_conf_objv::cv_real of a stub. */
		M0_ASSERT(obj->co_status == M0_CS_READY);
		t = m0_conf_obj_type(M0_CONF_CAST(obj, m0_conf_objv)->cv_real);
	}
	if (t == &M0_CONF_SITE_TYPE)
		return M0_CONF_PVER_LVL_SITES;
	if (t == &M0_CONF_RACK_TYPE)
		return M0_CONF_PVER_LVL_RACKS;
	if (t == &M0_CONF_ENCLOSURE_TYPE)
		return M0_CONF_PVER_LVL_ENCLS;
	if (t == &M0_CONF_CONTROLLER_TYPE)
		return M0_CONF_PVER_LVL_CTRLS;
	if (t == &M0_CONF_DRIVE_TYPE)
		return M0_CONF_PVER_LVL_DRIVES;
	M0_IMPOSSIBLE("Bad argument: "FID_F, FID_P(&obj->co_id));
}
 /* page break */

/** Finds or creates virtual pool version, described by a formulaic one. */
static int
conf_pver_formulate(const struct m0_conf_pver *fpver, struct m0_conf_pver **out)
{
	struct m0_conf_pver      *base = NULL;
	uint64_t                  failures_cid;
	struct m0_fid             virt_fid;
	const struct m0_conf_obj *virt;
	int                       rc;

	rc = conf_pver_formulaic_base(fpver, &base) ?:
	     conf_pver_failures_cid(base, &failures_cid);
	if (rc != 0)
		return M0_ERR(rc);

	virt_fid = m0_conf_pver_fid(M0_CONF_PVER_VIRTUAL,
				    fpver->pv_u.formulaic.pvf_id, failures_cid);
	virt = m0_conf_cache_lookup(fpver->pv_obj.co_cache, &virt_fid);
	if (virt != NULL) {
		*out = M0_CONF_CAST(virt, m0_conf_pver);
		M0_POST((*out)->pv_kind == M0_CONF_PVER_VIRTUAL);
		return M0_RC(0);
	}
	return M0_RC(conf_pver_virtual_create(
			     &virt_fid, base,
			     fpver->pv_u.formulaic.pvf_allowance, NULL, out));
}

struct conf_pver_enumerate_st {
	int  est_counter;
	bool est_enumerated;
};

static int conf_pver_enumerate_w(struct m0_conf_obj *obj, void *args)
{
	const bool                     skip_sanity_check = false;
	struct m0_conf_objv           *objv;
	struct conf_pver_enumerate_st *st = args;

	if (m0_conf_obj_type(obj) == &M0_CONF_OBJV_TYPE) {
		objv = M0_CONF_CAST(obj, m0_conf_objv);
		if (objv->cv_ix == 0) {
			M0_ASSERT(st->est_counter == 0);
			if (skip_sanity_check)
				/*
				 * The subtree has been enumerated already,
				 * and we are in too much hurry to do
				 * sanity checking.
				 */
				return M0_CW_STOP;
			st->est_enumerated = true;
		}
		if (st->est_enumerated) {
			M0_ASSERT(objv->cv_ix == st->est_counter);
		} else {
			M0_ASSERT(objv->cv_ix == -1);
			objv->cv_ix = st->est_counter;
		}
		M0_CNT_INC(st->est_counter);
	}
	return M0_CW_CONTINUE;
}

/** Sets m0_conf_objv::cv_ix for pver's objvs. */
static void conf_pver_enumerate(struct m0_conf_pver *pver)
{
	struct conf_pver_enumerate_st st = {0};
	int                           rc;

	M0_PRE(pver->pv_kind == M0_CONF_PVER_ACTUAL);

	rc = m0_conf_walk(conf_pver_enumerate_w, &pver->pv_obj, &st);
	M0_ASSERT(rc == 0); /* conf_objv_enumerate() cannot return error */
}

static int conf_pver_objvs_count(struct m0_conf_pver *base, uint32_t *out)
{
	const struct m0_conf_obj  *obj;
	const struct m0_conf_objv *objv;
	uint32_t                   level = 0;

	conf_pver_enumerate(base);
	obj = m0_conf_dir_tlist_tail(&base->pv_u.subtree.pvs_sitevs->cd_items);
	/* rack-v, encl-v, ctrl-v, disk-v */
	while (1) {
		if (obj == NULL || m0_conf_obj_is_stub(obj))
			return M0_ERR_INFO(-ENOENT, "Cannot reach the rightmost"
					   " disk-v from "FID_F"; reached"
					   " level %u",
					   FID_P(&base->pv_obj.co_id), level);
		objv = M0_CONF_CAST(obj, m0_conf_objv);
		if (objv->cv_children == NULL)
			break;
		obj = m0_conf_dir_tlist_tail(&objv->cv_children->cd_items);
		++level;
	}
	M0_ASSERT(level == M0_CONF_PVER_HEIGHT - 1);
	M0_ASSERT(objv->cv_ix > 0); /* guaranteed by conf_pver_enumerate() */
	*out = 1 + (uint32_t)objv->cv_ix;
	return M0_RC(0);
}

static int conf_pver_recd_build(struct m0_conf_obj *obj, void *args)
{
	uint32_t            *recd = args;
	struct m0_conf_objv *objv;
	unsigned             lvl;

	if (m0_conf_obj_type(obj) == &M0_CONF_OBJV_TYPE) {
		objv = M0_CONF_CAST(obj, m0_conf_objv);
		if (objv->cv_real->co_ha_state != M0_NC_ONLINE) {
			lvl = m0_conf_pver_level(obj);
			M0_ASSERT(lvl != 0 && lvl < M0_CONF_PVER_HEIGHT);
			M0_CNT_INC(recd[lvl]);
			return M0_CW_SKIP_SUBTREE;
		}
	}
	return M0_CW_CONTINUE;
}

static int conf_objv_failed_fill(struct m0_conf_obj *obj, void *args)
{
	struct arr_int_pos        *a = args;
	const struct m0_conf_objv *objv;

	M0_PRE(a->ap_arr.ai_count > 0 && a->ap_arr.ai_elems != NULL);

	if (m0_conf_obj_type(obj) != &M0_CONF_OBJV_TYPE)
		return M0_CW_CONTINUE;
	objv = M0_CONF_CAST(obj, m0_conf_objv);
	if (objv->cv_real->co_ha_state == M0_NC_ONLINE)
		return M0_CW_CONTINUE;
	M0_ASSERT(a->ap_pos < a->ap_arr.ai_count);
	a->ap_arr.ai_elems[a->ap_pos] = objv->cv_ix;
	M0_CNT_INC(a->ap_pos);
	return M0_CW_SKIP_SUBTREE;
}

/**
 * Computes index of combination of failed devices in the ordered
 * sequence of pver's devices.
 */
static int conf_pver_failures_cid(struct m0_conf_pver *base, uint64_t *out)
{
	struct arr_int_pos a = { .ap_arr = {0} };
	uint32_t           nr_total;
	int                rc;

	M0_PRE(base->pv_kind == M0_CONF_PVER_ACTUAL);
	M0_PRE(!M0_IS0(&base->pv_u.subtree.pvs_recd));

	rc = conf_pver_objvs_count(base, &nr_total);
	if (rc != 0)
		return M0_ERR(rc);
	/*
	 * Count the number of non-online devices in the base pver subtree.
	 */
	a.ap_arr.ai_count = m0_reduce(
		i, ARRAY_SIZE(base->pv_u.subtree.pvs_recd), 0,
		+ base->pv_u.subtree.pvs_recd[i]);
	M0_ALLOC_ARR(a.ap_arr.ai_elems, a.ap_arr.ai_count);
	if (a.ap_arr.ai_elems == NULL)
		return M0_ERR(-ENOMEM);
	/*
	 * Put indices of failed devices into the array.
	 */
	rc = m0_conf_walk(conf_objv_failed_fill, &base->pv_obj, &a);
	M0_ASSERT(rc == 0); /* conf_objv_failed_fill() cannot return error */
	M0_ASSERT(a.ap_pos == a.ap_arr.ai_count);
	/*
	 * Compute index of combination of failed devices.
	 */
	*out = (uint64_t)m0_combination_index(nr_total, a.ap_arr.ai_count,
					      a.ap_arr.ai_elems);
	m0_free(a.ap_arr.ai_elems);
	return M0_RC(0);
}

struct conf_pver_base_walk_st {
	uint32_t              bws_allowance[M0_CONF_PVER_HEIGHT];
	const struct arr_int *bws_failed;
	uint32_t              bws_failed_next;
	uint32_t              bws_disk_nr;
	struct m0_conf_dir   *bws_dirs[M0_CONF_PVER_HEIGHT];
};

/**
 * Checks if `obj' should be copied to the virtual pver subtree.
 *
 * @retval 0                   Current objv should be copied to the virtual pver
 *                             subtree.
 * @retval M0_CW_SKIP_SUBTREE  Current objv and its children should be excluded.
 * @retval M0_CW_CONTINUE      Current object is not an objv.
 * @retval -Exxx               Error.
 */
static int conf_pver_base_obj_check(struct m0_conf_obj *obj,
				    struct conf_pver_base_walk_st *st)
{
	const struct m0_conf_objv *objv;
	unsigned                   level;

	if (m0_conf_obj_type(obj) != &M0_CONF_OBJV_TYPE)
		return M0_RC(M0_CW_CONTINUE);
	objv = M0_CONF_CAST(obj, m0_conf_objv);
	level = m0_conf_pver_level(obj);
	M0_LOG(M0_DEBUG, "objv="FID_F" ix=%d real="FID_F" level=%u",
	       FID_P(&obj->co_id), objv->cv_ix, FID_P(&objv->cv_real->co_id),
	       level);
	if (st->bws_failed == NULL) {
		if (objv->cv_real->co_ha_state != M0_NC_ONLINE) {
			M0_CNT_DEC(st->bws_allowance[level]);
			return M0_RC(M0_CW_SKIP_SUBTREE);
		}
	} else if (st->bws_failed_next < st->bws_failed->ai_count &&
		   objv->cv_ix == st->bws_failed->ai_elems[
			   st->bws_failed_next]) {
		++st->bws_failed_next;
		if (st->bws_allowance[level] == 0)
			return M0_ERR_INFO(-EINVAL, "Failures are not"
					   " compatible with allowance vector at"
					   " level %u", level);
		--st->bws_allowance[level];
		return M0_RC(M0_CW_SKIP_SUBTREE);
	}
	return M0_RC(0);
}

/** This function is called for every object in base pver subtree. */
static int conf_pver_base_w(struct m0_conf_obj *obj, void *args)
{
	struct conf_pver_base_walk_st *st = args;
	struct m0_conf_cache          *cache = obj->co_cache;
	unsigned                       level;
	struct m0_fid                  virt_fid;
	struct m0_conf_obj            *new_obj;
	struct m0_conf_objv           *new_objv;
	const struct m0_fid           *downlink;
	int                            rc;

	rc = conf_pver_base_obj_check(obj, st);
	if (rc != 0)
		return rc; /* don't use M0_ERR or M0_RC here */
	level = m0_conf_pver_level(obj);
	virt_fid = conf_objv_virtual_fid(cache);
	/*
	 * Create a stub of m0_conf_objv.
	 */
	new_obj = m0_conf_obj_create(&virt_fid, cache);
	new_objv = M0_CONF_CAST(new_obj, m0_conf_objv);
	new_objv->cv_real = M0_CONF_CAST(obj, m0_conf_objv)->cv_real;
	/* Add it to the parent directory. */
	m0_conf_dir_add(st->bws_dirs[level], new_obj);

	rc = m0_conf_cache_add(cache, new_obj);
	/* m0_conf_cache_add() cannot fail, because conf_objv_virtual_fid()
	 * returns unique fids. */
	M0_ASSERT(rc == 0);

	downlink = new_obj->co_ops->coo_downlinks(new_obj)[0];
	if (downlink == NULL) {
		M0_ASSERT(level == M0_CONF_PVER_LVL_DRIVES);
		M0_CNT_INC(st->bws_disk_nr);
		goto out;
	}
	/* Create new_objv->cv_children directory. */
	rc = m0_conf_dir_new(new_obj, downlink, &M0_CONF_OBJV_TYPE, NULL,
			     &new_objv->cv_children);
	M0_ASSERT_INFO(rc == 0, "XXX BUG: error handling is not implemented");
	st->bws_dirs[level + 1] = new_objv->cv_children;
out:
	new_obj->co_status = M0_CS_READY;
	M0_POST(m0_conf_obj_invariant(new_obj));
	return M0_CW_CONTINUE;
}

/**
 * Adjusts the tolerance vector of a virtual pool version to make it consistent
 * with the underlying subtree.
 */
static int conf_pver_tolerance_adjust(struct m0_conf_pver *pver)
{
	int       rc;
	uint32_t  level = 0;

	M0_PRE(pver->pv_kind == M0_CONF_PVER_VIRTUAL);

	do {
		rc = m0_fd_tolerance_check(pver, &level);
		if (rc == -EINVAL)
			M0_CNT_DEC(pver->pv_u.subtree.pvs_tolerance[level]);
	} while (rc == -EINVAL);
	return rc;
}

/**
 * Creates virtual pool version: copies subtree of base pool version,
 * excluding objvs that correspond to failed devices.
 *
 * @param fid        Virtual pver fid.
 * @param base       Base pool version.
 * @param allowance  Allowance vector of the formulaic pver.
 * @param failed     [optional] Indices of failed devices in the base pver
 *                   subtree.
 * @param out        Result.
 *
 * If `failed' is not provided, objvs are excluded according to
 * m0_conf_obj::co_ha_state of objects they refer to.
 */
static int conf_pver_virtual_create(const struct m0_fid *fid,
				    struct m0_conf_pver *base,
				    const uint32_t *allowance,
				    struct arr_int *failed,
				    struct m0_conf_pver **out)
{
	struct m0_conf_obj           *pvobj;
	struct m0_conf_pver          *pver;
	struct m0_conf_pver_subtree  *pvsub;
	struct m0_conf_cache         *cache = base->pv_obj.co_cache;
	struct conf_pver_base_walk_st st;
	int                           rc;

	{ /* Validate the fid. */
		enum m0_conf_pver_kind kind;
		rc = m0_conf_pver_fid_read(fid, &kind, NULL, NULL);
		M0_PRE(rc == 0 && kind == M0_CONF_PVER_VIRTUAL);
	}
	M0_PRE(base->pv_kind == M0_CONF_PVER_ACTUAL);
	M0_PRE(ergo(failed == NULL,
		    _0C(m0_forall(i, M0_CONF_PVER_HEIGHT,
				  base->pv_u.subtree.pvs_recd[i] ==
				  allowance[i])) &&
		    _0C(!M0_IS0(&base->pv_u.subtree.pvs_recd))));
	M0_PRE(failed == NULL ||
	       failed->ai_count == m0_reduce(i, M0_CONF_PVER_HEIGHT, 0,
					     + allowance[i]));

	pvobj = m0_conf_obj_create(fid, cache);
	if (pvobj == NULL)
		return M0_ERR(-ENOMEM);

	pver = M0_CONF_CAST(pvobj, m0_conf_pver);
	pver->pv_kind = M0_CONF_PVER_VIRTUAL;
	pvsub = &pver->pv_u.subtree;
	/* Copy attributes from base. */
	pvsub->pvs_attr = base->pv_u.subtree.pvs_attr;
	memcpy(pvsub->pvs_tolerance, base->pv_u.subtree.pvs_tolerance,
	       sizeof pvsub->pvs_tolerance);
	rc = m0_conf_cache_add(cache, pvobj);
	/* m0_conf_cache_add() cannot fail: conf_pver_virtual_create()
	 * would not be called if the object existed in the cache. */
	M0_ASSERT(rc == 0);
	rc = m0_conf_dir_new(pvobj, &M0_CONF_PVER_SITEVS_FID,
			     &M0_CONF_OBJV_TYPE, NULL, &pvsub->pvs_sitevs);
	if (rc != 0) {
		rc = M0_ERR(rc);
		goto err;
	}
	pvobj->co_status = M0_CS_READY;

	/* Pre-walking state initialisation. */
	memcpy(st.bws_allowance, allowance, sizeof st.bws_allowance);
	st.bws_failed = failed;
	st.bws_failed_next = 0;
	st.bws_disk_nr = 0;
	st.bws_dirs[M0_CONF_PVER_LVL_SITES] = pvsub->pvs_sitevs;
	/* Build virtual pver subtree. */
	rc = m0_conf_walk(conf_pver_base_w, &base->pv_obj, &st);
	if (rc != 0) {
		rc = M0_ERR(rc);
		goto err;
	}
	pvsub->pvs_attr.pa_P = st.bws_disk_nr;
	/* Validate the pver attributes */
	if (!m0_pdclust_attr_check(&pvsub->pvs_attr)) {
		rc = M0_ERR(-EINVAL);
		goto err;
	}
	M0_ASSERT(pvsub->pvs_attr.pa_P > allowance[M0_CONF_PVER_LVL_DRIVES]);
	rc = conf_pver_tolerance_adjust(pver);
	if (rc != 0) {
		rc = M0_ERR(rc);
		goto err;
	}
	/* Post-walking state verification. */
	M0_ASSERT(M0_IS0(&st.bws_allowance));
	M0_ASSERT(failed == NULL ||
		  st.bws_failed_next == st.bws_failed->ai_count);

	M0_POST(m0_conf_obj_invariant(pvobj));
	*out = pver;
	return M0_RC(0);
err:
	conf_pver_subtree_delete(pvobj);
	return rc;
}

static int conf_obj_mark_deleted(struct m0_conf_obj *obj, void *args M0_UNUSED)
{
	M0_PRE(!obj->co_deleted);
	obj->co_deleted = true;
	return M0_CW_CONTINUE;
}

static void conf_pver_subtree_delete(struct m0_conf_obj *obj)
{
	int rc;

	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_PVER_TYPE);

	rc = m0_conf_walk(conf_obj_mark_deleted, obj, NULL);
	M0_ASSERT(rc == 0); /* conf_obj_mark_deleted() cannot fail */
	m0_conf_cache_gc(obj->co_cache);
}

#undef CONF_PVER_VECTOR_LOG

#undef M0_TRACE_SUBSYSTEM
/** @} conf-pvers */
