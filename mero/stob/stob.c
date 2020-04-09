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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 04/28/2010
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_STOB
#include "lib/trace.h"

#include "lib/misc.h"    /* M0_SET0 */
#include "lib/arith.h"   /* M0_3WAY */
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/string.h"  /* m0_strdup */
#include "lib/types.h"	 /* PRIu64 */
#include "mero/magic.h"

#include "stob/type.h"
#include "stob/domain.h"
#include "stob/stob.h"
#include "stob/stob_internal.h"
#include "stob/stob_xc.h"

/**
   @addtogroup stob
   @{
 */

M0_INTERNAL struct m0_stob_cache *
m0_stob_domain__cache(struct m0_stob_domain *dom)
{
	return &dom->sd_cache;
}

M0_INTERNAL struct m0_stob *
m0_stob_domain__stob_alloc(struct m0_stob_domain *dom,
			   const struct m0_fid *stob_fid)
{
	return dom->sd_ops->sdo_stob_alloc(dom, stob_fid);
}

M0_INTERNAL void m0_stob_domain__stob_free(struct m0_stob_domain *dom,
					   struct m0_stob *stob)
{
	dom->sd_ops->sdo_stob_free(dom, stob);
}

/** @todo move allocation out of cache lock if needed */
M0_INTERNAL int m0_stob_find_by_key(struct m0_stob_domain *dom,
				    const struct m0_fid *stob_fid,
				    struct m0_stob **out)
{
	struct m0_stob_cache *cache = m0_stob_domain__cache(dom);
	struct m0_stob	     *stob;

	m0_stob_cache_lock(cache);
	stob = m0_stob_cache_lookup(cache, stob_fid);
	if (stob != NULL) {
		M0_CNT_INC(stob->so_ref);
	} else {
		stob = m0_stob_domain__stob_alloc(dom, stob_fid);
		if (stob != NULL) {
			stob->so_domain = dom;
			m0_stob__id_set(stob, stob_fid);
			stob->so_state = CSS_UNKNOWN;
			stob->so_ref = 1;
			m0_stob_cache_add(cache, stob);
		}
	}
	m0_stob_cache_unlock(cache);

	*out = stob;
	return stob == NULL ? M0_ERR(-ENOMEM) : M0_RC(0);
}

M0_INTERNAL int m0_stob_find(const struct m0_stob_id *id, struct m0_stob **out)
{
	struct m0_stob_domain *dom = m0_stob_domain_find_by_stob_id(id);

	*out = NULL; /* XXX Workaround for gcc warning. */

	return dom == NULL ? M0_ERR(-EINVAL) :
	       m0_stob_find_by_key(dom, &id->si_fid, out);
}

M0_INTERNAL int m0_stob_lookup_by_key(struct m0_stob_domain *dom,
				      const struct m0_fid *stob_fid,
				      struct m0_stob **out)
{
	struct m0_stob_cache *cache = m0_stob_domain__cache(dom);
	struct m0_stob	     *stob;

	m0_stob_cache_lock(cache);
	stob = m0_stob_cache_lookup(cache, stob_fid);
	if (stob != NULL)
		M0_CNT_INC(stob->so_ref);
	m0_stob_cache_unlock(cache);

	*out = stob;
	return stob == NULL ? -ENOENT : 0;
}

M0_INTERNAL int m0_stob_lookup(const struct m0_stob_id *id,
			       struct m0_stob **out)
{
	struct m0_stob_domain *dom = m0_stob_domain_find_by_stob_id(id);

	return dom == NULL ? -EINVAL :
	       m0_stob_lookup_by_key(dom, &id->si_fid, out);
}

M0_INTERNAL int m0_stob_locate(struct m0_stob *stob)
{
	struct m0_stob_domain *dom = m0_stob_dom_get(stob);
	int		       rc;

	M0_ENTRY();
	M0_PRE(stob->so_ref > 0);
	M0_PRE(M0_IN(m0_stob_state_get(stob), (CSS_UNKNOWN, CSS_NOENT)));

	rc = dom->sd_ops->sdo_stob_init(stob, dom, m0_stob_fid_get(stob));
	if (rc == 0) {
		m0_stob__state_set(stob, CSS_EXISTS);
		m0_mutex_init(&stob->so_ref_mutex);
		m0_chan_init(&stob->so_ref_chan, &stob->so_ref_mutex);
	} else if (rc == -ENOENT)
		m0_stob__state_set(stob, CSS_NOENT);

	return M0_RC(M0_IN(rc, (0, -ENOENT)) ? 0 : rc);
}

