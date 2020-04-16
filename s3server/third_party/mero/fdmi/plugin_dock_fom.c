/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */

#define M0_TRACE_SUBSYSTEM    M0_TRACE_SUBSYS_FDMI


#include "lib/trace.h"
#include "mero/magic.h"       /* M0_CONFC_MAGIC, M0_CONFC_CTX_MAGIC */
#include "lib/misc.h"         /* M0_IN */
#include "lib/errno.h"        /* ENOMEM, EPROTO */
#include "lib/memory.h"       /* M0_ALLOC_ARR, m0_free */
#include "rpc/rpc_opcodes.h"  /* M0_FDMI_PLUGIN_DOCK_OPCODE */
#include "fop/fom.h"          /* M0_FOM_PHASE_INIT, etc. */
#include "fop/fop.h"
#include "fdmi/fdmi.h"
#include "fdmi/plugin_dock.h"
#include "fdmi/plugin_dock_internal.h"
#include "fdmi/fops.h"        /* m0_fop_fdmi_record */
#include "fdmi/service.h"

static int pdock_fom_create(struct m0_fop  *fop,
			    struct m0_fom **out,
			    struct m0_reqh *reqh);

enum fdmi_plugin_dock_fom_phase {
	FDMI_PLG_DOCK_FOM_INIT = M0_FOM_PHASE_INIT,
	FDMI_PLG_DOCK_FOM_FINI = M0_FOM_PHASE_FINISH,
	FDMI_PLG_DOCK_FOM_FEED_PLUGINS_WITH_REC,
	FDMI_PLG_DOCK_FOM_FINISH_WITH_REC,
};

static const struct m0_fom_type_ops pdock_fom_type_ops = {
        .fto_create = pdock_fom_create,
};

const struct m0_fom_type_ops *m0_fdmi__pdock_fom_type_ops_get(void)
{
	return &pdock_fom_type_ops;
}

static struct m0_fom_type pdock_fom_type;

static struct m0_sm_state_descr fdmi_plugin_dock_state_descr[] = {
        [FDMI_PLG_DOCK_FOM_INIT] = {
                .sd_flags       = M0_SDF_INITIAL,
                .sd_name        = "Init",
                .sd_allowed     =
		M0_BITS(FDMI_PLG_DOCK_FOM_FEED_PLUGINS_WITH_REC)
        },

        [FDMI_PLG_DOCK_FOM_FINI] = {
                .sd_flags       = M0_SDF_TERMINAL,
                .sd_name        = "Fini",
                .sd_allowed     = 0
        },

        [FDMI_PLG_DOCK_FOM_FEED_PLUGINS_WITH_REC] = {
                .sd_flags       = 0,
                .sd_name        = "Feed Plugins With Record",
                .sd_allowed     = M0_BITS(FDMI_PLG_DOCK_FOM_FINISH_WITH_REC)
        },

        [FDMI_PLG_DOCK_FOM_FINISH_WITH_REC] = {
                .sd_flags       = 0,
                .sd_name        = "Finish With Record",
                .sd_allowed     = M0_BITS(FDMI_PLG_DOCK_FOM_FINI)
        },
};

const struct m0_sm_conf fdmi_plugin_dock_fom_sm_conf = {
	.scf_name = "fdmi-plugin-dock-fom-sm",
	.scf_nr_states = ARRAY_SIZE(fdmi_plugin_dock_state_descr),
	.scf_state = fdmi_plugin_dock_state_descr
};

static void    pdock_fom_fini(struct m0_fom *fom);
static int     pdock_fom_tick(struct m0_fom *fom);
static size_t  pdock_fom_home_locality(const struct m0_fom *fom);

static const struct m0_fom_ops pdock_fom_ops = {
	.fo_fini          = pdock_fom_fini,
	.fo_tick          = pdock_fom_tick,
	.fo_home_locality = pdock_fom_home_locality,
};

static int pdock_fom_create(struct m0_fop  *fop,
			    struct m0_fom **out,
			    struct m0_reqh *reqh)
{
	struct pdock_fom                 *pd_fom;
	struct m0_fom                    *fom;
	struct m0_fop                    *reply_fop;
	struct m0_fop_fdmi_record_reply  *reply_fop_data;
	struct m0_fop_fdmi_record        *frec;
	struct m0_fdmi_record_reg        *rreg;
	int                               rc;

	M0_ENTRY();
	M0_ASSERT(fop != NULL);
	M0_ASSERT(out != NULL);
	M0_ASSERT(reqh != NULL);
	M0_ASSERT(m0_fop_data(fop) != NULL);

	M0_ALLOC_PTR(pd_fom);
	if (pd_fom == NULL)
		return M0_RC(-ENOMEM);
	M0_SET0(pd_fom);

	M0_ALLOC_PTR(reply_fop_data);
	if (reply_fop_data == NULL) {
		rc = -ENOMEM;
		goto fom_fini;
	}

