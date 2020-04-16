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
 * Original author: Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 18-Jul-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/string.h"         /* m0_streq */
#include "lib/locality.h"       /* m0_locality0_get */
#include "module/instance.h"    /* m0_get */
#include "be/domain.h"
#include "be/seg0.h"
#include "be/seg.h"
#include "be/seg_internal.h"    /* m0_be_seg_hdr */
#include "stob/stob.h"          /* m0_stob_find_by_key */
#include "stob/domain.h"        /* m0_stob_domain_init */

M0_TL_DESCR_DEFINE(zt, "m0_be_domain::bd_0types", M0_INTERNAL,
			   struct m0_be_0type, b0_linkage, b0_magic,
			   M0_BE_0TYPE_MAGIC, M0_BE_0TYPE_MAGIC);
M0_TL_DEFINE(zt, static, struct m0_be_0type);

M0_TL_DESCR_DEFINE(seg, "m0_be_domain::bd_segs", M0_INTERNAL,
			   struct m0_be_seg, bs_linkage, bs_magic,
			   M0_BE_SEG_MAGIC, M0_BE_SEG_MAGIC);
M0_TL_DEFINE(seg, static, struct m0_be_seg);

/**
 * @addtogroup be
 *
 * @{
 */

static void be_domain_lock(struct m0_be_domain *dom)
{
	m0_mutex_lock(&dom->bd_lock);
}

static void be_domain_unlock(struct m0_be_domain *dom)
{
	m0_mutex_unlock(&dom->bd_lock);
}

static int segobj_opt_iterate(struct m0_be_seg         *dict,
			      const struct m0_be_0type *objtype,
			      struct m0_buf            *opt,
			      char                    **suffix,
			      bool                      begin)
{
	struct m0_buf *buf;
	int            rc;

	rc = begin ?
		m0_be_seg_dict_begin(dict, objtype->b0_name,
				     (const char **)suffix, (void**) &buf) :
		m0_be_seg_dict_next(dict, objtype->b0_name, *suffix,
				    (const char**) suffix, (void**) &buf);

	if (rc == -ENOENT)
		return 0;
	else if (rc == 0) {
		if (buf != NULL)
			*opt = *buf;
		return +1;
	}

	return M0_RC(rc);
}

static int segobj_opt_next(struct m0_be_seg         *dict,
			   const struct m0_be_0type *objtype,
			   struct m0_buf            *opt,
			   char                    **suffix)
{
	return segobj_opt_iterate(dict, objtype, opt, suffix, false);
}

static int segobj_opt_begin(struct m0_be_seg         *dict,
			    const struct m0_be_0type *objtype,
			    struct m0_buf            *opt,
			    char                    **suffix)
{
	return segobj_opt_iterate(dict, objtype, opt, suffix, true);
}

static const char *id_cut(const char *prefix, const char *key)
{
	size_t len;
	char  *p;

	if (key == NULL)
		return key;

	p = strstr(key, prefix);
	len = strlen(prefix);

	return p == NULL || len >= strlen(key) ? NULL : p + len;
}

static int _0types_visit(struct m0_be_domain *dom, bool init)
{
	int                 rc = 0;
	int                 left;
	char               *suffix;
	const char         *id;
	struct m0_buf       opt;
	struct m0_be_seg   *dict;
	struct m0_be_0type *objtype;

	dict = m0_be_domain_seg0_get(dom);

	m0_tl_for(zt, &dom->bd_0types, objtype) {
		for (left = segobj_opt_begin(dict, objtype, &opt, &suffix);
		     left > 0 && rc == 0;
		     left = segobj_opt_next(dict, objtype, &opt, &suffix)) {
			id = id_cut(objtype->b0_name, suffix);
			rc = init ? objtype->b0_init(dom, id, &opt) :
				(objtype->b0_fini(dom, id, &opt), 0);

		}
	} m0_tl_endfor;

	return M0_RC(rc);
}

static int be_domain_stob_open(struct m0_be_domain  *dom,
			       uint64_t              stob_key,
			       const char           *stob_create_cfg,
			       struct m0_stob      **out,
			       bool                  create)
{
	int               rc;
	struct m0_stob_id stob_id;

	m0_stob_id_make(0, stob_key, &dom->bd_stob_domain->sd_id, &stob_id);
	rc = m0_stob_find(&stob_id, out);
	if (rc == 0) {
		rc = m0_stob_state_get(*out) == CSS_UNKNOWN ?
		     m0_stob_locate(*out) : 0;
		rc = rc ?: create && m0_stob_state_get(*out) == CSS_NOENT ?
		     m0_stob_create(*out, NULL, stob_create_cfg) : 0;
		rc = rc ?: m0_stob_state_get(*out) == CSS_EXISTS ? 0 : -ENOENT;
		if (rc != 0)
			m0_stob_put(*out);
	}
	M0_POST(ergo(rc == 0, m0_stob_state_get(*out) == CSS_EXISTS));
	return M0_RC(rc);
}

static int be_domain_seg_structs_create(struct m0_be_domain *dom,
					struct m0_be_tx     *tx,
					struct m0_be_seg    *seg)
{
	struct m0_be_tx_credit  cred = {};
	struct m0_be_tx         tx_ = {};
	struct m0_sm_group     *grp = m0_locality0_get()->lo_grp;
	bool                    use_local_tx = tx == NULL;
	bool                    tx_is_open;
	int                     rc;

