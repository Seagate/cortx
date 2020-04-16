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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 22-Mar-2013
 */

/**
 * @addtogroup dtm
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM

#include "lib/trace.h"
#include "ut/ut.h"
#include "lib/misc.h"                    /* M0_SET0 */

#include "dtm/dtm.h"
#include "dtm/fol.h"
#include "dtm/dtx.h"
#include "dtm/dtm_internal.h"
#include "dtm/remote.h"

enum {
	OPER_NR   =  3,
	UPDATE_NR = 15,
	REM_NR    =  7,
	DTM_NR    = REM_NR + 1,
	FAN_NR    = UPDATE_NR/REM_NR + 1 + 2
};

static struct m0_dtm              dtm_local;
static struct m0_dtm              dtm_remote[REM_NR];
static struct m0_dtm_local_remote remote_local[REM_NR];
static struct m0_dtm_local_remote remote_remote[REM_NR];
static struct m0_dtm_dtx          dx;
static struct m0_dtm_history      history0[UPDATE_NR * DTM_NR];
static struct m0_dtm_update       control_local[OPER_NR][2*DTM_NR];
static struct m0_dtm_update       control_remote[REM_NR][OPER_NR][1];
static struct m0_uint128          id0[UPDATE_NR * DTM_NR];
static struct m0_dtm_oper         oper_local[OPER_NR];
static struct m0_dtm_oper         oper_remote[REM_NR][OPER_NR];
static struct m0_dtm_update       update_local[OPER_NR][UPDATE_NR];
static struct m0_dtm_update       update_remote[REM_NR][OPER_NR][FAN_NR];
static struct m0_dtm_history     *history_local;
static struct m0_dtm_history     *history_remote[REM_NR];
static struct m0_dtm_fol          fol_remote[REM_NR];
static struct m0_tl               uu;
static struct m0_dtm_update_descr udescr[REM_NR][OPER_NR][FAN_NR];
static struct m0_dtm_update_descr ureply[REM_NR][OPER_NR][FAN_NR];
static struct m0_dtm_oper_descr   ode[REM_NR][OPER_NR];
static struct m0_dtm_oper_descr   ode_reply[REM_NR][OPER_NR];

static void noop(struct m0_dtm_op *op)
{}

static void never(struct m0_dtm_op *op)
{
	M0_IMPOSSIBLE("Never.");
}

static int undo_redo(struct m0_dtm_update *updt)
{
	return 0;
}

static const struct m0_dtm_update_type test_utype = {
	.updtt_id   = 0,
	.updtt_name = "test"
};

static const struct m0_dtm_update_ops test_ops = {
	.updo_redo = &undo_redo,
	.updo_undo = &undo_redo,
	.updo_type = &test_utype
};

static int update_init(struct m0_dtm_history *history, uint8_t id,
		       struct m0_dtm_update *update)
{
	M0_ASSERT(id == 0);
	update->upd_ops = &test_ops;
	return 0;
}

static void test_persistent(struct m0_dtm_history *history)
{}

static const struct m0_dtm_op_ops op_ops = {
	.doo_ready      = noop,
	.doo_late       = never,
	.doo_miser      = never
};

static const struct m0_dtm_history_ops hops;

static int hfind(struct m0_dtm *dtm, const struct m0_dtm_history_type *ht,
		 const struct m0_uint128 *id, struct m0_dtm_history **out)
{
	unsigned idx = id->u_lo + dtm->d_id.u_lo * UPDATE_NR;
	M0_UT_ASSERT(id->u_hi == 0 && IS_IN_ARRAY(idx, history0));
	*out = &history0[idx];
	if ((*out)->h_hi.hi_ups.t_magic == 0) {
		m0_dtm_history_init(*out, dtm);
		(*out)->h_ops = &hops;
	}
	return 0;
}

static const struct m0_dtm_history_type_ops htype_ops = {
	.hito_find = hfind
};

static const struct m0_dtm_history_type htype = {
	.hit_id     = 1,
	.hit_rem_id = 1,
	.hit_name   = "test-histories",
	.hit_ops    = &htype_ops
};

static const struct m0_uint128 *hid(const struct m0_dtm_history *history)
{
	int idx = history - history0;

	M0_UT_ASSERT(IS_IN_ARRAY(idx, id0));
	id0[idx].u_hi = 0;
	id0[idx].u_lo = idx % UPDATE_NR;
	return &id0[idx];
}

static const struct m0_dtm_history_ops hops = {
	.hio_type       = &htype,
	.hio_id         = &hid,
	.hio_persistent = &test_persistent,
	.hio_update     = &update_init
};

