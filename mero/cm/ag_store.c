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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 10/09/2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CM
#include "lib/trace.h"

#include "lib/bob.h"
#include "lib/misc.h"  /* M0_BITS */
#include "lib/errno.h" /* ENOENT EPERM */

#include "reqh/reqh.h"
#include "sm/sm.h"
#include "rpc/rpc_opcodes.h" /* M0_CM_SW_UPDATE_OPCODE */

#include "cm/sw.h"
#include "cm/cm.h"
#include "cm/proxy.h"

#include "be/op.h"           /* M0_BE_OP_SYNC */

/**
   @defgroup CMAGSTOREFOM aggregtion group store fom
   @ingroup CMAG

   Implementation of aggregation group store FOM.
   Provides mechanism to handle blocking operations like persisting
   aggregation group information (i.e. last processed aggregation group).
   When copy machine operation starts, aggregation group store fom reads
   persistent store and initialises copy machine to start processing from
   previously incomplete aggregation group after operations like quiesce
   and abort. In case of additional failures copy machine is reset and
   store information is discarded.

   @{
*/

enum ag_store_update_fom_phase {
	AG_STORE_INIT       = M0_FOM_PHASE_INIT,
	AG_STORE_FINI	 = M0_FOM_PHASE_FINISH,
	AG_STORE_INIT_WAIT,
	AG_STORE_START,
	AG_STORE_UPDATE,
	AG_STORE_UPDATE_WAIT,
	AG_STORE_COMPLETE,
	AG_STORE_NR
};

static const struct m0_fom_type_ops ag_store_update_fom_type_ops = {
};

static struct m0_sm_state_descr ag_store_update_sd[AG_STORE_NR] = {
	[AG_STORE_INIT] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "copy machine ag store init",
		.sd_allowed = M0_BITS(AG_STORE_START, AG_STORE_INIT_WAIT,
				      AG_STORE_UPDATE, AG_STORE_FINI)
	},
	[AG_STORE_INIT_WAIT] = {
		.sd_flags   = 0,
		.sd_name    = "ag store init wait",
		.sd_allowed = M0_BITS(AG_STORE_INIT_WAIT, AG_STORE_START,
				      AG_STORE_FINI)
	},
	[AG_STORE_START] = {
		.sd_flags   = 0,
		.sd_name    = "ag store start",
		.sd_allowed = M0_BITS(AG_STORE_UPDATE, AG_STORE_FINI)
	},
	[AG_STORE_UPDATE] = {
		.sd_flags   = 0,
		.sd_name    = "Update",
		.sd_allowed = M0_BITS(AG_STORE_UPDATE_WAIT, AG_STORE_FINI)
	},
	[AG_STORE_UPDATE_WAIT] = {
		.sd_flags   = 0,
		.sd_name    = "Update wait",
		.sd_allowed = M0_BITS(AG_STORE_UPDATE, AG_STORE_COMPLETE,
				      AG_STORE_FINI)
	},
	[AG_STORE_COMPLETE] = {
		.sd_flags   = 0,
		.sd_name    = "Update complete",
		.sd_allowed = M0_BITS(AG_STORE_UPDATE_WAIT, AG_STORE_FINI)
	},
	[AG_STORE_FINI] = {
		.sd_flags   = M0_SDF_TERMINAL,
		.sd_name    = "Fini",
		.sd_allowed = 0
	},
};

struct m0_sm_conf ag_store_update_conf = {
	.scf_name      = "sm: store update conf",
	.scf_nr_states = ARRAY_SIZE(ag_store_update_sd),
	.scf_state     = ag_store_update_sd,
};

static struct m0_cm *store2cm(struct m0_cm_ag_store *store)
{
	return container_of(store, struct m0_cm, cm_ag_store);
}

static struct m0_cm_ag_store *fom2store(struct m0_fom *fom)
{
	return container_of(fom, struct m0_cm_ag_store, s_fom);
}

