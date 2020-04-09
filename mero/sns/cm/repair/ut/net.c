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
 * Original creation date: 03/26/2013
 */

#include <unistd.h> /* sleep */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"

#include "sns/cm/repair/ag.h"           /* sag2repairag */
#include "sns/cm/repair/ut/cp_common.h" /* cp_prepare */
#include "ioservice/fid_convert.h"      /* m0_fid_convert_cob2stob */
#include "ioservice/io_service.h"       /* m0_ios_cdom_get */
#include "rpc/rpclib.h"                 /* m0_rpc_client_ctx */
#include "rpc/rpc_opcodes.h"            /* M0_CM_UT_SENDER_OPCODE */
#include "lib/fs.h"                     /* m0_file_read */
#include "lib/finject.h"
#include "ut/ut_rpc_machine.h"          /* m0_ut_rpc_mach_ctx */
#include "ut/stob.h"                    /* m0_ut_stob_create_by_stob_id */
#include "ut/misc.h"                    /* M0_UT_PATH */
#include "ut/ut.h"

#include "sns/cm/repair/ag.c"		/* m0_sns_cm_acc_cp_init */
#include "ha/msg.h"			/* m0_ha_msg */

#define DUMMY_DBNAME      "dummy-db"
#define DUMMY_COB_ID      20
#define DUMMY_SERVER_ADDR "0@lo:12345:34:10"

/* Receiver side. */
static struct m0_reqh            *s0_reqh;
static struct m0_cm              *recv_cm;
static struct m0_sns_cm          *recv_scm;
static struct m0_reqh_service    *scm_service;
static struct m0_cm_aggr_group   *ag_cpy;
static struct m0_sns_cm_repair_ag rag;
static struct m0_sns_cm_file_ctx  fctx;

/*
 * Global structures for read copy packet used for verification.
 * (Receiver side).
 */
static struct m0_sns_cm_repair_ag r_rag;
static struct m0_sns_cm_cp        r_sns_cp;
static struct m0_net_buffer       r_buf;
static struct m0_net_buffer_pool  r_nbp;

/* Sender side. */
enum {
	CLIENT_COB_DOM_ID  = 44,
	MAX_RPCS_IN_FLIGHT = 5,
	STOB_UPDATE_DELAY  = 1,
	MAX_RETRIES        = 5,
	CP_SINGLE          = 1,
	FAIL_NR            = 1,
	BUF_NR             = 4,
	SEG_NR             = 256,
	SEG_SIZE           = 4096,
	START_DATA         = 101,
	DEV_ID             = 1,
	KEY                = 1
};

static struct m0_net_domain  client_net_dom;
static struct m0_net_xprt   *xprt = &m0_net_lnet_xprt;
static struct m0_semaphore   sem;
static struct m0_semaphore   cp_sem;
static struct m0_semaphore   read_cp_sem;
static struct m0_semaphore   write_cp_sem;

static const char client_addr[] = "0@lo:12345:34:2";
static const char server_addr[] = "0@lo:12345:34:1";

static struct m0_rpc_client_ctx cctx = {
	.rcx_net_dom            = &client_net_dom,
	.rcx_local_addr         = client_addr,
	.rcx_remote_addr        = server_addr,
	.rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT,
	.rcx_fid                = &g_process_fid,
};

extern struct m0_cm_type sender_cm_cmt;

static struct m0_ut_rpc_mach_ctx rmach_ctx;
static struct m0_cm              sender_cm;
static struct m0_reqh_service   *sender_cm_service;
static struct m0_cm_cp           sender_cm_cp;
static struct m0_mero            sender_mero = { .cc_pool_width = 10 };
static struct m0_reqh_context    sender_rctx = { .rc_mero = &sender_mero };

/* Global structures for copy packet to be sent (Sender side). */
static struct m0_sns_cm_repair_ag s_rag;
static struct m0_sns_cm_cp        s_sns_cp;
static struct m0_net_buffer_pool  nbp;
static struct m0_cm_proxy        *sender_cm_proxy;
static struct m0_cm_proxy        *recv_cm_proxy;
static struct m0_rpc_conn         conn;
static struct m0_rpc_session      session;

static struct m0_fid gob_fid;
static struct m0_fid cob_fid;

static struct m0_cm_ag_id ag_id = {
	.ai_hi = {
		.u_hi = DEV_ID,
		.u_lo = KEY
	},
	.ai_lo = {
		.u_hi = 0,
		.u_lo = 1
	}
};

static const struct m0_fid      M0_SNS_CM_NET_UT_PVER = M0_FID_TINIT('v', 1, 8);

M0_INTERNAL void cob_create(struct m0_reqh *reqh, struct m0_cob_domain *cdom,
			    struct m0_be_domain *bedom,
			    uint64_t cont, struct m0_fid *gfid,
			    uint32_t cob_idx);
M0_INTERNAL void cob_delete(struct m0_cob_domain *cdom,
			    struct m0_be_domain *bedom,
			    uint64_t cont, const struct m0_fid *gfid);

M0_INTERNAL int m0_sns_cm_repair_cp_send(struct m0_cm_cp *cp);

static void fail_device(struct m0_cm *cm)
{
	struct m0_mero         *mero;
	struct m0_pool_version *pver;
	struct m0_reqh         *reqh;

	reqh = cm->cm_service.rs_reqh;
	mero = m0_cs_ctx_get(reqh);
	pver = m0_pool_version_find(&mero->cc_pools_common, &M0_SNS_CM_NET_UT_PVER);
	M0_UT_ASSERT(pver != NULL);
	pool_mach_transit(reqh, &pver->pv_mach, DEV_ID, M0_PNDS_FAILED);
	pool_mach_transit(reqh, &pver->pv_mach, DEV_ID, M0_PNDS_SNS_REPAIRING);
}

