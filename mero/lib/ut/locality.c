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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 04-Jun-2013
 */

#include "lib/misc.h"           /* m0_forall */
#include "lib/mutex.h"
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/locality.h"
#include "lib/finject.h"
#include "fop/fom.h"
#include "fop/fom_simple.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "ut/ut.h"

enum {
	NR = 4096,
	X_VALUE = 6
};

static bool                 passed[NR];
static uint64_t             core[NR];
static struct m0_mutex      lock;
static struct m0_semaphore  sem[NR];
static struct m0_sm_ast     ast[NR];
static struct m0_reqh       reqh;
static struct m0_fom_simple s[NR];
static struct m0_atomic64   hoarded;
static bool                 free_func_called;

static void fom_simple_svc_start(void)
{
	struct m0_reqh_service_type *stype;
	struct m0_reqh_service      *service;
	int                          rc;

	stype = m0_reqh_service_type_find("simple-fom-service");
	M0_ASSERT(stype != NULL);
	rc = m0_reqh_service_allocate(&service, stype, NULL);
	M0_ASSERT(rc == 0);
	m0_reqh_service_init(service, &reqh,
			     &M0_FID_INIT(0xdeadbeef, 0xbeefdead));
	rc = m0_reqh_service_start(service);
	M0_ASSERT(rc == 0);
	M0_POST(m0_reqh_service_invariant(service));
}

static void _reqh_init(void)
{
	int result;

	M0_SET0(&reqh);
	result = M0_REQH_INIT(&reqh,
			      .rhia_dtm       = (void*)1,
			      .rhia_db        = NULL,
			      .rhia_mdstore   = (void*)1,
			      .rhia_fid       = &g_process_fid,
		);
	M0_UT_ASSERT(result == 0);
	m0_reqh_start(&reqh);
	fom_simple_svc_start();
}

static void _reqh_fini(void)
{
	m0_reqh_shutdown(&reqh);
	m0_reqh_services_terminate(&reqh);
	m0_reqh_fini(&reqh);
}

static void _cb0(struct m0_sm_group *grp, struct m0_sm_ast *a)
{
	unsigned idx = a - ast;
	M0_UT_ASSERT(IS_IN_ARRAY(idx, ast));
	m0_mutex_lock(&lock);
	passed[idx] = true;
	core[m0_processor_id_get()]++;
	m0_mutex_unlock(&lock);
	m0_semaphore_up(&sem[idx]);
}

static int expected;
static int simple_tick(struct m0_fom *fom, int *x, int *__unused)
{

	M0_UT_ASSERT(*x == expected);

	++expected;
	if (++*x < NR)
		return M0_FSO_AGAIN;
	else {
		m0_semaphore_up(&sem[0]);
		return -1;
	}
}

static int tick_once(struct m0_fom *fom, int *x, int *__unused)
{
	return -1;
}

void free_func(struct m0_fom_simple *sfom)
{
	free_func_called = true;
	m0_semaphore_up(&sem[0]);
}

enum {
	SEMISIMPLE_S0 = M0_FOM_PHASE_FINISH + 1,
	SEMISIMPLE_S1,
	SEMISIMPLE_S2,
};

static struct m0_sm_state_descr semisimple_phases[] = {
	[M0_FOM_PHASE_INIT] = {
		.sd_name      = "init",
		.sd_allowed   = M0_BITS(SEMISIMPLE_S0),
		.sd_flags     = M0_SDF_INITIAL
	},
	[SEMISIMPLE_S0] = {
		.sd_name      = "ss0",
		.sd_allowed   = M0_BITS(SEMISIMPLE_S1)
	},
	[SEMISIMPLE_S1] = {
		.sd_name      = "ss1",
		.sd_allowed   = M0_BITS(SEMISIMPLE_S1, SEMISIMPLE_S2)
	},
	[SEMISIMPLE_S2] = {
		.sd_name      = "ss2",
		.sd_allowed   = M0_BITS(M0_FOM_PHASE_FINISH)
	},
	[M0_FOM_PHASE_FINISH] = {
		.sd_name      = "done",
		.sd_flags     = M0_SDF_TERMINAL
	}
};

static struct m0_sm_conf semisimple_conf = {
	.scf_name      = "semisimple fom",
	.scf_nr_states = ARRAY_SIZE(semisimple_phases),
	.scf_state     = semisimple_phases
};

