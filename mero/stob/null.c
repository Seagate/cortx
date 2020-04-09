/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dmytro Podgornyi <dmytro_podgornyi@xyratex.com>
 * Original creation date: 5-Mar-2014
 */

#include "stob/null.h"

#include "lib/errno.h"
#include "lib/memory.h"		/* M0_ALLOC_PTR */
#include "lib/tlist.h"		/* m0_tl */
#include "lib/mutex.h"		/* m0_mutex */
#include "lib/misc.h"		/* M0_SET0 */
#include "lib/string.h"		/* m0_strdup */

#include "fid/fid.h"

#include "stob/type.h"
#include "stob/domain.h"
#include "stob/stob.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_STOB
#include "lib/trace.h"

/**
 * Null stob doesn't have persistent storage so we want to keep track
 * of all created domains/stobs in RAM.
 */

enum {
	STOB_TYPE_NULL = 0xFF,
};

struct stob_null_lists;
struct stob_null_domain;

struct stob_null {
	struct stob_null_domain *sn_dom;
	struct m0_fid            sn_stob_fid;
	struct m0_tlink          sn_link;
	uint64_t                 sn_magic;
};

struct stob_null_domain {
	struct m0_stob_domain   snd_dom;
	struct stob_null_lists *snd_lists;
	char                   *snd_path;
	uint64_t                snd_dom_key;
	struct m0_tlink         snd_link;
	uint64_t                snd_magic;
	struct m0_tl            snd_stobs;
	struct m0_mutex         snd_lock;
};

struct stob_null_lists {
	struct m0_tl    snl_domains;
	struct m0_mutex snl_lock;
};

M0_TL_DESCR_DEFINE(stob_null_stobs, "list of null stobs in the domain",
		   static, struct stob_null, sn_link, sn_magic,
		   M0_STOB_NULL_MAGIC, M0_STOB_NULL_HEAD_MAGIC);
M0_TL_DEFINE(stob_null_stobs, static, struct stob_null);

M0_TL_DESCR_DEFINE(stob_null_domains, "list of null stob domains",
		   static, struct stob_null_domain, snd_link, snd_magic,
		   M0_STOB_DOM_NULL_MAGIC, M0_STOB_DOM_NULL_HEAD_MAGIC);
M0_TL_DEFINE(stob_null_domains, static, struct stob_null_domain);

/* export */
const struct m0_stob_type m0_stob_null_type;

static struct m0_stob_type_ops stob_null_type_ops;
static struct m0_stob_domain_ops stob_null_domain_ops;
static struct m0_stob_ops stob_null_ops;

static void stob_null_type_register(struct m0_stob_type *type)
{
	struct stob_null_lists *snl;

	M0_ALLOC_PTR(snl);
	if (snl != NULL) {
		stob_null_domains_tlist_init(&snl->snl_domains);
		m0_mutex_init(&snl->snl_lock);
	}
	type->st_private = snl;
}

static void stob_null_type_deregister(struct m0_stob_type *type)
{
	struct stob_null_lists *snl = type->st_private;

	m0_mutex_fini(&snl->snl_lock);
	stob_null_domains_tlist_fini(&snl->snl_domains);
	m0_free0(&type->st_private);
}

static struct stob_null_domain *
stob_null_domain_find(struct stob_null_lists *snl,
		      const char *path,
		      bool take_lock)
{
	struct stob_null_domain *snd;

	if (take_lock)
		m0_mutex_lock(&snl->snl_lock);
	snd = m0_tl_find(stob_null_domains, snd, &snl->snl_domains,
			 strcmp(snd->snd_path, path) == 0);
	if (take_lock)
		m0_mutex_unlock(&snl->snl_lock);
	return snd;
}

static int stob_null_domain_add(struct stob_null_domain *snd,
				struct stob_null_lists *snl)
{
	struct stob_null_domain *snd1;
	int                      rc;

	m0_mutex_lock(&snl->snl_lock);
	snd1 = stob_null_domain_find(snl, snd->snd_path, false);
	rc = snd1 != NULL ? -EEXIST : 0;
	if (rc == 0)
		stob_null_domains_tlink_init_at(snd, &snl->snl_domains);
	m0_mutex_unlock(&snl->snl_lock);
	return M0_RC(rc);
}

static void stob_null_domain_del(struct stob_null_domain *snd,
				 struct stob_null_lists *snl)
{
	m0_mutex_lock(&snl->snl_lock);
	stob_null_domains_tlink_del_fini(snd);
	m0_mutex_unlock(&snl->snl_lock);
}

static struct stob_null_domain *
stob_null_domain_container(struct m0_stob_domain *dom)
{
	return container_of(dom, struct stob_null_domain, snd_dom);
}

