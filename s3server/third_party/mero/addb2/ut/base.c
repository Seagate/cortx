/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 29-Jan-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT

#include "lib/trace.h"
#include "ut/ut.h"
#include "addb2/addb2.h"

#include "addb2/ut/common.h"

static const struct m0_addb2_mach_ops null_ops = {
};

/**
 * "init-fini" test.
 *
 * Smoke test: initialise and finalise addb2 machine.
 */
static void init_fini(void)
{
	struct m0_addb2_mach *m;

	m = m0_addb2_mach_init(&null_ops, NULL);
	M0_UT_ASSERT(m != NULL);
	m0_addb2_mach_fini(m);
}

enum {
	SKIPME = 0xbebebebebe4ebebe,
	END    = 0xbcbcbcbcbcbcb7bc
};

static bool trace_eq(const struct m0_addb2_trace *t0,
		     const struct m0_addb2_trace *t1)
{
	return t0->tr_nr == t1->tr_nr &&
		m0_forall(i, t1->tr_nr,
			  t0->tr_body[i] == t1->tr_body[i] ||
			  t0->tr_body[i] == SKIPME);
}

enum {
	LABEL_ID_0 = 0x17,
};

const uint64_t payload[] = {
	0x7472756374206d30,
	0x5f61646462325f74,
	0x726163652073686f,
	0x756c646265203d20,
	0x7b0a09092e74725f,
	0x6e72203d20332c0a,
	0x09092e74725f626f,
	0x6479203d20287569,
	0x3131303030303030,
	0x303030303030207c,
	0x204c4142454c5f49,
	0x445f302c202f2a20,
	0x505553482031372c,
	0x2031202a2f0a0909,
	0x094c4142454c5f56,
	0x414c55455f302c20,
	0x2020202020202020,
	0x2020202020202020,
	0x2f2a207061796c6f,
	0x6164202a2f0a0909,
	0x0930783066303030,
	0x3030303030303030
};

static struct m0_addb2_trace *shouldbe;

static int check_submit(const struct m0_addb2_mach *mach,
			struct m0_addb2_trace *trace)
{
	int nr;

	for (nr = 0; shouldbe->tr_body[nr] != END; ++nr)
		;
	shouldbe->tr_nr = nr;
	M0_UT_ASSERT(trace_eq(shouldbe, trace));
	M0_UT_ASSERT(submitted == 0);
	++ submitted;
	return 0;
}

/**
 * "push-pop" test.
 *
 * Push one label, pop it; check that the generated trace is valid.
 */
static void push_pop(void)
{
	struct m0_addb2_mach *m;

	shouldbe = &(struct m0_addb2_trace) {
		.tr_body = (uint64_t[]){
			0x1100000000000000 | LABEL_ID_0, /* PUSH 17, 1 */
			SKIPME,                          /* time-stamp */
			payload[0],                      /* payload */
			0x0f00000000000000,              /* POP */
			SKIPME,                          /* time-stamp */
			END
		}
	};

	m = mach_set(&check_submit);
	m0_addb2_push(LABEL_ID_0, 1, payload);
	m0_addb2_pop(LABEL_ID_0);
	mach_put(m);
}

/**
 * "push0-pop" test.
 *
 * Push a label with empty payload, pop it; check that the generated trace is
 * valid.
 */
static void push0_pop(void)
{
	struct m0_addb2_mach *m;

	shouldbe = &(struct m0_addb2_trace) {
		.tr_body = (uint64_t[]){
			0x1000000000000000 | LABEL_ID_0, /* PUSH 17, 0 */
			SKIPME,                          /* time-stamp */
			0x0f00000000000000,              /* POP */
			SKIPME,                          /* time-stamp */
			END
		}
	};

	m = mach_set(&check_submit);
	m0_addb2_push(LABEL_ID_0, 0, NULL);
	m0_addb2_pop(LABEL_ID_0);
	mach_put(m);
}

/**
 * "push5-pop" test.
 *
 * Push a label, pop it, check that the generated trace is valid.
 */
static void push5_pop(void)
{
	struct m0_addb2_mach *m;

	shouldbe = &(struct m0_addb2_trace) {
		.tr_body = (uint64_t[]){
			0x1500000000000000 | LABEL_ID_0, /* PUSH 17, 5 */
			SKIPME,                          /* time-stamp */
			payload[3],
			payload[4],
			payload[5],
			payload[6],
			payload[7],
			0x0f00000000000000,               /* POP */
			SKIPME,                           /* time-stamp */
			END
		}
	};

	m = mach_set(&check_submit);
	m0_addb2_push(LABEL_ID_0, 5, payload + 3);
	m0_addb2_pop(LABEL_ID_0);
	mach_put(m);
}