static uint64_t cp_single_get(const struct m0_cm_aggr_group *ag)
{
	return CP_SINGLE;
}

static void cp_ag_fini(struct m0_cm_aggr_group *ag)
{

	M0_PRE(ag != NULL);

	m0_cm_aggr_group_fini(ag);
}

static bool cp_ag_can_fini(const struct m0_cm_aggr_group *ag)
{
	struct m0_sns_cm_repair_ag *rag = sag2repairag(ag2snsag(ag));

	if ((rag->rag_acc_inuse_nr + ag->cag_transformed_cp_nr) ==
	     ag->cag_freed_cp_nr) {
		/*
		 * We wait until accumulator write is complete, before proceeding
		 * for read.
		 */
		if (rag->rag_acc_inuse_nr > 0)
			m0_semaphore_up(&write_cp_sem);
		return true;
	}

	return false;

}

static const struct m0_cm_aggr_group_ops group_ops = {
	.cago_local_cp_nr = &cp_single_get,
	.cago_fini        = &cp_ag_fini,
	.cago_ag_can_fini = &cp_ag_can_fini
};

/* Over-ridden copy packet FOM fini. */
static void dummy_fom_fini(struct m0_fom *fom)
{
	m0_cm_cp_fom_fini(fom);
	m0_semaphore_up(&cp_sem);
}

/* Over-ridden copy packet FOM locality (using single locality). */
static uint64_t dummy_fom_locality(const struct m0_fom *fom)
{
	return 0;
}

/* Over-ridden copy packet FOM tick. */
static int dummy_fom_tick(struct m0_fom *fom)
{
	struct m0_cm_cp *cp = container_of(fom, struct m0_cm_cp, c_fom);

	switch (m0_fom_phase(fom)) {
	case M0_FOM_PHASE_INIT:
		m0_fom_phase_set(fom, M0_CCP_XFORM);
		m0_semaphore_up(&sem);
		return cp->c_ops->co_action[M0_CCP_XFORM](cp);
	case M0_CCP_XFORM:
		m0_fom_phase_set(fom, M0_CCP_SEND);
		return cp->c_ops->co_action[M0_CCP_SEND](cp);
	case M0_CCP_SEND:
		m0_fom_phase_set(fom, M0_CCP_SEND_WAIT);
		return cp->c_ops->co_action[M0_CCP_SEND_WAIT](cp);
	case M0_CCP_SEND_WAIT:
		m0_fom_phase_set(fom, M0_CCP_FINI);
		return M0_FSO_WAIT;
	default:
		M0_IMPOSSIBLE("Bad State");
		return 0;
	}
}

/* Over-ridden copy packet FOM ops. */
static struct m0_fom_ops cp_fom_ops = {
	.fo_fini          = dummy_fom_fini,
	.fo_tick          = dummy_fom_tick,
	.fo_home_locality = dummy_fom_locality,
};

/* Over-ridden copy packet init phase. */
static int dummy_cp_init(struct m0_cm_cp *cp)
{
	/* This is used to ensure that ast has been posted. */
	m0_semaphore_up(&sem);
	return M0_FSO_AGAIN;
}

/* Passthorugh phase for testing purpose. */
static int dummy_cp_phase(struct m0_cm_cp *cp)
{
	return M0_FSO_AGAIN;
}

/* Passthorugh for testing purpose. */
static void dummy_cp_complete(struct m0_cm_cp *cp)
{
}

static uint64_t dummy_home_loc_helper(const struct m0_cm_cp *cp)
{
	return 1;
}

static void cm_cp_free(struct m0_cm_cp *cp)
{
	struct m0_cm_aggr_group *ag = cp->c_ag;

	m0_cm_cp_buf_release(cp);

	if (ag != NULL)
		m0_cm_ag_cp_del(ag, cp);
}

static bool sender_cm_cp_invariant(const struct m0_cm_cp *cp)
{
	return true;
}

static const struct m0_cm_cp_ops sender_cm_cp_ops = {
	.co_invariant = sender_cm_cp_invariant,
	.co_free = cm_cp_free
};

const struct m0_cm_cp_ops cp_dummy_ops = {
	.co_action = {
		[M0_CCP_INIT]      = &dummy_cp_init,
		[M0_CCP_READ]      = &dummy_cp_phase,
		[M0_CCP_WRITE_PRE] = &m0_sns_cm_cp_write_pre,
		[M0_CCP_WRITE]     = &dummy_cp_phase,
		[M0_CCP_IO_WAIT]   = &dummy_cp_phase,
		[M0_CCP_XFORM]     = &dummy_cp_phase,
		[M0_CCP_SW_CHECK]  = &dummy_cp_phase,
		[M0_CCP_SEND]      = &m0_sns_cm_repair_cp_send,
		[M0_CCP_SEND_WAIT] = &m0_sns_cm_cp_send_wait,
		[M0_CCP_RECV_INIT] = &dummy_cp_phase,
		[M0_CCP_RECV_WAIT] = &dummy_cp_phase,
		[M0_CCP_FAIL]      = &m0_sns_cm_cp_fail,
		[M0_CCP_FINI]      = &dummy_cp_phase
	},
	.co_action_nr  = M0_CCP_NR,
	.co_phase_next = &m0_sns_cm_cp_phase_next,
	.co_invariant  = &m0_sns_cm_cp_invariant,
	.co_complete   = &dummy_cp_complete,
	.co_free       = &cm_cp_free,
	.co_home_loc_helper = &dummy_home_loc_helper
};

