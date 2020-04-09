/* -*- C -*- */
/*
* COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
* Original author: Nachiket Sahasrabudhe <nachiket.sahasrabuddhe@seagate.com>
* Original creation date: 22-June-2016
*/

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"         /* M0_ERR_INFO */
#include "lib/chan.h"
#include "lib/memory.h"        /* M0_ALLOC_PTR */
#include "lib/errno.h"         /* ENOMEM */
#include "lib/finject.h"       /* M0_FI_ENABLED */
#include "module/instance.h"   /* m0_get */
#include "ha/failvec.h"
#include "ha/msg.h"            /* struct m0_ha_msg */
#include "ha/link.h"           /* m0_ha_link */
#include "ha/note.h"           /* m0_ha_note_handler_msg */
#include "pool/pool_machine.h" /* m0_poolmach_failvec_apply */
#include "pool/pool.h"
#include "reqh/reqh.h"
#include "fid/fid.h"

struct ha_fvec_handler_request {
	struct m0_chan     *fhr_chan;
	struct m0_tlink     fhr_tlink;
	/* unique identifier for the pool. */
	struct m0_fid       fhr_fid;
	/* Pool-machine to be updated on failvec. */
	struct m0_poolmach *fhr_pmach;
	/* Generation count for cookie. */
	uint64_t            fhr_gen;
	uint64_t            fhr_magic;
};

static uint32_t fvec_rep_msg_vec_len(const struct m0_ha_msg *msg)
{
	return msg->hm_data.u.hed_fvec_rep.mfp_nr;
}

static const struct m0_cookie*
		fvec_msg_cookie_get(const struct m0_ha_msg *msg)
{
	return msg->hm_data.hed_type == M0_HA_MSG_FAILURE_VEC_REQ ?
		&msg->hm_data.u.hed_fvec_req.mfq_cookie :
		  &msg->hm_data.u.hed_fvec_rep.mfp_cookie;
}
static const struct m0_ha_note * fvec_rep_msg_to_note(const struct m0_ha_msg
						      *msg)
{
	return msg->hm_data.u.hed_fvec_rep.mfp_vec.hmna_arr;
}
static const struct m0_fid*
		fvec_msg_to_pool(const struct m0_ha_msg *msg)
{
	return &msg->hm_data.u.hed_fvec_req.mfq_pool;
}

static uint32_t fvec_msg_type(const struct m0_ha_msg *msg)
{
	return msg->hm_data.hed_type;
}
M0_TL_DESCR_DEFINE(ha_fvreq, "failure-vec-req", static,
		   struct ha_fvec_handler_request, fhr_tlink, fhr_magic,
		   20, 21);               /* XXX */
M0_TL_DEFINE(ha_fvreq, static, struct ha_fvec_handler_request);

static void ha_fvec_handler_msg(struct m0_ha_handler *hh,
                                struct m0_ha         *ha,
                                struct m0_ha_link    *hl,
                                struct m0_ha_msg     *msg,
                                uint64_t              tag,
                                void                 *data)
{
	struct m0_ha_fvec_handler *hfh;

	hfh = container_of(hh, struct m0_ha_fvec_handler, hfh_handler);
	M0_ASSERT(hfh == data);

	switch (fvec_msg_type(msg)) {
	case M0_HA_MSG_FAILURE_VEC_REQ:
		m0_ha_fvec_req_handler(hfh, msg, hl);
		break;
	case M0_HA_MSG_FAILURE_VEC_REP:
		m0_ha_fvec_rep_handler(hfh, msg);
		break;
	default:
		M0_LOG(M0_DEBUG, "fvec handler being evoked for non-fvec msg");
		break;
	}
}

M0_INTERNAL int m0_ha_fvec_handler_init(struct m0_ha_fvec_handler *hfh,
                                        struct m0_ha_dispatcher *hd)
{
	M0_PRE(M0_IS0(hfh));

	m0_mutex_init(&hfh->hfh_lock);
	ha_fvreq_tlist_init(&hfh->hfh_fvreq);
	hfh->hfh_dispatcher = hd;
	hfh->hfh_handler = (struct m0_ha_handler) {
		.hh_data            = hfh,
		.hh_msg_received_cb = &ha_fvec_handler_msg,
	};
	m0_ha_dispatcher_attach(hfh->hfh_dispatcher, &hfh->hfh_handler);
	M0_ASSERT(m0_get()->i_fvec_handler == NULL);
	m0_get()->i_fvec_handler = hfh;
	return 0;
}

