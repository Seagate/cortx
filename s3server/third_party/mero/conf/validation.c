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
 * Authors:         Andriy Tkachuk <andriy.tkachuk@seagate.com>
 * Original creation date: 6-Jan-2016
 */

/**
 * @addtogroup conf_validation
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/validation.h"
#include "conf/glob.h"
#include "conf/dir.h"      /* m0_conf_dir_tl */
#include "conf/walk.h"
#include "conf/pvers.h"    /* m0_conf_pver_level */
#include "lib/string.h"    /* m0_vsnprintf */
#include "lib/errno.h"     /* ENOENT */
#include "lib/memory.h"    /* M0_ALLOC_ARR */
#include "net/net.h"       /* m0_net_endpoint_is_valid */

enum { CONF_GLOB_BATCH = 16 }; /* the value is arbitrary */

/** @see conf_io_stats_get() */
struct conf_io_stats {
	uint32_t cs_nr_ioservices;
	uint32_t cs_nr_iodevices;  /* pool width */
};

static const struct m0_conf_ruleset conf_rules;

static const struct m0_conf_ruleset *conf_validity_checks[] = {
	&conf_rules,
	/*
	 * Mero modules may define their own conf validation rules and add
	 * them here.
	 */
};

char *
m0_conf_validation_error(struct m0_conf_cache *cache, char *buf, size_t buflen)
{
	char *err;
	M0_PRE(buf != NULL && buflen != 0);

	m0_conf_cache_lock(cache);
	err = m0_conf_validation_error_locked(cache, buf, buflen);
	m0_conf_cache_unlock(cache);
	return err;
}

M0_INTERNAL char *
m0_conf_validation_error_locked(const struct m0_conf_cache *cache,
				char *buf, size_t buflen)
{
	unsigned                   i;
	const struct m0_conf_rule *rule;
	int                        rc;
	char                      *_buf;
	size_t                     _buflen;
	char                      *err;

	M0_PRE(buf != NULL && buflen != 0);
	M0_PRE(m0_conf_cache_is_locked(cache));

	for (i = 0; i < ARRAY_SIZE(conf_validity_checks); ++i) {
		for (rule = &conf_validity_checks[i]->cv_rules[0];
		     rule->cvr_name != NULL;
		     ++rule) {
			rc = snprintf(buf, buflen, "[%s.%s] ",
				      conf_validity_checks[i]->cv_name,
				      rule->cvr_name);
			M0_ASSERT(rc > 0 && (size_t)rc < buflen);
			_buflen = strlen(buf);
			_buf = buf + _buflen;
			err = rule->cvr_error(cache, _buf, buflen - _buflen);
			if (err == NULL)
				continue;
			return err == _buf ? buf : m0_vsnprintf(
				buf, buflen, "[%s.%s] %s",
				conf_validity_checks[i]->cv_name,
				rule->cvr_name, err);
		}
	}
	return NULL;
}

static char *conf_orphans_error(const struct m0_conf_cache *cache,
				char *buf, size_t buflen)
{
	const struct m0_conf_obj *obj;

	m0_tl_for(m0_conf_cache, &cache->ca_registry, obj) {
		if (obj->co_status != M0_CS_READY)
			return m0_vsnprintf(buf, buflen, FID_F" is not defined",
					    FID_P(&obj->co_id));
		if (obj->co_parent == NULL &&
		    m0_conf_obj_type(obj) != &M0_CONF_ROOT_TYPE)
			return m0_vsnprintf(buf, buflen, "Dangling object: "
					    FID_F, FID_P(&obj->co_id));
	} m0_tl_endfor;

	return NULL;
}

/*
 * XXX FIXME: conf_io_stats_get() counts _all_ IO services and sdevs.
 * This information is not very useful. The function should be modified
 * to count IO services and sdevs associated with a particular pool.
 */
