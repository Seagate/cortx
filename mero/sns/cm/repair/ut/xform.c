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
 * Original creation date: 08/16/2012
 */

#include "lib/locality.h"
#include "lib/finject.h"
#include "ioservice/io_service.h"
#include "mero/setup.h"
#include "sns/cm/repair/xform.c"
#include "sns/cm/repair/ut/cp_common.h"
#include "ioservice/fid_convert.h"	/* m0_fid_convert_cob2stob */
#include "ut/stob.h"			/* m0_ut_stob_create_by_stob_id */
#include "module/instance.h"            /* m0_get */

enum {
	SEG_NR                  = 16,
	SEG_SIZE                = 4096,
	BUF_NR                  = 2,
	DATA_NR                 = 5,
        PARITY_NR               = 2,
	CP_SINGLE               = 1,
	SINGLE_FAILURE          = 1,
	MULTI_FAILURES          = 2,
	SINGLE_FAIL_MULTI_CP_NR = 512,
	MULTI_FAIL_MULTI_CP_NR  = 5,
};

static struct m0_fid gob_fid;
static struct m0_fid cob_fid;

static struct m0_reqh            *reqh;
struct m0_pdclust_layout         *pdlay;
static struct m0_cm              *cm;
static struct m0_sns_cm          *scm;
static struct m0_reqh_service    *scm_service;
static struct m0_cob_domain      *cdom;
static struct m0_semaphore        sem;
static struct m0_net_buffer_pool  nbp;

/* Global structures for testing bufvec xor correctness. */
static struct m0_bufvec src;
static struct m0_bufvec dst;
static struct m0_bufvec xor;

/* Global structures for single copy packet test. */
static struct m0_sns_cm_repair_ag             s_rag;
static struct m0_sns_cm_repair_ag_failure_ctx s_fc[SINGLE_FAILURE];
static struct m0_sns_cm_cp                    s_cp;
static struct m0_net_buffer                   s_buf;
static struct m0_net_buffer                   s_acc_buf;

/*
 * Global structures for multiple copy packet test comprising of single
 * failure.
 */
static struct m0_sns_cm_repair_ag             m_rag;
static struct m0_sns_cm_repair_ag_failure_ctx m_fc[SINGLE_FAILURE];
static struct m0_sns_cm_cp                    m_cp[SINGLE_FAIL_MULTI_CP_NR];
static struct m0_net_buffer                   m_buf[SINGLE_FAIL_MULTI_CP_NR];
static struct m0_net_buffer                   m_acc_buf[SINGLE_FAILURE];

/*
 * Global structures for multiple copy packet test comprising of multiple
 * failures.
 */
static struct m0_sns_cm_repair_ag             n_rag;
static struct m0_sns_cm_repair_ag_failure_ctx n_fc[MULTI_FAILURES];
static struct m0_sns_cm_cp                    n_cp[MULTI_FAIL_MULTI_CP_NR];
static struct m0_net_buffer                   n_buf[MULTI_FAIL_MULTI_CP_NR][BUF_NR];
static struct m0_net_buffer                   n_acc_buf[MULTI_FAILURES][BUF_NR];

M0_INTERNAL void cob_create(struct m0_reqh *reqh, struct m0_cob_domain *cdom,
			    struct m0_be_domain *bedom,
                            uint64_t cont, struct m0_fid *gfid,
			    uint32_t cob_idx);
M0_INTERNAL void cob_delete(struct m0_cob_domain *cdom,
			    struct m0_be_domain *bedom,
			    uint64_t cont, const struct m0_fid *gfid);

static uint64_t cp_single_get(const struct m0_cm_aggr_group *ag)
{
	return CP_SINGLE;
}

static const struct m0_cm_aggr_group_ops group_single_ops = {
	.cago_local_cp_nr = &cp_single_get,
};

static uint64_t single_fail_multi_cp_get(const struct m0_cm_aggr_group *ag)
{
	return SINGLE_FAIL_MULTI_CP_NR;
}

static const struct m0_cm_aggr_group_ops group_single_fail_multi_cp_ops = {
	.cago_local_cp_nr = &single_fail_multi_cp_get,
};

static uint64_t multi_fail_multi_cp_get(const struct m0_cm_aggr_group *ag)
{
	return MULTI_FAIL_MULTI_CP_NR;
}

