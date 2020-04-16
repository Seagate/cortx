/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 01-Aug-2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ut/ut.h"
#include "ut/ut_internal.h"
#include "ut/module.h"            /* m0_ut_module */
#include "module/instance.h"      /* m0 */
#include "lib/errno.h"            /* ENOENT */
#include "lib/finject.h"          /* m0_fi_init */
#include "lib/finject_internal.h" /* m0_fi_states_get */
#include "lib/string.h"           /* m0_streq */
#include "lib/memory.h"           /* M0_ALLOC_PTR */

/**
 * @addtogroup ut
 * @{
 */

/*
 * syslog(8) will trim leading spaces of each kernel log line, so we need to use
 * a non-space character at the beginning of each line to preserve formatting
 */
#ifdef __KERNEL__
#define LOG_PREFIX "."
#else
#define LOG_PREFIX
#endif

static int test_suites_enable(const struct m0_ut_module *m);

struct ut_entry {
	struct m0_list_link ue_linkage;
	const char         *ue_suite_name;
	const char         *ue_test_name;
};

static struct m0_ut_module *ut_module(void)
{
	return m0_get()->i_moddata[M0_MODULE_UT];
}

M0_INTERNAL int m0_ut_init(struct m0 *instance)
{
	/*
	 * We cannot use ut_module() here: m0_get() is not available,
	 * because m0 instance have not reached M0_LEVEL_INST_ONCE yet.
	 */
	struct m0_ut_module *m = instance->i_moddata[M0_MODULE_UT];
	struct m0_ut_suite  *ts;
	int                  i;
	int                  rc;

	rc = test_suites_enable(m);
	if (rc != 0)
		return rc;
	/*
	 * Ensure that the loop below is able to create the required
	 * number of dependencies.
	 */
	M0_ASSERT(ARRAY_SIZE(m->ut_module.m_dep) >= m->ut_suites_nr);
	for (i = 0; i < m->ut_suites_nr; ++i) {
		ts = m->ut_suites[i];
		if (!ts->ts_enabled)
			continue;
		m0_ut_suite_module_setup(ts, instance);
		m0_module_dep_add(&m->ut_module, M0_LEVEL_UT_READY,
				  &ts->ts_module, M0_LEVEL_UT_SUITE_READY);
	}
	return m0_module_init(&instance->i_self, M0_LEVEL_INST_READY);
}
M0_EXPORTED(m0_ut_init);

M0_INTERNAL void m0_ut_fini(void)
{
	m0_module_fini(&m0_get()->i_self, M0_MODLEV_NONE);
}
M0_EXPORTED(m0_ut_fini);

M0_INTERNAL void m0_ut_add(struct m0_ut_module *m, struct m0_ut_suite *ts,
			   bool enable)
{
	M0_PRE(IS_IN_ARRAY(m->ut_suites_nr, m->ut_suites));
	m->ut_suites[m->ut_suites_nr++] = ts;
	ts->ts_masked = !enable;
}

static struct m0_ut_suite *
suite_find(const struct m0_ut_module *m, const char *name)
{
	int i;

	if (name == NULL)
		return NULL;

	for (i = 0; i < m->ut_suites_nr; ++i)
		if (m0_streq(m->ut_suites[i]->ts_name, name))
			return m->ut_suites[i];
	return NULL;
}

static struct m0_ut *get_test_by_name(const struct m0_ut_module *m,
				      const char *s_name, const char *t_name)
{
	struct m0_ut_suite *s;
	struct m0_ut       *t;

	if (t_name == NULL)
		return NULL;

	s = suite_find(m, s_name);
	if (s != NULL)
		for (t = s->ts_tests; t->t_name != NULL; ++t)
			if (m0_streq(t->t_name, t_name))
				return t;
	return NULL;
}

static void set_enabled_flag_for(const struct m0_ut_module *m,
				 const char *s_name, const char *t_name,
				 bool value)
{
	struct m0_ut_suite *s;
	struct m0_ut       *t;

	M0_PRE(s_name != NULL);

	s = suite_find(m, s_name);
	M0_ASSERT(s != NULL); /* ensured by test_list_populate() */
	s->ts_enabled = value;

	if (t_name == NULL) {
		for (t = s->ts_tests; t->t_name != NULL; ++t)
			t->t_enabled = value;
	} else {
		/*
		 * re-enable test suite if value is 'false', because in this
		 * case we disable particular tests, not a whole suite
		 */
		s->ts_enabled = s->ts_enabled || !value;
		t = get_test_by_name(m, s_name, t_name);
		M0_ASSERT(t != NULL); /* ensured by test_list_populate() */
		t->t_enabled = value;
	}
}

