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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 08/24/2010
 */

#include "balloc/balloc.h"

#include "be/extmap.h"
#include "be/seg.h"
#include "be/seg0.h"		/* m0_be_0type */

#include "dtm/dtm.h"		/* m0_dtx */

#include "fid/fid.h"		/* m0_fid */

#include "lib/finject.h"
#include "lib/errno.h"
#include "lib/locality.h"	/* m0_locality0_get */
#include "lib/memory.h"
#include "lib/string.h"
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ADSTOB
#include "lib/trace.h"		/* M0_LOG */

#include "addb2/addb2.h"
#include "module/instance.h"	/* m0_get */

#include "stob/ad.h"
#include "stob/ad_private.h"
#include "stob/ad_private_xc.h"
#include "stob/addb2.h"
#include "stob/domain.h"
#include "stob/io.h"
#include "stob/module.h"	/* m0_stob_ad_module */
#include "stob/stob.h"
#include "stob/stob_internal.h"	/* m0_stob__fid_set */
#include "stob/type.h"		/* m0_stob_type */
#include "be/domain.h"

/**
 * @addtogroup stobad
 *
 * @{
 */

enum {
	STOB_TYPE_AD = 0x02,
	/*
	 * As balloc space is exhausted or fragmented -
	 * we may not succeed the allocation in one request.
	 * From another side, we need to establish maximum
	 * possible number of fragments we may have from balloc
	 * to calculate BE credits at stob_ad_write_credit().
	 * That's why we have this constant. Too big value
	 * will result in excess credits, too low value will
	 * result in early ENOSPC errors when there is still
	 * some space available in balloc. So here might be
	 * the tradeoff.
	 */
	BALLOC_FRAGS_MAX = 2,
};

struct ad_domain_init_cfg {
	struct m0_be_domain *dic_dom;
};

struct ad_domain_cfg {
	struct m0_stob_id adg_id;
	struct m0_be_seg *adg_seg;
	m0_bcount_t       adg_container_size;
	uint32_t          adg_bshift;
	m0_bcount_t       adg_blocks_per_group;
	m0_bcount_t       adg_spare_blocks_per_group;
};

struct ad_domain_map {
	char                      adm_path[MAXPATHLEN];
	struct m0_stob_ad_domain *adm_dom;
	struct m0_tlink           adm_linkage;
	uint64_t                  adm_magic;
};

enum m0_stob_ad_0type_rec_format_version {
	M0_STOB_AD_0TYPE_REC_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_STOB_AD_0TYPE_REC_FORMAT_VERSION */
	/*M0_STOB_AD_0TYPE_REC_FORMAT_VERSION_2,*/
	/*M0_STOB_AD_0TYPE_REC_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_STOB_AD_0TYPE_REC_FORMAT_VERSION = M0_STOB_AD_0TYPE_REC_FORMAT_VERSION_1
};

static const struct m0_bob_type stob_ad_domain_bob_type = {
	.bt_name         = "m0_stob_ad_domain",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_stob_ad_domain, sad_magix),
	.bt_magix        = M0_STOB_AD_DOMAIN_MAGIC,
};
M0_BOB_DEFINE(static, &stob_ad_domain_bob_type, m0_stob_ad_domain);

static struct m0_stob_domain_ops stob_ad_domain_ops;
static struct m0_stob_ops stob_ad_ops;

static int stob_ad_io_init(struct m0_stob *stob, struct m0_stob_io *io);
static void stob_ad_write_credit(const struct m0_stob_domain *dom,
				 const struct m0_stob_io     *iv,
				 struct m0_be_tx_credit      *accum);
static void
stob_ad_rec_frag_undo_redo_op_cred(const struct m0_fol_frag *frag,
				   struct m0_be_tx_credit       *accum);
static int stob_ad_rec_frag_undo_redo_op(struct m0_fol_frag *frag,
					 struct m0_be_tx	*tx);

M0_FOL_FRAG_TYPE_DECLARE(stob_ad_rec_frag, static,
			 stob_ad_rec_frag_undo_redo_op,
			 stob_ad_rec_frag_undo_redo_op,
			 stob_ad_rec_frag_undo_redo_op_cred,
			 stob_ad_rec_frag_undo_redo_op_cred);
static int stob_ad_seg_free(struct m0_dtx *tx,
			    struct m0_stob_ad_domain *adom,
			    const struct m0_be_emap_seg *seg,
			    const struct m0_ext *ext,
			    uint64_t val);
static int stob_ad_punch(struct m0_stob *stob, struct m0_indexvec *range,
                         struct m0_dtx *tx);

M0_TL_DESCR_DEFINE(ad_domains, "ad stob domains", static, struct ad_domain_map,
		   adm_linkage, adm_magic, M0_AD_DOMAINS_MAGIC,
		   M0_AD_DOMAINS_HEAD_MAGIC);
M0_TL_DEFINE(ad_domains, static, struct ad_domain_map);

static int stob_ad_0type_init(struct m0_be_domain *dom,
			      const char *suffix,
			      const struct m0_buf *data)
{
	struct m0_stob_ad_module *module = &m0_get()->i_stob_ad_module;
	struct stob_ad_0type_rec *rec = data->b_addr;
	struct ad_domain_map     *ad;
	int                       rc;

	M0_PRE(rec != NULL && data->b_nob == sizeof(*rec));
	M0_PRE(strlen(suffix) < ARRAY_SIZE(ad->adm_path));

	M0_ALLOC_PTR(ad);
	rc = ad == NULL ? -ENOMEM : 0;

	if (rc == 0) {
		/* XXX won't be stored as pointer */
		ad->adm_dom = rec->sa0_ad_domain;
		strncpy(ad->adm_path, suffix, sizeof(ad->adm_path));
		m0_mutex_lock(&module->sam_lock);
		ad_domains_tlink_init_at_tail(ad, &module->sam_domains);
		m0_mutex_unlock(&module->sam_lock);
	}

	return M0_RC(rc);
}

static void stob_ad_0type_fini(struct m0_be_domain *dom,
			       const char *suffix,
			       const struct m0_buf *data)
{
	struct m0_stob_ad_module *module = &m0_get()->i_stob_ad_module;
	struct stob_ad_0type_rec *rec = data->b_addr;
	struct ad_domain_map     *ad;

	M0_PRE(rec != NULL && data->b_nob == sizeof(*rec));

	m0_mutex_lock(&module->sam_lock);
	ad = m0_tl_find(ad_domains, ad, &module->sam_domains,
			m0_streq(suffix, ad->adm_path));
	M0_ASSERT(ad != NULL);
	ad_domains_tlink_del_fini(ad);
	m0_free(ad);
	m0_mutex_unlock(&module->sam_lock);
}

struct m0_be_0type m0_stob_ad_0type = {
	.b0_name = "M0_BE:AD",
	.b0_init = stob_ad_0type_init,
	.b0_fini = stob_ad_0type_fini
};

M0_INTERNAL struct m0_stob_ad_domain *
stob_ad_domain2ad(const struct m0_stob_domain *dom)
{
	struct m0_stob_ad_domain *adom;

	adom = (struct m0_stob_ad_domain *)dom->sd_private;
	m0_stob_ad_domain_bob_check(adom);
	M0_ASSERT(m0_stob_domain__dom_key(m0_stob_domain_id_get(dom)) ==
		  adom->sad_dom_key);

	return adom;
}

M0_INTERNAL struct m0_balloc *
m0_stob_ad_domain2balloc(const struct m0_stob_domain *dom)
{
	struct m0_stob_ad_domain *adom = stob_ad_domain2ad(dom);

	return b2m0(adom->sad_ballroom);
}

static struct m0_stob_ad *stob_ad_stob2ad(const struct m0_stob *stob)
{
	return container_of(stob, struct m0_stob_ad, ad_stob);
}

static void stob_ad_type_register(struct m0_stob_type *type)
{
	struct m0_stob_ad_module *module = &m0_get()->i_stob_ad_module;
	int                       rc;

	M0_FOL_FRAG_TYPE_INIT(stob_ad_rec_frag, "AD record fragment");
	rc = m0_fol_frag_type_register(&stob_ad_rec_frag_type);
	M0_ASSERT(rc == 0); /* XXX void */
	m0_mutex_init(&module->sam_lock);
	ad_domains_tlist_init(&module->sam_domains);
}

static void stob_ad_type_deregister(struct m0_stob_type *type)
{
	struct m0_stob_ad_module *module = &m0_get()->i_stob_ad_module;

	ad_domains_tlist_fini(&module->sam_domains);
	m0_mutex_fini(&module->sam_lock);
	m0_fol_frag_type_deregister(&stob_ad_rec_frag_type);
}

M0_INTERNAL void m0_stob_ad_init_cfg_make(char **str, struct m0_be_domain *dom)
{
	char buf[0x40];

	snprintf(buf, ARRAY_SIZE(buf), "%p", dom);
	*str = m0_strdup(buf);
}

M0_INTERNAL void m0_stob_ad_cfg_make(char **str,
				     const struct m0_be_seg *seg,
				     const struct m0_stob_id *bstore_id,
				     const m0_bcount_t size)
{
	char buf[0x400];

	snprintf(buf, ARRAY_SIZE(buf), "%p:"FID_F":"FID_F":%lu", seg,
			FID_P(&bstore_id->si_domain_fid),
			FID_P(&bstore_id->si_fid),
			size);
	*str = m0_strdup(buf);
}

static int stob_ad_domain_cfg_init_parse(const char *str_cfg_init,
					 void **cfg_init)
{
	struct ad_domain_init_cfg *cfg;
	int                        rc;

	M0_ASSERT(str_cfg_init != NULL); /* TODO: remove this assert */
	if (str_cfg_init == NULL)
		return M0_ERR(-EINVAL);

	M0_ALLOC_PTR(cfg);
	if (cfg == NULL)
		return M0_ERR(-ENOMEM);

	rc = sscanf(str_cfg_init, "%p", (void **)&cfg->dic_dom);
	*cfg_init = cfg;
	M0_ASSERT(rc == 1); /* TODO: remove this assert */
	return rc == 1 ? 0 : -EINVAL;
}

