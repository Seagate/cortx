/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original author:  Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 20-Oct-2014
 */

/**
 * many are borrowed from ut/ut{.c,.h}
 */
#ifndef __KERNEL__
#include <errno.h>
#include <sys/types.h>
#endif

#include "clovis/st/clovis_st.h"
#include "clovis/st/clovis_st_misc.h"
#include "clovis/st/clovis_st_assert.h"

static struct clovis_st_ctx ctx = {
	.sx_cfg = {
		.sc_nr_threads = 1,
		.sc_run_selected = 0,
		.sc_nr_rounds  = 1,
		.sc_mode       = CLOVIS_ST_SEQ_MODE,
		.sc_pace       = 0,
	},
	.sx_nr_all   = 0,
	.sx_nr_selected = 0,
};

struct m0* clovis_st_get_mero(void)
{
	return ctx.sx_instance.si_instance->m0c_mero;
}

void clovis_st_set_instance(struct m0_clovis *instance)
{
	ctx.sx_instance.si_instance = instance;
}

struct m0_clovis* clovis_st_get_instance()
{
	return ctx.sx_instance.si_instance;
}

struct clovis_st_cfg clovis_st_get_cfg()
{
	return ctx.sx_cfg;
}

struct clovis_st_worker_stat* clovis_st_get_worker_stat(int idx)
{
	if (idx < 0 || idx >= ctx.sx_cfg.sc_nr_threads)
		return NULL;

	return ctx.sx_worker_stats + idx;
}

void clovis_st_set_tests(const char *tests)
{
	ctx.sx_cfg.sc_tests = tests;
}

const char* clovis_st_get_tests()
{
	return ctx.sx_cfg.sc_tests;
}

void clovis_st_set_test_mode(enum clovis_st_mode mode)
{
	ctx.sx_cfg.sc_mode = mode;
}

enum clovis_st_mode clovis_st_get_test_mode(void)
{
	return ctx.sx_cfg.sc_mode;
}

void clovis_st_set_nr_workers(int nr)
{
	if (nr < CLOVIS_ST_MAX_WORKER_NUM)
		ctx.sx_cfg.sc_nr_threads = nr;
	else
		ctx.sx_cfg.sc_nr_threads = CLOVIS_ST_MAX_WORKER_NUM;
}

int clovis_st_get_nr_workers(void)
{
	return ctx.sx_cfg.sc_nr_threads;
}

int clovis_st_set_worker_tid(int idx, pid_t tid)
{
	if (ctx.sx_worker_tids == NULL
	    || idx >= ctx.sx_cfg.sc_nr_threads)
		return -EINVAL;

	ctx.sx_worker_tids[idx] = tid;

	return 0;
}

int clovis_st_get_worker_idx(pid_t tid)
{
	int i;

	if (ctx.sx_worker_tids == NULL)
		return -ENOMEM;

	for (i = 0; i < ctx.sx_cfg.sc_nr_threads; i++) {
		if (ctx.sx_worker_tids[i] == tid)
			break;
	}

	/* not found */
	if (i == ctx.sx_cfg.sc_nr_threads)
		return -EINVAL;

	return i;
}

static struct clovis_st_suite *lookup_suite(const char *name)
{
	int i;

	if (name == NULL)
		return NULL;

	for (i = 0; i < ctx.sx_nr_all; ++i)
		if (str_eq(ctx.sx_all[i]->ss_name, name))
			return ctx.sx_all[i];
	return NULL;
}

static struct clovis_st_test *lookup_test(struct clovis_st_suite *s,
					  const char *t_name)
{
	struct clovis_st_test  *t;

	if (s == NULL || t_name == NULL)
		return NULL;

	for (t = s->ss_tests; t->st_name != NULL; t++)
		if (str_eq(t->st_name, t_name))
			return t;

	return NULL;
}

static int select_tests(const char *str)
{
	char *s;
	char *p;
	char *token;
	char *sub_token;
	struct clovis_st_suite *suite;
	struct clovis_st_test  *test;

	s = str_dup(str);
	if (s == NULL)
		return -ENOMEM;
	p = s;

	while (true) {
		token = strsep(&p, ",");
		if (token == NULL)
			break;

		sub_token = strchr(token, ':');
		if (sub_token != NULL)
			*sub_token++ = '\0';

		suite = lookup_suite(token);
		if (suite == NULL) continue;

		if (sub_token != NULL) {
			test = lookup_test(suite, sub_token);
			if (test == NULL) continue;

			test->st_enabled = 1;
		} else {
			/* All tests in this suite will be marked*/
			test = suite->ss_tests;
			while (test->st_name != NULL) {
				test->st_enabled = 1;
				test++;
			}
		}

		ctx.sx_selected[ctx.sx_nr_selected++] = suite;
	}

	mem_free(s);
	return 0;
}

