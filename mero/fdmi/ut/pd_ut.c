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
 * Original creation date: 10/27/2014
 */


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include "ut/ut.h"
#include "fdmi/module.h"
#include "fdmi/fdmi.h"
#include "fdmi/fops.h"
#include "fdmi/service.h"
#include "fdmi/plugin_dock_internal.h"
#include "fdmi/ut/sd_common.h"
#include "lib/string.h"
#include "lib/finject.h"
#include "rpc/item_internal.h"                /* m0_rpc_item_change_state */
#include "rpc/packet_internal.h"              /* m0_rpc_packet_traverse_items */
#include "rpc/rpc_internal.h"                 /* item2conn */
#include "rpc/rpc_machine_internal.h"
#include "rpc/conn_pool_internal.h"

/*----------------------------------------
  fdmi_pd_register_filter
  ----------------------------------------*/

static
int pd_ut_pcb_fdmi_rec(struct m0_uint128   *rec_id,
		       struct m0_buf        fdmi_rec,
		       struct m0_fid        filter_id);

void fdmi_pd_register_filter(void)
{
	const struct m0_fid              ffid = {
		.f_container = 0xDEA110C,
		.f_key       = 0xDA221ED,
	};
	const struct m0_fdmi_plugin_ops  pcb = {
		.po_fdmi_rec = pd_ut_pcb_fdmi_rec
	};
	const struct m0_fdmi_filter_desc fd;
	const struct m0_fdmi_pd_ops     *pdo = m0_fdmi_plugin_dock_api_get();
	struct m0_fdmi_filter_reg       *freg;
	struct m0_fid                    fids[2] = { [0] = ffid, [1] = { 0, 0 } };
	int                              rc;

	rc = (pdo->fpo_register_filter)(&ffid, &fd, NULL);
	M0_UT_ASSERT(rc == -EINVAL);

	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 0, 1);
	rc = (pdo->fpo_register_filter)(&ffid, &fd, &pcb);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 1, 1);
	rc = (pdo->fpo_register_filter)(&ffid, &fd, &pcb);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 2, 1);
	rc = (pdo->fpo_register_filter)(&ffid, &fd, &pcb);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fi_disable("m0_alloc", "fail_allocation");

	(pdo->fpo_enable_filters)(true, fids, ARRAY_SIZE(fids));

	/* add filter */
	rc = (pdo->fpo_register_filter)(&ffid, &fd, &pcb);
	M0_UT_ASSERT(rc == 0);

	/* find filter and see if it's disabled */
	freg = m0_fdmi__pdock_filter_reg_find(&ffid);
	M0_UT_ASSERT(freg != NULL);
	M0_UT_ASSERT(freg->ffr_flags & M0_BITS(M0_FDMI_FILTER_INACTIVE));

	/* toggle active flag back and forth */
	if (freg->ffr_flags & M0_BITS(M0_FDMI_FILTER_INACTIVE)) {

		(pdo->fpo_enable_filters)(true, fids, ARRAY_SIZE(fids));
		M0_UT_ASSERT(!(freg->ffr_flags &
			       M0_BITS(M0_FDMI_FILTER_INACTIVE)));

		(pdo->fpo_enable_filters)(false, fids, ARRAY_SIZE(fids));
		M0_UT_ASSERT(!!(freg->ffr_flags &
				M0_BITS(M0_FDMI_FILTER_INACTIVE)));
	}

	/* deregister filter */
	(pdo->fpo_deregister_plugin)(fids, ARRAY_SIZE(fids));
}

/*----------------------------------------
  fdmi_pd_fom_norpc
  ----------------------------------------*/

static struct m0_semaphore     g_sem;
struct m0_uint128              frid_new = M0_UINT128(0xEEEE, 0xEEEE);
struct m0_uint128             *frid_watch;


static void ut_pd_fom_fini(struct m0_fom *fom);

int (*native_create)(struct m0_fop  *fop,
		     struct m0_fom **out,
		     struct m0_reqh *reqh) = NULL;

