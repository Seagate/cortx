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
#include "rpc/rpc.h"          /* m0_rpc_post */
#include "rpc/rpclib.h"       /* m0_rpc_client_connect */
#include "lib/arith.h"        /* M0_CNT_INC, M0_CNT_DEC */
#include "lib/misc.h"         /* M0_IN */
#include "lib/errno.h"        /* ENOMEM, EPROTO */
#include "lib/memory.h"       /* M0_ALLOC_ARR, m0_free */
#include "lib/finject.h"      /* M0_FI_ENABLED */
#include "net/lnet/lnet.h"    /* M0_NET_LNET_XEP_ADDR_LEN */
#include "fop/fop.h"
#include "fop/fop_item_type.h"
#include "fop/fom_generic.h"
#include "rpc/rpc_opcodes.h"
#include "fdmi/fdmi.h"
#include "fdmi/plugin_dock.h"
#include "fdmi/plugin_dock_internal.h"
#include "fdmi/fops.h"        /* m0_fop_fdmi_record */
#include "fdmi/fops_xc.h"
#include "rpc/conn_pool.h"
#include "fdmi/module.h"

M0_TL_DESCR_DEFINE(fdmi_filters, "filter regs list", static,
		   struct m0_fdmi_filter_reg, ffr_link, ffr_magic,
		   M0_FDMI_FLTR_MAGIC, M0_FDMI_FLTR_HEAD_MAGIC);
M0_TL_DEFINE(fdmi_filters, static, struct m0_fdmi_filter_reg);

M0_TL_DESCR_DEFINE(fdmi_recs, "fdmi record regs list", static,
		   struct m0_fdmi_record_reg, frr_link, frr_magic,
		   M0_FDMI_RCRD_MAGIC, M0_FDMI_RCRD_HEAD_MAGIC);
M0_TL_DEFINE(fdmi_recs, static, struct m0_fdmi_record_reg);

M0_INTERNAL struct m0_rpc_conn_pool *ut_pdock_conn_pool(void)
{
	struct m0_fdmi_module *m = m0_fdmi_module__get();
	return &m->fdm_p.fdmp_conn_pool;
}

struct m0_fop_type m0_pdock_fdmi_filters_enable_fopt;
struct m0_fop_type m0_pdock_fdmi_filters_enable_rep_fopt;

static void pdock_record_release(struct m0_ref *ref);

static int pdock_client_post(struct m0_fop                *fop,
			     struct m0_rpc_session        *session,
			     const struct m0_rpc_item_ops *ri_ops);

#if 0  /* cut off until future development */
#define SERVER_ENDPOINT_ADDR "0@lo:12345:34:1"
#define SERVER_ENDPOINT      "lnet:" SERVER_ENDPOINT_ADDR


static void plugin_dock_fopts_init(void)
{
	M0_FOP_TYPE_INIT(&m0_pdock_fdmi_filters_enable_fopt,
			 .name      = "plugin dock fdmi filters enable",
			 .opcode    = M0_FDMI_FILTERS_ENABLE_OPCODE,
			 .xt        = m0_fdmi_filters_enable_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .sm        = &m0_generic_conf,
		);

	M0_FOP_TYPE_INIT(&m0_pdock_fdmi_filters_enable_rep_fopt,
			 .name      = "plugin dock fdmi filters enable reply",
			 .opcode    = M0_FDMI_FILTERS_ENABLE_REP_OPCODE,
			 .xt        = m0_fdmi_filters_enable_reply_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .sm        = &m0_generic_conf,
		);
}
#endif

struct m0_rpc_machine *m0_fdmi__pdock_conn_pool_rpc_machine()
{
	struct m0_fdmi_module *m = m0_fdmi_module__get();
	return m->fdm_p.fdmp_conn_pool.cp_rpc_mach;
}

static void test_print_fdmi_rec_list(void)
{
	struct m0_fdmi_module     *m = m0_fdmi_module__get();
	struct m0_fdmi_record_reg *rreg;

	m0_mutex_lock(&m->fdm_p.fdmp_fdmi_recs_lock);
	m0_tl_for(fdmi_recs, &m->fdm_p.fdmp_fdmi_recs, rreg) {
		M0_ENTRY("DBG: rreg: %p,  rid: " U128X_F,
			 rreg, U128_P(&rreg->frr_rec->fr_rec_id));
	} m0_tl_endfor;
	m0_mutex_unlock(&m->fdm_p.fdmp_fdmi_recs_lock);
}


