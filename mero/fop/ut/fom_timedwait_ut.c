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
 * Original author: Egor Nikulenkov <egor.nikulenkov@seagate.com>
 * Original creation date: 19-Mar-2017
 */

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/semaphore.h"
#include "lib/finject.h"
#include "rpc/rpc_opcodes.h"
#include "fop/fom.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "ut/ut.h"

enum tw_test {
	TW_TEST_IMMED_SUCCESS,
	TW_TEST_DELAYED_SUCCESS,
	TW_TEST_IMMED_TIMEOUT,
	TW_TEST_DELAYED_TIMEOUT,
	TW_TEST_IMMED_UNREACHABLE,
	TW_TEST_DELAYED_UNREACHABLE
};

struct tw_fom {
        struct m0_fom         tw_fom;
	enum tw_test          tw_test;
	struct m0_sm_ast      tw_wakeup;
	struct m0_fom_timeout tw_timeout;
	struct m0_semaphore   tw_sem_init;
	struct m0_semaphore   tw_sem_fini;
	struct m0_semaphore   tw_sem_timeout;
};

enum tw_fom_phase {
	INIT    = M0_FOM_PHASE_INIT,
	FINISH  = M0_FOM_PHASE_FINISH,
	PHASE1  = M0_FOM_PHASE_NR,
	PHASE2,
};

static struct m0_sm_state_descr tw_fom_phases[] = {
	[INIT] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "init",
		.sd_allowed = M0_BITS(PHASE1, PHASE2)
	},
	[PHASE1] = {
		.sd_name    = "phase1",
		.sd_allowed = M0_BITS(PHASE2)
	},
	[PHASE2] = {
		.sd_name    = "phase2",
		.sd_allowed = M0_BITS(FINISH)
	},
	[FINISH] = {
		.sd_name    = "finish",
		.sd_flags   = M0_SDF_TERMINAL,
	}
};

static struct m0_sm_conf tw_sm_conf = {
	.scf_name      = "tw_fom",
	.scf_nr_states = ARRAY_SIZE(tw_fom_phases),
	.scf_state     = tw_fom_phases,
};

static struct m0_fom_type tw_fomt;

static struct m0_reqh          twreqh;
static struct m0_reqh_service *twsvc;

static void   tw_fom_fini(struct m0_fom *fom);
static int    tw_fom_tick(struct m0_fom *fom);
static size_t tw_fom_home_locality(const struct m0_fom *fom);

static const struct m0_fom_ops tw_fom_ops = {
	.fo_fini          = tw_fom_fini,
	.fo_tick          = tw_fom_tick,
	.fo_home_locality = tw_fom_home_locality
};

const struct m0_fom_type_ops tw_fom_type_ops = {
	.fto_create = NULL
};

/*************************************************/
/*                  UT service                   */
/*************************************************/

static int  twsvc_start(struct m0_reqh_service *svc);
static void twsvc_stop (struct m0_reqh_service *svc);
static void twsvc_fini (struct m0_reqh_service *svc);

static const struct m0_reqh_service_ops twsvc_ops = {
	.rso_start_async = &m0_reqh_service_async_start_simple,
	.rso_start       = &twsvc_start,
	.rso_stop        = &twsvc_stop,
	.rso_fini        = &twsvc_fini
};

static int twsvc_start(struct m0_reqh_service *svc)
{
	return 0;
}

static void twsvc_stop(struct m0_reqh_service *svc)
{
}

static void twsvc_fini(struct m0_reqh_service *svc)
{
	m0_free(svc);
}

static int twsvc_type_allocate(struct m0_reqh_service            **svc,
			       const struct m0_reqh_service_type  *stype)
{
	M0_ALLOC_PTR(*svc);
	M0_UT_ASSERT(*svc != NULL);
	(*svc)->rs_type = stype;
	(*svc)->rs_ops = &twsvc_ops;
	return 0;
}

static const struct m0_reqh_service_type_ops twsvc_type_ops = {
	.rsto_service_allocate = &twsvc_type_allocate
};

static struct m0_reqh_service_type ut_tw_service_type = {
	.rst_name     = "tw_ut",
	.rst_ops      = &twsvc_type_ops,
	.rst_level    = M0_RS_LEVEL_NORMAL,
	.rst_typecode = M0_CST_DS1
};

/*************************************************/
/*                 FOM routines                  */
/*************************************************/

static size_t tw_fom_home_locality(const struct m0_fom *fom)
{
	return 1;
}

static void wakeup(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
        struct tw_fom *fom = M0_AMB(fom, ast, tw_wakeup);
        m0_fom_wakeup(&fom->tw_fom);
}

static void fom_reschedule(struct tw_fom *fom)
{
        struct m0_sm_group *grp;

        grp = &fom->tw_fom.fo_loc->fl_group;
        fom->tw_wakeup.sa_cb = wakeup;
        m0_sm_ast_post(grp, &fom->tw_wakeup);
}