int detour_create(struct m0_fop *fop, struct m0_fom **out, struct m0_reqh *reqh)
{
	int                        rc;
	struct pdock_fom          *pd_fom;
	struct m0_fom             *fom;
	struct m0_fdmi_record_reg *rreg;
	struct m0_fop_fdmi_record *frec;

	M0_ENTRY();

	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 0, 1);
	rc = (*native_create)(fop, out, reqh);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 1, 1);
	rc = (*native_create)(fop, out, reqh);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 2, 1);
	rc = (*native_create)(fop, out, reqh);
	M0_UT_ASSERT(rc == -ENOENT);

	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 3, 1);
	rc = (*native_create)(fop, out, reqh);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fi_disable("m0_alloc", "fail_allocation");

	rc = (*native_create)(fop, out, reqh);
	M0_UT_ASSERT(m0_fop_data(fop)
		     ? rc == 0
		     : rc == -ENOMEM);

	if (rc != 0) {
		m0_semaphore_up(&g_sem);
		return rc;
	}

	M0_UT_ASSERT(out != NULL);

	fom = *out;
	M0_UT_ASSERT(fom != NULL);

	pd_fom = container_of(fom, struct pdock_fom, pf_fom);
	pd_fom->pf_custom_fom_fini = ut_pd_fom_fini;

	frec = m0_fop_data(fop);
	M0_UT_ASSERT(frec != NULL);
	rreg = m0_fdmi__pdock_record_reg_find(&frec->fr_rec_id);
	M0_UT_ASSERT(rreg != NULL);

	return M0_RC(rc);
}

struct m0_fom_type_ops    fomt_ops = { .fto_create = detour_create };


#define FID_CTN   0xDEA110C
#define FID_KEY   0xDA221ED
#define FID_CTN1  0x1FA11ED
#define FID_KEY1  0x1A110CA
#define M0_FID(c_, k_)  { .f_container = c_, .f_key = k_ }

struct m0_fid     ffid = M0_FID(FID_CTN, FID_KEY);
struct m0_uint128 frid = M0_UINT128(0xBEC02, 0xF11ED);

static void ut_pd_fom_fini(struct m0_fom *fom)
{
	struct m0_fop_fdmi_record *frec;

	M0_ENTRY();

	M0_PRE(m0_fom_phase(fom) == M0_FOM_PHASE_FINISH);

	frec = m0_fop_data(fom->fo_fop);
	M0_UT_ASSERT(m0_uint128_eq(frid_watch, &frec->fr_rec_id));

	/* detach fop from fom */
	m0_free0(&fom->fo_fop);

	/* kill fom */
	m0_fom_fini(fom);

	m0_semaphore_up(&g_sem);

	m0_fi_disable("m0_fdmi__pdock_record_reg_find", "fail_find");

	M0_LEAVE();
}

struct m0_fid             ffids[2] = {
	[0] = M0_FID(FID_CTN, FID_KEY),
	[1] = M0_FID(FID_CTN1, FID_KEY1)
};
struct m0_fdmi_flt_id_arr farr     = {
	.fmf_flt_id = ffids,
	.fmf_count = ARRAY_SIZE(ffids)
};

static int pd_ut_pcb_fdmi_rec(struct m0_uint128   *rec_id,
			      struct m0_buf        fdmi_rec,
			      struct m0_fid        filter_id)
{
	struct m0_fdmi_record_reg *rreg;

	M0_ENTRY("fdmi record arrived: id " U128X_F
		 ", filter id = " FID_SF,
		 U128_P(rec_id),
		 FID_P(&filter_id));

	M0_UT_ASSERT(m0_uint128_eq(frid_watch, rec_id));
	M0_UT_ASSERT(m0_fid_eq(&ffids[0], &filter_id) ||
		     m0_fid_eq(&ffids[1], &filter_id));

	rreg = m0_fdmi__pdock_record_reg_find(rec_id);
	M0_UT_ASSERT(rreg != NULL);

	if (m0_uint128_eq(&frid_new, &rreg->frr_rec->fr_rec_id)) {
		m0_fi_enable_off_n_on_m("m0_fdmi__pdock_record_reg_find",
					"fail_find", 0, 1);
	}

	return 0;
}

