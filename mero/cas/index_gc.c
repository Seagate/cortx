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
 * Original author: Evgeny Exarevskiy  <evgeny.exarevskiy@seagate.com>
 * Original creation date: 15-Nov-2016
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CAS

#include "lib/trace.h"
#include "lib/memory.h"
#include "lib/assert.h"
#include "lib/cond.h"          /* m0_cond */
#include "fop/fop.h"           /* M0_FOP_TYPE_INIT */
#include "fop/fom_long_lock.h"
#include "fop/fom_generic.h"
#include "rpc/rpc_opcodes.h"
#include "rpc/item.h"          /* M0_RPC_ITEM_TYPE_REQUEST */
#include "cas/ctg_store.h"
#include "mero/setup.h"

/**
 * @page cas-gc Deleted index garbage collection
 *
 * @subsection cgc-lspec-state State Specification
 *
 * @verbatim
 *
 *                      FOPH_INIT
 *                          |
 *                          V
 *                   [generic phases]
 *                          .
 *                          .
 *                          V
 *                M0_FOPH_AUTHORISATION
 *                          |
 *                          V
 *                      CGC_LOOKUP
 *                          |
 *                          V
 * SUCCESS<----------CGC_INDEX_FOUND
 *                          |
 *                          V
 *                      TXN_INIT
 *                          |
 *                          V
 *                    CGC_CREDITS
 *                          |
 *                          V
 *                      TXN_OPEN
 *                          |
 *                          V
 *                   [generic phases]
 *                          .
 *                          .
 *                          V
 *                    CGC_TREE_CLEAN
 *                          |
 *                          V
 * SUCCESS<-----------CGC_TREE_DROP
 *                          |
 *                          V
 *                  CGC_LOCK_DEAD_INDEX
 *                          |
 *                          V
 *                 CGC_RM_FROM_DEAD_INDEX
 *                          |
 *                          V
 *                      CGC_SUCCESS
 *                          |
 *                          V
 *                       SUCCESS
 * @endverbatim
 */



static size_t cgc_fom_home_locality (const struct m0_fom *fom);
static int    cgc_fom_tick          (struct m0_fom *fom);
static void   cgc_fom_fini          (struct m0_fom *fom);
static void   cgc_retry             (void);

enum cgc_fom_phase {
	CGC_TREE_CLEAN = M0_FOPH_TYPE_SPECIFIC,
	CGC_LOOKUP,
	CGC_INDEX_FOUND,
	CGC_CREDITS,
	CGC_TREE_DROP,
	CGC_LOCK_DEAD_INDEX,
	CGC_RM_FROM_DEAD_INDEX,
	CGC_SUCCESS,
	CGC_NR
};

struct cgc_fom {
	struct m0_fom              cg_fom;
	struct m0_fop              cg_fop;
	struct m0_long_lock_link   cg_dead_index;
	struct m0_long_lock_addb2  cg_dead_index_addb2;
	struct m0_cas_ctg         *cg_ctg;
	struct m0_ctg_op           cg_ctg_op;
	struct m0_buf              cg_ctg_key;
	struct m0_reqh            *cg_reqh;
	m0_bcount_t                cg_del_limit;
};

struct cgc_context {
	struct m0_mutex  cgc_mutex;
	struct m0_cond   cgc_cond;
	int              cgc_running;
	bool             cgc_waiting;
	struct m0_be_op *cgc_op;
};

static struct cgc_context gc;

static const struct m0_fom_ops cgc_fom_ops = {
	.fo_fini          = &cgc_fom_fini,
	.fo_tick          = &cgc_fom_tick,
	.fo_home_locality = &cgc_fom_home_locality
};

M0_INTERNAL struct m0_fop_type cgc_fake_fopt;

static const struct m0_fom_type_ops cgc_fom_type_ops = {
	.fto_create = NULL
};