static bool
exists(const struct m0_ut_module *m, const char *s_name, const char *t_name)
{
	struct m0_ut_suite *s;
	struct m0_ut       *t;

	s = suite_find(m, s_name);
	if (s == NULL) {
		M0_LOG(M0_FATAL, "Unit-test suite '%s' not found!", s_name);
		return false;
	}

	/* if checking only suite existence */
	if (t_name == NULL)
		return true;

	/* got here? then need to check test existence */
	t = get_test_by_name(m, s_name, t_name);
	if (t == NULL) {
		M0_LOG(M0_FATAL, "Unit-test '%s:%s' not found!", s_name, t_name);
		return false;
	}
	return true;
}

static int test_add(struct m0_list *list, const char *suite, const char *test,
		    const struct m0_ut_module *m)
{
	struct ut_entry *e;
	int              rc = -ENOMEM;

	M0_PRE(suite != NULL);

	M0_ALLOC_PTR(e);
	if (e == NULL)
		return -ENOMEM;

	e->ue_suite_name = m0_strdup(suite);
	if (e->ue_suite_name == NULL)
		goto err;

	if (test != NULL) {
		e->ue_test_name = m0_strdup(test);
		if (e->ue_test_name == NULL)
			goto err;
	}

	if (exists(m, e->ue_suite_name, e->ue_test_name)) {
		m0_list_link_init(&e->ue_linkage);
		m0_list_add_tail(list, &e->ue_linkage);
		return 0;
	}
	rc = -ENOENT;
err:
	m0_free(e);
	return rc;
}

/**
 * Populates a list of ut_entry elements by parsing input string,
 * which should conform with the format 'suite[:test][,suite[:test]]'.
 *
 * @param  str   input string.
 * @param  list  initialised and empty m0_list.
 */
static int test_list_populate(struct m0_list *list, const char *str,
			      const struct m0_ut_module *m)
{
	char *s;
	char *p;
	char *token;
	char *subtoken;
	int   rc = 0;

	M0_PRE(str != NULL);

	s = m0_strdup(str);
	if (s == NULL)
		return -ENOMEM;
	p = s;

	while ((token = strsep(&p, ",")) != NULL) {
		subtoken = strchr(token, ':');
		if (subtoken != NULL)
			*subtoken++ = '\0';

		rc = test_add(list, token, subtoken, m);
		if (rc != 0)
			break;
	}
	m0_free(s);
	return rc;
}

static void test_list_destroy(struct m0_list *list)
{
	m0_list_entry_forall(e, list, struct ut_entry, ue_linkage,
			     m0_list_del(&e->ue_linkage);
			     m0_free((char *)e->ue_suite_name);
			     m0_free((char *)e->ue_test_name);
			     m0_free(e);
			     true);
	m0_list_fini(list);
}

static int test_list_create(struct m0_list *list, const struct m0_ut_module *m)
{
	int rc;

	M0_PRE(m->ut_tests != NULL && *m->ut_tests != '\0');

#ifdef __KERNEL__
	/*
	 * FI module is not initalised yet, but test_list_populate() calls
	 * m0_alloc, with uses M0_FI_ENABLED.
	 * M0_FI_ENABLED requires initialised FI.
	 */
	m0_fi_init();
#endif

	m0_list_init(list);
	rc = test_list_populate(list, m->ut_tests, m);
	if (rc != 0)
		test_list_destroy(list);

#ifdef __KERNEL__
	m0_fi_fini();
#endif
	return rc;
}