static int stob_null_domain_cfg_init_parse(const char *str_cfg_init,
					   void **cfg_init)
{
       return 0;
}

static void stob_null_domain_cfg_init_free(void *cfg_init)
{
}

static int stob_null_domain_cfg_create_parse(const char *str_cfg_create,
				      void **cfg_create)
{
       return 0;
}

static void stob_null_domain_cfg_create_free(void *cfg_create)
{
}

static int stob_null_domain_init(struct m0_stob_type *type,
				 const char *location_data,
				 void *cfg_init,
				 struct m0_stob_domain **out)
{
	struct stob_null_domain *snd;
	struct stob_null_lists  *snl = type->st_private;
	struct m0_fid            dom_id;
	uint8_t                  type_id;
	int                      rc;

	rc = snl == NULL ? -ENOMEM : 0;
	snd = rc == 0 ? stob_null_domain_find(snl, location_data, true) : NULL;
	rc = rc ?: snd == NULL ? -ENOENT : 0;
	if (rc == 0) {
		M0_SET0(&snd->snd_dom);
		snd->snd_dom.sd_ops = &stob_null_domain_ops;

		type_id = m0_stob_type_id_get(type);
		m0_stob_domain__dom_id_make(&dom_id, type_id,
					    0, snd->snd_dom_key);
		m0_stob_domain__id_set(&snd->snd_dom, &dom_id);
	}
	*out = rc == 0 ? &snd->snd_dom : NULL;
	return M0_RC(rc);
}

static void stob_null_domain_fini(struct m0_stob_domain *dom)
{
}

static int stob_null_domain_create(struct m0_stob_type *type,
				   const char *location_data,
				   uint64_t dom_key,
				   void *cfg_create)
{
	struct stob_null_domain *snd;
	struct stob_null_lists  *snl = type->st_private;
	int                      rc;

	rc = snl == NULL ? -ENOMEM : 0;
	snd = rc == 0 ? stob_null_domain_find(snl, location_data, true) : NULL;
	rc = rc ?: snd != NULL ? -EEXIST : 0;
	if (rc == 0)
		M0_ALLOC_PTR(snd);
	rc = rc ?: snd == NULL ? -ENOMEM : 0;
	if (rc == 0) {
		m0_mutex_init(&snd->snd_lock);
		snd->snd_dom_key = dom_key,
		m0_stob_domain__dom_id_make(&snd->snd_dom.sd_id,
					    m0_stob_type_id_get(type),
					    0, dom_key);
		snd->snd_path	 = m0_strdup(location_data);
		snd->snd_lists	 = snl;
		stob_null_stobs_tlist_init(&snd->snd_stobs);

		rc = stob_null_domain_add(snd, snl);
		if (rc != 0) {
			stob_null_stobs_tlist_fini(&snd->snd_stobs);
			m0_free(snd->snd_path);
			m0_mutex_fini(&snd->snd_lock);
			m0_free(snd);
		}
	}

	return M0_RC(rc);
}

static int stob_null_domain_destroy(struct m0_stob_type *type,
				    const char *location_data)
{
	struct stob_null_lists  *snl = type->st_private;
	struct stob_null_domain *snd;

	snd = stob_null_domain_find(snl, location_data, true);
	if (snd != NULL) {
		stob_null_domain_del(snd, snd->snd_lists);
		stob_null_stobs_tlist_fini(&snd->snd_stobs);
		m0_free(snd->snd_path);
		m0_mutex_fini(&snd->snd_lock);
		m0_free(snd);
	}
	return 0;
}

static struct m0_stob *stob_null_alloc(struct m0_stob_domain *dom,
				       const struct m0_fid *stob_fid)
{
	return m0_alloc(sizeof(struct m0_stob));
}

static void stob_null_free(struct m0_stob_domain *dom,
			   struct m0_stob *stob)
{
	m0_free(stob);
}

static int stob_null_cfg_parse(const char *str_cfg_create,
			       void **cfg_create)
{
       return 0;
}

static void stob_null_cfg_free(void *cfg_create)
{
}

static struct stob_null *stob_null_find(struct stob_null_domain *snd,
					const struct m0_fid *stob_fid,
					bool take_lock)
{
	struct stob_null *sn;

	if (take_lock)
		m0_mutex_lock(&snd->snd_lock);
	sn = m0_tl_find(stob_null_stobs, sn, &snd->snd_stobs,
			m0_fid_cmp(&sn->sn_stob_fid, stob_fid) == 0);
	if (take_lock)
		m0_mutex_unlock(&snd->snd_lock);
	return sn;
}

