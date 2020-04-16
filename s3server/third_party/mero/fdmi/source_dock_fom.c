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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include "lib/memory.h"
#include "rpc/rpc_opcodes.h"  /* M0_FDMI_SOURCE_DOCK_OPCODE */
#include "fop/fom_generic.h" /* m0_rpc_item_generic_reply_rc */
#include "fdmi/fdmi.h"
#include "fdmi/source_dock.h"
#include "fdmi/source_dock_internal.h"
#include "fdmi/fops.h"

static void fdmi_sd_fom_fini(struct m0_fom *fom);
static int fdmi_sd_fom_tick(struct m0_fom *fom);
static size_t fdmi_sd_fom_locality(const struct m0_fom *fom);
static int sd_fom_send_record(struct fdmi_sd_fom *sd_fom,
			      struct m0_fop      *fop,
			      const char         *ep);
static int sd_fom_process_matched_filters(struct m0_fdmi_src_dock *sd_ctx,
					  struct m0_fdmi_src_rec  *src_rec);
static int fdmi_filter_calc(struct fdmi_sd_fom         *sd_fom,
			    struct m0_fdmi_src_rec     *src_rec,
			    struct m0_conf_fdmi_filter *fdmi_filter);

static int fdmi_rr_fom_create(struct m0_fop *fop, struct m0_fom **out,
			      struct m0_reqh *reqh);
static void fdmi_rr_fom_fini(struct m0_fom *fom);
static int fdmi_rr_fom_tick(struct m0_fom *fom);

static void fdmi_rec_notif_replied(struct m0_rpc_item *item);

static const struct m0_rpc_item_ops fdmi_rec_not_item_ops = {
	.rio_replied = fdmi_rec_notif_replied
};

struct fdmi_pending_fop {
	uint64_t               fti_magic;
	struct m0_fop         *fti_fop;
	struct m0_tlink        fti_linkage;
	struct m0_clink        fti_clink;
	struct m0_rpc_session *fti_session;
	struct fdmi_sd_fom    *sd_fom;
};

M0_TL_DESCR_DEFINE(pending_fops, "pending fops list", M0_INTERNAL,
		   struct fdmi_pending_fop, fti_linkage, fti_magic,
		   M0_FDMI_SRC_DOCK_PENDING_FOP_MAGIC,
		   M0_FDMI_SRC_DOCK_PENDING_FOP_HEAD_MAGIC);

M0_TL_DEFINE(pending_fops, static, struct fdmi_pending_fop);

/*
 ******************************************************************************
 * FDMI Source Dock: Main FOM
 ******************************************************************************
 */

enum fdmi_src_dock_fom_phase {
	FDMI_SRC_DOCK_FOM_PHASE_INIT = M0_FOM_PHASE_INIT,
	FDMI_SRC_DOCK_FOM_PHASE_FINI = M0_FOM_PHASE_FINISH,
	FDMI_SRC_DOCK_FOM_PHASE_WAIT = M0_FOM_PHASE_NR,
	FDMI_SRC_DOCK_FOM_PHASE_GET_REC
};

static struct m0_sm_state_descr fdmi_src_dock_state_descr[] = {
	[FDMI_SRC_DOCK_FOM_PHASE_INIT] = {
		.sd_flags       = M0_SDF_INITIAL,
		.sd_name        = "Init",
		.sd_allowed     = M0_BITS(FDMI_SRC_DOCK_FOM_PHASE_WAIT)
	},
	[FDMI_SRC_DOCK_FOM_PHASE_WAIT] = {
		.sd_flags       = 0,
		.sd_name        = "WaitRec",
		.sd_allowed     = M0_BITS(FDMI_SRC_DOCK_FOM_PHASE_WAIT,
					  FDMI_SRC_DOCK_FOM_PHASE_GET_REC)
	},
	[FDMI_SRC_DOCK_FOM_PHASE_GET_REC] = {
		.sd_flags       = 0,
		.sd_name        = "GetRec",
		.sd_allowed     = M0_BITS(FDMI_SRC_DOCK_FOM_PHASE_WAIT,
					  FDMI_SRC_DOCK_FOM_PHASE_FINI)
	},
	[FDMI_SRC_DOCK_FOM_PHASE_FINI] = {
		.sd_flags       = M0_SDF_TERMINAL,
		.sd_name        = "Fini",
		.sd_allowed     = 0
	}
};