static int test_suites_enable(const struct m0_ut_module *m)
{
	struct m0_list  disable_list;
	struct m0_ut   *t;

	bool flag;
	int  i;
	int  rc = 0;

	flag = (m->ut_tests == NULL || m->ut_exclude);

	for (i = 0; i < m->ut_suites_nr; ++i) {
		m->ut_suites[i]->ts_enabled = flag;
		for (t = m->ut_suites[i]->ts_tests; t->t_name != NULL; ++t)
			t->t_enabled = flag;
	}

	if (m->ut_tests != NULL) {
		rc = test_list_create(&disable_list, m);
		if (rc != 0)
			return rc;

		m0_list_entry_forall(e, &disable_list, struct ut_entry, ue_linkage,
				     set_enabled_flag_for(m, e->ue_suite_name,
							  e->ue_test_name, !flag);
				     true);
		test_list_destroy(&disable_list);
	}

	return rc;
}

static inline const char *skipspaces(const char *str)
{
	while (isspace(*str))
		++str;
	return str;
}

/* Checks that all fault injections are disabled. */
static void check_all_fi_disabled(void)
{
#ifdef ENABLE_FAULT_INJECTION
	const struct m0_fi_fpoint_state *fi_states;
	const struct m0_fi_fpoint_state *state;
	uint32_t                         fi_free_idx;
	uint32_t                         i;

	/*
	 * Assume there is no concurrent access to the FI states at this point.
	 */
	fi_states = m0_fi_states_get();
	fi_free_idx = m0_fi_states_get_free_idx();
	for (i = 0; i < fi_free_idx; ++i) {
		state = &fi_states[i];
		M0_ASSERT_INFO(!fi_state_enabled(state),
			       "Fault injection is not disabled: %s(), \"%s\"",
			       state->fps_id.fpi_func, state->fps_id.fpi_tag);
	}
#endif /* ENABLE_FAULT_INJECTION */
}

static void run_test(const struct m0_ut *test, size_t max_name_len)
{
	static const char padding[256] = { [0 ... 254] = ' ', [255] = '\0' };

	size_t    pad_len;
	size_t    name_len;
	char      mem[16];
	uint64_t  mem_before;
	uint64_t  mem_after;
	uint64_t  mem_used;
	m0_time_t start;
	m0_time_t end;
	m0_time_t duration;

	if (!test->t_enabled)
		return;

	name_len = strlen(test->t_name);
	if (test->t_owner != NULL) {
		m0_console_printf(LOG_PREFIX "  %s [%s] ",
				  test->t_name, test->t_owner);
		name_len += strlen(test->t_owner) + 2; /* 2 for [] */
	} else
		m0_console_printf(LOG_PREFIX "  %s  ", test->t_name);
	mem_before = m0_allocated_total();
	start      = m0_time_now();

	/* run the test */
	test->t_proc();

	end       = m0_time_now();
	mem_after = m0_allocated_total();

	/* max_check is for case when max_name_len == 0 */
	pad_len  = max_check(name_len, max_name_len) - name_len;
	pad_len  = min_check(pad_len, ARRAY_SIZE(padding) - 1);
	duration = m0_time_sub(end, start);
	mem_used = mem_after - mem_before;

	m0_console_printf("%.*s%4" PRIu64 ".%02" PRIu64 " sec  %sB\n",
			  (int)pad_len, padding, m0_time_seconds(duration),
			  m0_time_nanoseconds(duration) / M0_TIME_ONE_MSEC / 10,
			  m0_bcount_with_suffix(mem, ARRAY_SIZE(mem), mem_used));
}

