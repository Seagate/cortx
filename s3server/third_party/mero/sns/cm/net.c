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
 * Original author: Anup Barve <anup_barve@xyratex.com>
 * Original creation date: 02/27/2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"

#include "lib/memory.h"

#include "cm/proxy.h"
#include "sns/cm/cm.h"
#include "sns/cm/cp.h"
#include "sns/cm/sns_cp_onwire.h"
#include "sns/cm/cm_utils.h"
#include "sns/cm/file.h"

#include "fop/fop.h"
#include "fop/fom.h"
#include "net/net.h"
#include "rpc/item.h"
#include "rpc/rpclib.h"
#include "rpc/session.h"
#include "rpc/conn.h"
#include "rpc/rpc_machine.h"           /* m0_rpc_machine */
#include "ioservice/fid_convert.h"     /* m0_fid_convert_stob2cob */

/**
 * @addtogroup SNSCMCP
 * @{
 */

static void cp_reply_received(struct m0_rpc_item *item);

/*
 * Over-ridden rpc item ops, required to send notification to the copy packet
 * send phase that reply has been received and the copy packet can be finalised.
 */
static const struct m0_rpc_item_ops cp_item_ops = {
	.rio_replied = cp_reply_received,
};

/* Creates indexvec structure based on number of segments and segment size. */
static int indexvec_prepare(struct m0_io_indexvec *iv, m0_bindex_t idx,
			    uint32_t seg_nr, size_t seg_size)
{
	int i;

	M0_PRE(iv != NULL);

	M0_ALLOC_ARR(iv->ci_iosegs, seg_nr);
	if (iv->ci_iosegs == NULL) {
		m0_free(iv);
		return M0_ERR(-ENOMEM);
	}

	iv->ci_nr = seg_nr;
	for (i = 0; i < seg_nr; ++i) {
		iv->ci_iosegs[i].ci_index = idx;
		iv->ci_iosegs[i].ci_count = seg_size;
		idx += seg_size;
	}
	return 0;
}

/* Converts in-memory copy packet structure to onwire copy packet structure. */
static int snscp_to_snscpx(const struct m0_sns_cm_cp *sns_cp,
			   struct m0_sns_cpx *sns_cpx)
{
	struct m0_net_buffer    *nbuf;
	const struct m0_cm_cp   *cp;
	uint32_t                 nbuf_seg_nr;
	uint32_t                 tmp_seg_nr;
	uint32_t                 nb_idx = 0;
	uint32_t                 nb_cnt;
	uint64_t                 offset;
	int                      rc;
	int                      i;

	M0_PRE(sns_cp != NULL);
	M0_PRE(sns_cpx != NULL);

	cp = &sns_cp->sc_base;

	sns_cpx->scx_stob_id = sns_cp->sc_stob_id;
	sns_cpx->scx_failed_idx = sns_cp->sc_failed_idx;
	sns_cpx->scx_cp.cpx_prio = cp->c_prio;
	sns_cpx->scx_phase = M0_CCP_SEND;
	m0_cm_ag_id_copy(&sns_cpx->scx_cp.cpx_ag_id, &cp->c_ag->cag_id);
	sns_cpx->scx_cp.cpx_ag_cp_idx = cp->c_ag_cp_idx;
	m0_bitmap_onwire_init(&sns_cpx->scx_cp.cpx_bm,
			      cp->c_ag->cag_cp_global_nr);
	m0_bitmap_store(&cp->c_xform_cp_indices, &sns_cpx->scx_cp.cpx_bm);
	sns_cpx->scx_cp.cpx_epoch = cp->c_epoch;

	offset = sns_cp->sc_index;
	nb_cnt = cp->c_buf_nr;
	M0_ALLOC_ARR(sns_cpx->scx_ivecs.cis_ivecs, nb_cnt);
	if (sns_cpx->scx_ivecs.cis_ivecs == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto out;
	}

