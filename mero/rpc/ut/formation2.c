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
 * Original author: Amit Jambure <amit_jambure@xyratex.com>
 * Original creation date: 05/25/2012
 */

#include "ut/ut.h"
#include "lib/mutex.h"
#include "lib/time.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/finject.h"
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"
#include "sm/sm.h"
#include "rpc/rpc_internal.h"

static struct m0_rpc_frm             *frm;
static struct m0_rpc_frm_constraints  constraints;
static struct m0_rpc_machine          rmachine;
static struct m0_rpc_chan             rchan;
static struct m0_rpc_session          session;
static struct m0_rpc_item_type        twoway_item_type;
static struct m0_rpc_item_type        oneway_item_type;

extern const struct m0_sm_conf outgoing_item_sm_conf;
extern const struct m0_sm_conf incoming_item_sm_conf;

static int frm_ut_init(void)
{
	int rc;

	twoway_item_type.rit_incoming_conf = incoming_item_sm_conf;
	twoway_item_type.rit_outgoing_conf = outgoing_item_sm_conf;
	oneway_item_type.rit_incoming_conf = incoming_item_sm_conf;
	oneway_item_type.rit_outgoing_conf = outgoing_item_sm_conf;
	rchan.rc_rpc_machine = &rmachine;
	frm = &rchan.rc_frm;
	rpc_conn_tlist_init(&rmachine.rm_outgoing_conns);
	m0_sm_group_init(&rmachine.rm_sm_grp);
	rmachine.rm_stopping = false;
	rc = M0_THREAD_INIT(&rmachine.rm_worker, struct m0_rpc_machine *,
			    NULL, &rpc_worker_thread_fn, &rmachine,
			    "rpc_worker");
	M0_ASSERT(rc == 0);
	m0_rpc_machine_lock(&rmachine);
	return 0;
}

static int frm_ut_fini(void)
{
	rmachine.rm_stopping = true;
	m0_clink_signal(&rmachine.rm_sm_grp.s_clink);
	m0_rpc_machine_unlock(&rmachine);
	m0_thread_join(&rmachine.rm_worker);
	m0_sm_group_fini(&rmachine.rm_sm_grp);
	rpc_conn_tlist_fini(&rmachine.rm_outgoing_conns);
	return 0;
}

enum { STACK_SIZE = 100 };

static struct m0_rpc_packet *packet_stack[STACK_SIZE];
static int top = 0;

static void packet_stack_push(struct m0_rpc_packet *p)
{
	M0_UT_ASSERT(p != NULL);
	M0_UT_ASSERT(top < STACK_SIZE - 1);
	packet_stack[top] = p;
	++top;
}

static struct m0_rpc_packet *packet_stack_pop(void)
{
	M0_UT_ASSERT(top > 0 && top < STACK_SIZE);
	--top;
	return packet_stack[top];
}

static bool packet_stack_is_empty(void)
{
	return top == 0;
}

static bool packet_ready_called;
static int  item_bind_count;

static void flags_reset(void)
{
	packet_ready_called = false;
	item_bind_count = 0;
}

static int packet_ready(struct m0_rpc_packet *p)
{
	M0_UT_ASSERT(frm_rmachine(p->rp_frm) == &rmachine);
	M0_UT_ASSERT(frm_rchan(p->rp_frm) == &rchan);
	packet_stack_push(p);
	packet_ready_called = true;
	return 0;
}

static struct m0_rpc_frm_ops frm_ops = {
	.fo_packet_ready = packet_ready,
};

static void frm_init_test(void)
{
	m0_rpc_frm_constraints_get_defaults(&constraints);
	m0_rpc_frm_init(frm, &constraints, &frm_ops);
	M0_UT_ASSERT(frm->f_state == FRM_IDLE);
}

static m0_bcount_t twoway_item_size(const struct m0_rpc_item *item)
{
	return 10;
}