static int run_suite(const struct m0_ut_suite *suite, int max_name_len)
{
	const struct m0_ut *test;

	char      leak[16];
	uint64_t  alloc_before;
	uint64_t  alloc_after;
	char      mem[16];
	uint64_t  mem_before;
	uint64_t  mem_after;
	uint64_t  mem_used;
	m0_time_t start;
	m0_time_t end;
	m0_time_t duration;
	int       rc = 0;

	if (suite->ts_masked) {
		if (suite->ts_enabled)
			m0_console_printf("#\n# %s  <<<<<<<<<<<<   -=!!!  "
					  "DISABLED  !!!=-\n#\n", suite->ts_name);
		return 0;
	}

	if (!suite->ts_enabled)
		return 0;

	if (suite->ts_owners != NULL)
		m0_console_printf("%s [%s]\n",
				  suite->ts_name, suite->ts_owners);
	else
		m0_console_printf("%s \n", suite->ts_name);

	alloc_before = m0_allocated();
	mem_before   = m0_allocated_total();
	start        = m0_time_now();

	if (suite->ts_init != NULL) {
		rc = suite->ts_init();
		if (rc != 0)
			M0_ERR_INFO(rc, "Unit-test suite initialization failure.");
	}

	for (test = suite->ts_tests; test->t_name != NULL; ++test) {
#ifndef __KERNEL__
		M0_INTERNAL void m0_cs_gotsignal_reset(void);

		m0_cs_gotsignal_reset();
#endif
		run_test(test, max_name_len);
	}

	if (suite->ts_fini != NULL) {
		rc = suite->ts_fini();
		if (rc != 0)
			M0_ERR_INFO(rc, "Unit-test suite finalization failure.");
	}

	/*
	 * Check that fault injections don't move beyond the test suite
	 * boundaries. We don't want them to cause unexpected side effects to
	 * further tests.
	 */
	check_all_fi_disabled();

	end         = m0_time_now();
	mem_after   = m0_allocated_total();
	alloc_after = m0_allocated();
	/* It's possible that some earlier allocated memory was released. */
	alloc_after = max64(alloc_after, alloc_before);
	duration    = m0_time_sub(end, start);
	mem_used    = mem_after - mem_before;

	m0_console_printf(LOG_PREFIX "  [ time: %" PRIu64 ".%02" PRIu64 " sec,"
			  " mem: %sB, leaked: %sB ]\n", m0_time_seconds(duration),
			  m0_time_nanoseconds(duration) / M0_TIME_ONE_MSEC / 10,
			  skipspaces(m0_bcount_with_suffix(mem, ARRAY_SIZE(mem),
							   mem_used)),
			  skipspaces(m0_bcount_with_suffix(leak, ARRAY_SIZE(leak),
						  alloc_after - alloc_before)));
	return rc;
}

static int max_test_name_len(const struct m0_ut_suite **suites, unsigned nr)
{
	const struct m0_ut *test;
	unsigned            i;
	size_t              max_len = 0;

	for (i = 0; i < nr; ++i) {
		for (test = suites[i]->ts_tests; test->t_name != NULL; ++test)
			max_len = max_check(strlen(test->t_name), max_len);
	}
	return max_len;
}

static int tests_run_all(const struct m0_ut_module *m)
{
	int i;
	int rc;

	for (i = rc = 0; i < m->ut_suites_nr && rc == 0; ++i)
		rc = run_suite(m->ut_suites[i],
			       max_test_name_len((const struct m0_ut_suite **)
						 m->ut_suites,
						 m->ut_suites_nr));
	return rc;
}

M0_INTERNAL int m0_ut_run(void)
{
	char        leak[16];
	const char *leak_str;
	uint64_t    alloc_before;
	uint64_t    alloc_after;
	char        mem[16];
	const char *mem_str;
	uint64_t    mem_before;
	uint64_t    mem_after;
	m0_time_t   start;
	m0_time_t   duration;
	int         rc;
	const struct m0_ut_module *m = ut_module();

	alloc_before = m0_allocated();
	mem_before   = m0_allocated_total();
	start        = m0_time_now();

	rc = tests_run_all(m);

	mem_after   = m0_allocated_total();
	alloc_after = m0_allocated();
	duration    = m0_time_sub(m0_time_now(), start);
	leak_str    = skipspaces(m0_bcount_with_suffix(
					 mem, ARRAY_SIZE(mem),
					 mem_after - mem_before));
	mem_str     = skipspaces(m0_bcount_with_suffix(
					 leak, ARRAY_SIZE(leak),
					 alloc_after - alloc_before));
	if (rc == 0)
		m0_console_printf("\nTime: %" PRIu64 ".%02" PRIu64 " sec,"
				  " Mem: %sB, Leaked: %sB, Asserts: %" PRIu64
				  "\nUnit tests status: SUCCESS\n",
				  m0_time_seconds(duration),
				  m0_time_nanoseconds(duration) /
					M0_TIME_ONE_MSEC / 10,
				  leak_str, mem_str,
				  m0_atomic64_get(&m->ut_asserts));
	return rc;
}
M0_EXPORTED(m0_ut_run);

