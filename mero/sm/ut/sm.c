/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 28-Oct-2011
 */

#include "ut/ut.h"
#include "lib/ub.h"
#include "lib/time.h"
#include "lib/errno.h"
#include "lib/arith.h"                    /* m0_rnd */
#include "lib/misc.h"                     /* M0_IN */
#include "lib/thread.h"

#include "sm/sm.h"

static struct m0_sm_group G;
static struct m0_sm       m;
static struct m0_sm_ast   ast;
static bool               more = true;
static struct m0_thread   ath;

static void ast_thread(int __d)
{
	while (more) {
		m0_chan_wait(&G.s_clink);
		m0_sm_group_lock(&G);
		m0_sm_asts_run(&G);
		m0_sm_group_unlock(&G);
	}
}

static int init(void) {
	m0_sm_group_init(&G);
	return 0;
}

static int fini(void) {
	more = false;
	m0_clink_signal(&G.s_clink);
	m0_thread_join(&ath);
	m0_sm_group_fini(&G);
	return 0;
}

/**
   Unit test for m0_sm_state_set().

   Performs a state transition for a very simple state machine:
   @dot
   digraph M {
           S_INITIAL -> S_TERMINAL
	   S_INITIAL -> S_FAILURE
	   S_FAILURE -> S_TERMINAL
   }
   @enddot
 */
static void transition(void)
{
	enum { S_INITIAL, S_TERMINAL, S_FAILURE, S_NR };
	struct m0_sm_state_descr states[S_NR] = {
		[S_INITIAL] = {
			.sd_flags     = M0_SDF_INITIAL,
			.sd_name      = "initial",
			.sd_in        = NULL,
			.sd_ex        = NULL,
			.sd_invariant = NULL,
			.sd_allowed   = M0_BITS(S_TERMINAL, S_FAILURE)
		},
		[S_FAILURE] = {
			.sd_flags     = M0_SDF_FAILURE,
			.sd_name      = "initial",
			.sd_in        = NULL,
			.sd_ex        = NULL,
			.sd_invariant = NULL,
			.sd_allowed   = M0_BITS(S_TERMINAL)
		},
		[S_TERMINAL] = {
			.sd_flags     = M0_SDF_TERMINAL,
			.sd_name      = "terminal",
			.sd_in        = NULL,
			.sd_ex        = NULL,
			.sd_invariant = NULL,
			.sd_allowed   = 0
		}
	};
	struct m0_sm_trans_descr trans[] = {
		{ "Step", S_INITIAL, S_TERMINAL },
		{ "Fail", S_INITIAL, S_FAILURE },
		{ "Fini", S_FAILURE, S_TERMINAL },
	};
	struct m0_sm_conf        conf = {
		.scf_name      = "test drive: transition",
		.scf_nr_states = S_NR,
		.scf_state     = states,
		.scf_trans_nr  = ARRAY_SIZE(trans),
		.scf_trans     = trans
	};

	m0_sm_conf_init(&conf);
	M0_UT_ASSERT(states[S_INITIAL].sd_trans[S_INITIAL] == ~0);
	M0_UT_ASSERT(states[S_INITIAL].sd_trans[S_TERMINAL] == 0);
	M0_UT_ASSERT(states[S_INITIAL].sd_trans[S_FAILURE] == 1);
	M0_UT_ASSERT(states[S_FAILURE].sd_trans[S_INITIAL] == ~0);
	M0_UT_ASSERT(states[S_FAILURE].sd_trans[S_FAILURE] == ~0);
	M0_UT_ASSERT(states[S_FAILURE].sd_trans[S_TERMINAL] == 2);
	M0_UT_ASSERT(states[S_TERMINAL].sd_trans[S_INITIAL] == ~0);
	M0_UT_ASSERT(states[S_TERMINAL].sd_trans[S_FAILURE] == ~0);
	M0_UT_ASSERT(states[S_TERMINAL].sd_trans[S_TERMINAL] == ~0);

	m0_sm_group_lock(&G);
	M0_SET0(&m);
	m0_sm_init(&m, &conf, S_INITIAL, &G);
	M0_UT_ASSERT(m.sm_state == S_INITIAL);
	m0_sm_state_set(&m, S_TERMINAL);
	M0_UT_ASSERT(m.sm_state == S_TERMINAL);
	m0_sm_fini(&m);

	M0_SET0(&m);
	m0_sm_init(&m, &conf, S_INITIAL, &G);
	M0_UT_ASSERT(m.sm_state == S_INITIAL);
	m0_sm_move(&m, 0, S_TERMINAL);
	M0_UT_ASSERT(m.sm_state == S_TERMINAL);
	m0_sm_fini(&m);

	M0_SET0(&m);
	m0_sm_init(&m, &conf, S_INITIAL, &G);
	M0_UT_ASSERT(m.sm_state == S_INITIAL);
	m0_sm_move(&m, -EINVAL, S_FAILURE);
	M0_UT_ASSERT(m.sm_state == S_FAILURE);
	M0_UT_ASSERT(m.sm_rc == -EINVAL);
	m0_sm_state_set(&m, S_TERMINAL);
	m0_sm_fini(&m);

	m0_sm_group_unlock(&G);
}