static bool twoway_item_try_merge(struct m0_rpc_item *container,
				  struct m0_rpc_item *component,
				  m0_bcount_t         limit)
{
	return false;
}

static void item_get_noop(struct m0_rpc_item *item)
{
	/* Do nothing */
}

static void item_put_noop(struct m0_rpc_item *item)
{
	/* Do nothing */
}

static struct m0_rpc_item_type_ops twoway_item_type_ops = {
	.rito_payload_size = twoway_item_size,
	.rito_try_merge    = twoway_item_try_merge,
	.rito_item_get     = item_get_noop,
	.rito_item_put     = item_put_noop,
};

static struct m0_rpc_item_type twoway_item_type = {
	.rit_flags = M0_RPC_ITEM_TYPE_REQUEST,
	.rit_ops   = &twoway_item_type_ops,
};

static m0_bcount_t oneway_item_size(const struct m0_rpc_item *item)
{
	return 20;
}

static struct m0_rpc_item_type_ops oneway_item_type_ops = {
	.rito_payload_size = oneway_item_size,
	.rito_item_get     = item_get_noop,
	.rito_item_put     = item_put_noop,
};

static struct m0_rpc_item_type oneway_item_type = {
	.rit_flags = M0_RPC_ITEM_TYPE_ONEWAY,
	.rit_ops   = &oneway_item_type_ops,
};

enum {
	TIMEDOUT = 1,
	WAITING  = 2,
	NEVER    = 3,

	NORMAL  = 1,
	ONEWAY  = 2,
};

static m0_time_t timeout; /* nano seconds */
static void set_timeout(uint64_t milli)
{
	timeout = milli * 1000 * 1000;
}

static struct m0_rpc_item *new_item(int deadline, int kind)
{
	struct m0_rpc_item_type *itype;
	struct m0_rpc_item      *item;
	static struct m0_sm_conf sm_conf;

	M0_UT_ASSERT(M0_IN(deadline, (TIMEDOUT, WAITING, NEVER)));
	M0_UT_ASSERT(M0_IN(kind,     (NORMAL, ONEWAY)));

	M0_ALLOC_PTR(item);
	M0_UT_ASSERT(item != NULL);

	itype = kind == ONEWAY ? &oneway_item_type : &twoway_item_type;
	m0_rpc_item_init(item, itype);
	item->ri_rmachine = &rmachine;

	m0_rpc_item_sm_init(item, M0_RPC_ITEM_OUTGOING);
	sm_conf = *item->ri_sm.sm_conf;
	sm_conf.scf_state[M0_RPC_ITEM_SENDING].sd_flags = M0_SDF_FINAL;
	item->ri_sm.sm_conf = &sm_conf;

	switch (deadline) {
	case TIMEDOUT:
		item->ri_deadline = 0;
		break;
	case NEVER:
		item->ri_deadline = M0_TIME_NEVER;
		break;
	case WAITING:
		item->ri_deadline = m0_time_from_now(0, timeout);
		break;
	}
	item->ri_prio = M0_RPC_ITEM_PRIO_MAX;
	item->ri_session = &session;

	return item;
}

static void
check_frm(enum frm_state state, uint64_t nr_items, uint64_t nr_packets)
{
	M0_UT_ASSERT(frm->f_state == state &&
		     frm->f_nr_items == nr_items &&
		     frm->f_nr_packets_enqed == nr_packets);
}

static void check_ready_packet_has_item(struct m0_rpc_item *item)
{
	struct m0_rpc_packet *p;

	p = packet_stack_pop();
	M0_UT_ASSERT(packet_stack_is_empty());
	M0_UT_ASSERT(m0_rpc_packet_is_carrying_item(p, item));
	check_frm(FRM_BUSY, 0, 1);
	m0_rpc_frm_packet_done(p);
	m0_rpc_packet_discard(p);
	check_frm(FRM_IDLE, 0, 0);
}