static struct m0_sm_conf fdmi_src_dock_fom_sm_conf = {
	.scf_name = "fdmi-src-dock-fom-sm",
	.scf_nr_states = ARRAY_SIZE(fdmi_src_dock_state_descr),
	.scf_state = fdmi_src_dock_state_descr
};


static const struct m0_fom_ops fdmi_sd_fom_ops = {
	.fo_fini          = fdmi_sd_fom_fini,
	.fo_tick          = fdmi_sd_fom_tick,
	.fo_home_locality = fdmi_sd_fom_locality
};

static const struct m0_fom_type_ops fdmi_sd_fom_type_ops = {
};

static struct m0_fom_type fdmi_sd_fom_type;

/*
 ******************************************************************************
 * FDMI Source Dock: Release Record FOP Handling FOM
 ******************************************************************************
 */

enum fdmi_rr_fom_phase {
	FDMI_RR_FOM_PHASE_INIT = M0_FOM_PHASE_INIT,
	FDMI_RR_FOM_PHASE_FINI = M0_FOM_PHASE_FINISH,
	FDMI_RR_FOM_PHASE_RELEASE_RECORD = M0_FOM_PHASE_NR
};

static struct m0_sm_state_descr fdmi_rr_fom_state_descr[] = {
	[FDMI_RR_FOM_PHASE_INIT] = {
		.sd_flags    = M0_SDF_INITIAL,
		.sd_name     = "Init",
		.sd_allowed  = M0_BITS(FDMI_RR_FOM_PHASE_FINI)
	},
	[FDMI_RR_FOM_PHASE_FINI] = {
		.sd_flags       = M0_SDF_TERMINAL,
		.sd_name        = "Fini",
		.sd_allowed     = 0
	}
};

/* extern */ const struct m0_sm_conf fdmi_rr_fom_sm_conf = {
	.scf_name      = "fdmi-rr-fom-sm",
	.scf_nr_states = ARRAY_SIZE(fdmi_rr_fom_state_descr),
	.scf_state     = fdmi_rr_fom_state_descr
};

/* extern */ const struct m0_fom_type_ops fdmi_rr_fom_type_ops = {
	.fto_create = fdmi_rr_fom_create
};

const struct m0_fom_ops fdmi_rr_fom_ops = {
	.fo_fini          = fdmi_rr_fom_fini,
	.fo_tick          = fdmi_rr_fom_tick,
	/** @todo Phase 2: check if the same locality func may be used */
	.fo_home_locality = fdmi_sd_fom_locality
};

/*
 ******************************************************************************
 * FDMI Source Dock Main FOM specific functions
 ******************************************************************************
 */

M0_INTERNAL void m0_fdmi__src_dock_fom_init(void)
{
	M0_ENTRY();
	m0_fom_type_init(&fdmi_sd_fom_type, M0_FDMI_SOURCE_DOCK_OPCODE,
			 &fdmi_sd_fom_type_ops, &m0_fdmi_service_type,
			 &fdmi_src_dock_fom_sm_conf);
	M0_LEAVE();
}

M0_INTERNAL int
m0_fdmi__src_dock_fom_start(struct m0_fdmi_src_dock *src_dock,
			    const struct m0_filterc_ops *filterc_ops,
			    struct m0_reqh *reqh)
{
	enum { MAX_RPCS_IN_FLIGHT = 32 };
	struct fdmi_sd_fom    *sd_fom = &src_dock->fsdc_sd_fom;
	struct m0_fom         *fom    = &sd_fom->fsf_fom;
	struct m0_rpc_machine *rpc_mach;
	int                    rc;

	M0_ENTRY();

