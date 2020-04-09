/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Mikhail Antropov <mikhail.v.antropov@xyratex.com>
 * Original creation date: 13-Feb-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF

#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/finject.h"   /* M0_FI_ENABLED */
#include "lib/memory.h"
#include "conf/load_fop.h"
#include "net/net.h"       /* m0_net_domain_get_max_buffer_segment_size */

/* tlists and tlist APIs referred from rpc layer. */
M0_TL_DESCR_DECLARE(rpcbulk, M0_EXTERN);

M0_INTERNAL bool m0_is_conf_load_fop(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_conf_load_fopt;
}

M0_INTERNAL bool m0_is_conf_load_fop_rep(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_conf_load_rep_fopt;
}

M0_INTERNAL struct m0_fop_conf_load *m0_conf_fop_to_load_fop(
						const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(m0_is_conf_load_fop(fop));

	return m0_fop_data(fop);
}

M0_INTERNAL struct m0_fop_conf_load_rep *m0_conf_fop_to_load_fop_rep(
						const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(m0_is_conf_load_fop_rep(fop));

	return m0_fop_data(fop);
}

M0_INTERNAL m0_bcount_t m0_conf_segment_size(struct m0_fop *fop)
{
	struct m0_net_domain *dom;

	if (M0_FI_ENABLED("const_size"))
		return 4096;
	dom = m0_fop_domain_get(fop);
	return m0_net_domain_get_max_buffer_segment_size(dom);
}

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