void __fdmi_pd_fom_norpc(bool register_filter)
{
	struct m0_reqh                  *reqh;
	struct m0_fop                   *fop;
	struct m0_fop_fdmi_record       *rec;
	struct m0_fop_type               fopt;
	int                              rc;

	struct m0_fdmi_filter_reg       *freg;
	const struct m0_fdmi_plugin_ops  pcb = {
		.po_fdmi_rec = pd_ut_pcb_fdmi_rec
	};
	const struct m0_fdmi_filter_desc fd;
	const struct m0_fdmi_pd_ops     *pdo = m0_fdmi_plugin_dock_api_get();

	M0_ENTRY();

	if (register_filter) {
		/* add filter */
		rc = (pdo->fpo_register_filter)(&ffid, &fd, &pcb);
		M0_UT_ASSERT(rc == 0);
		freg = m0_fdmi__pdock_filter_reg_find(&ffid);
		M0_UT_ASSERT(freg != NULL);
		M0_UT_ASSERT(freg->ffr_flags &
			     M0_BITS(M0_FDMI_FILTER_INACTIVE));
		(pdo->fpo_enable_filters)(true, ffids, ARRAY_SIZE(ffids));
		M0_UT_ASSERT(!(freg->ffr_flags &
			       M0_BITS(M0_FDMI_FILTER_INACTIVE)));
		rc = (pdo->fpo_register_filter)(&ffids[1], &fd, &pcb);
		M0_UT_ASSERT(rc == 0);
		freg = m0_fdmi__pdock_filter_reg_find(&ffid);
		M0_UT_ASSERT(freg != NULL);
		freg->ffr_pcb = NULL;
	}

	/* intercept fom create */
	fopt = m0_fop_fdmi_rec_not_fopt;
	native_create = fopt.ft_fom_type.ft_ops->fto_create;
	fopt.ft_fom_type = (struct m0_fom_type) {
/* hook it up*/
		.ft_ops        =
			&fomt_ops,
/* keep native*/
		.ft_conf       =
			m0_fop_fdmi_rec_not_fopt.ft_fom_type.ft_conf,
		.ft_state_conf =
			m0_fop_fdmi_rec_not_fopt.ft_fom_type.ft_state_conf,
		.ft_rstype     =
			m0_fop_fdmi_rec_not_fopt.ft_fom_type.ft_rstype,
	};

	fdmi_serv_start_ut(&filterc_stub_ops);

	m0_semaphore_init(&g_sem, 0);

	reqh = &g_sd_ut.mero.cc_reqh_ctx.rc_reqh;

	/* try with known fdmi record id */
	frid_watch = &frid;
	M0_ALLOC_PTR(rec);
	M0_UT_ASSERT(rec != NULL);
	M0_SET0(rec);
	rec->fr_rec_id       = *frid_watch;
	rec->fr_rec_type     = M0_FDMI_REC_TYPE_FOL;
	rec->fr_matched_flts = farr;

	fop = m0_fop_alloc(&fopt, rec, (void*)1);
	M0_UT_ASSERT(fop != NULL);
	fop->f_item.ri_rmachine = NULL;

	rc = m0_reqh_fop_handle(reqh, fop);
	M0_UT_ASSERT(rc == 0);

	/* wait for fom finishing */
	m0_semaphore_down(&g_sem);

	/* try with another fdmi record id */
	frid_watch = &frid_new;
	M0_ALLOC_PTR(rec);
	M0_UT_ASSERT(rec != NULL);
	M0_SET0(rec);
	rec->fr_rec_id       = *frid_watch;
	rec->fr_rec_type     = M0_FDMI_REC_TYPE_FOL;
	rec->fr_matched_flts = farr;

	fop = m0_fop_alloc(&fopt, rec, (void*)1);
	M0_UT_ASSERT(fop != NULL);
	fop->f_item.ri_rmachine = NULL;

	rc = m0_reqh_fop_handle(reqh, fop);
	M0_UT_ASSERT(rc == 0);

	/* wait for fom finishing */
	m0_semaphore_down(&g_sem);

	fdmi_serv_stop_ut();

	m0_semaphore_fini(&g_sem);
	M0_LEAVE();
}

void fdmi_pd_fom_norpc(void)
{
	__fdmi_pd_fom_norpc(false);
	__fdmi_pd_fom_norpc(true);
}

/*----------------------------------------
  fdmi_pd_rec_inject_fini
  ----------------------------------------*/

void fdmi_pd_rec_inject_fini(void) {
	struct m0_fop              *fop;
	struct m0_fdmi_record_reg  *rreg;
	struct m0_fop_fdmi_record  *rec;

	M0_ALLOC_PTR(rec);
	M0_UT_ASSERT(rec != NULL);
	M0_SET0(rec);
	rec->fr_rec_id       = frid;
	rec->fr_rec_type     = M0_FDMI_REC_TYPE_FOL;
	rec->fr_matched_flts = farr;

	fop = m0_fop_alloc(&m0_fop_fdmi_rec_not_fopt, rec, (void*)1);
	M0_UT_ASSERT(fop != NULL);
	fop->f_item.ri_rmachine = NULL;

	rreg = m0_fdmi__pdock_fdmi_record_register(fop);
	M0_UT_ASSERT(rreg != NULL);

	rreg->frr_ep_addr = m0_strdup("test");

	m0_fdmi__plugin_dock_fini();
	m0_free(rec);
	m0_free(fop);

	/* Init pd back, its used in other tests. */
	m0_fdmi__plugin_dock_init();
}

