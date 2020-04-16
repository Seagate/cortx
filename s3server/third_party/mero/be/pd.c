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
 * Original creation date: 7-Sep-2015
 */


/**
 * @addtogroup be
 *
 * Future directions
 * - embed sm into m0_be_pd_io and get rid of m0_be_pd_io::bpi_state;
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/pd.h"

#include "lib/assert.h"         /* M0_ASSERT */
#include "lib/memory.h"         /* M0_ALLOC_ARR */
#include "lib/locality.h"       /* m0_locality0_get */

#include "be/op.h"              /* m0_be_op */
#include "be/pool.h"            /* m0_be_pool_item */
#include "be/ha.h"              /* m0_be_io_err_send */

#include "mero/magic.h"         /* M0_BE_PD_IO_MAGIC */


struct m0_be_pd_io {
	struct m0_be_pd        *bpi_pd;
	enum m0_be_pd_io_state  bpi_state;
	struct m0_be_io         bpi_be_io;
	struct m0_be_op         bpi_op;
	struct m0_be_pool_item  bpi_pool_item;
	uint64_t                bpi_pool_magic;
};

M0_BE_POOL_DESCR_DEFINE(pdio, "pd_io pool", static, struct m0_be_pd_io,
			bpi_pool_item, bpi_pool_magic, M0_BE_PD_IO_MAGIC);
M0_BE_POOL_DEFINE(pdio, static, struct m0_be_pd_io);

static void be_pd_io_move(struct m0_be_pd        *pd,
                          struct m0_be_pd_io     *pdio,
                          enum m0_be_pd_io_state  state)
{
	enum m0_be_pd_io_state prev[] = {
		[M0_BPD_IO_IDLE]        = M0_BPD_IO_DONE,
		[M0_BPD_IO_IN_PROGRESS] = M0_BPD_IO_IDLE,
		[M0_BPD_IO_DONE]        = M0_BPD_IO_IN_PROGRESS,
	};

	M0_PRE((state == M0_BPD_IO_IDLE && pdio->bpi_state == M0_BPD_IO_IDLE) ||
	       prev[state] == pdio->bpi_state);

	pdio->bpi_state = state;
}

static void be_pd_io_op_done(struct m0_be_op *op, void *param)
{
	struct m0_be_pd_io *pdio = param;
	struct m0_be_pd    *pd   = pdio->bpi_pd;

	be_pd_io_move(pd, pdio, M0_BPD_IO_DONE);
}

M0_INTERNAL int m0_be_pd_init(struct m0_be_pd     *pd,
                              struct m0_be_pd_cfg *pd_cfg)
{
	struct m0_be_pd_io *pdio;
	uint32_t            i;
	int                 rc;

	struct m0_be_pool_cfg io_pool_cfg = {
		.bplc_q_size = pd_cfg->bpdc_seg_io_pending_max,
	};

	pd->bpd_cfg = *pd_cfg;
	rc = m0_be_io_sched_init(&pd->bpd_sched, &pd->bpd_cfg.bpdc_sched);
	M0_ASSERT(rc == 0);
	rc = pdio_be_pool_init(&pd->bpd_io_pool, &io_pool_cfg);
	M0_ASSERT(rc == 0);
	M0_ALLOC_ARR(pd->bpd_io, pd->bpd_cfg.bpdc_seg_io_nr);
	M0_ASSERT(pd->bpd_io != NULL);
	for (i = 0; i < pd->bpd_cfg.bpdc_seg_io_nr; ++i) {
		pdio = &pd->bpd_io[i];
		pdio->bpi_pd = pd;
		pdio->bpi_state = M0_BPD_IO_IDLE;
		rc = m0_be_io_init(&pdio->bpi_be_io);
		M0_ASSERT(rc == 0);
		rc = m0_be_io_allocate(&pdio->bpi_be_io,
				       &pd->bpd_cfg.bpdc_io_credit);
		M0_ASSERT(rc == 0);
		m0_be_op_init(&pdio->bpi_op);
		m0_be_op_callback_set(&pdio->bpi_op, &be_pd_io_op_done,
		                      pdio, M0_BOS_DONE);
		pdio_be_pool_add(&pd->bpd_io_pool, pdio);
	}
	rc = m0_be_io_init(&pd->bpd_sync_io);
	M0_ASSERT(rc == 0);
	rc = m0_be_io_allocate(&pd->bpd_sync_io, &M0_BE_IO_CREDIT(2, 2, 2));
	M0_ASSERT(rc == 0);
	pd->bpd_sync_in_progress = false;
	pd->bpd_sync_prev = m0_time_now();
	return 0;
}

