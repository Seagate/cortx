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
#include "ut/ut_rpc_machine.h"
#include "lib/misc.h"         /* m0_forall, IS_IN_ARRAY */
#include "lib/tlist.h"
#include "lib/errno.h"        /* EPROTO */
#include "lib/assert.h"
#include "lib/memory.h"
#include "fop/fop.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "rpc/rpc_opcodes.h"
#include "dtm/dtm_internal.h"
#include "dtm/operation.h"
#include "dtm/operation_xc.h"
#include "dtm/history.h"
#include "dtm/remote.h"
#include "dtm/update.h"
#include "dtm/fol.h"
#include "dtm/ltx.h"
#include "dtm/dtm.h"

M0_INTERNAL void up_print(const struct m0_dtm_up *up);
M0_INTERNAL void op_print(const struct m0_dtm_op *op);
M0_INTERNAL void hi_print(const struct m0_dtm_hi *hi);

enum {
	OPER_NR   = 64,
	UPDATE_NR = 6,
	TGT_DELTA = 4
};

static struct m0_uint128          dtm_id_src = { 1, 1 };
static struct m0_uint128          dtm_id_tgt = { 2, 2 };
static struct m0_tl               uu;
static struct m0_dtm              dtm_src;
static struct m0_dtm              dtm_tgt;
static struct m0_dtm_local_remote tgt;
static struct m0_dtm_local_remote local;
static struct m0_dtm_oper         oper_src[OPER_NR];
static struct m0_dtm_oper         oper_tgt[OPER_NR];
static struct m0_dtm_update       update_src[OPER_NR][UPDATE_NR];
static struct m0_dtm_update       update_tgt[OPER_NR][UPDATE_NR + TGT_DELTA];
static struct m0_dtm_update       control_src[OPER_NR][TGT_DELTA];
static struct m0_dtm_update       control_tgt[OPER_NR][TGT_DELTA];
static struct m0_dtm_history      history_src[UPDATE_NR];
static struct m0_dtm_history      history_tgt[UPDATE_NR];
static struct m0_uint128          id_src[UPDATE_NR];
static struct m0_uint128          id_tgt[UPDATE_NR];
static struct m0_dtm_update_descr udescr[UPDATE_NR + TGT_DELTA];
static struct m0_dtm_update_descr udescr_reply[UPDATE_NR + TGT_DELTA];
static struct m0_fop              redo_fop[OPER_NR];
static struct m0_fom              redo_fom[OPER_NR];
static struct m0_dtm_oper_descr   redo_ode[OPER_NR];
static struct m0_dtm_update_descr redo_udescr[OPER_NR][UPDATE_NR + TGT_DELTA];
static struct m0_dtm_oper_descr   ode = {
	.od_updates = {
		.ou_nr     = UPDATE_NR + TGT_DELTA,
		.ou_update = udescr
	}
};
static struct m0_dtm_oper_descr   reply = {
	.od_updates = {
		.ou_nr     = UPDATE_NR + TGT_DELTA,
		.ou_update = udescr_reply
	}
};
static m0_dtm_ver_t last[UPDATE_NR];
static struct m0_ut_rpc_mach_ctx test_ctx;

static void noop(struct m0_dtm_op *op)
{}

static int undo_redo(struct m0_dtm_update *updt)
{
	return 0;
}

static const struct m0_dtm_update_type test_utype = {
	.updtt_id   = 0,
	.updtt_name = "test update"
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
	.doo_late       = noop,
	.doo_miser      = noop
};

static int src_find(struct m0_dtm *dtm,
		    const struct m0_dtm_history_type *ht,
		    const struct m0_uint128 *id,
		    struct m0_dtm_history **out)
{
	if (id->u_hi == 0 && IS_IN_ARRAY(id->u_lo, history_src)) {
		*out = &history_src[id->u_lo];
		return 0;
	} else
		return -EPROTO;
}

static const struct m0_dtm_history_type_ops src_htype_ops = {
	.hito_find = src_find
};

static const struct m0_dtm_history_type src_htype = {
	.hit_id     = 2,
	.hit_rem_id = 2,
	.hit_name   = "source histories",
	.hit_ops    = &src_htype_ops
};

