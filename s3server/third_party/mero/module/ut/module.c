/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 10-Jan-2014
 */

#include "module/instance.h"
#include "module/param.h"
#include "lib/string.h"       /* m0_streq */
#include "lib/thread.h"       /* m0_thread */
#include "ut/ut.h"

static char       g_log[32] = "";
static struct m0 *g_instance;

static void _log(char c1, char c2)
{
	int    rc;
	size_t len = strlen(g_log);

	M0_PRE(len + 2 < sizeof g_log);

	rc = snprintf(g_log + len, sizeof(g_log) - len, "%c%c", c1, c2);
	M0_ASSERT(rc == 2);
}

static int modlev_enter(struct m0_module *module)
{
	/* Append "<module name> <next level>" record (2 chars) to the log. */
	_log(*module->m_name, '0' + module->m_cur + 1);
	return 0;
}

static void modlev_leave(struct m0_module *module)
{
	/* Append "<module name> <current level>" record to the log. */
	_log(*module->m_name, '0' + module->m_cur);
}

static struct m0_module modules[] = {
	{ .m_name = "a" },
	{ .m_name = "b" },
	{ .m_name = "c" },
	{ .m_name = "d" },
	{ .m_name = "e" },
	{ .m_name = "f" },
	{ .m_name = "g" },
	{ .m_name = "h" },
	{ .m_name = "i" }
};
enum module_id { A, B, C, D, E, F, G, H, I };

/*                             +------+
 *                             |  c1  |
 *               +------+      +------+
 *               |  b3  |----->|  c0  |---.
 * +------+      +------+      +------+   |
 * |  a3  | ~~-> |  b2  |<--.             |
 * +------+      +------+   |             |
 * |  a2  |      |  b1  |   |             |
 * +------+      +------+   |  +------+   |
 * |  a1  |<-----|  b0  |   |  |  d2  |<--'
 * +------+      +------+   |  +------+
 * |  a0  |                 `--|  d1  |
 * +------+                    +------+
 *                             |  d0  |
 *                             +------+
 */

static const struct m0_modlev levels[] = {
	{ .ml_enter = modlev_enter, .ml_leave = modlev_leave },
	{ .ml_enter = modlev_enter, .ml_leave = modlev_leave },
	{ .ml_enter = modlev_enter, .ml_leave = modlev_leave },
	{ .ml_enter = modlev_enter, .ml_leave = modlev_leave }
};

static struct m0_moddep dep_a[] = { /* no dependencies initially */ };
static struct m0_moddep inv_a[] = { M0_MODDEP_INIT(&modules[B], 0, 1) };

static struct m0_moddep dep_b[] = {
	M0_MODDEP_INIT(&modules[A], 0, 1),
	M0_MODDEP_INIT(&modules[C], 3, 0)
};
static struct m0_moddep inv_b[] = { M0_MODDEP_INIT(&modules[D], 1, 2) };

static struct m0_moddep dep_c[] = { M0_MODDEP_INIT(&modules[D], 0, 2) };
static struct m0_moddep inv_c[] = { M0_MODDEP_INIT(&modules[B], 3, 0) };

static struct m0_moddep dep_d[] = { M0_MODDEP_INIT(&modules[B], 1, 2) };
static struct m0_moddep inv_d[] = { M0_MODDEP_INIT(&modules[C], 0, 2) };

/*                                           +------+
 *               +------+   ,--------------->|  i0  |---.
 *               |  f3  |---'                +------+   |
 *               +------+      +------+                 |
 *               |  f2  |----->|  g2  |<----------------{
 *               +------+      +------+                 |
 *           ,-->|  f1  |      |  g1  |                 |
 * +------+  |   +------+      +------+      +------+   |
 * |  e0  |--'   |  f0  |      |  g0  |----->|  h0  |<--'
 * +------+      +------+      +------+      +------+
 */

static struct m0_moddep dep_e[] = { M0_MODDEP_INIT(&modules[F], 0, 1) };
static struct m0_moddep inv_e[0];

