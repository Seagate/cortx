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
 * Original creation date: 6-Sep-2015
 */


/**
 * @addtogroup be
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/log_discard.h"

#include "lib/errno.h"          /* ENOMEM */
#include "lib/ext.h"            /* m0_ext */
#include "lib/locality.h"       /* m0_locality_call */
#include "lib/memory.h"         /* M0_ALLOC_ARR */
#include "lib/misc.h"           /* container_of */

#include "sm/sm.h"              /* m0_sm_ast */

#include "be/op.h"              /* m0_be_op_active */
#include "be/pool.h"            /* m0_be_pool_item */

#include "mero/magic.h"         /* M0_BE_LOG_DISCARD_POOL_MAGIC */

enum be_log_discard_item_state {
	LDI_INIT,
	LDI_STARTING,
	LDI_FINISHED,
	LDI_SYNCED,
	LDI_DISCARDED,
};

struct m0_be_log_discard_item {
	struct m0_be_log_discard       *ldi_ld;
	enum be_log_discard_item_state  ldi_state;
	void                           *ldi_user_data;
	m0_time_t                       ldi_time_start;
	m0_time_t                       ldi_time_finish;
	bool                            ldi_synced;
	struct m0_ext                   ldi_ext;
	struct m0_tlink                 ldi_start_link;
	uint64_t                        ldi_magic;
	struct m0_be_pool_item          ldi_pool_item;
	uint64_t                        ldi_pool_magic;
	/**
	 * To run ldsc_discard callback outside the m0_be_log_discard lock.
	 * XXX Temporary solution.
	 */
	struct m0_sm_ast                ldi_discard_ast;
};


M0_TL_DESCR_DEFINE(ld_start, "m0_be_log_discard::lds_start_q", static,
		   struct m0_be_log_discard_item, ldi_start_link, ldi_magic,
		   M0_BE_LOG_DISCARD_MAGIC, M0_BE_LOG_DISCARD_HEAD_MAGIC);
M0_TL_DEFINE(ld_start, static, struct m0_be_log_discard_item);

M0_BE_POOL_DESCR_DEFINE(ld, "m0_be_log_discard::lds_item_pool", static,
			struct m0_be_log_discard_item, ldi_pool_item,
			ldi_pool_magic, M0_BE_LOG_DISCARD_POOL_MAGIC);
M0_BE_POOL_DEFINE(ld, static, struct m0_be_log_discard_item);

static void be_log_discard_sync_done_cb(struct m0_be_op *op, void *param);
static void be_log_discard_timer_start(struct m0_be_log_discard *ld);
static void be_log_discard_timer_cancel(struct m0_be_log_discard *ld);

M0_INTERNAL int m0_be_log_discard_init(struct m0_be_log_discard     *ld,
                                       struct m0_be_log_discard_cfg *ld_cfg)
{
	struct m0_be_log_discard_item *ldi;
	uint32_t                       i;
	int                            rc;
	struct m0_be_pool_cfg          pool_cfg = {
		.bplc_q_size = ld_cfg->ldsc_items_pending_max,
	};

	ld->lds_cfg = *ld_cfg;

	M0_ALLOC_ARR(ld->lds_item, ld->lds_cfg.ldsc_items_max);
	if (ld->lds_item == NULL)
		return M0_ERR(-ENOMEM);
	rc = ld_be_pool_init(&ld->lds_item_pool, &pool_cfg);
	if (rc != 0) {
		m0_free(ld->lds_item);
		return rc;
	}
	m0_mutex_init(&ld->lds_lock);
	ld_start_tlist_init(&ld->lds_start_q);
	for (i = 0; i < ld->lds_cfg.ldsc_items_max; ++i) {
		ldi = &ld->lds_item[i];
		*ldi = (struct m0_be_log_discard_item){
			.ldi_ld     = ld,
			.ldi_state  = LDI_INIT,
			.ldi_synced = false,
		};
		ld_be_pool_add(&ld->lds_item_pool, ldi);
		ld_start_tlink_init(ldi);
	}
	ld->lds_need_sync = false;
	ld->lds_sync_in_progress = false;
	ld->lds_sync_deadline = M0_TIME_NEVER;
	ld->lds_discard_left = 0;
	ld->lds_discard_waiting = false;
	m0_be_op_init(&ld->lds_sync_op);
	m0_be_op_callback_set(&ld->lds_sync_op, &be_log_discard_sync_done_cb,
			      ld, M0_BOS_GC);
	ld->lds_flush_op = NULL;
	m0_sm_timer_init(&ld->lds_sync_timer);
	ld->lds_stopping = false;
	m0_semaphore_init(&ld->lds_discard_wait_sem, 0);
	be_log_discard_timer_start(ld);
	return 0;
}