	tmp_seg_nr = cp->c_data_seg_nr;
	m0_tl_for(cp_data_buf, &cp->c_buffers, nbuf) {
		nbuf_seg_nr = min32(nbuf->nb_pool->nbp_seg_nr, tmp_seg_nr);
		tmp_seg_nr -= nbuf_seg_nr;
		rc = indexvec_prepare(&sns_cpx->scx_ivecs.
					       cis_ivecs[nb_idx],
					       offset,
					       nbuf_seg_nr,
					       nbuf->nb_pool->nbp_seg_size);
		if (rc != 0 )
			goto cleanup;

		offset += nbuf_seg_nr * nbuf->nb_pool->nbp_seg_size;
		M0_CNT_INC(nb_idx);
	} m0_tl_endfor;
	sns_cpx->scx_ivecs.cis_nr = nb_idx;
	sns_cpx->scx_cp.cpx_desc.id_nr = nb_idx;

	M0_ALLOC_ARR(sns_cpx->scx_cp.cpx_desc.id_descs,
		      sns_cpx->scx_cp.cpx_desc.id_nr);
	if (sns_cpx->scx_cp.cpx_desc.id_descs == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto cleanup;
	}

	goto out;

cleanup:
	for (i = 0; i < nb_idx; ++i)
		m0_free(&sns_cpx->scx_ivecs.cis_ivecs[nb_idx]);
	m0_free(sns_cpx->scx_ivecs.cis_ivecs);
	m0_bitmap_onwire_fini(&sns_cpx->scx_cp.cpx_bm);
out:
	return M0_RC(rc);
}

static void cp_reply_received(struct m0_rpc_item *req_item)
{
	struct m0_fop           *req_fop;
	struct m0_sns_cm_cp     *scp;
	struct m0_rpc_item      *rep_item;
	struct m0_cm_cp_fop     *cp_fop;
	struct m0_fop           *rep_fop;
	struct m0_sns_cpx_reply *sns_cpx_rep;

	req_fop = m0_rpc_item_to_fop(req_item);
	cp_fop = container_of(req_fop, struct m0_cm_cp_fop, cf_fop);
	scp = cp2snscp(cp_fop->cf_cp);
	if (req_item->ri_error == 0) {
		rep_item = req_item->ri_reply;
		if (m0_rpc_item_is_generic_reply_fop(rep_item))
			scp->sc_base.c_rc = m0_rpc_item_generic_reply_rc(rep_item);
		else {
			rep_fop = m0_rpc_item_to_fop(rep_item);
			sns_cpx_rep = m0_fop_data(rep_fop);
			scp->sc_base.c_rc = sns_cpx_rep->scr_cp_rep.cr_rc;
		}
	} else
		scp->sc_base.c_rc = req_item->ri_error;

	m0_fom_wakeup(&scp->sc_base.c_fom);
}

static void cp_fop_release(struct m0_ref *ref)
{
	struct m0_cm_cp_fop *cp_fop;
	struct m0_fop       *fop = container_of(ref, struct m0_fop, f_ref);

	cp_fop = container_of(fop, struct m0_cm_cp_fop, cf_fop);
	M0_ASSERT(cp_fop != NULL);
	m0_fop_fini(fop);
	m0_free(cp_fop);
}