static struct m0_moddep dep_f[] = {
	M0_MODDEP_INIT(&modules[G], 2, 2),
	M0_MODDEP_INIT(&modules[I], 3, 0)
};
static struct m0_moddep inv_f[] = { M0_MODDEP_INIT(&modules[E], 0, 1) };

static struct m0_moddep dep_g[] = { M0_MODDEP_INIT(&modules[H], 0, 0) };
static struct m0_moddep inv_g[] = {
	M0_MODDEP_INIT(&modules[F], 2, 2),
	M0_MODDEP_INIT(&modules[I], 0, 2)
};

static struct m0_moddep dep_h[0];
static struct m0_moddep inv_h[] = {
	M0_MODDEP_INIT(&modules[G], 0, 0),
	M0_MODDEP_INIT(&modules[I], 0, 0)
};

static struct m0_moddep dep_i[] = {
	M0_MODDEP_INIT(&modules[G], 0, 2),
	M0_MODDEP_INIT(&modules[H], 0, 0)
};
static struct m0_moddep inv_i[] = { M0_MODDEP_INIT(&modules[F], 3, 0) };

static void _reset(void)
{
	static struct {
		int               level_nr;
		struct m0_moddep *dep;
		unsigned          dep_nr;
		struct m0_moddep *inv;
		unsigned          inv_nr;
	} mods[] = {
#define ARR_INIT(name) name, ARRAY_SIZE(name)
#define MOD_REC(name, nr_levels) \
		{ (nr_levels), ARR_INIT(dep_ ## name), ARR_INIT(inv_ ## name) }

		MOD_REC(a, 4),
		MOD_REC(b, 4),
		MOD_REC(c, 2),
		MOD_REC(d, 3),
		MOD_REC(e, 1),
		MOD_REC(f, 4),
		MOD_REC(g, 3),
		MOD_REC(h, 1),
		MOD_REC(i, 1)
#undef MOD_REC
#undef ARR_INIT
	};
	unsigned i;

	M0_CASSERT(ARRAY_SIZE(mods) == ARRAY_SIZE(modules));
	for (i = 0; i < ARRAY_SIZE(mods); ++i) {
		modules[i].m_m0       = m0_get();
		modules[i].m_cur      = M0_MODLEV_NONE;
		M0_ASSERT(mods[i].level_nr <= ARRAY_SIZE(levels));
		modules[i].m_level    = levels;
		modules[i].m_level_nr = mods[i].level_nr;
		modules[i].m_dep_nr   = mods[i].dep_nr;
		modules[i].m_inv_nr   = mods[i].inv_nr;
		memcpy(modules[i].m_dep, mods[i].dep,
		       mods[i].dep_nr * sizeof modules[i].m_dep[0]);
		memcpy(modules[i].m_inv, mods[i].inv,
		       mods[i].inv_nr * sizeof modules[i].m_inv[0]);
	}
	*g_log = 0;
}

static int cur(enum module_id id)
{
	M0_PRE(IS_IN_ARRAY(id, modules));
	return modules[id].m_cur;
}

static void _test_module_init(void)
{
	int rc;

	_reset();

	M0_UT_ASSERT(m0_forall(i, ARRAY_SIZE(modules),
			       cur(i) == M0_MODLEV_NONE));
	rc = m0_module_init(&modules[B], 1);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(cur(A) == 1);
	M0_UT_ASSERT(cur(B) == 1);
	M0_UT_ASSERT(cur(C) == M0_MODLEV_NONE);
	M0_UT_ASSERT(cur(D) == M0_MODLEV_NONE);
	M0_UT_ASSERT(m0_streq(g_log, "a0a1b0b1"));

	rc = m0_module_init(&modules[B], 3);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(cur(A) == 1);
	M0_UT_ASSERT(cur(B) == 3);
	M0_UT_ASSERT(cur(C) == 0);
	M0_UT_ASSERT(cur(D) == 2);
	M0_UT_ASSERT(m0_streq(g_log, "a0a1b0b1b2d0d1d2c0b3"));
}

static void _test_module_fini(void)
{
	*g_log = 0;

	m0_module_fini(&modules[B], M0_MODLEV_NONE);
	M0_UT_ASSERT(cur(A) == M0_MODLEV_NONE);
	M0_UT_ASSERT(cur(B) == M0_MODLEV_NONE);
	M0_UT_ASSERT(cur(C) == M0_MODLEV_NONE);
	M0_UT_ASSERT(cur(D) == M0_MODLEV_NONE);
	M0_UT_ASSERT(m0_streq(g_log, "b3c0d2d1b2b1b0a1a0d0"));

	m0_module_fini(&modules[A], M0_MODLEV_NONE); /* a noop */
}

static int modlev_a2_enter(struct m0_module *module)
{
	M0_PRE(module == &modules[A]);

	m0_module_dep_add(module, 3, &modules[B], 2);
	return modlev_enter(module);
}

static void _test_module_dep_add(void)
{
	struct m0_modlev levels_a[ARRAY_SIZE(levels)];
	int              rc;

	_reset();

	memcpy(levels_a, levels, sizeof levels);
	levels_a[2].ml_enter = modlev_a2_enter;
	M0_ASSERT(modules[A].m_level == levels);
	modules[A].m_level = levels_a;

	M0_UT_ASSERT(modules[A].m_dep_nr == 0);
	/*
	 * m0_module_dep_add() is called implicitly: ->m_enter() callback,
	 * invoked when module A enters level 2, creates a3 -> b2 dependency.
	 */
	rc = m0_module_init(&modules[A], 3);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(modules[A].m_dep_nr == 1);
	M0_UT_ASSERT(cur(A) == 3);
	M0_UT_ASSERT(cur(B) == 2);
	M0_UT_ASSERT(cur(C) == M0_MODLEV_NONE);
	M0_UT_ASSERT(cur(D) == M0_MODLEV_NONE);
	M0_UT_ASSERT(m0_streq(g_log, "a0a1a2b0b1b2a3"));

	m0_module_fini(&modules[A], M0_MODLEV_NONE);
}

static void _test_module_fini_all(void)
{
	int rc;

	_reset();

	rc = m0_module_init(&modules[E], 0) ?: m0_module_init(&modules[F], 3);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(cur(E) == 0);
	M0_UT_ASSERT(cur(F) == 3);
	M0_UT_ASSERT(cur(G) == 2);
	M0_UT_ASSERT(cur(H) == 0);
	M0_UT_ASSERT(cur(I) == 0);
	M0_UT_ASSERT(m0_streq(g_log, "f0f1e0h0g0g1g2f2i0f3"));

	*g_log = 0;
	m0_module_fini(&modules[F], M0_MODLEV_NONE);
	M0_UT_ASSERT(cur(E) == 0);
	M0_UT_ASSERT(cur(F) == 1);
	M0_UT_ASSERT(cur(G) == M0_MODLEV_NONE);
	M0_UT_ASSERT(cur(H) == M0_MODLEV_NONE);
	M0_UT_ASSERT(cur(I) == M0_MODLEV_NONE);
	M0_UT_ASSERT(m0_streq(g_log, "f3i0f2g2g1g0h0"));

	m0_module_fini(&modules[E], M0_MODLEV_NONE);
	M0_UT_ASSERT(cur(E) == M0_MODLEV_NONE);
	M0_UT_ASSERT(cur(F) == M0_MODLEV_NONE);
	M0_UT_ASSERT(m0_streq(g_log, "f3i0f2g2g1g0h0e0f1f0"));
}

/*      amb
 *  +---------+
 *  |         |
 *  |   foo   |      bar
 *  | +-----+ |    +-----+
 *  | |  0  |<-----|  0  |
 *  | +-----+ |    +-----+
 *  +---------+
 */

/*
 * This ambient structure is not needed for _test_module_alt_init() to
 * work, but it resembles the structure of m0 instance and is used for
 * thinking over the initialisation of struct m0:
 *
 *   amb ~ m0, amb::a_foo ~ m0::i_net ("~" stands for "resembles").
 *
 * XXX DELETEME: Get rid of _test_module_alt_init(), struct amb, and the
 * accompanying functions, once the initialisation of m0 instance with
 * m0_module_init() is in place.
 */
struct amb {
	struct m0_module a_self;
	struct m0_module a_foo;
};

static void foobar_setup(struct m0_module *self, const char *name,
			 struct m0_module *other, bool source)
{
	static const struct m0_modlev levels[] = {
		{ .ml_enter = modlev_enter }
	};
	struct m0_moddep deps[] = { M0_MODDEP_INIT(other, 0, 0) };

	*self = (struct m0_module){
		.m_name     = name,
		.m_m0       = m0_get(),
		.m_cur      = M0_MODLEV_NONE,
		.m_level    = levels,
		.m_level_nr = ARRAY_SIZE(levels)
	};

	if (source) {
		memcpy(self->m_dep, deps, sizeof deps);
		self->m_dep_nr = ARRAY_SIZE(deps);
	} else {
		memcpy(self->m_inv, deps, sizeof deps);
		self->m_inv_nr = ARRAY_SIZE(deps);
	}
}

static void amb_setup(struct amb *amb, struct m0_module *bar)
{
	*amb = (struct amb){
		.a_self = { .m_name = "amb module" }
		/*
		 * Static initialiser of amb::a_foo is not available,
		 * because the initialisers of m0_module::m_{level,dep,inv}
		 * arrays are not visible here. This situation is quite
		 * common.
		 */
	};
	foobar_setup(&amb->a_foo, "foo module", bar, false);
}

static void _test_module_alt_init(void)
{
	struct amb       amb;
	struct m0_module bar;
	int              rc;

	amb_setup(&amb, &bar);
	foobar_setup(&bar, "bar module", &amb.a_foo, true);

	*g_log = 0;
	rc = m0_module_init(&bar, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(amb.a_foo.m_cur == 0);
	M0_UT_ASSERT(bar.m_cur == 0);
	M0_UT_ASSERT(m0_streq(g_log, "f0b0"));
	m0_module_fini(&bar, M0_MODLEV_NONE);
}

static void test_module(void)
{
	_test_module_init();
	_test_module_fini();
	_test_module_dep_add();
	_test_module_fini_all();
	_test_module_alt_init();
}

static void inherit(int _)
{
	struct m0 *orig;
	struct m0 *inst;
	struct m0  local;

	orig = inst = m0_get();
	M0_UT_ASSERT(inst == g_instance);

	m0_set(&local);
	inst = m0_get();
	/*
	 * We cannot use M0_UT_ASSERT() here, because m0_ut_assertimpl()
	 * obtains the address of m0_ut_module from m0 instance, and since
	 * `local' is zeroed, no valid pointers can be obtained from it.
	 */
	M0_ASSERT(inst == &local);
	m0_set(orig);
}

static void test_instance(void)
{
	struct m0_thread t = {0};
	struct m0       *inst;
	int              rc;

	g_instance = m0_get();

	rc = M0_THREAD_INIT(&t, int, NULL, &inherit, 0, "heir");
	M0_ASSERT(rc == 0);
	m0_thread_join(&t);

	/* Ensure that subthread's m0_set() has no impact on current thread's
	 * TLS. */
	inst = m0_get();
	M0_UT_ASSERT(inst == g_instance);
}

static const char *kv_get(const struct m0_param_source *_, const char *key)
{
	static struct {
		const char *key;
		const char *val;
	} db[] = {
		{ "rose",   "red" },
		{ "violet", "blue" }
	};
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(db); ++i) {
		if (m0_streq(db[i].key, key))
			return db[i].val;
	}
	return NULL;
}

static void test_param(void)
{
	struct m0_param_source param = {
		.ps_param_get = (void *(*)(const struct m0_param_source *,
					   const char *))kv_get
	};
	const char            *s;

	m0_param_source_add(&param);
	s = m0_param_get("rose");
	M0_UT_ASSERT(m0_streq(s, "red"));
	s = m0_param_get("daisy");
	M0_UT_ASSERT(s == NULL);
	m0_param_source_del(&param);
}

struct m0_ut_suite module_ut = {
	.ts_name  = "module-ut",
	.ts_tests = {
		{ "module",   test_module },
		{ "instance", test_instance },
		{ "param",    test_param },
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