/* Over-ridden read copy packet FOM tick. */
static int dummy_read_fom_tick(struct m0_fom *fom)
{
	struct m0_cm_cp *cp = container_of(fom, struct m0_cm_cp, c_fom);
	return cp->c_ops->co_action[m0_fom_phase(fom)](cp);
}

/* Over-ridden read copy packet FOM fini. */
static void dummy_read_fom_fini(struct m0_fom *fom)
{
	m0_cm_cp_fini(container_of(fom, struct m0_cm_cp, c_fom));
	m0_cm_lock(recv_cm);
	m0_cm_aggr_group_fini(&r_rag.rag_base.sag_base);
	m0_cm_unlock(recv_cm);
	m0_semaphore_up(&read_cp_sem);
}

/* Over-ridden read copy packet FOM ops. */
static struct m0_fom_ops read_cp_fom_ops = {
	.fo_fini          = dummy_read_fom_fini,
	.fo_tick          = dummy_read_fom_tick,
	.fo_home_locality = dummy_fom_locality,
};

/*
 * Over-ridden copy packet init phase for read copy packet.
 * For unit-test purpose, the epoch checking code is copied from
 * m0_sns_cm_cp_init().
 */
static int dummy_read_cp_init(struct m0_cm_cp *cp)
{
	/* This is used to ensure that ast has been posted. */
	m0_semaphore_up(&sem);
	return cp->c_ops->co_phase_next(cp);
}

/* Passthorugh phase for testing purpose. */
static int dummy_read_cp_phase(struct m0_cm_cp *cp)
{
	if (m0_fom_phase(&cp->c_fom) == M0_CCP_RECV_INIT &&
	    cp->c_epoch != recv_cm_proxy->px_epoch)
		return m0_sns_cm_cp_recv_init(cp);
	return cp->c_ops->co_phase_next(cp);
}

static void buffers_verify()
{
	int  i;
	int  j;
	int  rc;
	int  cnt = 0;
	char str[SEG_SIZE];

	for (i = 0; i < BUF_NR; ++i) {
		for (j = 0; j < SEG_NR; ++j) {
			memset(str, (START_DATA + i), SEG_SIZE);
			rc = memcmp(r_buf.nb_buffer.ov_buf[cnt], str, SEG_SIZE);
			M0_UT_ASSERT(rc == 0);
			cnt++;
	    }
	}
}

/* Passthorugh phase for testing purpose. */
static int dummy_read_cp_xform(struct m0_cm_cp *cp)
{
	buffers_verify();
	return cp->c_ops->co_phase_next(cp);
}

/*
 * Over-ridden copy packet write io wait phase. This is used when read operation
 * of copy packet has to be tested. In this case, write io wait phase will
 * simply be a passthrough phase.
 */
static int dummy_cp_write_io_wait(struct m0_cm_cp *cp)
{
	return cp->c_io_op == M0_CM_CP_WRITE ?
	       cp->c_ops->co_phase_next(cp) :
	       m0_sns_cm_cp_io_wait(cp);
}

/*
 * Over-ridden copy packet write phase. This is used when read operation of
 * copy packet has to be tested. In this case, write phase will simply be a
 * passthrough phase.
 */
static int dummy_cp_write(struct m0_cm_cp *cp)
{
	cp->c_io_op = M0_CM_CP_WRITE;
	cp->c_ops->co_phase_next(cp);
	return M0_FSO_AGAIN;
}

const struct m0_cm_cp_ops read_cp_ops = {
	.co_action = {
		[M0_CCP_INIT]      = &dummy_read_cp_init,
		[M0_CCP_READ]      = &m0_sns_cm_cp_read,
		[M0_CCP_WRITE_PRE] = &m0_sns_cm_cp_write_pre,
		[M0_CCP_WRITE]     = &dummy_cp_write,
		[M0_CCP_IO_WAIT]   = &dummy_cp_write_io_wait,
		[M0_CCP_XFORM]     = &dummy_read_cp_xform,
		[M0_CCP_SW_CHECK]  = &dummy_read_cp_phase,
		[M0_CCP_SEND]      = &dummy_read_cp_phase,
		[M0_CCP_SEND_WAIT] = &dummy_read_cp_phase,
		[M0_CCP_RECV_INIT] = &dummy_read_cp_phase,
		[M0_CCP_RECV_WAIT] = &dummy_read_cp_phase,
		[M0_CCP_FAIL]      = &m0_sns_cm_cp_fail,
		[M0_CCP_FINI]      = &dummy_read_cp_phase
	},
	.co_action_nr  = M0_CCP_NR,
	.co_phase_next = &m0_sns_cm_cp_phase_next,
	.co_invariant  = &m0_sns_cm_cp_invariant,
	.co_complete   = &dummy_cp_complete,
	.co_free       = &cm_cp_free
};

static void ag_setup(struct m0_sns_cm_ag *sag, struct m0_cm *cm)
{
	m0_cm_lock(cm);
	m0_cm_aggr_group_init(&sag->sag_base, cm, &ag_id, false, &group_ops);
	m0_cm_unlock(cm);
	sag->sag_fctx = &fctx;
	sag->sag_base.cag_transformed_cp_nr = 0;
	sag->sag_fnr = FAIL_NR;
	sag->sag_base.cag_cp_global_nr =
		sag->sag_base.cag_cp_local_nr + FAIL_NR;
}

/*
 * Read the copy packet which has completed its fom cycle and ended up
 * writing the data which was sent onwire. After reading, verify the
 * data for correctness.
 */