static void stob_ad_domain_cfg_init_free(void *cfg_init)
{
	m0_free(cfg_init);
}

static int stob_ad_domain_cfg_create_parse(const char *str_cfg_create,
					   void **cfg_create)
{
	struct ad_domain_cfg *cfg;
	m0_bcount_t           grp_blocks;
	int                   rc;

	if (str_cfg_create == NULL)
		return M0_ERR(-EINVAL);

	M0_ALLOC_PTR(cfg);
	if (cfg != NULL) {
		/* format = seg:domain_fid:fid:container_size */
		rc = sscanf(str_cfg_create, "%p:"FID_SF":"FID_SF":%lu",
			    (void **)&cfg->adg_seg,
			    FID_S(&cfg->adg_id.si_domain_fid),
			    FID_S(&cfg->adg_id.si_fid),
			    &cfg->adg_container_size);
		rc = rc == 6 ? 0 : -EINVAL;
	} else
		rc = -ENOMEM;

	if (rc == 0) {
		if (cfg->adg_container_size == 0)
			cfg->adg_container_size = BALLOC_DEF_CONTAINER_SIZE;
		cfg->adg_bshift     = BALLOC_DEF_BLOCK_SHIFT;
		/*
		 * Big number of groups slows balloc initialisation. Therefore,
		 * group size is counted depending on BALLOC_DEF_GROUPS_NR.
		 * Group size must be power of 2.
		 */
		grp_blocks = (cfg->adg_container_size >> cfg->adg_bshift) /
			     BALLOC_DEF_GROUPS_NR;
		grp_blocks = 1 << m0_log2(grp_blocks);
		grp_blocks = max64u(grp_blocks, BALLOC_DEF_BLOCKS_PER_GROUP);
		cfg->adg_blocks_per_group = grp_blocks;
		cfg->adg_spare_blocks_per_group =
			m0_stob_ad_spares_calc(grp_blocks);
		M0_LOG(M0_DEBUG, "device size %lu", cfg->adg_container_size);
		*cfg_create = cfg;
	}
	return M0_RC(rc);
}

/*
 * XXX @todo: A more sophisticated version of this function is necessary,
 * that will take into account the number of pool versions that the disk
 * belongs to, along with parameters of pdclust. The following module will
 * return a value around 20 % of blocks per group.
 *
 * On other note, reserving a fraction K / (N + K) is too conservative as
 * it takes into consideration the case when on failure all parity groups
 * need to be repaired (which is true only when N + 2K == P).
 * Probably K / P is the  right ratio.
 */
M0_INTERNAL m0_bcount_t m0_stob_ad_spares_calc(m0_bcount_t grp_blocks)
{
#ifdef __SPARE__SPACE__

	return  grp_blocks % 5 == 0 ? grp_blocks / 5 : grp_blocks / 5 + 1;
#else
	return 0;
#endif
}

static void stob_ad_domain_cfg_create_free(void *cfg_create)
{
	m0_free(cfg_create);
}

M0_INTERNAL bool m0_stob_ad_domain__invariant(struct m0_stob_ad_domain *adom)
{
	return _0C(adom->sad_ballroom != NULL);
}

static struct m0_sm_group *stob_ad_sm_group(void)
{
	return m0_locality0_get()->lo_grp;
}

static int stob_ad_bstore(struct m0_stob_id *stob_id, struct m0_stob **out)
{
	struct m0_stob *stob;
	int		rc;

	rc = m0_stob_find(stob_id, &stob);
	if (rc == 0) {
		if (m0_stob_state_get(stob) == CSS_UNKNOWN)
			rc = m0_stob_locate(stob);
		if (rc != 0 || m0_stob_state_get(stob) != CSS_EXISTS) {
			m0_stob_put(stob);
			rc = rc ?: -ENOENT;
		}
	}
	*out = rc == 0 ? stob : NULL;
	return M0_RC(rc);
}

static struct m0_stob_ad_domain *
stob_ad_domain_locate(const char *location_data)
{
	struct m0_stob_ad_module *module = &m0_get()->i_stob_ad_module;
	struct ad_domain_map *ad;

	m0_mutex_lock(&module->sam_lock);
	ad = m0_tl_find(ad_domains, ad, &module->sam_domains,
			m0_streq(location_data, ad->adm_path));
	m0_mutex_unlock(&module->sam_lock);
	return ad == NULL ? NULL : ad->adm_dom;
}

static int stob_ad_domain_init(struct m0_stob_type *type,
			       const char *location_data,
			       void *cfg_init,
			       struct m0_stob_domain **out)
{
	struct ad_domain_init_cfg *cfg = cfg_init;
	struct m0_stob_ad_domain  *adom;
	struct m0_stob_domain     *dom;
	struct m0_be_seg          *seg;
	struct m0_ad_balloc       *ballroom;
	bool                       balloc_inited;
	int                        rc = 0;

	adom = stob_ad_domain_locate(location_data);
	if (adom == NULL)
		return M0_RC(-ENOENT);
	else
		seg = m0_be_domain_seg(cfg->dic_dom, adom);

	if (seg == NULL) {
		M0_LOG(M0_ERROR, "segment doesn't exist for addr=%p", adom);
		return M0_ERR(-EINVAL);
	}

	M0_ASSERT(m0_stob_ad_domain__invariant(adom));

	M0_ALLOC_PTR(dom);
	if (dom == NULL)
		return M0_ERR(-ENOMEM);

	m0_stob_domain__dom_id_make(&dom->sd_id,
				    m0_stob_type_id_get(type),
				    0, adom->sad_dom_key);
	dom->sd_private = adom;
	dom->sd_ops     = &stob_ad_domain_ops;
	m0_be_emap_init(&adom->sad_adata, seg);

	ballroom = adom->sad_ballroom;
	m0_balloc_init(b2m0(ballroom));
	rc = ballroom->ab_ops->bo_init(ballroom, seg,
				       adom->sad_bshift,
				       adom->sad_container_size,
				       adom->sad_blocks_per_group,
#ifdef __SPARE_SPACE__
				       adom->sad_spare_blocks_per_group);
#else
					0);
#endif
	balloc_inited = rc == 0;

	rc = rc ?: stob_ad_bstore(&adom->sad_bstore_id,
				  &adom->sad_bstore);
	if (rc != 0) {
		if (balloc_inited)
			ballroom->ab_ops->bo_fini(ballroom);
		m0_be_emap_fini(&adom->sad_adata);
		m0_free(dom);
	} else {
		m0_stob_ad_domain_bob_init(adom);
		adom->sad_be_seg   = seg;
		adom->sad_babshift = adom->sad_bshift -
				m0_stob_block_shift(adom->sad_bstore);
		M0_LOG(M0_DEBUG, "sad_bshift = %lu\tstob bshift=%lu",
		       (unsigned long)adom->sad_bshift,
		       (unsigned long)m0_stob_block_shift(adom->sad_bstore));
		M0_ASSERT(adom->sad_babshift >= 0);
	}

	if (rc == 0)
		*out = dom;
	return rc == 0 ? M0_RC(rc) : M0_ERR(rc);
}

static void stob_ad_domain_fini(struct m0_stob_domain *dom)
{
	struct m0_stob_ad_domain *adom = stob_ad_domain2ad(dom);
	struct m0_ad_balloc      *ballroom = adom->sad_ballroom;

	ballroom->ab_ops->bo_fini(ballroom);
	m0_be_emap_fini(&adom->sad_adata);
	m0_stob_put(adom->sad_bstore);
	m0_stob_ad_domain_bob_fini(adom);
	m0_free(dom);
}

static void stob_ad_domain_create_credit(struct m0_be_seg *seg,
					 const char *location_data,
					 struct m0_be_tx_credit *accum)
{
	struct m0_be_emap map = {};
	struct m0_buf     data = { .b_nob = sizeof(struct stob_ad_0type_rec) };

	M0_BE_ALLOC_CREDIT_PTR((struct m0_stob_ad_domain *)NULL, seg, accum);
	m0_be_emap_init(&map, seg);
	m0_be_emap_credit(&map, M0_BEO_CREATE, 1, accum);
	m0_be_emap_fini(&map);
	m0_be_0type_add_credit(seg->bs_domain, &m0_stob_ad_0type,
			       location_data, &data, accum);
}

static void stob_ad_domain_destroy_credit(struct m0_be_seg *seg,
					  const char *location_data,
					  struct m0_be_tx_credit *accum)
{
	struct m0_be_emap map = {};

	M0_BE_FREE_CREDIT_PTR((struct m0_stob_ad_domain *)NULL, seg, accum);
	m0_be_emap_init(&map, seg);
	m0_be_emap_credit(&map, M0_BEO_DESTROY, 1, accum);
	m0_be_emap_fini(&map);
	m0_be_0type_del_credit(seg->bs_domain, &m0_stob_ad_0type,
			       location_data, accum);
}