static int tw_fom_tick(struct m0_fom *fom0)
{
	struct tw_fom *fom    = M0_AMB(fom, fom0, tw_fom);
	int            phase  = m0_fom_phase(fom0);
	int            result = M0_FSO_AGAIN;
	enum tw_test   test   = fom->tw_test;

	switch (phase) {
	case INIT:
		m0_semaphore_up(&fom->tw_sem_init);
		switch(test) {
		case TW_TEST_DELAYED_SUCCESS:
		case TW_TEST_DELAYED_UNREACHABLE:
			if (!m0_chan_has_waiters(&fom0->fo_sm_phase.sm_chan)) {
				/*
				 * Wait until fom waiter used internally in
				 * m0_fom_timedwait() adds clink to the channel.
				 */
				fom_reschedule(fom);
				result = M0_FSO_WAIT;
			} else {
				m0_fom_phase_set(fom0,
					test == TW_TEST_DELAYED_SUCCESS ?
						PHASE1 : PHASE2);
				result = M0_FSO_AGAIN;
			}
			break;
		case TW_TEST_DELAYED_TIMEOUT:
		case TW_TEST_IMMED_TIMEOUT:
		case TW_TEST_IMMED_UNREACHABLE:
			m0_fom_phase_set(fom0, PHASE1);
			result = M0_FSO_AGAIN;
			break;
		case TW_TEST_IMMED_SUCCESS:
			m0_fom_phase_set(fom0, PHASE1);
			/* Main thread should wake up FOM. */
			result = M0_FSO_WAIT;
			break;
		default:
			M0_IMPOSSIBLE("Unknown test!");
		}
		break;
	case PHASE1:
		if (M0_IN(test, (TW_TEST_DELAYED_TIMEOUT,
				 TW_TEST_IMMED_TIMEOUT)) &&
		    !m0_semaphore_trydown(&fom->tw_sem_timeout)) {
			fom_reschedule(fom);
			result = M0_FSO_WAIT;
		} else {
			m0_fom_phase_set(fom0, PHASE2);
			result = M0_FSO_AGAIN;
		}
		break;
	case PHASE2:
		m0_fom_phase_set(fom0, FINISH);
		result = M0_FSO_WAIT;
		break;
	default:
		M0_IMPOSSIBLE("Invalid phase");
	}
	return result;
}

static void tw_fom_create(struct m0_fom **out, struct m0_reqh *reqh,
			  enum tw_test test)
{
	struct m0_fom *fom0;
	struct tw_fom *fom_obj;

	M0_PRE(out != NULL);

	M0_ALLOC_PTR(fom_obj);
	M0_UT_ASSERT(fom_obj != NULL);

	fom_obj->tw_test = test;
	m0_semaphore_init(&fom_obj->tw_sem_init, 0);
	m0_semaphore_init(&fom_obj->tw_sem_fini, 0);
	m0_semaphore_init(&fom_obj->tw_sem_timeout, 0);
	fom0 = &fom_obj->tw_fom;
	m0_fom_init(fom0, &tw_fomt, &tw_fom_ops, NULL, NULL, reqh);
	*out = fom0;
}

static void tw_fom_destroy(struct tw_fom *fom)
{

	m0_semaphore_fini(&fom->tw_sem_init);
	m0_semaphore_fini(&fom->tw_sem_fini);
	m0_semaphore_fini(&fom->tw_sem_timeout);
	m0_free(fom);
}

static void tw_fom_fini(struct m0_fom *fom)
{
	struct tw_fom *tw_fom = M0_AMB(tw_fom, fom, tw_fom);

	m0_fom_fini(fom);
	m0_semaphore_up(&tw_fom->tw_sem_fini);
	/* Memory will be deallocated in test itself, see tw_fom_destroy(). */
}

static void tw_fom_start(struct m0_fom *fom)
{
	struct tw_fom *tw_fom = M0_AMB(tw_fom, fom, tw_fom);

	m0_fom_queue(fom);
	/* Wait until the fom is really executed. */
	m0_semaphore_down(&tw_fom->tw_sem_init);
}

static void tw_fom_timeout_signal(struct m0_fom *fom)
{
	struct tw_fom *tw_fom = M0_AMB(tw_fom, fom, tw_fom);

	m0_semaphore_up(&tw_fom->tw_sem_timeout);
}

/*************************************************/
/*                 REQH routines                 */
/*************************************************/

static void reqh_init(void)
{
	int rc;

	rc = M0_REQH_INIT(&twreqh,
			  .rhia_dtm     = (void *)1,
			  .rhia_mdstore = (void *)1,
			  .rhia_fid     = &g_process_fid);
	M0_UT_ASSERT(rc == 0);
}