static int shuffle_tests(struct clovis_st_suite *suite)
{
	int                   i;
	int                   j;
	int                   k;
	int                   index;
	int                   nr_tests;
	int                  *order;
	struct clovis_st_test t1;
	struct clovis_st_test t2;

	/* Count the tests */
	nr_tests = 0;
	while (suite->ss_tests[nr_tests].st_name != NULL)
		nr_tests++;

	MEM_ALLOC_ARR(order, nr_tests);
	if (order == NULL)
		return -ENOMEM;

	/* Produce a new order */
	for (i = 0; i < nr_tests; ) {
		index = generate_random(nr_tests);
		for (j = 0; j < i; j++)
                	if (order[j] == index) break;

		if (i == j) {
			order[i] = index;
			i++;
		}
		/* will be a indefinite loop? In theory, no!*/
	}

	/*
	 * Re-write the test order. As the number of tests in a
	 * suite is small (< 100), this shouldn't be a big issue
	 * for performance.
	 */
	k = 0;
	t1 = suite->ss_tests[0];
	for (i = 0; i < nr_tests; i++){

		/* look for the test to be replaced*/
		for (j = 0; j < nr_tests; j++)
			if (order[j] == k) break;

		t2 = suite->ss_tests[j];
		suite->ss_tests[j] = t1;
		t1 = t2;
		k = j;
	}
	mem_free(order);

	return 0;
}

static inline const char *skipspaces(const char *str)
{
	while (isspace(*str))
		++str;
	return str;
}

static const char padding[256] = { [0 ... 254] = ' ', [255] = '\0' };

static void run_test(const struct clovis_st_test *test)
{
	size_t   len;
	size_t   pad_len;
	size_t   name_len;
	size_t   max_name_len;
	uint64_t start;
	uint64_t end;
	uint64_t duration;

	console_printf(LOG_PREFIX "  %s  ", test->st_name);

	start = time_now();

	/*
	 * Turn on the cleaner so we have a chance to cleanup
	 * mess when an assert fails
	 */
	clovis_st_cleaner_enable();

	/* run the test */
	test->st_proc();

	/* Turn off cleaner now */
	clovis_st_cleaner_disable();

	end = time_now();
	duration = end - start;

	/* generate report */
	name_len = strlen(test->st_name);
	max_name_len = ctx.sx_max_name_len;

	len = (name_len > max_name_len)?name_len:max_name_len;
	pad_len = len - name_len;
	pad_len = (pad_len < ARRAY_SIZE(padding) - 1)?
		   pad_len:(ARRAY_SIZE(padding) - 1);

	console_printf("%.*s%4" PRIu64 ".%-2" PRIu64 " sec\n",
		       (int)pad_len, padding, time_seconds(duration),
		       time_nanoseconds(duration) / TIME_ONE_MSEC / 10);
}

static int run_suite(struct clovis_st_suite *suite)
{
	int			     rc = 0;
	uint64_t		     start;
	uint64_t		     end;
	uint64_t		     duration;
	const struct clovis_st_test *test;

	/* Change test order if random mode is enabled */
	if (clovis_st_get_test_mode() == CLOVIS_ST_RAND_MODE) {
		rc = shuffle_tests(suite);
		if (rc < 0)
			return rc;
	}

	/* Real test starts here*/
	console_printf("\n%s\n", suite->ss_name);
	start = time_now();

	/* suite initialisation */
	if (suite->ss_init != NULL) {
		rc = suite->ss_init();
		if (rc != 0) {
			console_printf("Clovis ST suite initialization failure.\n");
			return rc;
		}
	}
	/* run test and speed control */
	for (test = suite->ss_tests; test->st_name != NULL; test++){
		if (ctx.sx_cfg.sc_run_selected == 1
		    && test->st_enabled == 0)
			continue;

		run_test(test);

		/* clean the mess left by this test*/
		clovis_st_cleaner_empty_bin();
	}

	/* suite finalisation */
	if (suite->ss_fini != NULL) {
		rc = suite->ss_fini();
		if (rc != 0)
			console_printf("Clovis ST suite finalization failure.\n");
	}

	/* generate report */
	end = time_now();
	duration = end - start;

	console_printf(LOG_PREFIX "  [ time: %" PRIu64 ".%-" PRIu64 " sec]\n",
		       time_seconds(duration),
		       time_nanoseconds(duration) / TIME_ONE_MSEC / 10);
	return rc;
}

static int run_suites(struct clovis_st_suite **suites, int nr_suites)
{
	int i;
	int rc;

	rc = 0;
	for (i = 0; i <  nr_suites && rc == 0; i++)
		rc = run_suite(suites[i]);

	return rc;
}

