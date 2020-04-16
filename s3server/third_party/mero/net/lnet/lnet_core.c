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
 * Original author: Carl Braganza <Carl_Braganza@xyratex.com>
 *                  Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 11/16/2011
 */

#ifndef __KERNEL__
#include <stdio.h>      /* snprintf */
#endif /* __KERNEL__ */
#include "lib/misc.h"   /* m0_strtou32 */
#include "mero/magic.h"

/**
   @addtogroup LNetCore
   @{
 */

#ifdef NLX_DEBUG
static void nlx_print_core_ep_addr(const char *pre,
				   const struct nlx_core_ep_addr *cepa)
{
	NLXP("%s: %p nlx_core_ep_addr\n", pre, cepa);
	NLXP("\t    nid = %ld\n", (unsigned long) cepa->cepa_nid);
	NLXP("\t    pid = %d\n",  (unsigned) cepa->cepa_pid);
	NLXP("\t portal = %d\n", (unsigned) cepa->cepa_portal);
	NLXP("\t   tmid = %d\n", (unsigned) cepa->cepa_tmid);
}

static void
nlx_print_core_buffer_event(const char *pre,
			    const struct nlx_core_buffer_event *lcbev)
{
	NLXP("%s: %p nlx_core_buffer_event\n", pre, lcbev);
	NLXP("\tcbe_buffer_id: %lx\n", (unsigned long) lcbev->cbe_buffer_id);
	NLXP("\t     cbe_time: %lx\n", (unsigned long) lcbev->cbe_time);
	NLXP("\t   cbe_status: %d\n", lcbev->cbe_status);
	NLXP("\t cbe_unlinked: %d\n", (int) lcbev->cbe_unlinked);
	NLXP("\t   cbe_length: %ld\n", (unsigned long) lcbev->cbe_length);
	NLXP("\t   cbe_offset: %ld\n", (unsigned long) lcbev->cbe_offset);
	NLXP("\t   cbe_sender: %ld %d %d %d\n",
	     (unsigned long) lcbev->cbe_sender.cepa_nid,
	     (unsigned) lcbev->cbe_sender.cepa_pid,
	     (unsigned) lcbev->cbe_sender.cepa_portal,
	     (unsigned) lcbev->cbe_sender.cepa_tmid);
}

static void
nlx_print_net_buffer_event(const char *pre,
			   const struct m0_net_buffer_event *nbev)
{
	NLXP("%s: %p m0_net_buffer_event\n", (char*) pre, nbev);
	NLXP("\t  nbe_time: %lx\n", (unsigned long) nbev->nbe_time);
	NLXP("\tnbe_status: %d\n", nbev->nbe_status);
	NLXP("\tnbe_length: %ld\n", (unsigned long) nbev->nbe_length);
	NLXP("\tnbe_offset: %ld\n", (unsigned long) nbev->nbe_offset);
	if (nbev->nbe_ep != NULL)
		NLXP("\t    nbe_ep: %s\n", (char*) nbev->nbe_ep->nep_addr);
	else
		NLXP("\t    nbe_ep: %s\n", (char*) "NULL");
	NLXP("\tnbe_buffer: %p\n", nbev->nbe_buffer);
	if (nbev->nbe_buffer != NULL) {
		struct m0_net_buffer *nb = nbev->nbe_buffer;
		NLXP("\t\t  nb_qtype: %d\n", nb->nb_qtype);
		NLXP("\t\t  nb_flags: %lx\n", (unsigned long) nb->nb_flags);
	}
}

static void nlx_print_core_buffer(const char *pre,
				  const struct nlx_core_buffer *lcb)
{
	NLXP("%s: %p nlx_core_buffer\n", pre, lcb);
	NLXP("\t            magic: %lx\n", (unsigned long) lcb->cb_magic);
	NLXP("\t        buffer_id: %p\n", (void *) lcb->cb_buffer_id);
	NLXP("\t            qtype: %u\n", (unsigned) lcb->cb_qtype);
	NLXP("\t           length: %lu\n", (unsigned long) lcb->cb_length);
	NLXP("\t min_receive_size: %lu\n",
	     (unsigned long) lcb->cb_min_receive_size);
	NLXP("\t   max_operations: %u\n", (unsigned) lcb->cb_max_operations);
	NLXP("\t       match_bits: %lx\n", (unsigned long) lcb->cb_match_bits);
        nlx_print_core_ep_addr("\t          cb_addr", &lcb->cb_addr);
}

