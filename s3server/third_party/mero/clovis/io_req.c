/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original authors: Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Revision:         Pratik Shinde   <pratik.shinde@seagate.com>
 * Original creation date: 3-Nov-2014
 *
 * Original 'm0t1fs' author: Anand Vidwansa <anand_vidwansa@xyratex.com>
 */

#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_addb.h"
#include "clovis/pg.h"
#include "clovis/io.h"

#include "lib/errno.h"
#include "lib/semaphore.h"       /* m0_semaphore_{down|up}*/
#include "fid/fid.h"             /* m0_fid */
#include "rpc/rpclib.h"          /* m0_rpc_client_connect */
#include "lib/ext.h"             /* struct m0_ext */
#include "lib/misc.h"            /* M0_KEY_VAL_NULL */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"           /* M0_LOG */

/*
 * CPU usage threshold for parity calculation which is introuduced by
 * commit d4fcee53611e to solve the LNet timeout problem caused by
 * by IO overusing CPUs.
 */
struct m0_semaphore clovis_cpus_sem;

/** All possible state machine transitions for an IO requests */
static struct m0_sm_state_descr io_states[] = {
	[IRS_INITIALIZED]       = {
		.sd_flags       = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name        = "IO_initial",
		.sd_allowed     = M0_BITS(IRS_READING, IRS_WRITING,
					  IRS_FAILED, IRS_REQ_COMPLETE),
	},
	[IRS_READING]	        = {
		.sd_name        = "IO_reading",
		.sd_allowed     = M0_BITS(IRS_READ_COMPLETE, IRS_FAILED),
	},
	[IRS_READ_COMPLETE]     = {
		.sd_name        = "IO_read_complete",
		.sd_allowed     = M0_BITS(IRS_WRITING, IRS_REQ_COMPLETE,
					  IRS_DEGRADED_READING, IRS_FAILED,
					  IRS_READING),
	},
	[IRS_DEGRADED_READING]  = {
		.sd_name        = "IO_degraded_read",
		.sd_allowed     = M0_BITS(IRS_READ_COMPLETE, IRS_FAILED),
	},
	[IRS_DEGRADED_WRITING]  = {
		.sd_name        = "IO_degraded_write",
		.sd_allowed     = M0_BITS(IRS_WRITE_COMPLETE, IRS_FAILED),
	},
	[IRS_TRUNCATE]  = {
		.sd_name        = "IO_truncate",
		.sd_allowed     = M0_BITS(IRS_TRUNCATE_COMPLETE, IRS_FAILED),
	},
	[IRS_TRUNCATE_COMPLETE]  = {
		.sd_name        = "IO_truncate_complte",
		.sd_allowed     = M0_BITS(IRS_REQ_COMPLETE, IRS_FAILED),
	},
	[IRS_WRITING]           = {
		.sd_name        = "IO_writing",
		.sd_allowed     = M0_BITS(IRS_WRITE_COMPLETE, IRS_FAILED),
	},
	[IRS_WRITE_COMPLETE]    = {
		.sd_name        = "IO_write_complete",
		.sd_allowed     = M0_BITS(IRS_REQ_COMPLETE, IRS_FAILED,
					  IRS_TRUNCATE,
					  IRS_DEGRADED_WRITING),
	},
	[IRS_FAILED]            = {
		/* XXX Add M0_SDF_TERMINAL | M0_SDF_FINAL ? */
		.sd_flags       = M0_SDF_FAILURE,
		.sd_name        = "IO_req_failed",
		.sd_allowed     = M0_BITS(IRS_REQ_COMPLETE),
	},
	[IRS_REQ_COMPLETE]      = {
		/* XXX Add M0_SDF_FINAL ? */
		.sd_flags       = M0_SDF_TERMINAL,
		.sd_name        = "IO_req_complete",
	},
};

static struct m0_sm_trans_descr ioo_trans[] = {
	{ "init-reading",  IRS_INITIALIZED, IRS_READING               },
	{ "init-writing",  IRS_INITIALIZED, IRS_WRITING               },
	{ "init-complete", IRS_INITIALIZED, IRS_REQ_COMPLETE          },
	{ "init-failed",   IRS_INITIALIZED, IRS_FAILED                },

	{ "read-complete",  IRS_READING, IRS_READ_COMPLETE            },
	{ "read-failed",    IRS_READING, IRS_FAILED                   },
	{ "write-complete", IRS_WRITING, IRS_WRITE_COMPLETE           },
	{ "write-failed",   IRS_WRITING, IRS_FAILED                   },

	{ "rcompl-write",    IRS_READ_COMPLETE, IRS_WRITING           },
	{ "rcompl-complete", IRS_READ_COMPLETE, IRS_REQ_COMPLETE      },
	{ "rcompl-dgread",   IRS_READ_COMPLETE, IRS_DEGRADED_READING  },
	{ "rcompl-failed",   IRS_READ_COMPLETE, IRS_FAILED            },
	{ "rcompl-reading",  IRS_READ_COMPLETE, IRS_READING           },

	{ "wcompl-dgwrite",  IRS_WRITE_COMPLETE, IRS_DEGRADED_WRITING },
	{ "wcompl-complete", IRS_WRITE_COMPLETE, IRS_REQ_COMPLETE     },
	{ "wcompl-trunc",    IRS_WRITE_COMPLETE, IRS_TRUNCATE         },
	{ "wcompl-failed",   IRS_WRITE_COMPLETE, IRS_FAILED           },

	{ "trunc-tcompl",    IRS_TRUNCATE, IRS_TRUNCATE_COMPLETE      },
	{ "trunc-failed",    IRS_TRUNCATE, IRS_FAILED                 },

	{ "tcompl-complete", IRS_TRUNCATE_COMPLETE, IRS_REQ_COMPLETE  },
	{ "tcompl-failed",   IRS_TRUNCATE_COMPLETE, IRS_FAILED        },

	{ "dgread-rcompl",   IRS_DEGRADED_READING, IRS_READ_COMPLETE  },
	{ "dgread-failed",   IRS_DEGRADED_READING, IRS_FAILED         },
	{ "dgwrite-wcompl",  IRS_DEGRADED_WRITING, IRS_WRITE_COMPLETE },
	{ "dgwrite-failed",  IRS_DEGRADED_WRITING, IRS_FAILED         },

	{ "failed-complete", IRS_FAILED, IRS_REQ_COMPLETE             },
};

/** IO request state machine config */
struct m0_sm_conf io_sm_conf = {
	.scf_name      = "IO request state machine configuration",
	.scf_nr_states = ARRAY_SIZE(io_states),
	.scf_state     = io_states,
	.scf_trans     = ioo_trans,
	.scf_trans_nr  = ARRAY_SIZE(ioo_trans),
};

/** BOB definitions for m0_clovis_op_io */
const struct m0_bob_type ioo_bobtype;
M0_BOB_DEFINE(M0_INTERNAL, &ioo_bobtype, m0_clovis_op_io);

const struct m0_bob_type ioo_bobtype = {
	.bt_name         = "clovis_op_io_bobtype",
	.bt_magix_offset = offsetof(struct m0_clovis_op_io, ioo_magic),
	.bt_magix        = M0_CLOVIS_IOREQ_MAGIC,
	.bt_check        = NULL,
};

 /**
  * This is heavily based on m0t1fs/linux_kernel/file.c::is_pver_dud.
  *
  * If there are F(l) failures at level l, and K(l) failures are tolerable for
  * the level l, then the condition for pool-version to be non-dud is:
  *			\sum_over_l {F(l) / K(l)} <= 1
  * Once MERO-899 lands into master, this function will go away.
  */