	m0_semaphore_init(&sd_fom->fsf_shutdown, 0);
	rpc_mach = m0_reqh_rpc_mach_tlist_head(&reqh->rh_rpc_machines);
	M0_SET0(&sd_fom->fsf_conn_pool);
	rc = m0_rpc_conn_pool_init(&sd_fom->fsf_conn_pool, rpc_mach,
				   M0_TIME_NEVER, /* connection timeout */
				   MAX_RPCS_IN_FLIGHT);
	if (rc != 0)
		return M0_ERR(rc);
	M0_SET0(&sd_fom->fsf_filter_ctx);
	m0_filterc_ctx_init(&sd_fom->fsf_filter_ctx, filterc_ops);
	rc = sd_fom->fsf_filter_ctx.fcc_ops->fco_start(&sd_fom->fsf_filter_ctx,
						       reqh);
	if (rc != 0) {
		/**
		 * @todo FDMI service can't work without filterc.
		 * inform ADDB on critical error.
		 */
		M0_LOG(M0_WARN, "Cannot start filterc %d", rc);
		m0_rpc_conn_pool_fini(&sd_fom->fsf_conn_pool);
		m0_filterc_ctx_fini(&sd_fom->fsf_filter_ctx);
		return M0_ERR(rc);
	}
	m0_fdmi_eval_init(&sd_fom->fsf_flt_eval);
	m0_mutex_init(&sd_fom->fsf_pending_fops_lock);
	pending_fops_tlist_init(&sd_fom->fsf_pending_fops);
	m0_fom_init(fom, &fdmi_sd_fom_type, &fdmi_sd_fom_ops, NULL, NULL, reqh);
	m0_fom_queue(fom);
	return M0_RC(0);
}

static void wakeup_iff_waiting(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_fom *fom = ast->sa_datum;

	M0_ENTRY();
	if (m0_fom_is_waiting(fom))
		m0_fom_ready(fom);
	M0_LEAVE();
}

M0_INTERNAL void m0_fdmi__src_dock_fom_wakeup(struct fdmi_sd_fom *sd_fom)
{
	struct m0_fom *fom;

	M0_ENTRY("sd_fom %p", sd_fom);
	M0_PRE(sd_fom != NULL);

	fom = &sd_fom->fsf_fom;

	/**
	 * FOM can be uninitialized here, because posting
	 * is allowed even if FDMI service is not started
	 * @todo Small possibility of races exist (Phase 2).
	 */
	if (fom == NULL || fom->fo_loc == NULL) {
		M0_LEAVE("FDMI FOM is not initialized yet");
		return;
	}
	if (sd_fom->fsf_wakeup_ast.sa_next == NULL) {
		sd_fom->fsf_wakeup_ast = (struct m0_sm_ast) {
			.sa_cb    = wakeup_iff_waiting,
			.sa_datum = fom
		};
		m0_sm_ast_post(&fom->fo_loc->fl_group, &sd_fom->fsf_wakeup_ast);
	}
	M0_LEAVE();
}

M0_INTERNAL void
m0_fdmi__src_dock_fom_stop(struct m0_fdmi_src_dock *src_dock)
{
	M0_ENTRY();

	/* Wake up FOM, so it can stop itself */
	m0_fdmi__src_dock_fom_wakeup(&src_dock->fsdc_sd_fom);
	/* Wait for fom finished */
	m0_semaphore_down(&src_dock->fsdc_sd_fom.fsf_shutdown);
	m0_semaphore_fini(&src_dock->fsdc_sd_fom.fsf_shutdown);

	M0_LEAVE();
}

static size_t fdmi_sd_fom_locality(const struct m0_fom *fom)
{
	return 1;
}

static int apply_filters(struct fdmi_sd_fom     *sd_fom,
			 struct m0_fdmi_src_rec *src_rec)
{
	struct m0_fom              *fom = &sd_fom->fsf_fom;
	struct m0_filterc_ctx      *filterc = &sd_fom->fsf_filter_ctx;
	struct m0_conf_fdmi_filter *fdmi_filter;
	int                         matched;
	int                         rc = 0;
	int                         ret;

	M0_ENTRY("sd_fom %p, src_rec %p", sd_fom, src_rec);
	M0_PRE(m0_fdmi__record_is_valid(src_rec));