/* TODO Make cleanup on fail. */
static int stob_ad_domain_create(struct m0_stob_type *type,
				 const char *location_data,
				 uint64_t dom_key,
				 void *cfg_create)
{
	struct ad_domain_cfg     *cfg = (struct ad_domain_cfg *)cfg_create;
	struct m0_be_seg         *seg = cfg->adg_seg;
	struct m0_sm_group       *grp = stob_ad_sm_group();
	struct m0_stob_ad_domain *adom;
	struct m0_be_emap        *emap;
	struct m0_balloc         *cb = NULL;
	struct m0_be_tx           tx = {};
	struct m0_be_tx_credit    cred = M0_BE_TX_CREDIT(0, 0);
	struct stob_ad_0type_rec  seg0_ad_rec;
	struct m0_buf             seg0_data;
	int                       rc;

	M0_PRE(seg != NULL);
	M0_PRE(strlen(location_data) < ARRAY_SIZE(adom->sad_path));

	adom = stob_ad_domain_locate(location_data);
	if (adom != NULL)
		return M0_ERR(-EEXIST);

	m0_sm_group_lock(grp);
	m0_be_tx_init(&tx, 0, seg->bs_domain, grp, NULL, NULL, NULL, NULL);
	stob_ad_domain_create_credit(seg, location_data, &cred);
	m0_be_tx_prep(&tx, &cred);
	/* m0_balloc_create() makes own local transaction thereby must be called
	 * before openning of exclusive transaction. m0_balloc_destroy() is not
	 * implemented, so balloc won't be cleaned up on a further fail.
	 */
	rc = m0_balloc_create(dom_key, seg, grp, &cb);
	rc = rc ?: m0_be_tx_exclusive_open_sync(&tx);

	M0_ASSERT(adom == NULL);
	if (rc == 0)
		M0_BE_ALLOC_PTR_SYNC(adom, seg, &tx);
	if (adom != NULL) {
		m0_format_header_pack(&adom->sad_header, &(struct m0_format_tag){
			.ot_version = M0_STOB_AD_DOMAIN_FORMAT_VERSION,
			.ot_type    = M0_FORMAT_TYPE_STOB_AD_DOMAIN,
			.ot_footer_offset =
				offsetof(struct m0_stob_ad_domain, sad_footer)
		});
		adom->sad_dom_key          = dom_key;
		adom->sad_container_size   = cfg->adg_container_size;
		adom->sad_bshift           = cfg->adg_bshift;
		adom->sad_blocks_per_group = cfg->adg_blocks_per_group;
#ifdef __SPARE_SPACE__
		adom->sad_spare_blocks_per_group =
			cfg->adg_spare_blocks_per_group;
#endif
		adom->sad_bstore_id        = cfg->adg_id;
		adom->sad_overwrite        = false;
		strcpy(adom->sad_path, location_data);
		m0_format_footer_update(adom);
		emap = &adom->sad_adata;
		m0_be_emap_init(emap, seg);
		rc = M0_BE_OP_SYNC_RET(
			op,
			m0_be_emap_create(emap, &tx, &op),
			bo_u.u_emap.e_rc);
		m0_be_emap_fini(emap);

		seg0_ad_rec = (struct stob_ad_0type_rec){.sa0_ad_domain = adom}; /* XXX won't be a pointer */
		m0_format_header_pack(&seg0_ad_rec.sa0_header, &(struct m0_format_tag){
			.ot_version = M0_STOB_AD_0TYPE_REC_FORMAT_VERSION,
			.ot_type    = M0_FORMAT_TYPE_STOB_AD_0TYPE_REC,
			.ot_footer_offset = offsetof(struct stob_ad_0type_rec, sa0_footer)
		});
		m0_format_footer_update(&seg0_ad_rec);
		seg0_data   = M0_BUF_INIT_PTR(&seg0_ad_rec);
		rc = rc ?: m0_be_0type_add(&m0_stob_ad_0type, seg->bs_domain,
					   &tx, location_data, &seg0_data);
		if (rc == 0) {
			adom->sad_ballroom = &cb->cb_ballroom;
			M0_BE_TX_CAPTURE_PTR(seg, &tx, adom);
		}

		m0_be_tx_close_sync(&tx);
	}

	m0_be_tx_fini(&tx);
	m0_sm_group_unlock(grp);

	if (adom == NULL && rc == 0)
		rc = M0_ERR(-ENOMEM);

	return M0_RC(rc);
}

static int stob_ad_domain_destroy(struct m0_stob_type *type,
				  const char *location_data)
{
	struct m0_stob_ad_domain *adom = stob_ad_domain_locate(location_data);
	struct m0_sm_group       *grp  = stob_ad_sm_group();
	struct m0_be_emap        *emap = &adom->sad_adata;
	struct m0_be_seg         *seg;
	struct m0_be_tx           tx   = {};
	struct m0_be_tx_credit    cred = M0_BE_TX_CREDIT(0, 0);
	int                       rc;

	if (adom == NULL)
		return 0;

	seg = adom->sad_be_seg;
	m0_sm_group_lock(grp);
	m0_be_tx_init(&tx, 0, seg->bs_domain, grp, NULL, NULL, NULL, NULL);
	stob_ad_domain_destroy_credit(seg, location_data, &cred);
	m0_be_tx_prep(&tx, &cred);
	rc = m0_be_tx_exclusive_open_sync(&tx);
	if (rc == 0) {
		m0_be_emap_init(emap, seg);
		rc = M0_BE_OP_SYNC_RET(op, m0_be_emap_destroy(emap, &tx, &op),
				       bo_u.u_emap.e_rc);
		rc = rc ?: m0_be_0type_del(&m0_stob_ad_0type, seg->bs_domain,
					   &tx, location_data);
		if (rc == 0)
			M0_BE_FREE_PTR_SYNC(adom, seg, &tx);
		m0_be_tx_close_sync(&tx);
	}
	m0_be_tx_fini(&tx);
	m0_sm_group_unlock(grp);

	/* m0_balloc_destroy() isn't implemented */

	return M0_RC(rc);
}

static struct m0_stob *stob_ad_alloc(struct m0_stob_domain *dom,
				     const struct m0_fid *stob_fid)
{
	struct m0_stob_ad *adstob;

	M0_ALLOC_PTR(adstob);
	return adstob == NULL ? NULL : &adstob->ad_stob;
}

static void stob_ad_free(struct m0_stob_domain *dom,
			 struct m0_stob *stob)
{
	struct m0_stob_ad *adstob = stob_ad_stob2ad(stob);
	m0_free(adstob);
}

static int stob_ad_cfg_parse(const char *str_cfg_create, void **cfg_create)
{
	return 0;
}

static void stob_ad_cfg_free(void *cfg_create)
{
}

static int stob_ad_init(struct m0_stob *stob,
			struct m0_stob_domain *dom,
			const struct m0_fid *stob_fid)
{
	struct m0_stob_ad_domain *adom = stob_ad_domain2ad(dom);
	struct m0_be_emap_cursor  it = {};
	struct m0_uint128         prefix;
	int                       rc;

	prefix = M0_UINT128(stob_fid->f_container, stob_fid->f_key);
	M0_LOG(M0_DEBUG, U128X_F, U128_P(&prefix));
	stob->so_ops = &stob_ad_ops;
	rc = M0_BE_OP_SYNC_RET_WITH(
		&it.ec_op,
		m0_be_emap_lookup(&adom->sad_adata, &prefix, 0, &it),
		bo_u.u_emap.e_rc);
	if (rc == 0) {
		m0_be_emap_close(&it);
	}
	return rc == -ESRCH ? -ENOENT : rc;
}

static void stob_ad_fini(struct m0_stob *stob)
{
}

static void stob_ad_create_credit(struct m0_stob_domain *dom,
				  struct m0_be_tx_credit *accum)
{
	struct m0_stob_ad_domain *adom = stob_ad_domain2ad(dom);
	m0_be_emap_credit(&adom->sad_adata, M0_BEO_INSERT, 1, accum);
}

static int stob_ad_create(struct m0_stob *stob,
			  struct m0_stob_domain *dom,
			  struct m0_dtx *dtx,
			  const struct m0_fid *stob_fid,
			  void *cfg)
{
	struct m0_stob_ad_domain *adom = stob_ad_domain2ad(dom);
	struct m0_uint128         prefix;

	M0_PRE(dtx != NULL);
	prefix = M0_UINT128(stob_fid->f_container, stob_fid->f_key);
	M0_LOG(M0_DEBUG, U128X_F, U128_P(&prefix));
	return M0_BE_OP_SYNC_RET(op,
				 m0_be_emap_obj_insert(&adom->sad_adata,
						       &dtx->tx_betx, &op,
						       &prefix, AET_HOLE),
				 bo_u.u_emap.e_rc);
}

/**
 * Function to calculate credit required for punch operation.
 * Iterate the stob extents from the want indexvec which provides range for
 * punch and accumulate the credits required.
 * Once all extents are iterated, @got indexvec contains granted credits that
 * are used for punch operation.
 * The credits are calculated only for the range of segments provided by
 * user which is a vector of extents to be punched. If not all but part of the
 * credits are granted, user is responsible to handle the transaction break and
 * request for the remaining credits in the next transaction to complete the
 * corresponding punch operation.
 * Note: It is the responsibility of user to free @got indexvec.
 *
 * @param want  range provided by the user.
 * @param got   range for which credits are granted.
 */
static int stob_ad_punch_credit(struct m0_stob *stob,
                                struct m0_indexvec *want,
				struct m0_indexvec *got,
                                struct m0_be_tx_credit *accum)
{
	m0_bcount_t               count;
	m0_bcount_t               offset;
	m0_bcount_t               segs = 0;
	struct m0_ivec_cursor     cur;
	int                       rc = 0;
	struct m0_be_tx_credit    cred;
	struct m0_be_emap_cursor  it;
	struct m0_be_emap_seg    *seg = NULL;
	struct m0_be_engine      *eng;
	struct m0_stob_ad_domain *adom;
	struct m0_ad_balloc      *ballroom;
	struct m0_ext             todo = {
					.e_start = 0,
					.e_end   = M0_BCOUNT_MAX
				  };