M0_INTERNAL int m0_sns_cm_cp_send(struct m0_cm_cp *cp, struct m0_fop_type *ft)
{
	struct m0_sns_cm_cp    *sns_cp;
	struct m0_sns_cpx      *sns_cpx;
	struct m0_rpc_bulk     *rbulk = NULL;
	struct m0_rpc_bulk_buf *rbuf;
	struct m0_net_domain   *ndom;
	struct m0_net_buffer   *nbuf;
	struct m0_rpc_session  *session;
	struct m0_cm_cp_fop    *cp_fop;
	struct m0_fop          *fop;
	struct m0_rpc_item     *item;
	uint32_t                nbuf_seg_nr;
	uint32_t                tmp_seg_nr;
	uint64_t                offset;
	int                     rc;
	int                     i;

	M0_PRE(cp != NULL && m0_fom_phase(&cp->c_fom) == M0_CCP_SEND);
	M0_PRE(cp->c_cm_proxy != NULL);

	M0_ALLOC_PTR(cp_fop);
	if (cp_fop == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto out;
	}
	sns_cp = cp2snscp(cp);
	fop = &cp_fop->cf_fop;
	m0_fop_init(fop, ft, NULL, cp_fop_release);
	rc = m0_fop_data_alloc(fop);
	if (rc != 0) {
		m0_fop_fini(fop);
		m0_free(cp_fop);
		goto out;
	}

	sns_cpx = m0_fop_data(fop);
	M0_PRE(sns_cpx != NULL);
	cp_fop->cf_cp = cp;
	rc = snscp_to_snscpx(sns_cp, sns_cpx);
	if (rc != 0)
		goto out;

	rbulk = &cp->c_bulk;
	m0_mutex_lock(&cp->c_cm_proxy->px_mutex);
        session = cp->c_cm_proxy->px_session;
        ndom = session->s_conn->c_rpc_machine->rm_tm.ntm_dom;
	m0_mutex_unlock(&cp->c_cm_proxy->px_mutex);

	offset = sns_cp->sc_index;
	tmp_seg_nr = cp->c_data_seg_nr;
	m0_tl_for(cp_data_buf, &cp->c_buffers, nbuf) {
		nbuf_seg_nr = min32(nbuf->nb_pool->nbp_seg_nr, tmp_seg_nr);
		tmp_seg_nr -= nbuf_seg_nr;
		rc = m0_rpc_bulk_buf_add(rbulk, nbuf_seg_nr, 0,
					 ndom, NULL, &rbuf);
		if (rc != 0 || rbuf == NULL)
			goto out;

		for (i = 0; i < nbuf_seg_nr; ++i) {
			rc = m0_rpc_bulk_buf_databuf_add(rbuf,
					nbuf->nb_buffer.ov_buf[i],
					nbuf->nb_buffer.ov_vec.v_count[i],
					offset, ndom);
			offset += nbuf->nb_buffer.ov_vec.v_count[i];
			if (rc != 0)
				goto out;
		}
	} m0_tl_endfor;

	m0_mutex_lock(&rbulk->rb_mutex);
	m0_rpc_bulk_qtype(rbulk, M0_NET_QT_PASSIVE_BULK_SEND);
	m0_mutex_unlock(&rbulk->rb_mutex);

	rc = m0_rpc_bulk_store(rbulk, session->s_conn,
			       sns_cpx->scx_cp.cpx_desc.id_descs,
			       &m0_rpc__buf_bulk_cb);
	if (rc != 0)
		goto out;

	item  = m0_fop_to_rpc_item(fop);
	item->ri_ops = &cp_item_ops;
	item->ri_session = session;
	item->ri_prio  = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline = 0;

	m0_rpc_post(item);
	m0_fop_put_lock(fop);
out:
	if (rc != 0) {
		M0_LOG(M0_ERROR, "rc=%d", rc);
		m0_fom_phase_move(&cp->c_fom, rc, M0_CCP_FAIL);
		return M0_FSO_AGAIN;
	}

	m0_fom_phase_move(&cp->c_fom, rc, M0_CCP_SEND_WAIT);
	return M0_FSO_WAIT;
}

M0_TL_DESCR_DECLARE(rpcbulk, M0_EXTERN);
M0_TL_DECLARE(rpcbulk, M0_INTERNAL, struct m0_rpc_bulk_buf);

M0_INTERNAL int m0_sns_cm_cp_send_wait(struct m0_cm_cp *cp)
{
	struct m0_rpc_bulk *rbulk = &cp->c_bulk;
	int                 c_rc;

	M0_PRE(cp != NULL);

	c_rc = cp->c_rc;
	if (c_rc != 0 && c_rc != -ENOENT) {
		M0_LOG(M0_ERROR, "rc=%d", c_rc);
		/* Cleanup rpc bulk in m0_cm_cp_only_fini().*/
		m0_fom_phase_move(&cp->c_fom, c_rc, M0_CCP_FAIL);
		return M0_FSO_AGAIN;
	}

	/*
	 * Wait on channel till all net buffers are deleted from
	 * transfer machine.
	 */
	if (c_rc == 0) {
		m0_mutex_lock(&rbulk->rb_mutex);
		if (!rpcbulk_tlist_is_empty(&rbulk->rb_buflist)) {
			m0_fom_wait_on(&cp->c_fom, &rbulk->rb_chan, &cp->c_fom.fo_cb);
			m0_mutex_unlock(&rbulk->rb_mutex);
			return M0_FSO_WAIT;
		}
		m0_mutex_unlock(&rbulk->rb_mutex);
	}

	return cp->c_ops->co_phase_next(cp);
}