	if (use_local_tx) {
		tx = &tx_;
		m0_sm_group_lock(grp);
		m0_be_tx_init(tx, 0, dom, grp, NULL, NULL, NULL, NULL);
		m0_be_allocator_credit(m0_be_seg_allocator(seg), M0_BAO_CREATE,
				       0, 0, &cred);
		m0_be_seg_dict_create_credit(seg, &cred);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_open_sync(tx);
		tx_is_open = rc == 0;
	} else {
		rc = 0;
		tx_is_open = false;
	}
	rc = rc ?: m0_be_allocator_create(m0_be_seg_allocator(seg), tx,
					  dom->bd_cfg.bc_zone_pcnt,
					  ARRAY_SIZE(dom->bd_cfg.bc_zone_pcnt));
	if (rc == 0)
		m0_be_seg_dict_create(seg, tx);
	if (use_local_tx) {
		if (tx_is_open)
			m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
		m0_sm_group_unlock(grp);
	}
	return M0_RC(rc);
}

/**
 * @post ergo(!destroy, rc == 0)
 */
static int be_domain_seg_close(struct m0_be_domain *dom,
			       struct m0_be_seg    *seg,
			       bool                 destroy)
{
	int rc;

	be_domain_lock(dom);
	seg_tlink_del_fini(seg);
	be_domain_unlock(dom);

	m0_be_seg_dict_fini(seg);
	m0_be_allocator_fini(m0_be_seg_allocator(seg));
	m0_be_seg_close(seg);
	rc = destroy ? m0_be_seg_destroy(seg) : 0;
	m0_be_seg_fini(seg);
	return M0_RC(rc);
}

static int be_domain_seg_open(struct m0_be_domain *dom,
			      struct m0_be_seg    *seg,
			      uint64_t             stob_key)
{
	struct m0_stob *stob;
	int             rc;

	rc = be_domain_stob_open(dom, stob_key, NULL, &stob, false);
	if (rc == 0) {
		m0_be_seg_init(seg, stob, dom, M0_BE_SEG_FAKE_ID);
		m0_stob_put(stob);
		rc = m0_be_seg_open(seg);
		if (rc == 0) {
			(void)m0_be_allocator_init(m0_be_seg_allocator(seg),
						   seg);
			m0_be_seg_dict_init(seg);

			be_domain_lock(dom);
			seg_tlink_init_at_tail(seg, &dom->bd_segs);
			be_domain_unlock(dom);
		} else {
			m0_be_seg_fini(seg);
		}
	}
	return M0_RC(rc);
}

static int be_domain_seg_destroy(struct m0_be_domain *dom,
				 uint64_t             seg_id)
{
	struct m0_be_seg seg;

	return be_domain_seg_open(dom, &seg, seg_id) ?:
	       be_domain_seg_close(dom, &seg, true);
}

static int be_domain_seg_create(struct m0_be_domain              *dom,
				struct m0_be_tx                  *tx,
				struct m0_be_seg                 *seg,
				const struct m0_be_0type_seg_cfg *seg_cfg)
{
	struct m0_stob *stob;
	int             rc;
	int             rc1;

	rc = be_domain_stob_open(dom, seg_cfg->bsc_stob_key,
				 seg_cfg->bsc_stob_create_cfg, &stob, true);
	if (rc != 0)
		goto out;
	m0_be_seg_init(seg, stob, dom, M0_BE_SEG_FAKE_ID);
	m0_stob_put(stob);
	rc = m0_be_seg_create(seg, seg_cfg->bsc_size, seg_cfg->bsc_addr);
	m0_be_seg_fini(seg);
	if (rc != 0)
		goto out;
	rc = be_domain_seg_open(dom, seg, seg_cfg->bsc_stob_key);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "can't open segment after successful "
		       "creation. seg_cfg->bsc_stob_key = %"PRIu64", rc = %d",
		       seg_cfg->bsc_stob_key, rc);
		goto seg_destroy;
	}
	if (rc == 0 && seg_cfg->bsc_preallocate)
		rc = m0_be_reg__write(&M0_BE_REG_SEG(seg));
	if (rc == 0)
		goto out;

seg_destroy:
	rc1 = be_domain_seg_destroy(dom, seg_cfg->bsc_stob_key);
	if (rc1 != 0) {
		M0_LOG(M0_ERROR, "can't destroy segment after successful "
		       "creation. seg_cfg->bsc_stob_key = %"PRIu64", rc = %d",
		       seg_cfg->bsc_stob_key, rc1);
	}
out:
	return M0_RC(rc);
}

static int be_0type_seg_init(struct m0_be_domain *dom,
			     const char          *suffix,
			     const struct m0_buf *data)
{
	struct m0_be_0type_seg_cfg *cfg =
			(struct m0_be_0type_seg_cfg *)data->b_addr;
	struct m0_be_seg           *seg;
	int                         rc;

	M0_ENTRY("suffix='%s', stob_key=%"PRIu64, suffix, cfg->bsc_stob_key);

	/* seg0 is already initialized */
	if (m0_streq(suffix, "0"))
		return 0;

	M0_ALLOC_PTR(seg);
	if (seg != NULL) {
	       rc = be_domain_seg_open(dom, seg, cfg->bsc_stob_key);
	       if (rc != 0)
		       m0_free(seg);
	}
	rc = seg == NULL ? -ENOMEM : rc;
	return M0_RC(rc);
}