static bool is_pver_dud(uint32_t fdev_nr, uint32_t dev_k, uint32_t fsvc_nr,
			uint32_t svc_k)
{
	if (fdev_nr > 0 && dev_k == 0)
		return M0_RC(true);
	if (fsvc_nr > 0 && svc_k == 0)
		return M0_RC(true);
	return M0_RC((svc_k + fsvc_nr > 0) ?
		(fdev_nr * svc_k + fsvc_nr * dev_k) > dev_k * svc_k :
		fdev_nr > dev_k);
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::ioreq_sm_state_set
 */
M0_INTERNAL void ioreq_sm_state_set_locked(struct m0_clovis_op_io *ioo,
					   int state)
{
	M0_ENTRY();

	M0_PRE(ioo != NULL);
	M0_PRE(m0_sm_group_is_locked(ioo->ioo_sm.sm_grp));

	M0_LOG(M0_INFO, "[%p] IO request changes state %s -> %s",
	       ioo, io_states[ioreq_sm_state(ioo)].sd_name,
	       io_states[state].sd_name);
	m0_sm_state_set(&ioo->ioo_sm, state);

	M0_LEAVE();
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::ioreq_sm_failed
 */
M0_INTERNAL void ioreq_sm_failed_locked(struct m0_clovis_op_io *ioo, int rc)
{
	M0_ENTRY();

	M0_PRE(ioo != NULL);
	M0_PRE(m0_sm_group_is_locked(ioo->ioo_sm.sm_grp));

	/* Set the io operation state  - FAILED isn't a terminal state */
	m0_sm_move(&ioo->ioo_sm, rc, IRS_FAILED);

	M0_LEAVE();
}

static void ioreq_sm_executed_post(struct m0_clovis_op_io *ioo)
{

	M0_ENTRY();

	M0_PRE(ioo != NULL);
	M0_PRE(m0_sm_group_is_locked(ioo->ioo_sm.sm_grp));

	ioo->ioo_ast.sa_cb = ioo->ioo_ops->iro_iosm_handle_executed;
	m0_sm_ast_post(ioo->ioo_oo.oo_sm_grp, &ioo->ioo_ast);

	M0_LEAVE();
}

static int truncate_dispatch(struct m0_clovis_op_io *ioo)
{
	int                  rc = 0;
	struct m0_clovis_op *op;

	M0_ENTRY();

	M0_PRE(ioo != NULL);
	op = &ioo->ioo_oo.oo_oc.oc_op;

	if (ioreq_sm_state(ioo) == IRS_WRITE_COMPLETE &&
	    op->op_code == M0_CLOVIS_OC_FREE) {
		ioreq_sm_state_set_locked(ioo, IRS_TRUNCATE);
		rc = ioo->ioo_nwxfer.nxr_ops->nxo_dispatch(&ioo->ioo_nwxfer);
	}

	return M0_RC(rc);
}

/**
 * Sets the states of all devices (to ONLINE) once all the replies have
 * come back.
 * This is heavily based on m0t1fs/linux_kernel/file.c::device_state_reset
 *
 * @param xfer The network transfer for the completed request.
 */
static void nw_xfer_device_state_reset(struct nw_xfer_request *xfer)
{
	struct target_ioreq *ti;

	M0_ENTRY();

	M0_PRE(xfer != NULL);
	M0_PRE(xfer->nxr_state == NXS_COMPLETE);

	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		ti->ti_state = M0_PNDS_ONLINE;
	} m0_htable_endfor;

	M0_LEAVE();
}

/**
 * Resets ioo and target io requests when they are re-used to avoid
 * those polluted fields flowing into next step of IO.
 *
 * @param ioo  The object io operation needed to be reset.
 */
static void ioreq_ioo_reset(struct m0_clovis_op_io *ioo)
{
	struct nw_xfer_request *xfer;
	struct target_ioreq *ti;

	M0_ENTRY();

	M0_PRE(ioo != NULL);
	xfer = &ioo->ioo_nwxfer;

	xfer->nxr_rc = 0;
	xfer->nxr_bytes = 0;

	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		ti->ti_rc = 0;
	} m0_htable_endfor;

	ioo->ioo_rc = 0;
	M0_LEAVE();
}

/**
 * AST callback scheduled by RM-file-lock acquire, this does the actual
 * work of launching the operation's rpc messages.
 * This is heavily based on m0t1fs/linux_kernel/file.c::ioreq_iosm_handle
 *
 * @param grp The statemachine group the ast was posted to.
 * @param ast The ast struct, used to find the m0_clovis_op_io.
 */
static void ioreq_iosm_handle_launch(struct m0_sm_group *grp,
				      struct m0_sm_ast *ast)
{
	int                       rc;
	struct m0_clovis_op      *op;
	struct m0_clovis_op_io   *ioo;
	struct m0_pdclust_layout *play;

	M0_ENTRY();

	M0_PRE(grp != NULL);
	M0_PRE(m0_sm_group_is_locked(grp));
	M0_PRE(ast != NULL);
	ioo = bob_of(ast, struct m0_clovis_op_io, ioo_ast, &ioo_bobtype);
	M0_PRE_EX(m0_clovis_op_io_invariant(ioo));
	op = &ioo->ioo_oo.oo_oc.oc_op;
	play = pdlayout_get(ioo);

	/* @todo Do error handling based on m0_sm::sm_rc. */
	/*
	 * Since m0_sm is part of io_request, for any parity group
	 * which is partial, read-modify-write state transition is followed
	 * for all parity groups.
	 */
	if (ioo->ioo_map_idx == ioo->ioo_iomap_nr) {
		enum ioreq_state state;

		state = (op->op_code == M0_CLOVIS_OC_READ) ?
			IRS_READING : IRS_WRITING;

		if (state == IRS_WRITING) {
			if (op->op_code != M0_CLOVIS_OC_FREE) {
				rc = ioo->ioo_ops->iro_application_data_copy(ioo,
					CD_COPY_FROM_APP, 0);
				if (rc != 0) {
					M0_LOG(M0_ERROR, "iro_application_data_copy() "
							 "failed: rc=%d", rc);
					goto fail_locked;
				}
			}
			if (!m0_pdclust_is_replicated(play)) {
				rc = ioo->ioo_ops->iro_parity_recalc(ioo);
				if (rc != 0) {
					M0_LOG(M0_ERROR, "iro_parity_recalc() "
							"failed: rc=%d", rc);
					goto fail_locked;
				}
			}
		}

		ioreq_sm_state_set_locked(ioo, state);
		M0_ASSERT(ergo(op->op_code == M0_CLOVIS_OC_FREE,
			  ioreq_sm_state(ioo) == IRS_WRITING));
		if (op->op_code == M0_CLOVIS_OC_FREE) {
			ioreq_sm_state_set_locked(ioo, IRS_WRITE_COMPLETE);
			ioreq_sm_executed_post(ioo);
			goto out;
		}
		rc = ioo->ioo_nwxfer.nxr_ops->nxo_dispatch(&ioo->ioo_nwxfer);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "nxo_dispatch() failed: rc=%d", rc);
			goto fail_locked;
		}
	} else {
		struct target_ioreq *ti;
		uint32_t             seg;
		m0_bcount_t          read_pages = 0;

		m0_htable_for(tioreqht, ti, &ioo->ioo_nwxfer.nxr_tioreqs_hash) {
			for (seg = 0; seg < ti->ti_bufvec.ov_vec.v_nr; ++seg)
				if (ti->ti_pageattrs[seg] & PA_READ)
					++read_pages;
		} m0_htable_endfor;

		/* Read IO is issued only if byte count > 0. */
		if (read_pages > 0) {
			ioreq_sm_state_set_locked(ioo, IRS_READING);
			ioo->ioo_rmw_read_pages = read_pages;
			rc = ioo->ioo_nwxfer.nxr_ops->nxo_dispatch(
					&ioo->ioo_nwxfer);
			if (rc != 0) {
				M0_LOG(M0_ERROR,
				       "nxo_dispatch() failed: rc=%d", rc);
				goto fail_locked;
			}
		} else {
			/* Don't want the sm to complain (state transition)*/
			ioreq_sm_state_set_locked(ioo, IRS_READING);
			ioreq_sm_state_set_locked(ioo, IRS_READ_COMPLETE);

			/*
			 * If there is no READ IO issued, switch to
			 * ioreq iosm_handle_executed
			 */
			ioreq_sm_executed_post(ioo);
		}
	}
