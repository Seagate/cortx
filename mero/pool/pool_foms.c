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
 * Original author: Huang Hua <Hua_Huang@xyratex.com>
 * Original creation date: 06/19/2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_POOL
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/assert.h"
#include "lib/misc.h"    /* M0_BITS */
#include "fop/fop.h"
#include "fop/fom_generic.h"
#include "reqh/reqh_service.h"
#include "pool/pool.h"
#include "pool/pool_foms.h"
#include "pool/pool_fops.h"
#include "rpc/rpc_opcodes.h"
#include "mero/setup.h"
#include "conf/diter.h"
#include "conf/helpers.h"     /* m0_conf_drive_get */

static const struct m0_fom_ops poolmach_ops;

static int poolmach_fom_create(struct m0_fop *fop, struct m0_fom **out,
				  struct m0_reqh *reqh)
{
	struct m0_fop *rep_fop;
	struct m0_fom *fom;
	int            rc = 0;

	M0_PRE(fop != NULL);
	M0_PRE(out != NULL);

	M0_ALLOC_PTR(fom);
	if (fom == NULL)
		return M0_ERR(-ENOMEM);

	if (m0_fop_opcode(fop) == M0_POOLMACHINE_QUERY_OPCODE) {
		rep_fop = m0_fop_reply_alloc(fop,
					     &m0_fop_poolmach_query_rep_fopt);
		M0_LOG(M0_DEBUG, "create Query fop");
	} else if (m0_fop_opcode(fop) == M0_POOLMACHINE_SET_OPCODE) {
		rep_fop = m0_fop_reply_alloc(fop,
					     &m0_fop_poolmach_set_rep_fopt);
		M0_LOG(M0_DEBUG, "create set fop");
	} else {
		m0_free(fom);
		return M0_ERR(-EINVAL);
	}
	if (rep_fop == NULL) {
		m0_free(fom);
		return M0_ERR(-ENOMEM);
	}

	m0_fom_init(fom, &fop->f_type->ft_fom_type, &poolmach_ops, fop, rep_fop,
		    reqh);
	*out = fom;

        return M0_RC(rc);
}

static void poolmach_set_op(struct m0_fom *fom)
{
	int                      rc = 0;
	int                      i;
	int                      j;
	struct m0_fop           *req_fop = fom->fo_fop;
	struct m0_fop           *rep_fop = fom->fo_rep_fop;
	struct m0_reqh          *reqh    = m0_fom_reqh(fom);
	struct m0_mero          *mero    = m0_cs_ctx_get(reqh);
	struct m0_fid           *dev_fid;
	struct m0_conf_drive     *drive;
	struct m0_conf_pver    **conf_pver;
	struct m0_pool_version  *pv;
	struct m0_poolmach      *pm;

	struct m0_fop_poolmach_set     *set_fop     = m0_fop_data(req_fop);
	struct m0_fop_poolmach_set_rep *set_fop_rep = m0_fop_data(rep_fop);
	struct m0_poolmach_event        pme = {0};

	for (i = 0; i < set_fop->fps_dev_info.fpi_nr; ++i) {
		pme.pe_type  = set_fop->fps_type;
		pme.pe_state = set_fop->fps_dev_info.fpi_dev[i].fpd_state;
		/*
		 * Update pool-machines per pool-version to which device
		 * is associated.
		 */
		dev_fid = &set_fop->fps_dev_info.fpi_dev[i].fpd_fid;
		rc = m0_conf_drive_get(m0_reqh2confc(reqh), dev_fid, &drive);
		if (rc != 0)
			break;
		conf_pver = drive->ck_pvers;
		for (j = 0; conf_pver[j] != NULL; ++j) {
			pv = m0_pool_version_find(&mero->cc_pools_common,
						  &conf_pver[j]->pv_obj.co_id);
			pm = &pv->pv_mach;
			rc = m0_poolmach_fid_to_idx(pm, dev_fid, &pme.pe_index);
			M0_ASSERT(rc == 0);
			rc = m0_poolmach_state_transit(pm, &pme);
			if (rc != 0)
				break;
		}
		m0_confc_close(&drive->ck_obj);
	}
	set_fop_rep->fps_rc = rc;
}