static void init0(void)
{
	int          i;
	int          j;
	struct m0_tl uu;

	M0_SET0(&update_local);
	M0_SET0(&update_remote);
	M0_SET0(&control_local);
	M0_SET0(&control_remote);
	M0_SET0(&oper_local);
	M0_SET0(&oper_remote);
	M0_SET0(&history0);
	history_local = history0;
	for (i = 0; i < REM_NR; ++i)
		history_remote[i] = &history0[(i + 1) * UPDATE_NR];
	m0_dtm_init(&dtm_local, &M0_UINT128(0, 0));
	m0_dtm_history_type_register(&dtm_local, &htype);
	m0_dtm_history_type_register(&dtm_local, &m0_dtm_dtx_htype);
	m0_dtm_history_type_register(&dtm_local, &m0_dtm_fol_remote_htype);
	for (i = 0; i < ARRAY_SIZE(dtm_remote); ++i) {
		m0_dtm_init(&dtm_remote[i], &M0_UINT128(1, i + 1));
		m0_dtm_history_type_register(&dtm_remote[i], &htype);
		m0_dtm_history_type_register(&dtm_remote[i],
					     &m0_dtm_dtx_srv_htype);
		m0_dtm_history_type_register(&dtm_remote[i],
					     &m0_dtm_fol_remote_htype);
		m0_dtm_local_remote_init(&remote_local[i],
					 &dtm_remote[i].d_id, &dtm_local, NULL);
		m0_dtm_local_remote_init(&remote_remote[i],
					 &dtm_local.d_id, &dtm_remote[i], NULL);
	}
	for (i = 0; i < UPDATE_NR; ++i) {
		struct m0_dtm_history *history = &history_local[i];

		m0_dtm_history_init(history, &dtm_local);
		history->h_hi.hi_ver = 1;
		history->h_hi.hi_flags |= M0_DHF_OWNED;
		history->h_ops = &hops;
		history->h_rem =
			&remote_local[i % ARRAY_SIZE(remote_local)].lre_rem;
	}
	for (i = 0; i < OPER_NR; ++i) {
		m0_dtm_update_list_init(&uu);
		m0_dtm_update_link(&uu, control_local[i], 2*DTM_NR);
		m0_dtm_oper_init(&oper_local[i], &dtm_local, &uu);
		oper_local[i].oprt_op.op_ops = &op_ops;
	}
	for (i = 0; i < REM_NR; ++i) {
		m0_dtm_fol_init(&fol_remote[i], &dtm_remote[i]);
		for (j = 0; j < OPER_NR; ++j) {
			ode[i][j].od_updates.ou_update = udescr[i][j];
			ode_reply[i][j].od_updates.ou_update = ureply[i][j];
			m0_dtm_update_list_init(&uu);
			m0_dtm_update_link(&uu, control_remote[i][j], 1);
			m0_dtm_oper_init(&oper_remote[i][j],
					 &dtm_remote[i], &uu);
			oper_remote[i][j].oprt_op.op_ops = &op_ops;
		}
	}
}

static void fini0(void)
{
	int i;
	int j;

	for (i = 0; i < REM_NR; ++i) {
		for (j = 0; j < OPER_NR; ++j)
			m0_dtm_oper_fini(&oper_remote[i][j]);
		m0_dtm_fol_fini(&fol_remote[i]);
	}
	for (i = 0; i < OPER_NR; ++i)
		m0_dtm_oper_fini(&oper_local[i]);

	for (i = 0; i < UPDATE_NR; ++i)
		m0_dtm_history_fini(&history_local[i]);

	for (i = 0; i < ARRAY_SIZE(dtm_remote); ++i) {
		m0_dtm_local_remote_fini(&remote_remote[i]);
		m0_dtm_local_remote_fini(&remote_local[i]);
		m0_dtm_history_type_deregister(&dtm_remote[i], &htype);
		m0_dtm_history_type_deregister(&dtm_remote[i],
					       &m0_dtm_dtx_srv_htype);
		m0_dtm_history_type_deregister(&dtm_remote[i],
					       &m0_dtm_fol_remote_htype);
		m0_dtm_fini(&dtm_remote[i]);
	}
	m0_dtm_history_type_deregister(&dtm_local, &m0_dtm_dtx_htype);
	m0_dtm_history_type_deregister(&dtm_local, &m0_dtm_fol_remote_htype);
	m0_dtm_history_type_deregister(&dtm_local, &htype);
	m0_dtm_fini(&dtm_local);
}

static void init1(void)
{
	int result;

	init0();
	result = m0_dtm_dtx_init(&dx, &M0_UINT128(0, 0), &dtm_local, REM_NR);
	M0_UT_ASSERT(result == 0);
}

static void fini1(void)
{
	m0_dtm_dtx_fini(&dx);
	fini0();
}

static void init2(void)
{
	int i;
	int j;

	init1();
	for (i = 0; i < OPER_NR; ++i) {
		dtm_lock(&dtm_local);
		for (j = 0; j < UPDATE_NR; ++j) {
			update_local[i][j].upd_ops = &test_ops;
			m0_dtm_update_init(&update_local[i][j],
					   &history_local[j], &oper_local[i],
			   &M0_DTM_UPDATE_DATA(M0_DTM_USER_UPDATE_BASE + 2 * j,
					       M0_DUR_INC, 2 + i, 1 + i));
		}
		dtm_unlock(&dtm_local);
		m0_dtm_dtx_add(&dx, &oper_local[i]);
		m0_dtm_oper_close(&oper_local[i]);
		M0_UT_ASSERT(op_state(&oper_local[i].oprt_op, M0_DOS_PREPARE));
		for (j = 0; j < REM_NR; ++j)
			m0_dtm_oper_prepared(&oper_local[i],
					     &remote_local[j].lre_rem);
		M0_UT_ASSERT(op_state(&oper_local[i].oprt_op,
				      M0_DOS_INPROGRESS));
	}
	m0_dtm_dtx_close(&dx);
}