/**
 * "push^N-pop^N" test.
 *
 * Push a few labels, pop them all, check that the generated trace is valid.
 */
static void pushN_popN(void)
{
	struct m0_addb2_mach *m;

	shouldbe = &(struct m0_addb2_trace) {
		.tr_body = (uint64_t[]){
			0x1100000000000000 | (LABEL_ID_0 + 0), /* PUSH 17, 5 */
			SKIPME,                                /* time-stamp */
			payload[2],
			0x1000000000000000 | (LABEL_ID_0 + 2), /* PUSH 19, 0 */
			SKIPME,                                /* time-stamp */
			0x1300000000000000 | (LABEL_ID_0 + 3), /* PUSH 1a, 3 */
			SKIPME,                                /* time-stamp */
			payload[0],
			payload[1],
			payload[2],
			0x0f00000000000000,                    /* POP */
			SKIPME,                                /* time-stamp */
			0x0f00000000000000,                    /* POP */
			SKIPME,                                /* time-stamp */
			0x0f00000000000000,                    /* POP */
			SKIPME,                                /* time-stamp */
			END
		}
	};

	m = mach_set(&check_submit);
	m0_addb2_push(LABEL_ID_0 + 0, 1, payload + 2);
	m0_addb2_push(LABEL_ID_0 + 2, 0, NULL);
	m0_addb2_push(LABEL_ID_0 + 3, 3, payload);
	m0_addb2_pop(LABEL_ID_0 + 3);
	m0_addb2_pop(LABEL_ID_0 + 2);
	m0_addb2_pop(LABEL_ID_0 + 0);
	mach_put(m);
}

/**
 * "add" test: add one record in empty context, check that the generated trace
 * is valid.
 */
static void add(void)
{
	struct m0_addb2_mach *m;

	shouldbe = &(struct m0_addb2_trace) {
		.tr_body = (uint64_t[]){
			0x2100000000000000 | LABEL_ID_0, /* DATA 17, 1 */
			SKIPME,                          /* time-stamp */
			payload[0],                      /* payload */
			END
		}
	};

	m = mach_set(&check_submit);
	m0_addb2_add(LABEL_ID_0, 1, payload);
	mach_put(m);
}

/**
 * "add-var" test: add a number of records with variably-sized payloads in a
 * non-empty context, check that the generated trace is valid.
 */
static void add_var(void)
{
	struct m0_addb2_mach *m;
	int                   i;

	shouldbe = &(struct m0_addb2_trace) {
		.tr_body = (uint64_t[]){
			0x1100000000000000 | (LABEL_ID_0 + 0), /* PUSH 17, 1 */
			SKIPME,                                /* time-stamp */
			payload[0],
			0x1100000000000000 | (LABEL_ID_0 + 1), /* PUSH 18, 1 */
			SKIPME,                                /* time-stamp */
			payload[1],
			0x2000000000000000 | (LABEL_ID_0 + 0), /* DATA 17, 0 */
			SKIPME,                                /* time-stamp */
			0x2100000000000000 | (LABEL_ID_0 + 1), /* DATA 18, 1 */
			SKIPME,                                /* time-stamp */
			payload[1],
			0x2200000000000000 | (LABEL_ID_0 + 2), /* DATA 19, 2 */
			SKIPME,                                /* time-stamp */
			payload[2],
			payload[3],
			0x2300000000000000 | (LABEL_ID_0 + 3), /* DATA 1a, 3 */
			SKIPME,                                /* time-stamp */
			payload[3],
			payload[4],
			payload[5],
			0x0f00000000000000,                    /* POP */
			SKIPME,                                /* time-stamp */
			0x0f00000000000000,                    /* POP */
			SKIPME,                                /* time-stamp */
			END
		}
	};

	m = mach_set(&check_submit);
	m0_addb2_push(LABEL_ID_0 + 0, 1, payload);
	m0_addb2_push(LABEL_ID_0 + 1, 1, payload + 1);
	for (i = 0; i < 4; ++i)
		m0_addb2_add(LABEL_ID_0 + i, i, payload + i);
	m0_addb2_pop(LABEL_ID_0 + 1);
	m0_addb2_pop(LABEL_ID_0 + 0);
	mach_put(m);
}

static int  added;
static bool enough;
static int  total;
static int  keep;
static int  used;
static const struct m0_addb2_trace *busy[1000];