	M0_ENTRY("stob:%p, want:%p", stob, want);
	adom = stob_ad_domain2ad(m0_stob_dom_get(stob));
	ballroom = adom->sad_ballroom;
	m0_ivec_cursor_init(&cur, want);
	rc = stob_ad_cursor(adom, stob, 0, &it);
	if (rc != 0) {
		return M0_ERR(rc);
	}
	eng = m0_be_domain_engine(m0_be_emap_seg_domain(it.ec_map));
	count = 0;
	while (!m0_ivec_cursor_move(&cur, count)) {
		offset = m0_ivec_cursor_index(&cur);
		count  = m0_ivec_cursor_step(&cur);
		todo.e_start = offset;
		todo.e_end   = offset + count;
		rc = stob_ad_cursor(adom, stob, offset, &it);
		if (rc != 0)
			return M0_ERR(rc);

		seg = m0_be_emap_seg_get(&it);
		M0_ASSERT(m0_ext_is_valid(&seg->ee_ext) &&
			!m0_ext_is_empty(&seg->ee_ext));
		M0_LOG(M0_DEBUG, "stob:%p todo:"EXT_F ", existing ext:"EXT_F,
				stob, EXT_P(&todo), EXT_P(&seg->ee_ext));
		M0_SET0(&cred);
		m0_be_emap_credit(&adom->sad_adata, M0_BEO_PASTE, 1, &cred);
		ballroom->ab_ops->bo_free_credit(ballroom, 3, &cred);
		if (m0_be_should_break(eng, accum, &cred))
			break;
		count = seg->ee_ext.e_end - offset;
		M0_CNT_INC(segs);
		m0_be_tx_credit_add(accum, &cred);
		if (m0_be_emap_ext_is_last(&seg->ee_ext))
			break;
	}
	m0_be_emap_close(&it);

	if (segs == 0)
		return M0_RC(-EBUSY);
	got->iv_index[0] = want->iv_index[0];
	if (m0_be_emap_ext_is_last(&seg->ee_ext) ||
	    want->iv_vec.v_count[0] < seg->ee_ext.e_end)
		got->iv_vec.v_count[0] = want->iv_vec.v_count[0];
	else
		got->iv_vec.v_count[0] = seg->ee_ext.e_end;
	return M0_RC(0);
}

/**
 * Punches ad stob ext map and releases underlying storage object's
 * extents.
 *
 * @param @todo the target ext to delete, which will be marked as a hole.
 */
static int ext_punch(struct m0_stob *stob, struct m0_dtx *tx,
		     struct m0_ext  *todo)
{
	struct m0_stob_ad_domain *adom;
	struct m0_be_emap_cursor  it = {};
	struct m0_be_op          *it_op;
	struct m0_ext            *ext;
	int                       rc;

	adom = stob_ad_domain2ad(m0_stob_dom_get(stob));
	rc = stob_ad_cursor(adom, stob, todo->e_start, &it);
	if (rc != 0)
		return M0_ERR(rc);
	ext = &it.ec_seg.ee_ext;
	if (M0_FI_ENABLED("test-ext-release")) {
		/*
		 * Assert the target and existing extents are same, this
		 * ensures that existing extent at this offset is released
		 * (punched) before new extent is allocated and data is written
		 * at the same offset.
		 */
		M0_ASSERT(m0_ext_equal(todo, ext));
	}

	it_op = &it.ec_op;
	M0_SET0(it_op);
	m0_be_op_init(it_op);
	M0_LOG(M0_DEBUG, "ext ="EXT_F, EXT_P(todo));
	m0_be_emap_paste(&it, &tx->tx_betx, todo, AET_HOLE,
		 LAMBDA(void, (struct m0_be_emap_seg *__seg) {
			/* handle extent deletion. */
			rc = rc ?: stob_ad_seg_free(tx, adom, __seg,
						    &__seg->ee_ext,
						    __seg->ee_val);
		}),
		 LAMBDA(void, (struct m0_be_emap_seg *__seg,
			       struct m0_ext *__ext,
			       uint64_t __val) {
			/* cut left */
			M0_ASSERT(__ext->e_start > __seg->ee_ext.e_start);

			__seg->ee_val = __val;
			rc = rc ?: stob_ad_seg_free(tx, adom, __seg,
						    __ext, __val);
		}),
		 LAMBDA(void, (struct m0_be_emap_seg *__seg,
			       struct m0_ext *__ext,
			       uint64_t __val) {
			/* cut right */
			M0_ASSERT(__seg->ee_ext.e_end > __ext->e_end);
			if (__val < AET_MIN) {
				__seg->ee_val = __val +
					(__ext->e_end - __seg->ee_ext.e_start);
				/*
				 * Free physical sub-extent, but only when
				 * sub-extent starts at the left boundary of
				 * the logical extent, because otherwise
				 * "cut left" already freed it.
				 */
				if (__ext->e_start == __seg->ee_ext.e_start)
					rc = rc ?: stob_ad_seg_free(tx, adom,
								    __seg,
								    __ext,
								    __val);
			} else
				__seg->ee_val = __val;
		}));

	M0_ASSERT(m0_be_op_is_done(it_op));
	rc = m0_be_emap_op_rc(&it);
	m0_be_op_fini(it_op);
	return M0_RC(rc);
}

static void stob_ad_destroy_credit(struct m0_stob *stob,
				   struct m0_be_tx_credit *accum)
{
	struct m0_stob_ad_domain *adom;

	adom = stob_ad_domain2ad(m0_stob_dom_get(stob));
	m0_be_emap_credit(&adom->sad_adata, M0_BEO_DELETE, 1, accum);
}

static int stob_ad_destroy(struct m0_stob *stob, struct m0_dtx *tx)
{
	struct m0_stob_ad_domain *adom;
	struct m0_uint128         prefix;
	int                       rc;
	const struct m0_fid      *fid = m0_stob_fid_get(stob);

	adom   = stob_ad_domain2ad(m0_stob_dom_get(stob));
	prefix = M0_UINT128(fid->f_container, fid->f_key);
	rc = M0_BE_OP_SYNC_RET(op,
			       m0_be_emap_obj_delete(&adom->sad_adata,
						     &tx->tx_betx, &op,
						     &prefix),
			       bo_u.u_emap.e_rc);

	return M0_RC(rc);
}

/**
 * Truncates ad stob ext map and releases underlying storage object's extents.
 *
 * Stob is punched at location spanned by the 'range'.
 */
static int stob_ad_punch(struct m0_stob *stob, struct m0_indexvec *range,
			 struct m0_dtx *tx)
{
	struct m0_ext         todo;
	m0_bcount_t           count;
	m0_bcount_t           offset;
	struct m0_ivec_cursor cur;
	int                   rc = 0;

	m0_ivec_cursor_init(&cur, range);
	count = 0;
	while (!m0_ivec_cursor_move(&cur, count)) {
		offset = m0_ivec_cursor_index(&cur);
		count  = m0_ivec_cursor_step(&cur);
		todo.e_start = offset;
		todo.e_end   = offset + count;
		M0_LOG(M0_DEBUG, "stob %p, punching"EXT_F, stob, EXT_P(&todo));
		rc = ext_punch(stob, tx, &todo);
		if (rc != 0)
			return M0_ERR(rc);
	}
	return M0_RC(0);
}

static uint32_t stob_ad_block_shift(struct m0_stob *stob)
{
	struct m0_stob_ad_domain *adom;

	adom = stob_ad_domain2ad(m0_stob_dom_get(stob));
	return m0_stob_block_shift(adom->sad_bstore);
}

static struct m0_stob_type_ops stob_ad_type_ops = {
	.sto_register		     = &stob_ad_type_register,
	.sto_deregister		     = &stob_ad_type_deregister,
	.sto_domain_cfg_init_parse   = &stob_ad_domain_cfg_init_parse,
	.sto_domain_cfg_init_free    = &stob_ad_domain_cfg_init_free,
	.sto_domain_cfg_create_parse = &stob_ad_domain_cfg_create_parse,
	.sto_domain_cfg_create_free  = &stob_ad_domain_cfg_create_free,
	.sto_domain_init	     = &stob_ad_domain_init,
	.sto_domain_create	     = &stob_ad_domain_create,
	.sto_domain_destroy	     = &stob_ad_domain_destroy,
};

static struct m0_stob_domain_ops stob_ad_domain_ops = {
	.sdo_fini		= &stob_ad_domain_fini,
	.sdo_stob_alloc	    	= &stob_ad_alloc,
	.sdo_stob_free	    	= &stob_ad_free,
	.sdo_stob_cfg_parse 	= &stob_ad_cfg_parse,
	.sdo_stob_cfg_free  	= &stob_ad_cfg_free,
	.sdo_stob_init	    	= &stob_ad_init,
	.sdo_stob_create_credit	= &stob_ad_create_credit,
	.sdo_stob_create	= &stob_ad_create,
	.sdo_stob_write_credit	= &stob_ad_write_credit,
};

static struct m0_stob_ops stob_ad_ops = {
	.sop_fini            = &stob_ad_fini,
	.sop_destroy_credit  = &stob_ad_destroy_credit,
	.sop_destroy         = &stob_ad_destroy,
	.sop_punch_credit    = &stob_ad_punch_credit,
	.sop_punch           = &stob_ad_punch,
	.sop_io_init         = &stob_ad_io_init,
	.sop_block_shift     = &stob_ad_block_shift,
};

const struct m0_stob_type m0_stob_ad_type = {
	.st_ops  = &stob_ad_type_ops,
	.st_fidt = {
		.ft_id   = STOB_TYPE_AD,
		.ft_name = "adstob",
	},
};

/*
 * Adieu
 */

static const struct m0_stob_io_op stob_ad_io_op;

static bool stob_ad_endio(struct m0_clink *link);
static void stob_ad_io_release(struct m0_stob_ad_io *aio);

static int stob_ad_io_init(struct m0_stob *stob, struct m0_stob_io *io)
{
	struct m0_stob_ad_io *aio;
	int                   rc;

	M0_PRE(io->si_state == SIS_IDLE);

	M0_ALLOC_PTR(aio);
	if (aio != NULL) {
		io->si_stob_private = aio;
		io->si_op = &stob_ad_io_op;
		aio->ai_fore = io;
		m0_stob_io_init(&aio->ai_back);
		m0_clink_init(&aio->ai_clink, &stob_ad_endio);
		m0_clink_add_lock(&aio->ai_back.si_wait, &aio->ai_clink);
		rc = 0;
	} else {
		rc = M0_ERR(-ENOMEM);
	}
	return M0_RC(rc);
}

static void stob_ad_io_fini(struct m0_stob_io *io)
{
	struct m0_stob_ad_io *aio = io->si_stob_private;
	stob_ad_io_release(aio);
	m0_clink_del_lock(&aio->ai_clink);
	m0_clink_fini(&aio->ai_clink);
	m0_stob_io_fini(&aio->ai_back);
	m0_free(aio);
}