static bool x;

static void ast_cb(struct m0_sm_group *g, struct m0_sm_ast *a)
{
	M0_UT_ASSERT(g == &G);
	M0_UT_ASSERT(a == &ast);
	M0_UT_ASSERT(a->sa_datum == &ast_cb);
	x = true;
}

/**
   Unit test for m0_sm_ast_post().
 */
static void ast_test(void)
{

	x = false;
	ast.sa_cb = &ast_cb;
	ast.sa_datum = &ast_cb;
	m0_sm_ast_post(&G, &ast);
	M0_UT_ASSERT(!x);
	m0_sm_group_lock(&G);
	M0_UT_ASSERT(x);
	m0_sm_group_unlock(&G);

	/* test ast cancellation. */
	/* cancel before execution. */
	x = false;
	m0_sm_group_lock(&G);
	m0_sm_ast_post(&G, &ast);
	M0_UT_ASSERT(!x);
	M0_UT_ASSERT(ast.sa_next != NULL);
	m0_sm_ast_cancel(&G, &ast);
	M0_UT_ASSERT(ast.sa_next == NULL);
	m0_sm_group_unlock(&G);
	M0_UT_ASSERT(!x);

	/* cancel after execution. */
	x = false;
	m0_sm_ast_post(&G, &ast);
	M0_UT_ASSERT(!x);
	M0_UT_ASSERT(ast.sa_next != NULL);
	m0_sm_group_lock(&G);
	M0_UT_ASSERT(x);
	M0_UT_ASSERT(ast.sa_next == NULL);
	m0_sm_ast_cancel(&G, &ast);
	m0_sm_group_unlock(&G);
	M0_UT_ASSERT(ast.sa_next == NULL);
	x = false;
	m0_sm_group_lock(&G);
	M0_UT_ASSERT(!x);
	m0_sm_group_unlock(&G);
}

/**
   Unit test for m0_sm_timeout_arm().

   @dot
   digraph M {
           S_INITIAL -> S_0 [label="timeout t0"]
           S_0 -> S_1 [label="timeout t1"]
           S_0 -> S_2
           S_1 -> S_TERMINAL
           S_2 -> S_TERMINAL
   }
   @enddot
 */
enum { S_TMO_INITIAL, S_TMO_0, S_TMO_1, S_TMO_2, S_TMO_TERMINAL, S_TMO_NR };

static struct m0_sm_state_descr tmo_states[S_TMO_NR] = {
	[S_TMO_INITIAL] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "initial",
		.sd_in        = NULL,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(S_TMO_0)
	},
	[S_TMO_0] = {
		.sd_flags     = 0,
		.sd_name      = "0",
		.sd_in        = NULL,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(S_TMO_1, S_TMO_2)
	},
	[S_TMO_1] = {
		.sd_flags     = 0,
		.sd_name      = "0",
		.sd_in        = NULL,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(S_TMO_2, S_TMO_TERMINAL)
	},
	[S_TMO_2] = {
		.sd_flags     = 0,
		.sd_name      = "0",
		.sd_in        = NULL,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(S_TMO_0, S_TMO_TERMINAL)
	},
	[S_TMO_TERMINAL] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "terminal",
		.sd_in        = NULL,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = 0
	}
};