static const struct m0_uint128 *src_id(const struct m0_dtm_history *history)
{
	int idx = history - history_src;

	M0_PRE(IS_IN_ARRAY(idx, id_src));
	id_src[idx].u_hi = 0;
	id_src[idx].u_lo = idx;
	return &id_src[idx];
}

static const struct m0_dtm_history_ops src_ops = {
	.hio_type       = &src_htype,
	.hio_id         = &src_id,
	.hio_persistent = &test_persistent,
	.hio_update     = &update_init
};

static int tgt_find(struct m0_dtm *dtm,
		    const struct m0_dtm_history_type *ht,
		    const struct m0_uint128 *id,
		    struct m0_dtm_history **out)
{
	if (id->u_hi == 0 && IS_IN_ARRAY(id->u_lo, history_tgt)) {
		*out = &history_tgt[id->u_lo];
		return 0;
	} else
		return -EPROTO;
}

static const struct m0_dtm_history_type_ops tgt_htype_ops = {
	.hito_find = tgt_find
};

static const struct m0_dtm_history_type tgt_htype = {
	.hit_id     = 2,
	.hit_rem_id = 2,
	.hit_name   = "target histories",
	.hit_ops    = &tgt_htype_ops
};

static const struct m0_uint128 *tgt_id(const struct m0_dtm_history *history)
{
	int idx = history - history_tgt;

	M0_PRE(IS_IN_ARRAY(idx, id_tgt));
	id_tgt[idx].u_hi = 0;
	id_tgt[idx].u_lo = idx;
	return &id_tgt[idx];
}

static const struct m0_dtm_history_ops tgt_ops = {
	.hio_type       = &tgt_htype,
	.hio_id         = &tgt_id,
	.hio_persistent = &test_persistent,
	.hio_update     = &update_init
};

static struct m0_fop_type           test_fopt;
static struct m0_reqh_service      *test_svc;
static const struct m0_fop_type_ops test_ftype_ops;

static int service_start(struct m0_reqh_service *service)
{
	return 0;
}

static void service_stop(struct m0_reqh_service *service)
{}

static void service_fini(struct m0_reqh_service *service)
{
	m0_free(service);
}

static const struct m0_reqh_service_ops test_service_ops = {
	.rso_start = &service_start,
	.rso_stop  = &service_stop,
	.rso_fini  = &service_fini
};

static int service_allocate(struct m0_reqh_service **service,
			    const struct m0_reqh_service_type *stype)
{
	struct m0_reqh_service *svc;

	M0_ALLOC_PTR(svc);
	M0_UT_ASSERT(svc != NULL);
	svc->rs_type = stype;
	svc->rs_ops = &test_service_ops;
	*service = svc;
	return 0;
}

static const struct m0_reqh_service_type_ops stype_ops = {
	.rsto_service_allocate = &service_allocate
};

struct m0_reqh_service_type test_stype = {
	.rst_name  = "dtm-ub-service",
	.rst_ops   = &stype_ops,
	.rst_level = M0_RS_LEVEL_NORMAL,
};

static void test_fom_fini(struct m0_fom *fom)
{
	m0_fom_fini(fom);
}

static unsigned ticked;

static void op_ready(struct m0_dtm_op *op)
{
	struct m0_dtm_oper *oper;
	unsigned            idx;

	oper = M0_AMB(oper, op, oprt_op);
	idx = oper - &oper_tgt[0];
	M0_UT_ASSERT(IS_IN_ARRAY(idx, oper_tgt));
	m0_fom_wakeup(&redo_fom[idx]);
}

static void op_late(struct m0_dtm_op *op)
{
	M0_UT_ASSERT(0);
}

static void op_miser(struct m0_dtm_op *op)
{
	M0_UT_ASSERT(0);
}

static const struct m0_dtm_op_ops test_op_ops = {
	.doo_ready = &op_ready,
	.doo_late  = &op_late,
	.doo_miser = &op_miser
};

enum {
	FOM_INIT  = M0_FOM_PHASE_INIT,
	FOM_READY = M0_FOM_PHASE_NR
};