out:
	M0_LOG(M0_INFO, "nxr_bytes = %"PRIu64", copied_nr = %"PRIu64,
	       ioo->ioo_nwxfer.nxr_bytes, ioo->ioo_copied_nr);

	/* lock this as it isn't a locality group lock */
	m0_sm_group_lock(&op->op_sm_group);
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_LAUNCHED);
	m0_sm_group_unlock(&op->op_sm_group);

	M0_LEAVE();
	return;

fail_locked:
	ioo->ioo_rc = rc;
	ioreq_sm_failed_locked(ioo, rc);
	/* N.B. Failed is not a terminal state */
	ioreq_sm_state_set_locked(ioo, IRS_REQ_COMPLETE);

	/* fixed by commit 5a189beac81297ec9ea1cecf7016697aa02b0182 */
	ioo->ioo_nwxfer.nxr_ops->nxo_complete(&ioo->ioo_nwxfer, false);

	/* Move the operation state machine along */
	m0_sm_group_lock(&op->op_sm_group);
	m0_sm_fail(&op->op_sm, M0_CLOVIS_OS_FAILED, rc);
	m0_clovis_op_failed(op);
	m0_sm_group_unlock(&op->op_sm_group);

	M0_LOG(M0_ERROR, "ioreq_iosm_handle_launch failed");
	M0_LEAVE();
}

/**
 * AST callback scheduled once all the RPC replies have been received.
 * This is heavily based on m0t1fs/linux_kernel/file.c::ioreq_iosm_handle
 *
 * @param grp The statemachine group the ast was posted to.
 * @param ast The ast struct, used to find the m0_clovis_op_io.
 */
static void ioreq_iosm_handle_executed(struct m0_sm_group *grp,
				       struct m0_sm_ast *ast)
{
	int                       rc;
	bool                      rmw;
	struct m0_clovis         *instance;
	struct m0_clovis_op      *op;
	struct m0_clovis_op_io   *ioo;
	struct m0_pdclust_layout *play;

	M0_ENTRY("op_io:ast %p", ast);

	M0_PRE(grp != NULL);
	M0_PRE(ast != NULL);
	M0_PRE(m0_sm_group_is_locked(grp));
	ioo = bob_of(ast, struct m0_clovis_op_io, ioo_ast, &ioo_bobtype);
	M0_PRE_EX(m0_clovis_op_io_invariant(ioo));
	op = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0_clovis__op_instance(op);
	M0_PRE(instance != NULL);

	play = pdlayout_get(ioo);

	/* @todo Do error handling based on m0_sm::sm_rc. */
	/*
	 * Since m0_sm is part of io_request, for any parity group
	 * which is partial, read-modify-write state transition is followed
	 * for all parity groups.
	 */
	M0_LOG(M0_DEBUG, "map=%"PRIu64" map_nr=%"PRIu64,
	       ioo->ioo_map_idx, ioo->ioo_iomap_nr);
	rmw = ioo->ioo_map_idx != ioo->ioo_iomap_nr;
	if (ioreq_sm_state(ioo) == IRS_TRUNCATE_COMPLETE)
		goto done;
	if (!rmw) {
		enum ioreq_state state;

		state = op->op_code == M0_CLOVIS_OC_READ ?
			IRS_READ_COMPLETE: IRS_WRITE_COMPLETE;
		M0_ASSERT(ioreq_sm_state(ioo) == state);
		if (ioo->ioo_rc != 0) {
			rc = ioo->ioo_rc;
			M0_LOG(M0_DEBUG, "ioo->ioo_rc = %d", rc);
			goto fail_locked;
		}
		if (state == IRS_READ_COMPLETE) {
			/*
			 * Returns immediately if all devices are
			 * in healthy state.
			 */
			rc = ioo->ioo_ops->iro_dgmode_read(ioo, rmw);
			if (rc != 0) {
				M0_LOG(M0_INFO,
				       "iro_dgmode_read() returns error: %d",
				       rc);
				goto fail_locked;
			}

			/*
			 * If ioo's state has been changed to IRS_READING
			 * or IRS_DEGRADED_READING, this means iro_dgmode_read
			 * has just issue DGMODE IO, simply exit and it
			 * will re-entry here later. Otherwise proceed to
			 * read_verify and to copy data to APP.
			 */
			if (ioreq_sm_state(ioo) != IRS_READ_COMPLETE)
				goto out;

			rc = ioo->ioo_ops->iro_parity_verify(ioo);
			if (rc != 0) {
				M0_LOG(M0_ERROR,
				       "parity verification failed: rc=%d", rc);
				goto fail_locked;
			}

			if ((op->op_code == M0_CLOVIS_OC_READ &&
			     instance->m0c_config->cc_is_read_verify) &&
			     ioo->ioo_dgmap_nr > 0)
				rc = ioo->ioo_ops->iro_dgmode_recover(ioo);

			/* Valid data are available now, copy to application */
			rc = ioo->ioo_ops->iro_application_data_copy(ioo,
				CD_COPY_TO_APP, 0);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "iro_application_data_copy() "
						 "failed (to APP): rc=%d", rc);
				goto fail_locked;
			}
		} else {
			M0_ASSERT(state == IRS_WRITE_COMPLETE);

			/*
			 * Returns immediately if all devices are
			 * in healthy state.
			 */
			rc = ioo->ioo_ops->iro_dgmode_write(ioo, rmw);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "iro_dgmode_write() failed, "
						 "rc=%d", rc);
				goto fail_locked;
			}

			rc = truncate_dispatch(ioo);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "nxo_dispatch() failed: "
						 "rc=%d", rc);
				goto fail_locked;
			}

			if (ioreq_sm_state(ioo) != IRS_WRITE_COMPLETE)
				goto out;
		}
	} else {
		/*
		 * First stage of RMW done: ioo's state should be
		 * IRS_READ_COMPLETE when it reaches here.
		 */
		if (ioreq_sm_state(ioo) == IRS_READ_COMPLETE &&
		    op->op_code != M0_CLOVIS_OC_FREE) {
			/*
			 * If fops dispatch fails, we need to wait till all io
			 * fop callbacks are acked since IO fops have already
			 * been dispatched.
			 *
			 * Only fully modified pages from parity groups which
			 * have chosen read-rest approach or aligned parity
			 * groups, are copied since read-old approach needs
			 * reading of all spanned pages,(no matter fully
			 * modified or paritially modified) in order to
			 * calculate parity correctly.
			 */
			rc = ioo->ioo_ops->iro_application_data_copy(
				ioo, CD_COPY_FROM_APP,PA_FULLPAGE_MODIFY);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "iro_application_data_copy() "
				       "on FULLPAGE failed: rc=%d", rc);
				goto fail_locked;
			}

			/* Copies
			 * - fully modified pages from parity groups which have
			 *   chosen read_old approach and
			 * - partially modified pages from all parity groups.
			 */
			rc = ioo->ioo_ops->iro_application_data_copy(
				ioo, CD_COPY_FROM_APP, 0);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "iro_application_data_copy() "
						 "failed: rc=%d", rc);
				goto fail_locked;
			}
		}
		if (ioreq_sm_state(ioo) == IRS_READ_COMPLETE) {
			/* Finalizes the old read fops. */
			if (ioo->ioo_rmw_read_pages > 0) {
				ioo->ioo_nwxfer.nxr_ops->nxo_complete(
					&ioo->ioo_nwxfer, rmw);

				/*
				 * There is a subtle case for first write
				 * to an object when CROW optimisation is used:
				 * if it is a RMW write, it sends a read request
				 * first as Clovis doesn't have the concept of
				 * object size and an -ENOENT error will be
				 * returned as there isn't any thing exists in
				 * ios yet.
				 *
				 * Clovis has to trust the application that it
				 * has checked the existence of an object, so
				 * we can safely ignore the -ENOENT error here.
				 */
				if (ioo->ioo_rc == -ENOENT)
					ioreq_ioo_reset(ioo);
				else if (ioo->ioo_rc != 0) {
					M0_LOG(M0_ERROR, "ioo->ioo_rc=%d",
					       ioo->ioo_rc);

					rc = ioo->ioo_rc;
					goto fail_locked;
				}
				nw_xfer_device_state_reset(&ioo->ioo_nwxfer);
			}

			/* Prepare for the Write fops*/
			ioreq_sm_state_set_locked(ioo, IRS_WRITING);
			if (!m0_pdclust_is_replicated(play)) {
				rc = ioo->ioo_ops->iro_parity_recalc(ioo);
				if (rc != 0) {
					M0_LOG(M0_ERROR, "iro_parity_recalc()"
					       "failed: rc=%d", rc);
					goto fail_locked;
				}
			}

			rc = ioo->ioo_nwxfer.nxr_ops->nxo_dispatch(
					&ioo->ioo_nwxfer);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "nxo_dispatch() failed: "
						 "rc=%d", rc);
				goto fail_locked;
			}

			/*
			 * Simply return here as WRITE op will re-entry
			 * ioreq_iosm_handle_executed with different state.
			 */
			goto out;

		} else {
			/* 2nd stage of RMW done [WRITE] */
			M0_ASSERT(ioreq_sm_state(ioo) == IRS_WRITE_COMPLETE);

			/*
			 * Returns immediately if all devices are in healthy
			 * state.
			 */
			rc = ioo->ioo_ops->iro_dgmode_write(ioo, rmw);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "iro_dgmode_write() failed: "
						 "rc=%d", rc);
				goto fail_locked;
			}

			rc = truncate_dispatch(ioo);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "nxo_dispatch() failed: "
						 "rc=%d", rc);
				goto fail_locked;
			}

			if (ioreq_sm_state(ioo) != IRS_WRITE_COMPLETE)
				goto out;
		}
	}