static struct m0_sm_state_descr cgc_fom_phases[] = {
	[CGC_LOOKUP] = {
		.sd_name      = "cgc-lookup",
		.sd_allowed   = M0_BITS(CGC_INDEX_FOUND)
	},
	[CGC_INDEX_FOUND] = {
		.sd_name      = "cgc-index-found",
		.sd_allowed   = M0_BITS(M0_FOPH_TXN_INIT, M0_FOPH_SUCCESS)
	},
	[CGC_CREDITS] = {
		.sd_name      = "cgc-credits-get",
		.sd_allowed   = M0_BITS(M0_FOPH_TXN_OPEN)
	},
	[CGC_TREE_CLEAN] = {
		.sd_name      = "cgc-tree-clean",
		.sd_allowed   = M0_BITS(CGC_TREE_DROP)
	},
	[CGC_TREE_DROP] = {
		.sd_name      = "cgc-tree-drop",
		.sd_allowed   = M0_BITS(CGC_LOCK_DEAD_INDEX, M0_FOPH_SUCCESS)
	},
	[CGC_LOCK_DEAD_INDEX] = {
		.sd_name      = "cgc-lock-dead-index",
		.sd_allowed   = M0_BITS(CGC_RM_FROM_DEAD_INDEX)
	},
	[CGC_RM_FROM_DEAD_INDEX] = {
		.sd_name      = "cgc-rm-from-dead-index",
		.sd_allowed   = M0_BITS(CGC_SUCCESS),
	},
	[CGC_SUCCESS] = {
		.sd_name      = "cgc-success",
		.sd_allowed   = M0_BITS(M0_FOPH_SUCCESS)
	},
};

struct m0_sm_trans_descr cgc_fom_trans[] = {
	[ARRAY_SIZE(m0_generic_phases_trans)] =
	{ "cgc-starting",     M0_FOPH_TXN_INIT,        CGC_LOOKUP },
	{ "cgc-index-lookup", CGC_LOOKUP,              CGC_INDEX_FOUND },
	{ "cgc-start-txn",    CGC_INDEX_FOUND,         M0_FOPH_TXN_INIT },
	{ "cgc-no-job",       CGC_INDEX_FOUND,         M0_FOPH_SUCCESS },
	{ "cgc-starting",     M0_FOPH_TXN_OPEN,        CGC_CREDITS },
	{ "cgc-credits",      CGC_CREDITS,             M0_FOPH_TXN_OPEN },
	{ "cgc-tree-drop",    CGC_TREE_CLEAN,          CGC_TREE_DROP },
	{ "cgc-start-lock",   CGC_TREE_DROP,           CGC_LOCK_DEAD_INDEX },
	{ "cgc-commit-clean", CGC_TREE_DROP,           M0_FOPH_SUCCESS },
	{ "cgc-rm-dead",      CGC_LOCK_DEAD_INDEX,     CGC_RM_FROM_DEAD_INDEX },
	{ "cgc-forget",       CGC_RM_FROM_DEAD_INDEX,  CGC_SUCCESS },
	{ "cgc-done",         CGC_SUCCESS,             M0_FOPH_SUCCESS }
};

static struct m0_sm_conf cgc_sm_conf = {
	.scf_name      = "cgc-fom",
	.scf_nr_states = ARRAY_SIZE(cgc_fom_phases),
	.scf_state     = cgc_fom_phases,
	.scf_trans_nr  = ARRAY_SIZE(cgc_fom_trans),
	.scf_trans     = cgc_fom_trans
};

static size_t cgc_fom_home_locality(const struct m0_fom *fom)
{
	return 0;
}

