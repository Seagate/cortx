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
 * Original author: Anup Barve <Anup_Barve@xyratex.com>
 * Original creation date: 09/24/2012
 */

#include "cm/cp.h"
#include "cm/cp.c"
#include "cm/ut/common_service.h"
#include "sns/cm/cp.h"
#include "ioservice/fid_convert.h"  /* m0_fid_gob_make */
#include "lib/fs.h"                 /* m0_file_read */
#include "ut/misc.h"                /* M0_UT_PATH */
#include "ut/ut.h"

static struct m0_semaphore sem;

/* Single thread test vars. */
static struct m0_sns_cm_cp       s_sns_cp;
struct m0_net_buffer             s_nb;
static struct m0_net_buffer_pool nbp;
static struct m0_cm_aggr_group   s_ag;

enum {
	THREADS_NR = 17,
};

static int ut_cp_service_start(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL);
	return 0;
}

static void ut_cp_service_stop(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL);
}

static void ut_cp_service_fini(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL);
	m0_free(service);
}

static const struct m0_reqh_service_ops ut_cp_service_ops = {
	.rso_start = ut_cp_service_start,
	.rso_stop  = ut_cp_service_stop,
	.rso_fini  = ut_cp_service_fini
};

static int ut_cp_service_allocate(struct m0_reqh_service **service,
				  const struct m0_reqh_service_type *stype)
{
	struct m0_reqh_service *serv;

	M0_PRE(stype != NULL && service != NULL);

	M0_ALLOC_PTR(serv);
	M0_ASSERT(serv != NULL);

	serv->rs_type = stype;
	serv->rs_ops = &ut_cp_service_ops;
	*service = serv;
	return 0;
}

static const struct m0_reqh_service_type_ops ut_cp_service_type_ops = {
	.rsto_service_allocate = ut_cp_service_allocate
};

struct m0_reqh_service_type ut_cp_service_type = {
	.rst_name  = "ut-cp",
	.rst_ops   = &ut_cp_service_type_ops,
	.rst_level = M0_RS_LEVEL_NORMAL,
};

/* Multithreaded test vars. */
static struct m0_sns_cm_cp m_sns_cp[THREADS_NR];
static struct m0_cm_aggr_group m_ag[THREADS_NR];
static struct m0_net_buffer m_nb[THREADS_NR];

static int dummy_cp_read(struct m0_cm_cp *cp)
{
	cp->c_io_op = M0_CM_CP_READ;
	cp->c_ops->co_phase_next(cp);
	return M0_FSO_AGAIN;
}

static int dummy_cp_write_pre(struct m0_cm_cp *cp)
{
	M0_IMPOSSIBLE("M0_CCP_WRITE_PRE phase shouldn't be used!");
}

static int dummy_cp_write(struct m0_cm_cp *cp)
{
	cp->c_io_op = M0_CM_CP_WRITE;
	cp->c_ops->co_phase_next(cp);
	return M0_FSO_AGAIN;
}

static int dummy_cp_phase(struct m0_cm_cp *cp)
{
	return cp->c_ops->co_phase_next(cp);
}

static int dummy_cp_init(struct m0_cm_cp *cp)
{
	int rc = m0_sns_cm_cp_init(cp);

	m0_semaphore_up(&sem);
	return rc;
}

static int dummy_cp_phase_next(struct m0_cm_cp *cp)
{
	int phase = m0_fom_phase(&cp->c_fom);

	if (phase == M0_CCP_XFORM) {
		phase = M0_CCP_WRITE;
		m0_fom_phase_set(&cp->c_fom, phase);
		return M0_FSO_AGAIN;
	} else
		return m0_sns_cm_cp_phase_next(cp);
}

const struct m0_cm_cp_ops m0_sns_cm_cp_dummy_ops = {
        .co_action = {
                [M0_CCP_INIT]         = &dummy_cp_init,
                [M0_CCP_READ]         = &dummy_cp_read,
                [M0_CCP_WRITE_PRE]    = &dummy_cp_write_pre,
                [M0_CCP_WRITE]        = &dummy_cp_write,
                [M0_CCP_IO_WAIT]      = &dummy_cp_phase,
                [M0_CCP_XFORM]        = &dummy_cp_phase,
                [M0_CCP_SW_CHECK]     = &dummy_cp_phase,
                [M0_CCP_SEND]         = &dummy_cp_phase,
		[M0_CCP_SEND_WAIT]    = &dummy_cp_phase,
		[M0_CCP_RECV_INIT]    = &dummy_cp_phase,
		[M0_CCP_RECV_WAIT]    = &dummy_cp_phase,
                [M0_CCP_FINI]         = &m0_sns_cm_cp_fini,
        },
        .co_action_nr          = M0_CCP_NR,
        .co_phase_next         = &dummy_cp_phase_next,
        .co_invariant          = &m0_sns_cm_cp_invariant,
        .co_home_loc_helper    = &cp_home_loc_helper,
        .co_complete           = &m0_sns_cm_cp_complete,
        .co_free               = &m0_sns_cm_cp_free,
};

/*
 * Dummy fom fini function which finalises the copy packet by skipping the
 * sw_fill functionality.
 */