static char *conf_io_stats_get(const struct m0_conf_cache *cache,
			       struct conf_io_stats *stats,
			       char *buf, size_t buflen)
{
	struct m0_conf_glob           glob;
	const struct m0_conf_obj     *objv[CONF_GLOB_BATCH];
	const struct m0_conf_service *svc;
	int                           i;
	int                           rc;

	M0_SET0(stats);

	m0_conf_glob_init(&glob, M0_CONF_GLOB_ERR, NULL, cache, NULL,
			  M0_CONF_ROOT_NODES_FID, M0_CONF_ANY_FID,
	                  M0_CONF_NODE_PROCESSES_FID, M0_CONF_ANY_FID,
	                  M0_CONF_PROCESS_SERVICES_FID, M0_CONF_ANY_FID);

	while ((rc = m0_conf_glob(&glob, ARRAY_SIZE(objv), objv)) > 0) {
		for (i = 0; i < rc; ++i) {
			svc = M0_CONF_CAST(objv[i], m0_conf_service);
			if (svc->cs_type == M0_CST_IOS) {
				++stats->cs_nr_ioservices;
				stats->cs_nr_iodevices +=
					m0_conf_dir_len(svc->cs_sdevs);
			}
		}
	}
	return rc < 0 ? m0_conf_glob_error(&glob, buf, buflen) : NULL;
}

static bool conf_oostore_mode(const struct m0_conf_root *r)
{
	bool result = r->rt_mdredundancy > 0;
	M0_LOG(M0_INFO, FID_F": mdredundancy=%"PRIu32" ==> oostore_mode=%s",
	       FID_P(&r->rt_obj.co_id), r->rt_mdredundancy,
	       result ? "true" : "false");
	return result;
}

static char *_conf_root_error(const struct m0_conf_root *root,
			      char *buf, size_t buflen,
			      struct conf_io_stats *stats)
{
	const struct m0_conf_obj *obj;

	if (!conf_oostore_mode(root))
		/*
		 * Meta-data is stored at MD services, not IO services.
		 * No special MD pool is required.
		 */
		return NULL;

	if (root->rt_mdredundancy > stats->cs_nr_ioservices)
		return m0_vsnprintf(buf, buflen,
				    FID_F": metadata redundancy (%u) exceeds"
				    " the number of IO services (%u)",
				    FID_P(&root->rt_obj.co_id),
				    root->rt_mdredundancy,
				    stats->cs_nr_ioservices);
	obj = m0_conf_cache_lookup(root->rt_obj.co_cache, &root->rt_mdpool);
	if (obj == NULL)
		return m0_vsnprintf(buf, buflen,
				    FID_F": `mdpool' "FID_F" is missing",
				    FID_P(&root->rt_obj.co_id),
				    FID_P(&root->rt_mdpool));
	if (m0_conf_obj_grandparent(obj) != &root->rt_obj)
		return m0_vsnprintf(buf, buflen,
				    FID_F": `mdpool' "FID_F" belongs another"
				    " profile", FID_P(&root->rt_obj.co_id),
				    FID_P(&root->rt_mdpool));
	return NULL;
}

static char *conf_root_error(const struct m0_conf_cache *cache,
			     char *buf, size_t buflen)
{
	struct conf_io_stats      stats;
	struct m0_conf_glob       glob;
	const struct m0_conf_obj *obj;
	char                     *err;
	int                       rc;

	err = conf_io_stats_get(cache, &stats, buf, buflen);
	if (err != NULL)
		return err;

	if (stats.cs_nr_ioservices == 0)
		return m0_vsnprintf(buf, buflen, "No IO services");

	m0_conf_glob_init(&glob, M0_CONF_GLOB_ERR, NULL, cache, NULL, M0_FID0);

	while ((rc = m0_conf_glob(&glob, 1, &obj)) > 0) {
		err = _conf_root_error(M0_CONF_CAST(obj, m0_conf_root),
				       buf, buflen, &stats);
		if (err != NULL)
			return err;
	}
	return rc < 0 ? m0_conf_glob_error(&glob, buf, buflen) : NULL;
}

static const struct m0_conf_node *
conf_node_from_sdev(const struct m0_conf_sdev *sdev)
{
	return M0_CONF_CAST(
		m0_conf_obj_grandparent(
			m0_conf_obj_grandparent(
				m0_conf_obj_grandparent(&sdev->sd_obj))),
		m0_conf_node);
}