static const struct m0_cm_aggr_group_ops group_multi_fail_multi_cp_ops = {
	.cago_local_cp_nr = &multi_fail_multi_cp_get,
};

static size_t dummy_fom_locality(const struct m0_fom *fom)
{
	/* By default, use locality0. */
	return 0;
}

/* Dummy fom state routine to emulate only selective copy packet states. */
static int dummy_fom_tick(struct m0_fom *fom)
{
	struct m0_cm_cp *cp = container_of(fom, struct m0_cm_cp, c_fom);

	switch (m0_fom_phase(fom)) {
	case M0_FOM_PHASE_INIT:
		m0_fom_phase_set(fom, M0_CCP_XFORM);
		m0_semaphore_up(&sem);
		return cp->c_ops->co_action[M0_CCP_XFORM](cp);
	case M0_CCP_WRITE:
		m0_fom_phase_set(fom, M0_CCP_IO_WAIT);
		return M0_FSO_AGAIN;
	case M0_CCP_IO_WAIT:
		m0_fom_phase_set(fom, M0_CCP_FINI);
		return M0_FSO_WAIT;
	default:
		M0_IMPOSSIBLE("Bad State");
		return 0;
	}
}

static int dummy_acc_cp_fom_tick(struct m0_fom *fom)
{
	switch (m0_fom_phase(fom)) {
	case M0_FOM_PHASE_INIT:
		m0_fom_phase_set(fom, M0_CCP_WRITE);
		m0_semaphore_up(&sem);
		return M0_FSO_AGAIN;
	case M0_CCP_WRITE:
		m0_fom_phase_set(fom, M0_CCP_IO_WAIT);
		return M0_FSO_AGAIN;
	case M0_CCP_IO_WAIT:
		m0_fom_phase_set(fom, M0_CCP_FINI);
		return M0_FSO_WAIT;
	default:
		M0_IMPOSSIBLE("Bad State");
		return 0;
	}
}

static void single_cp_fom_fini(struct m0_fom *fom)
{
	struct m0_cm_cp *cp = container_of(fom, struct m0_cm_cp, c_fom);

	m0_cm_cp_fini(cp);
}

static void multiple_cp_fom_fini(struct m0_fom *fom)
{
	struct m0_cm_cp *cp = container_of(fom, struct m0_cm_cp, c_fom);

	m0_cm_cp_fini(cp);
}

/* Over-ridden copy packet FOM ops. */
static struct m0_fom_ops single_cp_fom_ops = {
	.fo_fini          = single_cp_fom_fini,
	.fo_tick          = dummy_fom_tick,
	.fo_home_locality = dummy_fom_locality
};

static struct m0_fom_ops acc_cp_fom_ops = {
	.fo_fini          = single_cp_fom_fini,
	.fo_tick          = dummy_acc_cp_fom_tick,
	.fo_home_locality = dummy_fom_locality
};

/* Over-ridden copy packet FOM ops. */
static struct m0_fom_ops multiple_cp_fom_ops = {
	.fo_fini          = multiple_cp_fom_fini,
	.fo_tick          = dummy_fom_tick,
	.fo_home_locality = dummy_fom_locality
};

/*
static bool dummy_xform_ut_accumulator_is_full(const struct m0_sns_cm_ag *sag,
					       int xform_cp_nr)
{
        uint64_t global_cp_nr = sag->sag_base.cag_cp_global_nr;

        return xform_cp_nr == global_cp_nr - sag->sag_fnr ? true : false;
}
*/
const struct m0_sns_cm_helpers xform_ut_repair_helpers = {
};

static void cp_buf_free(struct m0_sns_cm_ag *sag)
{
	struct m0_cm_cp            *acc_cp;
	struct m0_net_buffer       *nbuf;
	struct m0_sns_cm_repair_ag *rag;
	int                         i;

	rag = sag2repairag(sag);
	for (i = 0; i < sag->sag_fnr; ++i) {
		acc_cp = &rag->rag_fc[i].fc_tgt_acc_cp.sc_base;
		m0_tl_for(cp_data_buf, &acc_cp->c_buffers, nbuf) {
			m0_bufvec_free(&nbuf->nb_buffer);
		} m0_tl_endfor;
	}
}