static int cgc_fom_tick(struct m0_fom *fom0)
{
	struct cgc_fom   *fom    = M0_AMB(fom, fom0, cg_fom);
	int               phase  = m0_fom_phase(fom0);
	struct m0_ctg_op *ctg_op = &fom->cg_ctg_op;
	int               result = M0_FSO_AGAIN;
	int               rc;

	M0_ENTRY("fom %p phase %d", fom, phase);

	switch (phase) {
	case M0_FOPH_INIT ... M0_FOPH_NR - 1:
		result = m0_fom_tick_generic(fom0);
		/*
		 * Intercept generic fom control flow when starting transaction
		 * init. Note: when we intercepted it here, state is already set
		 * to M0_FOPH_TXN_INIT.
		 * Seek for an index to drop and collect credits before
		 * transaction initialized.
		 */
		if (phase == M0_FOPH_AUTHORISATION)
			m0_fom_phase_set(fom0, CGC_LOOKUP);
		/*
		 * Intercept generic fom control flow control after transaction
		 * init but before its start complete: need to reserve credits.
		 */
		if (phase == M0_FOPH_TXN_INIT)
			m0_fom_phase_set(fom0, CGC_CREDITS);
		/*
		 * Jump over fom_queue_reply() which does nothing if fom is
		 * local, but still asserts in absence of fo_rep_fop which we do
		 * not need.
		 */
		if (phase == M0_FOPH_TXN_COMMIT)
			m0_fom_phase_set(fom0, M0_FOPH_TXN_COMMIT_WAIT);
		break;
	case CGC_LOOKUP:
		m0_ctg_op_init(ctg_op, fom0, 0);
		/*
		 * Actually we need any value from the dead index. Min key value
		 * is ok. Do not need to lock dead index here: btree logic uses
		 * its own short time lock.
		 */
		result = m0_ctg_minkey(ctg_op, m0_ctg_dead_index(),
				       CGC_INDEX_FOUND);
		break;
	case CGC_INDEX_FOUND:
		rc = m0_ctg_op_rc(ctg_op);
		if (rc == 0) {
			fom->cg_ctg_key = ctg_op->co_out_key;
			fom->cg_ctg = *(struct m0_cas_ctg **)
				ctg_op->co_out_key.b_addr;
			M0_LOG(M0_DEBUG, "got index, ctg %p", fom->cg_ctg);
			/*
			 * Use generic fom to open transaction already having
			 * credits in hands.
			 * Also lock dead index for write.
			 */
			result = m0_long_write_lock(
					m0_ctg_lock(m0_ctg_dead_index()),
					&fom->cg_dead_index,
					M0_FOPH_TXN_INIT);
			result = M0_FOM_LONG_LOCK_RETURN(result);
		} else {
			/*
			 * -ENOENT is expected here meaning no entries in dead
			 * index. Otherwise it's some unexpected error.
			 */
			if (rc == -ENOENT)
				M0_LOG(M0_DEBUG, "nothing in dead index");
			else
				M0_LOG(M0_WARN, "cgc lookup error %d", rc);
			/*
			 * Done, exit fom. cgc_fom_fini() may decide to re-start
			 * GC fom if cas service dropped some index while we
			 * were busy here. Abort transaction.
			 */
			m0_fom_phase_set(fom0, M0_FOPH_SUCCESS);
		}
		m0_ctg_op_fini(ctg_op);
		break;
	case CGC_CREDITS:
		/*
		 * Calculate credits. Must not delete more than
		 * del_limit records in the single transaction.
		 * Must calculate credits now, after transaction init but before
		 * its open in the generic fom.
		 */
		m0_ctg_dead_clean_credit(&fom0->fo_tx.tx_betx_cred);
		m0_ctg_drop_credit(fom0, &fom0->fo_tx.tx_betx_cred,
				   fom->cg_ctg, &fom->cg_del_limit);
		m0_fom_phase_set(fom0, M0_FOPH_TXN_OPEN);
		break;

		/*
		 * This is M0_FOPH_TYPE_SPECIFIC in generic fom - kind of
		 * "legal" entry point for us.
		 */
	case CGC_TREE_CLEAN:
		/*
		 * Transaction is now open. Start tree cleanup process. Remove
		 * all records from the tree but keep empty tree alive.
		 */
		m0_ctg_op_init(ctg_op, fom0, 0);
		result = m0_ctg_truncate(ctg_op, fom->cg_ctg,
					 fom->cg_del_limit,
					 CGC_TREE_DROP);
		break;
	case CGC_TREE_DROP:
		rc = m0_ctg_op_rc(ctg_op);
		m0_ctg_op_fini(ctg_op);
		if (rc == 0 && m0_be_btree_is_empty(&fom->cg_ctg->cc_tree)) {
			M0_LOG(M0_DEBUG, "tree cleaned, now drop it");
			m0_ctg_op_init(ctg_op, fom0, 0);
			result = m0_ctg_drop(ctg_op, fom->cg_ctg,
					     CGC_LOCK_DEAD_INDEX);
		} else {
			M0_LOG(M0_DEBUG, "out of credits, commit & restart");
			m0_long_unlock(m0_ctg_lock(m0_ctg_dead_index()),
			       &fom->cg_dead_index);
			cgc_retry();
			/*
			 * If out of credits. Commit transaction and
			 * start from the very beginning, by creating
			 * new fom.
			 * Let generic fom commit transaction for us.
			 */
			m0_fom_phase_set(fom0, M0_FOPH_SUCCESS);
		}
		break;
	case CGC_LOCK_DEAD_INDEX:
		m0_ctg_op_fini(ctg_op);
		m0_ctg_op_init(ctg_op, fom0, 0);
		result = m0_ctg_fini(ctg_op, fom->cg_ctg,
				     CGC_RM_FROM_DEAD_INDEX);
		break;
	case CGC_RM_FROM_DEAD_INDEX:
		m0_ctg_op_fini(ctg_op);
		m0_ctg_op_init(ctg_op, fom0, 0);
		/*
		 * Now completely forget this ctg by deleting its descriptor
		 * from "dead index" catalogue.
		 */
		result = m0_ctg_delete(ctg_op, m0_ctg_dead_index(),
				       &fom->cg_ctg_key, CGC_SUCCESS);
		break;
	case CGC_SUCCESS:
		m0_long_unlock(m0_ctg_lock(m0_ctg_dead_index()),
			       &fom->cg_dead_index);
		m0_ctg_op_fini(ctg_op);
		/*
		 * Retry: maybe, have more trees to drop.
		 */
		cgc_retry();
		/*
		 * Let generic fom commit transaction for us.
		 */
		m0_fom_phase_set(fom0, M0_FOPH_SUCCESS);
		break;
	}
	return M0_RC(result);
}