static char *conf_iodev_error(const struct m0_conf_sdev *sdev,
			      const struct m0_conf_sdev **iodevs,
			      uint32_t nr_iodevs, char *buf, size_t buflen)
{
	uint32_t j;

	if (M0_CONF_CAST(m0_conf_obj_grandparent(&sdev->sd_obj),
			 m0_conf_service)->cs_type != M0_CST_IOS)
		return NULL;
	if (sdev->sd_dev_idx >= nr_iodevs)
		return m0_vsnprintf(buf, buflen, FID_F": dev_idx (%u) does not"
				    " belong [0, P) range; P=%u",
				    FID_P(&sdev->sd_obj.co_id),
				    sdev->sd_dev_idx, nr_iodevs);
	if (iodevs[sdev->sd_dev_idx] != sdev) {
		if (iodevs[sdev->sd_dev_idx] != NULL)
			return m0_vsnprintf(
				buf, buflen, FID_F": dev_idx is not unique,"
				" duplicates that of "FID_F,
				FID_P(&sdev->sd_obj.co_id),
				FID_P(&iodevs[sdev->sd_dev_idx]->sd_obj.co_id));
		/*
		 * XXX TODO: Check that sdev->sd_filename is not empty.
		 */
		if (m0_exists(i, nr_iodevs, iodevs[j = i] != NULL &&
			      m0_streq(iodevs[i]->sd_filename,
				       sdev->sd_filename) &&
			      conf_node_from_sdev(sdev) ==
			      conf_node_from_sdev(iodevs[i])))
			return m0_vsnprintf(
				buf, buflen, FID_F": filename is not unique,"
				" duplicates that of "FID_F,
				FID_P(&sdev->sd_obj.co_id),
				FID_P(&iodevs[j]->sd_obj.co_id));
		iodevs[sdev->sd_dev_idx] = sdev;
	}
	return NULL;
}

struct conf_pver_width_st {
	uint32_t *ws_width;
	char     *ws_buf;
	size_t    ws_buflen;
	char     *ws_err;
};

static int conf_pver_width_measure__dir(const struct m0_conf_obj *obj,
					struct conf_pver_width_st *st)
{
	const struct m0_tl       *dir;
	const struct m0_conf_obj *child;
	uint32_t                  children_level;

	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_DIR_TYPE);

	dir = &M0_CONF_CAST(obj, m0_conf_dir)->cd_items;
	if (m0_conf_dir_tlist_is_empty(dir)) {
		st->ws_err = m0_vsnprintf(st->ws_buf, st->ws_buflen,
					  FID_F": Missing children",
					  FID_P(&obj->co_parent->co_id));
		return M0_CW_STOP;
	}
	M0_ASSERT(m0_tl_forall(m0_conf_dir, x, dir,
			       m0_conf_obj_type(x) == &M0_CONF_OBJV_TYPE));
	children_level =
		m0_conf_obj_type(obj->co_parent) == &M0_CONF_PVER_TYPE ?
		M0_CONF_PVER_LVL_SITES :
		m0_conf_pver_level(obj->co_parent) + 1;
	if (m0_tl_exists(m0_conf_dir, x, dir, child = x,
			 m0_conf_pver_level(x) != children_level)) {
		st->ws_err = m0_vsnprintf(st->ws_buf, st->ws_buflen,
					  FID_F": Cannot adopt "FID_F,
					  FID_P(&obj->co_parent->co_id),
					  FID_P(&child->co_id));
		return M0_CW_STOP;
	}
	return M0_CW_CONTINUE;
}

static int conf_pver_width_measure_w(struct m0_conf_obj *obj, void *args)
{
	struct conf_pver_width_st *st = args;
	const struct m0_conf_objv *objv;
	unsigned                   level;

	M0_PRE(!m0_conf_obj_is_stub(obj));

	if (m0_conf_obj_type(obj) == &M0_CONF_DIR_TYPE)
		return conf_pver_width_measure__dir(obj, st);

	objv = M0_CONF_CAST(obj, m0_conf_objv);
	level = m0_conf_pver_level(obj);
	if ((level == M0_CONF_PVER_LVL_DRIVES) != (objv->cv_children == NULL)) {
		st->ws_err = m0_vsnprintf(st->ws_buf, st->ws_buflen,
					  FID_F": %s children",
					  FID_P(&obj->co_id),
					  objv->cv_children == NULL ?
					  "Missing" : "Unexpected");
		return M0_CW_STOP;
	}
	M0_CNT_INC(st->ws_width[level]);
	return M0_CW_CONTINUE;
}