static inline uint64_t nlx_core_buf_desc_checksum(const struct nlx_core_buf_desc
						  *cbd);

static void nlx_print_core_buf_desc(const char *pre,
				    const struct nlx_core_buf_desc *cbd)
{
	NLXP("%s: %p nlx_core_buf_desc\n", pre, cbd);
	NLXP("\t match_bits: %lx\n", (unsigned long) cbd->cbd_match_bits);
	NLXP("\t      qtype: %u\n", (unsigned) cbd->cbd_qtype);
	NLXP("\t       size: %ld\n", (unsigned long) cbd->cbd_size);
	NLXP("\t   checksum: %lx\n", (unsigned long) cbd->cbd_checksum);
	nlx_print_core_ep_addr("\t passive_ep", &cbd->cbd_passive_ep);
	NLXP("\t <checksum>: %lx\n", (unsigned long)
	     nlx_core_buf_desc_checksum(cbd));
}
#endif

/**
   Core TM invariant.
   @note Shouldn't require the mutex as it is called from nlx_kcore_eq_cb.
 */
static bool nlx_core_tm_invariant(const struct nlx_core_transfer_mc *lctm)
{
	return lctm != NULL && lctm->ctm_magic == M0_NET_LNET_CORE_TM_MAGIC &&
	    lctm->ctm_mb_counter >= M0_NET_LNET_BUFFER_ID_MIN &&
	    lctm->ctm_mb_counter <= M0_NET_LNET_BUFFER_ID_MAX;
}

/**
   Test that the network TM is locked.  Consumer address space only.
   The subroutine takes advantage of the fact that the core data structure
   is known to be embedded in the xo data structure, which keeps a pointer to
   the network TM structure.
   @param lctm LNet core TM pointer.
 */
static bool nlx_core_tm_is_locked(const struct nlx_core_transfer_mc *lctm)
{
	const struct nlx_xo_transfer_mc *xtm;

	if (!nlx_core_tm_invariant(lctm))
		return false;
	xtm = container_of(lctm, struct nlx_xo_transfer_mc, xtm_core);
	return nlx_tm_invariant(xtm->xtm_tm) &&
	    m0_mutex_is_locked(&xtm->xtm_tm->ntm_mutex);
}

/**
   Core buffer invariant.
   @note Shouldn't require the mutex as it is called from nlx_kcore_eq_cb.
 */
static bool nlx_core_buffer_invariant(const struct nlx_core_buffer *lcb)
{
	return lcb != NULL && lcb->cb_magic == M0_NET_LNET_CORE_BUF_MAGIC &&
		lcb->cb_buffer_id != 0;
}

static uint32_t nlx_core_kmem_loc_checksum(const struct nlx_core_kmem_loc *loc)
{
	int i;
	uint32_t ret;

	for (i = 0, ret = 0; i < ARRAY_SIZE(loc->kl_data); ++i)
		ret ^= loc->kl_data[i];
	return ret;
}

static void nlx_core_bev_free_cb(struct nlx_core_bev_link *ql)
{
	struct nlx_core_buffer_event *bev;
	if (ql != NULL) {
		bev = container_of(ql, struct nlx_core_buffer_event,
				   cbe_tm_link);
		NLX_FREE_ALIGNED_PTR(bev);
	}
}

