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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 19-Jul-2010
 */

#include "lib/string.h"         /* m0_strdup */
#include "lib/memory.h"
#include "lib/thread.h"         /* LAMBDA */
#include "lib/getopts.h"
#include "lib/ub.h"
#include "ut/ut.h"              /* m0_ut_init */
#include "ut/module.h"          /* m0_ut_module */
#include "module/instance.h"    /* m0 */

extern struct m0_ub_set m0_ad_ub;
extern struct m0_ub_set m0_adieu_ub;
extern struct m0_ub_set m0_atomic_ub;
extern struct m0_ub_set m0_bitmap_ub;
extern struct m0_ub_set m0_fol_ub;
extern struct m0_ub_set m0_fom_ub;
extern struct m0_ub_set m0_list_ub;
extern struct m0_ub_set m0_memory_ub;
extern struct m0_ub_set m0_parity_math_ub;
//extern struct m0_ub_set m0_rpc_ub;
extern struct m0_ub_set m0_thread_ub;
extern struct m0_ub_set m0_time_ub;
extern struct m0_ub_set m0_timer_ub;
extern struct m0_ub_set m0_tlist_ub;
extern struct m0_ub_set m0_trace_ub;
extern struct m0_ub_set m0_varr_ub;

#define UB_SANDBOX "./ub-sandbox"

struct ub_args {
	uint32_t ua_rounds;
	char    *ua_name;
	char    *ua_opts;
	bool     ua_ub_list;
};

static void ub_args_fini(struct ub_args *args)
{
	m0_free(args->ua_opts);
	m0_free(args->ua_name);
}

static int ub_args_parse(int argc, char *argv[], struct ub_args *out)
{
	out->ua_rounds = 1;
	out->ua_name = NULL;
	out->ua_opts = NULL;
	out->ua_ub_list = false;

	return M0_GETOPTS("ub", argc, argv,
		  M0_HELPARG('h'),
		  M0_VOIDARG('l', "List available benchmarks and exit",
			     LAMBDA(void, (void) {
					     out->ua_ub_list = true;
				     })),
		  M0_NUMBERARG('r', "Number of rounds a benchmark has to run",
			       LAMBDA(void, (int64_t rounds) {
					       out->ua_rounds = rounds;
				       })),
		  M0_STRINGARG('t', "Benchmark to run",
			       LAMBDA(void, (const char *str) {
					       out->ua_name = m0_strdup(str);
				       })),
		  M0_STRINGARG('o', "Optional parameters (ignored by most"
			       " benchmarks)",
			       LAMBDA(void, (const char *str) {
					       out->ua_opts = m0_strdup(str);
				       }))
		);
}

static void ub_add(const struct ub_args *args)
{
	/*
	 * Please maintain _reversed_ sorting order.
	 *
	 * These benchmarks are executed in reverse order from the way
	 * they are listed here.
	 */
	m0_ub_set_add(&m0_varr_ub);
	m0_ub_set_add(&m0_trace_ub);
	m0_ub_set_add(&m0_tlist_ub);
	m0_ub_set_add(&m0_timer_ub);
	m0_ub_set_add(&m0_time_ub);
	m0_ub_set_add(&m0_thread_ub);
//	m0_ub_set_add(&m0_rpc_ub);
//XXX_BE_DB	m0_ub_set_add(&m0_parity_math_ub);
	m0_ub_set_add(&m0_memory_ub);
	m0_ub_set_add(&m0_list_ub);
	m0_ub_set_add(&m0_fom_ub);
	m0_ub_set_add(&m0_fol_ub);
//XXX_BE_DB 	m0_ub_set_add(&m0_bitmap_ub);
//XXX_BE_DB 	m0_ub_set_add(&m0_atomic_ub);
	m0_ub_set_add(&m0_adieu_ub);
	m0_ub_set_add(&m0_ad_ub);
}

static int ub_run(const struct ub_args *args)
{
	int rc = 0;

	M0_PRE(!args->ua_ub_list);

	if (args->ua_name != NULL)
		rc = m0_ub_set_select(args->ua_name);

	return rc ?: m0_ub_run(args->ua_rounds, args->ua_opts);
}

int main(int argc, char *argv[])
{
	static struct m0 instance;
	struct ub_args args;
	int            rc;

	m0_instance_setup(&instance);
	(void)m0_ut_module_type.mt_create(&instance);

	((struct m0_ut_module *)instance.i_moddata[M0_MODULE_UT])->ut_sandbox =
		UB_SANDBOX;

	rc = m0_ut_init(&instance);
	if (rc != 0)
		return rc;

	rc = ub_args_parse(argc, argv, &args);
	if (rc == 0) {
		ub_add(&args);

		if (args.ua_ub_list)
			m0_ub_set_print();
		else
			rc = ub_run(&args);
	}
	ub_args_fini(&args);
	m0_ut_fini();

	return rc;
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