static void be_log_discard_wait(struct m0_be_log_discard *ld);

M0_INTERNAL void m0_be_log_discard_fini(struct m0_be_log_discard *ld)
{
	struct m0_be_log_discard_item *ldi;
	uint32_t                       nr = 0;

	be_log_discard_wait(ld);

	M0_PRE(!ld->lds_need_sync);
	M0_PRE(!ld->lds_sync_in_progress);

	be_log_discard_timer_cancel(ld);
	m0_semaphore_fini(&ld->lds_discard_wait_sem);
	m0_sm_timer_fini(&ld->lds_sync_timer);
	m0_be_op_fini(&ld->lds_sync_op);
	ldi = ld_be_pool_del(&ld->lds_item_pool);
	while (ldi != NULL) {
		++nr;
		M0_ASSERT(ldi->ldi_state == LDI_INIT);
		ld_start_tlink_fini(ldi);
		ldi = ld_be_pool_del(&ld->lds_item_pool);
	}
	M0_ASSERT(nr == ld->lds_cfg.ldsc_items_max);
	ld_start_tlist_fini(&ld->lds_start_q);
	m0_mutex_fini(&ld->lds_lock);
	ld_be_pool_fini(&ld->lds_item_pool);
	m0_free(ld->lds_item);
}

static void be_log_discard_lock(struct m0_be_log_discard *ld)
{
	m0_mutex_lock(&ld->lds_lock);
}

static void be_log_discard_unlock(struct m0_be_log_discard *ld)
{
	m0_mutex_unlock(&ld->lds_lock);
}

static bool be_log_discard_is_locked(struct m0_be_log_discard *ld)
{
	return m0_mutex_is_locked(&ld->lds_lock);
}

static void be_log_discard_check_sync(struct m0_be_log_discard *ld,
                                      bool                      force)
{
	struct m0_be_log_discard_item *ldi;

	M0_PRE(be_log_discard_is_locked(ld));

	if (ld->lds_need_sync || force ||
	    (!ld->lds_sync_in_progress &&
	     (ld_start_tlist_length(&ld->lds_start_q) >=
	      ld->lds_cfg.ldsc_items_threshold ||
	      (!ld_start_tlist_is_empty(&ld->lds_start_q) &&
	       ld->lds_sync_deadline <= m0_time_now()))))
		ld->lds_need_sync = true;
	if (ld->lds_need_sync && !ld->lds_sync_in_progress) {
		ld->lds_need_sync = false;
		ld->lds_sync_item = NULL;
		m0_tl_for(ld_start, &ld->lds_start_q, ldi) {
			if (ldi->ldi_state == LDI_STARTING)
				break;
			ld->lds_sync_item = ldi;
		} m0_tl_endfor;
		if (ld->lds_sync_item == NULL && ld->lds_flush_op != NULL) {
			M0_ASSERT(ld_start_tlist_is_empty(&ld->lds_start_q));
			m0_be_op_active(ld->lds_flush_op);
			m0_be_op_done(ld->lds_flush_op);
		}
		if (ld->lds_sync_item != NULL) {
			ld->lds_sync_in_progress = true;
			if (ld->lds_flush_op != NULL) {
				m0_be_op_set_add(ld->lds_flush_op,
						 &ld->lds_sync_op);
				ld->lds_flush_op = NULL;
			}
			M0_LOG(M0_DEBUG, "ld=%p lds_sync_item=%p",
			       ld, ld->lds_sync_item);
			/* be_log_discard_sync_done_cb() locks ld */
			be_log_discard_unlock(ld);
			ld->lds_cfg.ldsc_sync(ld, &ld->lds_sync_op,
			                      ld->lds_sync_item);
			be_log_discard_lock(ld);
		}
	}
}

static void be_log_discard_item_discard_ast(struct m0_sm_group *grp,
                                            struct m0_sm_ast   *ast)
{
	struct m0_be_log_discard_item *ldi = ast->sa_datum;
	struct m0_be_log_discard      *ld  = ldi->ldi_ld;

	M0_ENTRY("ld=%p ldi=%p", ld, ldi);

	ld->lds_cfg.ldsc_discard(ld, ldi);

	be_log_discard_lock(ld);
	ldi->ldi_state = LDI_DISCARDED;
	m0_be_log_discard_item_put(ld, ldi);
	M0_CNT_DEC(ld->lds_discard_left);
	if (ld->lds_discard_waiting)
		m0_semaphore_up(&ld->lds_discard_wait_sem);
	be_log_discard_unlock(ld);
}