M0_INTERNAL int nlx_core_bevq_provision(struct nlx_core_domain *lcdom,
					struct nlx_core_transfer_mc *lctm,
					size_t need)
{
	size_t have;
	int num_to_alloc;
	int rc = 0;

	M0_PRE(nlx_core_tm_is_locked(lctm));
	M0_PRE(need > 0);

	have = bev_cqueue_size(&lctm->ctm_bevq) - M0_NET_LNET_BEVQ_NUM_RESERVED;
	M0_ASSERT(have >= lctm->ctm_bev_needed);
	num_to_alloc = lctm->ctm_bev_needed + need - have;
	while (num_to_alloc > 0) {
		struct nlx_core_buffer_event *bev;
		rc = nlx_core_new_blessed_bev(lcdom, lctm, &bev);/* {uk} vary */
		if (rc != 0)
			break;
		bev_cqueue_add(&lctm->ctm_bevq, &bev->cbe_tm_link);
		--num_to_alloc;
	}
	if (rc == 0)
		lctm->ctm_bev_needed += need;
	have = bev_cqueue_size(&lctm->ctm_bevq) - M0_NET_LNET_BEVQ_NUM_RESERVED;
	M0_POST(have >= lctm->ctm_bev_needed);
	return M0_RC(rc);
}

M0_INTERNAL void nlx_core_bevq_release(struct nlx_core_transfer_mc *lctm,
				       size_t release)
{
	M0_PRE(nlx_core_tm_is_locked(lctm));
	M0_PRE(release > 0);
	M0_PRE(lctm->ctm_bev_needed >= release);

	lctm->ctm_bev_needed -= release;
	return;
}

M0_INTERNAL bool nlx_core_buf_event_get(struct nlx_core_transfer_mc *lctm,
					struct nlx_core_buffer_event *lcbe)
{
	struct nlx_core_bev_link *link;
	struct nlx_core_buffer_event *bev;

	M0_PRE(lcbe != NULL);
	M0_PRE(nlx_core_tm_is_locked(lctm));

	link = bev_cqueue_get(&lctm->ctm_bevq);
	if (link != NULL) {
		bev = container_of(link, struct nlx_core_buffer_event,
				   cbe_tm_link);
		*lcbe = *bev;
		M0_SET0(&lcbe->cbe_tm_link); /* copy is not in queue */
		/* Event structures released when network buffer unlinked */
		return true;
	}
	return false;
}

/**
   Helper subroutine to construct the match bit value from its components.
   @param tmid Transfer machine identifier.
   @param counter Buffer counter value.  The value of 0 is reserved for
   the TM receive message queue.
   @see nlx_core_match_bits_decode()
 */
static uint64_t nlx_core_match_bits_encode(uint32_t tmid, uint64_t counter)
{
	uint64_t mb;
	mb = ((uint64_t) tmid << M0_NET_LNET_TMID_SHIFT) |
		(counter & M0_NET_LNET_BUFFER_ID_MASK);
	return mb;
}

/**
   Helper subroutine to decode the match bits into its components.
   @param mb Match bit field.
   @param tmid Pointer to returned Transfer Machine id.
   @param counter Pointer to returned buffer counter value.
   @see nlx_core_match_bits_encode()
 */
static inline void nlx_core_match_bits_decode(uint64_t mb,
					      uint32_t *tmid,
					      uint64_t *counter)
{
	*tmid = (uint32_t) (mb >> M0_NET_LNET_TMID_SHIFT);
	*counter = mb & M0_NET_LNET_BUFFER_ID_MASK;
	return;
}

#define CBD_EP(f) cbd->cbd_passive_ep.cepa_ ## f
#define TM_EP(f) lctm->ctm_addr.cepa_ ## f
#define B_EP(f) lcbuf->cb_addr.cepa_ ## f

/**
   Compute the checksum of the network buffer descriptor payload.
   The computation relies on the fact that the cbd_data part of the union
   in the network buffer descriptor covers the payload.
   @retval checksum In little-endian order.
 */