static void be_0type_seg_fini(struct m0_be_domain *dom,
			      const char          *suffix,
			      const struct m0_buf *data)
{
	struct m0_be_seg *seg;

	M0_ENTRY("dom: %p, suffix: %s", dom, suffix);

	if (m0_streq(suffix, "0")) /* seg0 is finied separately */
		return;

	seg = m0_be_domain_seg_by_id(dom, m0_strtou64(suffix, NULL, 10));
	M0_ASSERT(seg != NULL);
	M0_LOG(M0_DEBUG, "seg: %p, suffix: %s", seg, suffix);

	be_domain_seg_close(dom, seg, false);
	m0_free(seg);
	M0_LEAVE();
}

/*
 * Current implementations assumes
 * m0_be_seg::bs_id == m0_stob_key_get(seg->bs_stob)
 */
static const struct m0_be_0type m0_be_0type_seg = {
	.b0_name = "M0_BE:SEG",
	.b0_init = be_0type_seg_init,
	.b0_fini = be_0type_seg_fini,
};

static void be_domain_log_cleanup(const char           *stob_domain_location,
				  struct m0_be_log_cfg *log_cfg,
				  bool                  create)
{
	const char *location_add = "-log";
	char       *location;
	size_t      size;
	int         rc;

	size  = strlen(stob_domain_location);
	size += strlen(location_add) + 1;
	location = m0_alloc(size);
	strncpy(location, stob_domain_location, size);
	strncat(location, location_add, size);
	M0_ASSERT(location[size - 1] == '\0');
	if (create) {
		m0_stob_domain__dom_id_make(
			&log_cfg->lc_store_cfg.lsc_stob_id.si_domain_fid,
			m0_stob_type_id_by_name("linuxstob"),
			0, log_cfg->lc_store_cfg.lsc_stob_domain_key);
	}
	log_cfg->lc_store_cfg.lsc_stob_domain_location = location;
	log_cfg->lc_store_cfg.lsc_stob_domain_init_cfg = "directio=true";
	if (create) {
		rc = m0_stob_domain_destroy_location(
			log_cfg->lc_store_cfg.lsc_stob_domain_location);
		/*
		 * copy-paste from M0_BE_DOMAIN_LEVEL_MKFS_STOB_DOMAIN_DESTROY.
		 */
		if (M0_IN(rc, (-ENOENT, 0))) {
			M0_LOG(M0_DEBUG, "rc = %d", rc);
		} else {
			M0_LOG(M0_WARN, "rc = %d", rc);
		}
	}
}

static int be_domain_mkfs_seg0(struct m0_be_domain              *dom,
                               const struct m0_be_0type_seg_cfg *seg0_cfg)
{
	struct m0_be_tx_credit  cred         = {};
	const struct m0_buf     seg0_cfg_buf = M0_BUF_INIT_PTR_CONST(seg0_cfg);
	struct m0_sm_group     *grp          = m0_locality0_get()->lo_grp;
	struct m0_be_tx         tx           = {};
	int                     rc;

	M0_ENTRY();

	m0_sm_group_lock(grp);
	m0_be_tx_init(&tx, 0, dom, grp, NULL, NULL, NULL, NULL);
	m0_be_0type_add_credit(dom, &dom->bd_0type_seg, "0",
			       &seg0_cfg_buf, &cred);
	m0_be_tx_prep(&tx, &cred);
	rc = m0_be_tx_exclusive_open_sync(&tx);
	if (rc == 0) {
		rc = m0_be_0type_add(&dom->bd_0type_seg, dom, &tx, "0",
		                     &seg0_cfg_buf);
		m0_be_tx_close_sync(&tx);
	}
	m0_be_tx_fini(&tx);
	m0_sm_group_unlock(grp);
	return M0_RC(rc);
}

static void be_domain_ldsc_sync(struct m0_be_log_discard      *ld,
                                struct m0_be_op               *op,
                                struct m0_be_log_discard_item *ldi)
{
	struct m0_be_domain *dom;
	struct m0_be_seg    *seg;
	struct m0_stob      *stobs[2];

	dom = container_of(ld, struct m0_be_domain, bd_log_discard);
	stobs[0] = m0_be_domain_seg0_get(dom)->bs_stob;
	seg = m0_be_domain_seg_first(dom);
	stobs[1] = seg != NULL ? seg->bs_stob : NULL;
	m0_be_pd_sync(&dom->bd_pd, 0, stobs, seg == NULL ? 1 : 2, op);
}

M0_INTERNAL void
m0_be_domain_cleanup_by_location(const char *stob_domain_location)
{
	struct m0_be_log_cfg log_cfg = {};

	/*
	 * It is safe to pass fake log_cfg into be_domain_log_cleanup().
	 * Location string is constructed inside the function. Other
	 * fields are accessed locally and not actually used.
	 */

	be_domain_log_cleanup(stob_domain_location, &log_cfg, true);
}

