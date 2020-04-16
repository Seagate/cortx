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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 * Refactoring    : Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 15-Jan-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "fop/fom_long_lock.h"
#include "fop/fom_generic.h"   /* m0_generic_conf, M0_FOPH_NR */
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "rpc/rpc_opcodes.h"   /* M0_UB_FOM_OPCODE */
#include "lib/memory.h"        /* m0_free */
#include "lib/ub.h"            /* M0_UB_ASSERT */

static struct m0_reqh          g_reqh;
static struct m0_reqh_service *g_svc;
/* 8 MB --- approximately twice the size of L3 cache on Mero team VMs. */
static char                    g_mem[8 * (1 << 20)];
static struct m0_mutex        *g_mutexes;
static size_t                  g_mutexes_nr;
static struct m0_long_lock     g_long_lock;

/** Benchmark presets. */
enum {
	/** How many FOMs should a benchmark add to a request handler. */
	ST_NR_FOMS        = 20000,

	/** Reciprocal for the frequency of writer locks. */
	ST_WRITER_PERIOD  = 1000,

	/** Default number of cpu_utilize() loop iterations. */
	ST_CYCLES_DEFAULT = 50,

	/** The number of cpu_utilize() iterations for SC_MEM_B scenario. */
	ST_CYCLES_BYTES   = ST_CYCLES_DEFAULT * 20,
};

/** Types of benchmarks. */
enum scenario {
	/** Accessing bytes of memory. */
	SC_MEM_B,

	/** Accessing kilobytes of memory. */
	SC_MEM_KB,

	/** Accessing megabytes of memory. */
	SC_MEM_MB,

	/** Taking and releasing a mutex. */
	SC_MUTEX,

	/** Taking and releasing mutexes --- one mutex per locality. */
	SC_MUTEX_PER_CPU,

	/** Taking and releasing a long lock. */
	SC_LONG_LOCK,

	/** Calling m0_fom_block_{enter,leave}(). */
	SC_BLOCK,

	SC_NR
};

/** Benchmark FOM. */
struct ub_fom {
	/** Generic m0_fom object. */
	struct m0_fom            uf_gen;

	/** Sequential number of this FOM. */
	size_t                   uf_seqn;

	/** Test to perform. */
	enum scenario            uf_test;

	/** Long lock link for SC_LONG_LOCK test. */
	struct m0_long_lock_link uf_link;
};

/* ----------------------------------------------------------------
 * Ticks
 * ---------------------------------------------------------------- */

static void cpu_utilize(const struct ub_fom *mach);

static void cpu_utilize_with_lock(struct ub_fom *mach, size_t lock_idx)
{
	M0_PRE(lock_idx < g_mutexes_nr);

	m0_mutex_lock(&g_mutexes[lock_idx]);
	cpu_utilize(mach);
	m0_mutex_unlock(&g_mutexes[lock_idx]);
}

/** No I/O overhead. */
static int mem_tick(struct m0_fom *fom)
{
	cpu_utilize(container_of(fom, struct ub_fom, uf_gen));
	/*
	 * Let request handler delete this fom.
	 *
	 * See `m0_fom_phase(fom) == M0_FOM_PHASE_FINISH' comment in
	 * fop/fom.h.
	 */
	m0_fom_phase_set(fom, M0_FOM_PHASE_FINISH);
	return M0_FSO_WAIT;
}

/** I/O overhead: acquisition of mutex shared between localities. */
static int mutex_tick(struct m0_fom *fom)
{
	cpu_utilize_with_lock(container_of(fom, struct ub_fom, uf_gen), 0);

	m0_fom_phase_set(fom, M0_FOM_PHASE_FINISH);
	return M0_FSO_WAIT;
}

/**
 * I/O overhead: several FOMs of one locality may contend for the same
 * mutex.
 *
 * Acquires the mutex associated with current locality and not shared
 * with other localities.  There is no concurrency with other FOMs of
 * current locality, because a locality runs its ->fo_tick()s
 * sequentially.
 */