static void read_and_verify()
{
	char data;

	M0_SET0(&r_rag);
	M0_SET0(&r_sns_cp);
	M0_SET0(&r_buf);
	M0_SET0(&r_nbp);

	m0_semaphore_init(&read_cp_sem, 0);

	ag_setup(&r_rag.rag_base, recv_cm);

	r_buf.nb_pool = &r_nbp;
	/*
	 * Purposefully fill the read bv with spaces i.e. ' '. This should get
	 * replaced by appropriate data, when the data is read.
	 */
	data = ' ';
	cp_prepare(&r_sns_cp.sc_base, &r_buf, SEG_NR * BUF_NR, SEG_SIZE,
		   &r_rag.rag_base, data, &read_cp_fom_ops, s0_reqh, 0, false,
		   recv_cm);

	r_sns_cp.sc_cobfid = cob_fid;
	m0_fid_convert_cob2stob(&cob_fid, &r_sns_cp.sc_stob_id);
	r_sns_cp.sc_index = 0;
	r_sns_cp.sc_base.c_ops = &read_cp_ops;
	m0_fom_queue(&r_sns_cp.sc_base.c_fom);

	m0_semaphore_down(&read_cp_sem);
}

/* Create and add the aggregation group to the list in copy machine. */
static void receiver_ag_create(struct m0_cm *cm)
{
	int                      i;
	struct m0_sns_cm_ag     *sag;
	struct m0_cm_aggr_group *ag;
	struct m0_sns_cm_cp     *sns_cp;

	sag = &rag.rag_base;
	ag_setup(sag, cm);
	ag = &sag->sag_base;
	sag->sag_base.cag_cm = cm;
	sag->sag_base.cag_has_incoming = true;
	sag->sag_local_tgts_nr = 1;
	m0_cm_proxy_in_count_alloc(&sag->sag_proxy_in_count, cm->cm_proxy_nr);
	M0_CNT_INC(sag->sag_proxy_in_count.p_count[recv_cm_proxy->px_id]);
	m0_mutex_init(&ag->cag_mutex);
	aggr_grps_in_tlink_init(ag);
	aggr_grps_out_tlink_init(ag);
	M0_ALLOC_ARR(rag.rag_fc, FAIL_NR);
	M0_UT_ASSERT(rag.rag_fc != NULL);
	for (i = 0; i < sag->sag_fnr; ++i) {
		rag.rag_fc[i].fc_tgt_cobfid = cob_fid;
		rag.rag_fc[i].fc_is_inuse = true;
		M0_CNT_INC(rag.rag_acc_inuse_nr);
		sns_cp = &rag.rag_fc[i].fc_tgt_acc_cp;
		m0_sns_cm_acc_cp_init(sns_cp, sag);
		sns_cp->sc_base.c_data_seg_nr = SEG_NR * BUF_NR;
		m0_fid_convert_cob2stob(&cob_fid, &sns_cp->sc_stob_id);
		sns_cp->sc_cobfid = rag.rag_fc[i].fc_tgt_cobfid;
		sns_cp->sc_is_acc = true;
		m0_cm_lock(cm);
		M0_UT_ASSERT(m0_sns_cm_buf_attach(&recv_scm->sc_ibp.sb_bp, &sns_cp->sc_base) == 0);
		m0_cm_unlock(cm);
	}

	m0_cm_lock(cm);
	m0_cm_aggr_group_add(cm, &sag->sag_base, true);
	ag_cpy = m0_cm_aggr_group_locate(cm, &ag_id, true);
	m0_cm_unlock(cm);
	M0_UT_ASSERT(&sag->sag_base == ag_cpy);
}

static void receiver_stob_create()
{
	struct m0_cob_domain *cdom;
	struct m0_stob_id     stob_id;
	int                   rc;

	m0_ios_cdom_get(s0_reqh, &cdom);
	cob_create(s0_reqh, cdom, s0_reqh->rh_beseg->bs_domain, 0, &gob_fid, 0);

	/*
	 * Create a stob. In actual repair scenario, this will already be
	 * created by the IO path.
	 */
	m0_fid_convert_cob2stob(&cob_fid, &stob_id);
	rc = m0_ut_stob_create_by_stob_id(&stob_id, NULL);
	M0_UT_ASSERT(rc == 0);
}

static void cm_ready(struct m0_cm *cm)
{
	m0_cm_lock(cm);
	m0_cm_state_set(cm, M0_CMS_READY);
	m0_cm_unlock(cm);
}

static void receiver_init(bool ag_create)
{
	int rc;

	M0_SET0(&rag);
	M0_SET0(&fctx);

	rc = m0_cm_type_register(&sender_cm_cmt);
	M0_UT_ASSERT(rc == 0);

	rc = cs_init(&sctx);
	M0_UT_ASSERT(rc == 0);

	s0_reqh = m0_cs_reqh_get(&sctx);
	scm_service = m0_reqh_service_find(
		m0_reqh_service_type_find("M0_CST_SNS_REP"), s0_reqh);
	M0_UT_ASSERT(scm_service != NULL);

	recv_cm = container_of(scm_service, struct m0_cm, cm_service);
	M0_UT_ASSERT(recv_cm != NULL);

	recv_scm = cm2sns(recv_cm);
	recv_scm->sc_op = CM_OP_REPAIR;

	m0_cm_lock(recv_cm);
	recv_cm->cm_epoch = m0_time_now();
	fail_device(recv_cm);
	M0_UT_ASSERT(recv_cm->cm_ops->cmo_prepare(recv_cm) == 0);
	m0_cm_state_set(recv_cm, M0_CMS_PREPARE);

	m0_cm_unlock(recv_cm);

	cm_ready(recv_cm);
	recv_cm->cm_sw_update.swu_is_complete = true;

	receiver_stob_create();
	M0_ALLOC_PTR(recv_cm_proxy);
	M0_UT_ASSERT(recv_cm_proxy != NULL);
	m0_cm_proxy_init(recv_cm_proxy, 0, &ag_id, &ag_id, client_addr);
	m0_cm_lock(recv_cm);
	m0_cm_proxy_add(recv_cm, recv_cm_proxy);
	M0_UT_ASSERT(recv_cm->cm_ops->cmo_start(recv_cm) == 0);
	m0_cm_state_set(recv_cm, M0_CMS_ACTIVE);
	m0_cm_unlock(recv_cm);
	if (ag_create)
		receiver_ag_create(recv_cm);
}