M0_INTERNAL struct m0_be_tx *m0_be_domain_tx_find(struct m0_be_domain *dom,
						  uint64_t id)
{
	return m0_be_engine__tx_find(m0_be_domain_engine(dom), id);
}

M0_INTERNAL struct m0_be_engine *m0_be_domain_engine(struct m0_be_domain *dom)
{
	return &dom->bd_engine;
}

M0_INTERNAL struct m0_be_seg *m0_be_domain_seg0_get(struct m0_be_domain *dom)
{
	return &dom->bd_seg0;
}

M0_INTERNAL struct m0_be_log *m0_be_domain_log(struct m0_be_domain *dom)
{
	return &m0_be_domain_engine(dom)->eng_log;
}

M0_INTERNAL struct m0_be_seg *m0_be_domain_seg(const struct m0_be_domain *dom,
					       const void                *addr)
{
	return m0_be_seg_contains(&dom->bd_seg0, addr) ?
		(struct m0_be_seg *) &dom->bd_seg0 :
		m0_tl_find(seg, seg, &dom->bd_segs,
			   m0_be_seg_contains(seg, addr));
}

M0_INTERNAL struct m0_be_seg *
m0_be_domain_seg_first(const struct m0_be_domain *dom)
{
	return m0_tl_find(seg, seg, &dom->bd_segs,
			  dom->bd_seg0.bs_id != seg->bs_id && seg->bs_id != 0);
}

M0_INTERNAL struct m0_be_seg *
m0_be_domain_seg_by_id(const struct m0_be_domain *dom, uint64_t id)
{
	return dom->bd_seg0.bs_id == id ? (struct m0_be_seg *) &dom->bd_seg0 :
		m0_tl_find(seg, seg, &dom->bd_segs, seg->bs_id == id);
}

static void be_domain_seg_suffix_make(const struct m0_be_domain *dom,
				      uint64_t                   seg_id,
				      char                      *str,
				      size_t                     str_size)
{
	int nr = snprintf(str, str_size, "%"PRIu64, seg_id);

	M0_ASSERT(nr < str_size);
}

M0_INTERNAL void
m0_be_domain_seg_create_credit(struct m0_be_domain              *dom,
			       const struct m0_be_0type_seg_cfg *seg_cfg,
			       struct m0_be_tx_credit           *cred)
{
	struct m0_be_seg_hdr fake_seg_header;
	struct m0_be_seg     fake_seg = {
		.bs_addr = &fake_seg_header,
		.bs_size = 1,
	};
	char                 suffix[64];

	be_domain_seg_suffix_make(dom, seg_cfg->bsc_stob_key,
				  suffix, ARRAY_SIZE(suffix));
	/* See next comment. The same applies to m0_be_allocator */
	m0_be_allocator_credit(NULL, M0_BAO_CREATE, 0, 0, cred);
	/*
	 * m0_be_btree credit interface was created a long time ago.
	 * It needs initalized segment allocator (at least as a parameter)
	 * to calculate credit. In m0_be_seg_dict m0_be_btree doesn't need to
	 * be allocated, so credit can be calculated without initialized
	 * allocator. First parameter should be changed to someting after proper
	 * interfaces introduced.
	 */
	m0_be_seg_dict_create_credit(&fake_seg, cred); /* XXX */
	m0_be_0type_add_credit(dom, &dom->bd_0type_seg, suffix,
			       &M0_BUF_INIT_PTR_CONST(seg_cfg), cred);
	m0_be_0type_del_credit(dom, &dom->bd_0type_seg, suffix, cred);
}

M0_INTERNAL void m0_be_domain_seg_destroy_credit(struct m0_be_domain    *dom,
						 struct m0_be_seg       *seg,
						 struct m0_be_tx_credit *cred)
{
	char suffix[64];

	be_domain_seg_suffix_make(dom, seg->bs_id, suffix, ARRAY_SIZE(suffix));
	m0_be_0type_del_credit(dom, &dom->bd_0type_seg, suffix, cred);
}

M0_INTERNAL int
m0_be_domain_seg_create(struct m0_be_domain               *dom,
			struct m0_be_tx                   *tx,
			const struct m0_be_0type_seg_cfg  *seg_cfg,
			struct m0_be_seg                 **out)
{
	struct m0_be_tx_credit  cred = {};
	struct m0_be_tx         tx_ = {};
	struct m0_sm_group     *grp = m0_locality0_get()->lo_grp;
	struct m0_be_seg       *seg;
	struct m0_be_seg        seg1 = {};
	bool                    use_local_tx = tx == NULL;
	bool                    tx_is_open;
	char                    suffix[64];
	int                     rc;
	int                     rc1;

	M0_PRE(ergo(tx != NULL, m0_be_tx__is_exclusive(tx)));
	M0_ASSERT_INFO(!seg_tlist_is_empty(&dom->bd_segs),
		       "seg0 should be added first");

	be_domain_seg_suffix_make(dom, seg_cfg->bsc_stob_key,
				  suffix, ARRAY_SIZE(suffix));