static const struct m0_sm_conf tmo_sm_conf = {
	.scf_name      = "test drive: timeout",
	.scf_nr_states = S_TMO_NR,
	.scf_state     = tmo_states
};

static void timeout(void)
{
	struct m0_sm_timeout t0;
	struct m0_sm_timeout t1;
	const long           delta = M0_TIME_ONE_SECOND/100;
	int                  result;

	result = M0_THREAD_INIT(&ath, int, NULL, &ast_thread, 0, "ast_thread");
	M0_UT_ASSERT(result == 0);

	m0_sm_group_lock(&G);
	M0_SET0(&m);
	m0_sm_init(&m, &tmo_sm_conf, S_TMO_INITIAL, &G);

	/* check that timeout initialisation and finalisation work. */
	m0_sm_timeout_init(&t1);
	m0_sm_timeout_fini(&t1);

	m0_sm_timeout_init(&t0);

	/* check that timeout works */
	result = m0_sm_timeout_arm(&m, &t0, m0_time_from_now(0, delta),
				   S_TMO_0, 0);
	M0_UT_ASSERT(result == 0);

	result = m0_sm_timedwait(&m, ~M0_BITS(S_TMO_INITIAL), M0_TIME_NEVER);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(m.sm_state == S_TMO_0);

	m0_sm_timeout_fini(&t0);
	m0_sm_timeout_init(&t1);

	/* check that state transition cancels the timeout */
	result = m0_sm_timeout_arm(&m, &t1, m0_time_from_now(0, delta),
				   S_TMO_1, 0);
	M0_UT_ASSERT(result == 0);

	m0_sm_state_set(&m, S_TMO_2);
	M0_UT_ASSERT(m.sm_state == S_TMO_2);

	result = m0_sm_timedwait(&m, ~M0_BITS(S_TMO_2), m0_time_from_now(0,
								   2 * delta));
	M0_UT_ASSERT(result == -ETIMEDOUT);
	M0_UT_ASSERT(m.sm_state == S_TMO_2);

	m0_sm_timeout_fini(&t1);
	m0_sm_timeout_init(&t1);

	/* check that timeout with a bitmask is not cancelled by a state
	   transition */
	m0_sm_state_set(&m, S_TMO_0);
	result = m0_sm_timeout_arm(&m, &t1, m0_time_from_now(0, delta), S_TMO_2,
				   M0_BITS(S_TMO_1));
	M0_UT_ASSERT(result == 0);

	m0_sm_state_set(&m, S_TMO_1);
	M0_UT_ASSERT(m.sm_state == S_TMO_1);

	result = m0_sm_timedwait(&m, M0_BITS(S_TMO_2), M0_TIME_NEVER);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(m.sm_state == S_TMO_2);

	m0_sm_timeout_fini(&t1);

	m0_sm_state_set(&m, S_TMO_TERMINAL);
	M0_UT_ASSERT(m.sm_state == S_TMO_TERMINAL);

	m0_sm_fini(&m);
	m0_sm_group_unlock(&G);
}

struct story {
	struct m0_sm cain;
	struct m0_sm abel;
};

enum { S_INITIAL, S_ITERATE, S_FRATRICIDE, S_TERMINAL, S_NR };

static int genesis_4_8(struct m0_sm *mach)
{
	struct story *s;

	s = container_of(mach, struct story, cain);
	m0_sm_fail(&s->abel, S_TERMINAL, -EINTR);
	return M0_SM_BREAK;
}

/**
   Unit test for multiple machines in the group.

   @dot
   digraph M {
           S_INITIAL -> S_ITERATE
           S_ITERATE -> S_ITERATE
           S_INITIAL -> S_FRATRICIDE [label="timeout"]
           S_ITERATE -> S_TERMINAL
           S_FRATRICIDE -> S_TERMINAL
   }
   @enddot
 */