static int ag_store_alloc(struct m0_cm_ag_store *store)
{
	struct m0_cm               *cm = store2cm(store);
	struct m0_be_seg           *seg = cm->cm_service.rs_reqh->rh_beseg;
	struct m0_cm_ag_store_data *data;
	struct m0_be_tx            *tx = &store->s_fom.fo_tx.tx_betx;
	char                        ag_store[80];
	int                         rc;

	sprintf(ag_store, "ag_store_%llu", (unsigned long long)cm->cm_id);

	M0_BE_ALLOC_PTR_SYNC(data, seg, tx);
	if (data == NULL) {
		rc = -ENOMEM;
	} else {
		rc = m0_be_seg_dict_insert(seg, tx, ag_store, data);
		if (rc == 0) {
			M0_SET0(data);
			M0_BE_TX_CAPTURE_PTR(seg, tx, data);
			M0_LOG(M0_DEBUG, "allocated data iter store = %p", data);
		} else
			M0_BE_FREE_PTR_SYNC(data, seg, tx);
	}
	m0_be_tx_close(tx);

	return M0_RC(rc);
}

static int ag_store_init_load(struct m0_cm_ag_store *store, struct m0_cm_ag_store_data **data)
{
	struct m0_be_tx_credit   cred = {};
	struct m0_cm            *cm = store2cm(store);
	struct m0_fom           *fom = &store->s_fom;
	struct m0_be_seg        *seg  = cm->cm_service.rs_reqh->rh_beseg;
	struct m0_be_tx         *tx  = &fom->fo_tx.tx_betx;
	char                     ag_store[80];
	int                      rc;

	sprintf(ag_store, "ag_store_%llu", (unsigned long long)cm->cm_id);
	rc = m0_be_seg_dict_lookup(seg, ag_store, (void**)data);
	if (rc == 0) {
		M0_LOG(M0_DEBUG, "in=["M0_AG_F"] out=["M0_AG_F"]",
				 M0_AG_P(&(*data)->d_in),  M0_AG_P(&(*data)->d_out));
		return M0_RC(rc);
	} else if (rc == -ENOENT) {
		M0_SET0(tx);
		m0_be_tx_init(tx, 0, seg->bs_domain, &fom->fo_loc->fl_group, NULL,
				NULL, NULL, NULL);
		M0_BE_ALLOC_CREDIT_PTR(*data, seg, &cred);
		m0_be_seg_dict_insert_credit(seg, ag_store, &cred);
		m0_be_tx_prep(tx, &cred);
		m0_be_tx_open(tx);
		M0_POST(tx->t_sm.sm_rc == 0);
	}

	return M0_RC(rc);
}

/**
 * Setting in and out AG id and epoch to in-memory store and its original cm.
 */
static void in_out_set(struct m0_cm *cm,
		       struct m0_cm_ag_store *store,
		       struct m0_cm_ag_store_data* s_data)
{
	m0_cm_lock(cm);
	store->s_data.d_in  = s_data->d_in;
	store->s_data.d_out = s_data->d_out;

	cm->cm_sw_last_updated_hi = s_data->d_in;
	cm->cm_last_processed_out = s_data->d_out;

	if (s_data->d_cm_epoch != 0) {
		/* Only do this if the epoch is valid. */
		store->s_data.d_cm_epoch = s_data->d_cm_epoch;
		cm->cm_epoch = s_data->d_cm_epoch;
	}
	M0_SET0(&cm->cm_last_out_hi);
	m0_cm_unlock(cm);
}

static int ag_store_init(struct m0_cm_ag_store *store)
{
	struct m0_cm               *cm = store2cm(store);
	struct m0_cm_ag_store_data *s_data;
	struct m0_fom              *fom = &store->s_fom;
	int                         phase;
	int                         rc;

	/*
	 * copying epoch from cm. It will be overwritten if a valid epoch
	 * is found from persistent storage.
	 */
	store->s_data.d_cm_epoch = cm->cm_epoch;

	rc = ag_store_init_load(store, &s_data);
	if (rc == 0 || rc == -ENOENT) {
		if (rc == 0) {
			in_out_set(cm, store, s_data);
			phase = AG_STORE_START;
			if (cm->cm_reset) {
				M0_SET0(&cm->cm_sw_last_updated_hi);
				M0_SET0(&cm->cm_last_processed_out);
			}

			if (cm->cm_proxy_nr > 0) {
				/*
				 * Wait until we receive sliding window updates
				 * from all other replicas.
				 */
				m0_cm_lock(cm);
				m0_cm_proxies_init_wait(cm, fom);
				rc = M0_FSO_WAIT;
				m0_cm_unlock(cm);
			} else
				rc = M0_FSO_AGAIN;
			/* Notify copy machine after reading persistent store. */
			m0_cm_notify(cm);
		} else {
			phase = AG_STORE_INIT_WAIT;
			rc = M0_FSO_AGAIN;
		}
		m0_fom_phase_set(fom, phase);
	}

	M0_LOG(M0_DEBUG," rc=%d ag_store_in=["M0_AG_F"] ag_store_out=["M0_AG_F"]",
		rc, M0_AG_P(&cm->cm_sw_last_updated_hi),
		M0_AG_P(&cm->cm_last_processed_out));
	return M0_RC(rc);
}