done:
	ioo->ioo_nwxfer.nxr_ops->nxo_complete(&ioo->ioo_nwxfer, rmw);

#ifdef CLOVIS_FOR_M0T1FS
	/* XXX: TODO: update the inode size on the mds */
#endif

	if (rmw)
		ioreq_sm_state_set_locked(ioo, IRS_REQ_COMPLETE);

	/*
	 * Move the operation state machine along: due to the lack of
	 * mechanism in Mero to inform Clovis if data(or FOL) has been safely
	 * written to disk (this can be done by piggying back max commited tx
	 * id or explict syncing data), Clovis assumes data is safe when
	 * it receive all replies from ioservies at this moment (although it
	 * is not true) and moves the state of this 'op' to STABLE.
	 *
	 * Clovis introduced SYNC APIs to allow an application explictly to
	 * flush data to disks.
	 */

	m0_sm_group_lock(&op->op_sm_group);
	m0_sm_move(&op->op_sm, ioo->ioo_rc, M0_CLOVIS_OS_EXECUTED);
	m0_clovis_op_executed(op);
	if (M0_IN(op->op_code, (M0_CLOVIS_OC_READ, M0_CLOVIS_OC_WRITE,
				M0_CLOVIS_OC_FREE))) {
		m0_sm_move(&op->op_sm, ioo->ioo_rc, M0_CLOVIS_OS_STABLE);
		m0_clovis_op_stable(op);
	}
	m0_sm_group_unlock(&op->op_sm_group);

	/* Post-processing for object op. */
	m0_clovis__obj_op_done(op);

out:
	M0_LEAVE();
	return;

fail_locked:
	ioo->ioo_rc = rc;
	ioreq_sm_failed_locked(ioo, rc);
	/* N.B. Failed is not a terminal state */
	ioreq_sm_state_set_locked(ioo, IRS_REQ_COMPLETE);
	/* XXX: a hack to prevent kernel panic. how to do it correctly? */
#if 1 || BACKPORT_UPSTREAM_FIX
	ioo->ioo_nwxfer.nxr_ops->nxo_complete(&ioo->ioo_nwxfer, false);
#else
	ioo->ioo_nwxfer.nxr_state = NXS_COMPLETE;
#endif

	/* As per bug MERO-2575, rc will be reported in op->op_rc and the
	 * op will be completed with status M0_CLOVIS_OS_STABLE */
	op->op_rc = ioo->ioo_rc;
	/* Move the operation state machine along */
	m0_sm_group_lock(&op->op_sm_group);
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_EXECUTED);
	m0_clovis_op_executed(op);
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_STABLE);
	m0_clovis_op_stable(op);
	m0_sm_group_unlock(&op->op_sm_group);

	M0_LOG(M0_DEBUG, "ioreq_iosm_handle_executed failed, rc=%d", rc);
	M0_LEAVE();
	return;
}

/**
 * Destroys the parity group iomap, freeing all the data_bufs etc.
 * This is heavily based on m0t1fs/linux_kernel/file.c::ioreq_iomaps_destroy
 *
 * @param ioo The IO operation whose map should be destroyed.
 */
static void ioreq_iomaps_destroy(struct m0_clovis_op_io *ioo)
{
	uint64_t id;

	M0_ENTRY("op_io %p", ioo);

	M0_PRE(ioo != NULL);
	M0_PRE(ioo->ioo_iomaps != NULL);

	for (id = 0; id < ioo->ioo_iomap_nr; ++id) {
		if (ioo->ioo_iomaps[id] != NULL) {
			pargrp_iomap_fini(ioo->ioo_iomaps[id], ioo->ioo_obj);
			m0_free0(&ioo->ioo_iomaps[id]);
		}
	}
	m0_free0(&ioo->ioo_iomaps);
	ioo->ioo_iomap_nr = 0;

	M0_LEAVE();
}

static int ioreq_iomaps_parity_groups_cal(struct m0_clovis_op_io *ioo)
{
	uint64_t                  seg;
	uint64_t                  grp;
	uint64_t                  grpstart;
	uint64_t                  grpend;
	uint64_t                 *grparray;
	uint64_t                  grparray_sz;
	struct m0_pdclust_layout *play;

	M0_ENTRY();

	play = pdlayout_get(ioo);

	/* Array of maximum possible number of groups spanned by req. */
	grparray_sz = m0_vec_count(&ioo->ioo_ext.iv_vec) / data_size(play) +
		      2 * SEG_NR(&ioo->ioo_ext);
	M0_LOG(M0_DEBUG, "ioo=%p arr_sz=%"PRIu64, ioo, grparray_sz);
	M0_ALLOC_ARR(grparray, grparray_sz);
	if (grparray == NULL)
		return M0_ERR_INFO(-ENOMEM, "Failed to allocate memory"
			                    " for int array");
	/*
	 * Finds out total number of parity groups spanned by
	 * m0_clovis_op_io::ioo_ext.
	 */
	for (seg = 0; seg < SEG_NR(&ioo->ioo_ext); ++seg) {
		grpstart = group_id(INDEX(&ioo->ioo_ext, seg), data_size(play));
		grpend	 = group_id(seg_endpos(&ioo->ioo_ext, seg) - 1,
				    data_size(play));
		for (grp = grpstart; grp <= grpend; ++grp) {
			uint64_t i;
			/*
			 * grparray is a temporary array to record found groups.
			 * Scan this array for [grpstart, grpend].
			 * If not found, record it in this array and
			 * increase ir_iomap_nr.
			 */
			for (i = 0; i < ioo->ioo_iomap_nr; ++i) {
				if (grparray[i] == grp)
					break;
			}
			/* 'grp' is not found. Adding it to @grparray */
			if (i == ioo->ioo_iomap_nr) {
				M0_ASSERT_INFO(i < grparray_sz,
					"nr=%"PRIu64" size=%"PRIu64,
					i , grparray_sz);
				grparray[i] = grp;
				++ioo->ioo_iomap_nr;
			}
		}
	}
	m0_free(grparray);
	return M0_RC(0);
}

/**
 * Builds the iomaps parity group for all the groups covered this IO request.
 * This is heavily based on m0t1fs/linux_kernel/file.c::ioreq_iomaps_prepare
 *
 * @param ioo The IO operation whose pgmap should be built.
 * @return 0 for success, -errno otherwise.
 */
