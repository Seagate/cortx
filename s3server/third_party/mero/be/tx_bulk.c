/* -*- C -*- */
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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 1-Sep-2015
 */


/**
 * @addtogroup be
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/tx_bulk.h"

#include "lib/memory.h"         /* M0_ALLOC_ARR */
#include "lib/locality.h"       /* m0_locality_get */
#include "lib/chan.h"           /* m0_clink */
#include "lib/errno.h"          /* ENOENT */

#include "be/tx.h"              /* m0_be_tx */
#include "be/op.h"              /* m0_be_op_active */
#include "be/domain.h"          /* m0_be_domain__group_limits */

#include "sm/sm.h"              /* m0_sm_ast */

enum {
	/**
	 * Maximum number of be_tx_bulk_worker-s in m0_be_tx_bulk.
	 *
	 * This value can be tuned to increase performance.
	 */
	BE_TX_BULK_WORKER_MAX = 0x40,
};

struct be_tx_bulk_worker {
	struct m0_be_tx       tbw_tx;
	struct m0_be_tx_bulk *tbw_tb;
	struct m0_sm_ast      tbw_init;
	struct m0_sm_ast      tbw_close;
	void                 *tbw_user;
	struct m0_clink       tbw_clink;
	struct m0_sm_group   *tbw_grp;
	int                   tbw_rc;
};

/* This function should be tuned using real life data. */
static uint32_t be_tx_bulk_worker_nr(struct m0_be_tx_bulk *tb,
                                     uint32_t              group_nr,
                                     uint32_t              tx_per_group)
{
	return min_check((uint32_t)BE_TX_BULK_WORKER_MAX,
			 tx_per_group * (group_nr + 1));
}

M0_INTERNAL int m0_be_tx_bulk_init(struct m0_be_tx_bulk     *tb,
                                   struct m0_be_tx_bulk_cfg *tb_cfg)
{
	uint32_t group_nr;
	uint32_t tx_per_group;
	int      rc;

	tb->btb_cfg = *tb_cfg;
	m0_be_domain__group_limits(tb->btb_cfg.tbc_dom,
				   &group_nr, &tx_per_group);
	tb->btb_worker_nr = be_tx_bulk_worker_nr(tb, group_nr, tx_per_group);
	tb->btb_done_nr = 0;
	tb->btb_stopping = false;
	tb->btb_done = false;
	m0_mutex_init(&tb->btb_lock);
	M0_ALLOC_ARR(tb->btb_worker, tb->btb_worker_nr);
	rc = tb->btb_worker == NULL ? -ENOMEM : 0;
	if (rc != 0)
		m0_mutex_fini(&tb->btb_lock);
	return rc;
}

M0_INTERNAL void m0_be_tx_bulk_fini(struct m0_be_tx_bulk *tb)
{
	m0_free(tb->btb_worker);
	m0_mutex_fini(&tb->btb_lock);
}

static void be_tx_bulk_lock(struct m0_be_tx_bulk *tb)
{
	m0_mutex_lock(&tb->btb_lock);
}

static void be_tx_bulk_unlock(struct m0_be_tx_bulk *tb)
{
	m0_mutex_unlock(&tb->btb_lock);
}

static bool be_tx_bulk_open_cb(struct m0_clink *clink);
static void be_tx_bulk_close_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast);
static void be_tx_bulk_gc_cb(struct m0_be_tx *tx, void *param);

static void be_tx_bulk_open(struct be_tx_bulk_worker *worker,
                            struct m0_be_tx_credit   *cred,
                            m0_bcount_t               cred_payload)
{
	struct m0_be_tx_bulk     *tb     = worker->tbw_tb;
	struct m0_be_tx_bulk_cfg *tb_cfg = &tb->btb_cfg;
	struct m0_be_tx          *tx     = &worker->tbw_tx;

	M0_SET0(tx);
	m0_be_tx_init(tx, 0, tb_cfg->tbc_dom, worker->tbw_grp,
		      NULL, NULL, NULL, NULL);
	m0_be_tx_gc_enable(tx, &be_tx_bulk_gc_cb, worker);

	M0_SET0(&worker->tbw_clink);
	m0_clink_init(&worker->tbw_clink, &be_tx_bulk_open_cb);
	m0_clink_add(&tx->t_sm.sm_chan, &worker->tbw_clink);

	m0_be_tx_prep(tx, cred);
	m0_be_tx_payload_prep(tx, cred_payload);
	m0_be_tx_open(tx);
}