static int ag_store_start(struct m0_cm_ag_store *store)
{
	struct m0_cm *cm = store2cm(store);

	m0_fom_phase_set(&store->s_fom, AG_STORE_UPDATE);
	M0_LOG(M0_DEBUG, "cm_sw_last_updated_hi=["M0_AG_F"]",
	       M0_AG_P(&cm->cm_sw_last_updated_hi));
	cm->cm_last_processed_out = cm->cm_sw_last_updated_hi;
	m0_cm_sw_update_start(cm);
	m0_chan_signal_lock(&cm->cm_proxy_init_wait);

	return M0_FSO_AGAIN;
}

static int ag_store_init_wait(struct m0_cm_ag_store *store)
{
	struct m0_cm               *cm = store2cm(store);
	struct m0_fom              *fom = &store->s_fom;
	struct m0_be_tx            *tx  = &fom->fo_tx.tx_betx;
	struct m0_cm_ag_store_data *s_data;
	int                         rc;

	switch (m0_be_tx_state(tx)) {
	case M0_BTS_FAILED :
		return tx->t_sm.sm_rc;
	case M0_BTS_GROUPING :
	case M0_BTS_OPENING :
		break;
	case M0_BTS_ACTIVE :
		rc = ag_store_alloc(store);
		if (rc != 0)
			return M0_RC(rc);
		break;
	case M0_BTS_DONE :
		rc = tx->t_sm.sm_rc;
		m0_be_tx_fini(tx);
		if (rc != 0)
			return M0_RC(rc);
		rc = ag_store_init_load(store, &s_data);
		if (rc != 0)
			return M0_ERR_INFO(rc, "Cannot load ag store, status=%d phase=%d",
						store->s_status, m0_fom_phase(fom));
		in_out_set(cm, store, s_data);
		m0_fom_phase_move(fom, 0, AG_STORE_START);
		if (cm->cm_proxy_nr > 0) {
			m0_cm_lock(cm);
			m0_cm_proxies_init_wait(cm, fom);
			m0_cm_unlock(cm);
			rc = M0_FSO_WAIT;
		} else
			rc = M0_FSO_AGAIN;

		m0_cm_notify(cm);
		return rc;
	default :
		break;
	}
	m0_fom_wait_on(fom, &tx->t_sm.sm_chan,
			&fom->fo_cb);

	return M0_FSO_WAIT;
}

static int ag_store__update(struct m0_cm_ag_store *store)
{
	struct m0_cm               *cm = store2cm(store);
	struct m0_be_seg           *seg = cm->cm_service.rs_reqh->rh_beseg;
	struct m0_cm_ag_store_data *s_data;
	struct m0_fom              *fom = &store->s_fom;
	struct m0_be_tx            *tx  = &fom->fo_tx.tx_betx;
	int                         rc;

	rc = ag_store_init_load(store, &s_data);
	if (rc != 0)
		return M0_ERR_INFO(rc, "Cannot load ag store, status=%d phase=%d",
					store->s_status, m0_fom_phase(fom));
	if (rc == 0) {
		s_data->d_in       = store->s_data.d_in;
		s_data->d_out      = store->s_data.d_out;
		s_data->d_cm_epoch = store->s_data.d_cm_epoch;
		M0_BE_TX_CAPTURE_PTR(seg, tx, s_data);
	}

	return M0_RC(rc);
}