static void group(void)
{
	struct m0_sm_state_descr states[S_NR] = {
		[S_INITIAL] = {
			.sd_flags     = M0_SDF_INITIAL,
			.sd_name      = "initial",
			.sd_in        = NULL,
			.sd_ex        = NULL,
			.sd_invariant = NULL,
			.sd_allowed   = M0_BITS(S_ITERATE, S_FRATRICIDE)
		},
		[S_ITERATE] = {
			.sd_flags     = 0,
			.sd_name      = "loop here",
			.sd_in        = NULL,
			.sd_ex        = NULL,
			.sd_invariant = NULL,
			.sd_allowed   = M0_BITS(S_ITERATE, S_TERMINAL)
		},
		[S_FRATRICIDE] = {
			.sd_flags     = 0,
			.sd_name      = "Let's go out to the field",
			.sd_in        = &genesis_4_8,
			.sd_ex        = NULL,
			.sd_invariant = NULL,
			.sd_allowed   = M0_BITS(S_TERMINAL)
		},
		[S_TERMINAL] = {
			.sd_flags     = M0_SDF_TERMINAL|M0_SDF_FAILURE,
			.sd_name      = "terminal",
			.sd_in        = NULL,
			.sd_ex        = NULL,
			.sd_invariant = NULL,
			.sd_allowed   = 0
		}
	};
	const struct m0_sm_conf        conf = {
		.scf_name      = "test drive: group",
		.scf_nr_states = S_NR,
		.scf_state     = states
	};

	struct story         s = {};
	struct m0_sm_timeout to;
	int                  result;

	m0_sm_group_lock(&G);
	m0_sm_init(&s.cain, &conf, S_INITIAL, &G);
	m0_sm_init(&s.abel, &conf, S_INITIAL, &G);

	/* check that timeout works */
	m0_sm_timeout_init(&to);
	result = m0_sm_timeout_arm(&s.cain, &to,
				   m0_time_from_now(0, M0_TIME_ONE_SECOND/100),
				   S_FRATRICIDE, 0);
	M0_UT_ASSERT(result == 0);

	while (s.abel.sm_rc == 0) {
		/* live, while you can */
		m0_sm_state_set(&s.abel, S_ITERATE);
		/* give providence a chance to run */
		m0_sm_asts_run(&G);
	}
	M0_UT_ASSERT(s.abel.sm_state == S_TERMINAL);
	M0_UT_ASSERT(s.cain.sm_state == S_FRATRICIDE);

	m0_sm_fail(&s.cain, S_TERMINAL, -1);

	m0_sm_timeout_fini(&to);

	m0_sm_fini(&s.abel);
	m0_sm_fini(&s.cain);
	m0_sm_group_unlock(&G);
}

enum { C_INIT, C_FLIP, C_HEAD, C_TAIL, C_DONE, C_OVER, C_WIN, C_LOSE, C_TIE,
       C_NR };

static int heads = 0;
static int tails = 0;

static int flip(struct m0_sm *mach)
{
	uint64_t cookie = ~heads + 11 * tails + 42;

	return m0_rnd(10, &cookie) >= 5 ? C_HEAD : C_TAIL;
}

static int head(struct m0_sm *mach)
{
	++heads;
	return C_DONE;
}

static int tail(struct m0_sm *mach)
{
	++tails;
	return C_DONE;
}

static int over(struct m0_sm *mach)
{
	if (tails > heads) {
		return C_WIN;
	} else if (tails == heads) {
		return C_TIE;
	} else {
		/* transit to a failure state */
		mach->sm_rc = heads - tails;
		return C_LOSE;
	}
}

/**
 * Unit test for chained state transition incurred by ->sd_in() call-back.
 *
 * @dot
 * digraph M {
 *         C_INIT -> C_FLIP
 *         C_FLIP -> C_HEAD
 *         C_FLIP -> C_TAIL
 *         C_HEAD -> C_DONE
 *         C_TAIL -> C_DONE
 *         C_DONE -> C_FLIP
 *         C_DONE -> C_OVER
 *         C_OVER -> C_LOSE
 *         C_OVER -> C_WIN
 *         C_OVER -> C_TIE
 * }
 *  @enddot
 */