/**
 * Computes the number of objvs at each level of pver subtree.
 *
 * Puts result into `pver_width' array, which should have capacity of
 * at least M0_CONF_PVER_HEIGHT elements.
 */
static char *conf_pver_width_error(const struct m0_conf_pver *pver,
				   uint32_t *pver_width,
				   size_t pver_width_nr,
				   char *buf, size_t buflen)
{
	struct conf_pver_width_st st = {
		.ws_width  = pver_width,
		.ws_buf    = buf,
		.ws_buflen = buflen
	};
	int rc;

	M0_PRE(pver->pv_kind == M0_CONF_PVER_ACTUAL);

	rc = m0_conf_walk(conf_pver_width_measure_w,
			  &pver->pv_u.subtree.pvs_sitevs->cd_obj, &st);
	M0_ASSERT(rc == 0); /* conf_pver_width_measure_w() cannot fail */
	M0_POST(ergo(st.ws_err == NULL,
		     m0_forall(i, pver_width_nr, (pver_width[i] > 0))));
	return st.ws_err;
}

/**
 * Tries to find base pver of given formulaic pver.
 *
 * @see conf_pver_formulaic_base()
 */
static char *conf_pver_formulaic_base_error(const struct m0_conf_pver *fpver,
					    const struct m0_conf_pver **out,
					    char *buf, size_t buflen)
{
	const struct m0_conf_obj *base;

	M0_PRE(fpver->pv_kind == M0_CONF_PVER_FORMULAIC);

	base = m0_conf_cache_lookup(fpver->pv_obj.co_cache,
				    &fpver->pv_u.formulaic.pvf_base);
	if (base == NULL)
		return m0_vsnprintf(buf, buflen,
				    "Base "FID_F" of formulaic pver "FID_F
				    " is missing",
				    FID_P(&fpver->pv_u.formulaic.pvf_base),
				    FID_P(&fpver->pv_obj.co_id));
	*out = M0_CONF_CAST(base, m0_conf_pver);
	return (*out)->pv_kind == M0_CONF_PVER_ACTUAL ? NULL :
		m0_vsnprintf(buf, buflen, "Base "FID_F" of formulaic pver "FID_F
			     " is not actual", FID_P(&base->co_id),
			     FID_P(&fpver->pv_obj.co_id));
}

static char *conf_pver_formulaic_error(const struct m0_conf_pver *fpver,
				       char *buf, size_t buflen)
{
	const struct m0_conf_pver_formulaic *form = &fpver->pv_u.formulaic;
	const struct m0_conf_obj     *pool;
	const struct m0_conf_pver    *base = NULL;
	const struct m0_pdclust_attr *base_attr;
	uint32_t                      pver_width[M0_CONF_PVER_HEIGHT] = {};
	char                         *err;
	int                           i;

	pool = m0_conf_obj_grandparent(&fpver->pv_obj);
	if (m0_fid_eq(&pool->co_id, &M0_CONF_CAST(m0_conf_obj_grandparent(pool),
						  m0_conf_root)->rt_mdpool))
		return m0_vsnprintf(buf, buflen, FID_F": MD pool may not have"
				    " formulaic pvers", FID_P(&pool->co_id));
	err = conf_pver_formulaic_base_error(fpver, &base, buf, buflen) ?:
		conf_pver_width_error(base, pver_width, ARRAY_SIZE(pver_width),
				      buf, buflen);
	if (err != NULL)
		return err;
	if (M0_IS0(&form->pvf_allowance))
		return m0_vsnprintf(buf, buflen, FID_F": Zeroed allowance",
				    FID_P(&fpver->pv_obj.co_id));
	if (m0_exists(j, ARRAY_SIZE(pver_width),
		      form->pvf_allowance[i = j] > pver_width[j]))
		return m0_vsnprintf(buf, buflen, FID_F": Allowing more"
				    " failures (%u) than there are objects (%u)"
				    " at level %d of base pver subtree",
				    FID_P(&fpver->pv_obj.co_id),
				    form->pvf_allowance[i], pver_width[i], i);
	base_attr = &base->pv_u.subtree.pvs_attr;
	/* Guaranteed by pver_check(). */
	M0_ASSERT(m0_pdclust_attr_check(base_attr));
	if (form->pvf_allowance[M0_CONF_PVER_LVL_DRIVES] > base_attr->pa_P -
	    base_attr->pa_N - 2*base_attr->pa_K)
		return m0_vsnprintf(buf, buflen, FID_F": Number of allowed disk"
				    " failures (%u) > P - N - 2K of base pver",
				    FID_P(&fpver->pv_obj.co_id),
				    form->pvf_allowance[
					    M0_CONF_PVER_LVL_DRIVES]);
	/*
	 * XXX TODO: Check if form->pvf_id is unique per cluster.
	 */
	return NULL;
}