	if (use_local_tx) {
		tx = &tx_;
		m0_sm_group_lock(grp);
		m0_be_tx_init(tx, 0, dom, grp, NULL, NULL, NULL, NULL);
		m0_be_domain_seg_create_credit(dom, seg_cfg, &cred);
		m0_be_tx_prep(tx, &cred);
		rc = m0_be_tx_exclusive_open_sync(tx);
		tx_is_open = rc == 0;
	} else {
		rc = 0;
		tx_is_open = false;
	}
	rc = rc ?: be_domain_seg_create(dom, tx, &seg1, seg_cfg);
	if (rc == 0) {
		be_domain_seg_close(dom, &seg1, false);
		rc = m0_be_0type_add(&dom->bd_0type_seg, dom, tx, suffix,
				     &M0_BUF_INIT_PTR_CONST(seg_cfg));
		if (rc == 0) {
			seg = m0_be_domain_seg(dom, seg_cfg->bsc_addr);
			M0_ASSERT(seg != NULL);
			rc = be_domain_seg_structs_create(dom, tx, seg);
			if (rc != 0) {
				rc1 = m0_be_0type_del(&dom->bd_0type_seg, dom,
						      tx, suffix);
				M0_ASSERT_INFO(rc1 != 0, "rc1 = %d", rc1);
			}
		}
		if (rc != 0) {
			rc1 = be_domain_seg_destroy(dom, seg_cfg->bsc_stob_key);
			M0_LOG(M0_ERROR, "can't destroy segment "
			       "just after creation. "
			       "seg_cfg->bsc_stob_key = %"PRIu64", rc = %d",
			       seg_cfg->bsc_stob_key, rc1);
		}
	}
	if (use_local_tx) {
		if (tx_is_open)
			m0_be_tx_close_sync(tx);
		m0_be_tx_fini(tx);
		m0_sm_group_unlock(grp);
	}
	if (out != NULL) {
		*out = rc != 0 ? NULL :
				 m0_be_domain_seg(dom, seg_cfg->bsc_addr);
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_be_domain_seg_destroy(struct m0_be_domain *dom,
					 struct m0_be_tx     *tx,
					 struct m0_be_seg    *seg)
{
	uint64_t seg_id = seg->bs_id;
	char     suffix[64];
	int      rc;

	M0_PRE(m0_be_tx__is_exclusive(tx));

	be_domain_seg_suffix_make(dom, seg->bs_id, suffix, ARRAY_SIZE(suffix));
	rc = m0_be_0type_del(&dom->bd_0type_seg, dom, tx, suffix);
	rc = rc ?: be_domain_seg_destroy(dom, seg_id);
	/*
	 * It may be a problem here if 0type record for the segment was removed
	 * but be_domain_seg_destroy() failed.
	 */
	return M0_RC(rc);
}

M0_INTERNAL bool m0_be_domain_is_locked(const struct m0_be_domain *dom)
{
	/* XXX: return m0_mutex_is_locked(&dom->bd_engine.eng_lock); */
	return true;
}

M0_INTERNAL void m0_be_domain__0type_register(struct m0_be_domain *dom,
					      struct m0_be_0type  *type)
{
	be_domain_lock(dom);
	zt_tlink_init_at_tail(type, &dom->bd_0types);
	be_domain_unlock(dom);
}

M0_INTERNAL void m0_be_domain__0type_unregister(struct m0_be_domain *dom,
						struct m0_be_0type  *type)
{
	be_domain_lock(dom);
	zt_tlink_del_fini(type);
	be_domain_unlock(dom);
}

static const struct m0_modlev levels_be_domain[];

static int be_domain_level_enter(struct m0_module *module)
{
	struct m0_be_domain     *dom = M0_AMB(dom, module, bd_module);
	struct m0_be_domain_cfg *cfg = &dom->bd_cfg;
	int                      level = module->m_cur + 1;
	const char              *level_name;
	int                      rc;
	unsigned                 i;

	level_name = levels_be_domain[level].ml_name;
	M0_ENTRY("dom=%p level=%d level_name=%s", dom, level, level_name);
	switch (level) {
	case M0_BE_DOMAIN_LEVEL_INIT:
		zt_tlist_init(&dom->bd_0types);
		seg_tlist_init(&dom->bd_segs);
		m0_mutex_init(&dom->bd_engine_lock);
		m0_mutex_init(&dom->bd_lock);
		return M0_RC(0);
	case M0_BE_DOMAIN_LEVEL_0TYPES_REGISTER:
		M0_ASSERT(cfg->bc_0types_nr > 0 && cfg->bc_0types != NULL);
		M0_ALLOC_ARR(dom->bd_0types_allocated, cfg->bc_0types_nr);
		if (dom->bd_0types_allocated == NULL)
			return M0_ERR(-ENOMEM);

		dom->bd_0type_seg = m0_be_0type_seg;
		m0_be_0type_register(dom, &dom->bd_0type_seg);

		for (i = 0; i < cfg->bc_0types_nr; ++i) {
			dom->bd_0types_allocated[i] = *cfg->bc_0types[i];
			m0_be_0type_register(dom, &dom->bd_0types_allocated[i]);
		}
		return M0_RC(0);
	case M0_BE_DOMAIN_LEVEL_MKFS_STOB_DOMAIN_DESTROY:
		if (!cfg->bc_mkfs_mode)
			return M0_RC(0);
		rc = m0_stob_domain_destroy_location(
		                                cfg->bc_stob_domain_location);
		/*
		 * Silence -ENOENT error - it's a perfectly valid case for the
		 * first run if the stob domain hasn't been created yet.
		 */
		return M0_IN(rc, (-ENOENT, 0)) ? M0_RC(0) : M0_ERR(rc);
	case M0_BE_DOMAIN_LEVEL_MKFS_STOB_DOMAIN_CREATE:
		if (!cfg->bc_mkfs_mode)
			return M0_RC(0);
		/*
		 * The stob domain is never destroyed in case of failure, even
		 * in mkfs mode: it helps with the debugging.
		 */
		return M0_RC(m0_stob_domain_create(cfg->bc_stob_domain_location,
						   cfg->bc_stob_domain_cfg_init,
						   cfg->bc_stob_domain_key,
						 cfg->bc_stob_domain_cfg_create,
						   &dom->bd_stob_domain));

	case M0_BE_DOMAIN_LEVEL_NORMAL_STOB_DOMAIN_INIT:
		if (cfg->bc_mkfs_mode)
			return M0_RC(0);
		return M0_RC(m0_stob_domain_init(cfg->bc_stob_domain_location,
						 cfg->bc_stob_domain_cfg_init,
						 &dom->bd_stob_domain));
	case M0_BE_DOMAIN_LEVEL_LOG_CONFIGURE:
		cfg->bc_log.lc_got_space_cb = m0_be_engine_got_log_space_cb;
		cfg->bc_log.lc_full_cb      = m0_be_engine_full_log_cb;
		cfg->bc_log.lc_lock         = &dom->bd_engine_lock;
		/*
		 * The next temporary solution is needed as long as BE log uses
		 * direct I/O and BE segments can't work with direct I/O.
		 * The direct I/O configuration is per stob domain, not
		 * individual stobs. So BE uses 2 stob domains: one with direct
		 * I/O enabled for the log, and another one without direct I/O
		 * for segments.
		 *
		 * TODO use a single stob domain for BE after paged is
		 * implemented.
		 */
		/* temporary solution BEGIN */
		be_domain_log_cleanup(cfg->bc_stob_domain_location,
		                      &cfg->bc_log, cfg->bc_mkfs_mode);
		/* temporary solution END */
		return M0_RC(0);
	case M0_BE_DOMAIN_LEVEL_MKFS_LOG_CREATE:
		if (!cfg->bc_mkfs_mode)
			return M0_RC(0);
		return M0_RC(m0_be_log_create(m0_be_domain_log(dom),
					      &cfg->bc_log));
	case M0_BE_DOMAIN_LEVEL_NORMAL_LOG_OPEN:
		if (cfg->bc_mkfs_mode)
			return M0_RC(0);
		return M0_RC(m0_be_log_open(m0_be_domain_log(dom),
		                            &cfg->bc_log));
	case M0_BE_DOMAIN_LEVEL_PD_INIT:
		M0_ASSERT(equi(cfg->bc_seg_cfg == NULL, cfg->bc_seg_nr == 0));
		M0_ASSERT_INFO(cfg->bc_pd_cfg.bpdc_seg_io_nr >=
			       cfg->bc_engine.bec_group_nr,
			      "seg_io_nr must be at least number of tx_groups");
		cfg->bc_pd_cfg.bpdc_sched.bisc_pos_start =
			m0_be_log_recovery_discarded(m0_be_domain_log(dom));
		m0_be_tx_group_seg_io_credit(
					&cfg->bc_engine.bec_group_cfg,
					&cfg->bc_pd_cfg.bpdc_io_credit);
		return M0_RC(m0_be_pd_init(&dom->bd_pd, &cfg->bc_pd_cfg));
	case M0_BE_DOMAIN_LEVEL_NORMAL_SEG0_OPEN:
		if (cfg->bc_mkfs_mode)
			return M0_RC(0);
		return M0_RC(be_domain_seg_open(dom, m0_be_domain_seg0_get(dom),
						cfg->bc_seg0_stob_key));
	case M0_BE_DOMAIN_LEVEL_NORMAL_0TYPES_VISIT:
		if (cfg->bc_mkfs_mode)
			return M0_RC(0);
		return M0_RC(_0types_visit(dom, true));
	case M0_BE_DOMAIN_LEVEL_LOG_DISCARD_INIT:
		cfg->bc_log_discard_cfg.ldsc_sync = &be_domain_ldsc_sync;
		cfg->bc_log_discard_cfg.ldsc_discard =
					&m0_be_tx_group_discard;
		cfg->bc_log_discard_cfg.ldsc_items_pending_max =
					cfg->bc_engine.bec_group_nr;
		return M0_RC(m0_be_log_discard_init(&dom->bd_log_discard,
						    &cfg->bc_log_discard_cfg));
	case M0_BE_DOMAIN_LEVEL_ENGINE_INIT:
		cfg->bc_engine.bec_domain = dom;
		cfg->bc_engine.bec_log_discard = &dom->bd_log_discard;
		cfg->bc_engine.bec_pd = &dom->bd_pd;
		cfg->bc_engine.bec_lock = &dom->bd_engine_lock;
		return M0_RC(m0_be_engine_init(&dom->bd_engine, dom,
					       &cfg->bc_engine));
	case M0_BE_DOMAIN_LEVEL_ENGINE_START:
		return M0_RC(m0_be_engine_start(&dom->bd_engine));
	case M0_BE_DOMAIN_LEVEL_MKFS_SEG0_CREATE:
		if (!cfg->bc_mkfs_mode)
			return M0_RC(0);
		/*
		 * seg0 can only be created after BE engine started because
		 * m0_be_allocator_create() and m0_be_seg_dict_create() need
		 * engine to process a transaction.
		 */
		return M0_RC(be_domain_seg_create(dom, NULL,
						  m0_be_domain_seg0_get(dom),
						  &cfg->bc_seg0_cfg));
	case M0_BE_DOMAIN_LEVEL_MKFS_SEG0_STRUCTS_CREATE:
		if (!cfg->bc_mkfs_mode)
			return M0_RC(0);
		return M0_RC(be_domain_seg_structs_create(dom, NULL,
						  m0_be_domain_seg0_get(dom)));
	case M0_BE_DOMAIN_LEVEL_MKFS_SEG0_0TYPE:
		if (!cfg->bc_mkfs_mode)
			return M0_RC(0);
		return M0_RC(be_domain_mkfs_seg0(dom, &cfg->bc_seg0_cfg));
	case M0_BE_DOMAIN_LEVEL_MKFS_SEGMENTS_CREATE:
		if (!cfg->bc_mkfs_mode)
			return M0_RC(0);
		rc = 0;
		for (i = 0; i < cfg->bc_seg_nr; ++i) {
			/*
			 * Currently there is one transaction per segment
			 * created. It may be changed in the future.
			 */
			rc = m0_be_domain_seg_create(dom, NULL,
						     &cfg->bc_seg_cfg[i],
						     NULL);
			if (rc != 0)
				break;
		}
		return M0_RC(rc);
	case M0_BE_DOMAIN_LEVEL_READY:
		return M0_RC(0);
	default:
		M0_IMPOSSIBLE("Unexpected level: %d %s", level, level_name);
	}
}

static void be_domain_level_leave(struct m0_module *module)
{
	struct m0_be_domain *dom = M0_AMB(dom, module, bd_module);
	struct m0_be_0type  *zt;
	int                  level = module->m_cur;
	const char          *level_name = levels_be_domain[level].ml_name;
	unsigned             i;

	M0_ENTRY("dom=%p level=%d level_name=%s", dom, level, level_name);
	switch (level) {
	case M0_BE_DOMAIN_LEVEL_INIT:
		m0_tl_teardown(zt, &dom->bd_0types, zt);
		m0_mutex_fini(&dom->bd_lock);
		m0_mutex_fini(&dom->bd_engine_lock);
		seg_tlist_fini(&dom->bd_segs);
		zt_tlist_fini(&dom->bd_0types);
		break;
	case M0_BE_DOMAIN_LEVEL_0TYPES_REGISTER:
		for (i = 0; i < dom->bd_cfg.bc_0types_nr; ++i) {
			m0_be_0type_unregister(dom,
					       &dom->bd_0types_allocated[i]);
		}
		m0_be_0type_unregister(dom, &dom->bd_0type_seg);

		m0_free0(&dom->bd_0types_allocated);
		break;
	case M0_BE_DOMAIN_LEVEL_MKFS_STOB_DOMAIN_DESTROY:
		break;
	case M0_BE_DOMAIN_LEVEL_MKFS_STOB_DOMAIN_CREATE:
		break;
	case M0_BE_DOMAIN_LEVEL_NORMAL_STOB_DOMAIN_INIT:
		m0_stob_domain_fini(dom->bd_stob_domain);
		break;
	case M0_BE_DOMAIN_LEVEL_LOG_CONFIGURE:
		m0_free(dom->bd_cfg.bc_log.lc_store_cfg.
					lsc_stob_domain_location);
		break;
	case M0_BE_DOMAIN_LEVEL_MKFS_LOG_CREATE:
		break;
	case M0_BE_DOMAIN_LEVEL_NORMAL_LOG_OPEN:
		m0_be_log_close(&dom->bd_engine.eng_log);
		break;
	case M0_BE_DOMAIN_LEVEL_PD_INIT:
		m0_be_pd_fini(&dom->bd_pd);
		break;
	case M0_BE_DOMAIN_LEVEL_NORMAL_SEG0_OPEN:
		/*
		 * XXX A bug in failure handling is here.
		 * If m0_be_domain_cfg::bc_mkfs_mode is set and a failure happens
		 * after M0_BE_DOMAIN_LEVEL_NORMAL_SEG0_OPEN but before
		 * M0_BE_DOMAIN_LEVEL_MKFS_SEG0_CREATE succeeds this
		 * finalisation code will be incorrect.
		 * Some time ago seg0 was required for log. Now it's not the
		 * case. The proper solution would be to init+start engine and
		 * only then init seg0 and visit 0types. TODO implement this.
		 */
		be_domain_seg_close(dom, m0_be_domain_seg0_get(dom), false);
		break;
	case M0_BE_DOMAIN_LEVEL_NORMAL_0TYPES_VISIT:
		(void)_0types_visit(dom, false);
		break;
	case M0_BE_DOMAIN_LEVEL_LOG_DISCARD_INIT:
		break;
	case M0_BE_DOMAIN_LEVEL_ENGINE_INIT:
		M0_BE_OP_SYNC(op, m0_be_log_discard_flush(&dom->bd_log_discard,
							  &op));
		/*
		 * XXX m0_be_log_discard initialised before and finalised before
		 * BE engine. It's a bug and it should be fixed.
		 */
		m0_be_log_discard_fini(&dom->bd_log_discard);
		m0_be_engine_fini(&dom->bd_engine);
		break;
	case M0_BE_DOMAIN_LEVEL_ENGINE_START:
		m0_be_engine_stop(&dom->bd_engine);
		break;
	case M0_BE_DOMAIN_LEVEL_MKFS_SEG0_CREATE:
	case M0_BE_DOMAIN_LEVEL_MKFS_SEG0_STRUCTS_CREATE:
	case M0_BE_DOMAIN_LEVEL_MKFS_SEG0_0TYPE:
	case M0_BE_DOMAIN_LEVEL_MKFS_SEGMENTS_CREATE:
	case M0_BE_DOMAIN_LEVEL_READY:
		break;
	default:
		M0_IMPOSSIBLE("Unexpected level: %d %s", level, level_name);
	}
	M0_LEAVE();
}

#define BE_DOMAIN_LEVEL(level) [level] = {              \
		.ml_name  = #level,                     \
		.ml_enter = be_domain_level_enter,      \
		.ml_leave = be_domain_level_leave,      \
	}
static const struct m0_modlev levels_be_domain[] = {
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_INIT),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_0TYPES_REGISTER),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_MKFS_STOB_DOMAIN_DESTROY),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_MKFS_STOB_DOMAIN_CREATE),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_NORMAL_STOB_DOMAIN_INIT),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_LOG_CONFIGURE),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_MKFS_LOG_CREATE),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_NORMAL_LOG_OPEN),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_PD_INIT),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_NORMAL_SEG0_OPEN),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_NORMAL_0TYPES_VISIT),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_LOG_DISCARD_INIT),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_ENGINE_INIT),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_ENGINE_START),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_MKFS_SEG0_CREATE),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_MKFS_SEG0_STRUCTS_CREATE),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_MKFS_SEG0_0TYPE),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_MKFS_SEGMENTS_CREATE),
	BE_DOMAIN_LEVEL(M0_BE_DOMAIN_LEVEL_READY),
};
#undef BE_DOMAIN_LEVEL