static struct m0_cm_cp* sender_cm_cp_alloc(struct m0_cm *cm)
{
	sender_cm_cp.c_ops = &sender_cm_cp_ops;
	return &sender_cm_cp;
}

static int sender_cm_setup(struct m0_cm *cm)
{
	return 0;
}

static int sender_cm_start(struct m0_cm *cm)
{
	return 0;
}

static void sender_cm_stop(struct m0_cm *cm)
{
}

static int sender_cm_data_next(struct m0_cm *cm, struct m0_cm_cp *cp)
{
	return -ENODATA;
}

static void sender_cm_fini(struct m0_cm *cm)
{
}

static int sender_cm_prepare(struct m0_cm *cm)
{
	return 0;
}

static int sender_cm_ag_next(struct m0_cm *cm,
			     const struct m0_cm_ag_id *id_curr,
			     struct m0_cm_ag_id *id_next)
{
	return -ENODATA;
}

static void sender_cm_ha_msg(struct m0_cm *cm,
			     struct m0_ha_msg *msg, int rc)
{
}

static const struct m0_cm_ops sender_cm_ops = {
	.cmo_setup     = sender_cm_setup,
	.cmo_prepare   = sender_cm_prepare,
	.cmo_start     = sender_cm_start,
	.cmo_stop      = sender_cm_stop,
	.cmo_cp_alloc  = sender_cm_cp_alloc,
	.cmo_data_next = sender_cm_data_next,
	.cmo_ag_next   = sender_cm_ag_next,
	.cmo_ha_msg    = sender_cm_ha_msg,
	.cmo_fini      = sender_cm_fini
};

static int sender_cm_service_start(struct m0_reqh_service *service)
{
	return m0_cm_setup(container_of(service, struct m0_cm, cm_service));
}

static void sender_cm_service_stop(struct m0_reqh_service *service)
{
	struct m0_cm *cm = container_of(service, struct m0_cm, cm_service);
	m0_cm_fini(cm);
}

static void sender_cm_service_fini(struct m0_reqh_service *service)
{
	sender_cm_service = NULL;
	M0_SET0(&sender_cm);
}

static const struct m0_reqh_service_ops sender_cm_service_ops = {
	.rso_start = sender_cm_service_start,
	.rso_stop  = sender_cm_service_stop,
	.rso_fini  = sender_cm_service_fini
};

static int sender_cm_service_allocate(struct m0_reqh_service **service,
				      const struct m0_reqh_service_type *stype)
{
	struct m0_cm *cm = &sender_cm;

	*service = &cm->cm_service;
	(*service)->rs_ops = &sender_cm_service_ops;
	(*service)->rs_sm.sm_state = M0_RST_INITIALISING;

	return m0_cm_init(cm, container_of(stype, struct m0_cm_type, ct_stype),
			  &sender_cm_ops);
}

static const struct m0_reqh_service_type_ops sender_cm_service_type_ops = {
	.rsto_service_allocate = sender_cm_service_allocate
};

M0_CM_TYPE_DECLARE(sender_cm, M0_CM_UT_SENDER_OPCODE,
		   &sender_cm_service_type_ops, "sender_cm", 0);

void sender_service_alloc_init()
{
	int rc;
	/* Internally calls m0_cm_init(). */
	M0_ASSERT(sender_cm_service == NULL);
	rc = m0_reqh_service_allocate(&sender_cm_service,
				      &sender_cm_cmt.ct_stype,
				      &sender_rctx);
	M0_ASSERT(rc == 0);
	m0_reqh_service_init(sender_cm_service, &rmach_ctx.rmc_reqh, NULL);
}

M0_TL_DECLARE(proxy_cp, M0_EXTERN, struct m0_cm_cp);

static void sender_ag_create()
{
	struct m0_sns_cm_ag  *sag;

	M0_SET0(&s_rag);
	sag = &s_rag.rag_base;
	ag_setup(sag, &sender_cm);
	m0_cm_lock(&sender_cm);
	m0_cm_aggr_group_add(&sender_cm, &sag->sag_base, false);
	ag_cpy = m0_cm_aggr_group_locate(&sender_cm, &ag_id, false);
	m0_cm_unlock(&sender_cm);
	M0_UT_ASSERT(&sag->sag_base == ag_cpy);
}

static void bp_below_threshold(struct m0_net_buffer_pool *bp)
{
}

static void buf_available(struct m0_net_buffer_pool *pool)
{
}

const struct m0_net_buffer_pool_ops bp_ops = {
        .nbpo_not_empty       = buf_available,
        .nbpo_below_threshold = bp_below_threshold
};