static int ioreq_iomaps_prepare(struct m0_clovis_op_io *ioo)
{
	int                       rc;
	uint64_t                  map;
	struct m0_pdclust_layout *play;
	struct m0_ivec_cursor     cursor;
	struct m0_bufvec_cursor   buf_cursor;
	bool                      bufvec = true;
	M0_ENTRY("op_io = %p", ioo);

	M0_PRE(ioo != NULL);
	play = pdlayout_get(ioo);

	rc = ioreq_iomaps_parity_groups_cal(ioo);
	if (rc != 0)
		return M0_RC(rc);

	if (ioo->ioo_oo.oo_oc.oc_op.op_code == M0_CLOVIS_OC_FREE)
		bufvec = false;
	M0_LOG(M0_DEBUG, "ioo=%p spanned_groups=%"PRIu64
			 " [N,K,us]=[%d,%d,%"PRIu64"]",
			 ioo, ioo->ioo_iomap_nr, layout_n(play),
			 layout_k(play), layout_unit_size(play));

	/* ioo->ioo_iomaps is zeroed out on allocation. */
	M0_ALLOC_ARR(ioo->ioo_iomaps, ioo->ioo_iomap_nr);
	if (ioo->ioo_iomaps == NULL) {
		rc = -ENOMEM;
		goto failed;
	}

	m0_ivec_cursor_init(&cursor, &ioo->ioo_ext);
	if (bufvec)
		m0_bufvec_cursor_init(&buf_cursor, &ioo->ioo_data);
	/*
	 * cursor is advanced maximum by parity group size in one iteration
	 * of this loop.
	 * This is done by pargrp_iomap::pi_ops::pi_populate().
	 */
	for (map = 0; !m0_ivec_cursor_move(&cursor, 0); ++map) {
		M0_ASSERT(map < ioo->ioo_iomap_nr);
		M0_ASSERT(ioo->ioo_iomaps[map] == NULL);
		M0_ALLOC_PTR(ioo->ioo_iomaps[map]);
		if (ioo->ioo_iomaps[map] == NULL) {
			rc = -ENOMEM;
			goto failed;
		}

		rc = pargrp_iomap_init(ioo->ioo_iomaps[map], ioo,
				       group_id(m0_ivec_cursor_index(&cursor),
						data_size(play)));
		if (rc != 0) {
			m0_free0(&ioo->ioo_iomaps[map]);
			goto failed;
		}

		/* @cursor is advanced in the following function */
		rc = ioo->ioo_iomaps[map]->pi_ops->
		     pi_populate(ioo->ioo_iomaps[map], &ioo->ioo_ext, &cursor,
				 bufvec ? &buf_cursor : NULL);
		if (rc != 0)
			goto failed;
		M0_LOG(M0_INFO, "pargrp_iomap id : %"PRIu64" populated",
		       ioo->ioo_iomaps[map]->pi_grpid);
	}

	return M0_RC(0);
failed:
	if (ioo->ioo_iomaps != NULL)
		ioreq_iomaps_destroy(ioo);

	return M0_ERR(rc);
}

/**
 * Copies at most one data buf to/from application memory.
 *
 * datacur is a cursor for the application buffers.
 * dir == CD_COPY_FROM_APP: app_datacur -> clovis_data
 * dir == CD_COPY_TO_APP: app_datacur <- clovis_data
 *
 * @param clovis_data The clovis memory buffer in the pg map.
 * @param app_datacur The application data buffers.
 * @param dir CD_COPY_TO_APP or CD_COPY_FROM_APP.
 * @return the number of bytes copied.
 */
static uint64_t data_buf_copy(struct data_buf          *clovis_data,
			      struct m0_bufvec_cursor  *app_datacur,
			      enum copy_direction       dir)
{
	void     *app_data;
	uint32_t  app_data_len;
	uint64_t  copied = 0;
	uint64_t  bytes;

	M0_ENTRY();

	M0_PRE(clovis_data != NULL);
	M0_PRE(app_datacur != NULL);
	M0_PRE_EX(data_buf_invariant(clovis_data));
	M0_PRE(M0_IN(dir, (CD_COPY_FROM_APP, CD_COPY_TO_APP)));

	bytes = clovis_data->db_buf.b_nob;
	while (bytes > 0) {
		app_data     = m0_bufvec_cursor_addr(app_datacur);
		app_data_len = m0_bufvec_cursor_step(app_datacur);

		/* Don't copy more bytes than we were supposed to */
		app_data_len = (app_data_len < bytes)?app_data_len:bytes;

		if (app_data == NULL)
			break;

		/* app_data == clovis_data->db_buf.b_addr implies zero copy */
		if (app_data != clovis_data->db_buf.b_addr) {
			if (dir == CD_COPY_FROM_APP)
				memcpy((char*)clovis_data->db_buf.b_addr +
				       copied, app_data, app_data_len);
			else
				memcpy(app_data,
				       (char*)clovis_data->db_buf.b_addr +
				       copied, app_data_len);
		}

		bytes  -= app_data_len;
		copied += app_data_len;

		if (m0_bufvec_cursor_move(app_datacur, app_data_len))
			break;
	}

	M0_LEAVE();
	return copied;
}

/**
 * Copies data for the block between applicaiton-provided and iomap buffers.
 * This is heavily based on m0t1fs/linux_kernel/file.c::user_data_copy
 *
 * @param map The parity group map in question.
 * @param obj The object this data corresponds to.
 * @param start The offset to start copying.
 * @param end The offset to stop copying.
 * @param datacur Where in the application-provided buffers we should operate.
 * @param dir CD_COPY_FROM_APP or CD_COPY_TO_APP
 * @param filter Flags that must be set when copying from the application.
 */
/** @todo reduce the number of arguments to this function, map+obj are in ioo */
static int clovis_application_data_copy(struct pargrp_iomap      *map,
					struct m0_clovis_obj     *obj,
					m0_bindex_t               start,
					m0_bindex_t               end,
					struct m0_bufvec_cursor  *datacur,
					enum copy_direction       dir,
					enum page_attr            filter)
{
	uint64_t                  bytes;
	uint32_t                  row = 0;
	uint32_t                  col = 0;
	uint32_t                  m_col;
	struct data_buf          *clovis_data;
	struct m0_pdclust_layout *play;
	struct m0_key_val        *key_val;
	m0_bindex_t               mask;
	m0_bindex_t               grp_size;

	M0_ENTRY("Copy %s application, start = %8"PRIu64", end = %8"PRIu64,
		 dir == CD_COPY_FROM_APP ? (char *)"from" : (char *)" to ",
		 start, end);

	M0_PRE(M0_IN(dir, (CD_COPY_FROM_APP, CD_COPY_TO_APP)));
	M0_PRE(map != NULL);
	M0_PRE(obj != NULL);
	M0_PRE(datacur != NULL);
	/* XXX: get rid of obj from the parameters */
	M0_PRE(map->pi_ioo->ioo_obj == obj);
	M0_PRE_EX(pargrp_iomap_invariant(map));
	M0_PRE(end > start);
	/* start/end are in the same object block */
	M0_PRE(start >> obj->ob_attr.oa_bshift ==
	       (end - 1) >> obj->ob_attr.oa_bshift);
	M0_PRE(datacur != NULL);

	play = pdlayout_get(map->pi_ioo);
	grp_size = data_size(play) * map->pi_grpid;
	/* Finds out the page from pargrp_iomap::pi_databufs. */
	page_pos_get(map, start, grp_size, &row, &col);
	/** If majority with replica be selected or not. */
	if (play->pl_attr.pa_K == 0 ||
	    m0_key_val_is_null(&map->pi_databufs[row][col]->db_maj_ele))
		clovis_data = map->pi_databufs[row][col];
	else {
		key_val = &map->pi_databufs[row][col]->db_maj_ele;
		m_col = *(uint32_t *)(key_val->kv_key.b_addr);
		if (m0_pdclust_unit_classify(play, m_col) == M0_PUT_DATA) {
			M0_ASSERT(m_col == 0);
			clovis_data = map->pi_databufs[row][m_col];
		} else if (m0_pdclust_unit_classify(play, m_col) ==
			   M0_PUT_PARITY)
			clovis_data = map->pi_paritybufs[row][m_col - 1];
		else
			/* No way of getting spares. */
			M0_IMPOSSIBLE();
	}
	M0_ASSERT(clovis_data != NULL);
	mask = ~SHIFT2MASK(obj->ob_attr.oa_bshift);