static void be_log_discard_item_discard(struct m0_be_log_discard      *ld,
                                        struct m0_be_log_discard_item *ldi)
{
	M0_LOG(M0_DEBUG, "ld=%p ldi=%p", ld, ldi);

	M0_PRE(be_log_discard_is_locked(ld));
	M0_PRE(ldi->ldi_state == LDI_SYNCED);

	M0_CNT_INC(ld->lds_discard_left);
	ldi->ldi_discard_ast = (struct m0_sm_ast){
		.sa_cb    = &be_log_discard_item_discard_ast,
		.sa_datum = ldi,
	};
	/* get out of the m0_be_log_discard lock */
	m0_sm_ast_post(m0_locality_here()->lo_grp, &ldi->ldi_discard_ast);
}

static void be_log_discard_item_trydiscard(struct m0_be_log_discard      *ld,
                                           struct m0_be_log_discard_item *ldi)
{
	M0_PRE(be_log_discard_is_locked(ld));
	M0_PRE(M0_IN(ldi->ldi_state, (LDI_STARTING, LDI_FINISHED)));

	if (ldi->ldi_state == LDI_FINISHED && ldi->ldi_synced)
		ldi->ldi_state = LDI_SYNCED;
	if (ldi->ldi_state == LDI_SYNCED)
		be_log_discard_item_discard(ld, ldi);
}

static void be_log_discard_sync_done_cb(struct m0_be_op *op, void *param)
{
	struct m0_be_log_discard_item *ldi;
	struct m0_be_log_discard      *ld = param;

	M0_LOG(M0_DEBUG, "ld=%p", ld);
	be_log_discard_lock(ld);
	M0_PRE(ld->lds_sync_item != NULL);
	m0_be_op_reset(op);
	m0_tl_for(ld_start, &ld->lds_start_q, ldi) {
		M0_ASSERT_INFO(ldi->ldi_state == LDI_FINISHED,
			       "ldi=%p state=%d", ldi, ldi->ldi_state);
		ld_start_tlist_del(ldi);
		ldi->ldi_synced = true;
		be_log_discard_item_trydiscard(ld, ldi);
		if (ldi == ld->lds_sync_item)
			break;
	} m0_tl_endfor;
	ld->lds_sync_item = NULL;
	ld->lds_sync_in_progress = false;
	if (ld_start_tlist_is_empty(&ld->lds_start_q))
		ld->lds_sync_deadline = M0_TIME_NEVER;
	be_log_discard_check_sync(ld, false);
	be_log_discard_unlock(ld);
}

static void be_log_discard_timer_cb(struct m0_sm_timer *timer);

static int be_log_discard_timer_start_loc(void *param)
{
	struct m0_be_log_discard *ld = param;
	m0_time_t                 deadline = 0;
	int                       rc;

	if (!ld->lds_stopping) {
		be_log_discard_lock(ld);
		deadline = ld->lds_sync_deadline == M0_TIME_NEVER ||
			ld->lds_sync_deadline <= m0_time_now() ?
			m0_time_now() + ld->lds_cfg.ldsc_sync_timeout :
			ld->lds_sync_deadline;
		rc = m0_sm_timer_start(&ld->lds_sync_timer,
		                       ld->lds_cfg.ldsc_loc->lo_grp,
		                       &be_log_discard_timer_cb, deadline);
		M0_ASSERT_INFO(rc == 0, "rc = %d", rc);
		be_log_discard_unlock(ld);
	}
	return 0;
}

static void be_log_discard_timer_start(struct m0_be_log_discard *ld)
{
	(void)m0_locality_call(ld->lds_cfg.ldsc_loc,
			       &be_log_discard_timer_start_loc, ld);
}

static int be_log_discard_timer_cancel_loc(void *param)
{
	struct m0_be_log_discard *ld = param;

	ld->lds_stopping = true;
	m0_sm_timer_cancel(&ld->lds_sync_timer);
	return 0;
}

static void be_log_discard_timer_cancel(struct m0_be_log_discard *ld)
{
	(void)m0_locality_call(ld->lds_cfg.ldsc_loc,
			       &be_log_discard_timer_cancel_loc, ld);
}

static void be_log_discard_timer_cb(struct m0_sm_timer *timer)
{
	struct m0_be_log_discard *ld;

	ld = container_of(timer, struct m0_be_log_discard, lds_sync_timer);
	be_log_discard_lock(ld);
	be_log_discard_check_sync(ld, false);
	m0_sm_timer_fini(&ld->lds_sync_timer);
	m0_sm_timer_init(&ld->lds_sync_timer);
	be_log_discard_unlock(ld);
	be_log_discard_timer_start_loc(ld);
}

