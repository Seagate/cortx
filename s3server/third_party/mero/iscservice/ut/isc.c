#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"
#include "lib/memory.h"
#include "ut/ut.h"
#include "ut/misc.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "rpc/rpc_machine.h"
#include "iscservice/isc.h"
#include "iscservice/isc_service.h"
#include "iscservice/ut/common.h"

static struct m0_reqh reqh;
static struct m0_rpc_machine rpc_machine;
static struct m0_reqh_service *isc;

static void init(void)
{
	int result;

	result = M0_REQH_INIT(&reqh,
			       .rhia_db = NULL,
			       .rhia_mdstore = (void *)1,
			       .rhia_fid = &g_process_fid);
	M0_UT_ASSERT(result == 0);
	m0_reqh_rpc_mach_tlink_init_at_tail(&rpc_machine,
					    &reqh.rh_rpc_machines);
	result = m0_reqh_service_allocate(&isc, &m0_iscs_type, NULL);
	M0_UT_ASSERT(result == 0);
	m0_reqh_service_init(isc, &reqh, NULL);
	m0_reqh_service_start(isc);
	m0_reqh_start(&reqh);
}

static void fini(void)
{
	m0_reqh_rpc_mach_tlist_pop(&reqh.rh_rpc_machines);
	m0_reqh_service_prepare_to_stop(isc);
	m0_reqh_idle_wait_for(&reqh, isc);
	m0_reqh_service_stop(isc);
	m0_reqh_service_fini(isc);
}

static int null_computation(struct m0_buf *in, struct m0_buf *out,
			    struct m0_isc_comp_private *comp_data, int *rc)
{
	rc = 0;
	return M0_FSO_AGAIN;
}

void comp_register(void *args)
{
	struct thr_args *thr_args = args;
	struct m0_fid   *fid = thr_args->ta_data;

	/* Wait till the last thread reaches here. */
	m0_semaphore_up(thr_args->ta_barrier);
	thr_args->ta_rc = m0_isc_comp_register(null_computation,
					       "null_computation", fid);
}

static void fid_set(void *fid, int tid)
{
	m0_fid_set((struct m0_fid *)fid, 0x1234, tid);
}

static void test_register(void)
{
	int                    rc;
	struct m0_fid          fid;
	struct cnc_cntrl_block cc_block;

	M0_SET0(&cc_block);
	init();
	m0_fid_set(&fid, 0x1234, 0x456);
	/* Register a new computation. */
	rc = m0_isc_comp_register(null_computation, "null_computation", &fid);
	M0_UT_ASSERT(rc == 0);
	/* Register an existing computation. */
	rc = m0_isc_comp_register(null_computation, "null_computation", &fid);
	M0_UT_ASSERT(rc == -EEXIST);
	rc = m0_isc_comp_state_probe(&fid);
	M0_UT_ASSERT(rc == M0_ICS_REGISTERED);
	/* Unregister. */
	m0_isc_comp_unregister(&fid);
	rc = m0_isc_comp_state_probe(&fid);
	M0_UT_ASSERT(rc == -ENOENT);
	cc_block_init(&cc_block, sizeof (struct m0_fid), fid_set);
	cc_block_launch(&cc_block, comp_register);
	fini();
}

static void test_init_fini(void)
{
	init();
	fini();
}

struct m0_ut_suite isc_api_ut = {
	.ts_name  = "isc-api-ut",
	.ts_init  = NULL,
	.ts_fini  = NULL,
	.ts_tests = {
		{"init", test_init_fini, "Nachiket"},
		{"comp-register", test_register, "Nachiket"},
		{NULL, NULL}
	}
};