static struct m0_sm_state_descr fom_phases[] = {
	[FOM_INIT] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "init",
		.sd_allowed   = M0_BITS(FOM_READY)
	},
	[FOM_READY] = {
		.sd_name      = "ready",
		.sd_allowed   = M0_BITS(M0_FOPH_FINISH)
	},
	[M0_FOPH_FINISH] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "SM finish",
	}
};

const struct m0_sm_conf test_conf = {
	.scf_name      = "dtm up fom",
	.scf_nr_states = ARRAY_SIZE(fom_phases),
	.scf_state     = fom_phases
};

static struct m0_mutex     lock;
static struct m0_semaphore seq;

static int test_fom_tick(struct m0_fom *fom)
{
	int                       result;
	int                       idx = fom->fo_fop - &redo_fop[0];
	struct m0_dtm_oper_descr *ode = m0_fop_data(fom->fo_fop);
	struct m0_tl              uu;
	struct m0_dtm_oper       *oper = &oper_tgt[idx];

	M0_UT_ASSERT(IS_IN_ARRAY(idx, oper_tgt));
	M0_UT_ASSERT(ode->od_updates.ou_nr <= ARRAY_SIZE(update_tgt[idx]));

	switch (m0_fom_phase(fom)) {
	case FOM_INIT:
		m0_semaphore_up(&seq);
		m0_dtm_update_list_init(&uu);
		m0_dtm_update_link(&uu, control_tgt[idx], TGT_DELTA);
		m0_dtm_oper_init(oper, &dtm_tgt, &uu);
		m0_dtm_update_list_fini(&uu);
		m0_dtm_update_list_init(&uu);
		m0_dtm_update_link(&uu, update_tgt[idx], ode->od_updates.ou_nr);

		result = m0_dtm_oper_build(oper, &uu, ode);
		M0_UT_ASSERT(result == 0);
		oper->oprt_op.op_ops = &test_op_ops;
		m0_dtm_update_list_fini(&uu);
		m0_dtm_oper_close(oper);
		m0_fom_phase_set(fom, FOM_READY);
		break;
	case FOM_READY:
		m0_mutex_lock(&lock);
		m0_dtm_oper_prepared(oper, NULL);
		m0_dtm_oper_done(oper, NULL);
		++ticked;
		m0_fom_phase_set(fom, M0_FOM_PHASE_FINISH);
		m0_mutex_unlock(&lock);
		break;
	default:
		M0_UT_ASSERT(0);
	}
	return M0_FSO_WAIT;
}

static size_t test_fom_home_locality(const struct m0_fom *fom)
{
	static size_t locality = 0;

	return locality++;
}

static const struct m0_fom_ops test_fom_ops = {
	.fo_fini          = &test_fom_fini,
	.fo_tick          = &test_fom_tick,
	.fo_home_locality = &test_fom_home_locality
};

static int test_fom_create(struct m0_fop *fop, struct m0_fom **out,
			   struct m0_reqh *reqh)
{
	unsigned idx = fop - &redo_fop[0];

	M0_UT_ASSERT(IS_IN_ARRAY(idx, redo_fop));
	M0_UT_ASSERT(redo_fom[idx].fo_magic == 0);

	*out = &redo_fom[idx];
	m0_fom_init(*out, &test_fopt.ft_fom_type, &test_fom_ops,
		    fop, NULL, reqh);
	return 0;
}

static const struct m0_fom_type_ops test_fom_type_ops = {
	.fto_create = &test_fom_create
};

static void rpc_fop_fom_init(void)
{
	int result;

	m0_mutex_init(&lock);
	m0_semaphore_init(&seq, 0);
	test_ctx = (struct m0_ut_rpc_mach_ctx) {
		.rmc_cob_id  = { 20 },
		.rmc_ep_addr = "0@lo:12345:34:10"
	};
	m0_ut_rpc_mach_init_and_add(&test_ctx);

	result = m0_reqh_service_type_register(&test_stype);
	M0_UT_ASSERT(result == 0);

	M0_FOP_TYPE_INIT(&test_fopt,
			 .name      = "dtm test fop",
			 .opcode    = M0_DTM_UP_OPCODE,
			 .xt        = m0_dtm_oper_descr_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fop_ops   = &test_ftype_ops,
			 .fom_ops   = &test_fom_type_ops,
			 .sm        = &test_conf,
			 .svc_type  = &test_stype,
			 .rpc_ops   = &m0_fop_default_item_type_ops);

	result = m0_reqh_service_allocate(&test_svc, &test_stype, NULL);
	M0_UT_ASSERT(result == 0);
	m0_reqh_service_init(test_svc, &test_ctx.rmc_reqh, NULL);

	result = m0_reqh_service_start(test_svc);
	M0_UT_ASSERT(result == 0);
}