M0_INTERNAL void m0_be_log_discard_sync(struct m0_be_log_discard *ld)
{
	be_log_discard_lock(ld);
	be_log_discard_check_sync(ld, true);
	be_log_discard_unlock(ld);
}

M0_INTERNAL void m0_be_log_discard_flush(struct m0_be_log_discard *ld,
                                         struct m0_be_op          *op)
{
	be_log_discard_lock(ld);
	M0_PRE(ld->lds_flush_op == NULL);
	ld->lds_flush_op = op;
	be_log_discard_check_sync(ld, true);
	be_log_discard_unlock(ld);
}

static void be_log_discard_wait(struct m0_be_log_discard *ld)
{
	int left;
	int i;

	M0_PRE(!ld->lds_discard_waiting);

	be_log_discard_lock(ld);
	left = ld->lds_discard_left;
	ld->lds_discard_waiting = true;
	be_log_discard_unlock(ld);

	for (i = 0; i < left; ++i)
		m0_semaphore_down(&ld->lds_discard_wait_sem);
}

M0_INTERNAL void
m0_be_log_discard_item_starting(struct m0_be_log_discard      *ld,
                                struct m0_be_log_discard_item *ldi)
{
	M0_LOG(M0_DEBUG, "ld=%p ldi=%p", ld, ldi);

	be_log_discard_lock(ld);
	M0_PRE(ldi->ldi_state == LDI_INIT);
	ldi->ldi_time_start = m0_time_now();
	ldi->ldi_state = LDI_STARTING;
	ld_start_tlist_add_tail(&ld->lds_start_q, ldi);
	ld->lds_sync_deadline = ldi->ldi_time_start +
				ld->lds_cfg.ldsc_sync_timeout;
	be_log_discard_check_sync(ld, false);
	be_log_discard_unlock(ld);
}

M0_INTERNAL void
m0_be_log_discard_item_finished(struct m0_be_log_discard      *ld,
                                struct m0_be_log_discard_item *ldi)
{
	M0_LOG(M0_DEBUG, "ld=%p ldi=%p", ld, ldi);

	be_log_discard_lock(ld);
	M0_PRE(ldi->ldi_state == LDI_STARTING);
	ldi->ldi_time_finish = m0_time_now();
	ldi->ldi_state = LDI_FINISHED;
	be_log_discard_item_trydiscard(ld, ldi);
	be_log_discard_unlock(ld);
}

static void be_log_discard_item_reset(struct m0_be_log_discard      *ld,
                                      struct m0_be_log_discard_item *ldi)
{
	M0_PRE(M0_IN(ldi->ldi_state, (LDI_INIT, LDI_DISCARDED)));

	ldi->ldi_state       = LDI_INIT;
	ldi->ldi_synced      = false;
	ldi->ldi_time_start  = 0;
	ldi->ldi_time_finish = 0;
	ldi->ldi_time_start  = M0_TIME_NEVER;
	ldi->ldi_time_finish = M0_TIME_NEVER;
}

M0_INTERNAL void
m0_be_log_discard_item_get(struct m0_be_log_discard       *ld,
                           struct m0_be_op                *op,
                           struct m0_be_log_discard_item **ldi)
{
	ld_be_pool_get(&ld->lds_item_pool, ldi, op);
}

M0_INTERNAL void m0_be_log_discard_item_put(struct m0_be_log_discard      *ld,
                                            struct m0_be_log_discard_item *ldi)
{
	M0_LOG(M0_DEBUG, "ld=%p ldi=%p", ld, ldi);

	be_log_discard_item_reset(ld, ldi);
	ld_be_pool_put(&ld->lds_item_pool, ldi);
}

M0_INTERNAL void
m0_be_log_discard_item_user_data_set(struct m0_be_log_discard_item *ldi,
                                     void                          *data)
{
	ldi->ldi_user_data = data;
}

M0_INTERNAL void *
m0_be_log_discard_item_user_data(struct m0_be_log_discard_item *ldi)
{
	return ldi->ldi_user_data;
}

M0_INTERNAL void
m0_be_log_discard_item_ext_set(struct m0_be_log_discard_item *ldi,
                               struct m0_ext                 *ext)
{
	ldi->ldi_ext = *ext;
}

M0_INTERNAL struct m0_ext *
m0_be_log_discard_item_ext(struct m0_be_log_discard_item *ldi)
{
	return &ldi->ldi_ext;
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
