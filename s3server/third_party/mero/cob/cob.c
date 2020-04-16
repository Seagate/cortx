/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nathan Rutman <nathan_rutman@xyratex.com>
 * Original creation date: 10/24/2010
 *
 * Mdstore changes: Yuriy Umanets <yuriy_umanets@xyratex.com>
 */

#include "fid/fid.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_COB
#include "lib/trace.h"

#define M0_COB_KEY_LOG(logger, fmt, key, fid_member, str_member, ...)      \
	M0_ ## logger (fmt, (long)(key)->fid_member.f_container,           \
		       (long)(key)->fid_member.f_key,                      \
		       m0_bitstring_len_get(&((key)->str_member)),         \
		       (char *)m0_bitstring_buf_get(&((key)->str_member)), \
		       ## __VA_ARGS__)
#define M0_COB_NSKEY_LOG(logger, fmt, key, ...)\
	M0_COB_KEY_LOG(logger, fmt, key, cnk_pfid, cnk_name, ## __VA_ARGS__)

#include "lib/misc.h"   /* M0_SET0 */
#include "lib/arith.h"   /* M0_3WAY */
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/bitstring.h"
#include "lib/locality.h"

#include "cob/cob.h"

#include "be/domain.h"
#include "be/btree.h"
#include "be/seg0.h"
#include "be/tx.h"

#include "module/instance.h"
#include "reqh/reqh_service.h"
#include "reqh/reqh.h"


/**
   @addtogroup cob
   @{
*/

/*
 * TODO: Move m0_be_tx_init() out of create/destroy functions and let user
 * control tx. This will simplify respective interfaces.
 */

enum {
	M0_COB_NAME_MAX = 256,
	M0_COB_EA_MAX   = 4096
};

static int cob0_init(struct m0_be_domain *dom, const char *suffix,
		     const struct m0_buf *data)
{
	struct m0_cob_domain  *cdom = *(struct m0_cob_domain**)data->b_addr;
	struct m0_reqh        *reqh = dom->bd_cfg.bc_engine.bec_reqh;
	unsigned               key;

	M0_ENTRY("suffix: %s, data: %p, cdom: %p", suffix, data->b_addr, cdom);

	/* @todo:
	 * The following code is a workaround until full-scale m0mkfs is ready.
	 * It's planned to put cob-domains inside rehq_{io, mds, ..}_services.
	 *
	 * During mkfs phase, rehq_{io, mds, ..}_services have to be allocated
	 * with service_ops->rsto_service_allocate(). Identifiers of the service
	 * and cob-domain will be the same: cob-domain will take the id of the
	 * containing service.
	 *
	 * During reload phase cob_0type->b0_init() will initialize the
	 * corresponding cob domain and {io, mds, ...}srv_0type->b0_init()
	 * will initialise containing service.
	 */

	key = cdom->cd_id.id < M0_IOS_COB_ID_START ? m0_get()->i_mds_cdom_key :
		m0_get()->i_ios_cdom_key;

	if (m0_reqh_lockers_get(reqh, key) == NULL)
		m0_reqh_lockers_set(reqh, key, cdom);

	return M0_RC(0);
}

static void cob0_fini(struct m0_be_domain *dom, const char *suffix,
		      const struct m0_buf *data)
{
	M0_ENTRY();
	M0_LEAVE();
}

struct m0_be_0type m0_be_cob0 = {
	.b0_name = "M0_BE:COB",
	.b0_init = cob0_init,
	.b0_fini = cob0_fini,
};

const struct m0_fid_type m0_cob_fid_type = {
	.ft_id   = 'C',
	.ft_name = "cob fid"
};

M0_INTERNAL const struct m0_fid *m0_cob_fid(const struct m0_cob *cob)
{
	M0_PRE(cob != NULL);

	return cob->co_file.fi_fid;
}

M0_INTERNAL int m0_cob_mod_init(void)
{
	m0_fid_type_register(&m0_cob_fid_type);
	return 0;
}

M0_INTERNAL void m0_cob_mod_fini(void)
{
	m0_fid_type_unregister(&m0_cob_fid_type);
}

#ifndef __KERNEL__
M0_INTERNAL void m0_cob_oikey_make(struct m0_cob_oikey *oikey,
				   const struct m0_fid *fid, int linkno)
{
	oikey->cok_fid = *fid;
	oikey->cok_linkno = linkno;
}

M0_INTERNAL int m0_cob_nskey_make(struct m0_cob_nskey **keyh,
				  const struct m0_fid *pfid,
				  const char *name, size_t namelen)
{
	struct m0_cob_nskey *key;

	key = m0_alloc(sizeof *key + namelen);
	if (key == NULL)
		return M0_ERR(-ENOMEM);
	key->cnk_pfid = *pfid;
	m0_bitstring_copy(&key->cnk_name, name, namelen);
	*keyh = key;
	return 0;
}

M0_INTERNAL int m0_cob_nskey_cmp(const struct m0_cob_nskey *k0,
				 const struct m0_cob_nskey *k1)
{
	int rc;

	M0_PRE(m0_fid_is_set(&k0->cnk_pfid));
	M0_PRE(m0_fid_is_set(&k1->cnk_pfid));

	rc = m0_fid_cmp(&k0->cnk_pfid, &k1->cnk_pfid);
	return rc ?: m0_bitstring_cmp(&k0->cnk_name, &k1->cnk_name);
}

M0_INTERNAL size_t m0_cob_nskey_size(const struct m0_cob_nskey *cnk)
{
	return sizeof *cnk +
		m0_bitstring_len_get(&cnk->cnk_name);
}

M0_INTERNAL int m0_cob_eakey_make(struct m0_cob_eakey **keyh,
				  const struct m0_fid *fid,
				  const char *name, size_t namelen)
{
	struct m0_cob_eakey *key;

	key = m0_alloc(sizeof *key + namelen);
	if (key == NULL)
		return M0_ERR(-ENOMEM);
	key->cek_fid = *fid;
	m0_bitstring_copy(&key->cek_name, name, namelen);
	*keyh = key;
	return 0;
}

/**
   Make eakey for iterator. Allocate space for maximal possible name.
*/
static int m0_cob_max_eakey_make(struct m0_cob_eakey **keyh,
   		                 const struct m0_fid *fid,
				 const char *name,
				 int namelen)
{
	struct m0_cob_eakey *key;

	key = m0_alloc(sizeof *key + M0_COB_NAME_MAX);
	if (key == NULL)
		return M0_ERR(-ENOMEM);
	key->cek_fid = *fid;
	m0_bitstring_copy(&key->cek_name, name, namelen);
	*keyh = key;
	return 0;
}

M0_INTERNAL int m0_cob_eakey_cmp(const struct m0_cob_eakey *k0,
				 const struct m0_cob_eakey *k1)
{
	int rc;

	M0_PRE(m0_fid_is_set(&k0->cek_fid));
	M0_PRE(m0_fid_is_set(&k1->cek_fid));

	rc = m0_fid_cmp(&k0->cek_fid, &k1->cek_fid);
	return rc ?: m0_bitstring_cmp(&k0->cek_name, &k1->cek_name);
}

M0_INTERNAL size_t m0_cob_eakey_size(const struct m0_cob_eakey *cek)
{
	return sizeof *cek +
		m0_bitstring_len_get(&cek->cek_name);
}

static size_t m0_cob_earec_size(const struct m0_cob_earec *rec)
{
	return sizeof *rec + rec->cer_size;
}

/**
   Maximal possible earec size.
 */
M0_INTERNAL size_t m0_cob_max_earec_size(void)
{
	return sizeof(struct m0_cob_earec) + M0_COB_EA_MAX;
}

/**
   Maximal possible eakey size.
 */
static size_t m0_cob_max_eakey_size(void)
{
	return sizeof(struct m0_cob_eakey) + M0_COB_NAME_MAX;
}

/**
   Fabrec size taking into account symlink length.
 */
static size_t m0_cob_fabrec_size(const struct m0_cob_fabrec *rec)
{
	return sizeof *rec + rec->cfb_linklen;
}

M0_INTERNAL int m0_cob_fabrec_make(struct m0_cob_fabrec **rech,
				   const char *link, size_t linklen)
{
	struct m0_cob_fabrec *rec;

	rec = m0_alloc(sizeof(struct m0_cob_fabrec) + linklen);
	if (rec == NULL)
		return M0_ERR(-ENOMEM);
	rec->cfb_linklen = linklen;
	if (linklen > 0)
		memcpy(rec->cfb_link, link, linklen);
	*rech = rec;
	return 0;
}

/**
   Maximal possible fabrec size.
 */
static size_t m0_cob_max_fabrec_size(void)
{
	return sizeof(struct m0_cob_fabrec) + M0_COB_NAME_MAX;
}

/**
   Allocate memory for maximal possible size of fabrec.
 */
static int m0_cob_max_fabrec_make(struct m0_cob_fabrec **rech)
{
	struct m0_cob_fabrec *rec;

	rec = m0_alloc(m0_cob_max_fabrec_size());
	if (rec == NULL)
		return M0_ERR(-ENOMEM);
	rec->cfb_linklen = M0_COB_NAME_MAX;
	*rech = rec;
	return 0;
}

/**
   Make nskey for iterator. Allocate space for maximal possible name.
*/
static int m0_cob_max_nskey_make(struct m0_cob_nskey **keyh,
				 const struct m0_fid *pfid,
				 const char *name,
				 int namelen)
{
	struct m0_cob_nskey *key;

	key = m0_alloc(sizeof *key + M0_COB_NAME_MAX);
	if (key == NULL)
		return M0_ERR(-ENOMEM);
	key->cnk_pfid = *pfid;
	m0_bitstring_copy(&key->cnk_name, name, namelen);
	*keyh = key;
	return 0;
}

/**
   Key size for iterator in which case we don't know exact length of key
   and want to allocate it for worst case scenario, that is, for max
   possible name len.
 */
static size_t m0_cob_max_nskey_size(void)
{
	return sizeof(struct m0_cob_nskey) + M0_COB_NAME_MAX;
}

/**
   Namespace table definition.
*/
static int ns_cmp(const void *key0, const void *key1)
{
	return m0_cob_nskey_cmp((const struct m0_cob_nskey *)key0,
				(const struct m0_cob_nskey *)key1);
}

static m0_bcount_t ns_ksize(const void *key)
{
	return m0_cob_nskey_size(key);
}

static m0_bcount_t ns_vsize(const void *val)
{
	return sizeof(struct m0_cob_nsrec);
}

static const struct m0_be_btree_kv_ops cob_ns_ops = {
	.ko_ksize   = ns_ksize,
	.ko_vsize   = ns_vsize,
	.ko_compare = ns_cmp
};

/**
   Object index table definition.
*/
static int oi_cmp(const void *key0, const void *key1)
{
	const struct m0_cob_oikey *cok0 = key0;
	const struct m0_cob_oikey *cok1 = key1;
	int                        rc;

	M0_PRE(m0_fid_is_set(&cok0->cok_fid));
	M0_PRE(m0_fid_is_set(&cok1->cok_fid));

	rc = m0_fid_cmp(&cok0->cok_fid, &cok1->cok_fid);
	return rc ?: M0_3WAY(cok0->cok_linkno, cok1->cok_linkno);
}

static m0_bcount_t oi_ksize(const void *key)
{
	return sizeof(struct m0_cob_oikey);
}

static const struct m0_be_btree_kv_ops cob_oi_ops = {
	.ko_ksize   = oi_ksize,
	.ko_vsize   = ns_ksize,
	.ko_compare = oi_cmp
};

/**
   File attributes table definition
 */
static int fb_cmp(const void *key0, const void *key1)
{
	const struct m0_cob_fabkey *cok0 = key0;
	const struct m0_cob_fabkey *cok1 = key1;

	M0_PRE(m0_fid_is_set(&cok0->cfb_fid));
	M0_PRE(m0_fid_is_set(&cok1->cfb_fid));

	return m0_fid_cmp(&cok0->cfb_fid, &cok1->cfb_fid);
}

static m0_bcount_t fb_ksize(const void *key)
{
	return sizeof(struct m0_cob_fabkey);
}

static m0_bcount_t fb_vsize(const void *val)
{
	return m0_cob_fabrec_size(val);
}

static const struct m0_be_btree_kv_ops cob_fab_ops = {
	.ko_ksize   = fb_ksize,
	.ko_vsize   = fb_vsize,
	.ko_compare = fb_cmp
};

/**
   Extended attributes table definition
 */
static int ea_cmp(const void *key0, const void *key1)
{
	return m0_cob_eakey_cmp(key0, key1);
}

static m0_bcount_t ea_ksize(const void *key)
{
	return m0_cob_eakey_size(key);
}

static m0_bcount_t ea_vsize(const void *val)
{
	return m0_cob_earec_size(val);
}

static const struct m0_be_btree_kv_ops cob_ea_ops = {
	.ko_ksize   = ea_ksize,
	.ko_vsize   = ea_vsize,
	.ko_compare = ea_cmp
};

/**
   Omg table definition.
*/
static int omg_cmp(const void *key0, const void *key1)
{
	const struct m0_cob_omgkey *cok0 = key0;
	const struct m0_cob_omgkey *cok1 = key1;
	return M0_3WAY(cok0->cok_omgid, cok1->cok_omgid);
}

static m0_bcount_t omg_ksize(const void *key)
{
	return sizeof(struct m0_cob_omgkey);
}

static m0_bcount_t omg_vsize(const void *val)
{
	return sizeof(struct m0_cob_omgrec);
}

static const struct m0_be_btree_kv_ops cob_omg_ops = {
	.ko_ksize   = omg_ksize,
	.ko_vsize   = omg_vsize,
	.ko_compare = omg_cmp
};

M0_UNUSED static char *cob_dom_id_make(char *buf, const struct m0_cob_domain_id *id,
			     const char *prefix)
{
	sprintf(buf, "%s%lu", prefix ? prefix : "", (unsigned long)id->id);
	return buf;
}

/**
   Set up a new cob domain

   Tables are identified by the domain id, which must be set before calling
   this function.
 */
int m0_cob_domain_init(struct m0_cob_domain *dom, struct m0_be_seg *seg)
{
	M0_ENTRY("dom=%p id=%"PRIx64"", dom, dom != NULL ? dom->cd_id.id : 0);

	M0_PRE(dom != NULL);
	M0_PRE(dom->cd_id.id != 0);

	m0_be_btree_init(&dom->cd_object_index,   seg, &cob_oi_ops);
	m0_be_btree_init(&dom->cd_namespace,	  seg, &cob_ns_ops);
	m0_be_btree_init(&dom->cd_fileattr_basic, seg, &cob_fab_ops);
	m0_be_btree_init(&dom->cd_fileattr_omg,   seg, &cob_omg_ops);
	m0_be_btree_init(&dom->cd_fileattr_ea,    seg, &cob_ea_ops);

	return M0_RC(0);
}

void m0_cob_domain_fini(struct m0_cob_domain *dom)
{
	m0_be_btree_fini(&dom->cd_fileattr_ea);
	m0_be_btree_fini(&dom->cd_fileattr_omg);
	m0_be_btree_fini(&dom->cd_fileattr_basic);
	m0_be_btree_fini(&dom->cd_object_index);
	m0_be_btree_fini(&dom->cd_namespace);
}

static void cob_domain_id2str(char **s, const struct m0_cob_domain_id *cdid)
{
	return m0_asprintf(s, "%016lX", cdid->id);
}

M0_INTERNAL int m0_cob_domain_credit_add(struct m0_cob_domain          *dom,
					 struct m0_be_domain           *bedom,
					 struct m0_be_seg              *seg,
				         const struct m0_cob_domain_id *cdid,
				         struct m0_be_tx_credit        *cred)
{
	char                *cdid_str;
	struct m0_buf        data = {}; /*XXX*/
	struct m0_be_btree   dummy = { .bb_seg = seg }; /* XXX */

	cob_domain_id2str(&cdid_str, cdid);
	if (cdid_str == NULL)
		return M0_ERR(-ENOMEM);
	m0_be_0type_add_credit(bedom, &m0_be_cob0, cdid_str, &data, cred);
	M0_BE_ALLOC_CREDIT_PTR(dom, seg, cred);
	m0_be_btree_create_credit(&dummy, 5 /* XXX */, cred);
	m0_free(cdid_str);
	return M0_RC(0);
}

M0_INTERNAL
int m0_cob_domain_create_prepared(struct m0_cob_domain          **out,
				  struct m0_sm_group             *grp,
				  const struct m0_cob_domain_id  *cdid,
				  struct m0_be_domain            *bedom,
				  struct m0_be_seg               *seg,
				  struct m0_be_tx                *tx)
{
	struct m0_cob_domain *dom;
	char                 *cdid_str;
	struct m0_buf         data = {}; /*XXX*/
	int                   rc;

	cob_domain_id2str(&cdid_str, cdid);
	if (cdid_str == NULL)
		return M0_ERR(-ENOMEM);

	M0_BE_ALLOC_PTR_SYNC(dom, seg, tx);
	if (dom == NULL) {
		m0_free(cdid_str);
		return M0_ERR(-ENOMEM);
	}

	m0_format_header_pack(&dom->cd_header, &(struct m0_format_tag){
		.ot_version = M0_COB_DOMAIN_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_COB_DOMAIN,
		.ot_footer_offset = offsetof(struct m0_cob_domain, cd_footer)
	});

	dom->cd_id = *cdid;

	m0_format_footer_update(dom);
	M0_BE_TX_CAPTURE_PTR(seg, tx, dom);

	rc = m0_cob_domain_init(dom, seg);
	M0_ASSERT(rc == 0); /* XXX */
	M0_BE_OP_SYNC(o, m0_be_btree_create(&dom->cd_object_index,   tx, &o));
	M0_BE_OP_SYNC(o, m0_be_btree_create(&dom->cd_namespace,      tx, &o));
	M0_BE_OP_SYNC(o, m0_be_btree_create(&dom->cd_fileattr_basic, tx, &o));
	M0_BE_OP_SYNC(o, m0_be_btree_create(&dom->cd_fileattr_omg,   tx, &o));
	M0_BE_OP_SYNC(o, m0_be_btree_create(&dom->cd_fileattr_ea,    tx, &o));

	data = M0_BUF_INIT_PTR(&dom);
	rc = m0_be_0type_add(&m0_be_cob0, bedom, tx, cdid_str, &data);
	M0_ASSERT(rc == 0);
	m0_free(cdid_str);

	*out = dom;

	return M0_RC(0);
}

int m0_cob_domain_create(struct m0_cob_domain          **dom,
			 struct m0_sm_group             *grp,
			 const struct m0_cob_domain_id  *cdid,
			 struct m0_be_domain            *bedom,
			 struct m0_be_seg               *seg)
{
	struct m0_be_tx_credit cred  = {};
	struct m0_be_tx       *tx;
	int                    rc;

	M0_PRE(cdid->id != 0);

	M0_ALLOC_PTR(tx);
	if (tx == NULL)
		return M0_ERR(-ENOMEM);

	m0_cob_domain_credit_add(*dom, bedom, seg, cdid, &cred);
	m0_be_tx_init(tx, 0, bedom, grp, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_exclusive_open_sync(tx);
	if (rc != 0)
		goto tx_fini;

	rc = m0_cob_domain_create_prepared(dom, grp, cdid, bedom, seg, tx);
	m0_be_tx_close_sync(tx);
tx_fini:
	m0_be_tx_fini(tx);
	m0_free(tx);
	return M0_RC(rc);
}

int m0_cob_domain_destroy(struct m0_cob_domain *dom,
			  struct m0_sm_group   *grp,
			  struct m0_be_domain  *bedom)
{
	struct m0_be_tx_credit cred  = {};
	struct m0_be_seg      *seg;
	char                  *cdid_str;
	struct m0_be_tx       *tx;
	int rc;

	M0_PRE(dom != NULL);

	M0_ALLOC_PTR(tx);
	if (tx == NULL)
		return M0_ERR(-ENOMEM);

	cob_domain_id2str(&cdid_str, &dom->cd_id);
	if (cdid_str == NULL) {
		m0_free(tx);
		return M0_ERR(-ENOMEM);
	}

	seg = m0_be_domain_seg(bedom, dom);
	m0_be_0type_del_credit(bedom, &m0_be_cob0, cdid_str, &cred);
	M0_BE_FREE_CREDIT_PTR(dom, seg, &cred);

	m0_be_btree_destroy_credit(&dom->cd_object_index,   &cred);
	m0_be_btree_destroy_credit(&dom->cd_namespace,      &cred);
	m0_be_btree_destroy_credit(&dom->cd_fileattr_basic, &cred);
	m0_be_btree_destroy_credit(&dom->cd_fileattr_omg,   &cred);
	m0_be_btree_destroy_credit(&dom->cd_fileattr_ea,    &cred);


	m0_be_tx_init(tx, 0, bedom, grp, NULL, NULL, NULL, NULL);
	m0_be_tx_prep(tx, &cred);
	rc = m0_be_tx_exclusive_open_sync(tx);
	if (rc != 0)
		goto tx_fini_return;

	rc = m0_be_0type_del(&m0_be_cob0, bedom, tx, cdid_str);
	M0_ASSERT(rc == 0);

	M0_BE_OP_SYNC(o, m0_be_btree_destroy(&dom->cd_object_index,   tx, &o));
	M0_BE_OP_SYNC(o, m0_be_btree_destroy(&dom->cd_namespace,      tx, &o));
	M0_BE_OP_SYNC(o, m0_be_btree_destroy(&dom->cd_fileattr_basic, tx, &o));
	M0_BE_OP_SYNC(o, m0_be_btree_destroy(&dom->cd_fileattr_omg,   tx, &o));
	M0_BE_OP_SYNC(o, m0_be_btree_destroy(&dom->cd_fileattr_ea,    tx, &o));

	m0_cob_domain_fini(dom);

	dom->cd_id.id = 0;
	M0_BE_TX_CAPTURE_PTR(seg, tx, &dom->cd_id);
	M0_BE_FREE_PTR_SYNC(dom, seg, tx);

	m0_be_tx_close_sync(tx);
tx_fini_return:
	m0_be_tx_fini(tx);
	m0_free(cdid_str);
	m0_free(tx);

	return M0_RC(rc);
}

#include <sys/stat.h>    /* S_ISDIR */

#define MKFS_ROOT_SIZE          4096
#define MKFS_ROOT_BLKSIZE       4096
#define MKFS_ROOT_BLOCKS        16

static int cob_table_delete(struct m0_be_btree *tree, struct m0_be_tx *tx,
			     struct m0_buf *key)
{
	return M0_BE_OP_SYNC_RET(op, m0_be_btree_delete(tree, tx, &op, key),
	                         bo_u.u_btree.t_rc);
}


static int cob_table_update(struct m0_be_btree *tree, struct m0_be_tx *tx,
			     struct m0_buf *key, struct m0_buf *val)
{
	return M0_BE_OP_SYNC_RET(op,
				 m0_be_btree_update(tree, tx, &op, key, val),
	                         bo_u.u_btree.t_rc);
}

static int cob_table_insert(struct m0_be_btree *tree, struct m0_be_tx *tx,
			     struct m0_buf *key, struct m0_buf *val)
{
	return M0_BE_OP_SYNC_RET(op,
				 m0_be_btree_insert(tree, tx, &op, key, val),
				 bo_u.u_btree.t_rc);
}

static int cob_table_lookup(struct m0_be_btree *tree, struct m0_buf *key,
			    struct m0_buf *out)
{
	return M0_BE_OP_SYNC_RET(op,
	                         m0_be_btree_lookup(tree, &op, key, out),
	                         bo_u.u_btree.t_rc);
}


/**
 * Create initial files system structures, such as: entire storage root, root
 * cob for sessions and root cob for hierarchy. Latter is only one of them
 * visible to user on client.
 */
M0_INTERNAL int m0_cob_domain_mkfs(struct m0_cob_domain *dom,
				   const struct m0_fid *rootfid,
				   struct m0_be_tx *tx)
{
	struct m0_cob_nskey  *nskey = NULL;
	struct m0_cob_nsrec   nsrec = {};
	struct m0_cob_omgkey  omgkey = {};
	struct m0_cob_omgrec  omgrec = {};
	struct m0_cob_fabrec *fabrec = NULL;
	struct m0_buf         key;
	struct m0_buf         rec;
	struct m0_cob        *cob;
	int                   rc;

	/**
	   Create terminator omgid record with id == ~0ULL.
	 */
	omgkey.cok_omgid = ~0ULL;

	m0_buf_init(&key, &omgkey, sizeof omgkey);
	m0_buf_init(&rec, &omgrec, sizeof omgrec);
	cob_table_insert(&dom->cd_fileattr_omg, tx, &key, &rec);

	/**
	   Create root cob where all namespace is stored.
	 */
	rc = m0_cob_alloc(dom, &cob);
	if (rc != 0)
		return M0_RC(rc);

	rc = m0_cob_nskey_make(&nskey, &M0_COB_ROOT_FID, M0_COB_ROOT_NAME,
			       strlen(M0_COB_ROOT_NAME));
	if (rc != 0) {
	    m0_cob_put(cob);
	    return M0_RC(rc);
	}

	m0_cob_nsrec_init(&nsrec);
	nsrec.cnr_omgid = 0;
	nsrec.cnr_fid = *rootfid;

	nsrec.cnr_nlink = 2;
	nsrec.cnr_size = MKFS_ROOT_SIZE;
	nsrec.cnr_blksize = MKFS_ROOT_BLKSIZE;
	nsrec.cnr_blocks = MKFS_ROOT_BLOCKS;
	nsrec.cnr_atime = nsrec.cnr_mtime = nsrec.cnr_ctime =
		m0_time_seconds(m0_time_now());

	omgrec.cor_uid = 0;
	omgrec.cor_gid = 0;
	omgrec.cor_mode = S_IFDIR |
			  S_IRUSR | S_IWUSR | S_IXUSR | /* rwx for owner */
			  S_IRGRP | S_IXGRP |           /* r-x for group */
			  S_IROTH | S_IXOTH;            /* r-x for others */

	rc = m0_cob_fabrec_make(&fabrec, NULL, 0);
	if (rc != 0) {
		m0_cob_put(cob);
		m0_free(nskey);
		return M0_RC(rc);
	}

	rc = m0_cob_create(cob, nskey, &nsrec, fabrec, &omgrec, tx);
	m0_cob_put(cob);
	if (rc == -EEXIST)
		rc = 0;
	if (rc != 0) {
		m0_free(nskey);
		m0_free(fabrec);
		return M0_RC(rc);
	}

	return 0;
}

static void cob_free_cb(struct m0_ref *ref);

static void cob_init(struct m0_cob_domain *dom, struct m0_cob *cob)
{
	m0_ref_init(&cob->co_ref, 1, cob_free_cb);
	/**
	 * @todo Get di type from configuration.
	 */
	m0_file_init(&cob->co_file, &cob->co_nsrec.cnr_fid, NULL,
		     M0_DI_DEFAULT_TYPE);
	cob->co_nskey = NULL;
	cob->co_dom = dom;
	cob->co_flags = 0;
}

static void cob_fini(struct m0_cob *cob)
{
	if (cob->co_flags & M0_CA_NSKEY_FREE)
		m0_free(cob->co_nskey);
	if (cob->co_flags & M0_CA_FABREC)
		m0_free(cob->co_fabrec);
}

/**
   Return cob memory to the pool
 */
static void cob_free_cb(struct m0_ref *ref)
{
	struct m0_cob *cob;

	cob = container_of(ref, struct m0_cob, co_ref);
	cob_fini(cob);
	m0_free(cob);
}

M0_INTERNAL void m0_cob_get(struct m0_cob *cob)
{
	m0_ref_get(&cob->co_ref);
}

M0_INTERNAL void m0_cob_put(struct m0_cob *cob)
{
	m0_ref_put(&cob->co_ref);
}

M0_INTERNAL int m0_cob_alloc(struct m0_cob_domain *dom, struct m0_cob **out)
{
	struct m0_cob *cob;

	M0_ALLOC_PTR(cob);
	if (cob == NULL) {
		*out = NULL;
		return M0_ERR(-ENOMEM);
	}

	cob_init(dom, cob);
	*out = cob;

	return 0;
}

static int cob_ns_lookup(struct m0_cob *cob);
static int cob_oi_lookup(struct m0_cob *cob);
static int cob_fab_lookup(struct m0_cob *cob);

/**
   Search for a record in the namespace table

   If the lookup fails, we return error and co_flags accurately reflects
   the missing fields.

   @see cob_oi_lookup
 */
static int cob_ns_lookup(struct m0_cob *cob)
{
	struct m0_buf   key;
	struct m0_buf   val;
	int             rc;

	M0_PRE(cob->co_nskey != NULL &&
	       m0_fid_is_set(&cob->co_nskey->cnk_pfid));

	m0_buf_init(&key, cob->co_nskey, m0_cob_nskey_size(cob->co_nskey));
	m0_buf_init(&val, &cob->co_nsrec, sizeof cob->co_nsrec);

	rc = cob_table_lookup(&cob->co_dom->cd_namespace, &key, &val);
	if (rc == 0) {
		cob->co_flags |= M0_CA_NSREC;
		M0_ASSERT(cob->co_nsrec.cnr_linkno > 0 ||
			  cob->co_nsrec.cnr_nlink > 0);
		M0_POST(m0_fid_is_set(m0_cob_fid(cob)));
	}
	return rc;
}

/**
   Search for a record in the object index table.
   Most likely we want stat data for a given fid, so let's do that as well.

   @see cob_ns_lookup
 */
static int cob_oi_lookup(struct m0_cob *cob)
{
	struct m0_be_btree_cursor cursor;
	struct m0_buf		  start;
	struct m0_buf		  key;
	struct m0_buf		  val;
	struct m0_cob_oikey       oldkey;
	struct m0_cob_oikey      *oikey;
	struct m0_cob_nskey      *nskey;
	int                       rc;

	if (cob->co_flags & M0_CA_NSKEY)
		return 0;

	if (cob->co_flags & M0_CA_NSKEY_FREE) {
		m0_free(cob->co_nskey);
		cob->co_flags &= ~M0_CA_NSKEY_FREE;
	}

	oldkey = cob->co_oikey;

	/*
	 * Find the name from the object index table. Note the key buffer
	 * is out of scope outside of this function, but the record is good
	 * until m0_db_pair_fini.
	 */
	m0_buf_init(&start, &cob->co_oikey, sizeof cob->co_oikey);

	/*
	 * We use cursor here because in some situations we need
	 * to find most suitable position instead of exact location.
	 */
	m0_be_btree_cursor_init(&cursor, &cob->co_dom->cd_object_index);
	rc = m0_be_btree_cursor_get_sync(&cursor, &start, true);
	if (rc != 0) {
		M0_LOG(M0_DEBUG, "btree_cursor_get_sync() failed with %d", rc);
		goto out;
	}

	m0_be_btree_cursor_kv_get(&cursor, &key, &val);
	nskey = (struct m0_cob_nskey *)val.b_addr;
	oikey = (struct m0_cob_oikey *)key.b_addr;

	M0_LOG(M0_DEBUG, "found: fid="FID_F" lno=%d pfid="FID_F" name='%s'",
	       FID_P(&oikey->cok_fid), (int)oikey->cok_linkno,
	       FID_P(&nskey->cnk_pfid), (char*)nskey->cnk_name.b_data);

	/*
	 * Found position should have same fid.
	 */
	if (!m0_fid_eq(&oldkey.cok_fid, &oikey->cok_fid)) {
		M0_LOG(M0_DEBUG, "old fid="FID_F" fid="FID_F,
				FID_P(&oldkey.cok_fid),
				FID_P(&oikey->cok_fid));
		rc = -ENOENT;
		goto out;
	}

	rc = m0_cob_nskey_make(&cob->co_nskey, &nskey->cnk_pfid,
			       m0_bitstring_buf_get(&nskey->cnk_name),
			       m0_bitstring_len_get(&nskey->cnk_name));
	cob->co_flags |= (M0_CA_NSKEY | M0_CA_NSKEY_FREE);
out:
	m0_be_btree_cursor_fini(&cursor);
	return M0_RC(rc);
}

/**
   Search for a record in the fileattr_basic table.

   @see cob_ns_lookup
   @see cob_oi_lookup
 */
static int cob_fab_lookup(struct m0_cob *cob)
{
	struct m0_cob_fabkey fabkey;
	struct m0_buf        key;
	struct m0_buf        val;
	int                  rc;

	if (cob->co_flags & M0_CA_FABREC)
		return 0;

	fabkey.cfb_fid = *m0_cob_fid(cob);
	rc = m0_cob_max_fabrec_make(&cob->co_fabrec);
	if (rc != 0)
		return M0_RC(rc);

	m0_buf_init(&key, &fabkey, sizeof fabkey);
	m0_buf_init(&val, cob->co_fabrec, m0_cob_max_fabrec_size());

	rc = cob_table_lookup(&cob->co_dom->cd_fileattr_basic, &key, &val);
	if (rc == 0)
		cob->co_flags |= M0_CA_FABREC;
	else
		cob->co_flags &= ~M0_CA_FABREC;

	return M0_RC(rc);
}

/**
   Search for a record in the fileattr_omg table.
   @see cob_fab_lookup
 */
static int cob_omg_lookup(struct m0_cob *cob)
{
	struct m0_cob_omgkey omgkey;
	struct m0_buf        key;
	struct m0_buf        val;
	int                  rc;

	if (cob->co_flags & M0_CA_OMGREC)
		return 0;

	omgkey.cok_omgid = cob->co_nsrec.cnr_omgid;

	m0_buf_init(&key, &omgkey, sizeof omgkey);
	m0_buf_init(&val, &cob->co_omgrec, sizeof cob->co_omgrec);

	rc = cob_table_lookup(&cob->co_dom->cd_fileattr_omg, &key, &val);
	if (rc == 0)
		cob->co_flags |= M0_CA_OMGREC;
	else
		cob->co_flags &= ~M0_CA_OMGREC;

	return rc;
}

/**
   Load fab and omg records according with @need flags.
 */
static int cob_get_fabomg(struct m0_cob *cob, uint64_t flags)
{
	int rc = 0;

	if (flags & M0_CA_FABREC) {
		rc = cob_fab_lookup(cob);
		if (rc != 0)
			return M0_RC(rc);
	}

	/*
	 * Get omg attributes as well if we need it.
	 */
	if (flags & M0_CA_OMGREC) {
		rc = cob_omg_lookup(cob);
		if (rc != 0)
			return M0_RC(rc);
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_lookup(struct m0_cob_domain *dom,
			      struct m0_cob_nskey *nskey, uint64_t flags,
			      struct m0_cob **out)
{
	struct m0_cob *cob;
	int            rc;

	M0_ASSERT(out != NULL);
	*out = NULL;

	rc = m0_cob_alloc(dom, &cob);
	if (rc != 0)
		return M0_RC(rc);

	cob->co_nskey = nskey;
	cob->co_flags |= M0_CA_NSKEY;

	if (flags & M0_CA_NSKEY_FREE)
		cob->co_flags |= M0_CA_NSKEY_FREE;

	rc = cob_ns_lookup(cob);
	if (rc != 0) {
		m0_cob_put(cob);
		return M0_RC(rc);
	}

	rc = cob_get_fabomg(cob, flags);
	if (rc != 0) {
		m0_cob_put(cob);
		return M0_RC(rc);
	}

	*out = cob;
	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_locate(struct m0_cob_domain *dom,
			      struct m0_cob_oikey *oikey, uint64_t flags,
			      struct m0_cob **out)
{
	struct m0_cob *cob;
	int rc;

	M0_PRE(m0_fid_is_set(&oikey->cok_fid));

	M0_ENTRY("dom=%p oikey=("FID_F", %d)", dom,
		 FID_P(&oikey->cok_fid), (int)oikey->cok_linkno);

	/*
	 * Zero out "out" just in case that if we fail here, it is
	 * easier to find abnormal using of NULL cob.
	 */
	M0_ASSERT(out != NULL);
	*out = NULL;

	/* Get cob memory. */
	rc = m0_cob_alloc(dom, &cob);
	if (rc != 0)
		return M0_RC(rc);

	cob->co_oikey = *oikey;
	rc = cob_oi_lookup(cob);
	if (rc != 0) {
		M0_LOG(M0_DEBUG, "cob_oi_lookup() failed with %d", rc);
		m0_cob_put(cob);
		return M0_RC(rc);
	}

	rc = cob_ns_lookup(cob);
	if (rc != 0) {
		M0_LOG(M0_DEBUG, "cob_ns_lookup() failed with %d", rc);
		m0_cob_put(cob);
		return M0_RC(rc);
	}

	rc = cob_get_fabomg(cob, flags);
	if (rc != 0) {
		M0_LOG(M0_DEBUG, "cob_get_fabomg() failed with %d", rc);
		m0_cob_put(cob);
		return M0_RC(rc);
	}

	*out = cob;
	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_iterator_init(struct m0_cob *cob,
				     struct m0_cob_iterator *it,
				     struct m0_bitstring *name)
{
	int rc;

	/*
	 * Prepare entry key using passed started pos.
	 */
	rc = m0_cob_max_nskey_make(&it->ci_key, m0_cob_fid(cob),
				   m0_bitstring_buf_get(name),
				   m0_bitstring_len_get(name));
	if (rc != 0)
		return M0_RC(rc);

	m0_be_btree_cursor_init(&it->ci_cursor, &cob->co_dom->cd_namespace);
	it->ci_cob = cob;
	return M0_RC(rc);
}

M0_INTERNAL void m0_cob_iterator_fini(struct m0_cob_iterator *it)
{
	m0_be_btree_cursor_fini(&it->ci_cursor);
	m0_free(it->ci_key);
}

M0_INTERNAL int m0_cob_iterator_get(struct m0_cob_iterator *it)
{
        struct m0_cob_nskey *nskey;
	struct m0_buf key;
	int rc;

	M0_COB_NSKEY_LOG(ENTRY, "[%lx:%lx]/%.*s", it->ci_key);
	m0_buf_init(&key, it->ci_key, m0_cob_nskey_size(it->ci_key));
	rc = m0_be_btree_cursor_get_sync(&it->ci_cursor, &key, true);
	if (rc == 0) {
		m0_be_btree_cursor_kv_get(&it->ci_cursor, &key, NULL);
		nskey = (struct m0_cob_nskey *)key.b_addr;

                /**
                   Check if we are stil inside the object bounds. Assert and
                   key copy can be done only in this case.
                 */
		if (!m0_fid_eq(&nskey->cnk_pfid, m0_cob_fid(it->ci_cob)))
			rc = -ENOENT;

                if (rc == 0) {
                        M0_ASSERT(m0_cob_nskey_size(nskey) <= m0_cob_max_nskey_size());
		        memcpy(it->ci_key, nskey, m0_cob_nskey_size(nskey));
                }
	}
	M0_COB_NSKEY_LOG(LEAVE, "[%lx:%lx]/%.*s rc: %d", it->ci_key, rc);
	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_iterator_next(struct m0_cob_iterator *it)
{
        struct m0_cob_nskey *nskey;
	struct m0_buf key;
	int rc;

	M0_COB_NSKEY_LOG(ENTRY, "[%lx:%lx]/%.*s", it->ci_key);
	rc = m0_be_btree_cursor_next_sync(&it->ci_cursor);
	if (rc == 0) {
		m0_be_btree_cursor_kv_get(&it->ci_cursor, &key, NULL);
		nskey = (struct m0_cob_nskey *)key.b_addr;

                /**
                   Check if we are stil inside the object bounds. Assert and
                   key copy can be done only in this case.
                 */
		if (!m0_fid_eq(&nskey->cnk_pfid, m0_cob_fid(it->ci_cob)))
			rc = -ENOENT;
                if (rc == 0) {
                        M0_ASSERT(m0_cob_nskey_size(nskey) <= m0_cob_max_nskey_size());
		        memcpy(it->ci_key, nskey, m0_cob_nskey_size(nskey));
                }
	}
	M0_COB_NSKEY_LOG(LEAVE, "[%lx:%lx]/%.*s rc: %d", it->ci_key, rc);
	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_ea_iterator_init(struct m0_cob *cob,
				        struct m0_cob_ea_iterator *it,
				        struct m0_bitstring *name)
{
	int rc;

	/*
	 * Prepare entry key using passed started pos.
	 */
	rc = m0_cob_max_eakey_make(&it->ci_key, m0_cob_fid(cob),
				   m0_bitstring_buf_get(name),
				   m0_bitstring_len_get(name));
	if (rc != 0)
		return M0_RC(rc);

        it->ci_rec = m0_alloc(m0_cob_max_earec_size());
	if (it->ci_rec == NULL) {
	        m0_free(it->ci_key);
		return M0_RC(rc);
        }

	m0_be_btree_cursor_init(&it->ci_cursor, &cob->co_dom->cd_fileattr_ea);
	it->ci_cob = cob;
	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_ea_iterator_get(struct m0_cob_ea_iterator *it)
{
	int rc;
	struct m0_buf key;
        struct m0_cob_eakey *eakey;

	m0_buf_init(&key, it->ci_key, m0_cob_eakey_size(it->ci_key));
	rc = m0_be_btree_cursor_get_sync(&it->ci_cursor, &key, true);
	if (rc == 0) {
		m0_be_btree_cursor_kv_get(&it->ci_cursor, &key, NULL);
                eakey = (struct m0_cob_eakey *)key.b_addr;

                /**
                   Check if we are stil inside the object bounds. Assert and
                   key copy can be done only in this case.
                 */
                if (!m0_fid_eq(&eakey->cek_fid, m0_cob_fid(it->ci_cob)))
                         rc = -ENOENT;

                if (rc == 0) {
                        M0_ASSERT(m0_cob_eakey_size(eakey) <=
				  m0_cob_max_eakey_size());
		        memcpy(it->ci_key, eakey, m0_cob_eakey_size(eakey));
                }
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_ea_iterator_next(struct m0_cob_ea_iterator *it)
{
	int rc;
	struct m0_buf key;
        struct m0_cob_eakey *eakey;

	rc = m0_be_btree_cursor_next_sync(&it->ci_cursor);
	if (rc == 0) {
		m0_be_btree_cursor_kv_get(&it->ci_cursor, &key, NULL);
                eakey = (struct m0_cob_eakey *)key.b_addr;

                /**
                   Check if we are stil inside the object bounds. Assert and
                   key copy can be done only in this case.
                 */
                if (!m0_fid_eq(&eakey->cek_fid, m0_cob_fid(it->ci_cob)))
                         rc = -ENOENT;

                if (rc == 0) {
                        M0_ASSERT(m0_cob_eakey_size(eakey) <= m0_cob_max_eakey_size());
		        memcpy(it->ci_key, eakey, m0_cob_eakey_size(eakey));
                }
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_cob_ea_iterator_fini(struct m0_cob_ea_iterator *it)
{
	m0_free(it->ci_key);
	m0_free(it->ci_rec);
}

/**
   For assertions only.
 */
static bool m0_cob_is_valid(struct m0_cob *cob)
{
	return m0_fid_is_set(m0_cob_fid(cob));
}

M0_INTERNAL int m0_cob_alloc_omgid(struct m0_cob_domain *dom, uint64_t *omgid)
{
	struct m0_be_btree_cursor cursor;
	struct m0_cob_omgkey      omgkey;
	struct m0_buf             kbuf;
	int                       rc;

	M0_ENTRY();

	m0_be_btree_cursor_init(&cursor, &dom->cd_fileattr_omg);

	/*
	 * Look for ~0ULL terminator record and do a step back to find last
	 * allocated omgid. Terminator record should be prepared in storage
	 * init time (mkfs or else).
	 */
	omgkey.cok_omgid = ~0ULL;
	m0_buf_init(&kbuf, &omgkey, sizeof omgkey);
	rc = m0_be_btree_cursor_get_sync(&cursor, &kbuf, true);

	/*
	 * In case of error, most probably due to no terminator record found,
	 * one needs to run mkfs.
	 */
	if (rc == 0) {
                rc = m0_be_btree_cursor_prev_sync(&cursor);
		if (omgid != NULL) {
			if (rc == 0) {
				/* We found last allocated omgid.
				 * Bump it by one. */
				m0_be_btree_cursor_kv_get(&cursor, &kbuf, NULL);
				omgkey = *(struct m0_cob_omgkey*)kbuf.b_addr;
				*omgid = ++omgkey.cok_omgid;
			} else {
				/* No last allocated found, this alloc call is
				 * the first one. */
				*omgid = 0;
			}
		}
		rc = 0;
	}

	m0_be_btree_cursor_fini(&cursor);
	return M0_RC(rc);
}


M0_INTERNAL int m0_cob_create(struct m0_cob *cob,
			      struct m0_cob_nskey *nskey,
			      struct m0_cob_nsrec *nsrec,
			      struct m0_cob_fabrec *fabrec,
			      struct m0_cob_omgrec *omgrec,
			      struct m0_be_tx *tx)
{
	struct m0_buf         key;
	struct m0_buf         val;
	struct m0_cob_omgkey  omgkey;
	struct m0_cob_fabkey  fabkey;
	int                   rc;

	M0_PRE(cob != NULL);
	M0_PRE(nskey != NULL);
	M0_PRE(nsrec != NULL);
	M0_PRE(m0_fid_is_set(&nsrec->cnr_fid));
	M0_PRE(m0_fid_is_set(&nskey->cnk_pfid));

	M0_ENTRY("nskey=("FID_F", '%s') nsrec=("FID_F", %d)",
		 FID_P(&nskey->cnk_pfid), (char*)nskey->cnk_name.b_data,
		 FID_P(&nsrec->cnr_fid), (int)nsrec->cnr_linkno);

	if (omgrec != NULL) {
		rc = m0_cob_alloc_omgid(cob->co_dom, &nsrec->cnr_omgid);
		if (rc != 0)
			goto out;
	}

	cob->co_nskey = nskey;
	cob->co_flags |= M0_CA_NSKEY;

	/*
	 * This is what name_add will use to create new name.
	 */
	cob->co_nsrec = *nsrec;
	cob->co_flags |= M0_CA_NSREC;
	cob->co_nsrec.cnr_cntr = 0;

	/*
	 * Intialize counter with 1 which is what will be used
	 * for adding second name. We do it this way to avoid
	 * doing special m0_cob_update() solely for having
	 * this field stored in db.
	 */
	nsrec->cnr_cntr = 1;

	/*
	 * Let's create name, statdata and object index.
	 */
	rc = m0_cob_name_add(cob, nskey, nsrec, tx);
	if (rc != 0)
		goto out;
	cob->co_flags |= M0_CA_NSKEY_FREE;

	if (fabrec != NULL) {
		/*
		 * Prepare key for attribute tables.
		 */
		fabkey.cfb_fid = *m0_cob_fid(cob);

		/*
		 * Now let's update file attributes. Cache the fabrec.
		 */
		cob->co_fabrec = fabrec;

		/*
		 * Add to fileattr-basic table.
		 */
		m0_buf_init(&key, &fabkey, sizeof fabkey);
		m0_buf_init(&val, cob->co_fabrec,
			    m0_cob_fabrec_size(cob->co_fabrec));
		cob_table_insert(&cob->co_dom->cd_fileattr_basic, tx, &key,
		                                                      &val);
		cob->co_flags |= M0_CA_FABREC;
	}

	if (omgrec != NULL) {
		/*
		 * Prepare omg key.
		 */
		omgkey.cok_omgid = nsrec->cnr_omgid;

		/*
		 * Now let's update omg attributes. Cache the omgrec.
		 */
		cob->co_omgrec = *omgrec;
		cob->co_flags |= M0_CA_OMGREC;

		/*
		 * Add to fileattr-omg table.
		 */
		m0_buf_init(&key, &omgkey, sizeof omgkey);
		m0_buf_init(&val, &cob->co_omgrec, sizeof cob->co_omgrec);
		rc = cob_table_lookup(&cob->co_dom->cd_fileattr_omg, &key,
		                                                     &val);
		if (rc == -ENOENT)
			cob_table_insert(&cob->co_dom->cd_fileattr_omg, tx,
					 &key, &val);
		else
			M0_LOG(M0_DEBUG, "the same omgkey: %"PRIx64" is being "
			       "added multiple times", omgkey.cok_omgid);
		rc = 0;
	}
out:
	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_delete(struct m0_cob *cob, struct m0_be_tx *tx)
{
	struct m0_cob_fabkey fabkey;
	struct m0_cob_omgkey omgkey;
	struct m0_cob_oikey  oikey;
	struct m0_buf        key;
	struct m0_cob       *sdcob;
	bool                 sdname;
	int                  rc;

	M0_PRE(m0_cob_is_valid(cob));
	M0_PRE(cob->co_flags & M0_CA_NSKEY);

	m0_cob_oikey_make(&oikey, m0_cob_fid(cob), 0);
	rc = m0_cob_locate(cob->co_dom, &oikey, 0, &sdcob);
	if (rc != 0)
		goto out;
	sdname = (m0_cob_nskey_cmp(cob->co_nskey, sdcob->co_nskey) == 0);
	m0_cob_put(sdcob);

	/*
	 * Delete last name from namespace and object index.
	 */
	rc = m0_cob_name_del(cob, cob->co_nskey, tx);
	if (rc != 0)
		goto out;

	/*
	 * Is this a statdata name?
	 */
	if (sdname) {
		/*
		 * Remove from the fileattr_basic table.
		 */
		fabkey.cfb_fid = *m0_cob_fid(cob);
		m0_buf_init(&key, &fabkey, sizeof fabkey);

		/*
		 * Ignore errors; it's a dangling table entry but causes
		 * no harm.
		 */
		cob_table_delete(&cob->co_dom->cd_fileattr_basic, tx, &key);

		/*
		 * @todo: Omgrec may be shared between multiple objects.
		 * Delete should take this into account as well as update.
		 */
		omgkey.cok_omgid = cob->co_nsrec.cnr_omgid;

		/*
		 * Remove from the fileattr_omg table.
		 */
		m0_buf_init(&key, &omgkey, sizeof omgkey);

		/*
		 * Ignore errors; it's a dangling table entry but causes
		 * no harm.
		 */
		cob_table_delete(&cob->co_dom->cd_fileattr_omg, tx, &key);
	}
out:
	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_delete_put(struct m0_cob *cob, struct m0_be_tx *tx)
{
	int rc = m0_cob_delete(cob, tx);
	m0_cob_put(cob);
	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_update(struct m0_cob *cob,
			      struct m0_cob_nsrec *nsrec,
			      struct m0_cob_fabrec *fabrec,
			      struct m0_cob_omgrec *omgrec,
			      struct m0_be_tx *tx)
{
	struct m0_cob_omgkey  omgkey;
	struct m0_cob_fabkey  fabkey;
	struct m0_buf         key;
	struct m0_buf         val;
	int                   rc = 0;

	M0_PRE(m0_cob_is_valid(cob));
	M0_PRE(cob->co_flags & M0_CA_NSKEY);

	if (nsrec != NULL) {
		M0_ASSERT(nsrec->cnr_nlink > 0);

		cob->co_nsrec = *nsrec;
		cob->co_flags |= M0_CA_NSREC;

		m0_buf_init(&key, cob->co_nskey, m0_cob_nskey_size(cob->co_nskey));
		m0_buf_init(&val, &cob->co_nsrec, sizeof cob->co_nsrec);
		rc = cob_table_update(&cob->co_dom->cd_namespace,
				      tx, &key, &val);
	}

	if (rc == 0 && fabrec != NULL) {
		fabkey.cfb_fid = *m0_cob_fid(cob);
		if (fabrec != cob->co_fabrec) {
			if (cob->co_flags & M0_CA_FABREC)
				m0_free(cob->co_fabrec);
			cob->co_fabrec = fabrec;
		}
		cob->co_flags |= M0_CA_FABREC;


		m0_buf_init(&key, &fabkey, sizeof fabkey);
		m0_buf_init(&val,cob->co_fabrec, m0_cob_fabrec_size(cob->co_fabrec));
		rc = cob_table_update(&cob->co_dom->cd_fileattr_basic,
				      tx, &key, &val);
	}

	if (rc == 0 && omgrec != NULL) {
		/*
		 * @todo: Omgrec may be shared between multiple objects.
		 * We need to take this into account.
		 */
		omgkey.cok_omgid = cob->co_nsrec.cnr_omgid;

		cob->co_omgrec = *omgrec;
		cob->co_flags |= M0_CA_OMGREC;

		m0_buf_init(&key, &omgkey, sizeof omgkey);
		m0_buf_init(&val, &cob->co_omgrec, sizeof cob->co_omgrec);
		rc = cob_table_update(&cob->co_dom->cd_fileattr_omg,
				      tx, &key, &val);
	}

	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_name_add(struct m0_cob *cob,
				struct m0_cob_nskey *nskey,
				struct m0_cob_nsrec *nsrec,
				struct m0_be_tx *tx)
{
	struct m0_cob_oikey  oikey;
	struct m0_buf        key;
	struct m0_buf        val;
	int                  rc;

	M0_PRE(cob != NULL);
	M0_PRE(nskey != NULL);
	M0_PRE(m0_fid_is_set(&nskey->cnk_pfid));
	M0_PRE(m0_cob_is_valid(cob));

	m0_cob_oikey_make(&oikey, &nsrec->cnr_fid, cob->co_nsrec.cnr_cntr);

	m0_buf_init(&key, &oikey, sizeof oikey);
	m0_buf_init(&val, nskey, m0_cob_nskey_size(nskey));
	rc = cob_table_insert(&cob->co_dom->cd_object_index, tx, &key, &val);
	if (rc != 0)
		return M0_RC_INFO(rc, "fid="FID_F" cntr=%u",
				  FID_P(&nsrec->cnr_fid),
				  (unsigned)cob->co_nsrec.cnr_cntr);

	m0_buf_init(&key, nskey, m0_cob_nskey_size(nskey));
	m0_buf_init(&val, nsrec, sizeof *nsrec);
	rc = cob_table_insert(&cob->co_dom->cd_namespace, tx, &key, &val);
	if (rc != 0) {
		m0_buf_init(&key, &oikey, sizeof oikey);
		cob_table_delete(&cob->co_dom->cd_object_index, tx, &key);
		return M0_RC_INFO(rc, "parent="FID_F" name='%*s'",
				  FID_P(&nskey->cnk_pfid),
				  nskey->cnk_name.b_len,
				  (char*)nskey->cnk_name.b_data);
	}

	return rc;
}

M0_INTERNAL int m0_cob_name_del(struct m0_cob *cob,
				struct m0_cob_nskey *nskey,
				struct m0_be_tx *tx)
{
	struct m0_cob_oikey oikey;
	struct m0_cob_nsrec nsrec;
	struct m0_buf       key;
	struct m0_buf       val;
	int                 rc;

	M0_PRE(m0_cob_is_valid(cob));
	M0_PRE(cob->co_flags & M0_CA_NSKEY);

	/*
	 * Kill name from namespace.
	 */
	m0_buf_init(&key, nskey, m0_cob_nskey_size(nskey));
	m0_buf_init(&val, &nsrec, sizeof nsrec);

	rc = cob_table_lookup(&cob->co_dom->cd_namespace, &key, &val);
	if (rc != 0)
		goto out;

	rc = cob_table_delete(&cob->co_dom->cd_namespace, tx, &key);
	if (rc != 0)
		goto out;

	/*
	 * Let's also kill object index entry.
	 */
	m0_cob_oikey_make(&oikey, m0_cob_fid(cob), nsrec.cnr_linkno);
	m0_buf_init(&key, &oikey, sizeof oikey);
	rc = cob_table_delete(&cob->co_dom->cd_object_index, tx, &key);

out:
	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_name_update(struct m0_cob *cob,
				   struct m0_cob_nskey *srckey,
				   struct m0_cob_nskey *tgtkey,
				   struct m0_be_tx *tx)
{
	struct m0_cob_oikey  oikey;
	struct m0_buf        key;
	struct m0_buf        val;
	int                  rc;

	M0_PRE(m0_cob_is_valid(cob));
	M0_PRE(srckey != NULL && tgtkey != NULL);

	/*
	 * Insert new record with nsrec found with srckey.
	 */
	m0_buf_init(&key, srckey, m0_cob_nskey_size(srckey));
	rc = cob_table_lookup(&cob->co_dom->cd_namespace, &key, &val);
	if (rc != 0)
		goto out;

	m0_buf_init(&key, tgtkey, m0_cob_nskey_size(tgtkey));
	/* here @val consists value to insert */
	cob_table_insert(&cob->co_dom->cd_namespace, tx, &key, &val);

	/*
	 * Kill old record. Error will be returned if
	 * nothing found.
	 */
	m0_buf_init(&key, srckey, m0_cob_nskey_size(srckey));
	rc = cob_table_delete(&cob->co_dom->cd_namespace, tx, &key);
	if (rc != 0)
		goto out;

	/* Update object index */
	m0_buf_init(&key, &oikey, sizeof oikey);
	m0_buf_init(&val, tgtkey, m0_cob_nskey_size(tgtkey));
	rc = cob_table_update(&cob->co_dom->cd_object_index, tx, &key, &val);
	if (rc != 0)
		goto out;

	/*
	 * Update key to new one.
	 */
	if (cob->co_flags & M0_CA_NSKEY_FREE)
		m0_free(cob->co_nskey);
	m0_cob_nskey_make(&cob->co_nskey, &tgtkey->cnk_pfid,
			  m0_bitstring_buf_get(&tgtkey->cnk_name),
			  m0_bitstring_len_get(&tgtkey->cnk_name));
	cob->co_flags |= M0_CA_NSKEY_FREE;
out:
	return M0_RC(rc);
}

M0_INTERNAL void m0_cob_nsrec_init(struct m0_cob_nsrec *nsrec)
{
	m0_format_header_pack(&nsrec->cnr_header, &(struct m0_format_tag){
		.ot_version = M0_COB_NSREC_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_COB_NSREC,
		.ot_footer_offset = offsetof(struct m0_cob_nsrec, cnr_footer)
	});
	m0_format_footer_update(nsrec);
}

M0_INTERNAL int m0_cob_setattr(struct m0_cob *cob, struct m0_cob_attr *attr,
			       struct m0_be_tx *tx)
{
	struct m0_cob_nsrec   nsrec_prev;
	struct m0_cob_nsrec  *nsrec = NULL;
	struct m0_cob_fabrec *fabrec = NULL;
	struct m0_cob_omgrec *omgrec = NULL;
	int                   rc;

	M0_ENTRY();
	M0_ASSERT(cob != NULL);

	/*
	 * Handle basic stat fields update.
	 */
	if (cob->co_flags & M0_CA_NSREC) {
		nsrec = &cob->co_nsrec;
		m0_cob_nsrec_init(nsrec);
		nsrec_prev = *nsrec;
		if (attr->ca_valid & M0_COB_ATIME)
			nsrec->cnr_atime = attr->ca_atime;
		if (attr->ca_valid & M0_COB_MTIME)
			nsrec->cnr_mtime = attr->ca_mtime;
		if (attr->ca_valid & M0_COB_CTIME)
			nsrec->cnr_ctime = attr->ca_ctime;
		if (attr->ca_valid & M0_COB_SIZE) {
			if (nsrec->cnr_size > attr->ca_size &&
			    attr->ca_size != 0) {
				rc = -EINVAL;
				/* Restore older attributed. */
				*nsrec = nsrec_prev;
				goto out;
			}
			nsrec->cnr_size = attr->ca_size;
		}
		/*if (attr->ca_valid & M0_COB_RDEV)
			nsrec->cnr_rdev = attr->ca_rdev;*/
		if (attr->ca_valid & M0_COB_BLOCKS)
			nsrec->cnr_blocks = attr->ca_blocks;
		if (attr->ca_valid & M0_COB_BLKSIZE)
			nsrec->cnr_blksize = attr->ca_blksize;
		if (attr->ca_valid & M0_COB_LID)
			nsrec->cnr_lid = attr->ca_lid;
		if (attr->ca_valid & M0_COB_NLINK) {
			M0_ASSERT(attr->ca_nlink > 0);
			nsrec->cnr_nlink = attr->ca_nlink;
		}
		//nsrec->cnr_version = attr->ca_version;
	}

	/*
	 * Handle uid/gid/mode update.
	 */
	if (cob->co_flags & M0_CA_OMGREC) {
		omgrec = &cob->co_omgrec;
		if (attr->ca_valid & M0_COB_UID)
			omgrec->cor_uid = attr->ca_uid;
		if (attr->ca_valid & M0_COB_GID)
			omgrec->cor_gid = attr->ca_gid;
		if (attr->ca_valid & M0_COB_MODE)
			omgrec->cor_mode = attr->ca_mode;
	}

	/*
	 * @todo: update fabrec.
	 */
	if (cob->co_flags & M0_CA_FABREC)
		fabrec = cob->co_fabrec;

	rc = m0_cob_update(cob, nsrec, fabrec, omgrec, tx);
out:
	M0_LEAVE("rc: %d", rc);
	return M0_RC(rc);

}

M0_INTERNAL int m0_cob_size_update(struct m0_cob *cob, uint64_t size,
				   struct m0_be_tx *tx)
{
	int rc;

	cob->co_nsrec.cnr_size = size;
	rc = m0_cob_update(cob, &cob->co_nsrec, NULL, NULL, tx);

	return rc != 0 ? M0_ERR(rc) : M0_RC(rc);
}

M0_INTERNAL int m0_cob_ea_get(struct m0_cob *cob,
                              struct m0_cob_eakey *eakey,
                              struct m0_cob_earec *out,
                              struct m0_be_tx *tx)
{
	struct m0_buf key;
	struct m0_buf val;
	int           rc;

	m0_buf_init(&key, eakey, m0_cob_eakey_size(eakey));
	m0_buf_init(&val, out, m0_cob_max_earec_size());
	rc = cob_table_lookup(&cob->co_dom->cd_fileattr_ea, &key, &val);

	return M0_RC(rc);
}

M0_INTERNAL int m0_cob_ea_set(struct m0_cob *cob,
			      struct m0_cob_eakey *eakey,
			      struct m0_cob_earec *earec,
			      struct m0_be_tx *tx)
{
	struct m0_buf key;
	struct m0_buf val;

	M0_PRE(cob != NULL);
	M0_PRE(eakey != NULL);
	M0_PRE(m0_fid_is_set(&eakey->cek_fid));
	M0_PRE(m0_cob_is_valid(cob));

	m0_cob_ea_del(cob, eakey, tx);

	m0_buf_init(&key, eakey, m0_cob_eakey_size(eakey));
	m0_buf_init(&val, earec, m0_cob_earec_size(earec));
	cob_table_insert(&cob->co_dom->cd_fileattr_ea, tx, &key, &val);

	return 0;
}

M0_INTERNAL int m0_cob_ea_del(struct m0_cob *cob,
			      struct m0_cob_eakey *eakey,
			      struct m0_be_tx *tx)
{
	struct m0_buf key;
	int           rc;

	M0_PRE(m0_cob_is_valid(cob));

	m0_buf_init(&key, eakey, m0_cob_eakey_size(eakey));
	rc = cob_table_delete(&cob->co_dom->cd_fileattr_ea, tx, &key);
	return M0_RC(rc);
}

enum cob_table_optype {
	COB_TABLE_DELETE,
	COB_TABLE_UPDATE,
	COB_TABLE_INSERT,
};

enum cob_table_kvtype {
	COB_KVTYPE_OMG,
	COB_KVTYPE_FAB,
	COB_KVTYPE_FEA,
	COB_KVTYPE_NS,
	COB_KVTYPE_OI,
};

static void cob_table_tx_credit(struct m0_be_btree *tree,
				enum cob_table_optype t_optype,
				enum cob_table_kvtype t_kvtype,
				struct m0_be_tx_credit *accum)
{
	const struct {
		m0_bcount_t s_key;
		m0_bcount_t s_rec;
	} kv_size[] = {
		[COB_KVTYPE_OMG] = {
			.s_key = sizeof(struct m0_cob_omgkey),
			.s_rec = sizeof(struct m0_cob_omgrec),
		},
		[COB_KVTYPE_FAB] = {
			.s_key = sizeof(struct m0_cob_fabkey),
			.s_rec = m0_cob_max_fabrec_size(),
		},
		[COB_KVTYPE_FEA] = {
			.s_key = m0_cob_max_eakey_size(),
			.s_rec = m0_cob_max_earec_size(),
		},
		[COB_KVTYPE_NS] = {
			.s_key = m0_cob_max_nskey_size(),
			.s_rec = sizeof(struct m0_cob_nsrec),
		},
		[COB_KVTYPE_OI] = {
			.s_key = sizeof(struct m0_cob_oikey),
			.s_rec = m0_cob_max_nskey_size(),
				     /* XXX ^^^^^ is it right? */
		},
	};
	m0_bcount_t ksize;
	m0_bcount_t vsize;

	M0_PRE(M0_IN(t_kvtype, (COB_KVTYPE_OMG, COB_KVTYPE_FAB, COB_KVTYPE_FEA,
				COB_KVTYPE_NS, COB_KVTYPE_OI)));

	ksize = kv_size[t_kvtype].s_key;
	vsize = kv_size[t_kvtype].s_rec;

	switch (t_optype) {
	case COB_TABLE_DELETE:
		m0_be_btree_delete_credit(tree, 1, ksize, vsize, accum);
		break;
	case COB_TABLE_UPDATE:
		m0_be_btree_update_credit(tree, 1, vsize, accum);
		break;
	case COB_TABLE_INSERT:
		m0_be_btree_insert_credit(tree, 1, ksize, vsize, accum);
		break;
	default:
		M0_IMPOSSIBLE("Impossible cob btree optype");
	}
}

M0_INTERNAL void m0_cob_tx_credit(struct m0_cob_domain *dom,
				  enum m0_cob_op optype,
				  struct m0_be_tx_credit *accum)
{
	int i;

#define TCREDIT(table, t_optype, t_kvtype, accum)		\
	cob_table_tx_credit((table), COB_TABLE_##t_optype,	\
			    COB_KVTYPE_##t_kvtype, (accum))

	switch (optype) {
	case M0_COB_OP_DOMAIN_MKFS:
		TCREDIT(&dom->cd_fileattr_omg, INSERT, OMG, accum);
		for (i = 0; i < 2; ++i)
			m0_cob_tx_credit(dom, M0_COB_OP_CREATE, accum);
		break;
	case M0_COB_OP_TRUNCATE:
	case M0_COB_OP_LOOKUP:
	case M0_COB_OP_LOCATE:
		break;
	case M0_COB_OP_CREATE:
		m0_cob_tx_credit(dom, M0_COB_OP_NAME_ADD, accum);
		TCREDIT(&dom->cd_fileattr_basic, INSERT, FAB, accum);
		TCREDIT(&dom->cd_fileattr_omg, INSERT, OMG, accum);
		break;
	case M0_COB_OP_DELETE:
	case M0_COB_OP_DELETE_PUT:
		m0_cob_tx_credit(dom, M0_COB_OP_LOCATE, accum);
		m0_cob_tx_credit(dom, M0_COB_OP_NAME_DEL, accum);
		TCREDIT(&dom->cd_fileattr_basic, DELETE, FAB, accum);
		TCREDIT(&dom->cd_fileattr_omg, DELETE, FAB, accum);
		break;
	case M0_COB_OP_UPDATE:
		TCREDIT(&dom->cd_namespace, UPDATE, NS, accum);
		TCREDIT(&dom->cd_fileattr_basic, UPDATE, FAB, accum);
		TCREDIT(&dom->cd_fileattr_omg, UPDATE, OMG, accum);
		break;
	case M0_COB_OP_FEA_SET:
		TCREDIT(&dom->cd_fileattr_ea, DELETE, FEA, accum);
		TCREDIT(&dom->cd_fileattr_ea, INSERT, FEA, accum);
		break;
	case M0_COB_OP_FEA_DEL:
		TCREDIT(&dom->cd_fileattr_ea, DELETE, FEA, accum);
		break;
	case M0_COB_OP_NAME_ADD:
		TCREDIT(&dom->cd_object_index, INSERT, OI, accum);
		TCREDIT(&dom->cd_object_index, DELETE, OI, accum);
		TCREDIT(&dom->cd_namespace, INSERT, NS, accum);
		break;
	case M0_COB_OP_NAME_DEL:
		TCREDIT(&dom->cd_namespace, DELETE, NS, accum);
		TCREDIT(&dom->cd_object_index, DELETE, OI, accum);
		break;
	case M0_COB_OP_NAME_UPDATE:
		TCREDIT(&dom->cd_namespace, INSERT, NS, accum);
		TCREDIT(&dom->cd_namespace, DELETE, NS, accum);
		TCREDIT(&dom->cd_object_index, UPDATE, OI, accum);
		break;
	default:
		M0_IMPOSSIBLE("Impossible cob optype");
	}
#undef TCREDIT
}

M0_INTERNAL void m0_cob_ea_get_credit(struct m0_cob *cob,
				      struct m0_cob_eakey *eakey,
				      struct m0_cob_earec *out,
				      struct m0_be_tx_credit *accum)
{
	M0_IMPOSSIBLE("-ENOSYS");
}

M0_INTERNAL void m0_cob_ea_set_credit(struct m0_cob *cob,
				      struct m0_cob_eakey *eakey,
				      struct m0_cob_earec *earec,
				      struct m0_be_tx_credit *accum)
{
	M0_IMPOSSIBLE("-ENOSYS");
}

M0_INTERNAL void m0_cob_ea_del_credit(struct m0_cob *cob,
				      struct m0_cob_eakey *eakey,
				      struct m0_be_tx_credit *accum)
{
	M0_IMPOSSIBLE("-ENOSYS");
}

M0_INTERNAL void m0_cob_ea_iterator_init_credit(struct m0_cob *cob,
						struct m0_cob_ea_iterator *it,
						struct m0_bitstring *name,
						struct m0_be_tx_credit *accum)
{
	M0_IMPOSSIBLE("-ENOSYS");
}
#endif

/** @} end group cob */
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