M0_INTERNAL void m0_stob_create_credit(struct m0_stob_domain *dom,
				       struct m0_be_tx_credit *accum)
{
	dom->sd_ops->sdo_stob_create_credit(dom, accum);
}

M0_INTERNAL int m0_stob_create(struct m0_stob *stob,
			       struct m0_dtx *dtx,
			       const char *str_cfg)
{
	struct m0_stob_domain		*dom	  = m0_stob_dom_get(stob);
	const struct m0_stob_domain_ops *dom_ops  = dom->sd_ops;
	void				*cfg;
	int				 rc;

	M0_ENTRY("stob=%p so_id="STOB_ID_F" so_ref=%"PRIu64,
		 stob, STOB_ID_P(m0_stob_id_get(stob)), stob->so_ref);
	M0_PRE(M0_IN(m0_stob_state_get(stob), (CSS_EXISTS, CSS_NOENT)));
	M0_PRE(stob->so_ref > 0);

	rc = dom_ops->sdo_stob_cfg_parse(str_cfg, &cfg);
	if (rc == 0) {
		rc = m0_stob_state_get(stob) == CSS_EXISTS ? -EEXIST :
		     dom_ops->sdo_stob_create(stob, dom, dtx,
					      m0_stob_fid_get(stob), cfg);
		dom_ops->sdo_stob_cfg_free(cfg);
		if (rc == 0) {
			m0_mutex_init(&stob->so_ref_mutex);
			m0_chan_init(&stob->so_ref_chan, &stob->so_ref_mutex);
		}
	}
	m0_stob__state_set(stob,
			   rc == 0 ? CSS_EXISTS : m0_stob_state_get(stob));
	if (rc == -ENOENT)
		stob->so_ops->sop_destroy(stob, dtx);

	return M0_RC(rc);
}

M0_INTERNAL void m0_stob_destroy_credit(struct m0_stob *stob,
					struct m0_be_tx_credit *accum)
{
	stob->so_ops->sop_destroy_credit(stob, accum);
}

M0_INTERNAL void m0_stob_delete_mark(struct m0_stob *stob)
{
	M0_PRE(m0_stob_state_get(stob) == CSS_EXISTS);

	m0_stob__state_set(stob, CSS_DELETE);
}