static void frm_test1(void)
{
	struct m0_rpc_item *item;
	/*
	 * Timedout item triggers immediate formation.
	 * Waiting item do not trigger immediate formation, but they are
	 * formed once deadline is passed.
	 */
	void perform_test(int deadline, int kind)
	{
		set_timeout(100);
		item = new_item(deadline, kind);
		flags_reset();
		m0_rpc_frm_enq_item(frm, item);
		if (deadline == WAITING) {
			int result;

			M0_UT_ASSERT(!packet_ready_called);
			check_frm(FRM_BUSY, 1, 0);
			/* Allow RPC worker to process timeout AST */
			m0_rpc_machine_unlock(&rmachine);
			/*
			 * The original code slept for 2*timeout here. This is
			 * unreliable and led to spurious assertion failures
			 * below. Explicitly wait until the item enters an
			 * appropriate state.
			 */
			result = m0_rpc_item_timedwait(item,
				    M0_BITS(M0_RPC_ITEM_URGENT,
					    M0_RPC_ITEM_SENDING,
					    M0_RPC_ITEM_SENT,
					    M0_RPC_ITEM_WAITING_FOR_REPLY,
					    M0_RPC_ITEM_REPLIED),
				    M0_TIME_NEVER);
			M0_UT_ASSERT(result == 0);
			m0_rpc_machine_lock(&rmachine);
		}
		M0_UT_ASSERT(packet_ready_called);
		check_ready_packet_has_item(item);
		m0_rpc_item_fini(item);
		m0_free(item);
	}
	M0_ENTRY();

	/* Do not let formation trigger because of size limit */
	frm->f_constraints.fc_max_nr_bytes_accumulated = ~0;

	perform_test(TIMEDOUT, NORMAL);
	perform_test(TIMEDOUT, ONEWAY);
	perform_test(WAITING,  NORMAL);
	perform_test(WAITING,  ONEWAY);

	/* Test: item is moved to URGENT state when call to m0_sm_timeout_arm()
	   fails to start item->ri_deadline_timeout in frm_insert().
	 */
	set_timeout(100);
	item = new_item(WAITING, NORMAL);
	flags_reset();
	m0_fi_enable_once("m0_sm_timeout_arm", "failed");
	m0_rpc_frm_enq_item(frm, item);
	M0_UT_ASSERT(packet_ready_called);
	check_ready_packet_has_item(item);
	m0_free(item);

	M0_LEAVE();
}

static void frm_test2(void)
{
	/* formation triggers when accumulated bytes exceed limit */

	void perform_test(int kind)
	{
		enum { N = 4 };
		struct m0_rpc_item   *items[N];
		struct m0_rpc_packet *p;
		int                   i;
		m0_bcount_t           item_size;

		for (i = 0; i < N; ++i)
			items[i] = new_item(WAITING, kind);
		item_size = m0_rpc_item_size(items[0]);
		/* include all ready items */
		frm->f_constraints.fc_max_packet_size = ~0;
		/*
		 * set fc_max_nr_bytes_accumulated such that, formation triggers
		 * when last item from items[] is enqued
		 */
		frm->f_constraints.fc_max_nr_bytes_accumulated =
			(N - 1) * item_size + item_size / 2;

		flags_reset();
		for (i = 0; i < N - 1; ++i) {
			m0_rpc_frm_enq_item(frm, items[i]);
			M0_UT_ASSERT(!packet_ready_called);
			check_frm(FRM_BUSY, i + 1, 0);
		}
		m0_rpc_frm_enq_item(frm, items[N - 1]);
		M0_UT_ASSERT(packet_ready_called);
		check_frm(FRM_BUSY, 0, 1);

		p = packet_stack_pop();
		M0_UT_ASSERT(packet_stack_is_empty());
		for (i = 0; i < N; ++i)
			M0_UT_ASSERT(
				m0_rpc_packet_is_carrying_item(p, items[i]));

		m0_rpc_frm_packet_done(p);
		check_frm(FRM_IDLE, 0, 0);

		m0_rpc_packet_discard(p);
		for (i = 0; i < N; ++i) {
			m0_rpc_item_fini(items[i]);
			m0_free(items[i]);
		}
	}

	M0_ENTRY();

	set_timeout(999);

	perform_test(NORMAL);
	perform_test(ONEWAY);

	M0_LEAVE();
}