static void reqh_start(void)
{
	int rc;

	rc = m0_reqh_service_allocate(&twsvc, &ut_tw_service_type, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_service_init(twsvc, &twreqh, NULL);
	m0_reqh_service_start(twsvc);
	m0_reqh_start(&twreqh);
}

static void reqh_stop(void)
{
	m0_reqh_service_prepare_to_stop(twsvc);
	m0_reqh_idle_wait_for(&twreqh, twsvc);
	m0_reqh_service_stop(twsvc);
	m0_reqh_service_fini(twsvc);
}

static void reqh_fini(void)
{
	m0_reqh_services_terminate(&twreqh);
	m0_reqh_fini(&twreqh);
}

/*************************************************/
/*                   UT init/fini                */
/*************************************************/

static struct m0_fom *tw_init(enum tw_test test)
{
	struct m0_fom *tw_fom;

	reqh_init();
	reqh_start();
	tw_fom_create(&tw_fom, &twreqh, test);
	tw_fom_start(tw_fom);
	return tw_fom;
}

static void tw_fini(struct m0_fom *fom0)
{
	struct tw_fom *fom = M0_AMB(fom, fom0, tw_fom);

	reqh_stop();
	/* Assure that FOM is finalised. */
	m0_semaphore_down(&fom->tw_sem_fini);
	reqh_fini();
	tw_fom_destroy(fom);
}

/*************************************************/
/*                    Test cases                 */
/*************************************************/

static void immediate_success(void)
{
	struct m0_fom *tw_fom;
	int            rc;

	tw_fom = tw_init(TW_TEST_IMMED_SUCCESS);
	rc = m0_fom_timedwait(tw_fom, M0_BITS(PHASE1), M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	/* FOM should wait after moving to PHASE1, wakeup it. */
	m0_fom_wakeup(tw_fom);
	tw_fini(tw_fom);
}

static void delayed_success(void)
{
	struct m0_fom *tw_fom;
	int            rc;

	tw_fom = tw_init(TW_TEST_DELAYED_SUCCESS);
	rc = m0_fom_timedwait(tw_fom, M0_BITS(PHASE1), M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	tw_fini(tw_fom);
}

static void immediate_timeout(void)
{
	struct m0_fom *tw_fom;
	int            rc;

	tw_fom = tw_init(TW_TEST_IMMED_TIMEOUT);
	rc = m0_fom_timedwait(tw_fom, M0_BITS(PHASE2), m0_time_now());
	M0_UT_ASSERT(rc == -ETIMEDOUT);
	tw_fom_timeout_signal(tw_fom);
	tw_fini(tw_fom);
}

static void delayed_timeout(void)
{
	struct m0_fom *tw_fom;
	int            rc;

	tw_fom = tw_init(TW_TEST_DELAYED_TIMEOUT);
	rc = m0_fom_timedwait(tw_fom, M0_BITS(PHASE2),
			      m0_time_now() + 100 * M0_TIME_ONE_MSEC);
	M0_UT_ASSERT(rc == -ETIMEDOUT);
	tw_fom_timeout_signal(tw_fom);
	tw_fini(tw_fom);
}

static void immediate_unreachable(void)
{
	struct m0_fom *tw_fom;
	int            rc;

	tw_fom = tw_init(TW_TEST_IMMED_UNREACHABLE);
	rc = m0_fom_timedwait(tw_fom, M0_BITS(PHASE1), M0_TIME_NEVER);
	M0_UT_ASSERT(rc == -ESRCH);
	tw_fini(tw_fom);
}

static void delayed_unreachable(void)
{
	struct m0_fom *tw_fom;
	int            rc;

	tw_fom = tw_init(TW_TEST_DELAYED_UNREACHABLE);
	rc = m0_fom_timedwait(tw_fom, M0_BITS(PHASE1), M0_TIME_NEVER);
	M0_UT_ASSERT(rc == -ESRCH);
	tw_fini(tw_fom);
}

int tw_test_suite_init(void)
{
	m0_fom_type_init(&tw_fomt, M0_UT_TIMEDWAIT_FOM_OPCODE,
			 &tw_fom_type_ops, &ut_tw_service_type,
			 &tw_sm_conf);
	return 0;
}

struct m0_ut_suite fom_timedwait_ut = {
	.ts_name = "fom-timedwait-ut",
	.ts_init = tw_test_suite_init,
	.ts_fini = NULL,
	.ts_tests = {
		{ "immediate-success",     immediate_success     },
		{ "delayed-success",       delayed_success       },
		{ "immediate-timeout",     immediate_timeout     },
		{ "delayed-timeout",       delayed_timeout       },
		{ "immediate-unreachable", immediate_unreachable },
		{ "delayed-unreachable",   delayed_unreachable   },
		{ NULL, NULL }
	}
};

M0_EXPORTED(fom_timedwait_ut);

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