static void *stob_ad_addr_open(const void *buf, uint32_t shift)
{
	uint64_t addr = (uint64_t)buf;

	M0_PRE(((addr << shift) >> shift) == addr);
	return (void *)(addr << shift);
}

/**
   Helper function to allocate a given number of blocks in the underlying
   storage object.
 */
static int stob_ad_balloc(struct m0_stob_ad_domain *adom, struct m0_dtx *tx,
			  m0_bcount_t count, struct m0_ext *out,
			  uint64_t alloc_type)
{
	struct m0_ad_balloc *ballroom = adom->sad_ballroom;
	int                  rc;

	count >>= adom->sad_babshift;
	M0_LOG(M0_DEBUG, "count=%lu", (unsigned long)count);
	M0_ASSERT(count > 0);
	rc = ballroom->ab_ops->bo_alloc(ballroom, tx, count, out, alloc_type);
	out->e_start <<= adom->sad_babshift;
	out->e_end   <<= adom->sad_babshift;
	m0_ext_init(out);

	return M0_RC(rc);
}

/**
   Helper function to free a given block extent in the underlying storage
   object.
 */
static int stob_ad_bfree(struct m0_stob_ad_domain *adom, struct m0_dtx *tx,
			 struct m0_ext *ext)
{
	struct m0_ad_balloc *ballroom = adom->sad_ballroom;
	struct m0_ext        tgt;

	M0_PRE((ext->e_start & ((1ULL << adom->sad_babshift) - 1)) == 0);
	M0_PRE((ext->e_end   & ((1ULL << adom->sad_babshift) - 1)) == 0);

	tgt.e_start = ext->e_start >> adom->sad_babshift;
	tgt.e_end   = ext->e_end   >> adom->sad_babshift;
	m0_ext_init(&tgt);
	return ballroom->ab_ops->bo_free(ballroom, tx, &tgt);
}

M0_INTERNAL int stob_ad_cursor(struct m0_stob_ad_domain *adom,
			       struct m0_stob *obj,
			       uint64_t offset,
			       struct m0_be_emap_cursor *it)
{
	const struct m0_fid *fid = m0_stob_fid_get(obj);
	struct m0_uint128    prefix;
	int                  rc;

	prefix = M0_UINT128(fid->f_container, fid->f_key);
	M0_LOG(M0_DEBUG, FID_F, FID_P(fid));
	M0_SET0(&it->ec_op);
	rc = M0_BE_OP_SYNC_RET_WITH(
		&it->ec_op,
		m0_be_emap_lookup(&adom->sad_adata, &prefix, offset, it),
		bo_u.u_emap.e_rc);
	return M0_RC(rc);
}

static uint32_t stob_ad_write_map_count(struct m0_stob_ad_domain *adom,
					struct m0_indexvec *iv, bool pack)
{
	uint32_t               frags;
	m0_bcount_t            frag_size;
	m0_bcount_t            grp_size;
	bool                   eov;
	struct m0_ivec_cursor  it;

	M0_ENTRY("dom=%p bshift=%u babshift=%d pack=%hhx", adom,
		 adom->sad_bshift, adom->sad_babshift, pack);

	frags = 0;
	m0_ivec_cursor_init(&it, iv);
	grp_size = adom->sad_blocks_per_group << adom->sad_babshift;

	if (pack)
		m0_indexvec_pack(iv);
	do {
		frag_size = min_check(m0_ivec_cursor_step(&it), grp_size);
		M0_ASSERT(frag_size > 0);
		M0_ASSERT(frag_size <= (size_t)~0ULL);
		M0_LOG(M0_DEBUG, "frag_size=0x%lx", frag_size);

		eov = m0_ivec_cursor_move(&it, frag_size);

		M0_CNT_INC(frags);
	} while (!eov);

	M0_LEAVE("dom=%p frags=%u", adom, frags);
	return frags;
}

static void stob_ad_write_credit(const struct m0_stob_domain *dom,
				 const struct m0_stob_io     *io,
				 struct m0_be_tx_credit      *accum)
{
	struct m0_stob_ad_domain *adom = stob_ad_domain2ad(dom);
	struct m0_ad_balloc      *ballroom = adom->sad_ballroom;
	/* XXX discard const, because stob_ad_write_map_count() changes iv */
	struct m0_indexvec       *iv = (struct m0_indexvec *) &io->si_stob;
	int                       bfrags = BALLOC_FRAGS_MAX;
	int                       frags;

	frags = stob_ad_write_map_count(adom, iv, false);
	M0_LOG(M0_DEBUG, "frags=%d", frags);
	frags = max_check(frags, bfrags);

	if (ballroom->ab_ops->bo_alloc_credit != NULL)
		ballroom->ab_ops->bo_alloc_credit(ballroom, bfrags, accum);

	/*
	 * XXX We don't know if MERO-2099 is triggered by miscalulating of
	 * emap credit (BETREE_DELETE epecially). Adding one more extra credit
	 * of 'emap paste' (that is frags + 1) to verify this idea.
	 */
	m0_be_emap_credit(&adom->sad_adata, M0_BEO_PASTE, frags + 1, accum);

	if (adom->sad_overwrite && ballroom->ab_ops->bo_free_credit != NULL) {
		/* for each emap_paste() seg_free() could be called 3 times */
		ballroom->ab_ops->bo_free_credit(ballroom, 3 * frags, accum);
	}
	m0_stob_io_credit(io, m0_stob_dom_get(adom->sad_bstore), accum);
}

/**
   Releases vectors allocated for back IO.

   @note that back->si_stob.ov_vec.v_count is _not_ freed separately, as it is
   aliased to back->si_user.z_bvec.ov_vec.v_count.

   @see ad_vec_alloc()
 */
static void stob_ad_io_release(struct m0_stob_ad_io *aio)
{
	struct m0_stob_io *back = &aio->ai_back;

	M0_ASSERT(back->si_user.ov_vec.v_count == back->si_stob.iv_vec.v_count);
	m0_free0(&back->si_user.ov_vec.v_count);
	back->si_stob.iv_vec.v_count = NULL;

	m0_free0(&back->si_user.ov_buf);
	m0_free0(&back->si_stob.iv_index);

	back->si_obj = NULL;
}

/**
   Initializes cursors at the beginning of a pass.
 */
static int stob_ad_cursors_init(struct m0_stob_io *io,
				struct m0_stob_ad_domain *adom,
				struct m0_be_emap_cursor *it,
				struct m0_vec_cursor *src,
				struct m0_vec_cursor *dst,
				struct m0_be_emap_caret *map)
{
	int rc;

	rc = stob_ad_cursor(adom, io->si_obj, io->si_stob.iv_index[0], it);
	if (rc == 0) {
		m0_vec_cursor_init(src, &io->si_user.ov_vec);
		m0_vec_cursor_init(dst, &io->si_stob.iv_vec);
		m0_be_emap_caret_init(map, it, io->si_stob.iv_index[0]);
	}
	return M0_RC(rc);
}

/**
   Finalizes the cursors that need finalisation.
 */
static void stob_ad_cursors_fini(struct m0_be_emap_cursor *it,
				 struct m0_vec_cursor *src,
				 struct m0_vec_cursor *dst,
				 struct m0_be_emap_caret *map)
{
	m0_be_emap_caret_fini(map);
	m0_be_emap_close(it);
}

/**
   Allocates back IO buffers after number of fragments has been calculated.

   @see stob_ad_io_release()
 */
static int stob_ad_vec_alloc(struct m0_stob *obj,
			     struct m0_stob_io *back,
			     uint32_t frags)
{
	m0_bcount_t *counts;
	int          rc = 0;

	M0_ASSERT(back->si_user.ov_vec.v_count == NULL);

	if (frags > 0) {
		M0_ALLOC_ARR(counts, frags);
		back->si_user.ov_vec.v_count = counts;
		back->si_stob.iv_vec.v_count = counts;
		M0_ALLOC_ARR(back->si_user.ov_buf, frags);
		M0_ALLOC_ARR(back->si_stob.iv_index, frags);

		back->si_user.ov_vec.v_nr = frags;
		back->si_stob.iv_vec.v_nr = frags;

		if (counts == NULL || back->si_user.ov_buf == NULL ||
		    back->si_stob.iv_index == NULL) {
			m0_free(counts);
			m0_free(back->si_user.ov_buf);
			m0_free(back->si_stob.iv_index);
			rc = M0_ERR(-ENOMEM);
		}
	}
	return M0_RC(rc);
}

/**
 * Constructs back IO for read.
 *
 * This is done in two passes:
 *
 *     - first, calculate number of fragments, taking holes into account. This
 *       pass iterates over user buffers list (src), target extents list (dst)
 *       and extents map (map). Once this pass is completed, back IO vectors can
 *       be allocated;
 *
 *     - then, iterate over the same sequences again. For holes, call memset()
 *       immediately, for other fragments, fill back IO vectors with the
 *       fragment description.
 *
 * @note assumes that allocation data can not change concurrently.
 *
 * @note memset() could become a bottleneck here.
 *
 * @note cursors and fragment sizes are measured in blocks.
 */
static int stob_ad_read_prepare(struct m0_stob_io        *io,
				struct m0_stob_ad_domain *adom,
				struct m0_vec_cursor     *src,
				struct m0_vec_cursor     *dst,
				struct m0_be_emap_caret  *car)
{
	struct m0_be_emap_cursor *it;
	struct m0_be_emap_seg    *seg;
	struct m0_stob_io        *back;
	struct m0_stob_ad_io     *aio = io->si_stob_private;
	uint32_t                  frags;
	uint32_t                  frags_not_empty;
	uint32_t                  bshift;
	m0_bcount_t               frag_size; /* measured in blocks */
	m0_bindex_t               off;       /* measured in blocks */
	int                       rc;
	int                       i;
	int                       idx;
	bool                      eosrc;
	bool                      eodst;
	int                       eomap;