M0_INTERNAL void m0_ha_fvec_handler_fini(struct m0_ha_fvec_handler *hfh)
{
	M0_ASSERT(m0_get()->i_fvec_handler == hfh);
	m0_get()->i_fvec_handler = NULL;
	m0_ha_dispatcher_detach(hfh->hfh_dispatcher, &hfh->hfh_handler);
	ha_fvreq_tlist_fini(&hfh->hfh_fvreq);
	m0_mutex_fini(&hfh->hfh_lock);
}

M0_INTERNAL int m0_ha_fvec_handler_add(struct m0_ha_fvec_handler *hfh,
                                       const struct m0_fid *pool_fid,
				       struct m0_poolmach *pool_mach,
				       struct m0_chan *chan,
				       struct m0_cookie *cookie)
{
	struct ha_fvec_handler_request *fvec_req;

	M0_ENTRY("pool_fid"FID_F"PVER fid"FID_F, FID_P(pool_fid),
			FID_P(&pool_mach->pm_pver->pv_id));

	M0_PRE(hfh != NULL);

	M0_ALLOC_PTR(fvec_req);
	if (fvec_req == NULL)
		return M0_ERR_INFO(-ENOMEM, "No sufficient memory to"
				   "create a failvec fetch request");
	m0_mutex_lock(&hfh->hfh_lock);
	*fvec_req = (struct ha_fvec_handler_request) {
		.fhr_chan  = chan,
		.fhr_fid   = *pool_fid,
		.fhr_pmach = pool_mach,
	};
	m0_cookie_new(&fvec_req->fhr_gen);
	m0_cookie_init(cookie, &fvec_req->fhr_gen);
	ha_fvreq_tlink_init_at_tail(fvec_req, &hfh->hfh_fvreq);
	m0_mutex_unlock(&hfh->hfh_lock);
	return M0_RC(0);
}

M0_INTERNAL void m0_ha_fvec_rep_handler(struct m0_ha_fvec_handler *hfh,
					const struct m0_ha_msg *msg)
{
	struct ha_fvec_handler_request *fhr;
	struct m0_cookie                cookie;
	struct m0_ha_nvec               nvec;

	M0_PRE(fvec_msg_type(msg) == M0_HA_MSG_FAILURE_VEC_REP);
	m0_mutex_lock(&hfh->hfh_lock);
	cookie = *(fvec_msg_cookie_get(msg));
	fhr = m0_cookie_of(&cookie, struct ha_fvec_handler_request,
			   fhr_gen);
	M0_ASSERT(fhr != NULL);
	ha_fvreq_tlink_del_fini(fhr);
	m0_mutex_unlock(&hfh->hfh_lock);
	nvec.nv_nr   = fvec_rep_msg_vec_len(msg);
	nvec.nv_note = (struct m0_ha_note *)fvec_rep_msg_to_note(msg);
	M0_LOG(M0_DEBUG, "pool_fid"FID_F"PVER fid"FID_F"note length:%d",
			FID_P(&fhr->fhr_fid),
			FID_P(&fhr->fhr_pmach->pm_pver->pv_id),
			nvec.nv_nr);
	m0_poolmach_failvec_apply(fhr->fhr_pmach, &nvec);
	m0_chan_signal_lock(fhr->fhr_chan);
	m0_free(fhr);
}

M0_INTERNAL void m0_ha_fvec_req_handler(struct m0_ha_fvec_handler *hfh,
					const struct m0_ha_msg *msg,
					struct m0_ha_link *hl)
{
	int                             rc;
	/* This code is never expected to get exercised in production
	 * environment.  It's a place holder to placate various UT's that
	 * indirectly send ha message to fetch failure vector.
	 */
	M0_PRE(fvec_msg_type(msg) == M0_HA_MSG_FAILURE_VEC_REQ);
	M0_PRE(hl != NULL);
	rc = m0_ha_msg_fvec_send(fvec_msg_to_pool(msg),
				 fvec_msg_cookie_get(msg), hl,
			         M0_HA_MSG_FAILURE_VEC_REP);
	M0_ASSERT(rc == 0);
}

M0_INTERNAL int m0_ha_failvec_fetch(const struct m0_fid *pool_fid,
				    struct m0_poolmach *pmach,
				    struct m0_chan *chan)
{
	struct m0_cookie req_cookie;
	int              rc;

	M0_PRE(pool_fid != NULL && pmach != NULL && chan != NULL);

