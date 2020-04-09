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
 * Original creation date: 16-Mar-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF

#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "conf/flip_fop.h"

M0_INTERNAL bool m0_is_conf_flip_fop(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_conf_flip_fopt;
}

M0_INTERNAL bool m0_is_conf_flip_fop_rep(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_conf_flip_rep_fopt;
}

M0_INTERNAL struct m0_fop_conf_flip *m0_conf_fop_to_flip_fop(
						const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(m0_is_conf_flip_fop(fop));

	return m0_fop_data(fop);
}

M0_INTERNAL struct m0_fop_conf_flip_rep *m0_conf_fop_to_flip_fop_rep(
						const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_PRE(m0_is_conf_flip_fop_rep(fop));

	return m0_fop_data(fop);
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