static int mutex_per_cpu_tick(struct m0_fom *fom)
{
	struct ub_fom *m = container_of(fom, struct ub_fom, uf_gen);

	cpu_utilize_with_lock(m, m->uf_seqn % g_mutexes_nr);

	m0_fom_phase_set(fom, M0_FOM_PHASE_FINISH);
	return M0_FSO_WAIT;
}

/** I/O overhead: acquisition of m0_long_lock shared between localities. */
static int long_lock_tick(struct m0_fom *fom)
{
	enum {
		REQ_LOCK = M0_FOM_PHASE_INIT,
		GOT_LOCK = M0_FOPH_NR + 1
	};
	struct ub_fom *m;
	int            phase;
	bool           is_writer;
	bool         (*lock)(struct m0_long_lock *lock,
			     struct m0_long_lock_link *link, int next_phase);
	void         (*unlock)(struct m0_long_lock *lock,
			       struct m0_long_lock_link *link);

	m = container_of(fom, struct ub_fom, uf_gen);
	is_writer = (m->uf_seqn % ST_WRITER_PERIOD == 0);
	lock   = is_writer ? m0_long_write_lock   : m0_long_read_lock;
	unlock = is_writer ? m0_long_write_unlock : m0_long_read_unlock;

	phase = m0_fom_phase(fom);
	if (phase == REQ_LOCK)
		/* Initialise lock acquisition. */
		return M0_FOM_LONG_LOCK_RETURN(lock(&g_long_lock, &m->uf_link,
						    GOT_LOCK));
	M0_ASSERT(phase == GOT_LOCK);
	cpu_utilize(m);
	unlock(&g_long_lock, &m->uf_link);

	m0_fom_phase_set(fom, M0_FOM_PHASE_FINISH);
	return M0_FSO_WAIT;
}

/** I/O overhead: m0_loc_thread creation. */
static int block_tick(struct m0_fom *fom)
{
	m0_fom_block_enter(fom);
	cpu_utilize_with_lock(container_of(fom, struct ub_fom, uf_gen), 0);
	m0_fom_block_leave(fom);

	m0_fom_phase_set(fom, M0_FOM_PHASE_FINISH);
	return M0_FSO_WAIT;
}

static size_t mem_mask(enum scenario test)
{
	if (test == SC_MEM_B)
		return 0xff;    /* 256 bytes max */
	else if (test == SC_MEM_MB)
		return 0xfffff; /* 1 MB max */
	else
		return 0xffff;  /* 64 KB max */
}

static size_t cycles(enum scenario test)
{
	return test == SC_MEM_B ? ST_CYCLES_BYTES : ST_CYCLES_DEFAULT;
}

/** Performs raw memory operations. */
static void cpu_utilize(const struct ub_fom *mach)
{
	volatile char x M0_UNUSED;
	size_t        left;
	size_t        start;
	size_t        len;
	size_t        i;

	M0_PRE(0 <= mach->uf_test && mach->uf_test < SC_NR);

	start = mach->uf_seqn % ARRAY_SIZE(g_mem);
	len = min_check(ARRAY_SIZE(g_mem) - start,
			start & mem_mask(mach->uf_test));
	M0_ASSERT(start + len <= ARRAY_SIZE(g_mem));

	for (left = cycles(mach->uf_test); left > 0; --left) {
		for (i = start; i < start + len; ++i) {
			switch (mach->uf_seqn % 3) {
			case 0:
				g_mem[i] = (char)i; /* write */
				break;
			case 1:
				x = g_mem[i];       /* read */
				break;
			default:
				++g_mem[i];         /* read and write */
			}
		}
	}
}

static int (*ticks[SC_NR])(struct m0_fom *fom) = {
	[SC_MEM_B]         = mem_tick,
	[SC_MEM_KB]        = mem_tick,
	[SC_MEM_MB]        = mem_tick,
	[SC_MUTEX]         = mutex_tick,
	[SC_MUTEX_PER_CPU] = mutex_per_cpu_tick,
	[SC_LONG_LOCK]     = long_lock_tick,
	[SC_BLOCK]         = block_tick
};

/* ----------------------------------------------------------------
 * Service operations
 * ---------------------------------------------------------------- */

static int dummy_service_start(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL);
	return 0;
}

static void dummy_service_stop(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL);
}