static void frm_test3(void)
{
	/* If max_nr_packets_enqed is reached, formation is stopped */
	struct m0_rpc_item   *item;
	uint64_t              saved;

	M0_ENTRY();


	item = new_item(TIMEDOUT, NORMAL);
	saved = frm->f_constraints.fc_max_nr_packets_enqed;
	frm->f_constraints.fc_max_nr_packets_enqed = 0;
	flags_reset();
	m0_rpc_frm_enq_item(frm, item);
	M0_UT_ASSERT(!packet_ready_called);
	check_frm(FRM_BUSY, 1, 0);

	frm->f_constraints.fc_max_nr_packets_enqed = saved;
	m0_rpc_frm_run_formation(frm);
	M0_UT_ASSERT(packet_ready_called);

	check_ready_packet_has_item(item);
	m0_rpc_item_fini(item);
	m0_free(item);

	M0_LEAVE();
}

static void frm_do_test5(const int N, const int ITEMS_PER_PACKET)
{
	/* Multiple packets are formed if ready items don't fit in one packet */
	struct m0_rpc_item   *items[N];
	struct m0_rpc_packet *p;
	m0_bcount_t           saved_max_nr_bytes_acc;
	m0_bcount_t           saved_max_packet_size;
	int                   nr_packets;
	int                   i;

	M0_ENTRY("N: %d ITEMS_PER_PACKET: %d", N, ITEMS_PER_PACKET);

	for (i = 0; i < N; ++i)
		items[i] = new_item(WAITING, NORMAL);

	saved_max_nr_bytes_acc = frm->f_constraints.fc_max_nr_bytes_accumulated;
	frm->f_constraints.fc_max_nr_bytes_accumulated = ~0;

	flags_reset();
	for (i = 0; i < N; ++i)
		m0_rpc_frm_enq_item(frm, items[i]);

	M0_UT_ASSERT(!packet_ready_called);
	check_frm(FRM_BUSY, N, 0);

	saved_max_packet_size = frm->f_constraints.fc_max_packet_size;
	/* Each packet should carry ITEMS_PER_PACKET items */
	frm->f_constraints.fc_max_packet_size =
		ITEMS_PER_PACKET * m0_rpc_item_size(items[0]) +
		m0_rpc_packet_onwire_header_size() +
		m0_rpc_packet_onwire_footer_size();
	/* trigger formation so that all items are formed */
	frm->f_constraints.fc_max_nr_bytes_accumulated = 0;
	m0_rpc_frm_run_formation(frm);
	nr_packets = N / ITEMS_PER_PACKET + (N % ITEMS_PER_PACKET != 0 ? 1 : 0);
	M0_UT_ASSERT(packet_ready_called && top == nr_packets);
	check_frm(FRM_BUSY, 0, nr_packets);

	for (i = 0; i < nr_packets; ++i) {
		p = packet_stack[i];
		if (N % ITEMS_PER_PACKET == 0 ||
		    i != nr_packets - 1)
			M0_UT_ASSERT(p->rp_ow.poh_nr_items == ITEMS_PER_PACKET);
		else
			M0_UT_ASSERT(p->rp_ow.poh_nr_items == N %
				     ITEMS_PER_PACKET);
		(void)packet_stack_pop();
		m0_rpc_frm_packet_done(p);
		m0_rpc_packet_discard(p);
	}
	check_frm(FRM_IDLE, 0, 0);
	for (i = 0; i < N; ++i) {
		m0_rpc_item_fini(items[i]);
		m0_free(items[i]);
	}
	frm->f_constraints.fc_max_packet_size = saved_max_packet_size;
	frm->f_constraints.fc_max_nr_bytes_accumulated = saved_max_nr_bytes_acc;
	M0_UT_ASSERT(packet_stack_is_empty());

	M0_LEAVE();
}