static void tgt_fid_cob_create(struct m0_reqh *reqh)
{
	struct m0_stob_id stob_id;
        int		  rc;

	m0_ios_cdom_get(reqh, &cdom);
        cob_create(reqh, cdom, reqh->rh_beseg->bs_domain, 0, &gob_fid,
		   m0_fid_cob_device_id(&cob_fid));
	m0_fid_convert_cob2stob(&cob_fid, &stob_id);
	rc = m0_ut_stob_create_by_stob_id(&stob_id, NULL);
	M0_ASSERT(rc == 0);
}

static void ag_init(struct m0_sns_cm_repair_ag *rag)
{
	struct m0_cm_aggr_group *ag = &rag->rag_base.sag_base;

	/* Workaround to avoid lock of uninitialised mutex */
	m0_mutex_init(&ag->cag_mutex);
}

static void ag_fini(struct m0_sns_cm_repair_ag *rag)
{
	struct m0_cm_aggr_group *ag = &rag->rag_base.sag_base;

	m0_mutex_fini(&ag->cag_mutex);
}

static void ag_prepare(struct m0_sns_cm_repair_ag *rag, int failure_nr,
		       const struct m0_cm_aggr_group_ops *ag_ops,
		       struct m0_sns_cm_repair_ag_failure_ctx *fc)
{
	struct m0_sns_cm_ag *sag;
	int                  i;

	sag = &rag->rag_base;
	sag->sag_base.cag_transformed_cp_nr = 0;
	sag->sag_fnr = failure_nr;
	sag->sag_base.cag_ops = ag_ops;
	sag->sag_base.cag_cp_local_nr =
		sag->sag_base.cag_ops->cago_local_cp_nr(&sag->sag_base);
	sag->sag_base.cag_cp_global_nr = sag->sag_base.cag_cp_local_nr +
					 failure_nr;
	for (i = 0; i < failure_nr; ++i) {
		fc[i].fc_tgt_cobfid = cob_fid;
		fc[i].fc_is_inuse = true;
	}
	rag->rag_fc = fc;
}

/* Tests the correctness of the bufvec_xor function. */
static void test_bufvec_xor()
{
	bv_alloc_populate(&src, '4', SEG_NR, SEG_SIZE);
	bv_alloc_populate(&dst, 'D', SEG_NR, SEG_SIZE);
	/*
	 * Actual result is anticipated and stored in new bufvec, which is
	 * used for comparison with xor'ed output.
	 * 4 XOR D = p
	 */
	bv_alloc_populate(&xor, 'p', SEG_NR, SEG_SIZE);
	bufvec_xor(&dst, &src, SEG_SIZE * SEG_NR);
	bv_compare(&dst, &xor, SEG_NR, SEG_SIZE);
	bv_free(&src);
	bv_free(&dst);
	bv_free(&xor);
}

/*
 * Test to check that single copy packet is treated as passthrough by the
 * transformation function.
 */
static void test_single_cp(void)
{
	struct m0_sns_cm_ag       *sag;
	struct m0_cm_cp           *cp = &s_cp.sc_base;
	struct m0_sns_cm_file_ctx  fctx;

	m0_semaphore_init(&sem, 0);
	ag_prepare(&s_rag, SINGLE_FAILURE, &group_single_ops, s_fc);
	s_acc_buf.nb_pool = &nbp;
	s_buf.nb_pool = &nbp;
	fctx.sf_layout = m0_pdl_to_layout(pdlay);
	sag = &s_rag.rag_base;
	sag->sag_fctx = &fctx;
	cp_prepare(cp, &s_buf, SEG_NR, SEG_SIZE, sag, 'e',
		   &single_cp_fom_ops, reqh, 0, false, NULL);
	cp_prepare(&s_fc[0].fc_tgt_acc_cp.sc_base, &s_acc_buf, SEG_NR,
		   SEG_SIZE, sag, 0, &acc_cp_fom_ops, reqh, 0, true, NULL);
	m0_bitmap_init(&s_fc[0].fc_tgt_acc_cp.sc_base.c_xform_cp_indices,
		       sag->sag_base.cag_cp_global_nr);
	s_cp.sc_is_local = true;
	m0_fom_queue(&cp->c_fom);

	/* Wait till ast gets posted. */
	m0_semaphore_down(&sem);
	m0_reqh_idle_wait(reqh);

	/*
	 * These asserts ensure that the single copy packet has been treated
	 * as passthrough.
	 */
	M0_UT_ASSERT(sag->sag_base.cag_transformed_cp_nr == CP_SINGLE);
	M0_UT_ASSERT(sag->sag_base.cag_cp_local_nr == CP_SINGLE);
	m0_semaphore_fini(&sem);
	bv_free(&s_buf.nb_buffer);
	cp_buf_free(sag);
}