struct
m0_fdmi_filter_reg *m0_fdmi__pdock_filter_reg_find(const struct m0_fid *fid)
{
	struct m0_fdmi_module     *m = m0_fdmi_module__get();
	struct m0_fdmi_filter_reg *reg;

	M0_ENTRY();

	m0_mutex_lock(&m->fdm_p.fdmp_fdmi_filters_lock);
	reg = m0_tl_find(fdmi_filters, freg, &m->fdm_p.fdmp_fdmi_filters,
			 m0_fid_eq(fid, &freg->ffr_ffid));
	m0_mutex_unlock(&m->fdm_p.fdmp_fdmi_filters_lock);

	M0_LEAVE();
	return reg;
}

struct
m0_fdmi_record_reg *m0_fdmi__pdock_record_reg_find(const struct m0_uint128 *rid)
{
	struct m0_fdmi_module     *m = m0_fdmi_module__get();
	struct m0_fdmi_record_reg *reg;

	M0_ENTRY("rid " U128X_F, U128_P(rid));

	if (M0_FI_ENABLED("fail_find"))
		return NULL;

	m0_mutex_lock(&m->fdm_p.fdmp_fdmi_recs_lock);
	reg = m0_tl_find(fdmi_recs, rreg, &m->fdm_p.fdmp_fdmi_recs,
			 m0_uint128_eq( rid, &rreg->frr_rec->fr_rec_id ));
	m0_mutex_unlock(&m->fdm_p.fdmp_fdmi_recs_lock);

	M0_LEAVE("<< reg %p", reg);
	return reg;
}

/**
  Private pdock API. Plugin calls it via m0_fdmi_pd_ops::fpo_register_filter()
  when registers new filter description along with id and callback.
 */

static int register_filter(const struct m0_fid              *fid,
			   const struct m0_fdmi_filter_desc *desc,
			   const struct m0_fdmi_plugin_ops  *pcb)
{
	struct m0_fdmi_module      *m = m0_fdmi_module__get();
	struct m0_fdmi_plugin_ops  *pcb_copy;
	struct m0_fdmi_filter_desc *desc_copy;
	struct m0_fdmi_filter_reg  *filter;
	int                         rc;

	M0_ENTRY();

	if (desc == NULL || pcb == NULL)
		return M0_RC(-EINVAL);

	M0_ALLOC_PTR(pcb_copy);
	if (pcb_copy == NULL)
		return M0_RC(-ENOMEM);
	else
		*pcb_copy = *pcb;

	M0_ALLOC_PTR(desc_copy);
	if (desc_copy == NULL)
	{
		rc = -ENOMEM;
		goto free_pcb;
	}
	else
		*desc_copy = *desc;

	M0_ALLOC_PTR(filter);
	if (filter == NULL) {
		rc = -ENOMEM;
		goto free_desc;
	} else {
		*filter = (struct m0_fdmi_filter_reg) {
			.ffr_ffid  = *fid,
			.ffr_desc  = desc_copy,
			.ffr_pcb   = pcb_copy,
#if 0  /* cut off until future development */
                         /*
			  * FIXME: we need to get the endpoint at runtime
			  * but not statically defined one (phase 2)
			  */
			.ffr_ep    = SERVER_ENDPOINT,
#endif
			.ffr_flags = M0_BITS(M0_FDMI_FILTER_INACTIVE),
			.ffr_magic = M0_FDMI_FLTR_MAGIC
		};

	}

	m0_mutex_lock(&m->fdm_p.fdmp_fdmi_filters_lock);
	fdmi_filters_tlink_init_at_tail(filter, &m->fdm_p.fdmp_fdmi_filters);
	m0_mutex_unlock(&m->fdm_p.fdmp_fdmi_filters_lock);

	return M0_RC(0);

free_desc:
	m0_free(desc_copy);
free_pcb:
	m0_free(pcb_copy);

	return M0_RC(rc);
}