/*----------------------------------------
  fdmi_pd_fake_rec_reg
  ----------------------------------------*/

static
struct pdock_client_conn {
	struct m0_rpc_conn    pc_conn;
	struct m0_rpc_session pc_sess;
}  g_cc;
static struct test_rpc_env         g_rpc_env;
static struct m0_rpc_packet       *g_rpc_packet;

static void my_item_done(struct m0_rpc_packet *p,
			 struct m0_rpc_item *item, int rc)
{
	m0_rpc_item_change_state(item, M0_RPC_ITEM_SENT);
	m0_rpc_item_timer_stop(item);
	m0_rpc_conn_ha_timer_stop(item2conn(item));
}

static int my_packet_ready(struct m0_rpc_packet *p)
{
	g_rpc_packet = p;
	m0_rpc_packet_traverse_items(p, &my_item_done, 0);
	return 0;
}


const struct m0_rpc_frm_ops frm_ops = {
	.fo_packet_ready = my_packet_ready,
};


extern struct fdmi_sd_ut_ctx       g_sd_ut;
extern const struct m0_filterc_ops filterc_send_notif_ops;


void fdmi_pd_fake_rec_reg(void)
{
	struct m0_fop             *fop;

	M0_ENTRY();

	fdmi_serv_start_ut(&filterc_send_notif_ops);
	prepare_rpc_env(&g_rpc_env, &g_sd_ut.mero.cc_reqh_ctx.rc_reqh,
			&frm_ops, true, &g_cc.pc_conn, &g_cc.pc_sess);
	fop = m0_fop_alloc(&m0_fop_fdmi_rec_not_fopt, NULL,
			   &g_rpc_env.tre_rpc_machine);
	M0_UT_ASSERT(fop != 0);
	m0_fop_to_rpc_item(fop)->ri_session  = &g_cc.pc_sess;
	m0_free(fop);
	unprepare_rpc_env(&g_rpc_env);
	fdmi_serv_stop_ut();

	M0_LEAVE();
}

/*----------------------------------------
  fdmi_pd_fake_rec_release
  ----------------------------------------*/

void fdmi_pd_fake_rec_release(void)
{
	struct m0_fop               *fop;
	struct m0_fdmi_record_reg   *rreg;
	struct m0_fop_fdmi_record   *rec;
	const struct m0_fdmi_pd_ops *pdo = m0_fdmi_plugin_dock_api_get();
	struct m0_uint128            zero_frid = M0_UINT128(0, 0);

	M0_ENTRY();

	M0_ALLOC_PTR(rec);
	M0_UT_ASSERT(rec != NULL);
	M0_SET0(rec);
	rec->fr_rec_id       = frid;
	rec->fr_rec_type     = M0_FDMI_REC_TYPE_FOL;
	rec->fr_matched_flts = farr;

	fdmi_serv_start_ut(&filterc_send_notif_ops);

	prepare_rpc_env(&g_rpc_env, &g_sd_ut.mero.cc_reqh_ctx.rc_reqh,
			&frm_ops, true, &g_cc.pc_conn, &g_cc.pc_sess);

	fop = m0_fop_alloc(&m0_fop_fdmi_rec_not_fopt, rec,
			   &g_rpc_env.tre_rpc_machine);
	M0_UT_ASSERT(fop != 0);

	m0_fop_to_rpc_item(fop)->ri_session  = &g_cc.pc_sess;

	m0_fi_enable_off_n_on_m("m0_fdmi__pdock_fdmi_record_register",
				"fail_fdmi_rec_reg", 0, 1);
	rreg = m0_fdmi__pdock_fdmi_record_register(fop);
	M0_UT_ASSERT(rreg == NULL);
	m0_fi_disable("m0_fdmi__pdock_fdmi_record_register",
		      "fail_fdmi_rec_reg");

	rreg = m0_fdmi__pdock_fdmi_record_register(fop);
	M0_UT_ASSERT(rreg != NULL);

	/* lock reg to not allow release to happen */
	m0_ref_get(&rreg->frr_ref);
	M0_UT_ASSERT(m0_ref_read(&rreg->frr_ref) == 2);

	/* unknown record id */
	(*pdo->fpo_release_fdmi_rec)(&zero_frid, &ffid);
	M0_UT_ASSERT(m0_ref_read(&rreg->frr_ref) == 2);

	/* known record id */
	(*pdo->fpo_release_fdmi_rec)(&frid, &ffid);
	M0_UT_ASSERT(m0_ref_read(&rreg->frr_ref) == 1);

	m0_free(fop);
	m0_free(rec);

	unprepare_rpc_env(&g_rpc_env);
	fdmi_serv_stop_ut();

	M0_LEAVE();
}