/*
 * Test to check that multiple copy packets are collected by the
 * transformation function, in case of single failure.
 */
static void test_multi_cp_single_failure(void)
{
	struct m0_sns_cm_ag       *sag;
	struct m0_cm_cp           *cp;
	struct m0_sns_cm_file_ctx  fctx;
	int                        i;

	m0_semaphore_init(&sem, 0);
	ag_prepare(&m_rag, SINGLE_FAILURE, &group_single_fail_multi_cp_ops,
		   m_fc);
	m_acc_buf[0].nb_pool = &nbp;
	sag = &m_rag.rag_base;
	fctx.sf_layout = m0_pdl_to_layout(pdlay);
	sag->sag_fctx = &fctx;
	cp_prepare(&m_fc[0].fc_tgt_acc_cp.sc_base, &m_acc_buf[0], SEG_NR,
		   SEG_SIZE, sag, 0, &acc_cp_fom_ops, reqh, 0, true, NULL);
	m0_bitmap_init(&m_fc[0].fc_tgt_acc_cp.sc_base.c_xform_cp_indices,
		       sag->sag_base.cag_cp_global_nr);
	for (i = 0; i < SINGLE_FAIL_MULTI_CP_NR; ++i) {
		m_buf[i].nb_pool = &nbp;
		cp = &m_cp[i].sc_base;
		m_cp[i].sc_is_local = true;
		cp_prepare(cp, &m_buf[i], SEG_NR, SEG_SIZE, sag,
			   'r', &multiple_cp_fom_ops, reqh, i, false, NULL);
		m0_fom_queue(&cp->c_fom);
		m0_semaphore_down(&sem);
	}
	m0_reqh_idle_wait(reqh);

	/*
	 * These asserts ensure that all the copy packets have been collected
	 * by the transformation function.
	 */
	M0_UT_ASSERT(sag->sag_base.cag_transformed_cp_nr ==
		     SINGLE_FAIL_MULTI_CP_NR);
	M0_UT_ASSERT(sag->sag_base.cag_cp_local_nr == SINGLE_FAIL_MULTI_CP_NR);
	m0_semaphore_fini(&sem);
	for (i = 0; i < SINGLE_FAIL_MULTI_CP_NR; ++i)
		bv_free(&m_buf[i].nb_buffer);

	cp_buf_free(sag);
}

static void rs_init()
{
	uint64_t local_cp_nr;

	local_cp_nr = n_rag.rag_base.sag_base.cag_cp_local_nr;
	M0_UT_ASSERT(m0_parity_math_init(&n_rag.rag_math, DATA_NR,
					 PARITY_NR) == 0);
	M0_UT_ASSERT(m0_sns_ir_init(&n_rag.rag_math, local_cp_nr,
				    &n_rag.rag_ir) == 0);
}

static void buffers_attach(struct m0_net_buffer *nb, struct m0_cm_cp *cp,
			   char data)
{
	int i;

        for (i = 1; i < BUF_NR; ++i) {
                bv_alloc_populate(&nb[i].nb_buffer, data, SEG_NR, SEG_SIZE);
                m0_cm_cp_buf_add(cp, &nb[i]);
                nb[i].nb_pool = &nbp;
                nb[i].nb_pool->nbp_seg_nr = SEG_NR;
                nb[i].nb_pool->nbp_seg_size = SEG_SIZE;
                nb[i].nb_pool->nbp_buf_nr = 1;
        }
}

/*
 * Creates a copy packet and queues it for execution.
 */