static int full_submit(const struct m0_addb2_mach *mach,
		       struct m0_addb2_trace *trace)
{
	int i;

	++ submitted;
	M0_UT_ASSERT(trace->tr_nr % 3 == 0);
	for (i = 0; i < trace->tr_nr; i += 3) {
		M0_UT_ASSERT(trace->tr_body[i] ==
			     (0x2100000000000000 | (total + i / 3)));
		/* trace->tr_body[i + 1] is the time-stamp. */
		M0_UT_ASSERT(trace->tr_body[i + 2] == payload[0]);
	}
	total += trace->tr_nr / 3;
	M0_UT_ASSERT(total == added);
	enough = (submitted > 10);
	if (keep) {
		M0_UT_ASSERT(used < ARRAY_SIZE(busy));
		busy[used++] = trace;
	}
	return keep;
}

/**
 * "full" test: add records with variable identifiers until a number of trace
 * buffers is fully occupied. Check the traces.
 */
static void full(void)
{
	struct m0_addb2_mach *m;

	m = mach_set(&full_submit);
	total = 0;
	keep  = 0;
	for (enough = false, added = 0; !enough; ++added)
		m0_addb2_add(added, 1, payload);
	mach_put(m);
}

static bool idled = false;

void idle_idle(const struct m0_addb2_mach *mach)
{
	M0_UT_ASSERT(used == 0);
	idled = true;
}

/**
 * "stop-idle" test: check that m0_addb2_stop() works correctly.
 */
static void stop_idle(void)
{
	struct m0_addb2_mach *m;

	idle = &idle_idle;
	m = mach_set(&full_submit);
	total = 0;
	keep  = +1;
	used  = 0;
	for (enough = false, added = 0; !enough; ++added)
		m0_addb2_add(added, 1, payload);
	M0_UT_ASSERT(used > 0);
	m0_addb2_mach_stop(m);
	while (used > 0)
		m0_addb2_trace_done(busy[--used]);
	M0_UT_ASSERT(idled);
	mach_put(m);
}

static uint64_t found;
static unsigned depth;

static int sensor_submit(const struct m0_addb2_mach *mach,
			 struct m0_addb2_trace *trace)
{
	int i;

	++ submitted;
	for (i = 0; i < depth; ++ i)
		M0_UT_ASSERT(trace->tr_body[2 * i] == /* PUSH 17 + i, 0 */
			     (0x1000000000000000 | (LABEL_ID_0 + i)));
	M0_UT_ASSERT(trace->tr_body[2 * i] !=         /* PUSH 17 + i, 0 */
		     (0x1000000000000000 | (LABEL_ID_0 + i)));
	for (; i < trace->tr_nr; ++ i) {
		if (trace->tr_body[i] == SENSOR_MARKER)
			found = trace->tr_body[i + 1];
	}
	return 0;
}

/**
 * "sensor-depth" test: create a depth context, add a sensor in it, force trace
 * buffer to completion. Check that the context and sensor are reprodced in the
 * next trace buffer.
 */
static void sensor_depth(void)
{
	struct m0_addb2_mach  *m;
	struct m0_addb2_sensor s;

	seq = 1;
	sensor_finalised = false;
	found = 0;
	submitted = 0;

	M0_SET0(&s);
	m = mach_set(&sensor_submit);
	for (depth = 0; depth < M0_ADDB2_LABEL_MAX; ++depth)
		m0_addb2_push(LABEL_ID_0 + depth, 0, NULL);
	m0_addb2_sensor_add(&s, LABEL_ID_0 + 5, 2, -1, &sensor_ops);
	fill_one(m);

	while (depth > 0) {
		m0_addb2_pop(LABEL_ID_0 + depth - 1);
		M0_UT_ASSERT(sensor_finalised);

		fill_one(m);
		-- depth;
		fill_one(m);
	}
	mach_put(m);
	M0_UT_ASSERT(found == seq);
}

struct m0_ut_suite addb2_base_ut = {
	.ts_name = "addb2-base",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "init-fini",     &init_fini },
		{ "push-pop",      &push_pop },
		{ "push0-pop",     &push0_pop },
		{ "push5-pop",     &push5_pop },
		{ "push^N-pop^N",  &pushN_popN },
		{ "add",           &add },
		{ "add-var",       &add_var },
		{ "full",          &full },
		{ "stop-idle",     &stop_idle },
		{ "sensor-depth",  &sensor_depth },
		{ NULL, NULL }
	}
};

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