	rreg = m0_fdmi__pdock_fdmi_record_register(fop);
	if (rreg == NULL) {
		M0_LOG(M0_ERROR, "FDMI record failed to register");
		rc = -ENOENT;
		goto rep_fini;
	} else {
		int                        idx;
		struct m0_fdmi_flt_id_arr *filters =
			&rreg->frr_rec->fr_matched_flts;

		M0_LOG(M0_DEBUG, "FDMI record arrived: id = "
		       U128X_F, U128_P(&rreg->frr_rec->fr_rec_id));
		M0_LOG(M0_DEBUG, "FDMI record type = %d",
		       rreg->frr_rec->fr_rec_type);
		M0_LOG(M0_DEBUG, "*   matched filters count = [%d]",
		       filters->fmf_count);

		for (idx = 0; idx < filters->fmf_count; idx++) {
			M0_LOG(M0_DEBUG, "*   [%4d] = "
			       FID_SF, idx, FID_P(&filters->fmf_flt_id[idx]));
		}
	}

	/* get prepared to inspecting record guts */
	frec = m0_fop_data(fop);

	/* set up reply fop */
	reply_fop_data->frn_frt = frec->fr_rec_type;

	reply_fop = m0_fop_alloc(&m0_fop_fdmi_rec_not_rep_fopt, reply_fop_data,
				 m0_fdmi__pdock_conn_pool_rpc_machine());
	if (reply_fop == NULL) {
		rc = -ENOMEM;
		goto rep_fini;
	}

	if (m0_fop_to_rpc_item(fop)->ri_rmachine == NULL) {
		/* seems odd, as no rpc machine found attached !
		 * it must be ut in run, so reply has to be skipped
		 */
		m0_free(reply_fop);
		m0_free(reply_fop_data);
		reply_fop = NULL;
	}

	/* set up fom */
	fom = &pd_fom->pf_fom;

	m0_fom_init(fom, &fop->f_type->ft_fom_type,
		    &pdock_fom_ops, fop, reply_fop, reqh);

	M0_ASSERT(m0_fom_phase(fom) == FDMI_PLG_DOCK_FOM_INIT);
	*out = fom;

	return M0_RC(0);

rep_fini:
	m0_free(reply_fop_data);
	if (rreg != NULL)
		m0_ref_put(&rreg->frr_ref);
fom_fini:
	m0_free(pd_fom);
	return M0_RC(rc);


}

static void pdock_fom_fini(struct m0_fom *fom)
{
        struct pdock_fom *pd_fom;

	M0_ENTRY();

	pd_fom = container_of(fom, struct pdock_fom, pf_fom);

	if (pd_fom->pf_custom_fom_fini != NULL) {
		(*pd_fom->pf_custom_fom_fini)(fom);
	} else {
		m0_fom_fini(fom);
	}

	m0_free(pd_fom);

	M0_LEAVE();
}

static int pdock_fom_tick__init(struct m0_fom *fom)
{
	struct pdock_fom          *pd_fom;

	M0_ENTRY();

	if (fom->fo_rep_fop != NULL) {
		struct m0_fop_fdmi_record *fdmi_rec =
			(struct m0_fop_fdmi_record*) m0_fop_data(fom->fo_fop);

		M0_LOG(M0_DEBUG, "send reply fop data %p, rid " U128X_F,
		       fdmi_rec, U128_P(&fdmi_rec->fr_rec_id));

		m0_rpc_reply_post(m0_fop_to_rpc_item(fom->fo_fop),
				  m0_fop_to_rpc_item(fom->fo_rep_fop));
	}

	pd_fom = container_of(fom, struct pdock_fom, pf_fom);

	/* reset position in filter id array */
	pd_fom->pf_pos = 0;

	/* unveil fop data */
	pd_fom->pf_rec = m0_fop_data(fom->fo_fop);

	m0_fom_phase_set(fom, FDMI_PLG_DOCK_FOM_FEED_PLUGINS_WITH_REC);

	M0_LEAVE();
	return M0_FSO_AGAIN;
}