static char *conf_pver_actual_error(const struct m0_conf_pver *pver,
				    const struct m0_conf_sdev **iodevs,
				    uint32_t nr_iodevs,
				    char *buf, size_t buflen)
{
	const struct m0_conf_pver_subtree *sub = &pver->pv_u.subtree;
	uint32_t                   pver_width[M0_CONF_PVER_HEIGHT] = {};
	struct m0_conf_glob        glob;
	const struct m0_conf_obj  *objs[CONF_GLOB_BATCH];
	const struct m0_conf_objv *diskv;
	char                      *err;
	int                        i;
	int                        rc;

	err = conf_pver_width_error(pver, pver_width, ARRAY_SIZE(pver_width),
				    buf, buflen);
	if (err != NULL)
		return err;
	if (M0_IS0(&sub->pvs_tolerance) && sub->pvs_attr.pa_N != 1)
		return m0_vsnprintf(buf, buflen, FID_F": Zeroed tolerance is"
				    " supported only with N = 1",
				    FID_P(&pver->pv_obj.co_id));
	if (m0_exists(j, ARRAY_SIZE(pver_width),
		      sub->pvs_tolerance[i = j] > pver_width[j]))
		return m0_vsnprintf(buf, buflen, FID_F": Tolerating more"
				    " failures (%u) than there are objects (%u)"
				    " at level %d of pver subtree",
				    FID_P(&pver->pv_obj.co_id),
				    sub->pvs_tolerance[i], pver_width[i], i);
	if (!m0_pdclust_attr_check(&sub->pvs_attr))
		return m0_vsnprintf(buf, buflen, FID_F": P < N + 2K",
				    FID_P(&pver->pv_obj.co_id));
	/*
	 * XXX TODO: Check if m0_conf_objv::cv_real pointers are unique.
	 */
	if (sub->pvs_attr.pa_P > nr_iodevs)
		return m0_vsnprintf(buf, buflen,
				    FID_F": Pool width (%u) exceeds total"
				    " number of IO devices (%u)",
				    FID_P(&pver->pv_obj.co_id),
				    sub->pvs_attr.pa_P, nr_iodevs);

	m0_conf_glob_init(&glob, M0_CONF_GLOB_ERR, NULL, NULL, &pver->pv_obj,
			  M0_CONF_PVER_SITEVS_FID, M0_CONF_ANY_FID,
			  M0_CONF_SITEV_RACKVS_FID, M0_CONF_ANY_FID,
			  M0_CONF_RACKV_ENCLVS_FID, M0_CONF_ANY_FID,
			  M0_CONF_ENCLV_CTRLVS_FID, M0_CONF_ANY_FID,
			  M0_CONF_CTRLV_DRIVEVS_FID, M0_CONF_ANY_FID);

	while ((rc = m0_conf_glob(&glob, ARRAY_SIZE(objs), objs)) > 0) {
		for (i = 0; i < rc; ++i) {
			diskv = M0_CONF_CAST(objs[i], m0_conf_objv);
			err = conf_iodev_error(
				M0_CONF_CAST(diskv->cv_real,
					     m0_conf_drive)->ck_sdev,
				iodevs, nr_iodevs, buf, buflen);
			if (err != NULL)
				return err;
		}
	}
	return rc < 0 ? m0_conf_glob_error(&glob, buf, buflen) : NULL;
}