static void frm_test5(void)
{
	M0_ENTRY();
	frm_do_test5(7, 3);
	frm_do_test5(7, 6);
	frm_do_test5(8, 8);
	frm_do_test5(8, 2);
	frm_do_test5(4, 1);
	M0_LEAVE();
}

static void frm_test6(void)
{
	/* If packet allocation fails then packet is not formed and items
	   remain in frm */
	struct m0_rpc_item   *item;

	M0_ENTRY();

	flags_reset();
	item = new_item(TIMEDOUT, NORMAL);

	m0_fi_enable_once("m0_alloc", "fail_allocation");

	m0_rpc_frm_enq_item(frm, item);
	M0_UT_ASSERT(!packet_ready_called);
	check_frm(FRM_BUSY, 1, 0);

	/* this time allocation succeds */
	m0_rpc_frm_run_formation(frm);
	M0_UT_ASSERT(packet_ready_called);
	check_frm(FRM_BUSY, 0, 1);

	check_ready_packet_has_item(item);
	m0_rpc_item_fini(item);
	m0_free(item);

	M0_LEAVE();
}

static void frm_test7(void)
{
	/* higher priority item is added to packet before lower priority */
	struct m0_rpc_item   *item1;
	struct m0_rpc_item   *item2;
	m0_bcount_t           saved_max_packet_size;
	int                   saved_max_nr_packets_enqed;

	M0_ENTRY();

	item1 = new_item(TIMEDOUT, NORMAL);
	item2 = new_item(TIMEDOUT, NORMAL);
	item1->ri_prio = M0_RPC_ITEM_PRIO_MIN;
	item2->ri_prio = M0_RPC_ITEM_PRIO_MAX;

	/* Only 1 item should be included per packet */
	saved_max_packet_size = frm->f_constraints.fc_max_packet_size;
	frm->f_constraints.fc_max_packet_size = m0_rpc_item_size(item1) +
					m0_rpc_packet_onwire_header_size() +
					m0_rpc_packet_onwire_footer_size() +
					m0_rpc_item_size(item1) / 2;

	saved_max_nr_packets_enqed = frm->f_constraints.fc_max_nr_packets_enqed;
	frm->f_constraints.fc_max_nr_packets_enqed = 0; /* disable formation */

	flags_reset();

	m0_rpc_frm_enq_item(frm, item1);
	M0_UT_ASSERT(!packet_ready_called);
	check_frm(FRM_BUSY, 1, 0);

	m0_rpc_frm_enq_item(frm, item2);
	M0_UT_ASSERT(!packet_ready_called);
	check_frm(FRM_BUSY, 2, 0);

	/* Enable formation */
	frm->f_constraints.fc_max_nr_packets_enqed = saved_max_nr_packets_enqed;
	m0_rpc_frm_run_formation(frm);

	/* Two packets should be generated */
	M0_UT_ASSERT(packet_ready_called && top == 2);
	check_frm(FRM_BUSY, 0, 2);

	/* First packet should be carrying item2 because it has higher
	   priority
	 */
	M0_UT_ASSERT(m0_rpc_packet_is_carrying_item(packet_stack[0], item2));
	M0_UT_ASSERT(m0_rpc_packet_is_carrying_item(packet_stack[1], item1));

	m0_rpc_frm_packet_done(packet_stack[0]);
	m0_rpc_frm_packet_done(packet_stack[1]);

	m0_rpc_packet_discard(packet_stack_pop());
	m0_rpc_packet_discard(packet_stack_pop());
	M0_UT_ASSERT(packet_stack_is_empty());

	m0_rpc_item_fini(item1);
	m0_rpc_item_fini(item2);
	m0_free(item1);
	m0_free(item2);

	frm->f_constraints.fc_max_packet_size = saved_max_packet_size;

	M0_LEAVE();
}