M0_INTERNAL void m0_be_pd_fini(struct m0_be_pd *pd)
{
	struct m0_be_pd_io *pdio;
	uint32_t            nr = 0;

	m0_be_io_deallocate(&pd->bpd_sync_io);
	m0_be_io_fini(&pd->bpd_sync_io);
	pdio = pdio_be_pool_del(&pd->bpd_io_pool);
	while (pdio != NULL) {
		M0_ASSERT(pdio->bpi_state == M0_BPD_IO_IDLE);
		++nr;
		m0_be_op_fini(&pdio->bpi_op);
		m0_be_io_deallocate(&pdio->bpi_be_io);
		m0_be_io_fini(&pdio->bpi_be_io);
		pdio = pdio_be_pool_del(&pd->bpd_io_pool);
	}
	M0_ASSERT(nr == pd->bpd_cfg.bpdc_seg_io_nr);
	pdio_be_pool_fini(&pd->bpd_io_pool);
	m0_free(pd->bpd_io);
	m0_be_io_sched_fini(&pd->bpd_sched);
}

M0_INTERNAL void m0_be_pd_io_add(struct m0_be_pd    *pd,
                                 struct m0_be_pd_io *pdio,
                                 struct m0_ext      *ext,
                                 struct m0_be_op    *op)
{
	M0_LOG(M0_DEBUG, "pd=%p pdio=%p ext=%p "EXT_F" op=%p",
	       pd, pdio, ext, EXT_P(ext != NULL ? ext : &M0_EXT(0, 0)), op);

	/*
	 * XXX move to TAKEN state here to avoid additional
	 * pre-allocation of be_op's.
	 */
	be_pd_io_move(pd, pdio, M0_BPD_IO_IN_PROGRESS);
	m0_be_op_set_add(op, &pdio->bpi_op);
	m0_be_io_sched_lock(&pd->bpd_sched);
	m0_be_io_sched_add(&pd->bpd_sched, m0_be_pd_io_be_io(pdio),
			   ext, &pdio->bpi_op);
	m0_be_io_sched_unlock(&pd->bpd_sched);
}

M0_INTERNAL void m0_be_pd_io_get(struct m0_be_pd     *pd,
				 struct m0_be_pd_io **pdio,
				 struct m0_be_op     *op)
{
	pdio_be_pool_get(&pd->bpd_io_pool, pdio, op);
}

M0_INTERNAL void m0_be_pd_io_put(struct m0_be_pd    *pd,
				 struct m0_be_pd_io *pdio)
{
	be_pd_io_move(pd, pdio, M0_BPD_IO_IDLE);
	m0_be_io_reset(m0_be_pd_io_be_io(pdio));
	m0_be_op_reset(&pdio->bpi_op);
	pdio_be_pool_put(&pd->bpd_io_pool, pdio);
}

M0_INTERNAL struct m0_be_io *m0_be_pd_io_be_io(struct m0_be_pd_io *pdio)
{
	return &pdio->bpi_be_io;
}

static void be_pd_sync_run(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_be_pd  *pd = ast->sa_datum;
	m0_time_t         now;
	int               rc;

	m0_be_op_active(pd->bpd_sync_op);
	now = m0_time_now();
	pd->bpd_sync_runtime = now;
	pd->bpd_sync_delay   = now - pd->bpd_sync_delay;
	pd->bpd_sync_prev    = now - pd->bpd_sync_prev;

	rc = M0_BE_OP_SYNC_RC(op, m0_be_io_launch(&pd->bpd_sync_io, &op));
	if (rc != 0)
		m0_be_io_err_send(-rc, M0_BE_LOC_NONE, SIO_SYNC);

	now = m0_time_now();
	pd->bpd_sync_runtime = now - pd->bpd_sync_runtime;
	M0_LOG(M0_DEBUG, "runtime=%lu delay=%lu prev=%lu rc=%d",
	       pd->bpd_sync_runtime, pd->bpd_sync_delay, pd->bpd_sync_prev, rc);
	pd->bpd_sync_prev        = now;
	pd->bpd_sync_in_progress = false;
	m0_be_op_done(pd->bpd_sync_op);
}

M0_INTERNAL void m0_be_pd_sync(struct m0_be_pd  *pd,
                               m0_bindex_t       pos,
                               struct m0_stob  **stobs,
                               int               nr,
                               struct m0_be_op  *op)
{
	struct m0_be_io *bio;
	int i;

	M0_ENTRY("pd=%p pos=%lu nr=%d op=%p", pd, pos, nr, op);
	M0_PRE(nr <= 2);
	M0_PRE(!pd->bpd_sync_in_progress);

	pd->bpd_sync_in_progress = true;
	bio = &pd->bpd_sync_io;
	m0_be_io_reset(bio);
	for (i = 0; i < nr; ++i)
		m0_be_io_add(bio, stobs[i], &pd->bpd_sync_read_to[i], 0, 1);
	m0_be_io_sync_enable(bio);
	m0_be_io_configure(bio, SIO_READ);
	pd->bpd_sync_ast = (struct m0_sm_ast){
		.sa_cb    = &be_pd_sync_run,
		.sa_datum = pd,
	};
	pd->bpd_sync_op    = op;
	pd->bpd_sync_delay = m0_time_now();
	m0_sm_ast_post(m0_locality0_get()->lo_grp, &pd->bpd_sync_ast);
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