	do {
		/* @todo fco_get_next shouldn't block (phase 2) */
		m0_fom_block_enter(fom);
		ret = filterc->fcc_ops->fco_get_next(&sd_fom->fsf_filter_iter,
						     &fdmi_filter);
		m0_fom_block_leave(fom);
		if (ret > 0) {
			matched = fdmi_filter_calc(sd_fom, src_rec,
						   fdmi_filter);
			src_rec->fsr_matched = (matched > 0);
			if (matched < 0) {
				/**
				 * @todo Mark FDMI filter as invalid
				 * (send HA not?) (phase 2)
				 */
			} else if (matched && !src_rec->fsr_dryrun) {
				/**
				 * This list is accessed and modified
				 * from thread at a time. No protection
				 * needed.
				 */
				fdmi_matched_filter_list_tlink_init_at(
					fdmi_filter, &src_rec->fsr_filter_list);
			}
		} else if (ret < 0) {
			rc = ret;
		}
	} while (ret > 0);
	return M0_RC(rc);
}

static int process_fdmi_rec(struct fdmi_sd_fom *sd_fom,
			    struct m0_fdmi_src_rec *src_rec)
{
	struct m0_fom         *fom = &sd_fom->fsf_fom;
	struct m0_filterc_ctx *filterc = &sd_fom->fsf_filter_ctx;
	int                    ret;

	M0_ENTRY("sd_fom %p, src_rec %p", sd_fom, src_rec);
	M0_PRE(m0_fdmi__record_is_valid(src_rec));

	M0_LOG(M0_DEBUG, "FDMI record id = "U128X_F,
	       U128_P(&src_rec->fsr_rec_id));
	/*
	 * Inform source that posted fdmi record handling started, call
	 * fs_begin()
	 */
	m0_fdmi__fs_begin(src_rec);

	m0_fom_block_enter(fom);
	ret = filterc->fcc_ops->fco_open(filterc,
					 m0_fdmi__sd_rec_type_id_get(src_rec),
					 &sd_fom->fsf_filter_iter);
	m0_fom_block_leave(fom);

	if (ret == 0) {
		ret = apply_filters(sd_fom, src_rec);
		filterc->fcc_ops->fco_close(&sd_fom->fsf_filter_iter);
	}
	return M0_RC(ret);
}

static void fdmi_sd_fom_fini(struct m0_fom *fom)
{
	struct fdmi_sd_fom    *sd_fom = M0_AMB(sd_fom, fom, fsf_fom);
	struct m0_filterc_ctx *filterc_ctx = &sd_fom->fsf_filter_ctx;

	M0_ENTRY("fom %p", fom);

	M0_LOG(M0_DEBUG, "deinit filterc ctx");
	filterc_ctx->fcc_ops->fco_stop(filterc_ctx);
	m0_filterc_ctx_fini(filterc_ctx);

	m0_fdmi_eval_fini(&sd_fom->fsf_flt_eval);

	m0_rpc_conn_pool_fini(&sd_fom->fsf_conn_pool);
	m0_mutex_fini(&sd_fom->fsf_pending_fops_lock);
	pending_fops_tlist_fini(&sd_fom->fsf_pending_fops);
	m0_semaphore_up(&sd_fom->fsf_shutdown);
	m0_fom_fini(fom);

	M0_LEAVE();
}

static int fdmi_post_fop(struct m0_fop *fop, struct m0_rpc_session *session)
{
	struct m0_rpc_item *item;

	M0_ENTRY("fop: %p, session: %p", fop, session);

	item                     = &fop->f_item;
	item->ri_ops             = &fdmi_rec_not_item_ops;
	item->ri_session         = session;
	item->ri_prio            = M0_RPC_ITEM_PRIO_MID;

	/** @todo what deadline is better? (phase 2) */
	item->ri_deadline        = M0_TIME_IMMEDIATELY;

	item->ri_resend_interval = m0_time(M0_RPC_ITEM_RESEND_INTERVAL, 0);
	item->ri_nr_sent_max     = ~(uint64_t)0;

	return M0_RC(m0_rpc_post(item));
}