static void cp_multi_failures_post(char data, int cnt, int index)
{
	struct m0_sns_cm_ag *sag;
	struct m0_cm_cp *cp;

	n_buf[cnt][0].nb_pool = &nbp;
	sag = &n_rag.rag_base;
	cp = &n_cp[cnt].sc_base;
	cp_prepare(cp, &n_buf[cnt][0], SEG_NR, SEG_SIZE,
		   sag, data, &multiple_cp_fom_ops, reqh, index,
		   false, NULL);
	buffers_attach(n_buf[cnt], cp, data);

	cp->c_data_seg_nr = SEG_NR * BUF_NR;
	n_cp[cnt].sc_is_local = true;
	m0_bitmap_init(&cp->c_xform_cp_indices,
			sag->sag_base.cag_cp_global_nr);
	m0_bitmap_set(&cp->c_xform_cp_indices, index, true);
	m0_fom_queue(&cp->c_fom);
	m0_semaphore_down(&sem);
}

/*
 * Test to check that multiple copy packets are collected by the
 * transformation function, in case of multiple failures.
 */
static void test_multi_cp_multi_failures(void)
{
	int                       i;
	int                       j;
	struct m0_net_buffer     *nbuf;
	struct m0_sns_cm_ag      *sag;
	struct m0_sns_cm_file_ctx fctx;
	struct m0_pool_version    pv;
	struct m0_poolmach        pm;

	m0_fi_enable("m0_sns_cm_tgt_ep", "local-ep");
	m0_semaphore_init(&sem, 0);
	ag_prepare(&n_rag, MULTI_FAILURES, &group_multi_fail_multi_cp_ops,
		   n_fc);
	sag = &n_rag.rag_base;
	pm.pm_pver = &pv;
	fctx.sf_pm = &pm;
	fctx.sf_layout = m0_pdl_to_layout(pdlay);
	sag->sag_fctx = &fctx;
	for (i = 0; i < MULTI_FAILURES; ++i) {
		n_acc_buf[i][0].nb_pool = &nbp;
		cp_prepare(&n_fc[i].fc_tgt_acc_cp.sc_base, &n_acc_buf[i][0],
			   SEG_NR, SEG_SIZE, sag, 0, &acc_cp_fom_ops,
			   reqh, 0, true, NULL);
		buffers_attach(n_acc_buf[i], &n_fc[i].fc_tgt_acc_cp.sc_base, 0);
		m0_bitmap_init(&n_fc[i].fc_tgt_acc_cp.sc_base.c_xform_cp_indices,
			       sag->sag_base.cag_cp_global_nr);
		n_fc[i].fc_is_inuse = true;
		m0_cm_cp_bufvec_merge(&n_fc[i].fc_tgt_acc_cp.sc_base);
	}

	rs_init();

	/*
	 * The test case is illustrated as follows.
	 * N = 5, K = 2
	 *
	 * According to RS encoding,
	 * if d0 = 'r', d1 = 's', d2 = 't', d3 = 'u', d4 = 'v', then
	 *    p1 = 'w' and p2 = 'p'
	 * Consider,
	 * D0 = BUF_NR buffers having data = 'r' (index in ag = 0)
	 * D1 = FAILED                           (index in ag = 1)
	 * D2 = BUF_NR buffers having data = 't' (index in ag = 2)
	 * D3 = BUF_NR buffers having data = 'u' (index in ag = 3)
	 * D4 = BUF_NR buffers having data = 'v' (index in ag = 4)
	 * P1 = BUF_NR buffers having data = 'w' (index in ag = 5)
	 * P2 = FAILED                           (index in ag = 6)
	 *
	 * In above case, after recovery, the accumulator buffers should have
	 * recovered values of D1 and P2 respectively.
	 */
	M0_UT_ASSERT(m0_sns_ir_failure_register(&n_acc_buf[0][0].nb_buffer,
						        1, &n_rag.rag_ir) == 0);
	n_rag.rag_fc[0].fc_failed_idx = 1;
	M0_UT_ASSERT(m0_sns_ir_failure_register(&n_acc_buf[1][0].nb_buffer,
						        6, &n_rag.rag_ir) == 0);
	n_rag.rag_fc[0].fc_failed_idx = 6;
	M0_UT_ASSERT(m0_sns_ir_mat_compute(&n_rag.rag_ir) == 0);

	cp_multi_failures_post('r', 0, 0);
	cp_multi_failures_post('t', 1, 2);
	cp_multi_failures_post('u', 2, 3);
	cp_multi_failures_post('v', 3, 4);
	cp_multi_failures_post('w', 4, 5);

	m0_reqh_idle_wait(reqh);

	/* Verify that first accumulator contains recovered data for D1. */
	bv_alloc_populate(&src, 's', SEG_NR, SEG_SIZE);
	m0_tl_for(cp_data_buf, &n_rag.rag_fc[0].fc_tgt_acc_cp.sc_base.c_buffers,
		  nbuf) {
		bv_compare(&src, &nbuf->nb_buffer, SEG_NR, SEG_SIZE);
	} m0_tl_endfor;
	bv_free(&src);

	/* Verify that first accumulator contains recovered data for P2. */
	bv_alloc_populate(&src, 'p', SEG_NR, SEG_SIZE);
	m0_tl_for(cp_data_buf, &n_rag.rag_fc[1].fc_tgt_acc_cp.sc_base.c_buffers,
		  nbuf) {
		bv_compare(&src, &nbuf->nb_buffer, SEG_NR, SEG_SIZE);
	} m0_tl_endfor;
	bv_free(&src);

        /*
         * These asserts ensure that all the copy packets have been collected
         * by the transformation function.
         */
        m0_semaphore_fini(&sem);

        for (i = 0; i < MULTI_FAIL_MULTI_CP_NR; ++i)
		for (j = 0; j < BUF_NR; ++j)
			bv_free(&n_buf[i][j].nb_buffer);
        m0_sns_ir_fini(&n_rag.rag_ir);
        m0_parity_math_fini(&n_rag.rag_math);

        M0_UT_ASSERT(sag->sag_base.cag_transformed_cp_nr ==
		     MULTI_FAIL_MULTI_CP_NR);
        M0_UT_ASSERT(sag->sag_base.cag_cp_local_nr == MULTI_FAIL_MULTI_CP_NR);
	cp_buf_free(sag);
	m0_fi_disable("m0_sns_cm_tgt_ep", "local-ep");
}