static void sender_init()
{
	struct m0_confc      *confc;
	struct m0_locality   *locality;
	struct m0_net_domain *ndom;
	struct m0_cm_ag_id    agid0;
	char                 *confstr = NULL;
	uint32_t              colours;
	int                   nr_bufs;
	int                   rc;

	M0_SET0(&rmach_ctx);
	M0_SET0(&sender_cm);
	M0_SET0(&sender_cm_cp);
	M0_SET0(&s_rag);
	M0_SET0(&s_sns_cp);
	M0_SET0(&nbp);
	M0_SET0(&conn);
	M0_SET0(&session);


	rmach_ctx.rmc_cob_id.id = DUMMY_COB_ID;
	rmach_ctx.rmc_ep_addr   = DUMMY_SERVER_ADDR;
	m0_ut_rpc_mach_init_and_add(&rmach_ctx);

	locality = m0_locality0_get();
	sender_service_alloc_init();
	confc = m0_reqh2confc(sender_cm_service->rs_reqh);
	rc = m0_file_read(M0_UT_PATH("diter.xc"), &confstr);
	M0_UT_ASSERT(rc == 0);
	rc = m0_confc_init(confc, locality->lo_grp, NULL, NULL, confstr);
	M0_UT_ASSERT(rc == 0);
	m0_free0(&confstr);

	rc = m0_reqh_service_start(sender_cm_service);
	M0_UT_ASSERT(rc == 0);

	m0_cm_cp_init(&sender_cm_cmt, NULL);
	m0_cm_lock(&sender_cm);
	sender_cm.cm_epoch = m0_time_now();
	M0_UT_ASSERT(sender_cm.cm_ops->cmo_prepare(&sender_cm) == 0);
	m0_cm_state_set(&sender_cm, M0_CMS_PREPARE);

	m0_cm_unlock(&sender_cm);

	cm_ready(&sender_cm);
	ndom = &rmach_ctx.rmc_net_dom;
	colours = m0_reqh_nr_localities(&rmach_ctx.rmc_reqh);
	rc = m0_net_buffer_pool_init(&nbp, ndom, 0, SEG_NR, SEG_SIZE,
				     colours, M0_0VEC_SHIFT, false);
	M0_UT_ASSERT(rc == 0);
	nbp.nbp_ops = &bp_ops;
	m0_net_buffer_pool_lock(&nbp);
        nr_bufs = m0_net_buffer_pool_provision(&nbp, 4);
	m0_net_buffer_pool_unlock(&nbp);
	M0_UT_ASSERT(nr_bufs == 4);
	M0_UT_ASSERT(recv_scm->sc_obp.sb_bp.nbp_buf_nr != 4);

	sender_ag_create();

	sender_cm.cm_sw_update.swu_is_complete = true;

	rc = m0_net_domain_init(&client_net_dom, xprt);
	M0_UT_ASSERT(rc == 0);

	rc = m0_rpc_client_start(&cctx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_client_connect(&conn, &session, &cctx.rcx_rpc_machine,
				   cctx.rcx_remote_addr, NULL,
				   cctx.rcx_max_rpcs_in_flight,
				   M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	M0_ALLOC_PTR(sender_cm_proxy);
	M0_UT_ASSERT(sender_cm_proxy != NULL);
	sender_cm_proxy->px_conn = &conn;
	sender_cm_proxy->px_session = &session;
	M0_SET0(&agid0);
	m0_cm_lock(&sender_cm);
	m0_cm_proxy_init(sender_cm_proxy, 0, &ag_id, &ag_id, m0_rpc_conn_addr(&conn));
	m0_cm_proxy_add(&sender_cm, sender_cm_proxy);
	M0_UT_ASSERT(sender_cm.cm_ops->cmo_start(&sender_cm) == 0);
	m0_cm_state_set(&sender_cm, M0_CMS_ACTIVE);
	m0_cm_unlock(&sender_cm);
}

static void receiver_fini()
{
	struct m0_cob_domain *cdom;
	struct m0_stob_id     stob_id;
	int                   rc;

	recv_cm_proxy->px_is_done = true;
	m0_cm_lock(recv_cm);
	m0_cm_proxy_del(recv_cm, recv_cm_proxy);
	m0_cm_proxy_fini(recv_cm_proxy);
	recv_cm->cm_ops->cmo_stop(recv_cm);
	m0_cm_state_set(recv_cm, M0_CMS_STOP);
	m0_cm_unlock(recv_cm);
	m0_fid_convert_cob2stob(&cob_fid, &stob_id);
	rc = m0_ut_stob_destroy_by_stob_id(&stob_id);
	M0_UT_ASSERT(rc == 0);
	m0_ios_cdom_get(s0_reqh, &cdom);
	cob_delete(cdom, s0_reqh->rh_beseg->bs_domain, 0, &gob_fid);
	m0_free(r_rag.rag_fc);
	cs_fini(&sctx);
	m0_cm_type_deregister(&sender_cm_cmt);
}

static void sender_fini()
{
	struct m0_confc *confc;
	int              rc;

	sender_cm_proxy->px_is_done = true;
	m0_cm_lock(&sender_cm);
	m0_cm_proxy_del(&sender_cm, sender_cm_proxy);
	m0_cm_proxy_fini(sender_cm_proxy);
	sender_cm.cm_ops->cmo_stop(&sender_cm);
	m0_cm_state_set(&sender_cm, M0_CMS_STOP);
	m0_cm_unlock(&sender_cm);
	rc = m0_rpc_session_destroy(&session, M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_conn_destroy(&conn, M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
        rc = m0_rpc_client_stop(&cctx);
        M0_UT_ASSERT(rc == 0);
        m0_net_domain_fini(&client_net_dom);
	m0_reqh_idle_wait(&rmach_ctx.rmc_reqh);
	confc = m0_reqh2confc(sender_cm_service->rs_reqh);
	m0_confc_fini(confc);
	m0_reqh_service_prepare_to_stop(sender_cm_service);
	m0_reqh_service_stop(sender_cm_service);
	m0_reqh_service_fini(sender_cm_service);
	m0_ut_rpc_mach_fini(&rmach_ctx);

	bv_free(&r_buf.nb_buffer);

	m0_semaphore_fini(&sem);
	m0_semaphore_fini(&cp_sem);
	m0_semaphore_fini(&read_cp_sem);
}

static void test_fini()
{
	m0_chan_signal_lock(&sender_cm.cm_complete);
	sender_fini();
	m0_chan_signal_lock(&recv_cm->cm_complete);
	receiver_fini();
}

static void test_init(bool ag_create)
{
	M0_SET0(&rag);
	M0_SET0(&fctx);
	M0_SET0(&r_rag);
	M0_SET0(&r_sns_cp);
	M0_SET0(&r_buf);
	M0_SET0(&r_nbp);
	M0_SET0(&client_net_dom);
	M0_SET0(&sem);
	M0_SET0(&cp_sem);
	M0_SET0(&read_cp_sem);
	M0_SET0(&rmach_ctx);
	M0_SET0(&sender_cm);
	M0_SET0(&sender_cm_cp);
	M0_SET0(&s_rag);
	M0_SET0(&s_sns_cp);
	M0_SET0(&nbp);
	M0_SET0(&conn);
	M0_SET0(&session);
	M0_SET0(&gob_fid);
	M0_SET0(&cob_fid);

	m0_fid_gob_make(&gob_fid, 0, 4);
	m0_fid_convert_gob2cob(&gob_fid, &cob_fid, 0);
	receiver_init(ag_create);
	sender_init();
	recv_cm_proxy->px_epoch   = sender_cm.cm_epoch;
	sender_cm_proxy->px_epoch = recv_cm->cm_epoch;
}

static void test_cp_send_mismatch_epoch()
{
	struct m0_sns_cm_ag   *sag;
	struct m0_net_buffer  *nbuf;
	int                    i;
	char                   data;
	struct m0_pool_version pv;
	struct m0_poolmach     pm;
	m0_time_t              epoch_saved;

	m0_fi_enable("m0_sns_cm_tgt_ep", "local-ep");
	m0_fi_enable("cpp_data_next", "enodata");
	m0_fi_enable("m0_ha_local_state_set", "no_ha");

	test_init(false);
	M0_UT_ASSERT(recv_scm->sc_obp.sb_bp.nbp_buf_nr != 4);

	m0_semaphore_init(&sem, 0);
	m0_semaphore_init(&cp_sem, 0);

	sag = &s_rag.rag_base;
	pm.pm_pver = &pv;
	fctx.sf_pm = &pm;
	sag->sag_fctx = &fctx;
	data = START_DATA;
	m0_net_buffer_pool_lock(&nbp);
	nbuf = m0_net_buffer_pool_get(&nbp, M0_BUFFER_ANY_COLOUR);
	m0_net_buffer_pool_unlock(&nbp);
	cp_prepare(&s_sns_cp.sc_base, nbuf, SEG_NR, SEG_SIZE,
		   sag, data, &cp_fom_ops,
		   sender_cm_service->rs_reqh, 0, false,
		   &sender_cm);
	for (i = 1; i < BUF_NR; ++i) {
		data = i + START_DATA;
		m0_net_buffer_pool_lock(&nbp);
		nbuf = m0_net_buffer_pool_get(&nbp, M0_BUFFER_ANY_COLOUR);
		m0_net_buffer_pool_unlock(&nbp);
		bv_populate(&nbuf->nb_buffer, data, SEG_NR, SEG_SIZE);
		m0_cm_cp_buf_add(&s_sns_cp.sc_base, nbuf);
	}
	m0_tl_for(cp_data_buf, &s_sns_cp.sc_base.c_buffers, nbuf) {
		M0_UT_ASSERT(nbuf != NULL);
	} m0_tl_endfor;

	m0_bitmap_init(&s_sns_cp.sc_base.c_xform_cp_indices,
		       sag->sag_base.cag_cp_global_nr);
	s_sns_cp.sc_base.c_ops = &cp_dummy_ops;
	/* Set some bit to true. */
	m0_bitmap_set(&s_sns_cp.sc_base.c_xform_cp_indices, 1, true);
	M0_CNT_INC(sag->sag_base.cag_transformed_cp_nr);
	s_sns_cp.sc_base.c_cm_proxy = sender_cm_proxy;
	s_sns_cp.sc_cobfid = cob_fid;
	m0_fid_convert_cob2stob(&cob_fid, &s_sns_cp.sc_stob_id);
	s_sns_cp.sc_index = 0;
	s_sns_cp.sc_base.c_data_seg_nr = SEG_NR * BUF_NR;
	s_sns_cp.sc_base.c_ag = &sag->sag_base;
	m0_cm_ag_cp_add(&sag->sag_base, &s_sns_cp.sc_base);
	/* Assume this as accumulator copy packet to be sent on remote side. */
	s_sns_cp.sc_base.c_ag_cp_idx = ~0;

	epoch_saved = recv_cm_proxy->px_epoch;
	recv_cm_proxy->px_epoch = 0x1234567890abcdef;
	m0_fom_queue(&s_sns_cp.sc_base.c_fom);

	/* Wait till ast gets posted. */
	m0_semaphore_down(&sem);
	m0_semaphore_down(&cp_sem);
	sleep(STOB_UPDATE_DELAY);

	recv_cm_proxy->px_epoch = epoch_saved;
	M0_UT_ASSERT(recv_scm->sc_obp.sb_bp.nbp_buf_nr != 4);
	m0_net_buffer_pool_lock(&nbp);
	while (m0_net_buffer_pool_prune(&nbp))
	{;}
	m0_net_buffer_pool_unlock(&nbp);

	test_fini();

	m0_fi_disable("m0_sns_cm_tgt_ep", "local-ep");
	m0_fi_disable("cpp_data_next", "enodata");
	m0_fi_disable("m0_ha_local_state_set", "no_ha");
}

static void test_cp_send_recv_verify()
{
	struct m0_sns_cm_ag      *sag;
	struct m0_net_buffer     *nbuf;
	int                       i;
	char                      data;
	struct m0_pool_version    pv;
	struct m0_poolmach        pm;
	struct m0_pdclust_layout *pdlay;

	m0_fi_enable("m0_sns_cm_tgt_ep", "local-ep");
	m0_fi_enable("cpp_data_next", "enodata");
	m0_fi_enable("m0_ha_local_state_set", "no_ha");
	m0_fi_enable("cp_stob_release_exts", "no-stob-punch");

	test_init(true);
	M0_UT_ASSERT(recv_scm->sc_obp.sb_bp.nbp_buf_nr != 4);

	m0_semaphore_init(&sem, 0);
	m0_semaphore_init(&cp_sem, 0);
	m0_semaphore_init(&write_cp_sem, 0);

	layout_gen(&pdlay, s0_reqh);
	sag = &s_rag.rag_base;
	pm.pm_pver = &pv;
	fctx.sf_pm = &pm;
	fctx.sf_layout = m0_pdl_to_layout(pdlay);
	sag->sag_fctx = &fctx;
	data = START_DATA;
        m0_net_buffer_pool_lock(&nbp);
	nbuf = m0_net_buffer_pool_get(&nbp, M0_BUFFER_ANY_COLOUR);
        m0_net_buffer_pool_unlock(&nbp);
	cp_prepare(&s_sns_cp.sc_base, nbuf, SEG_NR, SEG_SIZE,
		   sag, data, &cp_fom_ops,
		   sender_cm_service->rs_reqh, 0, false,
		   &sender_cm);
	for (i = 1; i < BUF_NR; ++i) {
		data = i + START_DATA;
		m0_net_buffer_pool_lock(&nbp);
		nbuf = m0_net_buffer_pool_get(&nbp, M0_BUFFER_ANY_COLOUR);
		m0_net_buffer_pool_unlock(&nbp);
		bv_populate(&nbuf->nb_buffer, data, SEG_NR, SEG_SIZE);
		m0_cm_cp_buf_add(&s_sns_cp.sc_base, nbuf);
	}
	m0_tl_for(cp_data_buf, &s_sns_cp.sc_base.c_buffers, nbuf) {
		M0_UT_ASSERT(nbuf != NULL);
	} m0_tl_endfor;

	m0_bitmap_init(&s_sns_cp.sc_base.c_xform_cp_indices,
		       sag->sag_base.cag_cp_global_nr);
	s_sns_cp.sc_base.c_ops = &cp_dummy_ops;
	/* Set some bit to true. */
	m0_bitmap_set(&s_sns_cp.sc_base.c_xform_cp_indices, 1, true);
	M0_CNT_INC(sag->sag_base.cag_transformed_cp_nr);
	s_sns_cp.sc_base.c_cm_proxy = sender_cm_proxy;
	s_sns_cp.sc_cobfid = cob_fid;
	m0_fid_convert_cob2stob(&cob_fid, &s_sns_cp.sc_stob_id);
	s_sns_cp.sc_index = 0;
	s_sns_cp.sc_base.c_data_seg_nr = SEG_NR * BUF_NR;
	s_sns_cp.sc_base.c_ag = &sag->sag_base;
	m0_cm_ag_cp_add(&sag->sag_base, &s_sns_cp.sc_base);
	/* Assume this as accumulator copy packet to be sent on remote side. */
	s_sns_cp.sc_base.c_ag_cp_idx = ~0;
	s_sns_cp.sc_base.c_epoch = sender_cm.cm_epoch;

	m0_fom_queue(&s_sns_cp.sc_base.c_fom);

	/* Wait till ast gets posted. */
	m0_semaphore_down(&sem);
	m0_semaphore_down(&cp_sem);
	m0_semaphore_down(&write_cp_sem);
	sleep(STOB_UPDATE_DELAY);

	read_and_verify();

	M0_UT_ASSERT(recv_scm->sc_obp.sb_bp.nbp_buf_nr != 4);
	m0_net_buffer_pool_lock(&nbp);
        while (m0_net_buffer_pool_prune(&nbp))
        {;}
        m0_net_buffer_pool_unlock(&nbp);

	m0_semaphore_fini(&write_cp_sem);

	layout_destroy(pdlay);
	test_fini();

	m0_fi_disable("cp_stob_release_exts", "no-stob-punch");
	m0_fi_disable("m0_sns_cm_tgt_ep", "local-ep");
	m0_fi_disable("cpp_data_next", "enodata");
	m0_fi_disable("m0_ha_local_state_set", "no_ha");
}

struct m0_ut_suite snscm_net_ut = {
	.ts_name = "snscm_net-ut",
	.ts_tests = {
		{ "cp-send-recv-verify", test_cp_send_recv_verify },
		{ "cp-send-mismatched-epoch", test_cp_send_mismatch_epoch },
		{ NULL, NULL }
	}
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