static bool pending_fop_clink_cb(struct m0_clink *clink)
{
	struct fdmi_pending_fop *pending_fop = M0_AMB(pending_fop, clink,
						      fti_clink);
	struct fdmi_sd_fom      *sd_fom = pending_fop->sd_fom;
	M0_ENTRY();

	if (m0_rpc_conn_pool_session_established(pending_fop->fti_session))
		fdmi_post_fop(pending_fop->fti_fop, pending_fop->fti_session);
	else
		m0_rpc_conn_pool_put(&pending_fop->sd_fom->fsf_conn_pool,
				     pending_fop->fti_session);
	m0_mutex_lock(&sd_fom->fsf_pending_fops_lock);
	pending_fops_tlist_del(pending_fop);
	m0_mutex_unlock(&sd_fom->fsf_pending_fops_lock);
	m0_free(pending_fop);

	M0_LEAVE();
	return true;
}

static int sd_fom_save_pending_fop(struct fdmi_sd_fom    *sd_fom,
				   struct m0_fop         *fop,
				   struct m0_rpc_session *session)
{
	struct fdmi_pending_fop *pending_fop;

	M0_ENTRY();

	M0_ALLOC_PTR(pending_fop);
	if (pending_fop == NULL)
		return M0_ERR(-ENOMEM);

	m0_fop_get(fop);
	pending_fop->fti_fop = fop;
	m0_clink_init(&pending_fop->fti_clink, pending_fop_clink_cb);
	pending_fop->fti_clink.cl_is_oneshot = true;
	pending_fop->fti_session = session;
	pending_fop->sd_fom = sd_fom;
	m0_clink_add_lock(m0_rpc_conn_pool_session_chan(session),
			  &pending_fop->fti_clink);
	m0_mutex_lock(&sd_fom->fsf_pending_fops_lock);
	pending_fops_tlink_init_at_tail(pending_fop, &sd_fom->fsf_pending_fops);
	m0_mutex_unlock(&sd_fom->fsf_pending_fops_lock);
	return M0_RC(0);
}

static int sd_fom_send_record(struct fdmi_sd_fom *sd_fom, struct m0_fop *fop,
			      const char *ep)
{
	int                    rc;
	struct m0_rpc_session *session;

	M0_ENTRY("sd_fom %p, fop %p, ep %s", sd_fom, fop, ep);
	rc = m0_rpc_conn_pool_get_async(&sd_fom->fsf_conn_pool, ep, &session);
	if (rc == 0)
		rc = fdmi_post_fop(fop, session);
	else if (rc == -EBUSY)
		rc = sd_fom_save_pending_fop(sd_fom, fop, session);
	return M0_RC(rc);
}

static int filters_nr(struct m0_fdmi_src_rec *src_rec, const char *endpoint)
{
	int n;

	M0_ENTRY("src_rec=%p, endpoint=%s", src_rec, endpoint);
	M0_PRE(m0_fdmi__record_is_valid(src_rec));
	n = m0_tl_reduce(fdmi_matched_filter_list, flt,
			 &src_rec->fsr_filter_list, 0,
			 + !!m0_streq(endpoint, flt->ff_endpoints[0]));
	M0_LEAVE("==> %d", n);
	return n;
}

static struct m0_rpc_machine *m0_fdmi__sd_conn_pool_rpc_machine(void)
{
	struct m0_fdmi_src_dock *src_dock = m0_fdmi_src_dock_get();
	return src_dock->fsdc_sd_fom.fsf_conn_pool.cp_rpc_mach;
}

static struct m0_fop *alloc_fdmi_rec_fop(int filter_num)
{
	struct m0_fop_fdmi_record *fop_data;
	struct m0_fop             *fop;

	M0_PRE(filter_num > 0);

	M0_ALLOC_PTR(fop_data);
	if (fop_data == NULL)
		goto data_alloc_fail;
	fop_data->fr_matched_flts.fmf_count = filter_num;

	M0_ALLOC_ARR(fop_data->fr_matched_flts.fmf_flt_id, filter_num);
	if (fop_data->fr_matched_flts.fmf_flt_id == NULL)
		goto flts_alloc_fail;

	fop = m0_fop_alloc(&m0_fop_fdmi_rec_not_fopt, fop_data,
			   m0_fdmi__sd_conn_pool_rpc_machine());
	if (fop == NULL)
		goto fop_alloc_fail;
	return fop;
fop_alloc_fail:
	m0_free(fop_data->fr_matched_flts.fmf_flt_id);
flts_alloc_fail:
	m0_free(fop_data);
data_alloc_fail:
	return NULL;
}