	/* Clovis only supports whole block operations */
	M0_ASSERT(end - start == clovis_data->db_buf.b_nob);

	if (dir == CD_COPY_FROM_APP) {
		if ((clovis_data->db_flags & filter) == filter) {
			if (clovis_data->db_flags & PA_COPY_FRMUSR_DONE) {
				m0_bufvec_cursor_move(datacur, end - start);
				return M0_RC(0);
			}

			/*
			 * Note: data has been read into auxiliary buffer
			 * directly for READOLD method.
			 */
			if (clovis_data->db_auxbuf.b_addr != NULL &&
			    map->pi_rtype == PIR_READOLD) {
				if (filter != 0) {
					m0_bufvec_cursor_move(
						datacur, end - start);
					return M0_RC(0);
				}
			}

			/* Copies to appropriate offset within page. */
			bytes = data_buf_copy(clovis_data, datacur, dir);
			M0_LOG(M0_DEBUG, "%"PRIu64
					 " bytes copied from application "
					 "from offset %"PRIu64, bytes, start);
			map->pi_ioo->ioo_copied_nr += bytes;

			/*
			 * application_data_copy() may be called to handle
			 * only part of PA_FULLPAGE_MODIFY page.
			 * In this case we should mark the page as done only
			 * when the last piece is processed.
			 * Otherwise, the rest piece of the page
			 * will be ignored.
			 */
			if (ergo(clovis_data->db_flags & PA_FULLPAGE_MODIFY,
				(end & mask) == 0))
				clovis_data->db_flags |= PA_COPY_FRMUSR_DONE;

			if (bytes != end - start)
				return M0_ERR_INFO(
					-EFAULT, "[%p] Failed to"
					" copy_from_user: %" PRIu64 " !="
					" %" PRIu64 " - %" PRIu64,
					map->pi_ioo, bytes, end, start);
		}
	} else {
		bytes = data_buf_copy(clovis_data, datacur, dir);

		map->pi_ioo->ioo_copied_nr += end - start - bytes;

		M0_LOG(M0_DEBUG, "%"PRIu64
		       " bytes copied to application from offset " "%"PRIu64,
		       bytes, start);

		if (bytes != end - start)
			return M0_ERR(-EFAULT);
	}

	return M0_RC(0);
}

/**
 * Copies the file-data between the iomap buffers and the application-provided
 * buffers, one row at a time.
 * This is heavily based on m0t1fs/linux_kernel/file.c::ioreq_user_data_copy
 *
 * @param ioo The io operation whose data should be copied.
 * @param dir CD_COPY_FROM_APP or CD_COPY_TO_APP
 * @param filter Flags that must be set when copying from the application.
 * @return 0 for success, -errno otherwise.
 */
static int ioreq_application_data_copy(struct m0_clovis_op_io *ioo,
				       enum copy_direction dir,
				       enum page_attr filter)
{
	int                       rc;
	uint64_t                  map;
	m0_bindex_t               grpstart;
	m0_bindex_t               grpend;
	m0_bindex_t               pgstart;
	m0_bindex_t               pgend;
	m0_bcount_t               count;
	struct m0_bufvec_cursor   appdatacur;
	struct m0_ivec_cursor     extcur;
	struct m0_pdclust_layout *play;

	M0_ENTRY("op_io : %p, %s application. filter = 0x%x", ioo,
		 dir == CD_COPY_FROM_APP ? (char *)"from" : (char *)"to",
		 filter);

	M0_PRE(M0_IN(dir, (CD_COPY_FROM_APP, CD_COPY_TO_APP)));
	M0_PRE_EX(m0_clovis_op_io_invariant(ioo));

	m0_bufvec_cursor_init(&appdatacur, &ioo->ioo_data);
	m0_ivec_cursor_init(&extcur, &ioo->ioo_ext);
	play = pdlayout_get(ioo);

	for (map = 0; map < ioo->ioo_iomap_nr; ++map) {
		M0_ASSERT_EX(pargrp_iomap_invariant(ioo->ioo_iomaps[map]));

		count    = 0;
		grpstart = data_size(play) * ioo->ioo_iomaps[map]->pi_grpid;
		grpend   = grpstart + data_size(play);

		while (!m0_ivec_cursor_move(&extcur, count) &&
			m0_ivec_cursor_index(&extcur) < grpend) {

			pgstart = m0_ivec_cursor_index(&extcur);
			pgend = min64u(m0_round_up(pgstart + 1,
						   m0_clovis__page_size(ioo)),
				       pgstart + m0_ivec_cursor_step(&extcur));
			count = pgend - pgstart;

			/*
			* This takes care of finding correct page from
			* current pargrp_iomap structure from pgstart
			* and pgend.
			*/
			rc = clovis_application_data_copy(
				ioo->ioo_iomaps[map], ioo->ioo_obj,
				pgstart, pgend, &appdatacur, dir, filter);
			if (rc != 0)
				return M0_ERR_INFO(
					rc, "[%p] Copy failed (pgstart=%" PRIu64
					" pgend=%" PRIu64 ")",
					ioo, pgstart, pgend);
		}
	}

	return M0_RC(0);
}

/**
 * Recalculates the parity for each row of this operations io map.
 * This is heavily based on m0t1fs/linux_kernel/file.c::ioreq_partiy_recalc
 *
 * @param ioo The io operation in question.
 * @return 0 for success, -errno otherwise.
 */
static int ioreq_parity_recalc(struct m0_clovis_op_io *ioo)
{
	int      rc = 0;
	uint64_t map;

	M0_ENTRY("io_request : %p", ioo);
	M0_PRE_EX(m0_clovis_op_io_invariant(ioo));

	m0_semaphore_down(&clovis_cpus_sem);

	for (map = 0; map < ioo->ioo_iomap_nr; ++map) {
		rc = ioo->ioo_iomaps[map]->pi_ops->pi_parity_recalc(ioo->
				ioo_iomaps[map]);
		if (rc != 0)
			break;
	}

	m0_semaphore_up(&clovis_cpus_sem);

	return rc == 0 ? M0_RC(rc) :
		M0_ERR_INFO(rc, "Parity recalc failed for grpid=%3"PRIu64,
				 ioo->ioo_iomaps[map]->pi_grpid);
}

/**
 * Reconstructs the missing data of parity groups.
 * This is heavily based on m0t1fs/linux_kernel/file.c::ioreq_dgmode_recover
 *
 * @param ioo The io operation in question.
 * @return 0 for success, -errno otherwise.
 */
static int ioreq_dgmode_recover(struct m0_clovis_op_io *ioo)
{
	struct m0_pdclust_layout  *play;
	int                        rc = 0;
	uint64_t                   cnt;

	M0_ENTRY();
	M0_PRE_EX(m0_clovis_op_io_invariant(ioo));
	M0_PRE(ioreq_sm_state(ioo) == IRS_READ_COMPLETE);

	play = pdlayout_get(ioo);
	for (cnt = 0; cnt < ioo->ioo_iomap_nr; ++cnt) {
		if (ioo->ioo_iomaps[cnt]->pi_state == PI_DEGRADED) {
			if (m0_pdclust_is_replicated(play)) {
				rc = ioo->ioo_iomaps[cnt]->pi_ops->
				      pi_replica_recover(ioo->ioo_iomaps[cnt]);
			} else
				rc = ioo->ioo_iomaps[cnt]->pi_ops->
					pi_dgmode_recover(ioo->ioo_iomaps[cnt]);
			if (rc != 0)
				return M0_ERR(rc);
		}
	}

	return M0_RC(rc);
}

/**
 * @todo  This code is not required once MERO-899 lands into master.
 * Returns true if a given session is already marked as failed. In case
 * a session is not already marked for failure, the functions marks it
 * and returns false.
 */
static bool is_session_marked(struct m0_clovis_op_io *ioo,
			      struct m0_rpc_session *session)
{
	uint64_t i;
	uint64_t max_failures;
	uint64_t session_id;