static void cgc_retry(void)
{
	/*
	 * Increment gc.cgc_running to launch same GC fom again
	 * just after fini: we still have data to delete.
	 */
	m0_mutex_lock(&gc.cgc_mutex);
	gc.cgc_running++;
	m0_mutex_unlock(&gc.cgc_mutex);
}

M0_INTERNAL void m0_cas_gc_init(void)
{
	M0_ENTRY();
	m0_sm_conf_extend(m0_generic_conf.scf_state, cgc_fom_phases,
			  m0_generic_conf.scf_nr_states);
	m0_sm_conf_trans_extend(&m0_generic_conf, &cgc_sm_conf);
	cgc_fom_phases[M0_FOPH_TXN_INIT].sd_allowed |= M0_BITS(CGC_LOOKUP);
	cgc_fom_phases[M0_FOPH_TXN_OPEN].sd_allowed |= M0_BITS(CGC_CREDITS);
	m0_sm_conf_init(&cgc_sm_conf);
	m0_mutex_init(&gc.cgc_mutex);
	m0_cond_init(&gc.cgc_cond, &gc.cgc_mutex);
	gc.cgc_running = 0;

	/*
	 * Actually we do not need a fop. But generic fom wants it, and it must
	 * be of type request and mutabo.
	 */
	M0_FOP_TYPE_INIT(&cgc_fake_fopt,
			 .name      = "cgc-fake",
			 .opcode    = M0_CAS_GCF_FOP_OPCODE,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST |
				      M0_RPC_ITEM_TYPE_MUTABO,
			 .fom_ops   = &cgc_fom_type_ops,
			 .sm        = &cgc_sm_conf,
			 .svc_type  = &m0_cas_service_type);

	M0_LEAVE();
}

M0_INTERNAL void m0_cas_gc_fini(void)
{
	M0_ENTRY();
	gc.cgc_running = 0;
	m0_fop_type_fini(&cgc_fake_fopt);
	m0_cond_fini(&gc.cgc_cond);
	m0_mutex_fini(&gc.cgc_mutex);
	m0_sm_conf_fini(&cgc_sm_conf);
	M0_LEAVE();
}

static void cgc_fop_release(struct m0_ref *ref)
{
	struct m0_fop *fop;

	M0_ENTRY();
	fop = container_of(ref, struct m0_fop, f_ref);
	/* Fop is a part of cgc_fom, it shouldn't be freed. */
	m0_fop_fini(fop);
	M0_LEAVE();
}

static void cgc_start_fom(struct m0_fom *fom0, struct m0_fop *fop)
{
	struct cgc_fom *fom = M0_AMB(fom, fom0, cg_fom);

	M0_ENTRY();

	/*
	 * Need a fop to use transactions from generic fom. If it is null, crash
	 * in fom_is_update().
	 */
	m0_fop_init(fop, &cgc_fake_fopt, NULL, &cgc_fop_release);
	m0_fom_init(fom0, &fop->f_type->ft_fom_type,
		    &cgc_fom_ops, fop, NULL, fom->cg_reqh);
	fom0->fo_local = true;
	m0_long_lock_link_init(&fom->cg_dead_index, fom0,
			       &fom->cg_dead_index_addb2);
	m0_fom_queue(fom0);
	M0_LEAVE();
}