static struct m0_fop *fop_create(struct m0_fdmi_src_rec *src_rec,
				 const char             *endpoint)
{
	int                         filter_num;
	struct m0_conf_fdmi_filter *flt;
	struct m0_conf_fdmi_filter *tmp;
	int                         k;
	struct m0_fop              *fop = NULL;
	struct m0_fop_fdmi_record  *fop_data;
	int                         idx; /* XXX: TEMP */
	struct m0_fdmi_flt_id_arr  *matched = NULL;

	M0_ENTRY("src_rec %p, endpoint %s", src_rec, endpoint);
	M0_PRE(m0_fdmi__record_is_valid(src_rec));

	filter_num = filters_nr(src_rec, endpoint);
	if (filter_num > 0) {
		fop = alloc_fdmi_rec_fop(filter_num);
		if (fop == NULL)
			return NULL;
		fop_data = m0_fop_data(fop);
		fop_data->fr_rec_id   = src_rec->fsr_rec_id;
		fop_data->fr_rec_type = m0_fdmi__sd_rec_type_id_get(src_rec);
		matched = &fop_data->fr_matched_flts;
		k = 0;
		while (k < filter_num) {
			flt = fdmi_matched_filter_list_tlist_head(
				&src_rec->fsr_filter_list);
			if (m0_streq(endpoint, flt->ff_endpoints[0])) {
				matched->fmf_flt_id[k++] = flt->ff_filter_id;
				tmp = flt;
				flt = fdmi_matched_filter_list_tlist_next(
					&src_rec->fsr_filter_list, flt);
				fdmi_matched_filter_list_tlink_del_fini(tmp);
			} else {
				flt = fdmi_matched_filter_list_tlist_next(
					&src_rec->fsr_filter_list, flt);
			}
		}
		M0_LOG(M0_DEBUG, "FDMI record id = "U128X_F,
		       U128_P(&fop_data->fr_rec_id));
		M0_LOG(M0_DEBUG, "FDMI record type = %d", fop_data->fr_rec_type);
		M0_LOG(M0_DEBUG, "*   matched filters count = [%d]",
		       matched->fmf_count);
		for (idx = 0; idx < matched->fmf_count; idx++) {
			M0_LOG(M0_DEBUG, "*   [%4d] = "FID_SF, idx,
			       FID_P(&matched->fmf_flt_id[idx]));
		}
	}
	M0_LOG(M0_DEBUG, "ret fop %p", fop);
	return fop;
}

static int payload_encode(struct m0_fdmi_src_rec *src_rec, struct m0_fop *fop)
{
	struct m0_fop_fdmi_record *rec = m0_fop_data(fop);

	M0_ENTRY("src_rec %p fop %p", src_rec, fop);
	M0_PRE(m0_fdmi__record_is_valid(src_rec));
	return M0_RC(src_rec->fsr_src->fs_encode(src_rec, &rec->fr_payload));
}

static int sd_fom_process_matched_filters(struct m0_fdmi_src_dock *sd_ctx,
					  struct m0_fdmi_src_rec  *src_rec)
{
	int                         rc = 0;
	struct m0_conf_fdmi_filter *matched_filter;
	const char                 *endpoint;

	M0_ENTRY("sd_ctx %p src_rec %p", sd_ctx, src_rec);
	M0_PRE(m0_fdmi__record_is_valid(src_rec));