void dummy_cp_fom_fini(struct m0_fom *fom)
{
	m0_cm_cp_fini(bob_of(fom, struct m0_cm_cp, c_fom, &cp_bob));
}

/*
 * Over-ridden copy packet FOM ops.
 * This is done to bypass the sw_ag_fill call, which is to be tested
 * separately.
 */
static struct m0_fom_ops dummy_cp_fom_ops = {
	.fo_fini          = dummy_cp_fom_fini,
	.fo_tick          = cp_fom_tick,
	.fo_home_locality = cp_fom_locality
};

/*
 * Populates the copy packet and queues it to the request handler
 * for processing.
 */
static void cp_post(struct m0_sns_cm_cp *sns_cp, struct m0_cm_aggr_group *ag,
		    struct m0_net_buffer *nb)
{
	struct m0_cm_cp *cp;
	struct m0_fid    gfid;

	m0_fid_gob_make(&gfid, 1, 1);
	cp = &sns_cp->sc_base;
	cp->c_ag = ag;
	m0_stob_id_make(0, 1, &gfid, &sns_cp->sc_stob_id);
	m0_fid_convert_gob2cob(&gfid, &sns_cp->sc_cobfid, 1);
	cp->c_ops = &m0_sns_cm_cp_dummy_ops;
	m0_cm_cp_init(&cm_ut_cmt, NULL);
	m0_cm_cp_fom_init(ag->cag_cm, cp, NULL, NULL);
	/* Over-ride the fom ops. */
	cp->c_fom.fo_ops = &dummy_cp_fom_ops;
	m0_cm_cp_buf_add(cp, nb);
	m0_fom_queue(&cp->c_fom);
	m0_semaphore_down(&sem);
}

/*
 * Tests the copy packet fom functionality by posting a single copy packet
 * to the reqh.
 */
static void test_cp_single_thread(void)
{
	m0_semaphore_init(&sem, 0);
	s_ag.cag_cm = &cm_ut[0].ut_cm;
	s_ag.cag_cp_local_nr = 1;
	s_nb.nb_pool = &nbp;
	cp_post(&s_sns_cp, &s_ag, &s_nb);

        /*
         * Wait until all the foms in the request handler locality runq are
         * processed.
         */
        m0_reqh_idle_wait(&cmut_rmach_ctx.rmc_reqh);
	m0_semaphore_fini(&sem);
}

static void cp_op(const int tid)
{
	m_ag[tid].cag_cm = &cm_ut[0].ut_cm;
	m_ag[tid].cag_cp_local_nr = 1;
	m_nb[tid].nb_pool = &nbp;
	cp_post(&m_sns_cp[tid], &m_ag[tid], &m_nb[tid]);
}

/*
 * Tests the copy packet fom functionality by posting multiple copy packets
 * to the reqh.
 */
static void test_cp_multi_thread(void)
{
	int               i;
	struct m0_thread *cp_thread;

	m0_semaphore_init(&sem, 0);

        M0_ALLOC_ARR(cp_thread, THREADS_NR);
        M0_UT_ASSERT(cp_thread != NULL);

	/* Post multiple copy packets to the request handler queue. */
	for (i = 0; i < THREADS_NR; ++i)
		M0_UT_ASSERT(M0_THREAD_INIT(&cp_thread[i], int, NULL, &cp_op, i,
					    "cp_thread_%d", i) == 0);

	for (i = 0; i < THREADS_NR; ++i)
		m0_thread_join(&cp_thread[i]);
        /*
         * Wait until all the foms in the request handler locality runq are
         * processed.
         */
        m0_reqh_idle_wait(&cmut_rmach_ctx.rmc_reqh);
        m0_free(cp_thread);
	m0_semaphore_fini(&sem);
}
/*
 * Initialises the request handler since copy packet fom has to be tested using
 * request handler infrastructure.
 */
static int cm_cp_init(void)
{
	int rc;

	M0_SET0(&cmut_rmach_ctx);
	cmut_rmach_ctx.rmc_cob_id.id = DUMMY_COB_ID;
	cmut_rmach_ctx.rmc_ep_addr   = DUMMY_SERVER_ADDR;
	m0_ut_rpc_mach_init_and_add(&cmut_rmach_ctx);
	rc = m0_cm_type_register(&cm_ut_cmt);
	M0_ASSERT(rc == 0);
	cm_ut_service_alloc_init(&cmut_rmach_ctx.rmc_reqh);

	rc = m0_reqh_service_start(cm_ut_service);
	M0_ASSERT(rc == 0);

	return 0;
}

/* Finalises the request handler. */
static int cm_cp_fini(void)
{
	cm_ut_service_cleanup();
	m0_cm_type_deregister(&cm_ut_cmt);
	m0_ut_rpc_mach_fini(&cmut_rmach_ctx);
	return 0;
}

struct m0_ut_suite cm_cp_ut = {
        .ts_name = "cm-cp-ut",
        .ts_init = &cm_cp_init,
        .ts_fini = &cm_cp_fini,
        .ts_tests = {
                { "cp-single_thread", test_cp_single_thread },
                { "cp-multi_thread", test_cp_multi_thread },
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