static void dummy_service_fini(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL);
	m0_free(service);
}

static const struct m0_reqh_service_ops dummy_service_ops = {
	.rso_start = dummy_service_start,
	.rso_stop  = dummy_service_stop,
	.rso_fini  = dummy_service_fini
};

static int dummy_service_allocate(struct m0_reqh_service **service,
				  const struct m0_reqh_service_type *stype)
{
	struct m0_reqh_service *svc;

	M0_PRE(stype != NULL && service != NULL);

	M0_ALLOC_PTR(svc);
	M0_UB_ASSERT(svc != NULL);

	svc->rs_type = stype;
	svc->rs_ops = &dummy_service_ops;
	*service = svc;

	return 0;
}

static const struct m0_reqh_service_type_ops _stype_ops = {
	.rsto_service_allocate = dummy_service_allocate
};

struct m0_reqh_service_type ub_fom_stype = {
	.rst_name  = "ub-fom-service",
	.rst_ops   = &_stype_ops,
	.rst_level = M0_RS_LEVEL_NORMAL,
};

/* ----------------------------------------------------------------
 * FOM operations
 * ---------------------------------------------------------------- */

static void ub_fom_fini(struct m0_fom *fom)
{
	struct ub_fom *m = container_of(fom, struct ub_fom, uf_gen);

	m0_long_lock_link_fini(&m->uf_link);
	m0_fom_fini(fom);
	m0_free(m);
}

static int ub_fom_tick(struct m0_fom *fom)
{
	struct ub_fom *m = container_of(fom, struct ub_fom, uf_gen);

	IS_IN_ARRAY(m->uf_test, ticks);
	return ticks[m->uf_test](fom);
}

static size_t ub_fom_home_locality(const struct m0_fom *fom)
{
	static size_t locality = 0;

	M0_PRE(fom != NULL);
	return locality++;
}

static const struct m0_fom_ops ub_fom_ops = {
	.fo_fini          = ub_fom_fini,
	.fo_tick          = ub_fom_tick,
	.fo_home_locality = ub_fom_home_locality
};

static struct m0_fom_type ub_fom_type;

static const struct m0_fom_type_ops ub_fom_type_ops = {
	.fto_create = NULL
};

static void ub_fom_create(struct m0_fom **out, struct m0_reqh *reqh,
			  size_t seqn, enum scenario test)
{
	struct ub_fom *m;

	M0_ALLOC_PTR(m);
	M0_UB_ASSERT(m != NULL);

	*out = &m->uf_gen;
	m0_fom_init(*out, &ub_fom_type, &ub_fom_ops, NULL, NULL, reqh);

	m->uf_seqn = seqn;
	m->uf_test = test;
	m0_long_lock_link_init(&m->uf_link, *out, NULL);
}

static void
reqh_fom_add(struct m0_reqh *reqh, size_t seqn, enum scenario test)
{
	struct m0_fom *fom;

	ub_fom_create(&fom, reqh, seqn, test);

	m0_rwlock_read_lock(&reqh->rh_rwlock);
        M0_PRE(m0_reqh_state_get(reqh) == M0_REQH_ST_NORMAL);
	m0_fom_queue(fom);
	m0_rwlock_read_unlock(&reqh->rh_rwlock);
}

/** Creates dummy FOMs and adds them to request handler's queue. */
static void reqh_test(struct m0_reqh *reqh, enum scenario test)
{
	size_t seqn;

	M0_LOG(M0_DEBUG, "scenario #%u", test);

	for (seqn = 0; seqn < ST_NR_FOMS; ++seqn)
		reqh_fom_add(reqh, seqn, test);

	m0_reqh_idle_wait(reqh);
}

#define _UB_ROUND_DEFINE(name, test) \
static void name(int iter)           \
{                                    \
	reqh_test(&g_reqh, test);    \
}                                    \
struct __ ## name ## _semicolon_catcher