int clovis_st_run(const char *test_list_str)
{
	int      rc;
	int	 worker_idx;
	uint64_t start;
	uint64_t end;
	uint64_t duration;
	uint64_t nr_asserts = 0;
	uint64_t nr_failed_asserts = 0;
	uint64_t csec;

	worker_idx  = clovis_st_get_worker_idx(get_tid());
	if (worker_idx < 0)
		return worker_idx;

	/* Runs all test suites now*/
	start = time_now();
	ctx.sx_worker_stats[worker_idx].sws_nr_asserts = 0;
	ctx.sx_worker_stats[worker_idx].sws_nr_failed_asserts = 0;

	if (test_list_str == NULL)
		rc = run_suites(ctx.sx_all, ctx.sx_nr_all);
	else {
		ctx.sx_cfg.sc_run_selected = true;
		select_tests(test_list_str);
		rc = run_suites(ctx.sx_selected, ctx.sx_nr_selected);
	}

	/* Generates report */
	end = time_now();
	duration = end - start;
	csec = time_nanoseconds(duration) / TIME_ONE_MSEC / 10;
	nr_asserts = ctx.sx_worker_stats[worker_idx].sws_nr_asserts;
	nr_failed_asserts =
		ctx.sx_worker_stats[worker_idx].sws_nr_failed_asserts;

	if (rc == 0)
		console_printf("\nTime: %" PRIu64 ".%-2" PRIu64 " sec,"
			       " Asserts: %" PRIu64 ", "
			       " Failed Asserts: %" PRIu64
			       "\nClovis tests Done\n",
			       time_seconds(duration), csec,
			       nr_asserts, nr_failed_asserts);
	return nr_failed_asserts;
}

void clovis_st_list(bool with_tests)
{
	int			     i;
	const struct clovis_st_test *t;

	for (i = 0; i < ctx.sx_nr_all; ++i) {
		console_printf("%s\n", ctx.sx_all[i]->ss_name);
		if (with_tests)
			for (t = ctx.sx_all[i]->ss_tests; t->st_name != NULL; t++)
				console_printf("  %s\n", t->st_name);
	}
}

int clovis_st_init(void)
{
	/* Object ID allocator */
	clovis_oid_allocator_init();

	/* allocate memory for test suites */
	MEM_ALLOC_ARR(ctx.sx_all, CLOVIS_ST_MAX_SUITE_NUM);
	if (ctx.sx_all == NULL)
		goto err_exit;

	MEM_ALLOC_ARR(ctx.sx_selected, CLOVIS_ST_MAX_SUITE_NUM);
	if (ctx.sx_selected == NULL)
		goto err_exit;

	MEM_ALLOC_ARR(ctx.sx_worker_tids, CLOVIS_ST_MAX_WORKER_NUM);
	if (ctx.sx_worker_tids == NULL)
		goto err_exit;

	MEM_ALLOC_ARR(ctx.sx_worker_stats, CLOVIS_ST_MAX_WORKER_NUM);
	if (ctx.sx_worker_stats == NULL)
		goto err_exit;

	return 0;

err_exit:
	clovis_oid_allocator_fini();

	if (ctx.sx_all != NULL)
		mem_free(ctx.sx_all);

	if (ctx.sx_selected != NULL)
		mem_free(ctx.sx_selected);

	if (ctx.sx_worker_tids != NULL)
		mem_free(ctx.sx_worker_tids);

	if (ctx.sx_worker_stats != NULL)
		mem_free(ctx.sx_worker_stats);

	return -ENOMEM;
}

void clovis_st_fini(void)
{
	clovis_oid_allocator_fini();

	/* free memory */
	mem_free(ctx.sx_all);
	mem_free(ctx.sx_selected);

	return;
}

int clovis_st_add(struct clovis_st_suite *suite)
{
	int                     str_len;
	size_t                  max_len;
	struct clovis_st_test  *t;

	if (ctx.sx_nr_all >= CLOVIS_ST_MAX_SUITE_NUM)
		return -EINVAL;

	/* check if tests in this suite have been correctly set*/
	max_len = ctx.sx_max_name_len;
	for (t = suite->ss_tests; t->st_name != NULL; t++){
		if (t->st_proc == NULL)
			return -EINVAL;

		str_len = strlen(t->st_name);
		max_len = (str_len > max_len)?str_len:max_len;
	}

	/* update ctx details*/
	ctx.sx_all[ctx.sx_nr_all++] = suite;
	ctx.sx_max_name_len = max_len;

	return 0;
}

/**
 * This is the place to add test suites
 */
extern struct clovis_st_suite st_suite_clovis_idx;
extern struct clovis_st_suite st_suite_clovis_obj;
extern struct clovis_st_suite st_suite_clovis_read;
extern struct clovis_st_suite st_suite_clovis_write;
extern struct clovis_st_suite st_suite_clovis_osync;
extern struct clovis_st_suite st_suite_clovis_isync;
extern struct clovis_st_suite st_suite_clovis_layout;
extern struct clovis_st_suite st_suite_clovis_example;
extern struct clovis_st_suite st_suite_clovis_mt;

void clovis_st_add_suites()
{
	clovis_st_add(&st_suite_clovis_example);
	clovis_st_add(&st_suite_clovis_obj);
	clovis_st_add(&st_suite_clovis_read);
	clovis_st_add(&st_suite_clovis_write);
	clovis_st_add(&st_suite_clovis_layout);
	clovis_st_add(&st_suite_clovis_osync);
	clovis_st_add(&st_suite_clovis_idx);
#ifndef __KERNEL__
	clovis_st_add(&st_suite_clovis_isync);
	clovis_st_add(&st_suite_clovis_mt);
#endif
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