static inline uint64_t nlx_core_buf_desc_checksum(const struct nlx_core_buf_desc
						  *cbd)
{
	int i;
	uint64_t checksum;

	/* ensure that the checksum computation covers all the payload */
	M0_CASSERT(sizeof *cbd ==
		   sizeof cbd->cbd_data + sizeof cbd->cbd_checksum);

	for (i = 0, checksum = 0; i < ARRAY_SIZE(cbd->cbd_data); ++i)
		checksum ^= cbd->cbd_data[i];
	return __cpu_to_le64(checksum);
}

M0_INTERNAL void nlx_core_buf_desc_encode(struct nlx_core_transfer_mc *lctm,
					  struct nlx_core_buffer *lcbuf,
					  struct nlx_core_buf_desc *cbd)
{
	M0_PRE(nlx_core_tm_is_locked(lctm));
	M0_PRE(nlx_core_tm_invariant(lctm));
	M0_PRE(nlx_core_buffer_invariant(lcbuf));
	M0_PRE(lcbuf->cb_qtype == M0_NET_QT_PASSIVE_BULK_SEND ||
	       lcbuf->cb_qtype == M0_NET_QT_PASSIVE_BULK_RECV);

	/* generate match bits */
	lcbuf->cb_match_bits =
		nlx_core_match_bits_encode(lctm->ctm_addr.cepa_tmid,
					   lctm->ctm_mb_counter);
	if (++lctm->ctm_mb_counter > M0_NET_LNET_BUFFER_ID_MAX)
		lctm->ctm_mb_counter = M0_NET_LNET_BUFFER_ID_MIN;

	/* create the descriptor */
	M0_SET_ARR0(cbd->cbd_data);
	cbd->cbd_match_bits = __cpu_to_le64(lcbuf->cb_match_bits);

	CBD_EP(nid)         = __cpu_to_le64(TM_EP(nid));
	CBD_EP(pid)         = __cpu_to_le32(TM_EP(pid));
	CBD_EP(portal)      = __cpu_to_le32(TM_EP(portal));
	CBD_EP(tmid)        = __cpu_to_le32(TM_EP(tmid));

	cbd->cbd_qtype      = __cpu_to_le32(lcbuf->cb_qtype);
	cbd->cbd_size       = __cpu_to_le64(lcbuf->cb_length);

	cbd->cbd_checksum   = nlx_core_buf_desc_checksum(cbd);

	NLXDBG(lctm, 2, nlx_print_core_buf_desc("encode", cbd));

	M0_POST(nlx_core_tm_invariant(lctm));
	M0_POST(nlx_core_buffer_invariant(lcbuf));
	return;
}

M0_INTERNAL int nlx_core_buf_desc_decode(struct nlx_core_transfer_mc *lctm,
					 struct nlx_core_buffer *lcbuf,
					 struct nlx_core_buf_desc *cbd)
{
	uint64_t i64;
	uint32_t i32;

	NLXDBG(lctm, 2, nlx_print_core_buf_desc("decode", cbd));

	M0_PRE(nlx_core_tm_is_locked(lctm));
	M0_PRE(nlx_core_tm_invariant(lctm));
	M0_PRE(nlx_core_buffer_invariant(lcbuf));
	M0_PRE(lcbuf->cb_qtype == M0_NET_QT_ACTIVE_BULK_SEND ||
	       lcbuf->cb_qtype == M0_NET_QT_ACTIVE_BULK_RECV);

	i64 = nlx_core_buf_desc_checksum(cbd);
	if (i64 != cbd->cbd_checksum)
		return M0_ERR(-EINVAL);

	i64 = __le64_to_cpu(cbd->cbd_size);

	i32 = __le32_to_cpu(cbd->cbd_qtype);
	if (i32 == M0_NET_QT_PASSIVE_BULK_SEND) {
		if (lcbuf->cb_qtype != M0_NET_QT_ACTIVE_BULK_RECV)
			return M0_ERR(-EPERM);
		if (i64 > lcbuf->cb_length)
			return M0_ERR(-EFBIG);
		lcbuf->cb_length = i64; /* passive send size used */
	} else if (i32 == M0_NET_QT_PASSIVE_BULK_RECV) {
		if (lcbuf->cb_qtype != M0_NET_QT_ACTIVE_BULK_SEND)
			return M0_ERR(-EPERM);
		if (lcbuf->cb_length > i64)
			return M0_ERR(-EFBIG);
	        /* active send size used */
	} else
		return M0_ERR(-EINVAL);