_UB_ROUND_DEFINE(ub_fom_mem_b,         SC_MEM_B);
_UB_ROUND_DEFINE(ub_fom_mem_kb,        SC_MEM_KB);
_UB_ROUND_DEFINE(ub_fom_mem_mb,        SC_MEM_MB);
_UB_ROUND_DEFINE(ub_fom_mutex,         SC_MUTEX);
_UB_ROUND_DEFINE(ub_fom_mutex_per_cpu, SC_MUTEX_PER_CPU);
_UB_ROUND_DEFINE(ub_fom_long_lock,     SC_LONG_LOCK);
#ifndef ENABLE_PROFILER
_UB_ROUND_DEFINE(ub_fom_block,         SC_BLOCK);
#endif

#undef _UB_ROUND_DEFINE

/* ---------------------------------------------------------------- */

static int _init(const char *opts M0_UNUSED)
{
	size_t i;
	int    rc;

	rc = m0_reqh_service_type_register(&ub_fom_stype);
	M0_UB_ASSERT(rc == 0);

	m0_fom_type_init(&ub_fom_type, M0_UB_FOM_OPCODE,
			 &ub_fom_type_ops, &ub_fom_stype, &m0_generic_conf);

	/* This benchmark doesn't need network, database and some other
	 * subsystems for its operation.  Simplistic initialisation
	 * is justified. */
	rc = M0_REQH_INIT(&g_reqh,
			  .rhia_dtm       = (void *)1,
			  .rhia_db        = NULL,
			  .rhia_mdstore   = (void *)1);
	M0_UB_ASSERT(rc == 0);
	m0_reqh_start(&g_reqh);

	rc = m0_reqh_service_allocate(&g_svc, &ub_fom_stype, NULL);
	M0_UB_ASSERT(rc == 0);
	m0_reqh_service_init(g_svc, &g_reqh, NULL);

	rc = m0_reqh_service_start(g_svc);
	M0_UB_ASSERT(rc == 0);

	for (i = 0; i < ARRAY_SIZE(g_mem); ++i)
		g_mem[i] = (char)i; /* dummy values */

	g_mutexes_nr = m0_reqh_nr_localities(&g_reqh);
	M0_ALLOC_ARR(g_mutexes, g_mutexes_nr);
	M0_UB_ASSERT(g_mutexes != NULL);
	for (i = 0; i < g_mutexes_nr; ++i)
		m0_mutex_init(&g_mutexes[i]);

	m0_long_lock_init(&g_long_lock);
	return 0;
}

static void _fini(void)
{
	size_t i;

	m0_long_lock_fini(&g_long_lock);
	for (i = 0; i < g_mutexes_nr; ++i)
		m0_mutex_fini(&g_mutexes[i]);
	m0_free(g_mutexes);

	m0_reqh_service_prepare_to_stop(g_svc);
	m0_reqh_shutdown_wait(&g_reqh);
	m0_reqh_service_stop(g_svc);
	m0_reqh_service_fini(g_svc);
	m0_reqh_services_terminate(&g_reqh);
	m0_reqh_fini(&g_reqh);
	m0_reqh_service_type_unregister(&ub_fom_stype);
}

struct m0_ub_set m0_fom_ub = {
	.us_name = "fom-ub",
	.us_init = _init,
	.us_fini = _fini,
	.us_run  = {
		{ .ub_name  = "mem-bytes",
		  .ub_iter  = 1,
		  .ub_round = ub_fom_mem_b },
		{ .ub_name  = "mem-KB",
		  .ub_iter  = 1,
		  .ub_round = ub_fom_mem_kb },
		{ .ub_name  = "mem-MB",
		  .ub_iter  = 1,
		  .ub_round = ub_fom_mem_mb },
		{ .ub_name  = "shared-mutex",
		  .ub_iter  = 1,
		  .ub_round = ub_fom_mutex },
		{ .ub_name  = "mutex-per-locality",
		  .ub_iter  = 1,
		  .ub_round = ub_fom_mutex_per_cpu },
		{ .ub_name  = "long-lock",
		  .ub_iter  = 1,
		  .ub_round = ub_fom_long_lock },
#ifndef ENABLE_PROFILER
		{ .ub_name  = "block",
		  .ub_iter  = 1,
		  .ub_round = ub_fom_block },
#endif
		{ .ub_name = NULL}
	}
};

#undef M0_TRACE_SUBSYSTEM