/**
 * Finalise current index GC fom and, maybe, start new one.
 */
static void cgc_fom_fini(struct m0_fom *fom0)
{
	struct cgc_fom *fom = M0_AMB(fom, fom0, cg_fom);

	M0_ENTRY();
	M0_ASSERT(fom0->fo_fop != NULL);
	M0_ASSERT(fom0->fo_fop == &fom->cg_fop);
	m0_mutex_lock(&gc.cgc_mutex);
	gc.cgc_running--;
	/*
	 * Our fop has 2 reference counts: 1 for create, 1 for fom init.
	 * Actually we do not use fop but it required by asserts inside
	 * transaction routines.
	 * We do not use and do not have rpc so can't directly use m0_fop_put():
	 * it crashes inside assert for m0_fop_rpc_is_locked().
	 * So use m0_ref_put() manually.
	 */
	m0_ref_put(&fom0->fo_fop->f_ref);
	m0_ref_put(&fom0->fo_fop->f_ref);
	fom0->fo_fop = NULL;
	m0_fom_fini(fom0);
	m0_long_lock_link_fini(&fom->cg_dead_index);
	/*
	 * If have more job to do, start another fom using current fom memory.
	 */
	if (gc.cgc_running > 0) {
		/*
		 * Must fill fom0 by zeroes when reusing fom memory. Else assert
		 * in m0_be_tx_init() because tx is not all-zero.
		 */
		M0_SET0(fom0);
		M0_SET0(&fom->cg_fop);
		cgc_start_fom(fom0, &fom->cg_fop);
	} else {
		m0_free(fom);
		if (gc.cgc_waiting) {
			M0_LOG(M0_DEBUG, "Waking waiter for gc complete");
			m0_be_op_done(gc.cgc_op);
		}
		gc.cgc_waiting = false;
		m0_ctg_store_fini();
		m0_cond_broadcast(&gc.cgc_cond);
	}
	m0_mutex_unlock(&gc.cgc_mutex);
	M0_LEAVE();
}

M0_INTERNAL void m0_cas_gc_start(struct m0_reqh_service *service)
{
	struct cgc_fom         *fom;
	int                     rc;
	struct m0_reqh         *reqh = service->rs_reqh;
	struct m0_reqh_context *rctx;
	struct m0_be_domain    *dom;

	M0_ENTRY();

	/* Check if UT domain is preset */
	dom = m0_cas__ut_svc_be_get(service);
	if (dom == NULL) {
		rctx = m0_cs_reqh_context(reqh);
		dom = rctx->rc_beseg->bs_domain;
	}

	m0_mutex_lock(&gc.cgc_mutex);
	if (gc.cgc_running == 0) {
		/*
		 * GC fom was not running, start it now.
		 */
		M0_ALLOC_PTR(fom);
		rc = m0_ctg_store_init(dom);
		if (rc != 0 || fom == NULL) {
			M0_LOG(M0_WARN, "CGC start error fom=%p, rc=%d",
					fom, rc);
			m0_free(fom);
			if (rc == 0)
				m0_ctg_store_fini();
		} else {
			fom->cg_reqh = reqh;
			cgc_start_fom(&fom->cg_fom, &fom->cg_fop);
			gc.cgc_running++;
		}
	} else
		gc.cgc_running++;
	m0_mutex_unlock(&gc.cgc_mutex);
	M0_LEAVE();
}

M0_INTERNAL void m0_cas_gc_wait_sync(void)
{
	M0_ENTRY();
	m0_mutex_lock(&gc.cgc_mutex);
	while (gc.cgc_running > 0)
		m0_cond_wait(&gc.cgc_cond);
	m0_mutex_unlock(&gc.cgc_mutex);
	M0_LEAVE();
}

M0_INTERNAL void m0_cas_gc_wait_async(struct m0_be_op *beop)
{
	M0_ENTRY();
	m0_be_op_active(beop);
	m0_mutex_lock(&gc.cgc_mutex);
	if (gc.cgc_running == 0)
		m0_be_op_done(beop);
	else {
		M0_LOG(M0_DEBUG, "Setup wait for gc complete");
		gc.cgc_waiting = true;
		gc.cgc_op = beop;
	}
	m0_mutex_unlock(&gc.cgc_mutex);
	M0_LEAVE();
}

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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