static int ag_store_update(struct m0_cm_ag_store *store)
{
	struct m0_cm      *cm = store2cm(store);
	struct m0_fom     *fom = &store->s_fom;
	struct m0_dtx     *tx = &fom->fo_tx;
	struct m0_be_seg  *seg  = cm->cm_service.rs_reqh->rh_beseg;
	int                rc;

	if (tx->tx_state < M0_DTX_INIT) {
		M0_SET0(tx);
		m0_dtx_init(tx, seg->bs_domain,
				&fom->fo_loc->fl_group);
		tx->tx_betx_cred = M0_BE_TX_CREDIT_TYPE(struct m0_cm_ag_store_data);
	}

	if (m0_be_tx_state(&tx->tx_betx) == M0_BTS_PREPARE) {
		m0_dtx_open(tx);
		return M0_FSO_AGAIN;
	}
        else if (M0_IN(m0_be_tx_state(&tx->tx_betx), (M0_BTS_OPENING,
						      M0_BTS_GROUPING))) {
		m0_fom_wait_on(fom, &tx->tx_betx.t_sm.sm_chan, &fom->fo_cb);
		return M0_FSO_WAIT;
	}

	m0_dtx_opened(tx);
	m0_cm_lock(cm);
	rc = ag_store__update(store);
	m0_cm_unlock(cm);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "AG Store update failed: %d", rc);
		store->s_status = S_FINI;
		if (rc != -ENOENT)
			return M0_ERR(rc);
	}
	m0_fom_wait_on(fom, &tx->tx_betx.t_sm.sm_chan, &fom->fo_cb);
	m0_dtx_done(tx);
	m0_fom_phase_move(fom, rc, AG_STORE_UPDATE_WAIT);

	return M0_FSO_WAIT;
}

static int ag_store_update_wait(struct m0_cm_ag_store *store)
{
	struct m0_fom  *fom = &store->s_fom;
	struct m0_dtx  *tx = &fom->fo_tx;
	int             rc = M0_FSO_WAIT;

	if (tx->tx_state == M0_DTX_DONE) {
		if (m0_be_tx_state(&tx->tx_betx) != M0_BTS_DONE) {
			m0_fom_wait_on(fom, &tx->tx_betx.t_sm.sm_chan,
				       &fom->fo_cb);
		} else {
			m0_dtx_fini(tx);
			M0_SET0(tx);
			switch (store->s_status) {
			case S_ACTIVE :
				m0_fom_phase_move(fom, 0, AG_STORE_UPDATE);
				rc = M0_FSO_AGAIN;
				break;
			case S_COMPLETE :
				m0_fom_phase_move(fom, 0, AG_STORE_COMPLETE);
				rc = M0_FSO_AGAIN;
				break;
			case S_FINI:
				m0_fom_phase_move(fom, 0, AG_STORE_FINI);
				rc = M0_FSO_WAIT;
				break;
			default:
				M0_IMPOSSIBLE("Invalid status !!");
			};
		}
	}

	return rc;
}

static int ag_store_complete(struct m0_cm_ag_store *store)
{
	struct m0_cm               *cm = store2cm(store);
	struct m0_fom              *fom = &store->s_fom;
	struct m0_dtx              *tx = &fom->fo_tx;
	struct m0_be_seg           *seg = cm->cm_service.rs_reqh->rh_beseg;
	struct m0_cm_ag_store_data *s_data;
	char                        ag_store[80];
	int                         rc;

	rc = ag_store_init_load(store, &s_data);
	if (rc != 0)
		return M0_ERR_INFO(rc, "Cannot load ag store, status=%d phase=%d",
					store->s_status, m0_fom_phase(fom));

	sprintf(ag_store, "ag_store_%llu", (unsigned long long)cm->cm_id);
        if (tx->tx_state < M0_DTX_INIT) {
	        M0_SET0(tx);
                m0_dtx_init(tx, seg->bs_domain,
                                &fom->fo_loc->fl_group);
		M0_BE_FREE_CREDIT_PTR(s_data, seg, &tx->tx_betx_cred);
		m0_be_seg_dict_delete_credit(seg, ag_store,
					     &tx->tx_betx_cred);
        }

        if (m0_be_tx_state(&tx->tx_betx) == M0_BTS_PREPARE)
                m0_dtx_open(tx);
	if (m0_be_tx_state(&tx->tx_betx) == M0_BTS_FAILED)
		return M0_RC(tx->tx_betx.t_sm.sm_rc);
        if (M0_IN(m0_be_tx_state(&tx->tx_betx), (M0_BTS_OPENING,
						 M0_BTS_GROUPING))) {
                m0_fom_wait_on(fom, &tx->tx_betx.t_sm.sm_chan, &fom->fo_cb);
                return M0_FSO_WAIT;
        } else
		m0_dtx_opened(tx);

	M0_BE_FREE_PTR_SYNC(s_data, seg, &tx->tx_betx);
	m0_be_seg_dict_delete(seg, &tx->tx_betx, ag_store);
	m0_fom_wait_on(fom, &tx->tx_betx.t_sm.sm_chan, &fom->fo_cb);
	m0_dtx_done(tx);
	m0_fom_phase_move(fom, rc, AG_STORE_UPDATE_WAIT);
	store->s_status = S_FINI;

	return M0_FSO_WAIT;
}