static void rpc_fop_fom_fini(void)
{
	m0_reqh_service_prepare_to_stop(test_svc);
	m0_reqh_shutdown_wait(&test_ctx.rmc_reqh);
	m0_reqh_service_stop(test_svc);
	m0_reqh_service_fini(test_svc);
	m0_reqh_service_type_unregister(&test_stype);
	m0_ut_rpc_mach_fini(&test_ctx);
	m0_fop_type_fini(&test_fopt);
	m0_semaphore_fini(&seq);
	m0_mutex_fini(&lock);
}

static void src_init(struct m0_dtm_remote *dtm, unsigned flags, int ctrl)
{
	int i;

	M0_ASSERT(ctrl <= TGT_DELTA);

	rpc_fop_fom_init();

	memset(oper_src, 0, sizeof oper_src);
	memset(update_src, 0, sizeof update_src);
	memset(control_src, 0, sizeof control_src);
	memset(history_src, 0, sizeof history_src);
	M0_SET0(&dtm_src);
	M0_SET0(&tgt);
	M0_SET0(&redo_fop);
	M0_SET0(&redo_fom);
	M0_SET0(&redo_ode);
	M0_SET0(&redo_udescr);
	m0_dtm_init(&dtm_src, &dtm_id_src);
	m0_dtm_history_type_register(&dtm_src, &src_htype);
	m0_dtm_history_type_register(&dtm_src, &m0_dtm_fol_remote_htype);
	m0_dtm_local_remote_init(&tgt, &dtm_id_tgt, &dtm_src,
				 &test_ctx.rmc_reqh);

	for (i = 0; i < ARRAY_SIZE(history_src); ++i) {
		m0_dtm_history_init(&history_src[i], &dtm_src);
		history_src[i].h_hi.hi_ver = 1;
		history_src[i].h_hi.hi_flags |= flags;
		history_src[i].h_ops = &src_ops;
		history_src[i].h_rem = dtm;
	}
	for (i = 0; i < ARRAY_SIZE(oper_src); ++i) {
		m0_dtm_update_list_init(&uu);
		m0_dtm_update_link(&uu, control_src[i], ctrl);
		m0_dtm_oper_init(&oper_src[i], &dtm_src, &uu);
		oper_src[i].oprt_op.op_ops = &op_ops;
		redo_ode[i] = (struct m0_dtm_oper_descr) {
			.od_updates = {
				.ou_nr     = UPDATE_NR + TGT_DELTA,
				.ou_update = redo_udescr[i]
			}
		};
		m0_fop_init(&redo_fop[i], &test_fopt, &redo_ode[i],
			    &m0_fop_release);
		redo_fop[i].f_item.ri_rmachine = &test_ctx.rmc_rpc;
	}
	for (i = 0; i < ARRAY_SIZE(last); ++i)
		last[i] = 1;
}

static void src_fini(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(oper_src); ++i) {
		m0_dtm_oper_fini(&oper_src[i]);
		redo_fop[i].f_data.fd_data = NULL;
		m0_fop_fini(&redo_fop[i]);
	}
	for (i = 0; i < ARRAY_SIZE(history_src); ++i)
		m0_dtm_history_fini(&history_src[i]);
	m0_dtm_local_remote_fini(&tgt);
	m0_dtm_history_type_deregister(&dtm_src, &src_htype);
	m0_dtm_history_type_deregister(&dtm_src, &m0_dtm_fol_remote_htype);
	m0_dtm_fini(&dtm_src);

	rpc_fop_fom_fini();
}