static int pdock_fom_tick__feed_plugin_with_rec(struct m0_fom *fom)
{
	struct pdock_fom            *pd_fom;
	struct m0_fop_fdmi_record   *frec;
	struct m0_fid               *fids;
	struct m0_fdmi_filter_reg   *freg;
	struct m0_fdmi_plugin_ops   *pcb;
	int                          rc;

	M0_ENTRY();

	pd_fom = container_of(fom, struct pdock_fom, pf_fom);
	frec   = pd_fom->pf_rec;
	fids   = frec->fr_matched_flts.fmf_flt_id;

	M0_ASSERT(pd_fom->pf_pos < frec->fr_matched_flts.fmf_count);

	/* filter lookup */
	freg = m0_fdmi__pdock_filter_reg_find(&fids[pd_fom->pf_pos]);

	if (freg == NULL) {
		/* filter not found, quit */
		m0_fom_phase_set(fom, FDMI_PLG_DOCK_FOM_FINISH_WITH_REC);
		M0_LOG(M0_NOTICE,
		       "filter reg not found: ffid = "FID_SF,
		       FID_P(&fids[pd_fom->pf_pos]));
		M0_LEAVE();
		return M0_FSO_AGAIN;
	}

	/* test filter reg for being in active state */
	if (freg->ffr_flags & M0_BITS(M0_FDMI_FILTER_INACTIVE,
				      M0_FDMI_FILTER_DEAD)) {
		M0_LOG(M0_NOTICE, "filter reg is not active: id = "FID_SF,
		       FID_P(&freg->ffr_ffid));
		/* an odd situation, but possible and not critical
		 * one, so plugin feed-up is anyway going to happen
		 */
	}

	pcb = freg->ffr_pcb;

	if (pcb == NULL) {
		M0_LOG(M0_ERROR, "filter reg contains null in pcb");
		goto move_on;
	}

	/* plugin feed */
	rc = (*pcb->po_fdmi_rec)(&frec->fr_rec_id,
				 frec->fr_payload,
				 fids[pd_fom->pf_pos]);
	if (rc == 0) {
		/*
		 * Plugin accepted the record,
		 * so increment fdmi record refc
		 */
		struct m0_fdmi_record_reg *rreg;

		rreg = m0_fdmi__pdock_record_reg_find(&frec->fr_rec_id);

		if (rreg == NULL) {
			/* critical error,
			   have to finish with no registration */
			m0_fom_phase_set(fom,
					 FDMI_PLG_DOCK_FOM_FINISH_WITH_REC);
			M0_LOG(M0_ERROR,
			       "record not registered in plugin dock");
			M0_LEAVE();
			return M0_FSO_AGAIN;
		}

		m0_ref_get(&rreg->frr_ref);
	} else {
		M0_LOG(M0_NOTICE,
		       "plugin has rejected the record processing: "
		       "rc = %i, frid = "U128X_F", ffid = "FID_SF, rc,
		       U128_P(&frec->fr_rec_id),
		       FID_P(&fids[pd_fom->pf_pos]));
	}

move_on:
	/* move forward and yeild */
	++pd_fom->pf_pos;

	if (pd_fom->pf_pos >= frec->fr_matched_flts.fmf_count) {
		m0_fom_phase_set(fom, FDMI_PLG_DOCK_FOM_FINISH_WITH_REC);
	}

	M0_LEAVE();
	return M0_FSO_AGAIN;
}

static int pdock_fom_tick__finish_with_rec(struct m0_fom *fom)
{
	struct pdock_fom            *pd_fom;
	struct m0_fop_fdmi_record   *frec;
	struct m0_fdmi_record_reg   *rreg;

	M0_ENTRY();

	pd_fom = container_of(fom, struct pdock_fom, pf_fom);
	frec   = pd_fom->pf_rec;
	rreg   = m0_fdmi__pdock_record_reg_find(&frec->fr_rec_id);
	if (rreg != NULL) {
		/**
		 * Release record reg refc:
		 * in case there was no feed done the record ref
		 * is going to be destroyed immediately, otherwise
		 * this will happen when the last release request
		 * is replied.
		 */

		/**
		 * Blocking fom is done to handle possible posting
		 * release request, therefore possible blocking
		 * on rpc connect when posting.
		 */
		m0_fom_block_enter(fom);
		m0_ref_put(&rreg->frr_ref);
		m0_fom_block_leave(fom);
	}

	M0_LOG(M0_DEBUG, "set fom state FOM_FINI");
	m0_fom_phase_set(fom, FDMI_PLG_DOCK_FOM_FINI);

	M0_LEAVE();
	return M0_FSO_WAIT;
}

static int pdock_fom_tick(struct m0_fom *fom)
{
	int rc = M0_FSO_AGAIN;

	M0_ENTRY("fom %p", fom);
	M0_LOG(M0_DEBUG, "fom phase %i", m0_fom_phase(fom));

	switch (m0_fom_phase(fom)) {

	case FDMI_PLG_DOCK_FOM_INIT:
		rc = pdock_fom_tick__init(fom);
		break;

	case FDMI_PLG_DOCK_FOM_FEED_PLUGINS_WITH_REC:
		rc = pdock_fom_tick__feed_plugin_with_rec(fom);
		break;

	case FDMI_PLG_DOCK_FOM_FINISH_WITH_REC:
		rc = pdock_fom_tick__finish_with_rec(fom);
		break;

	default:
		M0_IMPOSSIBLE("Phase not defined");
	}

	M0_LEAVE("<< rc %d", rc);
	return rc;
}

static size_t pdock_fom_home_locality(const struct m0_fom *fom)
{
	M0_ENTRY();
	M0_PRE(fom != NULL);
	M0_LEAVE();
	return 1;
}

M0_INTERNAL int m0_fdmi__plugin_dock_fom_init(void)
{
	M0_ENTRY();
	/* Initialize FDMI plugin dock fom */
	m0_fom_type_init(&pdock_fom_type, M0_FDMI_PLUGIN_DOCK_OPCODE,
			 &pdock_fom_type_ops, &m0_fdmi_service_type,
			 &fdmi_plugin_dock_fom_sm_conf);

	return M0_RC(0);
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