static void fini2(void)
{
	fini1();
}

static void init3(void)
{
	int i;
	int j;
	int result;

	init2();
	for (i = 0; i < OPER_NR; ++i) {
		for (j = 0; j < REM_NR; ++j) {
			struct m0_dtm_oper       *oper = &oper_remote[j][i];
			struct m0_dtm_oper_descr *o    = &ode[j][i];

			o->od_updates.ou_nr = UPDATE_NR + 2 * REM_NR;
			m0_dtm_oper_pack(&oper_local[i],
					 &remote_local[j].lre_rem, o);
			m0_dtm_update_list_init(&uu);
			m0_dtm_update_link(&uu, update_remote[j][i], FAN_NR);
			result = m0_dtm_oper_build(oper, &uu, o);
			M0_UT_ASSERT(result == 0);
		}
	}
}

static void fini3(void)
{
	fini2();
};

struct m0_dtm_dtx_party {
	struct m0_dtm_dtx     *pa_dtx;
	struct m0_dtm_controlh pa_ch;
};

static void init4(void)
{
	int  i;
	int  j;
	bool progress;
	int  done = 0;

	init3();
	do {
		progress = false;
		for (i = 0; i < REM_NR; ++i) {
			for (j = 0; j < OPER_NR; ++j) {
				struct m0_dtm_oper *oper;

				oper = &oper_remote[i][j];
				if (op_state(&oper->oprt_op, M0_DOS_LIMBO)) {
					m0_dtm_oper_close(oper);
					progress = true;
				}
				if (op_state(&oper->oprt_op, M0_DOS_PREPARE)) {
					m0_dtm_oper_prepared(oper, NULL);
					progress = true;
				}
				if (op_state(&oper->oprt_op, M0_DOS_INPROGRESS)) {
					m0_dtm_oper_done(oper, NULL);
					m0_dtm_oper_done(oper,
						    &remote_remote[i].lre_rem);
					progress = true;
					done++;
				}
			}
		}
	} while (progress && done < OPER_NR);
	for (i = 0; i < REM_NR; ++i) {
		for (j = 0; j < OPER_NR; ++j) {
			struct m0_dtm_oper *oper = &oper_remote[i][j];
			struct m0_dtm_oper *loper = &oper_local[j];

			M0_UT_ASSERT(op_state(&oper->oprt_op, M0_DOS_VOLATILE));
			ode_reply[i][j].od_updates.ou_nr = FAN_NR;
			m0_dtm_reply_pack(oper, &ode[i][j], &ode_reply[i][j]);
			m0_dtm_reply_unpack(loper, &ode_reply[i][j]);
			m0_dtm_oper_done(loper, &remote_local[i].lre_rem);
		}
	}
	M0_UT_ASSERT(m0_forall(i, OPER_NR, op_state(&oper_local[i].oprt_op,
						    M0_DOS_VOLATILE)));
/*
	for (i = 0; i < OPER_NR; ++i) {
		M0_LOG(M0_FATAL, "l%i", i);
		oper_print(&oper_local[i]);
	}
	for (i = 0; i < REM_NR; ++i) {
		M0_LOG(M0_FATAL, "d%i", i);
		history_print(&dx.dt_party[i].pa_ch.ch_history);
	}
*/
}

static void fini4(void)
{
	fini3();
}

static void init5(void)
{
	int i;

	init4();
	for (i = 0; i < REM_NR; ++i) {
		M0_UT_ASSERT(dx.dt_nr_fixed == i);
		m0_dtm_history_persistent(&dtm_remote[i].d_fol.fo_ch.ch_history,
					  ~0ULL);
		M0_UT_ASSERT(dx.dt_nr_fixed == i + 1);
	}
	M0_UT_ASSERT(dx.dt_nr_fixed == dx.dt_nr);
}

static void fini5(void)
{
	fini4();
}

static void dtm_setup(void)
{
	init0();
	fini0();
}

static void dtx_setup(void)
{
	init1();
	fini1();
}

static void dtx_populate(void)
{
	init2();
	fini2();
}

static void dtx_pack(void)
{
	init3();
	fini3();
}

static void dtx_reply(void)
{
	init4();
	fini4();
}

static void dtx_fix(void)
{
	init5();
	fini5();
}

struct m0_ut_suite dtm_dtx_ut = {
	.ts_name = "dtm-dtx-ut",
	.ts_tests = {
		{ "dtm-setup",    dtm_setup     },
		{ "dtx-setup",    dtx_setup     },
		{ "dtx-populate", dtx_populate  },
		{ "dtx-pack",     dtx_pack      },
		{ "dtx-reply",    dtx_reply     },
		{ "dtx-fix",      dtx_fix       },
		{ NULL, NULL }
	}
};
M0_EXPORTED(dtm_dtx_ut);

#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm group */

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