static void tgt_init(void)
{
	int i;

	memset(oper_tgt, 0, sizeof oper_tgt);
	memset(update_tgt, 0, sizeof update_tgt);
	memset(control_tgt, 0, sizeof control_tgt);
	memset(history_tgt, 0, sizeof history_tgt);
	M0_SET0(&dtm_tgt);
	M0_SET0(&local);
	m0_dtm_init(&dtm_tgt, &dtm_id_tgt);
	m0_dtm_history_type_register(&dtm_tgt, &tgt_htype);
	m0_dtm_history_type_register(&dtm_tgt, &m0_dtm_fol_remote_htype);
	m0_dtm_local_remote_init(&local, &dtm_id_src, &dtm_tgt, NULL);

	for (i = 0; i < ARRAY_SIZE(history_tgt); ++i) {
		m0_dtm_history_init(&history_tgt[i], &dtm_tgt);
		history_tgt[i].h_hi.hi_ver = 1;
		history_tgt[i].h_hi.hi_flags |= M0_DHF_OWNED;
		history_tgt[i].h_ops = &tgt_ops;
		history_tgt[i].h_rem = NULL;
	}
	for (i = 0; i < ARRAY_SIZE(oper_tgt); ++i) {
		m0_dtm_update_list_init(&uu);
		m0_dtm_update_link(&uu, control_tgt[i], TGT_DELTA);
		m0_dtm_oper_init(&oper_tgt[i], &dtm_tgt, &uu);
		oper_tgt[i].oprt_op.op_ops = &op_ops;
	}
}

static void tgt_fini(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(oper_tgt); ++i)
		m0_dtm_oper_fini(&oper_tgt[i]);
	m0_dtm_update_list_fini(&uu);
	for (i = 0; i < ARRAY_SIZE(history_tgt); ++i)
		m0_dtm_history_fini(&history_tgt[i]);
	m0_dtm_local_remote_fini(&local);
	m0_dtm_history_type_deregister(&dtm_tgt, &tgt_htype);
	m0_dtm_history_type_deregister(&dtm_tgt, &m0_dtm_fol_remote_htype);
	m0_dtm_fini(&dtm_tgt);
}

static void oper_populate(int i, unsigned nr)
{
	int j;

	dtm_lock(&dtm_src);
	for (j = 0; j < nr; ++j) {
		update_src[i][j].upd_ops = &test_ops;
		m0_dtm_update_init(&update_src[i][j], &history_src[j],
				   &oper_src[i],
			   &M0_DTM_UPDATE_DATA(M0_DTM_USER_UPDATE_BASE + 1 + j,
					       M0_DUR_SET, i + 2, last[j]));
		last[j] = i + 2;
	}
	dtm_unlock(&dtm_src);
	m0_dtm_oper_close(&oper_src[i]);

	ode.od_updates.ou_nr   = nr + TGT_DELTA;
	reply.od_updates.ou_nr = nr + TGT_DELTA;
}

static void transmit_build(void)
{
	int i;
	int result;

	src_init(&tgt.lre_rem, 0, 2);
	tgt_init();
	for (i = 0; i < ARRAY_SIZE(oper_src); ++i) {
		unsigned nr = (i%UPDATE_NR) + 1;
		oper_populate(i, nr);
		m0_dtm_oper_pack(&oper_src[i], &tgt.lre_rem, &ode);
		m0_dtm_oper_pack(&oper_src[i], &tgt.lre_rem, &redo_ode[i]);
		update_src[i][0].upd_comm.uc_body = &redo_fop[i];

		m0_dtm_update_list_init(&uu);
		m0_dtm_update_link(&uu, update_tgt[i], nr + TGT_DELTA);

		result = m0_dtm_oper_build(&oper_tgt[i], &uu, &ode);
		M0_UT_ASSERT(result == 0);
		m0_dtm_oper_close(&oper_tgt[i]);
		m0_dtm_oper_prepared(&oper_tgt[i], NULL);
		m0_dtm_oper_done(&oper_tgt[i], NULL);

		m0_dtm_reply_pack(&oper_tgt[i], &ode, &reply);
		m0_dtm_reply_unpack(&oper_src[i], &reply);
		m0_dtm_oper_prepared(&oper_src[i], &tgt.lre_rem);
		m0_dtm_oper_done(&oper_src[i], &tgt.lre_rem);
	}
}