static void cp_buf_acquire(struct m0_cm_cp *cp)
{
	struct m0_sns_cm *sns_cm = cm2sns(cp->c_ag->cag_cm);
	int               rc;

	rc = m0_sns_cm_buf_attach(&sns_cm->sc_ibp.sb_bp, cp);
	M0_ASSERT(rc == 0);
}

static void cp_reply_post(struct m0_cm_cp *cp)
{
	struct m0_fop           *r_fop = cp->c_fom.fo_rep_fop;
	struct m0_sns_cpx_reply *sns_cpx_rep;

	sns_cpx_rep = m0_fop_data(r_fop);
	sns_cpx_rep->scr_cp_rep.cr_rc = cp->c_rc;
	m0_rpc_reply_post(&cp->c_fom.fo_fop->f_item, &r_fop->f_item);
}

static uint32_t seg_nr_get(const struct m0_sns_cpx *sns_cpx, uint32_t ivec_nr)
{
	int      i;
	uint32_t seg_nr = 0;

	M0_PRE(sns_cpx != NULL);

	for (i = 0; i < ivec_nr; ++i)
		seg_nr += sns_cpx->scx_ivecs.cis_ivecs[i].ci_nr;

	return seg_nr;
}

static int ag_cp_recvd_from_proxy(struct m0_cm_aggr_group *ag,
				  struct m0_sns_cm_cp *scp)
{
	struct m0_sns_cm_ag         *sag = ag2snsag(ag);
	struct m0_cm_proxy          *proxy = scp->sc_base.c_cm_proxy;
	struct m0_cm_proxy_in_count *pcount;

	M0_PRE(m0_cm_ag_is_locked(ag));

	pcount = &sag->sag_proxy_in_count;
	if (ag->cag_is_frozen && pcount->p_count[proxy->px_id] == 0)
		return -ENOENT;
	M0_CNT_DEC(pcount->p_count[proxy->px_id]);
	m0_cm_ag_cp_add_locked(ag, &scp->sc_base);

	return 0;
}

/* Converts onwire copy packet structure to in-memory copy packet structure. */
static int snscpx_to_snscp(const struct m0_sns_cpx *sns_cpx,
                            struct m0_sns_cm_cp *sns_cp)
{
	struct m0_cm_ag_id       ag_id;
	struct m0_cm            *cm;
	struct m0_cm_aggr_group *ag;
	int                      rc;

	M0_PRE(sns_cp != NULL);
	M0_PRE(sns_cpx != NULL);

	sns_cp->sc_stob_id = sns_cpx->scx_stob_id;
	m0_fid_convert_stob2cob(&sns_cpx->scx_stob_id, &sns_cp->sc_cobfid);
	sns_cp->sc_failed_idx = sns_cpx->scx_failed_idx;

	sns_cp->sc_index =
		sns_cpx->scx_ivecs.cis_ivecs[0].ci_iosegs[0].ci_index;

	sns_cp->sc_base.c_prio = sns_cpx->scx_cp.cpx_prio;
	sns_cp->sc_base.c_epoch = sns_cpx->scx_cp.cpx_epoch;

	m0_cm_ag_id_copy(&ag_id, &sns_cpx->scx_cp.cpx_ag_id);

	cm = cpfom2cm(&sns_cp->sc_base.c_fom);
	m0_cm_lock(cm);
	ag = m0_cm_aggr_group_locate(cm, &ag_id, true);
	m0_cm_unlock(cm);
	if (ag == NULL) {
		M0_LOG(M0_WARN, "ag="M0_AG_F" not found", M0_AG_P(&ag_id));
		return -ENOENT;
	}

	m0_cm_ag_lock(ag);
	rc = ag_cp_recvd_from_proxy(ag, sns_cp);
	m0_cm_ag_unlock(ag);

	if (rc != 0)
		return M0_ERR(rc);

	sns_cp->sc_base.c_ag_cp_idx = sns_cpx->scx_cp.cpx_ag_cp_idx;
	m0_bitmap_init(&sns_cp->sc_base.c_xform_cp_indices,
			ag->cag_cp_global_nr);
	m0_bitmap_load(&sns_cpx->scx_cp.cpx_bm,
			&sns_cp->sc_base.c_xform_cp_indices);