static int semisimple_tick(struct m0_fom *fom, int *x, int *phase)
{
	switch (*phase) {
	case M0_FOM_PHASE_INIT:
		M0_UT_ASSERT(*x == NR);
		*phase = SEMISIMPLE_S0;
		return M0_FSO_AGAIN;
	case SEMISIMPLE_S0:
		M0_UT_ASSERT(*x == NR);
		*phase = SEMISIMPLE_S1;
		--*x;
		m0_semaphore_up(&sem[0]);
		return M0_FSO_WAIT;
	case SEMISIMPLE_S1:
		M0_UT_ASSERT(*x < NR);
		*phase = --*x == 0 ? SEMISIMPLE_S2 : SEMISIMPLE_S1;
		return M0_FSO_AGAIN;
	case SEMISIMPLE_S2:
		M0_UT_ASSERT(*x == 0);
		m0_semaphore_up(&sem[0]);
		return -1;
	default:
		M0_UT_ASSERT(0);
		return -1;
	}
}

static int cat_tick(struct m0_fom *fom, void *null, int *__unused)
{
	struct m0_fom_simple *whisker = container_of(fom, struct m0_fom_simple,
						     si_fom);
	int idx = whisker->si_locality;
	M0_UT_ASSERT(null == NULL);
	M0_UT_ASSERT(IS_IN_ARRAY(idx, sem));
	M0_UT_ASSERT(whisker == &s[idx]);
	m0_atomic64_inc(&hoarded);
	m0_semaphore_up(&sem[idx]);
	return -1;
}

void test_locality(void)
{
	unsigned             i;
	struct m0_bitmap     online;
	int                  result;
	size_t               here;
	int                  nr;

	m0_mutex_init(&lock);
	_reqh_init();
	for (i = 0; i < ARRAY_SIZE(sem); ++i) {
		m0_semaphore_init(&sem[i], 0);
		ast[i].sa_cb = &_cb0;
	}

	here = m0_locality_here()->lo_idx;
	m0_sm_ast_post(m0_locality_get(here)->lo_grp, &ast[0]);
	m0_semaphore_down(&sem[0]);
	M0_UT_ASSERT(passed[0]);
	passed[0] = false;
	M0_UT_ASSERT(m0_forall(j, ARRAY_SIZE(core), core[j] == !!(j == here)));
	core[here] = 0;

	for (i = 0; i < NR; ++i)
		m0_sm_ast_post(m0_locality_get(i)->lo_grp, &ast[i]);

	for (i = 0; i < NR; ++i)
		m0_semaphore_down(&sem[i]);

	result = m0_bitmap_init(&online, m0_processor_nr_max());
	M0_ASSERT(result == 0);
	m0_processors_online(&online);

	M0_UT_ASSERT(m0_forall(j, NR, passed[j]));
	M0_UT_ASSERT(m0_forall(j, ARRAY_SIZE(core),
			       (core[j] != 0) == (j < online.b_nr &&
						  m0_bitmap_get(&online, j))));
	nr = 0;
	free_func_called = false;
	M0_SET0(&s[0]);
	M0_FOM_SIMPLE_POST(&s[0], &reqh, NULL, &tick_once, free_func, &nr, 1);
	m0_semaphore_down(&sem[0]);
	m0_reqh_idle_wait(&reqh);
	M0_UT_ASSERT(free_func_called);

	nr = 0;
	M0_SET0(&s[0]);
	M0_FOM_SIMPLE_POST(&s[0], &reqh, NULL, &simple_tick, NULL, &nr, 1);
	m0_semaphore_down(&sem[0]);
	M0_UT_ASSERT(nr == NR);
	m0_reqh_idle_wait(&reqh);
	M0_SET0(&s[0]);
	M0_FOM_SIMPLE_POST(&s[0], &reqh, &semisimple_conf,
			   &semisimple_tick, NULL, &nr, M0_FOM_SIMPLE_HERE);
	m0_semaphore_down(&sem[0]);
	m0_fom_wakeup(&s[0].si_fom);
	m0_semaphore_down(&sem[0]);
	M0_UT_ASSERT(nr == 0);
	m0_reqh_idle_wait(&reqh);
	M0_SET0(&s[0]);
	m0_atomic64_set(&hoarded, 0);
	memset(s, 0, sizeof s);
	m0_fom_simple_hoard(s, ARRAY_SIZE(s), &reqh, NULL,
			    &cat_tick, NULL, NULL);
	for (i = 0; i < ARRAY_SIZE(sem); ++i)
		m0_semaphore_down(&sem[i]);
	M0_UT_ASSERT(m0_atomic64_get(&hoarded) == NR);
	for (i = 0; i < ARRAY_SIZE(sem); ++i)
		m0_semaphore_fini(&sem[i]);
	m0_bitmap_fini(&online);
	_reqh_fini();
	m0_mutex_fini(&lock);
}
M0_EXPORTED(test_locality);

static int entered;
static int left;
static int ticked;
static int key0;
static int key;
static int keyother;
static bool has0;

static int ctor(void *area, void *cookie)
{
	M0_UT_ASSERT(cookie == &ctor);
	((char *)area)[0] = 'x';
	return 0;
}