	session_id = session->s_session_id;
	max_failures = tolerance_of_level(ioo, M0_CONF_PVER_LVL_CTRLS);
	for (i = 0; i < max_failures; ++i) {
		if (ioo->ioo_failed_session[i] == session_id)
			return M0_RC(true);
		else if (ioo->ioo_failed_session[i] == ~(uint64_t)0) {
			ioo->ioo_failed_session[i] = session_id;
			return M0_RC(false);
		}
	}
	return M0_RC(false);
}

/**
 * Returns number of failed devices or -EIO if number of failed devices exceeds
 * the value of K (number of spare devices in parity group). Once MERO-899 lands
 * into master the code for this function will change. In that case it will only
 * check if a given pool is dud.
 *
 * This is heavily based on m0t1fs/linux_kernel/file.c::device_check
 */
static int device_check(struct m0_clovis_op_io *ioo)
{
	int                       rc = 0;
	uint32_t                  fdev_nr = 0;
	uint32_t                  fsvc_nr = 0;
	uint64_t                  max_failures;
	enum m0_pool_nd_state     state;
	struct target_ioreq      *ti;
	struct m0_pdclust_layout *play;
	struct m0_clovis         *instance;
	struct m0_poolmach       *pm;
	struct m0_pool_version   *pv;

	M0_ENTRY();
	M0_PRE(ioo != NULL);
	M0_PRE(M0_IN(ioreq_sm_state(ioo),
		     (IRS_READ_COMPLETE, IRS_WRITE_COMPLETE)));

	instance = m0_clovis__op_instance(&ioo->ioo_oo.oo_oc.oc_op);
	play = pdlayout_get(ioo);
	max_failures = tolerance_of_level(ioo, M0_CONF_PVER_LVL_CTRLS);

	pv = m0_pool_version_find(&instance->m0c_pools_common, &ioo->ioo_pver);
	M0_ASSERT(pv != NULL);
	pm = &pv->pv_mach;

	m0_htable_for (tioreqht, ti, &ioo->ioo_nwxfer.nxr_tioreqs_hash) {
		rc = m0_poolmach_device_state(pm, ti->ti_obj, &state);
		if (rc != 0)
			return M0_ERR(rc);

		ti->ti_state = state;
		if (ti->ti_rc == -ECANCELED) {
			/* The case when a particular service is down. */
			if (!is_session_marked(ioo, ti->ti_session)) {
				M0_CNT_INC(fsvc_nr);
			}
		} else if (M0_IN(state, (M0_PNDS_FAILED, M0_PNDS_OFFLINE,
			   M0_PNDS_SNS_REPAIRING, M0_PNDS_SNS_REPAIRED)) &&
			   !is_session_marked(ioo, ti->ti_session)) {
			/*
			 * The case when multiple devices under the same service
			 * are unavailable.
			 */
			M0_CNT_INC(fdev_nr);
		}

	} m0_htable_endfor;

	M0_LOG(M0_DEBUG, "failed devices = %d\ttolerance=%d", (int)fdev_nr,
		         (int)layout_k(play));
	if (is_pver_dud(fdev_nr, layout_k(play), fsvc_nr, max_failures))
		return M0_ERR_INFO(-EIO, "[%p] Failed to recover data "
				"since number of failed data units "
				"(%lu) exceeds number of parity "
				"units in parity group (%lu) OR "
				"number of failed services (%lu) "
				"exceeds number of max failures "
				"supported (%lu)",
				ioo, (unsigned long)fdev_nr,
				(unsigned long)layout_k(play),
				(unsigned long)fsvc_nr,
				(unsigned long)max_failures);
	return M0_RC(fdev_nr);
}

/**
 * Degraded mode read for a Clovis IO operation.
 * This is heavily based on m0t1fs/linux_kernel/file.c::ioreq_dgmode_read
 *
 * @param ioo The io operation in question.
 * @param rmw The flag for Read-Modify-Write (RMW).
 * @return 0 for success, -errno otherwise.
 */
static int ioreq_dgmode_read(struct m0_clovis_op_io *ioo, bool rmw)
{
	int                     rc       = 0;
	uint64_t                id;
	struct nw_xfer_request *xfer;
	struct ioreq_fop       *irfop;
	struct target_ioreq    *ti;
	enum m0_pool_nd_state   state;
	struct m0_poolmach     *pm;

	M0_ENTRY();
	M0_PRE_EX(m0_clovis_op_io_invariant(ioo));

	/*
	 * Note: If devices are in the state of M0_PNDS_SNS_REPARED, the op
	 * 'ioo' switchs back to IRS_READING state (see the code below
	 * ['else' part of 'ir_dgmap_nr > 0'] and comments in dgmode_process).
	 * How to tell if an op is doing normal or degraded io so that to avoid
	 * multiple entries of (or a loop) ioreq_dgmode_read? A flag
	 * 'ioo_dgmode_io_sent' is used here!
	 */
	if (ioo->ioo_dgmode_io_sent == true) {
		/*
		 * Recovers lost data using parity recovery algorithms
		 * only if one or more devices were in FAILED, OFFLINE,
		 * REPAIRING state.
		 */
		if (ioo->ioo_dgmap_nr > 0)
			rc = ioo->ioo_ops->iro_dgmode_recover(ioo);

		return M0_RC(rc);
	}
	/*
	 * If all devices are ONLINE, all requests return success.
	 * In case of read before write, due to CROW, COB will not be present,
	 * resulting into ENOENT error.
	 */
	xfer = &ioo->ioo_nwxfer;
	if (xfer->nxr_rc == 0 || xfer->nxr_rc == -ENOENT)
		return M0_RC(xfer->nxr_rc);

	/*
	 * Number of failed devices is not a criteria good enough
	 * by itself. Even if one/more devices failed but IO request
	 * could complete if IO request did not send any pages to
	 * failed device(s) at all.
	 */
	rc = device_check(ioo);
	if (rc < 0)
		return M0_RC(rc);

	pm = clovis_ioo_to_poolmach(ioo);
	M0_ASSERT(pm != NULL);

	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		/*
		 * Data was retrieved successfully, so no need to check the
		 * state of the device.
		 */
		if (ti->ti_rc == 0)
			continue;

		/* state is already queried in device_check() and stored
		 * in ti->ti_state. Why do we do this again?
		 */
		rc = m0_poolmach_device_state(
			pm, ti->ti_obj, &state);
		if (rc != 0)
			return M0_ERR(rc);
		M0_LOG(M0_INFO, "device state for "FID_F" is %d",
		       FID_P(&ti->ti_fid), state);
		ti->ti_state = state;

		if (!M0_IN(state, (M0_PNDS_FAILED, M0_PNDS_OFFLINE,
			   M0_PNDS_SNS_REPAIRING, M0_PNDS_SNS_REPAIRED,
			   M0_PNDS_SNS_REBALANCING)))
			continue;
		/*
		 * Finds out parity groups for which read IO failed and marks
		 * them as DEGRADED. This is necessary since read IO request
		 * could be reading only a part of a parity group but if it
		 * failed, rest of the parity group also needs to be read
		 * (subject to file size) in order to re-generate lost data.
		 */
		m0_tl_for (iofops, &ti->ti_iofops, irfop) {
			rc = ioreq_fop_dgmode_read(irfop);
			if (rc != 0)
				break;
		} m0_tl_endfor;
	} m0_htable_endfor;

	if (rc != 0)
		return M0_ERR_INFO(rc, "[%p] dgmode failed", ioo);

	M0_LOG(M0_DEBUG, "[%p] dgmap_nr=%u is in dgmode",
	       ioo, ioo->ioo_dgmap_nr);
	/*
	 * Starts processing the pages again if any of the parity groups
	 * spanned by input IO-request is in degraded mode.
	 */
	if (ioo->ioo_dgmap_nr > 0) {
		M0_LOG(M0_WARN, "[%p]Process the failed parity groups", ioo);
		if (ioreq_sm_state(ioo) == IRS_READ_COMPLETE)
			ioreq_sm_state_set_locked(ioo, IRS_DEGRADED_READING);
		for (id = 0; id < ioo->ioo_iomap_nr; ++id) {
			rc = ioo->ioo_iomaps[id]->pi_ops->
			        pi_dgmode_postprocess(ioo->ioo_iomaps[id]);
			if (rc != 0)
				break;
		}
	} else {
		M0_ASSERT(ioreq_sm_state(ioo) == IRS_READ_COMPLETE);
		ioreq_sm_state_set_locked(ioo, IRS_READING);
		/*
		 * By this time, the page count in target_ioreq::ti_ivec and
		 * target_ioreq::ti_bufvec is greater than 1, but it is
		 * invalid since the distribution[Sining: layout] is about to
		 * change.
		 * Ergo, page counts in index and buffer vectors are reset.
		 */

		m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
			ti->ti_ivec.iv_vec.v_nr = 0;
		} m0_htable_endfor;
	}

	xfer->nxr_ops->nxo_complete(xfer, rmw);

	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		ti->ti_databytes = 0;
		ti->ti_parbytes  = 0;
		ti->ti_rc        = 0;
	} m0_htable_endfor;

	/* Resets the status code before starting degraded mode read IO. */
	ioo->ioo_rc = xfer->nxr_rc = 0;

	rc = xfer->nxr_ops->nxo_distribute(xfer);
	if (rc != 0)
		return M0_ERR(rc);

	rc = xfer->nxr_ops->nxo_dispatch(xfer);
	if (rc != 0)
		return M0_ERR(rc);
	ioo->ioo_dgmode_io_sent = true;

	return M0_RC(rc);
}

