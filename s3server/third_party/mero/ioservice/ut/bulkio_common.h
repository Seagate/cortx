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
 * Original author: Anand Vidwansa <Anand_Vidwansa@xyratex.com>
 * Original creation date: 02/21/2012
 */

#pragma once

#ifndef __MERO_IOSERVICE_ST_COMMON_H__
#define __MERO_IOSERVICE_ST_COMMON_H__

#include "lib/list.h"
#include "mero/init.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "cob/cob.h"            /* m0_cob_domain */
#include "ioservice/io_fops.h"  /* m0_io_fop */
#include "rpc/rpc.h"            /* m0_rpc_bulk, m0_rpc_bulk_buf */
#include "rpc/rpc_opcodes.h"    /* M0_RPC_OPCODES */
#include "lib/thread.h"         /* M0_THREAD_INIT */
#include "lib/misc.h"           /* M0_SET_ARR0 */

enum IO_UT_VALUES {
	IO_FIDS_NR           = 2,
	IO_SEGS_NR           = 16,
	IO_SEG_SIZE          = 4096,
	IO_SEQ_LEN           = 8,
	IO_FOPS_NR           = 16,
	IO_FID_SINGLE        = 1,
	IO_FOP_SINGLE        = 1,
	IO_XPRT_NR           = 1,
	IO_CLIENT_SVC_ID     = 2,
	IO_SERVER_SVC_ID     = 1,
	IO_ADDR_LEN          = 32,
	IO_STR_LEN           = 16,
	IO_SEG_STEP          = 64,
	IO_RPC_ITEM_TIMEOUT  = 300,
	IO_SEG_START_OFFSET  = IO_SEG_SIZE,
	IO_CLIENT_COBDOM_ID  = 21,
	IO_SERVER_COBDOM_ID  = 29,
	IO_RPC_MAX_IN_FLIGHT = 32,
};
#define IO_CLIENT_DBNAME   "bulk_c_db"
#define IO_SERVER_DBFILE   "bulkio_st.db"
#define IO_SERVER_LOGFILE  "bulkio_st.log"
#define IO_SERVER_STOBFILE "bulk_st_stob"

static const struct m0_fid CONF_PVER_FID = M0_FID_TINIT('v', 1, 24);
static const struct m0_fid CONF_PVER_FID1 = M0_FID_TINIT('v', 1, 25);
static const struct m0_fid CONF_PROFILE_FID = M0_FID_TINIT('p', 1, 0);
/* Structure containing data needed for UT. */
struct bulkio_params {
	/* Fids of global files. */
	struct m0_fid              bp_fids[IO_FIDS_NR];

	/* Tracks offsets for global fids. */
	uint64_t                   bp_offsets[IO_FIDS_NR];

	/* In-memory fops for read IO. */
	struct m0_io_fop         **bp_rfops;

	/* In-memory fops for write IO. */
	struct m0_io_fop         **bp_wfops;

	int                        bp_rfops_nr;
	int                        bp_wfops_nr;

	/* Read buffers to which data will be transferred. */
	struct m0_net_buffer     **bp_iobuf;

	/* Threads to post rpc items to rpc layer. */
	struct m0_thread         **bp_threads;

	/*
	 * Standard buffers containing a data pattern.
	 * Primarily used for data verification in read and write IO.
	 */
	char                      *bp_readbuf;
	char                      *bp_writebuf;

	/* Structures used by client-side rpc code. */
	struct m0_cob_domain       bp_ccbdom;
	struct m0_net_domain       bp_cnetdom;

	const char                *bp_caddr;
	char                      *bp_cdbname;
	const char                *bp_saddr;
	char                      *bp_slogfile;

	struct m0_rpc_client_ctx  *bp_cctx;
	struct m0_rpc_server_ctx  *bp_sctx;

	struct m0_net_xprt        *bp_xprt;

	struct m0_rm_domain        bp_rdom;
	struct m0_rm_resource_type bp_flock_rt;
	struct m0_file             bp_file[IO_FIDS_NR];

	/** Transaction ID included in the last reply received */
	struct m0_be_tx_remid      bp_remid;
};

/* A structure used to pass as argument to io threads. */
struct thrd_arg {
	/* Index in fops array to be posted to rpc layer. */
	int                   ta_index;
	/* Type of fop to be sent (read/write). */
	enum M0_RPC_OPCODES   ta_op;
	/* bulkio_params structure which contains common data. */
	struct bulkio_params *ta_bp;
};

/* Common APIs used by bulk client as well as UT code. */
int bulkio_client_start(struct bulkio_params *bp, const char *caddr,
			const char *saddr);

void bulkio_client_stop(struct m0_rpc_client_ctx *cctx);

int bulkio_server_start(struct bulkio_params *bp, const char *saddr);

void bulkio_server_stop(struct m0_rpc_server_ctx *sctx);

void bulkio_params_init(struct bulkio_params *bp);

void bulkio_params_fini(struct bulkio_params *bp);

void bulkio_test(struct bulkio_params *bp, int fids_nr, int fops_nr,
		 int segs_nr);

extern int m0_bufvec_alloc_aligned(struct m0_bufvec *bufvec, uint32_t num_segs,
				   m0_bcount_t seg_size, unsigned shift);

/**
 * Sends an fsync fop request through the session provided within an io
 * thread args struct.
 * @param remid Remote ID of the transaction that is to be fsynced.
 * @param t Bulkio parameters.
 * @return the rc included in the fsync fop reply.
 */
int io_fsync_send_fop(struct m0_be_tx_remid *remid, struct thrd_arg *t);
void io_fops_rpc_submit(struct thrd_arg *t);

void io_fops_destroy(struct bulkio_params *bp);

void io_fops_create(struct bulkio_params *bp, enum M0_RPC_OPCODES op,
		    int fids_nr, int fops_nr, int segs_nr);

void cob_attr_default_fill(struct m0_cob_attr *attr);
#endif /* __MERO_IOSERVICE_ST_COMMON_H__ */