/*
 * Initialises the request handler since copy packet fom has to be tested using
 * request handler infrastructure.
 */
static int xform_init(void)
{
	int rc;

	rc = cs_init(&sctx);
	M0_ASSERT(rc == 0);

	m0_fid_gob_make(&gob_fid, 0, 4);
	m0_fid_convert_gob2cob(&gob_fid, &cob_fid, 1);

	reqh = m0_cs_reqh_get(&sctx);
	layout_gen(&pdlay, reqh);
	tgt_fid_cob_create(reqh);

	scm_service = m0_reqh_service_find(
		m0_reqh_service_type_find("M0_CST_SNS_REP"), reqh);
        M0_ASSERT(scm_service != NULL);

        cm = container_of(scm_service, struct m0_cm, cm_service);
        M0_ASSERT(cm != NULL);

        scm = cm2sns(cm);
	scm->sc_cob_dom = cdom;
	scm->sc_helpers = &xform_ut_repair_helpers;

	ag_init(&s_rag);
	ag_init(&m_rag);
	ag_init(&n_rag);

	return 0;
}

static int xform_fini(void)
{
	struct m0_cob_domain *cdom;
	struct m0_stob_id     stob_id;
	int                   rc;

	ag_fini(&n_rag);
	ag_fini(&m_rag);
	ag_fini(&s_rag);

	m0_fid_convert_cob2stob(&cob_fid, &stob_id);
	rc = m0_ut_stob_destroy_by_stob_id(&stob_id);
	M0_UT_ASSERT(rc == 0);
	m0_ios_cdom_get(reqh, &cdom);
	cob_delete(cdom, reqh->rh_beseg->bs_domain, 0, &gob_fid);
	layout_destroy(pdlay);
        cs_fini(&sctx);
        return 0;
}

struct m0_ut_suite snscm_xform_ut = {
	.ts_name = "snscm_xform-ut",
	.ts_init = &xform_init,
	.ts_fini = &xform_fini,
	.ts_tests = {
		{ "bufvec_xor_correctness", test_bufvec_xor },
		{ "single_cp_passthrough", test_single_cp },
		{ "multi_cp_single_failure",
			test_multi_cp_single_failure },
		{ "multi_cp_multi_failures",
			test_multi_cp_multi_failures },
		{ NULL, NULL }
	}
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