static void transmit_test(void)
{
	transmit_build();
	tgt_fini();
	src_fini();
}

#if 0

static void db_init(void)
{
	int result;

	result = m0_dbenv_init(&db, db_name, 0, true);
	M0_UT_ASSERT(result == 0);
	result = m0_emap_init(&emap, &db, "nonce");
	M0_UT_ASSERT(result == 0);
}

static void db_fini(void)
{
	m0_emap_fini(&emap);
	m0_dbenv_fini(&db);
}

static struct m0_dtm_ltx ltx;
static uint64_t          hi = 7;

static void ltx_init(void)
{
	M0_SET0(&ltx);
	src_init(NULL, M0_DHF_OWNED, 1);
	m0_dtm_history_type_register(&dtm_src, &m0_dtm_ltx_htype);
	db_init();
}

static void ltx_fini(unsigned nr)
{
	int result;
	int i;

	result = m0_emap_obj_insert(&emap, &ltx.lx_tx, &M0_UINT128(hi++, 0), 9);
	M0_UT_ASSERT(result == 0);
	for (i = 0; i < nr; ++i) {
		m0_dtm_oper_prepared(&oper_src[i], NULL);
		m0_dtm_oper_done(&oper_src[i], NULL);
	}
	m0_dtm_ltx_close(&ltx);
	db_fini();
	M0_UT_ASSERT(m0_forall(j, nr,
		      _0C(op_state(&oper_src[j].oprt_op, M0_DOS_PERSISTENT))));
	m0_dtm_history_type_deregister(&dtm_src, &m0_dtm_ltx_htype);
	m0_dtm_update_list_fini(&uu);
	src_fini();
	M0_SET0(&ltx);
}

static void ltx_test_1_N(void)
{
	int result;

	ltx_init();

	m0_dtm_ltx_init(&ltx, &dtm_src, &db);
	result = m0_dtm_ltx_open(&ltx);
	M0_UT_ASSERT(result == 0);

	m0_dtm_ltx_add(&ltx, &oper_src[0]);
	oper_populate(0, UPDATE_NR);
	ltx_fini(1);
}

static void ltx_test_N_N(void)
{
	int result;
	int i;

	ltx_init();

	M0_CASSERT(UPDATE_NR + 1 < OPER_NR);

	m0_dtm_ltx_init(&ltx, &dtm_src, &db);
	result = m0_dtm_ltx_open(&ltx);
	M0_UT_ASSERT(result == 0);

	for (i = 0; i < UPDATE_NR; ++i) {
		m0_dtm_ltx_add(&ltx, &oper_src[i]);
		oper_populate(i, ((i*3) % UPDATE_NR) + 1);
	}
	ltx_fini(UPDATE_NR);
}
#endif

static void redo_test(void)
{
	int i;

	transmit_build();

	/* crash tgt */
	tgt_fini();
	tgt_init();

	ticked = 0;
	/* reinitialise the semaphore to count foms afresh. */
	m0_semaphore_fini(&seq);
	m0_semaphore_init(&seq, 0);

	m0_dtm_history_reset(&tgt.lre_rem.re_fol.rfo_ch.ch_history, 2);
	/* wait until all foms start execution. */
	for (i = 0; i < OPER_NR; ++i)
		m0_semaphore_down(&seq);
	m0_reqh_idle_wait(&test_ctx.rmc_reqh);
	M0_UT_ASSERT(ticked == OPER_NR);
	tgt_fini();
	src_fini();
}

struct m0_ut_suite dtm_transmit_ut = {
	.ts_name = "dtm-transmit-ut",
	.ts_tests = {
		{ "transmit",    transmit_test  },
#if 0
		{ "ltx-1-N",     ltx_test_1_N   },
		{ "ltx-N-N",     ltx_test_N_N   },
#endif
		{ "redo",        redo_test      },
		{ NULL, NULL }
	}
};
M0_EXPORTED(dtm_transmit_ut);

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