/**
  Private pdock API. Plugin calls it via m0_fdmi_pd_ops::fpo_enable_filters()
  when in need to control state of particular filter(s).
 */

static void enable_filters(bool           enable,
			   struct m0_fid *filter_ids,
			   uint32_t       filter_count)
{
	struct m0_fdmi_filter_reg *reg;
	uint32_t                   idx;

	M0_ENTRY();

	/* put local mark */
	for (idx = 0; idx < filter_count; ++idx) {
		reg = m0_fdmi__pdock_filter_reg_find(&filter_ids[idx]);
		if (reg != NULL) {
			M0_ASSERT(m0_fid_eq(&reg->ffr_ffid, &filter_ids[idx]));

			enable  ? ({ reg->ffr_flags &= ~M0_BITS(
						        M0_FDMI_FILTER_INACTIVE,
							M0_FDMI_FILTER_DEAD); })
				: ({ reg->ffr_flags |= M0_BITS(
							M0_FDMI_FILTER_INACTIVE
							); });
		} else {
			M0_LOG(M0_ERROR,
			       "filter reg not found: ffid = "FID_SF,
			       FID_P(&filter_ids[idx]));
		}
	}

	/** @todo Phase 2: implement posting filter descriptions to filterd */

	M0_LEAVE();
}

static void pdock_record_reg_cleanup(struct m0_rpc_item *item,
				     bool                replied)
{
	struct m0_fdmi_module          *m = m0_fdmi_module__get();
	struct m0_fop                  *fop;
	struct m0_fop_fdmi_rec_release *rdata;
	struct m0_fdmi_record_reg      *rreg;

	M0_ENTRY("item = %p, replied = %i", item, replied);

	fop = m0_rpc_item_to_fop(item);
	rdata = m0_fop_data(fop);

	if (replied) {
		M0_LOG(M0_DEBUG,
		       "`release fdmi record` successfully replied: id = "
		       U128X_F, U128_P(&rdata->frr_frid));
	} else {
		M0_LOG(M0_DEBUG,
		       "`release fdmi record` was not replied: id = "
		       U128X_F, U128_P(&rdata->frr_frid));
	}

	rreg = m0_fdmi__pdock_record_reg_find(&rdata->frr_frid);
	if (rreg != NULL) {
		M0_LOG(M0_DEBUG, "remove and free rreg %p, rid " U128X_F,
		       rreg, U128_P(&rreg->frr_rec->fr_rec_id));
		m0_mutex_lock(&m->fdm_p.fdmp_fdmi_recs_lock);
		fdmi_recs_tlist_remove(rreg);
		m0_mutex_unlock(&m->fdm_p.fdmp_fdmi_recs_lock);

		if (rreg->frr_sess != NULL)
			m0_rpc_conn_pool_put(&m->fdm_p.fdmp_conn_pool,
					     rreg->frr_sess);

		if (rreg->frr_ep_addr != NULL)
			m0_free(rreg->frr_ep_addr);

		m0_fop_put(rreg->frr_fop);
		m0_free(rreg);
	} else {
		M0_LOG(M0_ERROR,
		       "fdmi record was not found in pdock: id = "U128X_F,
		       U128_P(&rdata->frr_frid));
	}

	M0_LEAVE();
}

static void release_replied(struct m0_rpc_item *item)
{
	M0_ENTRY("item %p, ri_error 0x%x", item, item->ri_error);

	/* FIXME: Can be refactored (phase 2) */
	pdock_record_reg_cleanup(item, (bool)(item->ri_error == 0));
	M0_LEAVE();
}

struct m0_rpc_item_ops release_ri_ops = {
	.rio_replied = release_replied
};

/**
  Private pdock API. Plugin calls it via m0_fdmi_pd_ops::fpo_release_fdmi_rec()
  when done with FDMI record.
 */

static void release_fdmi_rec(struct m0_uint128 *rec_id,
			     struct m0_fid     *filter_id M0_UNUSED)
{
	struct m0_fdmi_record_reg *reg;

	M0_ENTRY();