	M0_PRE(io->si_opcode == SIO_READ);

	bshift = m0_stob_block_shift(adom->sad_bstore);
	it     = car->ct_it;
	seg    = m0_be_emap_seg_get(it);
	back   = &aio->ai_back;

	M0_LOG(M0_DEBUG, "ext="EXT_F" val=0x%llx",
		EXT_P(&seg->ee_ext), (unsigned long long)seg->ee_val);

	frags = frags_not_empty = 0;
	do {
		off = io->si_stob.iv_index[dst->vc_seg] + dst->vc_offset;

		/*
		 * The next fragment starts at the offset off and the extents
		 * car has to be positioned at this offset. There are two ways
		 * to do this:
		 *
		 * * lookup an extent containing off (m0_emap_lookup()), or
		 *
		 * * iterate from the current position (m0_emap_caret_move())
		 *   until off is reached.
		 *
		 * Lookup incurs an overhead of tree traversal, whereas
		 * iteration could become expensive when extents car is
		 * fragmented and target extents are far from each other.
		 *
		 * Iteration is used for now, because when extents car is
		 * fragmented or IO locality of reference is weak, performance
		 * will be bad anyway.
		 *
		 * Note: the code relies on the target extents being in
		 * increasing offset order in dst.
		 */
		M0_ASSERT(off >= car->ct_index);
		eomap = m0_be_emap_caret_move_sync(car, off - car->ct_index);
		if (eomap < 0)
			return M0_RC(eomap);
		M0_ASSERT(eomap == 0);
		M0_ASSERT(m0_ext_is_in(&seg->ee_ext, off));

		frag_size = min3(m0_vec_cursor_step(src),
				 m0_vec_cursor_step(dst),
				 m0_be_emap_caret_step(car));
		M0_ASSERT(frag_size > 0);
		if (frag_size > (size_t)~0ULL)
			return M0_ERR(-EOVERFLOW);

		frags++;
		if (seg->ee_val < AET_MIN)
			frags_not_empty++;

		eosrc = m0_vec_cursor_move(src, frag_size);
		eodst = m0_vec_cursor_move(dst, frag_size);
		eomap = m0_be_emap_caret_move_sync(car, frag_size);
		if (eomap < 0)
			return M0_RC(eomap);
		M0_ASSERT(eosrc == eodst);
		M0_ASSERT(!eomap);
	} while (!eosrc);

	M0_LOG(M0_DEBUG, "frags=%d frags_not_empty=%d",
			(int)frags, (int)frags_not_empty);

	stob_ad_cursors_fini(it, src, dst, car);

	rc = stob_ad_vec_alloc(io->si_obj, back, frags_not_empty);
	if (rc != 0)
		return M0_RC(rc);

	rc = stob_ad_cursors_init(io, adom, it, src, dst, car);
	if (rc != 0)
		return M0_RC(rc);

	for (idx = i = 0; i < frags; ++i) {
		void        *buf;
		m0_bindex_t  off;

		buf = io->si_user.ov_buf[src->vc_seg] + src->vc_offset;
		off = io->si_stob.iv_index[dst->vc_seg] + dst->vc_offset;

		M0_ASSERT(off >= car->ct_index);
		eomap = m0_be_emap_caret_move_sync(car, off - car->ct_index);
		if (eomap < 0)
			return M0_RC(eomap);
		M0_ASSERT(eomap == 0);
		M0_ASSERT(m0_ext_is_in(&seg->ee_ext, off));

		frag_size = min3(m0_vec_cursor_step(src),
				 m0_vec_cursor_step(dst),
				 m0_be_emap_caret_step(car));

		/* that is too expensive:
		M0_LOG(M0_DEBUG, "%2d: sz=%lx buf=%p off=%lx "
			"ext="EXT_F" val=%lx",
			idx, (unsigned long)frag_size, buf,
			(unsigned long)off, EXT_P(&seg->ee_ext),
			(unsigned long)seg->ee_val); */
		if (seg->ee_val == AET_HOLE) {
			/*
			 * Read of a hole or unallocated space (beyond
			 * end of the file).
			 */
			memset(stob_ad_addr_open(buf, bshift),
			       0, frag_size << bshift);
			io->si_count += frag_size;
		} else {
			M0_ASSERT(seg->ee_val < AET_MIN);

			back->si_user.ov_vec.v_count[idx] = frag_size;
			back->si_user.ov_buf[idx] = buf;

			back->si_stob.iv_index[idx] = seg->ee_val +
				(off - seg->ee_ext.e_start);
			idx++;
		}
		m0_vec_cursor_move(src, frag_size);
		m0_vec_cursor_move(dst, frag_size);
		rc = m0_be_emap_caret_move_sync(car, frag_size);
		if (rc < 0)
			break;
		M0_ASSERT(rc == 0);
	}
	M0_ASSERT(ergo(rc == 0, idx == frags_not_empty));
	return M0_RC(rc);
}

/**
   A linked list of allocated extents.
 */
struct stob_ad_write_ext {
	struct m0_ext             we_ext;
	struct stob_ad_write_ext *we_next;
};

/**
   A cursor over allocated extents.
 */
struct stob_ad_wext_cursor {
	const struct stob_ad_write_ext *wc_wext;
	m0_bcount_t                     wc_done;
};

static void stob_ad_wext_cursor_init(struct stob_ad_wext_cursor *wc,
				     struct stob_ad_write_ext *wext)
{
	wc->wc_wext = wext;
	wc->wc_done = 0;
}

static m0_bcount_t stob_ad_wext_cursor_step(struct stob_ad_wext_cursor *wc)
{
	M0_PRE(wc->wc_wext != NULL);
	M0_PRE(wc->wc_done < m0_ext_length(&wc->wc_wext->we_ext));

	return m0_ext_length(&wc->wc_wext->we_ext) - wc->wc_done;
}

static bool stob_ad_wext_cursor_move(struct stob_ad_wext_cursor *wc,
				     m0_bcount_t count)
{
	while (count > 0 && wc->wc_wext != NULL) {
		m0_bcount_t step;

		step = stob_ad_wext_cursor_step(wc);
		if (count >= step) {
			wc->wc_wext = wc->wc_wext->we_next;
			wc->wc_done = 0;
			count -= step;
		} else {
			wc->wc_done += count;
			count = 0;
		}
	}
	return wc->wc_wext == NULL;
}

/**
   Calculates how many fragments this IO request contains.

   @note extent map and dst are not used here, because write allocates new space
   for data, ignoring existing allocations in the overwritten extent of the
   file.
 */
static uint32_t stob_ad_write_count(struct m0_vec_cursor *src,
				    struct stob_ad_wext_cursor *wc)
{
	m0_bcount_t frag_size;
	bool        eosrc;
	bool        eoext;
	uint32_t    frags = 0;

	do {
		frag_size = min_check(m0_vec_cursor_step(src),
				      stob_ad_wext_cursor_step(wc));
		M0_ASSERT(frag_size > 0);
		M0_ASSERT(frag_size <= (size_t)~0ULL);

		eosrc = m0_vec_cursor_move(src, frag_size);
		eoext = stob_ad_wext_cursor_move(wc, frag_size);

		M0_ASSERT(ergo(eosrc, eoext));
		++frags;
	} while (!eoext);
	return frags;
}

/**
   Fills back IO request with information about fragments.
 */
static void stob_ad_write_back_fill(struct m0_stob_io *io,
				    struct m0_stob_io *back,
				    struct m0_vec_cursor *src,
				    struct stob_ad_wext_cursor *wc)
{
	m0_bcount_t    frag_size;
	uint32_t       idx;
	bool           eosrc;
	bool           eoext;

	idx = 0;
	do {
		void *buf;

		frag_size = min_check(m0_vec_cursor_step(src),
				      stob_ad_wext_cursor_step(wc));

		buf = io->si_user.ov_buf[src->vc_seg] + src->vc_offset;

		back->si_user.ov_vec.v_count[idx] = frag_size;
		back->si_user.ov_buf[idx] = buf;

		back->si_stob.iv_index[idx] =
			wc->wc_wext->we_ext.e_start + wc->wc_done;

		eosrc = m0_vec_cursor_move(src, frag_size);
		eoext = stob_ad_wext_cursor_move(wc, frag_size);
		idx++;
		M0_ASSERT(eosrc == eoext);
	} while (!eoext);
	M0_ASSERT(idx == back->si_stob.iv_vec.v_nr);
}

/**
 * Helper function used by ad_write_map_ext() to free sub-segment "ext" from
 * allocated segment "seg".
 */
static int stob_ad_seg_free(struct m0_dtx *tx,
			    struct m0_stob_ad_domain *adom,
			    const struct m0_be_emap_seg *seg,
			    const struct m0_ext *ext,
			    uint64_t val)
{
	m0_bcount_t delta = ext->e_start - seg->ee_ext.e_start;
	struct m0_ext tocut = {
		.e_start = val + delta,
		.e_end   = val + delta + m0_ext_length(ext)
	};
	m0_ext_init(&tocut);
	if (val < AET_MIN) {
		M0_LOG(M0_DEBUG, "freeing "EXT_F"@"EXT_F,
				 EXT_P(ext), EXT_P(&tocut));
	}

	return val < AET_MIN ? stob_ad_bfree(adom, tx, &tocut) : 0;
}

/**
 * Inserts allocated extent into AD storage object allocation map, possibly
 * overwriting a number of existing extents.
 *
 * @param offset - an offset in AD stob name-space;
 * @param ext - an extent in the underlying object name-space.
 *
 * This function updates extent mapping of AD storage to map an extent in its
 * logical name-space, starting with offset to an extent ext in the underlying
 * storage object name-space.
 */