/*----------------------------------------
  fdmi_pd_fake_release_rep
  ----------------------------------------*/

extern struct m0_rpc_item_ops release_ri_ops;
extern struct m0_fop_type m0_fop_fdmi_rec_release_rep_fopt;

void fdmi_pd_fake_release_rep(void)
{
	struct m0_fop              *fop;
	struct m0_fop              *rep_fop;
	struct m0_fdmi_record_reg  *rreg;
	struct m0_fop_fdmi_record  *rec;
	struct m0_rpc_item         *item;

	struct m0_fop_fdmi_rec_release rr;

	M0_ENTRY();

	M0_ALLOC_PTR(rec);
	M0_UT_ASSERT(rec != NULL);
	M0_SET0(rec);
	rec->fr_rec_id       = frid;
	rec->fr_rec_type     = M0_FDMI_REC_TYPE_FOL;
	rec->fr_matched_flts = farr;

	fdmi_serv_start_ut(&filterc_send_notif_ops);

	prepare_rpc_env(&g_rpc_env, &g_sd_ut.mero.cc_reqh_ctx.rc_reqh,
			&frm_ops, true, &g_cc.pc_conn, &g_cc.pc_sess);

	/* imitate fdmi rec notification fop */
	fop = m0_fop_alloc(&m0_fop_fdmi_rec_not_fopt, rec,
			   &g_rpc_env.tre_rpc_machine);
	M0_UT_ASSERT(fop != 0);

	item = m0_fop_to_rpc_item(fop);
	item->ri_session  = &g_cc.pc_sess;

	rreg = m0_fdmi__pdock_fdmi_record_register(fop);
	M0_UT_ASSERT(rreg != NULL);

	rreg->frr_fop = fop;
	rreg->frr_sess = item->ri_session;
	rreg->frr_ep_addr = m0_strdup(m0_rpc_item_remote_ep_addr(item));

	/* imitate reply fop */
	rr.frr_frid = frid;

	rep_fop = m0_fop_alloc(&m0_fop_fdmi_rec_release_rep_fopt, &rr,
			       &g_rpc_env.tre_rpc_machine);
	M0_UT_ASSERT(fop != 0);

	item = m0_fop_to_rpc_item(rep_fop);
	item->ri_session  = &g_cc.pc_sess;

	m0_sm_group_lock(&item->ri_rmachine->rm_sm_grp);

	/* imitate successful rpc reply */
	rreg->frr_sess = NULL;
 	(release_ri_ops.rio_replied)(m0_fop_to_rpc_item(rep_fop));

	/* imitate failed rpc reply */
	m0_fop_to_rpc_item(rep_fop)->ri_error = -1;
 	(release_ri_ops.rio_replied)(m0_fop_to_rpc_item(rep_fop));

	m0_sm_group_unlock(&item->ri_rmachine->rm_sm_grp);

	m0_free(rep_fop);

	unprepare_rpc_env(&g_rpc_env);
	fdmi_serv_stop_ut();

	M0_LEAVE();
}

/*----------------------------------------
  fdmi_pd_fake_release_nomem
 ----------------------------------------*/
struct m0_rpc_conn_pool *ut_pdock_conn_pool(void);