M0_INTERNAL int m0_stob_destroy(struct m0_stob *stob, struct m0_dtx *dtx)
{
	int rc;

	M0_ENTRY("stob=%p so_id="STOB_ID_F" so_ref=%"PRIu64,
		 stob, STOB_ID_P(m0_stob_id_get(stob)), stob->so_ref);
	/*
	 * ioservice ensures stob existence.
	 * @see cob_ops_fom_tick().
	 */
	M0_PRE(M0_IN(m0_stob_state_get(stob), (CSS_EXISTS, CSS_DELETE)));
	M0_ASSERT_INFO(stob->so_ref == 1, "so_ref=%"PRIu64, stob->so_ref);

	rc = stob->so_ops->sop_destroy(stob, dtx);
	if (rc == 0) {
		m0_stob__state_set(stob, CSS_NOENT);
		m0_chan_fini_lock(&stob->so_ref_chan);
		m0_mutex_fini(&stob->so_ref_mutex);
		m0_stob_put(stob);
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_stob_punch_credit(struct m0_stob *stob,
				     struct m0_indexvec *want,
				     struct m0_indexvec *got,
				     struct m0_be_tx_credit *accum)
{
	M0_PRE(m0_stob_state_get(stob) != CSS_UNKNOWN);
	return stob->so_ops->sop_punch_credit(stob, want, got, accum);
}

M0_INTERNAL int m0_stob_punch(struct m0_stob *stob,
			      struct m0_indexvec *range,
			      struct m0_dtx *dtx)
{
	int rc;

	M0_ENTRY("stob=%p so_id="STOB_ID_F" so_ref=%"PRIu64,
		 stob, STOB_ID_P(m0_stob_id_get(stob)), stob->so_ref);
	M0_PRE(M0_IN(m0_stob_state_get(stob), (CSS_EXISTS, CSS_DELETE)));
	rc = stob->so_ops->sop_punch(stob, range, dtx);
	return M0_RC(rc);
}

M0_INTERNAL uint64_t m0_stob_dom_id_get(struct m0_stob *stob)
{
	return m0_stob_id_dom_id_get(m0_stob_id_get(stob));
}

M0_INTERNAL const struct m0_stob_id *m0_stob_id_get(struct m0_stob *stob)
{
	return &stob->so_id;
}

M0_INTERNAL const struct m0_fid *m0_stob_fid_get(struct m0_stob *stob)
{
	return &stob->so_id.si_fid;
}

M0_INTERNAL uint64_t m0_stob_id_dom_id_get(const struct m0_stob_id *stob_id)
{
	return m0_fid_tget(&stob_id->si_domain_fid);
}

M0_INTERNAL enum m0_stob_state m0_stob_state_get(struct m0_stob *stob)
{
	return stob->so_state;
}

M0_INTERNAL uint32_t m0_stob_block_shift(struct m0_stob *stob)
{
	return stob->so_ops->sop_block_shift(stob);
}

M0_INTERNAL void m0_stob_get(struct m0_stob *stob)
{
	struct m0_stob_cache *cache;

	cache = m0_stob_domain__cache(m0_stob_dom_get(stob));

	m0_stob_cache_lock(cache);
	M0_ENTRY("stob=%p so_id="STOB_ID_F" so_ref=%"PRIu64,
		 stob, STOB_ID_P(m0_stob_id_get(stob)), stob->so_ref);
	M0_ASSERT(stob->so_ref > 0);
	M0_CNT_INC(stob->so_ref);
	M0_LEAVE("stob=%p so_id="STOB_ID_F" so_ref=%"PRIu64,
		 stob, STOB_ID_P(m0_stob_id_get(stob)), stob->so_ref);
	m0_stob_cache_unlock(cache);
}

M0_INTERNAL void m0_stob_put(struct m0_stob *stob)
{
	struct m0_stob_cache *cache;

	cache = m0_stob_domain__cache(m0_stob_dom_get(stob));

	m0_stob_cache_lock(cache);
	M0_ENTRY("stob=%p so_id="STOB_ID_F" so_ref=%"PRIu64,
		 stob, STOB_ID_P(m0_stob_id_get(stob)), stob->so_ref);
	M0_CNT_DEC(stob->so_ref);
	if (stob->so_ref == 0)
		m0_stob_cache_idle(cache, stob);
	m0_stob_cache_unlock(cache);

	M0_LOG(M0_DEBUG, "stob %p, fid="FID_F" so_ref %"PRIu64", released ref, "
	       "chan_waiters %"PRIu32, stob, FID_P(&stob->so_id.si_fid),
	       stob->so_ref, stob->so_ref_chan.ch_waiters);
	if (m0_chan_has_waiters(&stob->so_ref_chan)) {
		M0_ASSERT(stob->so_ref >= 1);
		if (stob->so_ref == 1)
			m0_chan_signal_lock(&stob->so_ref_chan);
	}
}

M0_INTERNAL void m0_stob__id_set(struct m0_stob *stob,
				 const struct m0_fid *stob_fid)
{
	M0_LOG(M0_DEBUG, FID_F, FID_P(stob_fid));
	stob->so_id.si_domain_fid = stob->so_domain->sd_id;
	stob->so_id.si_fid = *stob_fid;
}

M0_INTERNAL void m0_stob__cache_evict(struct m0_stob *stob)
{
	struct m0_stob_domain *dom = m0_stob_dom_get(stob);

	if (m0_stob_state_get(stob) != CSS_UNKNOWN)
		stob->so_ops->sop_fini(stob);
	dom->sd_ops->sdo_stob_free(dom, stob);
}

M0_INTERNAL void m0_stob__state_set(struct m0_stob *stob,
				    enum m0_stob_state state)
{
	stob->so_state = state;
}

M0_INTERNAL struct m0_stob_domain *m0_stob_dom_get(struct m0_stob *stob)
{
	return stob->so_domain;
}

M0_INTERNAL void m0_stob_id_make(uint64_t container,
				 uint64_t key,
				 const struct m0_fid *dom_id,
				 struct m0_stob_id *stob_id)
{
	stob_id->si_domain_fid = *dom_id;
	m0_fid_tset(&stob_id->si_fid, m0_fid_tget(dom_id), container, key);
}

M0_INTERNAL bool m0_stob_id_eq(const struct m0_stob_id *stob_id0,
                               const struct m0_stob_id *stob_id1)
{
	return m0_fid_eq(&stob_id0->si_domain_fid, &stob_id1->si_domain_fid) &&
	       m0_fid_eq(&stob_id0->si_fid,        &stob_id1->si_fid);

}

M0_INTERNAL int m0_stob_fd(struct m0_stob *stob)
{
	M0_PRE(stob->so_ops != NULL && stob->so_ops->sop_fd != NULL);

	return stob->so_ops->sop_fd(stob);
}

M0_INTERNAL int m0_stob_mod_init(void)
{
	m0_xc_stob_stob_init();
	return 0;
}

M0_INTERNAL void m0_stob_mod_fini(void)
{
	m0_xc_stob_stob_fini();
}

/** @} end group stob */
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