	reg = m0_fdmi__pdock_record_reg_find(rec_id);
	if (reg == NULL) {
		M0_LOG(M0_NOTICE, "FDMI record not listed: id = "
		       U128X_F, U128_P(rec_id));
		goto leave;
	}

	m0_ref_put(&reg->frr_ref);  /* pdock_record_ref_release() is gonna be
				     * called when refc gets to zero
				     */
leave:
	M0_LEAVE();
}


M0_INTERNAL struct
m0_fdmi_record_reg *m0_fdmi__pdock_fdmi_record_register(struct m0_fop *fop)
{
	struct m0_fdmi_module     *m = m0_fdmi_module__get();
	struct m0_fop_fdmi_record *frec;
	struct m0_fdmi_record_reg *rreg;

	M0_ENTRY();
	M0_ASSERT(m->fdm_p.fdmp_dock_inited);

	if (M0_FI_ENABLED("fail_fdmi_rec_reg"))
		return NULL;

	frec = m0_fop_data(fop);

	/* prepare record registration entry */

	M0_ALLOC_PTR(rreg);
	if (rreg == NULL) {
		M0_LOG(M0_ERROR, "No memory available");
		goto leave;
	}

	rreg->frr_rec = frec;  /* attaching fop payload to reg entry */
	rreg->frr_fop = fop;
	m0_fop_get(rreg->frr_fop);

	if (m0_fop_to_rpc_item(fop)->ri_rmachine != NULL) {  /* the test is for
							      * the sake of ut,
							      * when no rpc
							      * appears being
							      * in use */
		const char *ep_addr =
			m0_rpc_item_remote_ep_addr(m0_fop_to_rpc_item(fop));

		M0_LOG(M0_DEBUG, "FOP remote endpoint address = %s", ep_addr);

		rreg->frr_ep_addr = m0_strdup(ep_addr);
	}

	/* lock the record until fom is done with the one */
	m0_ref_init(&rreg->frr_ref, 1, pdock_record_release);

	/* keep registration entry */
	m0_mutex_lock(&m->fdm_p.fdmp_fdmi_recs_lock);
	fdmi_recs_tlink_init_at_tail(rreg, &m->fdm_p.fdmp_fdmi_recs);
	m0_mutex_unlock(&m->fdm_p.fdmp_fdmi_recs_lock);

	test_print_fdmi_rec_list();

	M0_LOG(M0_DEBUG, "add to list rreg %p, rid " U128X_F,
	       rreg, U128_P(&rreg->frr_rec->fr_rec_id));

leave:
	M0_LEAVE();
	return rreg;
}

/**
 * Called when fdmi record refc just got to zero
 */