static struct m0_sm_state_descr states[C_NR] = {
	[C_INIT] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "initial",
		.sd_in        = NULL,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(C_FLIP)
	},
	[C_FLIP] = {
		.sd_flags     = 0,
		.sd_name      = "flip a coin",
		.sd_in        = flip,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(C_HEAD, C_TAIL)
	},
	[C_HEAD] = {
		.sd_flags     = 0,
		.sd_name      = "Head, I win!",
		.sd_in        = head,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(C_DONE)
	},
	[C_TAIL] = {
		.sd_flags     = 0,
		.sd_name      = "Tail, you lose!",
		.sd_in        = tail,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(C_DONE)
	},
	[C_DONE] = {
		.sd_flags     = 0,
		.sd_name      = "round done",
		.sd_in        = NULL,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(C_OVER, C_FLIP)
	},
	[C_OVER] = {
		.sd_flags     = 0,
		.sd_name      = "game over",
		.sd_in        = over,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = M0_BITS(C_WIN, C_LOSE, C_TIE)
	},
	[C_WIN] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "tails win",
		.sd_in        = NULL,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = 0
	},
	[C_LOSE] = {
		.sd_flags     = M0_SDF_TERMINAL|M0_SDF_FAILURE,
		.sd_name      = "tails lose",
		.sd_in        = NULL,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = 0
	},
	[C_TIE] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "tie",
		.sd_in        = NULL,
		.sd_ex        = NULL,
		.sd_invariant = NULL,
		.sd_allowed   = 0
	},
};

enum { NR_RUNS = 20 };

static const struct m0_sm_conf conf = {
	.scf_name      = "test drive: chain",
	.scf_nr_states = C_NR,
	.scf_state     = states
};

static void chain(void)
{
	int i;

	m0_sm_group_lock(&G);
	M0_SET0(&m);
	m0_sm_init(&m, &conf, C_INIT, &G);
	M0_UT_ASSERT(m.sm_state == C_INIT);

	for (i = 0; i < NR_RUNS; ++i) {
		m0_sm_state_set(&m, C_FLIP);
		M0_UT_ASSERT(m.sm_state == C_DONE);
	}
	M0_UT_ASSERT(tails + heads == NR_RUNS);
	m0_sm_state_set(&m, C_OVER);
	M0_UT_ASSERT(M0_IN(m.sm_state, (C_WIN, C_LOSE, C_TIE)));

	m0_sm_fini(&m);
	m0_sm_group_unlock(&G);
}

static struct m0_sm_ast_wait wait;
static struct m0_mutex       wait_guard;

static void ast_wait_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	x = true;
	m0_mutex_lock(&wait_guard);
	m0_sm_ast_wait_signal(&wait);
	m0_mutex_unlock(&wait_guard);
}

static void ast_wait(void)
{
	m0_mutex_init(&wait_guard);
	m0_sm_ast_wait_init(&wait, &wait_guard);
	x = false;
	ast.sa_cb = &ast_wait_cb;
	m0_mutex_lock(&wait_guard);
	m0_sm_ast_wait_post(&wait, &G, &ast);
	m0_sm_ast_wait(&wait);
	M0_UT_ASSERT(x);

	m0_sm_ast_wait_fini(&wait);
	m0_mutex_unlock(&wait_guard);
	m0_mutex_fini(&wait_guard);
}

struct m0_ut_suite sm_ut = {
	.ts_name = "sm-ut",
	.ts_init = init,
	.ts_fini = fini,
	.ts_tests = {
		{ "transition", transition },
		{ "ast",        ast_test },
		{ "timeout",    timeout },
		{ "group",      group },
		{ "chain",      chain },
		{ "wait",       ast_wait },
		{ NULL, NULL }
	}
};
M0_EXPORTED(sm_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
