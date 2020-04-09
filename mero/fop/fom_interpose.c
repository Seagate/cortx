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
 * Original author: Sergey Shilov <sergey.shilov@seagate.com>,
 *                  Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 10/17/2016
 */

#include "lib/assert.h"
#include "lib/misc.h"
#include "lib/buf.h"
#include "fop/fop.h"

#include "fop/fom_interpose.h"

enum { INTERPOSE_CONT = M0_FSO_NR + 1 };

static int thrall_finish(struct m0_fom *fom, struct m0_fom_interpose *proxy,
			 int result);

static const struct m0_fom_interpose_ops thrall_ops = {
	.io_post = {
		[0 ... ARRAY_SIZE(thrall_ops.io_post) - 1] = &thrall_finish
	}
};

static int interpose_tick(struct m0_fom *fom)
{
	struct m0_fom_interpose *proxy = M0_AMB(proxy, fom->fo_ops, fi_shim);
	int                      phase = m0_fom_phase(fom);
	int                      result;

	M0_PRE(IS_IN_ARRAY(phase, proxy->fi_ops->io_pre));
	M0_PRE(IS_IN_ARRAY(phase, proxy->fi_ops->io_post));
	if (proxy->fi_ops->io_pre[phase] != NULL) {
		result = (proxy->fi_ops->io_pre[phase])(fom, proxy);
		if (result != INTERPOSE_CONT)
			return result;
	}
	/*
	 * Perhaps restore original fom->fo_ops around this call?
	 */
	result = proxy->fi_orig->fo_tick(fom);
	M0_ASSERT(result != INTERPOSE_CONT);
	if (proxy->fi_ops->io_post[phase] != NULL)
		result = (proxy->fi_ops->io_post[phase])(fom, proxy, result);
	M0_POST(M0_IN(result, (M0_FSO_WAIT, M0_FSO_AGAIN)));
	return result;
}

static int thrall_finish(struct m0_fom *fom, struct m0_fom_interpose *proxy,
			 int result)
{
	struct m0_fom_thralldom *thrall = M0_AMB(thrall, proxy, ft_fief);
	int                      phase  = m0_fom_phase(fom);

	if (phase == M0_FOM_PHASE_FINISH) {
		/* Mors liberat. */
		m0_fom_interpose_leave(fom, proxy);
		if (thrall->ft_end != NULL)
			thrall->ft_end(thrall, fom);
		m0_fom_wakeup(thrall->ft_master);
	}
	return result;
}

M0_INTERNAL void m0_fom_interpose_enter(struct m0_fom           *fom,
					struct m0_fom_interpose *proxy)
{
	M0_PRE(proxy->fi_ops != NULL);
	/*
	 * Activate the interposition. Substitute shim fom operation vector for
	 * the original one after saving the original in proxy->fi_orig.
	 */
	proxy->fi_orig =  fom->fo_ops;
	proxy->fi_shim = *fom->fo_ops;
	proxy->fi_shim.fo_tick = &interpose_tick;
	fom->fo_ops = &proxy->fi_shim;
}

M0_INTERNAL void m0_fom_interpose_leave(struct m0_fom           *fom,
					struct m0_fom_interpose *proxy)
{
	M0_PRE(fom->fo_ops == &proxy->fi_shim);
	fom->fo_ops = proxy->fi_orig;
}

M0_INTERNAL void m0_fom_enthrall(struct m0_fom *master, struct m0_fom *serf,
				 struct m0_fom_thralldom *thrall,
				 void (*end)(struct m0_fom_thralldom *thrall,
					     struct m0_fom           *serf))
{
	thrall->ft_master      = master;
	thrall->ft_fief.fi_ops = &thrall_ops;
	thrall->ft_end         = end;
	m0_fom_interpose_enter(serf, &thrall->ft_fief);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