static int stob_null_add(struct stob_null *sn, struct stob_null_domain *snd)
{
	struct stob_null *sn1;
	int               rc;

	m0_mutex_lock(&snd->snd_lock);
	sn1 = stob_null_find(snd, &sn->sn_stob_fid, false);
	rc = sn1 != NULL ? -EEXIST : 0;
	if (sn1 == NULL)
		stob_null_stobs_tlink_init_at(sn, &snd->snd_stobs);
	m0_mutex_unlock(&snd->snd_lock);
	return M0_RC(rc);
}

static void stob_null_del(struct stob_null *sn, struct stob_null_domain *snd)
{
	m0_mutex_lock(&snd->snd_lock);
	stob_null_stobs_tlink_del_fini(sn);
	m0_mutex_unlock(&snd->snd_lock);
}

static int stob_null_init(struct m0_stob *stob,
			  struct m0_stob_domain *dom,
			  const struct m0_fid *stob_fid)
{
	struct stob_null_domain *snd = stob_null_domain_container(dom);
	struct stob_null        *sn  = stob_null_find(snd, stob_fid, true);

	stob->so_private = sn;
	stob->so_ops = &stob_null_ops;

	if (sn != NULL)
		sn->sn_dom = snd;

	return sn == NULL ? -ENOENT : 0;
}

static void stob_null_fini(struct m0_stob *stob)
{
}

static void stob_null_create_credit(struct m0_stob_domain *dom,
			     struct m0_be_tx_credit *accum)
{
}

static int stob_null_create(struct m0_stob *stob,
			    struct m0_stob_domain *dom,
			    struct m0_dtx *dtx,
			    const struct m0_fid *stob_fid,
			    void *cfg)
{
	struct stob_null_domain *snd = stob_null_domain_container(dom);
	struct stob_null        *sn;
	int                      rc;

	M0_ALLOC_PTR(sn);
	rc = sn == NULL ? -ENOMEM : 0;
	if (sn != NULL) {
		sn->sn_stob_fid = *stob_fid;
		rc = stob_null_add(sn, snd);
		if (rc != 0) {
			m0_free(sn);
		} else {
			stob_null_init(stob, dom, stob_fid);
		}
	}
	/* TODO allocate memory for stob-io */
	return M0_RC(rc);
}

static void stob_null_destroy_credit(struct m0_stob *stob,
				     struct m0_be_tx_credit *accum)
{
}

static int stob_null_destroy(struct m0_stob *stob, struct m0_dtx *dtx)
{
	struct stob_null *sn = stob->so_private;

	stob_null_del(sn, sn->sn_dom);
	m0_free(sn);
	return 0;
}

static int stob_null_punch(struct m0_stob *stob,
			   struct m0_indexvec *range,
			   struct m0_dtx *dtx)
{
	return 0;
}

static uint32_t stob_null_block_shift(struct m0_stob *stob)
{
	return 0;
}

static struct m0_stob_type_ops stob_null_type_ops = {
	.sto_register                = &stob_null_type_register,
	.sto_deregister              = &stob_null_type_deregister,
	.sto_domain_cfg_init_parse   = &stob_null_domain_cfg_init_parse,
	.sto_domain_cfg_init_free    = &stob_null_domain_cfg_init_free,
	.sto_domain_cfg_create_parse = &stob_null_domain_cfg_create_parse,
	.sto_domain_cfg_create_free  = &stob_null_domain_cfg_create_free,
	.sto_domain_init             = &stob_null_domain_init,
	.sto_domain_create           = &stob_null_domain_create,
	.sto_domain_destroy          = &stob_null_domain_destroy,
};

static struct m0_stob_domain_ops stob_null_domain_ops = {
	.sdo_fini               = &stob_null_domain_fini,
	.sdo_stob_alloc         = &stob_null_alloc,
	.sdo_stob_free          = &stob_null_free,
	.sdo_stob_cfg_parse     = &stob_null_cfg_parse,
	.sdo_stob_cfg_free      = &stob_null_cfg_free,
	.sdo_stob_init          = &stob_null_init,
	.sdo_stob_create_credit = &stob_null_create_credit,
	.sdo_stob_create        = &stob_null_create,
};

static struct m0_stob_ops stob_null_ops = {
	.sop_fini           = &stob_null_fini,
	.sop_destroy_credit = &stob_null_destroy_credit,
	.sop_destroy        = &stob_null_destroy,
	.sop_punch          = &stob_null_punch,
	.sop_block_shift    = &stob_null_block_shift,
};

const struct m0_stob_type m0_stob_null_type = {
	.st_ops  = &stob_null_type_ops,
	.st_fidt = {
		.ft_id   = STOB_TYPE_NULL,
		.ft_name = "nullstob",
	},
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