static void pdock_record_release(struct m0_ref *ref)
{
	struct m0_fdmi_module          *m = m0_fdmi_module__get();
	struct m0_fdmi_record_reg      *rreg;
	struct m0_fop                  *req;
	struct m0_fop_fdmi_rec_release *req_data;
	int                             rc;

	M0_ENTRY();

	req      = NULL;
	req_data = NULL;
	rc       = 0;

	rreg = container_of(ref, struct m0_fdmi_record_reg, frr_ref);

	M0_LOG(M0_DEBUG, "Will send release for rreg %p, rid " U128X_F,
	       rreg, U128_P(&rreg->frr_rec->fr_rec_id));

	if (rreg->frr_ep_addr == NULL) {
		/* No way to post anything over RPC */
		rc = -EACCES;
		goto leave;
	}

	/* Post release request */

	M0_ALLOC_PTR(req_data);
	if (req_data == NULL) {
		M0_LOG(M0_ERROR, "request data allocation failed");
		M0_RC(-ENOMEM);
		return;
	}

	req_data->frr_frt  = rreg->frr_rec->fr_rec_type;
	req_data->frr_frid = rreg->frr_rec->fr_rec_id;

	req = m0_fop_alloc(&m0_fop_fdmi_rec_release_fopt, req_data,
			   m0_fdmi__pdock_conn_pool_rpc_machine());
	if (req == NULL) {
		m0_free(req_data);
		M0_LOG(M0_ERROR, "fop allocation failed");
		rc = -ENOMEM;
		goto leave;
	}

	/* @todo Possibly blocks here for a long time (phase 2) */
	rc = m0_rpc_conn_pool_get_sync(&m->fdm_p.fdmp_conn_pool,
				       rreg->frr_ep_addr, &rreg->frr_sess);

	if (rc != 0) {
		rc = -ENOENT;
		M0_LOG(M0_ERROR,
		       "RPC failed to get connection to post release request: "
		       "id = " U128X_F, U128_P(&req_data->frr_frid));
		goto leave;
	}

	/* FIXME: TEMP */
	M0_LOG(M0_WARN, "Processed FDMI rec "
	       U128X_F, U128_P(&rreg->frr_rec->fr_rec_id));

	rc = pdock_client_post(req, rreg->frr_sess, &release_ri_ops);
	if (rc != 0) {
		M0_LOG(M0_ERROR,
		       "RPC failed to post release request: id = "
		       U128X_F", rc = %d",
		       U128_P(&req_data->frr_frid), rc);
		if (rreg->frr_sess != NULL) {
			m0_rpc_conn_pool_put(&m->fdm_p.fdmp_conn_pool,
					     rreg->frr_sess);
		}
	}

leave:
	if (req != NULL) {
		if (m0_fop_to_rpc_item(req)->ri_rmachine != NULL &&
		    rreg->frr_sess != NULL)
			m0_fop_put_lock(req);
		else
			m0_free(req);
	}

	M0_RC(rc);
}

/**
 * Private pdock API. Plugin calls it via
 * m0_fdmi_pd_ops::fpo_deregister_plugin()
 * when does not need in feeding FDMI records anymore.
 */
static void deregister_plugin(struct m0_fid *filter_ids,
			      uint64_t       filter_count)
{
	struct m0_fdmi_module     *m = m0_fdmi_module__get();
	struct m0_fdmi_filter_reg *freg;
	int                        idx;

	M0_ENTRY();

	/* deactivate filters */
	enable_filters(false, filter_ids, filter_count);

	/* kill their registrations */
	for (idx = 0; idx < filter_count; idx++) {
		freg = m0_fdmi__pdock_filter_reg_find(&filter_ids[idx]);

		if (freg == NULL) {
			M0_LOG(M0_ERROR,
			       "Filter reg not found: id = "FID_SF,
			       FID_P(&filter_ids[idx]));
			continue;
		}

		m0_mutex_lock(&m->fdm_p.fdmp_fdmi_filters_lock);
		fdmi_filters_tlist_remove(freg);
		m0_mutex_unlock(&m->fdm_p.fdmp_fdmi_filters_lock);
		m0_free(freg->ffr_desc);
		m0_free(freg->ffr_pcb);
#if 0
		m0_free(freg->ffr_ep);
#endif
	}

	M0_LEAVE();
}

const struct m0_fdmi_pd_ops fdmi_pdo = {
	.fpo_register_filter   = register_filter,
	.fpo_enable_filters    = enable_filters,
	.fpo_release_fdmi_rec  = release_fdmi_rec,
	.fpo_deregister_plugin = deregister_plugin
};

const struct m0_fdmi_pd_ops *m0_fdmi_plugin_dock_api_get(void)
{
	return &fdmi_pdo;
}

M0_INTERNAL int m0_fdmi__plugin_dock_init(void)
{
	struct m0_fdmi_module *m = m0_fdmi_module__get();
	M0_ENTRY();

	if (m->fdm_p.fdmp_dock_inited) {
		M0_LOG(M0_WARN, "Plugin dock is already initialized.");
		return M0_RC(0);
	}

	/* Disabled until future development */
#if 0
	/* Prepare RPC receiving */
	plugin_dock_fopts_init();
#endif
	/* Initialise communication context. */
	fdmi_filters_tlist_init(&m->fdm_p.fdmp_fdmi_filters);
	m0_mutex_init(&m->fdm_p.fdmp_fdmi_filters_lock);
	fdmi_recs_tlist_init(&m->fdm_p.fdmp_fdmi_recs);
	m0_mutex_init(&m->fdm_p.fdmp_fdmi_recs_lock);
	m->fdm_p.fdmp_dock_inited = true;
	return M0_RC(0);
}