static char *
conf_pvers_error(const struct m0_conf_cache *cache, char *buf, size_t buflen)
{
	struct conf_io_stats        stats;
	const struct m0_conf_sdev **iodevs;
	struct m0_conf_glob         glob;
	const struct m0_conf_obj   *objv[CONF_GLOB_BATCH];
	const struct m0_conf_pver  *pver;
	char                       *err;
	int                         i;
	int                         rc;

	err = conf_io_stats_get(cache, &stats, buf, buflen);
	if (err != NULL)
		return err;
	/*
	 * XXX FIXME: Check the number of IO devices associated with a
	 * particular pool, not the total number of IO devices in cluster.
	 */
	if (stats.cs_nr_iodevices == 0)
		return m0_vsnprintf(buf, buflen, "No IO devices");

	M0_ALLOC_ARR(iodevs, stats.cs_nr_iodevices);
	if (iodevs == NULL)
		return m0_vsnprintf(buf, buflen, "Insufficient memory");

	m0_conf_glob_init(&glob, M0_CONF_GLOB_ERR, NULL, cache, NULL,
			  M0_CONF_ROOT_POOLS_FID, M0_CONF_ANY_FID,
			  M0_CONF_POOL_PVERS_FID, M0_CONF_ANY_FID);

	while ((rc = m0_conf_glob(&glob, ARRAY_SIZE(objv), objv)) > 0) {
		for (i = 0; i < rc; ++i) {
			pver = M0_CONF_CAST(objv[i], m0_conf_pver);
			err = pver->pv_kind == M0_CONF_PVER_ACTUAL ?
				conf_pver_actual_error(pver, iodevs,
						       stats.cs_nr_iodevices,
						       buf, buflen) :
				conf_pver_formulaic_error(pver, buf, buflen);
			if (err != NULL)
				break;
		}
	}
	m0_free(iodevs);
	if (err != NULL)
		return err;

	return rc < 0 ? m0_conf_glob_error(&glob, buf, buflen) : NULL;
}

static char *conf_process_endpoint_error(const struct m0_conf_process *proc,
					 char *buf, size_t buflen)
{
	struct m0_conf_glob       glob;
	const struct m0_conf_obj *objv[CONF_GLOB_BATCH];
	const char              **epp;
	int                       i;
	int                       rc;

	if (!m0_net_endpoint_is_valid(proc->pc_endpoint))
		return m0_vsnprintf(buf, buflen, FID_F": Invalid endpoint: %s",
				    FID_P(&proc->pc_obj.co_id),
				    proc->pc_endpoint);

	m0_conf_glob_init(&glob, M0_CONF_GLOB_ERR, NULL, NULL, &proc->pc_obj,
			  M0_CONF_PROCESS_SERVICES_FID, M0_CONF_ANY_FID);

	while ((rc = m0_conf_glob(&glob, ARRAY_SIZE(objv), objv)) > 0) {
		for (i = 0; i < rc; ++i) {
			for (epp = M0_CONF_CAST(objv[i],
						m0_conf_service)->cs_endpoints;
			     *epp != NULL; ++epp) {
				if (!m0_net_endpoint_is_valid(*epp))
					return m0_vsnprintf(
						buf, buflen,
						FID_F": Invalid endpoint: %s",
						FID_P(&objv[i]->co_id), *epp);
			}
		}
	}
	return rc < 0 ? m0_conf_glob_error(&glob, buf, buflen) : NULL;
}

static char *
conf_endpoint_error(const struct m0_conf_cache *cache, char *buf, size_t buflen)
{
	struct m0_conf_glob       glob;
	const struct m0_conf_obj *objv[CONF_GLOB_BATCH];
	char                     *err;
	int                       i;
	int                       rc;

	m0_conf_glob_init(&glob, M0_CONF_GLOB_ERR, NULL, cache, NULL,
			  M0_CONF_ROOT_NODES_FID, M0_CONF_ANY_FID,
			  M0_CONF_NODE_PROCESSES_FID, M0_CONF_ANY_FID);

	while ((rc = m0_conf_glob(&glob, ARRAY_SIZE(objv), objv)) > 0) {
		for (i = 0; i < rc; ++i) {
			err = conf_process_endpoint_error(
				M0_CONF_CAST(objv[i], m0_conf_process),
				buf, buflen);
			if (err != NULL)
				return err;
		}
	}
	return rc < 0 ? m0_conf_glob_error(&glob, buf, buflen) : NULL;
}