	if (M0_FI_ENABLED("kernel-ut-no-ha")) {
			m0_chan_signal_lock(chan);
			return M0_RC(0);
	}
	rc = m0_ha_fvec_handler_add(m0_get()->i_fvec_handler,
				    pool_fid, pmach, chan, &req_cookie);
	if (rc != 0)
		return M0_RC(rc);
	rc = m0_ha_msg_fvec_send(pool_fid, &req_cookie, m0_get()->i_ha_link,
				 M0_HA_MSG_FAILURE_VEC_REQ);
	return M0_RC(rc);
}

static void ha_ut_fvec_reply_populate(struct m0_ha_msg *msg)
{
        struct m0_ha_note *note;
        uint32_t           nv_nr = 10;
        uint32_t           key;
        uint32_t           states[] = {M0_NC_FAILED, M0_NC_REPAIR,
                                       M0_NC_REPAIRED, M0_NC_REBALANCE};
        m0_time_t          seed;

        seed = m0_time_now();
        note = msg->hm_data.u.hed_fvec_rep.mfp_vec.hmna_arr;
        for (key = 0; key < nv_nr; ++key) {
                note[key].no_id = M0_FID_TINIT('d', 1, key);
                if (key < nv_nr / 2)
                        note[key].no_state = M0_NC_FAILED;
                else
                        note[key].no_state = states[m0_rnd(ARRAY_SIZE(states),
                                                           &seed)];
        }
        msg->hm_data.u.hed_fvec_rep.mfp_nr = nv_nr;
}

static void ha_msg_fvec_build(struct m0_ha_msg *msg,
			      const struct m0_fid *pool_fid,
			      struct m0_ha_link *hl)
{
	struct m0_reqh    *reqh = hl->hln_cfg.hlc_reqh;
	struct m0_pool    *pool;
	struct m0_pooldev *pd;
	struct m0_ha_note *note;
	uint32_t           note_nr = 0;

	pool = m0_pool_find(reqh->rh_pools, pool_fid);
	M0_ASSERT(pool != NULL);
	note = msg->hm_data.u.hed_fvec_rep.mfp_vec.hmna_arr;
	m0_tl_for(pool_failed_devs, &pool->po_failed_devices, pd) {
		M0_LOG(M0_DEBUG, "adding nvec for "FID_F" state: %d",
			FID_P(&pd->pd_id), pd->pd_state);
		note[note_nr].no_id = pd->pd_id;
		note[note_nr].no_state = pd->pd_state;
		M0_CNT_INC(note_nr);
	} m0_tl_endfor;
	msg->hm_data.u.hed_fvec_rep.mfp_nr = note_nr;
}

M0_INTERNAL int m0_ha_msg_fvec_send(const struct m0_fid *pool_fid,
				    const struct m0_cookie *cookie,
				    struct m0_ha_link *hl,
				    uint32_t  type)
{
	struct m0_ha_msg *msg;
	uint64_t          tag;

	M0_PRE(M0_IN(type, (M0_HA_MSG_FAILURE_VEC_REQ,
			    M0_HA_MSG_FAILURE_VEC_REP)));
	M0_PRE(hl != NULL);

	M0_ALLOC_PTR(msg);
	if (msg == NULL)
		return M0_ERR(-ENOMEM);
	msg->hm_data.hed_type = type;
	if (type == M0_HA_MSG_FAILURE_VEC_REQ) {
		msg->hm_data.u.hed_fvec_req.mfq_pool = *pool_fid;
		msg->hm_data.u.hed_fvec_req.mfq_cookie = *cookie;
	} else {
		msg->hm_data.u.hed_fvec_rep.mfp_pool = *pool_fid;
		msg->hm_data.u.hed_fvec_rep.mfp_cookie = *cookie;
	}

	if (type == M0_HA_MSG_FAILURE_VEC_REP) {
		if (M0_FI_ENABLED("non-trivial-fvec"))
			ha_ut_fvec_reply_populate(msg);
		else {
			/*
			 * Failur vector is returned by Mero procress only
			 * in system tests by a m0d that mocks Halon.
			 * In production failure vector must be fetched from
			 * Halon only. Here we do not guard against the
			 * concurrent device transtions and sending the failure
			 * vector.
			 */
			ha_msg_fvec_build(msg, pool_fid, hl);
		}
	}
	m0_ha_link_send(hl, msg, &tag);
	m0_free(msg);
	return M0_RC(0);
}

#undef M0_TRACE_SUBSYSTEM