M0_INTERNAL void m0_ut_list(bool with_tests, bool yaml_output)
{
	const struct m0_ut_module *m = ut_module();
	const struct m0_ut        *t;
	int                        i;

	if (yaml_output)
		m0_console_printf("---\n");

	for (i = 0; i < m->ut_suites_nr; ++i) {
		if (yaml_output) {
			m0_console_printf("- %s:\n", m->ut_suites[i]->ts_name);
			if (m->ut_suites[i]->ts_yaml_config_string != NULL)
				m0_console_printf("    config: %s\n",
					m->ut_suites[i]->ts_yaml_config_string);
		} else {
			m0_console_printf("%s\n", m->ut_suites[i]->ts_name);
		}
		if (with_tests) {
			if (yaml_output)
				m0_console_printf("    tests:\n");
			for (t = m->ut_suites[i]->ts_tests; t->t_name != NULL; ++t)
				m0_console_printf(yaml_output ? "      - %s\n" :
						  "  %s\n", t->t_name);
		}
	}
}

static void ut_owners_print(const struct m0_ut_suite *suite)
{
	const struct m0_ut *t;

	for (t = suite->ts_tests; t->t_name != NULL; ++t) {
		if (t->t_owner != NULL)
			m0_console_printf("  %s: %s\n", t->t_name, t->t_owner);
	}
}

M0_INTERNAL void m0_ut_list_owners(void)
{
	const struct m0_ut_module *m = ut_module();
	const struct m0_ut_suite  *s;
	const struct m0_ut        *t;
	int                        i;

	for (i = 0; i < m->ut_suites_nr; ++i) {
		s = m->ut_suites[i];
		if (s->ts_owners == NULL) {
			for (t = s->ts_tests; t->t_name != NULL; ++t) {
				if (t->t_owner != NULL) {
					m0_console_printf("%s\n", s->ts_name);
					ut_owners_print(s);
					break;
				}
			}
		} else {
			m0_console_printf("%s: %s\n", s->ts_name, s->ts_owners);
			ut_owners_print(s);
		}
	}
}

M0_INTERNAL bool m0_ut_assertimpl(bool c, const char *str_c, const char *file,
				  int lno, const char *func)
{
	static char buf[4096];

	m0_atomic64_inc(&ut_module()->ut_asserts);
	if (!c) {
		snprintf(buf, sizeof buf,
			"Unit-test assertion failed: %s", str_c);
		m0_panic(&(struct m0_panic_ctx){
				.pc_expr = buf,  .pc_func   = func,
				.pc_file = file, .pc_lineno = lno,
				.pc_fmt  = NULL });
	}
	return c;
}
M0_EXPORTED(m0_ut_assertimpl);

M0_INTERNAL bool m0_ut_small_credits(void)
{
	return ut_module()->ut_small_credits;
}
M0_EXPORTED(m0_ut_small_credits);

#ifndef __KERNEL__
#include <stdlib.h>                       /* qsort */

static int cmp(const struct m0_ut_suite **s0, const struct m0_ut_suite **s1)
{
	return (*s0)->ts_order - (*s1)->ts_order;
}

M0_INTERNAL void m0_ut_shuffle(unsigned seed)
{
	struct m0_ut_module *m = ut_module();
	unsigned             i;

	M0_PRE(m->ut_suites_nr > 0);

	srand(seed);
	for (i = 1; i < m->ut_suites_nr; ++i)
		m->ut_suites[i]->ts_order = rand();
	qsort(m->ut_suites + 1, m->ut_suites_nr - 1, sizeof m->ut_suites[0],
	      (void *)&cmp);
}

M0_INTERNAL void m0_ut_start_from(const char *suite)
{
	struct m0_ut_module *m = ut_module();
	unsigned             i;
	int                  o;

	for (i = 0, o = 0; i < m->ut_suites_nr; ++i, ++o) {
		if (m0_streq(m->ut_suites[i]->ts_name, suite))
			o = -m->ut_suites_nr - 1;
		m->ut_suites[i]->ts_order = o;
	}
	qsort(m->ut_suites, m->ut_suites_nr, sizeof m->ut_suites[0],
	      (void *)&cmp);
}
#endif

struct m0_fid g_process_fid = M0_FID_TINIT('r', 1, 1);

/** @} ut */
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