M0_INTERNAL int m0_fdmi__plugin_dock_start(struct m0_reqh *reqh)
{
	struct m0_fdmi_module *m = m0_fdmi_module__get();
	struct m0_rpc_machine *rpc_machine;
	int                    rc;

	M0_ENTRY();

	M0_ASSERT(m->fdm_p.fdmp_dock_inited);
	rpc_machine = m0_reqh_rpc_mach_tlist_head(&reqh->rh_rpc_machines);

	M0_SET0(&m->fdm_p.fdmp_conn_pool);
	rc = m0_rpc_conn_pool_init(&m->fdm_p.fdmp_conn_pool, rpc_machine,
			M0_TIME_NEVER, /* connection timeout*/
			32             /* max rpcs in flight */);

	return M0_RC(rc);
}

M0_INTERNAL void m0_fdmi__plugin_dock_stop()
{
	struct m0_fdmi_module *m = m0_fdmi_module__get();
	M0_ENTRY();
	m0_rpc_conn_pool_fini(&m->fdm_p.fdmp_conn_pool);
	M0_LEAVE();
}

M0_INTERNAL void m0_fdmi__plugin_dock_fini(void)
{
	struct m0_fdmi_module     *m = m0_fdmi_module__get();
	struct m0_fdmi_record_reg *rreg;
	struct m0_fdmi_filter_reg *freg;

	M0_ENTRY();

	m0_mutex_lock(&m->fdm_p.fdmp_fdmi_recs_lock);
	m0_tl_teardown(fdmi_recs, &m->fdm_p.fdmp_fdmi_recs, rreg) {
		/* @todo Find out what to do with frr_rec (phase 2). */
		M0_LOG(M0_DEBUG, "teardown: remove and free rreg %p, rid "
		       U128X_F, rreg, U128_P(&rreg->frr_rec->fr_rec_id));
		if (rreg->frr_ep_addr != NULL)
			m0_free(rreg->frr_ep_addr);
		m0_free(rreg);
	}
	m0_mutex_unlock(&m->fdm_p.fdmp_fdmi_recs_lock);

	m0_mutex_lock(&m->fdm_p.fdmp_fdmi_filters_lock);
	m0_tl_teardown(fdmi_filters, &m->fdm_p.fdmp_fdmi_filters, freg) {
		m0_free(freg->ffr_desc);
		m0_free(freg->ffr_pcb);
#if 0
		m0_free(freg->ffr_ep);
#endif
	}
	m0_mutex_unlock(&m->fdm_p.fdmp_fdmi_filters_lock);

	m0_fop_type_fini(&m0_pdock_fdmi_filters_enable_fopt);
	m0_fop_type_fini(&m0_pdock_fdmi_filters_enable_rep_fopt);
	m->fdm_p.fdmp_dock_inited = false;

	fdmi_filters_tlist_fini(&m->fdm_p.fdmp_fdmi_filters);
	m0_mutex_fini(&m->fdm_p.fdmp_fdmi_filters_lock);

	fdmi_recs_tlist_fini(&m->fdm_p.fdmp_fdmi_recs);
	m0_mutex_fini(&m->fdm_p.fdmp_fdmi_recs_lock);

	M0_LEAVE();
}

static int pdock_client_post(struct m0_fop                *fop,
			     struct m0_rpc_session        *session,
			     const struct m0_rpc_item_ops *ri_ops)
{
	int                 rc;
	struct m0_rpc_item *item;

	M0_ENTRY("fop: %p, session: %p", fop, session);

	item                     = &fop->f_item;
	item->ri_ops             = ri_ops;
	item->ri_session         = session;
	item->ri_prio            = M0_RPC_ITEM_PRIO_MID;
	item->ri_deadline        = M0_TIME_IMMEDIATELY;
	item->ri_resend_interval = m0_time(M0_RPC_ITEM_RESEND_INTERVAL, 0);
	item->ri_nr_sent_max     = ~(uint64_t)0;

	rc = m0_rpc_post(item);

	return M0_RC(rc);
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
