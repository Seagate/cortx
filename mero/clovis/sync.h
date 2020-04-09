/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED, A SEAGATE COMPANY
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
 * Original authors: Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Revision:         Pratik Shinde   <pratik.shinde@seagate.com>
 *
 * 27-Mar-2017: Modified by Sining and Pratik to support SYNC on index and ops.
 * 23-Jun-2015: Modified by Sining from m0t1fs/linux_kernel/fsync.c for object.
 * 11-Apr-2014: Original created for fsync in m0t1fs.
 */

#include "mdservice/fsync_fops.h"       /* m0_fop_fsync_mds_fopt */
#include "cas/cas.h"                    /* m0_fop_fsync_cas_fopt */
#include "fop/fop.h"                    /* m0_fop */

#pragma once

#ifndef __MERO_CLOVIS_OSYNC_H__
#define __MERO_CLOVIS_OSYNC_H__

/**
 * Experimental: sync operation for objects (sync for short).
 * This is heavily based on the m0t1fs::fsync work, see fsync DLD
 * for details. Clovis re-uses the fsync fop defined in m0t1fs.
 */

/* import */
struct m0_clovis_obj;
struct m0_clovis_op_sync;
struct m0_reqh_service_ctx;

M0_TL_DESCR_DECLARE(spti, M0_EXTERN);
M0_TL_DECLARE(spti, M0_EXTERN, struct m0_reqh_service_txid);

enum clovis_sync_type {
	CLOVIS_SYNC_ENTITY = 0,
	CLOVIS_SYNC_OP,
	CLOVIS_SYNC_INSTANCE
};

/**
 * The entity or op an SYNC request is going to sync.
 */
struct clovis_sync_target {
	uint32_t                         srt_type;
	union {
		struct m0_clovis_entity *srt_ent;
		struct m0_clovis_op     *srt_op;
	} u;

	/* Link to an SYNC request list. */
	struct m0_tlink                  srt_tlink;
	uint64_t                         srt_tlink_magic;
};

struct clovis_sync_request {
	/* Back pointer to sync_op. */
	struct m0_clovis_op_sync        *sr_op_sync;

	/** List of targets to sync. */
	struct m0_tl                     sr_targets;

	/** List of {service, txid} pairs constructed from all targets. */
	struct m0_mutex                  sr_stxs_lock;
	struct m0_tl                     sr_stxs;

	/**
	 * Records the number of FSYNC fops and fops(wrpper).
	 * sr_nr_fops seems redundant here but the purpose is to avoid
	 * scanning the list to get the length of sr_fops.
	 */
	struct m0_mutex                  sr_fops_lock;
	struct m0_tl                     sr_fops;
	int32_t                          sr_nr_fops;


	/* Post an AST when all fops of this SYNC request are done. */
	struct m0_sm_ast                 sr_ast;

	int32_t                          sr_rc;
};

/**
 * Wrapper for sync messages, used to list/group pending replies
 * and pair fop/reply with the struct m0_reqh_service_txid
 * that needs updating.
 */
struct clovis_sync_fop_wrapper {
	/** The fop for fsync messages */
	struct m0_fop                sfw_fop;

	/**
	 * The service transaction that needs updating
	 * gain the m0t1fs_inode::ci_pending_txid_lock lock
	 * for inodes or the m0_reqh_service_ctx::sc_max_pending_tx_lock
	 * for the super block before dereferencing
	 */
	struct m0_reqh_service_txid *sfw_stx;

	struct clovis_sync_request  *sfw_req;

	/* AST to handle when receiving reply fop. */
	struct m0_sm_ast             sfw_ast;

	/* Link to FSYNC fop list in a request. */
	struct m0_tlink              sfw_tlink;
	uint64_t                     sfw_tlink_magic;
};

/**
 * Ugly abstraction of clovis_sync interactions with wider mero code
 * - purely to facilitate unit testing.
 * - this is used in sync.c and its unit tests.
 */
struct clovis_sync_interactions {
	int (*si_post_rpc)(struct m0_rpc_item *item);
	int (*si_wait_for_reply)(struct m0_rpc_item *item, m0_time_t timeout);
	void (*si_fop_fini)(struct m0_fop *fop);
	void (*si_fop_put)(struct m0_fop *fop);
};

/**
 * Updates sync records in fop callbacks.
 * Service must be specified, one or both of csb/inode should be specified.
 * new_txid may be null.
 */
M0_INTERNAL void
clovis_sync_record_update(struct m0_reqh_service_ctx *service,
			   struct m0_clovis_entity   *obj,
			   struct m0_clovis_op       *op,
			   struct m0_be_tx_remid     *btr);

/**
 * Return first entity from sync operation.
 * It is used as helper function to get clovis instance from
 * entity for sync operation.
 */
M0_INTERNAL struct m0_clovis_entity *
m0_clovis__op_sync_entity(const struct m0_clovis_op *op);

#endif /* __MERO_CLOVIS_OSYNC_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