static int (*ag_store_action[]) (struct m0_cm_ag_store *store) = {
	[AG_STORE_INIT]        = ag_store_init,
	[AG_STORE_INIT_WAIT]   = ag_store_init_wait,
	[AG_STORE_START]       = ag_store_start,
	[AG_STORE_UPDATE]      = ag_store_update,
	[AG_STORE_UPDATE_WAIT] = ag_store_update_wait,
	[AG_STORE_COMPLETE]    = ag_store_complete,
};

static uint64_t ag_store_fom_locality(const struct m0_fom *fom)
{
	return fom->fo_type->ft_id;
}
static int ag_store_fom_tick(struct m0_fom *fom)
{
	struct m0_cm_ag_store *store;
	int                    phase = m0_fom_phase(fom);
	int                    rc;

	store = fom2store(fom);
	rc = ag_store_action[phase](store);
	if (rc < 0) {
		m0_fom_phase_move(fom, 0, AG_STORE_FINI);
		rc = M0_FSO_WAIT;
	}

	return M0_RC(rc);
}

static void ag_store_fom_fini(struct m0_fom *fom)
{
	struct m0_cm_ag_store *store = fom2store(fom);
	struct m0_cm *cm = store2cm(store);

	m0_fom_fini(fom);
	m0_chan_signal_lock(&cm->cm_complete);
}

static const struct m0_fom_ops ag_store_update_fom_ops = {
	.fo_fini          = ag_store_fom_fini,
	.fo_tick          = ag_store_fom_tick,
	.fo_home_locality = ag_store_fom_locality
};

M0_INTERNAL void m0_cm_ag_store_init(struct m0_cm_type *cmtype)
{
	m0_fom_type_init(&cmtype->ct_ag_store_fomt, cmtype->ct_fom_id + 3,
			 &ag_store_update_fom_type_ops,
			 &cmtype->ct_stype, &ag_store_update_conf);
}

M0_INTERNAL void m0_cm_ag_store_complete(struct m0_cm_ag_store *store)
{
	store->s_status = S_COMPLETE;
}

M0_INTERNAL void m0_cm_ag_store_fini(struct m0_cm_ag_store *store)
{
	store->s_status = S_FINI;
}

M0_INTERNAL bool m0_cm_ag_store_is_complete(struct m0_cm_ag_store *store)
{
	return m0_fom_phase(&store->s_fom) == AG_STORE_FINI;
}

M0_INTERNAL void m0_cm_ag_store_fom_start(struct m0_cm *cm)
{
	struct m0_cm_ag_store *store = &cm->cm_ag_store;
	struct m0_fom         *fom = &store->s_fom;

	store->s_status = S_ACTIVE;
	m0_fom_init(&cm->cm_ag_store.s_fom, &cm->cm_type->ct_ag_store_fomt,
		    &ag_store_update_fom_ops, NULL, NULL, cm->cm_service.rs_reqh);
	m0_fom_queue(fom);

	M0_LEAVE();
}

#undef M0_TRACE_SUBSYSTEM

/** @} endgroup CMSWFOM */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