static void frm_test8(void)
{
	/* Add items with random priority and random deadline */
	enum { N = 100 };
	enum m0_rpc_item_priority  prio;
	struct m0_rpc_packet      *p;
	struct m0_rpc_item        *items[N];
	m0_bcount_t                saved_max_nr_bytes_acc;
	uint64_t                   seed_deadline;
	uint64_t                   seed_prio;
	uint64_t                   seed_kind;
	m0_time_t                  seed_timeout;
	m0_time_t                  _timeout;
	int                        saved_max_nr_packets_enqed;
	int                        i;
	int                        deadline;
	int                        kind;

	saved_max_nr_packets_enqed = frm->f_constraints.fc_max_nr_packets_enqed;
	frm->f_constraints.fc_max_nr_packets_enqed = 0; /* disable formation */

	/* start with some random seed */
	seed_deadline = 13;
	seed_kind     = 17;
	seed_prio     = 57;

	flags_reset();
	for (i = 0; i < N; ++i) {
		deadline = m0_rnd(3, &seed_deadline) + 1;
		kind     = m0_rnd(2, &seed_kind) + 1;
		prio     = m0_rnd(M0_RPC_ITEM_PRIO_NR, &seed_prio);
		_timeout = m0_rnd(1000, &seed_timeout);

		set_timeout(_timeout);

		items[i] = new_item(deadline, kind);
		items[i]->ri_prio = prio;

		m0_rpc_frm_enq_item(frm, items[i]);
		M0_UT_ASSERT(!packet_ready_called);
		check_frm(FRM_BUSY, i + 1, 0);
	}
	frm->f_constraints.fc_max_nr_packets_enqed = ~0; /* enable formation */
	saved_max_nr_bytes_acc = frm->f_constraints.fc_max_nr_bytes_accumulated;
	/* Make frm to form all items */
	frm->f_constraints.fc_max_nr_bytes_accumulated = 0;
	m0_rpc_frm_run_formation(frm);
	M0_UT_ASSERT(packet_ready_called);
	check_frm(FRM_BUSY, 0, top);

	while (!packet_stack_is_empty()) {
		p = packet_stack_pop();
		m0_rpc_frm_packet_done(p);
		m0_rpc_packet_discard(p);
	}
	check_frm(FRM_IDLE, 0, 0);
	for (i = 0; i < N; i++) {
		m0_rpc_item_fini(items[i]);
		m0_free(items[i]);
	}

	frm->f_constraints.fc_max_nr_packets_enqed = saved_max_nr_packets_enqed;
	frm->f_constraints.fc_max_nr_bytes_accumulated = saved_max_nr_bytes_acc;

	M0_LEAVE();
}

static void frm_fini_test(void)
{
	m0_rpc_frm_fini(frm);
	M0_UT_ASSERT(frm->f_state == FRM_UNINITIALISED);
}

struct m0_ut_suite frm_ut = {
	.ts_name = "rpc-formation-ut",
	.ts_init = frm_ut_init,
	.ts_fini = frm_ut_fini,
	.ts_tests = {
		{ "frm-init",     frm_init_test},
		{ "frm-test1",    frm_test1    },
		{ "frm-test2",    frm_test2    },
		{ "frm-test3",    frm_test3    },
		{ "frm-test5",    frm_test5    },
		{ "frm-test6",    frm_test6    },
		{ "frm-test7",    frm_test7    },
		{ "frm-test8",    frm_test8    },
		{ "frm-fini",     frm_fini_test},
		{ NULL,           NULL         }
	}
};
M0_EXPORTED(frm_ut);