M0_INTERNAL void m0_be_domain_module_setup(struct m0_be_domain           *dom,
					   const struct m0_be_domain_cfg *cfg)
{
	m0_module_setup(&dom->bd_module, "m0_be_domain module",
			levels_be_domain, ARRAY_SIZE(levels_be_domain),
			m0_get());
	dom->bd_cfg = *cfg;
}

M0_INTERNAL void m0_be_domain_tx_size_max(struct m0_be_domain    *dom,
                                          struct m0_be_tx_credit *cred,
                                          m0_bcount_t            *payload_size)
{
	m0_be_engine_tx_size_max(&dom->bd_engine, cred, payload_size);
}

M0_INTERNAL void m0_be_domain__group_limits(struct m0_be_domain *dom,
                                            uint32_t            *group_nr,
                                            uint32_t            *tx_per_group)
{
	m0_be_engine__group_limits(&dom->bd_engine, group_nr, tx_per_group);
}

M0_INTERNAL bool m0_be_domain_is_stob_log(struct m0_be_domain     *dom,
                                          const struct m0_stob_id *stob_id)
{
	bool it_is;

	be_domain_lock(dom);
	it_is = m0_be_log_contains_stob(m0_be_domain_log(dom), stob_id);
	be_domain_unlock(dom);
	return it_is;
}