static int stob_ad_write_map_ext(struct m0_stob_io *io,
				 struct m0_stob_ad_domain *adom,
				 m0_bindex_t off,
				 struct m0_be_emap_cursor *orig,
				 const struct m0_ext *ext)
{
	int                    result;
	int                    rc = 0;
	struct m0_be_emap_cursor  it = {};
	/* an extent in the logical name-space to be mapped to ext. */
	struct m0_ext          todo = {
		.e_start = off,
		.e_end   = off + m0_ext_length(ext)
	};
	m0_ext_init(&todo);

	M0_ENTRY("ext="EXT_F" val=0x%llx", EXT_P(&todo),
		 (unsigned long long)ext->e_start);

	result = M0_BE_OP_SYNC_RET_WITH(
			&it.ec_op,
			m0_be_emap_lookup(orig->ec_map, &orig->ec_seg.ee_pre,
					  off, &it),
			bo_u.u_emap.e_rc);
	if (result != 0)
		return M0_RC(result);
	/*
	 * Insert a new segment into extent map, overwriting parts of the map.
	 *
	 * Some existing segments are deleted completely, others are
	 * cut. m0_emap_paste() invokes supplied call-backs to notify the caller
	 * about changes in the map.
	 *
	 * Call-backs are used to free space from overwritten parts of the file.
	 *
	 * Each call-back takes a segment argument, seg. seg->ee_ext is a
	 * logical extent of the segment and seg->ee_val is the starting offset
	 * of the corresponding physical extent.
	 */
	M0_SET0(&it.ec_op);
	m0_be_op_init(&it.ec_op);
	m0_be_emap_paste(&it, &io->si_tx->tx_betx, &todo, ext->e_start,
	 LAMBDA(void, (struct m0_be_emap_seg *seg) {
			/* handle extent deletion. */
			if (adom->sad_overwrite) {
				M0_LOG(M0_DEBUG, "del: val=0x%llx",
					(unsigned long long)seg->ee_val);
				M0_ASSERT_INFO(seg->ee_val != ext->e_start,
				"Delete of the same just allocated block");
				rc = rc ?:
				     stob_ad_seg_free(io->si_tx, adom, seg,
						      &seg->ee_ext, seg->ee_val);
			}
		 }),
	 LAMBDA(void, (struct m0_be_emap_seg *seg, struct m0_ext *ext,
		       uint64_t val) {
			/* cut left */
			M0_ASSERT(ext->e_start > seg->ee_ext.e_start);

			seg->ee_val = val;
			if (adom->sad_overwrite)
				rc = rc ?:
				     stob_ad_seg_free(io->si_tx, adom, seg,
						      ext, val);
		}),
	 LAMBDA(void, (struct m0_be_emap_seg *seg, struct m0_ext *ext,
		       uint64_t val) {
			/* cut right */
			M0_ASSERT(seg->ee_ext.e_end > ext->e_end);
			if (val < AET_MIN) {
				seg->ee_val = val +
					(ext->e_end - seg->ee_ext.e_start);
				/*
				 * Free physical sub-extent, but only when
				 * sub-extent starts at the left boundary of the
				 * logical extent, because otherwise "cut left"
				 * already freed it.
				 */
				if (adom->sad_overwrite &&
				    ext->e_start == seg->ee_ext.e_start)
					rc = rc ?:
					     stob_ad_seg_free(io->si_tx, adom,
							      seg, ext, val);
			} else
				seg->ee_val = val;
		}));
	M0_ASSERT(m0_be_op_is_done(&it.ec_op));
	result = it.ec_op.bo_u.u_emap.e_rc;
	m0_be_op_fini(&it.ec_op);
	m0_be_emap_close(&it);

	return M0_RC(result ?: rc);
}

static int stob_ad_fol_frag_alloc(struct m0_fol_frag *frag, uint32_t frags)
{
	struct stob_ad_rec_frag *arp;

	M0_PRE(frag != NULL);

	M0_ALLOC_PTR(arp);
	if (arp == NULL)
		return M0_ERR(-ENOMEM);
	m0_fol_frag_init(frag, arp, &stob_ad_rec_frag_type);

	arp->arp_seg.ps_segments = frags;

	M0_ALLOC_ARR(arp->arp_seg.ps_old_data, frags);
	if (arp->arp_seg.ps_old_data == NULL) {
		m0_free(arp);
		return M0_ERR(-ENOMEM);
	}
	return 0;
}

static void stob_ad_fol_frag_free(struct m0_fol_frag *frag)
{
	struct stob_ad_rec_frag *arp = frag->rp_data;

	m0_free(arp->arp_seg.ps_old_data);
	m0_free(arp);
}

/**
 * Updates extent map, inserting newly allocated extents into it.
 *
 * @param dst - target extents in AD storage object;
 * @param wc - allocated extents.
 *
 * Total size of extents in dst and wc is the same, but their boundaries not
 * necessary match. Iterate over both sequences at the same time, mapping
 * contiguous chunks of AD stob name-space to contiguous chunks of the
 * underlying object name-space.
 */
static int stob_ad_write_map(struct m0_stob_io *io,
			     struct m0_stob_ad_domain *adom,
			     struct m0_ivec_cursor *dst,
			     struct m0_be_emap_caret *map,
			     struct stob_ad_wext_cursor *wc,
			     uint32_t frags)
{
	int			 rc;
	m0_bcount_t		 frag_size;
	m0_bindex_t		 off;
	bool			 eodst;
	bool			 eoext;
	struct m0_ext		 todo;
	struct m0_fol_frag	*frag = io->si_fol_frag;
	struct stob_ad_rec_frag	*arp;
	uint32_t		 i = 0;
	uint32_t                 last_seg;

	M0_ENTRY("io=%p dom=%p frags=%u", io, adom, frags);

	rc = stob_ad_fol_frag_alloc(frag, frags);
	if (rc != 0)
		return M0_RC(rc);
	arp = frag->rp_data;
	arp->arp_stob_id = *m0_stob_id_get(io->si_obj);
	arp->arp_dom_id  = *m0_stob_domain_id_get(m0_stob_dom_get(io->si_obj));

	do {
		off = m0_ivec_cursor_index(dst);
		frag_size = min_check(m0_ivec_cursor_step(dst),
				      stob_ad_wext_cursor_step(wc));

		todo.e_start = wc->wc_wext->we_ext.e_start + wc->wc_done;
		todo.e_end   = todo.e_start + frag_size;
		m0_ext_init(&todo);

		M0_ASSERT(i < frags);
		arp->arp_seg.ps_old_data[i] = (struct m0_be_emap_seg) {
			.ee_ext = {
				.e_start = off,
				.e_end   = off + m0_ext_length(&todo)
			},
			.ee_val = todo.e_start,
			.ee_pre = map->ct_it->ec_seg.ee_pre
		};
		m0_ext_init(&arp->arp_seg.ps_old_data[i].ee_ext);

		rc = stob_ad_write_map_ext(io, adom, off, map->ct_it, &todo);
		if (rc != 0)
			break;

		last_seg = dst->ic_cur.vc_seg;
		eodst = m0_ivec_cursor_move(dst, frag_size);
		eoext = stob_ad_wext_cursor_move(wc, frag_size);

		/*
		 * In m0_vec_cursor_move() if (count >= steps) evaluates to
		 * false and also the segments are not empty
		 * (m0_vec_cursor_normalize() skips empty segments) then
		 * cur->vc_seg is not incremented. i.e. cursor is not actually
		 * moved ahead.
		 * In such cases index i shouldn't also be advanced. Otherwise
		 * it will end up accessing invalid index of
		 * arp->arp_seg.ps_old_data[].
		 */
		if (last_seg != dst->ic_cur.vc_seg)
			++i;

		M0_ASSERT(eodst == eoext);
	} while (!eodst);

	if (rc == 0)
		m0_fol_frag_add(&io->si_tx->tx_fol_rec, frag);
	else
		stob_ad_fol_frag_free(frag);

	return M0_RC(rc);
}

/**
   Frees wext list.
 */
static void stob_ad_wext_fini(struct stob_ad_write_ext *wext)
{
	struct stob_ad_write_ext *next;

	for (wext = wext->we_next; wext != NULL; wext = next) {
		next = wext->we_next;
		m0_free(wext);
	}
}

/**
 * Constructs back IO for write.
 *
 * - allocates space for data to be written (first loop);
 *
 * - calculates number of fragments (ad_write_count());
 *
 * - constructs back IO (ad_write_back_fill());
 *
 * - updates extent map for this AD object with allocated extents
 *   (ad_write_map()).
 */
static int stob_ad_write_prepare(struct m0_stob_io        *io,
				 struct m0_stob_ad_domain *adom,
				 struct m0_vec_cursor     *src,
				 struct m0_be_emap_caret  *map)
{
	m0_bcount_t                 todo;
	uint32_t                    bfrags = 0;
	int                         rc;
	struct stob_ad_write_ext    head;
	struct stob_ad_write_ext   *wext;
	struct stob_ad_write_ext   *next;
	struct m0_stob_io          *back;
	struct m0_stob_ad_io       *aio = io->si_stob_private;
	struct stob_ad_wext_cursor  wc;