static int enter(struct m0_locality_chore *chore,
		 struct m0_locality *loc, void *place)
{
	char *data      = m0_locality_data(key);
	char *data0     = m0_locality_data(key0);
	char *dataother = m0_locality_data(keyother);

	M0_UT_ASSERT(chore->lc_datum == &lock);
	M0_UT_ASSERT(place != NULL);
	M0_UT_ASSERT(data != NULL);
	M0_UT_ASSERT(data[0] == 'x');
	M0_UT_ASSERT(dataother != NULL);
	M0_UT_ASSERT(dataother[0] == 0);
	M0_UT_ASSERT((data0 != NULL) == has0);
	m0_mutex_lock(&lock);
	++entered;
	m0_mutex_unlock(&lock);
	*(long *)place = (long)chore + (long)loc + (long)place;
	return 0;
}

static void leave(struct m0_locality_chore *chore,
		  struct m0_locality *loc, void *place)
{
	M0_UT_ASSERT(chore->lc_datum == &lock);
	M0_UT_ASSERT(place != NULL);
	m0_mutex_lock(&lock);
	M0_UT_ASSERT(*(long *)place == (long)chore + (long)loc + (long)place);
	++left;
	m0_mutex_unlock(&lock);
}

static void tick(struct m0_locality_chore *chore,
		 struct m0_locality *loc, void *place)
{
	M0_UT_ASSERT(chore->lc_datum == &lock);
	M0_UT_ASSERT(place != NULL);
	m0_mutex_lock(&lock);
	++ticked;
	m0_mutex_unlock(&lock);
}

static int nosys(struct m0_locality_chore *chore,
		 struct m0_locality *loc, void *place)
{
	return -ENOSYS;
}

void test_locality_chore(void)
{
	struct m0_locality_chore           chore;
	int                                result;
	int                                nr_loc;
	struct m0_locality_chore_ops ops = {
		.co_enter = &enter,
		.co_leave = &leave,
		.co_tick  = &tick
	};

	memset(passed, 0, sizeof passed);
	memset(core,   0, sizeof core);
	memset(sem,    0, sizeof sem);
	memset(ast,    0, sizeof ast);
	memset(s,      0, sizeof s);
	M0_SET0(&reqh);
	M0_SET0(&lock);
	M0_SET0(&hoarded);
	free_func_called = false;
	expected = 0;

	m0_mutex_init(&lock);
	nr_loc = m0_fom_dom()->fd_localities_nr;
	key = m0_locality_data_alloc(721, &ctor, NULL, &ctor);
	M0_UT_ASSERT(key >= 0);
	key0 = m0_locality_data_alloc(721, NULL, NULL, NULL);
	has0 = true;
	M0_UT_ASSERT(key0 >= 0);
	M0_UT_ASSERT(key != key0);
	keyother = m0_locality_data_alloc(721, NULL, NULL, NULL);
	M0_UT_ASSERT(keyother >= 0);
	entered = left = ticked = 0;
	M0_SET0(&chore);
	result = m0_locality_chore_init(&chore, &ops, &lock,
					M0_MKTIME(1, 0), 4096);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(entered == nr_loc);
	M0_UT_ASSERT(left == 0);
	m0_locality_chore_fini(&chore);
	M0_UT_ASSERT(entered == nr_loc);
	M0_UT_ASSERT(left == nr_loc);

	M0_SET0(&chore);
	m0_fi_enable_once("m0_alloc", "keep_quiet");
	result = m0_locality_chore_init(&chore, &ops, &lock,
					M0_MKTIME(1, 0), 1ULL << 60);
	M0_UT_ASSERT(result == -ENOMEM);

	M0_SET0(&chore);
	ops.co_enter = &nosys;
	result = m0_locality_chore_init(&chore, &ops, &lock,
					M0_MKTIME(1, 0), 4096);
	M0_UT_ASSERT(result == -ENOSYS);
	m0_locality_data_free(key0);
	has0 = false;
	_reqh_init();
	ops.co_enter = &enter;
	entered = left = ticked = 0;
	M0_SET0(&chore);
	result = m0_locality_chore_init(&chore, &ops, &lock,
					M0_MKTIME(1, 0), 4096);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(entered == nr_loc);
	M0_UT_ASSERT(left == 0);
	m0_locality_chore_fini(&chore);
	M0_UT_ASSERT(entered == nr_loc);
	M0_UT_ASSERT(left == nr_loc);
	m0_locality_data_free(keyother);
	m0_fi_enable_once("m0_alloc", "keep_quiet");
	result = m0_locality_data_alloc(1ULL << 60, NULL, NULL, NULL);
	M0_UT_ASSERT(result == -ENOMEM);
	_reqh_fini();
	m0_locality_data_free(key);
	m0_mutex_fini(&lock);
}
M0_EXPORTED(test_locality_chore);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