static void be_tx_bulk_init_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct be_tx_bulk_worker *worker = ast->sa_datum;
	struct m0_be_tx_credit    cred = {};
	m0_bcount_t               cred_payload = 0;
	struct m0_be_tx_bulk     *tb;
	uint32_t                  i;
	bool                      next_exists;
	bool                      stopping;
	bool                      done;
	int                       rc;

	M0_PRE(ast == &worker->tbw_init);

	tb = worker->tbw_tb;
	worker->tbw_grp = grp;
	be_tx_bulk_lock(tb);
	stopping = tb->btb_stopping;
	be_tx_bulk_unlock(tb);
	/* @see be_tx_bulk_open_cb() */
	if (worker->tbw_rc != 0)
		m0_be_tx_fini(&worker->tbw_tx);
	if (!stopping) {
		rc = M0_BE_OP_SYNC_RET(op, tb->btb_cfg.tbc_next(tb, &op,
						tb->btb_cfg.tbc_datum,
						&worker->tbw_user), bo_rc);
		M0_ASSERT_INFO(M0_IN(rc, (0, -ENOENT)), "rc=%d", rc);
		next_exists = rc == 0;
	}
	if (!stopping && next_exists) {
		tb->btb_cfg.tbc_credit(tb, &cred, &cred_payload,
		                       tb->btb_cfg.tbc_datum, worker->tbw_user);
		be_tx_bulk_open(worker, &cred, cred_payload);
	} else {
		be_tx_bulk_lock(tb);
		tb->btb_stopping = true;
		done = ++tb->btb_done_nr == tb->btb_worker_nr;
		if (done) {
			tb->btb_rc = 0;
			for (i = 0; i < tb->btb_worker_nr; ++i) {
				rc = tb->btb_worker[i].tbw_rc;
				if (rc != 0) {
					tb->btb_rc = rc;
					break;
				}
			}
			tb->btb_done = true;
		}
		be_tx_bulk_unlock(tb);
		if (done)
			m0_be_op_done(tb->btb_op);
	}
}

static bool be_tx_bulk_open_cb(struct m0_clink *clink)
{
	struct be_tx_bulk_worker *worker;
	struct m0_be_tx_bulk     *tb;
	struct m0_be_tx          *tx;

	worker = container_of(clink, struct be_tx_bulk_worker, tbw_clink);
	tx = &worker->tbw_tx;
	tb =  worker->tbw_tb;
	if (M0_IN(m0_be_tx_state(tx), (M0_BTS_ACTIVE, M0_BTS_FAILED))) {
		m0_clink_del(&worker->tbw_clink);
		m0_clink_fini(&worker->tbw_clink);

		if (m0_be_tx_state(tx) == M0_BTS_ACTIVE) {
			worker->tbw_close.sa_cb    = &be_tx_bulk_close_cb;
			worker->tbw_close.sa_datum = worker;
			m0_sm_ast_post(worker->tbw_grp, &worker->tbw_close);
		} else {
			be_tx_bulk_lock(tb);
			tb->btb_stopping = true;
			be_tx_bulk_unlock(tb);
			worker->tbw_rc = tx->t_sm.sm_rc;
			M0_LOG(M0_ERROR, "tx=%p rc=%d", tx, worker->tbw_rc);
			/*
			 * Can't call m0_be_tx_fini(tx) here because
			 * m0_be_tx_put() for M0_BTS_FAILED transaction
			 * is called after worker transition.
			 *
			 * be_tx_bulk_init_cb() will do this.
			 */
			be_tx_bulk_gc_cb(tx, worker);
		}
	}
	return false;
}

static void be_tx_bulk_close_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_be_tx_bulk_cfg  *tb_cfg;
	struct be_tx_bulk_worker  *worker = ast->sa_datum;
	struct m0_be_tx_bulk      *tb;

	M0_PRE(ast == &worker->tbw_close);
	tb = worker->tbw_tb;
	tb_cfg = &tb->btb_cfg;
	M0_BE_OP_SYNC(op, tb_cfg->tbc_do(tb, &worker->tbw_tx, &op,
	                                 tb_cfg->tbc_datum, worker->tbw_user));
	m0_be_tx_close(&worker->tbw_tx);
}

static void be_tx_bulk_gc_cb(struct m0_be_tx *tx, void *param)
{
	struct be_tx_bulk_worker *worker = param;

	M0_PRE(tx == &worker->tbw_tx);

	m0_sm_ast_post(worker->tbw_grp, &worker->tbw_init);
}

M0_INTERNAL void m0_be_tx_bulk_run(struct m0_be_tx_bulk *tb,
                                   struct m0_be_op      *op)
{
	struct be_tx_bulk_worker *worker;
	struct m0_locality       *loc;
	uint32_t                  i;

	tb->btb_op = op;
	m0_be_op_active(tb->btb_op);
	for (i = 0; i < tb->btb_worker_nr; ++i) {
		loc = m0_locality_get(i);
		worker = &tb->btb_worker[i];
		worker->tbw_tb = tb;
		worker->tbw_rc = 0;
		worker->tbw_init.sa_cb    = &be_tx_bulk_init_cb;
		worker->tbw_init.sa_datum = worker;
		m0_sm_ast_post(loc->lo_grp, &worker->tbw_init);
	}
}

M0_INTERNAL int m0_be_tx_bulk_status(struct m0_be_tx_bulk *tb)
{
	int rc;

	be_tx_bulk_lock(tb);
	M0_PRE(tb->btb_done);
	rc = tb->btb_rc;
	be_tx_bulk_unlock(tb);
	return rc;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of be group */

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