/**
 * Degraded mode write for a Clovis IO operation.
 * This is heavily based on m0t1fs/linux_kernel/file.c::ioreq_dgmode_write
 *
 * @param ioo The io operation in question.
 * @param rmw The flag for Read-Modify-Write (RMW).
 * @return 0 for success, -errno otherwise.
 */
static int ioreq_dgmode_write(struct m0_clovis_op_io *ioo, bool rmw)
{
	int                      rc;
	struct target_ioreq     *ti;
	struct nw_xfer_request  *xfer;
	struct m0_clovis        *instance;

	M0_ENTRY();
	M0_PRE_EX(m0_clovis_op_io_invariant(ioo));

	instance = m0_clovis__op_instance(&ioo->ioo_oo.oo_oc.oc_op);
	xfer = &ioo->ioo_nwxfer;

	/* See the comments in ioreq_dgmode_read */
	if (ioo->ioo_dgmode_io_sent)
		return M0_RC(xfer->nxr_rc);

	/* -E2BIG: see commit 52c1072141d*/
	/* In oostore mode we do not enter the degraded mode write. */
	if (m0_clovis__is_oostore(instance) ||
	    M0_IN(xfer->nxr_rc, (0, -E2BIG)))
		return M0_RC(xfer->nxr_rc);

	rc = device_check(ioo);
	if (rc < 0)
		return M0_RC(rc);

	/*
	 * This IO request has already acquired distributed lock on the
	 * file by this time.
	 * Degraded mode write needs to handle 2 prime use-cases.
	 * 1. SNS repair still to start on associated global fid.
	 * 2. SNS repair has completed for associated global fid.
	 * Both use-cases imply unavailability of one or more devices.
	 *
	 * In first use-case, repair is yet to start on file. Hence,
	 * rest of the file data which goes on healthy devices can be
	 * written safely.
	 * In this case, the fops meant for failed device(s) will be simply
	 * dropped and rest of the fops will be sent to respective ioservice
	 * instances for writing data to servers.
	 * Later when this IO request relinquishes the distributed lock on
	 * associated global fid and SNS repair starts on the file, the lost
	 * data will be regenerated using parity recovery algorithms.
	 *
	 * The second use-case implies completion of SNS repair for associated
	 * global fid and the lost data is regenerated on distributed spare
	 * units.
	 * Ergo, all the file data meant for lost device(s) will be redirected
	 * towards corresponding spare unit(s). Later when SNS rebalance phase
	 * commences, it will migrate the data from spare to a new device, thus
	 * making spare available for recovery again.
	 * In this case, old fops will be discarded and all pages spanned by
	 * IO request will be reshuffled by redirecting pages meant for
	 * failed device(s) to its corresponding spare unit(s).
	 */
	ioreq_sm_state_set_locked(ioo, IRS_DEGRADED_WRITING);

	/*
	 * Finalizes current fops which are not valid anymore.
	 * Fops need to be finalized in either case since old network buffers
	 * from IO fops are still enqueued in transfer machine and removal
	 * of these buffers would lead to finalization of rpc bulk object.
	 */
	xfer->nxr_ops->nxo_complete(xfer, rmw);

	/*
	 * Resets count of data bytes and parity bytes along with
	 * return status.
	 * Fops meant for failed devices are dropped in
	 * nw_xfer_req_dispatch().
	 */
	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		ti->ti_databytes = 0;
		ti->ti_parbytes  = 0;
		ti->ti_rc        = 0;
		ti->ti_req_type  = TI_NONE;
	} m0_htable_endfor;

	/*
	 * Redistributes all pages by routing pages for failed devices
	 * to spare units for each parity group.
	 */
	rc = xfer->nxr_ops->nxo_distribute(xfer);
	if (rc != 0)
		return M0_ERR_INFO(rc, "Failed to prepare dgmode write fops");

	xfer->nxr_rc = 0;
	ioo->ioo_rc = 0;

	rc = xfer->nxr_ops->nxo_dispatch(xfer);
	if (rc != 0)
		return M0_ERR_INFO(rc, "Failed to dispatch degraded mode"
			       "write IO fops");

	ioo->ioo_dgmode_io_sent = true;

	return M0_RC(xfer->nxr_rc);
}

static int ioreq_parity_verify(struct m0_clovis_op_io *ioo)
{
	struct pargrp_iomap      *iomap = NULL;
	struct m0_pdclust_layout *play;
	struct m0_clovis         *instance;
	struct m0_clovis_op      *op;
	int                       rc = 0;
	uint64_t                  grp;

	M0_ENTRY("m0_clovis_op_io : %p", ioo);
	M0_PRE_EX(m0_clovis_op_io_invariant(ioo));

	op = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0_clovis__op_instance(op);
	play = pdlayout_get(ioo);

	if (!(op->op_code == M0_CLOVIS_OC_READ &&
	      instance->m0c_config->cc_is_read_verify))
		return M0_RC(0);

	m0_semaphore_down(&clovis_cpus_sem);

	for (grp = 0; grp < ioo->ioo_iomap_nr; ++grp) {
		iomap = ioo->ioo_iomaps[grp];
		if (iomap->pi_state == PI_DEGRADED) {
			/* data is recovered from existing data and parity.
			 * It's meaningless to do parity verification */
			continue;
		}
		if (m0_pdclust_is_replicated(play))
			rc = iomap->pi_ops->pi_parity_replica_verify(iomap);
		else
			rc = iomap->pi_ops->pi_parity_verify(iomap);
		if (rc != 0)
			break;
	}

	m0_semaphore_up(&clovis_cpus_sem);
	return rc != 0 ? M0_ERR_INFO(rc, "Parity verification failed for "
					 "grpid=%"PRIu64,
					 iomap->pi_grpid) : M0_RC(rc);
}
/* XXX (Sining): should we rename ioreq_xxx to ioo_xxx?*/
const struct m0_clovis_op_io_ops ioo_ops = {
	.iro_iomaps_prepare        = ioreq_iomaps_prepare,
	.iro_iomaps_destroy        = ioreq_iomaps_destroy,
	.iro_application_data_copy = ioreq_application_data_copy,
	.iro_parity_recalc         = ioreq_parity_recalc,
	.iro_parity_verify         = ioreq_parity_verify,
	.iro_iosm_handle_launch    = ioreq_iosm_handle_launch,
	.iro_iosm_handle_executed  = ioreq_iosm_handle_executed,
	.iro_dgmode_read           = ioreq_dgmode_read,
	.iro_dgmode_write          = ioreq_dgmode_write,
	.iro_dgmode_recover        = ioreq_dgmode_recover,
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
