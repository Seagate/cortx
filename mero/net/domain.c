/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 05/17/2010
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET
#include "lib/trace.h"

#include "lib/assert.h"
#include "net/net_internal.h"

/**
 @addtogroup net
 @{
*/

static void net_domain_fini(struct m0_net_domain *dom);

int m0_net_domain_init(struct m0_net_domain *dom, struct m0_net_xprt *xprt)
{
	int rc;

	M0_ENTRY();
	m0_mutex_lock(&m0_net_mutex);
	M0_PRE(dom->nd_xprt == NULL);

	m0_mutex_init(&dom->nd_mutex);
	m0_list_init(&dom->nd_registered_bufs);
	m0_list_init(&dom->nd_tms);

	dom->nd_xprt_private = NULL;
	dom->nd_xprt = xprt;
	rc = xprt->nx_ops->xo_dom_init(xprt, dom);
	if (rc != 0) {
		dom->nd_xprt = NULL; /* prevent call to xo_dom_fini */
		net_domain_fini(dom);
		m0_mutex_unlock(&m0_net_mutex);
		return M0_RC(rc);
	}
	dom->nd_get_max_buffer_segment_size =
			xprt->nx_ops->xo_get_max_buffer_segment_size(dom);
	dom->nd_get_max_buffer_segments =
			xprt->nx_ops->xo_get_max_buffer_segments(dom);
	dom->nd_get_max_buffer_size =
			xprt->nx_ops->xo_get_max_buffer_size(dom);
	dom->nd_get_max_buffer_desc_size =
			xprt->nx_ops->xo_get_max_buffer_desc_size(dom);
	m0_mutex_unlock(&m0_net_mutex);
	return M0_RC(rc);
}
M0_EXPORTED(m0_net_domain_init);

void m0_net_domain_fini(struct m0_net_domain *dom)
{
	M0_ENTRY();
	m0_mutex_lock(&m0_net_mutex);
	net_domain_fini(dom);
	m0_mutex_unlock(&m0_net_mutex);
	M0_LEAVE();
}
M0_EXPORTED(m0_net_domain_fini);

static void net_domain_fini(struct m0_net_domain *dom)
{
	M0_PRE(m0_mutex_is_locked(&m0_net_mutex));
	M0_PRE(m0_list_is_empty(&dom->nd_tms));
	M0_PRE(m0_list_is_empty(&dom->nd_registered_bufs));

	if (dom->nd_xprt != NULL) {
		dom->nd_xprt->nx_ops->xo_dom_fini(dom);
		dom->nd_xprt = NULL;
	}
	dom->nd_xprt_private = NULL;

	m0_list_fini(&dom->nd_tms);
	m0_list_fini(&dom->nd_registered_bufs);
	m0_mutex_fini(&dom->nd_mutex);
}

#define DOM_GET_PARAM(Fn, Type)				\
Type m0_net_domain_get_##Fn(struct m0_net_domain *dom)	\
{							\
	Type rc;					\
	M0_PRE(dom != NULL);				\
	M0_PRE(dom->nd_xprt != NULL);			\
	rc = dom->nd_get_##Fn;                          \
	return rc;                                      \
}

DOM_GET_PARAM(max_buffer_size, m0_bcount_t);
DOM_GET_PARAM(max_buffer_segment_size, m0_bcount_t);
DOM_GET_PARAM(max_buffer_segments, int32_t);
DOM_GET_PARAM(max_buffer_desc_size, m0_bcount_t);

/** @} end of net group */
#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