	M0_LOG(M0_DEBUG, "FDMI record id = "U128X_F,
	       U128_P(&src_rec->fsr_rec_id));
	while (!fdmi_matched_filter_list_tlist_is_empty(
					&src_rec->fsr_filter_list)) {
		struct m0_fop *fop;
		matched_filter = fdmi_matched_filter_list_tlist_head(
			&src_rec->fsr_filter_list);
		/*
		 * Currently only 1 endpoint is specified
		 * for a filter => take 1st array item
		 */
		endpoint = matched_filter->ff_endpoints[0];
		fop = fop_create(src_rec, endpoint);
		if (fop == NULL)
			continue;
		M0_LOG(M0_DEBUG, "will send fdmi rec");
		fop->f_opaque = src_rec;

		/* @todo check rc and handle errors properly. */
		rc = payload_encode(src_rec, fop);

		m0_ref_get(&src_rec->fsr_ref);
		sd_fom_send_record(&sd_ctx->fsdc_sd_fom, fop, endpoint);
		if (rc == 0) {
			m0_fdmi__fs_get(src_rec);
			/**
			 * @todo store map <fdmi record id, endpoint>,
			 * Phase 2
			 */
		} else {
			m0_ref_put(&src_rec->fsr_ref);
		}
		m0_fop_put_lock(fop);
	}
	return M0_RC(rc);
}

static int node_eval(void                        *data,
		     struct m0_fdmi_flt_var_node *value_desc,
		     struct m0_fdmi_flt_operand  *value)
{
	struct m0_fdmi_src_rec *src_rec = data;

	M0_PRE(m0_fdmi__record_is_valid(src_rec));
	return src_rec->fsr_src->fs_node_eval(src_rec, value_desc, value);
}

static int fdmi_filter_calc(struct fdmi_sd_fom         *sd_fom,
			    struct m0_fdmi_src_rec     *src_rec,
			    struct m0_conf_fdmi_filter *fdmi_filter)
{
	struct m0_fdmi_eval_var_info get_var_info;

	M0_ENTRY("sd_fom %p, src_rec %p, fdmi_filter %p",
		 sd_fom, src_rec, fdmi_filter);
	M0_PRE(m0_fdmi__record_is_valid(src_rec));

	get_var_info.user_data    = src_rec;
	get_var_info.get_value_cb = node_eval;

	return M0_RC(m0_fdmi_eval_flt(&sd_fom->fsf_flt_eval,
				      &fdmi_filter->ff_filter,
				      &get_var_info));
}

static int fdmi_sd_fom_tick(struct m0_fom *fom)
{
	struct fdmi_sd_fom      *sd_fom = M0_AMB(sd_fom, fom, fsf_fom);
	struct m0_fdmi_src_dock *sd_ctx = M0_AMB(sd_ctx, sd_fom, fsdc_sd_fom);
	struct m0_reqh_service  *rsvc = fom->fo_service;
	struct m0_fdmi_src_rec  *src_rec;
	int                      rc;

	M0_ENTRY("fom %p", fom);

	M0_LOG(M0_DEBUG, "sd_fom_tick sd_ctx %p", sd_ctx);

	switch (m0_fom_phase(fom)) {
	case FDMI_SRC_DOCK_FOM_PHASE_INIT:
		m0_fom_phase_set(fom, FDMI_SRC_DOCK_FOM_PHASE_WAIT);
		return M0_RC(M0_FSO_AGAIN);
	case FDMI_SRC_DOCK_FOM_PHASE_WAIT:
		M0_LOG(M0_DEBUG, "wait phase");
		m0_fom_phase_set(fom, FDMI_SRC_DOCK_FOM_PHASE_GET_REC);
		return M0_RC(M0_FSO_AGAIN);
	case FDMI_SRC_DOCK_FOM_PHASE_GET_REC:
		M0_LOG(M0_DEBUG, "get rec");

		m0_mutex_lock(&sd_ctx->fsdc_list_mutex);
		src_rec = fdmi_record_list_tlist_pop(
			&sd_ctx->fsdc_posted_rec_list);
		m0_mutex_unlock(&sd_ctx->fsdc_list_mutex);

		if (src_rec == NULL) {
			if (m0_reqh_service_state_get(rsvc) == M0_RST_STOPPING) {
				m0_fom_phase_set(fom,
						 FDMI_SRC_DOCK_FOM_PHASE_FINI);
			} else {
				m0_fom_phase_set(fom,
						 FDMI_SRC_DOCK_FOM_PHASE_WAIT);
			}
			return M0_RC(M0_FSO_WAIT);
		} else {
			M0_ASSERT(m0_fdmi__record_is_valid(src_rec));
			rc = process_fdmi_rec(sd_fom, src_rec);
			if (rc == 0) {
				if (!fdmi_matched_filter_list_tlist_is_empty(
				    &src_rec->fsr_filter_list)) {
					sd_fom_process_matched_filters(sd_ctx,
								       src_rec);
				}
			} else if (rc != -ENOENT) {
				/**
				 * -ENOENT error means that configuration does
				 * not have filters group matching the record
				 * type. This is fine, ignoring.
				 */
				M0_LOG(M0_ERROR,
				       "FDMI record processing error %d", rc);
			}
			/**
			 * Source dock is done with this record (however,
			 * there are stil locks caused by sending this record
			 * to plugin).
			 */
			m0_fdmi__fs_put(src_rec);
			m0_ref_put(&src_rec->fsr_ref);
			return M0_RC(M0_FSO_AGAIN);
		}
	}
	return M0_RC(M0_FSO_WAIT);
}