static void poolmach_query_op(struct m0_fom *fom)
{
	uint32_t                 idx;
	int                      rc = 0;
	int                      i;
	int                      j;
	struct m0_fop           *req_fop = fom->fo_fop;
	struct m0_fop           *rep_fop = fom->fo_rep_fop;
	struct m0_reqh          *reqh    = m0_fom_reqh(fom);
	struct m0_mero          *mero    = m0_cs_ctx_get(reqh);
	struct m0_conf_drive    *drive;
	struct m0_conf_pver    **conf_pver;
	struct m0_pool_version  *pv;
	struct m0_poolmach      *pm;
	struct m0_fid           *dev_fid;

	struct m0_fop_poolmach_query     *query_fop     = m0_fop_data(req_fop);
	struct m0_fop_poolmach_query_rep *query_fop_rep = m0_fop_data(rep_fop);

	M0_ALLOC_ARR(query_fop_rep->fqr_dev_info.fpi_dev,
		     query_fop->fpq_dev_idx.fpx_nr);
	if (query_fop_rep->fqr_dev_info.fpi_dev == NULL) {
		query_fop_rep->fqr_rc = -ENOMEM;
		return;
	}

	query_fop_rep->fqr_dev_info.fpi_nr = query_fop->fpq_dev_idx.fpx_nr;

	for (i = 0; i < query_fop->fpq_dev_idx.fpx_nr; ++i) {
		dev_fid = &query_fop->fpq_dev_idx.fpx_fid[i];
		rc = m0_conf_drive_get(m0_reqh2confc(reqh), dev_fid, &drive);
		if (rc != 0)
			break;
		conf_pver = drive->ck_pvers;
		/*
		 * Search the first pool version for the given drive fid
		 * and return the corresponding state.
		 */
		for (j = 0; conf_pver[j] != NULL; ++j) {
			pv = m0_pool_version_find(&mero->cc_pools_common,
						  &conf_pver[j]->pv_obj.co_id);
			pm = &pv->pv_mach;
			M0_ASSERT(query_fop->fpq_type == M0_POOL_DEVICE);
			rc = m0_poolmach_fid_to_idx(pm, dev_fid, &idx);
			if (rc == -ENOENT)
				continue;
			m0_poolmach_device_state(pm, idx,
			    &query_fop_rep->fqr_dev_info.fpi_dev[i].fpd_state);
			break;
		}
		query_fop_rep->fqr_dev_info.fpi_dev[i].fpd_fid = *dev_fid;
		m0_confc_close(&drive->ck_obj);
	}
	query_fop_rep->fqr_rc = rc;
}

static int poolmach_fom_tick(struct m0_fom *fom)
{
	struct m0_fop *req_fop = fom->fo_fop;

	/* first handle generic phase */
	if (m0_fom_phase(fom) < M0_FOPH_NR)
		return m0_fom_tick_generic(fom);

	switch (m0_fop_opcode(req_fop)) {
	case M0_POOLMACHINE_QUERY_OPCODE: {
		poolmach_query_op(fom);
		break;
	}
	case M0_POOLMACHINE_SET_OPCODE: {
		poolmach_set_op(fom);
		break;
	}
	default:
		M0_IMPOSSIBLE("Invalid opcode");
		m0_fom_phase_move(fom, -EINVAL, M0_FOPH_FAILURE);
		return M0_FSO_AGAIN;
	}

	m0_fom_phase_move(fom, 0, M0_FOPH_SUCCESS);
	return M0_FSO_AGAIN;
}

static size_t poolmach_fom_home_locality(const struct m0_fom *fom)
{
	M0_PRE(fom != NULL);

	return m0_fop_opcode(fom->fo_fop);
}

static void poolmach_fom_fini(struct m0_fom *fom)
{
	m0_fom_fini(fom);
	m0_free(fom);
}

/**
 * I/O FOM operation vector.
 */
static const struct m0_fom_ops poolmach_ops = {
	.fo_fini          = poolmach_fom_fini,
	.fo_tick          = poolmach_fom_tick,
	.fo_home_locality = poolmach_fom_home_locality
};

/**
 * I/O FOM type operation vector.
 */
const struct m0_fom_type_ops poolmach_fom_type_ops = {
	.fto_create = poolmach_fom_create
};

struct m0_sm_state_descr poolmach_phases[] = {
	[M0_FOPH_POOLMACH_FOM_EXEC] = {
		.sd_name    = "Pool Machine query/set",
		.sd_allowed = M0_BITS(M0_FOPH_SUCCESS, M0_FOPH_FAILURE)
	}
};

const struct m0_sm_conf poolmach_conf = {
	.scf_name      = "poolmach",
	.scf_nr_states = ARRAY_SIZE(poolmach_phases),
	.scf_state     = poolmach_phases
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