	sns_cp->sc_base.c_buf_nr = 0;
	sns_cp->sc_base.c_data_seg_nr = seg_nr_get(sns_cpx,
			sns_cpx->scx_ivecs.cis_nr);

	return 0;
}

M0_INTERNAL int m0_sns_cm_cp_recv_init(struct m0_cm_cp *cp)
{
	struct m0_rpc_bulk_buf *rbuf;
	struct m0_net_domain   *ndom;
	struct m0_net_buffer   *nbuf;
	struct m0_rpc_bulk     *rbulk;
	struct m0_rpc_session  *session;
	struct m0_cm_proxy     *cm_proxy;
	struct m0_fop          *fop = cp->c_fom.fo_fop;
	struct m0_sns_cpx      *sns_cpx;
	struct m0_sns_cm_cp    *sns_cp = cp2snscp(cp);
	uint32_t                nbuf_idx = 0;
	int                     rc;

	M0_PRE(cp != NULL && m0_fom_phase(&cp->c_fom) == M0_CCP_RECV_INIT);

	cm_proxy = cp->c_cm_proxy;
	sns_cpx = m0_fop_data(fop);
	M0_PRE(sns_cpx != NULL);
	if (sns_cpx->scx_cp.cpx_epoch != cm_proxy->px_epoch) {
		M0_LOG(M0_WARN, "delayed/stale cp epoch:%llx "
		       "proxy epoch=%llx (%s)",
		       (unsigned long long)cp->c_epoch,
		       (unsigned long long)cm_proxy->px_epoch,
		       cm_proxy->px_endpoint);
		cp->c_rc = -EPERM;
		cp_reply_post(cp);
		m0_fom_phase_set(&cp->c_fom, M0_CCP_FINI);
		return M0_FSO_WAIT;
	}

	rc = snscpx_to_snscp(sns_cpx, sns_cp);
	if (rc != 0) {
		cp->c_rc = rc;
		cp_reply_post(cp);
		m0_fom_phase_set(&cp->c_fom, M0_CCP_FINI);
		return M0_FSO_WAIT;
	}

	cp_buf_acquire(cp);
	session = fop->f_item.ri_session;
	ndom = session->s_conn->c_rpc_machine->rm_tm.ntm_dom;
	rbulk = &cp->c_bulk;

	m0_tl_for(cp_data_buf, &cp->c_buffers, nbuf) {
		nbuf->nb_buffer.ov_vec.v_nr =
			sns_cpx->scx_ivecs.cis_ivecs[nbuf_idx].ci_nr;
		rc = m0_rpc_bulk_buf_add(rbulk,
				sns_cpx->scx_ivecs.cis_ivecs[nbuf_idx].ci_nr,
				0, ndom, nbuf, &rbuf);
		if (rc != 0 || rbuf == NULL)
			goto out;
		M0_CNT_INC(nbuf_idx);
	} m0_tl_endfor;

	m0_mutex_lock(&rbulk->rb_mutex);
	m0_rpc_bulk_qtype(rbulk, M0_NET_QT_ACTIVE_BULK_RECV);
	m0_fom_wait_on(&cp->c_fom, &rbulk->rb_chan, &cp->c_fom.fo_cb);
	m0_mutex_unlock(&rbulk->rb_mutex);

	rc = m0_rpc_bulk_load(rbulk, session->s_conn,
			      sns_cpx->scx_cp.cpx_desc.id_descs,
			      &m0_rpc__buf_bulk_cb);
	if (rc != 0) {
		m0_mutex_lock(&rbulk->rb_mutex);
		m0_fom_callback_cancel(&cp->c_fom.fo_cb);
		m0_mutex_unlock(&rbulk->rb_mutex);
		m0_rpc_bulk_buflist_empty(rbulk);
	}

out:
	if (rc != 0) {
		M0_LOG(M0_ERROR, "recv init failure: %d", rc);
		cp->c_rc = rc;
		m0_fom_phase_set(&cp->c_fom, M0_CCP_RECV_WAIT);
		return M0_FSO_AGAIN;
	}
	return cp->c_ops->co_phase_next(cp);
}