M0_INTERNAL bool m0_be_domain_is_stob_seg(struct m0_be_domain     *dom,
                                          const struct m0_stob_id *stob_id)
{
	bool it_is;

	be_domain_lock(dom);
	it_is = m0_tl_exists(seg, seg, &dom->bd_segs,
	                     m0_be_seg_contains_stob(seg, stob_id));
	be_domain_unlock(dom);
	return it_is;
}

M0_INTERNAL struct m0_be_seg *
m0_be_domain_seg_by_addr(struct m0_be_domain *dom,
                         void                *addr)
{
	struct m0_be_seg *seg;

	be_domain_lock(dom);
	seg = m0_tl_find(seg, seg, &dom->bd_segs,
	                 m0_be_seg_contains(seg, addr));
	be_domain_unlock(dom);
	return seg;
}

/*
 * Note: the implementation is not as efficient as it can be for now.
 * We'll make it more efficient after page daemon is introduced.
 */
M0_INTERNAL bool m0_be_domain_seg_is_valid(struct m0_be_domain *dom,
                                           struct m0_be_seg    *seg)
{
	struct m0_be_seg *result;

	be_domain_lock(dom);
	result = m0_tl_find(seg, s, &dom->bd_segs, seg == s);
	be_domain_unlock(dom);
	return result;
}

#undef M0_TRACE_SUBSYSTEM
/** @} end of be group */

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