static void fdmi_rec_notif_replied(struct m0_rpc_item *item)
{
	struct m0_fdmi_src_rec  *src_rec;
	struct m0_fdmi_src_dock *src_dock;
	int                      rc;

	M0_ENTRY("item=%p", item);

	src_dock = m0_fdmi_src_dock_get();
	src_rec = m0_rpc_item_to_fop(item)->f_opaque;
	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	rc = item->ri_error ?: m0_rpc_item_generic_reply_rc(item->ri_reply);
	if (rc != 0)
		M0_LOG(M0_ERROR, "FDMI reply error %d item->ri_error %d",
		       rc, item->ri_error);

	m0_fdmi__handle_reply(src_dock, src_rec, rc);
	m0_rpc_conn_pool_put(&src_dock->fsdc_sd_fom.fsf_conn_pool,
			     item->ri_session);
	M0_LEAVE();
}

/*
 ******************************************************************************
 * FDMI SD Release Record FOM specific functions
 ******************************************************************************
 */

static int fdmi_rr_fom_create(struct m0_fop *fop, struct m0_fom **out,
			      struct m0_reqh *reqh)
{
	struct fdmi_rr_fom *rr_fom;
	struct m0_fom      *fom;
	struct m0_fop      *reply_fop;
	int                 rc = 0;

	M0_ENTRY("fop %p", fop);

	M0_ALLOC_PTR(rr_fom);
	if (rr_fom == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto end;
	}
	fom = &rr_fom->frf_fom;
	reply_fop = m0_fop_alloc(&m0_fop_fdmi_rec_release_rep_fopt, NULL,
				 m0_fdmi__sd_conn_pool_rpc_machine());
	if (reply_fop == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto end;
	}
	m0_fom_init(fom, &fop->f_type->ft_fom_type, &fdmi_rr_fom_ops,
		    fop, reply_fop, reqh);
	M0_ASSERT(m0_fom_phase(fom) == FDMI_RR_FOM_PHASE_INIT);
	*out = fom;
end:
	if (rc != 0) {
		m0_free(rr_fom);
		*out = NULL;
	}
	return M0_RC(rc);
}

static void fdmi_rr_fom_fini(struct m0_fom *fom)
{
	struct fdmi_rr_fom *rr_fom = M0_AMB(rr_fom, fom, frf_fom);

	M0_ENTRY("fom %p", fom);
	m0_fom_fini(fom);
	m0_free(rr_fom);
	M0_LEAVE();
}

static int fdmi_rr_fom_tick(struct m0_fom *fom)
{
	struct m0_fop_fdmi_rec_release       *fop_data;
	struct m0_fop_fdmi_rec_release_reply *reply_data;
	struct m0_rpc_item                   *item;

	M0_ENTRY("fom %p", fom);

	fop_data = m0_fop_data(fom->fo_fop);
	m0_fdmi__handle_release(&fop_data->frr_frid);
	reply_data = m0_fop_data(fom->fo_rep_fop);
	reply_data->frrr_rc = 0;
	item = m0_fop_to_rpc_item(fom->fo_rep_fop);
	m0_rpc_reply_post(&fom->fo_fop->f_item, item);
	m0_fom_phase_set(fom, FDMI_RR_FOM_PHASE_FINI);

	M0_LEAVE();
	return M0_FSO_WAIT;
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