void fdmi_pd_fake_release_nomem()
{
	struct m0_fop               *fop;
	struct m0_fdmi_record_reg   *rreg;
	struct m0_fdmi_record_reg   *rreg_test;
	struct m0_fop_fdmi_record   *rec;
	const struct m0_fdmi_pd_ops *pdo = m0_fdmi_plugin_dock_api_get();
	struct m0_rpc_conn_pool     *conn_pool = ut_pdock_conn_pool();
	struct m0_rpc_conn_pool_item *pool_item;

	m0_fi_disable("m0_alloc", "fail_allocation");

	M0_ALLOC_PTR(rec);
	M0_UT_ASSERT(rec != NULL);
	M0_SET0(rec);
	rec->fr_rec_id       = M0_UINT128(0x1111, 0x2222);
	rec->fr_rec_type     = M0_FDMI_REC_TYPE_FOL;
	rec->fr_matched_flts = farr;


	fdmi_serv_start_ut(&filterc_send_notif_ops);

	/* injecting fake conn */
	M0_ALLOC_PTR(pool_item);
	rpc_conn_pool_items_tlink_init_at_tail(pool_item,
					       &conn_pool->cp_items);

	fop = m0_fop_alloc(&m0_fop_fdmi_rec_not_fopt, rec,
			   &g_rpc_env.tre_rpc_machine);
	M0_UT_ASSERT(fop != 0);

	m0_fop_to_rpc_item(fop)->ri_session = &pool_item->cpi_rpc_link.rlk_sess;
	m0_fop_to_rpc_item(fop)->ri_rmachine = &g_rpc_env.tre_rpc_machine;

	prepare_rpc_env(&g_rpc_env, &g_sd_ut.mero.cc_reqh_ctx.rc_reqh,
			&frm_ops, true,
			&pool_item->cpi_rpc_link.rlk_conn,
			&pool_item->cpi_rpc_link.rlk_sess);

	rreg = m0_fdmi__pdock_fdmi_record_register(fop);
	M0_UT_ASSERT(rreg != NULL);

	/* lock reg to not allow release to happen */
	M0_UT_ASSERT(m0_ref_read(&rreg->frr_ref) == 1);

	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 1, 1);
	(*pdo->fpo_release_fdmi_rec)(&rec->fr_rec_id, &ffid);

	/* record remains registered */
	rreg_test = m0_fdmi__pdock_record_reg_find(&rec->fr_rec_id);
	M0_UT_ASSERT(rreg_test != NULL);
	M0_UT_ASSERT(m0_ref_read(&rreg_test->frr_ref) == 0);

	m0_fi_disable("m0_alloc", "fail_allocation");


	m0_fi_enable_off_n_on_m("m0_rpc_conn_pool_get", "fail_conn_get", 0, 1);
	m0_ref_get(&rreg->frr_ref);
	(*pdo->fpo_release_fdmi_rec)(&rec->fr_rec_id, &ffid);

	/* record remains registered */
	rreg_test = m0_fdmi__pdock_record_reg_find(&rec->fr_rec_id);
	M0_UT_ASSERT(rreg_test != NULL);
	M0_UT_ASSERT(m0_ref_read(&rreg_test->frr_ref) == 0);

	m0_fi_disable("m0_rpc_conn_pool_get", "fail_conn_get");


	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 0, 1);
	m0_ref_get(&rreg->frr_ref);
	(*pdo->fpo_release_fdmi_rec)(&rec->fr_rec_id, &ffid);

	/* record remains registered */
	rreg_test = m0_fdmi__pdock_record_reg_find(&rec->fr_rec_id);
	M0_UT_ASSERT(rreg_test != NULL);
	M0_UT_ASSERT(m0_ref_read(&rreg_test->frr_ref) == 0);

	m0_fi_disable("m0_alloc", "fail_allocation");


	m0_ref_get(&rreg->frr_ref);

	m0_free(fop);

	unprepare_rpc_env(&g_rpc_env);

	rpc_conn_pool_items_tlink_del_fini(pool_item);
	m0_free(pool_item);
	pool_item = NULL;

	fdmi_serv_stop_ut();
}

struct m0_ut_suite fdmi_pd_ut = {
	.ts_name = "fdmi-pd-ut",
	.ts_tests = {
		{ "fdmi-pd-register-filter",    fdmi_pd_register_filter    },
		{ "fdmi-pd-fom-norpc",          fdmi_pd_fom_norpc          },
		{ "fdmi-pd-rec-inject-fini",    fdmi_pd_rec_inject_fini    },
		{ "fdmi-pd-fake-release-nomem", fdmi_pd_fake_release_nomem },
		{ "fdmi-pd-fake-release-rep",   fdmi_pd_fake_release_rep   },
		{ "fdmi-pd-fake-rec-release",   fdmi_pd_fake_rec_release   },
		{ "fdmi-pd-fake-rec-reg",       fdmi_pd_fake_rec_reg       },
		{ NULL, NULL },
	},
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