static char *conf_service_type_error(const struct m0_conf_cache *cache,
				     char *buf, size_t buflen)
{
	struct m0_conf_glob       glob;
	const struct m0_conf_obj *objv[CONF_GLOB_BATCH];
	enum m0_conf_service_type svc_type;
	bool                      confd_p = false;
	int                       i;
	int                       rc;

	m0_conf_glob_init(&glob, M0_CONF_GLOB_ERR, NULL, cache, NULL,
			  M0_CONF_ROOT_NODES_FID, M0_CONF_ANY_FID,
			  M0_CONF_NODE_PROCESSES_FID, M0_CONF_ANY_FID,
			  M0_CONF_PROCESS_SERVICES_FID, M0_CONF_ANY_FID);

	while ((rc = m0_conf_glob(&glob, ARRAY_SIZE(objv), objv)) > 0) {
		for (i = 0; i < rc; ++i) {
			svc_type = M0_CONF_CAST(objv[i],
						m0_conf_service)->cs_type;
			if (!m0_conf_service_type_is_valid(svc_type))
				return m0_vsnprintf(
					buf, buflen,
					FID_F": Invalid service type: %d",
					FID_P(&objv[i]->co_id), svc_type);
			if (svc_type == M0_CST_CONFD)
				confd_p = true;
		}
	}
	if (rc < 0)
		return m0_conf_glob_error(&glob, buf, buflen);
	if (confd_p)
		return NULL;
	return m0_vsnprintf(buf, buflen, "No confd service defined");
}

static char *_conf_service_sdevs_error(const struct m0_conf_service *svc,
				       char *buf, size_t buflen)
{
	bool has_sdevs = !m0_conf_dir_tlist_is_empty(&svc->cs_sdevs->cd_items);

	if (M0_IN(svc->cs_type, (M0_CST_IOS, M0_CST_CAS)))
		return has_sdevs ? NULL : m0_vsnprintf(
			buf, buflen,
			FID_F": `sdevs' of %s service must not be empty",
			FID_P(&svc->cs_obj.co_id),
			svc->cs_type == M0_CST_IOS ? "an IO" : "a CAS");
	return has_sdevs ? m0_vsnprintf(
		buf, buflen,
		FID_F": `sdevs' must be empty (neither IOS nor CAS)",
		FID_P(&svc->cs_obj.co_id)) : NULL;
}

static char *conf_service_sdevs_error(const struct m0_conf_cache *cache,
				      char *buf, size_t buflen)
{
	struct m0_conf_glob       glob;
	const struct m0_conf_obj *objv[CONF_GLOB_BATCH];
	char                     *err;
	int                       i;
	int                       rc;

	m0_conf_glob_init(&glob, M0_CONF_GLOB_ERR, NULL, cache, NULL,
			  M0_CONF_ROOT_NODES_FID, M0_CONF_ANY_FID,
			  M0_CONF_NODE_PROCESSES_FID, M0_CONF_ANY_FID,
			  M0_CONF_PROCESS_SERVICES_FID, M0_CONF_ANY_FID);

	while ((rc = m0_conf_glob(&glob, ARRAY_SIZE(objv), objv)) > 0) {
		for (i = 0; i < rc; ++i) {
			err = _conf_service_sdevs_error(
				M0_CONF_CAST(objv[i], m0_conf_service),
				buf, buflen);
			if (err != NULL)
				return err;
		}
	}
	return rc < 0 ? m0_conf_glob_error(&glob, buf, buflen) : NULL;
}

static const struct m0_conf_ruleset conf_rules = {
	.cv_name  = "m0_conf_rules",
	.cv_rules = {
#define _ENTRY(name) { #name, name }
		_ENTRY(conf_orphans_error),
		_ENTRY(conf_root_error),
		_ENTRY(conf_endpoint_error),
		_ENTRY(conf_service_type_error),
		_ENTRY(conf_service_sdevs_error),
		_ENTRY(conf_pvers_error),
#undef _ENTRY
		{ NULL, NULL }
	}
};

#undef M0_TRACE_SUBSYSTEM
/** @} */

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