	M0_PRE(io->si_opcode == SIO_WRITE);
	M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id, M0_AVI_AD_WR_PREPARE);
	todo = m0_vec_count(&io->si_user.ov_vec);
	M0_ENTRY("op=%d sz=%lu", io->si_opcode, (unsigned long)todo);
	back = &aio->ai_back;
	M0_SET0(&head);
	wext = &head;
	wext->we_next = NULL;
	while (1) {
		m0_bcount_t got;

		M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id,
			     M0_AVI_AD_BALLOC_START);
		rc = stob_ad_balloc(adom, io->si_tx, todo, &wext->we_ext,
				    aio->ai_balloc_flags);
		M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id,
			     M0_AVI_AD_BALLOC_END);
		if (rc != 0)
			break;
		got = m0_ext_length(&wext->we_ext);
		M0_ASSERT(todo >= got);
		M0_LOG(M0_DEBUG, "got=%lu: " EXT_F, got, EXT_P(&wext->we_ext));
		todo -= got;
		++bfrags;
		if (todo > 0) {
			if (bfrags >= BALLOC_FRAGS_MAX) {
				rc = M0_ERR(-ENOSPC);
				break;
			}
			M0_ALLOC_PTR(next);
			if (next != NULL) {
				wext->we_next = next;
				wext = next;
			} else {
				rc = M0_ERR(-ENOMEM);
				break;
			}
		} else
			break;
	}

	M0_LOG(M0_DEBUG, "bfrags=%u", bfrags);

	if (rc == 0) {
		uint32_t frags;

		stob_ad_wext_cursor_init(&wc, &head);
		frags = stob_ad_write_count(src, &wc);
		rc = stob_ad_vec_alloc(io->si_obj, back, frags);
		if (rc == 0) {
			struct m0_ivec_cursor dst;
			/* reset src */
			m0_vec_cursor_init(src, &io->si_user.ov_vec);
			/* reset wc */
			stob_ad_wext_cursor_init(&wc, &head);
			stob_ad_write_back_fill(io, back, src, &wc);

			m0_ivec_cursor_init(&dst, &io->si_stob);
			stob_ad_wext_cursor_init(&wc, &head);
			frags = max_check(bfrags, stob_ad_write_map_count(adom,
							   &io->si_stob, true));
			rc = stob_ad_write_map(io, adom, &dst, map, &wc, frags);
		}
	}
	stob_ad_wext_fini(&head);
	return M0_RC(rc);
}

static int stob_ad_io_launch_prepare(struct m0_stob_io *io)
{
	struct m0_be_emap_cursor  it;
	struct m0_vec_cursor      src;
	struct m0_vec_cursor      dst;
	struct m0_be_emap_caret   map;
	struct m0_stob_ad_domain *adom;
	struct m0_stob_ad_io     *aio  = io->si_stob_private;
	struct m0_stob_io        *back = &aio->ai_back;
	int                       rc;

	M0_PRE(io->si_stob.iv_vec.v_nr > 0);
	M0_PRE(!m0_vec_is_empty(&io->si_user.ov_vec));
	M0_PRE(io->si_state == SIS_PREPARED);

	/* prefix fragments execution mode is not yet supported */
	M0_PRE((io->si_flags & SIF_PREFIX) == 0);
	/* only read-write at the moment */
	M0_PRE(io->si_opcode == SIO_READ || io->si_opcode == SIO_WRITE);

	M0_ENTRY("op=%d, stob %p, stob_id="STOB_ID_F,
		 io->si_opcode, io->si_obj, STOB_ID_P(&io->si_obj->so_id));

	M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id, M0_AVI_AD_PREPARE);
	adom = stob_ad_domain2ad(m0_stob_dom_get(io->si_obj));
	rc = stob_ad_cursors_init(io, adom, &it, &src, &dst, &map);
	if (rc != 0)
		return M0_RC(rc);

	back->si_opcode   = io->si_opcode;
	back->si_flags    = io->si_flags;
	back->si_fol_frag = io->si_fol_frag;
	back->si_id       = io->si_id;

	switch (io->si_opcode) {
	case SIO_READ:
		rc = stob_ad_read_prepare(io, adom, &src, &dst, &map);
		break;
	case SIO_WRITE:
		rc = stob_ad_write_prepare(io, adom, &src, &map);
		break;
	default:
		M0_IMPOSSIBLE("Invalid io type.");
	}
	stob_ad_cursors_fini(&it, &src, &dst, &map);

	return rc;
}

/**
 * Launch asynchronous IO.
 *
 * Call ad_write_prepare() or ad_read_prepare() to do the bulk of work, then
 * launch back IO just constructed.
 */
static int stob_ad_io_launch(struct m0_stob_io *io)
{
	struct m0_stob_ad_domain *adom;
	struct m0_stob_ad_io     *aio     = io->si_stob_private;
	struct m0_stob_io        *back    = &aio->ai_back;
	int                       rc      = 0;
	bool                      wentout = false;

	M0_PRE(io->si_stob.iv_vec.v_nr > 0);
	M0_PRE(!m0_vec_is_empty(&io->si_user.ov_vec));
	M0_PRE(io->si_state == SIS_BUSY);

	/* prefix fragments execution mode is not yet supported */
	M0_PRE((io->si_flags & SIF_PREFIX) == 0);
	/* only read-write at the moment */
	M0_PRE(io->si_opcode == SIO_READ || io->si_opcode == SIO_WRITE);

	M0_ENTRY("op=%d stob_id="STOB_ID_F,
		 io->si_opcode, STOB_ID_P(&io->si_obj->so_id));
	M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id, M0_AVI_AD_LAUNCH);

	adom = stob_ad_domain2ad(m0_stob_dom_get(io->si_obj));

	if (back->si_stob.iv_vec.v_nr > 0) {
		/**
		 * Sorts index vecs in incremental order.
		 * @todo : Needs to check performance impact
		 *        of sorting each stobio on ad stob.
		 */
		M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id,
			     M0_AVI_AD_SORT_START);
		m0_stob_iovec_sort(back);
		M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id,
			     M0_AVI_AD_SORT_END);
		rc = m0_stob_io_prepare_and_launch(back, adom->sad_bstore,
						   io->si_tx, io->si_scope);
		wentout = rc == 0;
	} else {
		/*
		 * Back IO request was constructed OK, but is empty (all
		 * IO was satisfied from holes). Notify caller about
		 * completion.
		 */
		M0_ASSERT(io->si_opcode == SIO_READ);
		stob_ad_endio(&aio->ai_clink);
	}

	if (!wentout)
		stob_ad_io_release(aio);
	return M0_RC(rc);
}

static bool stob_ad_endio(struct m0_clink *link)
{
	struct m0_stob_ad_io *aio;
	struct m0_stob_io    *io;

	aio = container_of(link, struct m0_stob_ad_io, ai_clink);
	io = aio->ai_fore;

	M0_ENTRY("op=%di, stob %p, stob_id="STOB_ID_F,
		 io->si_opcode, io->si_obj, STOB_ID_P(&io->si_obj->so_id));

	M0_ASSERT(io->si_state == SIS_BUSY);
	M0_ASSERT(aio->ai_back.si_state == SIS_IDLE);

	io->si_rc     = aio->ai_back.si_rc;
	io->si_count += aio->ai_back.si_count;
	io->si_state  = SIS_IDLE;
	M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id, M0_AVI_AD_ENDIO);
	M0_ADDB2_ADD(M0_AVI_STOB_IO_END, FID_P(m0_stob_fid_get(io->si_obj)),
		     m0_time_sub(m0_time_now(), io->si_start),
		     io->si_rc, io->si_count, aio->ai_back.si_user.ov_vec.v_nr);
	stob_ad_io_release(aio);
	m0_chan_broadcast_lock(&io->si_wait);
	return true;
}

/**
    Implementation of m0_fol_frag_ops::rpo_undo_credit and
                      m0_fol_frag_ops::rpo_redo_credit().
 */
static void
stob_ad_rec_frag_undo_redo_op_cred(const struct m0_fol_frag *frag,
				   struct m0_be_tx_credit   *accum)
{
	struct stob_ad_rec_frag  *arp  = frag->rp_data;
	struct m0_stob_domain    *dom  = m0_stob_domain_find(&arp->arp_dom_id);
	struct m0_stob_ad_domain *adom = stob_ad_domain2ad(dom);

	M0_PRE(dom != NULL);
	m0_be_emap_credit(&adom->sad_adata, M0_BEO_UPDATE,
			  arp->arp_seg.ps_segments, accum);
}

/**
    Implementation of m0_fol_frag_ops::rpo_undo and
                      m0_fol_frag_ops::rpo_redo ().
 */
static int stob_ad_rec_frag_undo_redo_op(struct m0_fol_frag *frag,
					 struct m0_be_tx    *tx)
{
	struct stob_ad_rec_frag  *arp  = frag->rp_data;
	struct m0_stob_domain    *dom  = m0_stob_domain_find(&arp->arp_dom_id);
	struct m0_stob_ad_domain *adom = stob_ad_domain2ad(dom);
	struct m0_be_emap_seg    *old_data = arp->arp_seg.ps_old_data;
	struct m0_be_emap_cursor  it;
	int		          i;
	int		          rc = 0;

	M0_PRE(dom != NULL);

	for (i = 0; rc == 0 && i < arp->arp_seg.ps_segments; ++i) {
		M0_SET0(&it.ec_op);
		rc = M0_BE_OP_SYNC_RET_WITH(
			&it.ec_op,
			m0_be_emap_lookup(&adom->sad_adata,
					  &old_data[i].ee_pre,
					  old_data[i].ee_ext.e_start,
					  &it),
			bo_u.u_emap.e_rc);
		if (rc == 0) {
			M0_LOG(M0_DEBUG, "%3d: ext="EXT_F" val=0x%llx",
				i, EXT_P(&old_data[i].ee_ext),
				(unsigned long long)old_data[i].ee_val);
			M0_SET0(&it.ec_op);
			rc = M0_BE_OP_SYNC_RET_WITH(
				&it.ec_op,
				m0_be_emap_extent_update(&it, tx, &old_data[i]),
				bo_u.u_emap.e_rc);
			m0_be_emap_close(&it);
		}
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_stob_ad_balloc_set(struct m0_stob_io *io, uint64_t flags)
{
	struct m0_stob_ad_io *aio = io->si_stob_private;

	M0_PRE(io->si_stob_magic == STOB_TYPE_AD);
	M0_PRE(aio != NULL);
	aio->ai_balloc_flags = flags;
}

M0_INTERNAL void m0_stob_ad_balloc_clear(struct m0_stob_io *io)
{
	struct m0_stob_ad_io *aio = io->si_stob_private;

	M0_PRE(aio != NULL);
	M0_PRE(m0_stob_domain_is_of_type(io->si_obj->so_domain,
					 &m0_stob_ad_type));

	aio->ai_balloc_flags = 0;
}

static const struct m0_stob_io_op stob_ad_io_op = {
	.sio_launch  = stob_ad_io_launch,
	.sio_prepare = stob_ad_io_launch_prepare,
	.sio_fini    = stob_ad_io_fini,
};

/** @} end group stobad */

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