M0_INTERNAL int m0_sns_cm_cp_recv_wait(struct m0_cm_cp *cp)
{
	struct m0_rpc_bulk *rbulk;
	int                 rc;

	M0_PRE(cp != NULL && m0_fom_phase(&cp->c_fom) == M0_CCP_RECV_WAIT);

	rc = cp->c_rc;
	if (rc == 0) {
		rbulk = &cp->c_bulk;
		m0_mutex_lock(&rbulk->rb_mutex);
		rc = rbulk->rb_rc;
		m0_mutex_unlock(&rbulk->rb_mutex);
		if (rc != 0 && rc != -ENODATA) {
			M0_LOG(M0_ERROR, "Bulk recv failed with rc=%d", rc);
			cp->c_rc = rc;
		}
	}

	cp_reply_post(cp);

	if (rc != 0) {
		M0_LOG(M0_ERROR, "recv wait failure: %d", rc);
		m0_fom_phase_move(&cp->c_fom, rc, M0_CCP_FAIL);
		return M0_FSO_AGAIN;
	}
	return cp->c_ops->co_phase_next(cp);
}

M0_INTERNAL int m0_sns_cm_cp_sw_check(struct m0_cm_cp *cp)
{
	struct m0_sns_cm_cp    *scp         = cp2snscp(cp);
	struct m0_fid           cob_fid;
	struct m0_cm           *cm          = cpfom2cm(&cp->c_fom);
	struct m0_cm_proxy     *cm_proxy;
	struct m0_conf_obj     *svc;
	enum m0_cm_state        cm_state;
	const char             *remote_rep;
	int                     rc;
	struct m0_pool_version *pv;

	M0_PRE(cp != NULL && m0_fom_phase(&cp->c_fom) == M0_CCP_SW_CHECK);

	m0_fid_convert_stob2cob(&scp->sc_stob_id, &cob_fid);
	pv = m0_sns_cm_pool_version_get(ag2snsag(cp->c_ag)->sag_fctx);
	if (cp->c_cm_proxy == NULL) {
		remote_rep = m0_sns_cm_tgt_ep(cm, pv, &cob_fid, &svc);
		m0_cm_lock(cm);
		cm_proxy = m0_cm_proxy_locate(cm, remote_rep);
		M0_ASSERT(cm_proxy != NULL);
		cp->c_cm_proxy = cm_proxy;
		m0_cm_unlock(cm);
		m0_confc_close(svc);
	} else
		cm_proxy = cp->c_cm_proxy;

	m0_cm_lock(cm);
	cm_state = m0_cm_state_get(cm);
	m0_cm_unlock(cm);
	/* abort in case of cm failure */
	if (cm_state == M0_CMS_FAIL) {
		m0_fom_phase_move(&cp->c_fom, 0, M0_CCP_FINI);
		return M0_FSO_WAIT;
	}
	m0_cm_proxy_lock(cm_proxy);
	if (m0_cm_ag_id_cmp(&cp->c_ag->cag_id, &cm_proxy->px_sw.sw_lo) >= 0 &&
	    m0_cm_ag_id_cmp(&cp->c_ag->cag_id, &cm_proxy->px_sw.sw_hi) <= 0) {
		rc = cp->c_ops->co_phase_next(cp);
	} else {
		/*
		 * If remote replica has already stopped due to some reason,
		 * all the pending copy packets addressed to that copy machine
		 * must be finalised.
		 */
		if (M0_IN(cm_proxy->px_status, (M0_PX_COMPLETE, M0_PX_STOP, M0_PX_FAILED)) ||
		    m0_cm_ag_id_cmp(&cp->c_ag->cag_id, &cm_proxy->px_sw.sw_lo) < 0) {
			m0_fom_phase_move(&cp->c_fom, -ENOENT, M0_CCP_FAIL);
			rc = M0_FSO_AGAIN;
		} else {
			m0_cm_proxy_cp_add(cm_proxy, cp);
			rc = M0_FSO_WAIT;
		}
	}
	m0_cm_proxy_unlock(cm_proxy);

	return M0_RC(rc);
}

/** @} SNSCMCP */
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
