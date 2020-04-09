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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 06/07/2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CM
#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/arith.h"           /* m0_rnd() */

#include "rpc/rpc.h"

#include "fop/fop.h"
#include "fop/fop_item_type.h"
#include "fop/fom.h"

#include "cm/proxy.h"
#include "cm/cm.h"
#include "cm/repreb/sw_onwire_fop.h"
#include "cm/repreb/sw_onwire_fom.h"

/**
   @addtogroup XXX

   @{
 */

static struct m0_sm_state_descr repreb_sw_fom_phases[] = {
	[SWOPH_START] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "Start",
		.sd_allowed   = M0_BITS(SWOPH_FINI)
	},
	[SWOPH_FINI] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "Fini",
		.sd_allowed   = 0
	},
};

const struct m0_sm_conf m0_cm_repreb_sw_onwire_conf = {
	.scf_name      = "Repair/re-balance sw update",
	.scf_nr_states = ARRAY_SIZE(repreb_sw_fom_phases),
	.scf_state     = repreb_sw_fom_phases
};

static int repreb_sw_fom_tick(struct m0_fom *fom)
{
	struct m0_reqh_service     *service;
	struct m0_cm               *cm;
	struct m0_cm_sw_onwire     *swo_fop;
	struct m0_cm_sw_onwire_rep *swo_rep;
	struct m0_cm_proxy         *cm_proxy;
	struct m0_fop              *rfop;
	int                         rc = 0;
	const char                 *ep;

	service = fom->fo_service;
	switch (m0_fom_phase(fom)) {
	case SWOPH_START:
		swo_fop = m0_fop_data(fom->fo_fop);
		cm = m0_cmsvc2cm(service);
		if (cm == NULL)
			return M0_ERR(-EINVAL);
		M0_LOG(M0_DEBUG, "Rcvd from %s hi: [%lu] [%lu] [%lu] [%lu] "
				 "[%lu] [%lu] [%lu]",
		       swo_fop->swo_cm_ep.ep,
		       swo_fop->swo_in_interval.sw_hi.ai_hi.u_hi,
		       swo_fop->swo_in_interval.sw_hi.ai_hi.u_lo,
		       swo_fop->swo_in_interval.sw_hi.ai_lo.u_hi,
		       swo_fop->swo_in_interval.sw_hi.ai_lo.u_lo,
		       cm->cm_aggr_grps_in_nr,
		       cm->cm_aggr_grps_out_nr,
		       cm->cm_proxy_nr);

		ep = swo_fop->swo_cm_ep.ep;
		m0_cm_lock(cm);
		cm_proxy = m0_cm_proxy_locate(cm, ep);
		if (cm_proxy != NULL) {
			ID_LOG("proxy hi", &cm_proxy->px_sw.sw_hi);
			rc = m0_cm_proxy_update(cm_proxy,
						&swo_fop->swo_in_interval,
						&swo_fop->swo_out_interval,
						swo_fop->swo_cm_status,
						swo_fop->swo_cm_epoch);
		} else
			rc = -ENOENT;
		m0_cm_unlock(cm);

		rfop = fom->fo_rep_fop;
		swo_rep = m0_fop_data(rfop);
		swo_rep->swr_rc = rc;
		m0_rpc_reply_post(&fom->fo_fop->f_item, &rfop->f_item);
		m0_fom_phase_set(fom, SWOPH_FINI);
		break;
	default:
		M0_IMPOSSIBLE("Invalid fop");
		return M0_ERR(-EINVAL);
	}
	return M0_FSO_WAIT;
}

static void repreb_sw_fom_fini(struct m0_fom *fom)
{
	M0_PRE(fom != NULL);

	m0_fom_fini(fom);
	m0_free(fom);
}

static size_t repreb_sw_fom_home_locality(const struct m0_fom *fom)
{
	struct m0_cm_sw_onwire *swo_fop;

	swo_fop = m0_fop_data(fom->fo_fop);
	return m0_rnd(1 << 30, &swo_fop->swo_sender_id);
}

static const struct m0_fom_ops repreb_sw_fom_ops = {
	.fo_fini          = repreb_sw_fom_fini,
	.fo_tick          = repreb_sw_fom_tick,
	.fo_home_locality = repreb_sw_fom_home_locality,
};

M0_INTERNAL int m0_cm_repreb_sw_onwire_fom_create(struct m0_fop *fop,
						  struct m0_fop *r_fop,
						  struct m0_fom **out,
						  struct m0_reqh *reqh)
{
	struct m0_fom *fom;

	M0_PRE(fop != NULL);
	M0_PRE(out != NULL);

	M0_ALLOC_PTR(fom);
	if (fom == NULL)
		return M0_ERR(-ENOMEM);

	m0_fom_init(fom, &fop->f_type->ft_fom_type, &repreb_sw_fom_ops, fop,
		    r_fop, reqh);

	*out = fom;
	return 0;
}

#undef M0_TRACE_SUBSYSTEM

/** @} XXX */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