	B_EP(nid)    = __le64_to_cpu(CBD_EP(nid));
	B_EP(pid)    = __le32_to_cpu(CBD_EP(pid));
	B_EP(portal) = __le32_to_cpu(CBD_EP(portal));
	B_EP(tmid)   = __le32_to_cpu(CBD_EP(tmid));

	lcbuf->cb_match_bits = __le64_to_cpu(cbd->cbd_match_bits);
	nlx_core_match_bits_decode(lcbuf->cb_match_bits, &i32, &i64);
	if (i64 < M0_NET_LNET_BUFFER_ID_MIN ||
	    i64 > M0_NET_LNET_BUFFER_ID_MAX ||
	    i32 != B_EP(tmid))
		return M0_ERR(-EINVAL);

	return 0;
}

#undef B_EP
#undef TM_EP
#undef CBD_EP

int nlx_core_ep_addr_decode(struct nlx_core_domain *lcdom,
			    const char *ep_addr,
			    struct nlx_core_ep_addr *cepa)
{
	char nidstr[M0_NET_LNET_NIDSTR_SIZE];
	char *cp = strchr(ep_addr, ':');
	char *endp;
	size_t n = cp - ep_addr;
	int rc;

	M0_ENTRY("ep_addr=%s", ep_addr);

	if (cp == NULL || n == 0 || n >= sizeof nidstr)
		return M0_ERR(-EINVAL);
	strncpy(nidstr, ep_addr, n);
	nidstr[n] = 0;
	rc = nlx_core_nidstr_decode(lcdom, nidstr, &cepa->cepa_nid);
	if (rc != 0)
		return M0_RC(rc);
	++cp;
	cepa->cepa_pid = m0_strtou32(cp, &endp, 10);
	if (*endp != ':')
		return M0_ERR(-EINVAL);
	cp = endp + 1;
	cepa->cepa_portal = m0_strtou32(cp, &endp, 10);
	if (*endp != ':')
		return M0_ERR(-EINVAL);
	cp = endp + 1;
	if (strcmp(cp, "*") == 0) {
		cepa->cepa_tmid = M0_NET_LNET_TMID_INVALID;
	} else {
		cepa->cepa_tmid = m0_strtou32(cp, &endp, 10);
		if (*endp != 0 || cepa->cepa_tmid > M0_NET_LNET_TMID_MAX ||
		    cepa->cepa_tmid == 0)
			return M0_ERR(-EINVAL);
	}
	return 0;
}

void nlx_core_ep_addr_encode(struct nlx_core_domain *lcdom,
			     const struct nlx_core_ep_addr *cepa,
			     char buf[M0_NET_LNET_XEP_ADDR_LEN])
{
	const char *fmt;
	int rc;
	int n;

	rc = nlx_core_nidstr_encode(lcdom, cepa->cepa_nid, buf);
	M0_ASSERT(rc == 0);
	n = strlen(buf);

	if (cepa->cepa_tmid != M0_NET_LNET_TMID_INVALID)
		fmt = ":%u:%u:%u";
	else
		fmt = ":%u:%u:*";
	snprintf(buf + n, M0_NET_LNET_XEP_ADDR_LEN - n, fmt,
		 cepa->cepa_pid, cepa->cepa_portal, cepa->cepa_tmid);
}

M0_INTERNAL void nlx_core_dom_set_debug(struct nlx_core_domain *lcdom,
					unsigned dbg)
{
	lcdom->_debug_ = dbg;
}

M0_INTERNAL void nlx_core_tm_set_debug(struct nlx_core_transfer_mc *lctm,
				       unsigned dbg)
{
	lctm->_debug_ = dbg;
}

/** @} */ /* LNetCore */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
