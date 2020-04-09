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
 * Original creation date: 02/21/2011
 */

#include "bulkio_common.h"
#include "rpc/rpclib.h"             /* m0_rpc_server_ctx */
#include "ioservice/fid_convert.h"  /* m0_fid_gob_make */
#include "mdservice/fsync_fops.h"   /* m0_fop_fsync */
#include "ut/misc.h"                /* M0_UT_PATH */
#include "ut/ut.h"

#define S_DBFILE        "bulkio_st.db"
#define S_STOBFILE      "bulkio_st_stob"
#define S_ADDB_STOBFILE "linuxstob:bulkio_st_addb_stob"

/**
   @todo This value can be reduced after multiple message delivery in a
    single buffer is supported.
 */

M0_TL_DESCR_DECLARE(rpcbulk, M0_EXTERN);

extern struct m0_reqh_service_type m0_ioservice_type;

#ifndef __KERNEL__
static char **server_argv_alloc(const char *server_ep_addr, int *argc)
{
	enum { STRLEN = 16 };
	int         n;
	char      **ret;
	char        ep[IO_ADDR_LEN];
	char        tm_len[STRLEN];
	char        rpc_size[STRLEN];
	const char *argv[] = {
		"bulkio_st", "-T", "AD", "-D", S_DBFILE,
		"-S", S_STOBFILE, "-A", S_ADDB_STOBFILE, "-e", ep,
		"-q", tm_len, "-m", rpc_size, "-w", "10", "-G", ep,
		"-f", M0_UT_CONF_PROCESS, "-H", server_ep_addr,
		"-c", M0_UT_PATH("diter.xc")
	};

	n = snprintf(ep, sizeof ep, "lnet:%s", server_ep_addr);
	M0_ASSERT(n < sizeof ep);

	n = snprintf(tm_len, sizeof tm_len, "%d", M0_NET_TM_RECV_QUEUE_DEF_LEN);
	M0_ASSERT(n < sizeof tm_len);

	n = snprintf(rpc_size, sizeof rpc_size, "%d",
		     M0_RPC_DEF_MAX_RPC_MSG_SIZE);
	M0_ASSERT(n < sizeof rpc_size);

	M0_ALLOC_ARR(ret, ARRAY_SIZE(argv));
	M0_ASSERT(ret != NULL);

	for (n = 0; n < ARRAY_SIZE(argv); ++n) {
		ret[n] = m0_strdup(argv[n]);
		M0_ASSERT(ret[n] != NULL);
	}
	*argc = ARRAY_SIZE(argv);
	return ret;
}

int bulkio_server_start(struct bulkio_params *bp, const char *saddr)
{
	int argc;

	M0_PRE(saddr != NULL && *saddr != '\0');

	bp->bp_slogfile = m0_strdup(IO_SERVER_LOGFILE);
	M0_ASSERT(bp->bp_slogfile != NULL);

	M0_ALLOC_PTR(bp->bp_sctx);
	M0_ASSERT(bp->bp_sctx != NULL);
	*bp->bp_sctx = (struct m0_rpc_server_ctx){
		.rsx_xprts_nr         = IO_XPRT_NR,
		.rsx_argv             = server_argv_alloc(saddr, &argc),
		.rsx_argc             = argc,
		.rsx_xprts            = &bp->bp_xprt,
		.rsx_log_file_name    = bp->bp_slogfile
	};
	return m0_rpc_server_start(bp->bp_sctx);
}

void bulkio_server_stop(struct m0_rpc_server_ctx *sctx)
{
	int i;

	m0_rpc_server_stop(sctx);

	for (i = 0; i < sctx->rsx_argc; ++i)
		m0_free(sctx->rsx_argv[i]);
	m0_free(sctx->rsx_argv);
	m0_free(sctx);
}
#endif

static void io_fids_init(struct bulkio_params *bp)
{
	int           i;
	struct m0_fid gfid;

	M0_ASSERT(bp != NULL);
	/* Populates fids. */
	for (i = 0; i < IO_FIDS_NR; ++i) {
		m0_fid_gob_make(&gfid, 2, 12345 + i);
	        m0_fid_convert_gob2cob(&gfid, &bp->bp_fids[i],
				       M0_AD_STOB_DOM_KEY_DEFAULT);
	}
}

static void io_buffers_allocate(struct bulkio_params *bp)
{
	int i;

	M0_ASSERT(bp != NULL);

	/* Initialized the standard buffer with a data pattern for read IO. */
	memset(bp->bp_readbuf, 'b', M0_0VEC_ALIGN);
	memset(bp->bp_writebuf, 'a', M0_0VEC_ALIGN);

	for (i = 0; i < IO_FOPS_NR; ++i)
		m0_bufvec_alloc_aligned(&bp->bp_iobuf[i]->nb_buffer,
					IO_SEGS_NR, M0_0VEC_ALIGN,
					M0_0VEC_SHIFT);
}

static void io_buffers_deallocate(struct bulkio_params *bp)
{
	int i;

	M0_ASSERT(bp != NULL);

	for (i = 0; i < IO_FOPS_NR; ++i)
		m0_bufvec_free_aligned(&bp->bp_iobuf[i]->nb_buffer,
				       M0_0VEC_SHIFT);
}

static void io_fop_populate(struct bulkio_params *bp, int index,
			    uint64_t off_index, struct m0_io_fop **io_fops,
			    int segs_nr)
{
	int			i;
	int			rc;
	struct m0_io_fop       *iofop;
	struct m0_rpc_bulk     *rbulk;
	struct m0_rpc_bulk_buf *rbuf;
	struct m0_fop_cob_rw   *rw;

	M0_ASSERT(bp != NULL);
	M0_ASSERT(io_fops != NULL);

	iofop = io_fops[index];
	rw    = io_rw_get(&iofop->if_fop);
	rw->crw_fid = bp->bp_fids[off_index];
	rw->crw_index = m0_fid_cob_device_id(&rw->crw_fid);
	rw->crw_pver = CONF_PVER_FID;

	rbulk = &iofop->if_rbulk;
	/*
	 * Adds a m0_rpc_bulk_buf structure to list of such structures
	 * in m0_rpc_bulk.
	 */
	rc = m0_rpc_bulk_buf_add(rbulk, segs_nr, 0,
				 &bp->bp_cnetdom, NULL, &rbuf);
	M0_ASSERT(rc == 0);
	M0_ASSERT(rbuf != NULL);

	/* Adds io buffers to m0_rpc_bulk_buf structure. */
	for (i = 0; i < segs_nr; ++i) {
		rc = m0_rpc_bulk_buf_databuf_add(rbuf,
			bp->bp_iobuf[index]->nb_buffer.ov_buf[i],
			bp->bp_iobuf[index]->nb_buffer.ov_vec.v_count[i],
			bp->bp_offsets[off_index], &bp->bp_cnetdom);
		M0_ASSERT(rc == 0);

		bp->bp_offsets[off_index] +=
			bp->bp_iobuf[index]->nb_buffer.ov_vec.v_count[i];
	}

	/*
	 * Allocates memory for array of net buf descriptors and array of
	 * index vectors from io fop.
	 */
	rc = m0_io_fop_prepare(&iofop->if_fop);
	M0_ASSERT(rc == 0);
	M0_ASSERT(rw->crw_desc.id_nr ==
		     m0_tlist_length(&rpcbulk_tl, &rbulk->rb_buflist));
	M0_ASSERT(rw->crw_desc.id_descs != NULL);

	/*
	 * Stores the net buf desc/s after adding the corresponding
	 * net buffers to transfer machine to io fop wire format.
	 */
	rc = m0_rpc_bulk_store(rbulk, &bp->bp_cctx->rcx_connection,
			       rw->crw_desc.id_descs, &m0_rpc__buf_bulk_cb);
	M0_ASSERT(rc == 0);
}

void io_fops_create(struct bulkio_params *bp, enum M0_RPC_OPCODES op,
		    int fids_nr, int fops_nr, int segs_nr)
{
	int		     i;
	int		     rc;
	uint64_t	     seed;
	uint64_t	     rnd;
	struct m0_fop_type  *fopt;
	struct m0_io_fop   **io_fops;

	seed = 0;
	for (i = 0; i < fids_nr; ++i)
		bp->bp_offsets[i] = IO_SEG_START_OFFSET;
	if (op == M0_IOSERVICE_WRITEV_OPCODE) {
		M0_ASSERT(bp->bp_wfops == NULL);
		M0_ALLOC_ARR(bp->bp_wfops, fops_nr);
		fopt = &m0_fop_cob_writev_fopt;
		io_fops = bp->bp_wfops;
		bp->bp_wfops_nr = fops_nr;
	} else {
		M0_ASSERT(bp->bp_rfops == NULL);
		M0_ALLOC_ARR(bp->bp_rfops, fops_nr);
		fopt = &m0_fop_cob_readv_fopt;
		io_fops = bp->bp_rfops;
		bp->bp_rfops_nr = fops_nr;
	}
	M0_ASSERT(io_fops != NULL);

	/* Allocates io fops. */
	for (i = 0; i < fops_nr; ++i) {
		M0_ALLOC_PTR(io_fops[i]);
		M0_ASSERT(io_fops[i] != NULL);
		rc = m0_io_fop_init(io_fops[i], &bp->bp_fids[0], fopt, NULL);
		M0_ASSERT(rc == 0);
	}

	/* Populates io fops. */
	for (i = 0; i < fops_nr; ++i) {
		if (fids_nr < fops_nr) {
			rnd = m0_rnd(fids_nr, &seed);
			M0_ASSERT(rnd < fids_nr);
		}
		else rnd = i;

		io_fop_populate(bp, i, rnd, io_fops, segs_nr);
	}
}

void io_fops_destroy(struct bulkio_params *bp)
{
	int i;

	for (i = 0; i < bp->bp_wfops_nr; ++i)
		if (bp->bp_wfops[i] != NULL)
			m0_fop_put_lock(&bp->bp_wfops[i]->if_fop);
	for (i = 0; i < bp->bp_rfops_nr; ++i)
		if (bp->bp_rfops[i] != NULL)
			m0_fop_put_lock(&bp->bp_rfops[i]->if_fop);

	m0_free0(&bp->bp_rfops);
	m0_free0(&bp->bp_wfops);
	bp->bp_wfops_nr = bp->bp_rfops_nr = 0;
}

/**
 * Sends an fsync fop request to trigger the placing of a specific transaction.
 * @param remid remote id of the transaction to be fsync'ed.
 * @param t structure containing the bulkio parameters for the operation. From
 * those parameters only the network session is relevant to the function.
 * @return the rc included in the corresponding fsync fop reply.
 */
int io_fsync_send_fop(struct m0_be_tx_remid *remid, struct thrd_arg *t)
{
	int                             rc;
	struct m0_fop                  *fop;
	struct m0_fop_fsync            *ffop;
	struct m0_fop_fsync_rep        *rfop;

	struct m0_rpc_item             *item;
	struct bulkio_params           *bp;
	struct m0_rpc_machine          *machine;

	bp = t->ta_bp;

	machine = session_machine(&bp->bp_cctx->rcx_session);

	/* create a fop from fsync_fopt */
	fop = m0_fop_alloc(&m0_fop_fsync_ios_fopt, NULL, machine);
	M0_UT_ASSERT(fop != NULL);

	/* populate fop */
	ffop = m0_fop_data(fop);
	ffop->ff_be_remid = *remid;
	ffop->ff_fsync_mode = M0_FSYNC_MODE_ACTIVE;

	/* send fop */
	item = &fop->f_item;
	item->ri_session = &bp->bp_cctx->rcx_session;
	item->ri_prio = M0_RPC_ITEM_PRIO_MID;
	rc = m0_rpc_post(item);
	M0_UT_ASSERT(rc == 0);

	/* get and process the reply */
	rc = m0_rpc_item_wait_for_reply(item, M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
	rfop = m0_fop_data(m0_rpc_item_to_fop(fop->f_item.ri_reply));

	/* check the values returned */
	M0_UT_ASSERT(rfop->ffr_be_remid.tri_txid == remid->tri_txid);
	rc = rfop->ffr_rc;

	/* release structs */
	m0_fop_put_lock(fop);

	return rc;
}


void io_fops_rpc_submit(struct thrd_arg *t)
{
	int                          i;
	int                          j;
	int                          rc;
	struct m0_rpc_item          *item;
	struct m0_rpc_bulk          *rbulk;
	struct m0_io_fop           **io_fops;
	struct m0_fop_cob_rw_reply  *rw_reply;
	struct bulkio_params        *bp;

	i = t->ta_index;
	bp = t->ta_bp;
	io_fops = (t->ta_op == M0_IOSERVICE_WRITEV_OPCODE) ? bp->bp_wfops :
		  bp->bp_rfops;
	M0_SET0(&io_fops[i]->if_fop.f_item.ri_sm);
	rbulk = m0_fop_to_rpcbulk(&io_fops[i]->if_fop);
	item = &io_fops[i]->if_fop.f_item;
	item->ri_session = &bp->bp_cctx->rcx_session;
	item->ri_prio = M0_RPC_ITEM_PRIO_MID;
	rc = m0_rpc_post(item);
	M0_ASSERT(rc == 0);

	rc = m0_rpc_item_wait_for_reply(item, M0_TIME_NEVER);
	M0_ASSERT(rc == 0);
	rw_reply = io_rw_rep_get(m0_rpc_item_to_fop(item->ri_reply));
	M0_ASSERT(rw_reply->rwr_rc == 0);
	bp->bp_remid = rw_reply->rwr_mod_rep.fmr_remid;

	if (m0_is_read_fop(&io_fops[i]->if_fop)) {
		for (j = 0; j < bp->bp_iobuf[i]->nb_buffer.ov_vec.v_nr; ++j) {
			rc = memcmp(bp->bp_iobuf[i]->nb_buffer.ov_buf[j],
				    bp->bp_readbuf,
				    bp->bp_iobuf[i]->nb_buffer.ov_vec.
				    v_count[j]);
			M0_ASSERT(rc == 0);
			memset(bp->bp_iobuf[i]->nb_buffer.ov_buf[j], 'a',
			       M0_0VEC_ALIGN);
		}
		m0_mutex_lock(&rbulk->rb_mutex);
		M0_ASSERT(rbulk->rb_rc == 0);
		m0_mutex_unlock(&rbulk->rb_mutex);
	}
}

void bulkio_params_init(struct bulkio_params *bp)
{
	int i;
	int rc;

	M0_ASSERT(bp != NULL);

	/* Initialize fids and allocate buffers used for bulk transfer. */
	io_fids_init(bp);

	M0_ASSERT(bp->bp_iobuf == NULL);
	M0_ALLOC_ARR(bp->bp_iobuf, IO_FOPS_NR);
	M0_ASSERT(bp->bp_iobuf != NULL);

	M0_ASSERT(bp->bp_threads == NULL);
	M0_ALLOC_ARR(bp->bp_threads, IO_FOPS_NR);
	M0_ASSERT(bp->bp_threads != NULL);
	for (i = 0; i < IO_FOPS_NR; ++i) {
		M0_ALLOC_PTR(bp->bp_iobuf[i]);
		M0_ASSERT(bp->bp_iobuf[i] != NULL);
		M0_ALLOC_PTR(bp->bp_threads[i]);
		M0_ASSERT(bp->bp_threads[i] != NULL);
	}

	M0_ASSERT(bp->bp_readbuf == NULL);
	M0_ALLOC_ARR(bp->bp_readbuf, M0_0VEC_ALIGN);
	M0_ASSERT(bp->bp_readbuf != NULL);

	M0_ASSERT(bp->bp_writebuf == NULL);
	M0_ALLOC_ARR(bp->bp_writebuf, M0_0VEC_ALIGN);
	M0_ASSERT(bp->bp_writebuf != NULL);

	io_buffers_allocate(bp);

	bp->bp_xprt = &m0_net_lnet_xprt;
	rc = m0_net_domain_init(&bp->bp_cnetdom, bp->bp_xprt);
	M0_ASSERT(rc == 0);

	for (i = 0; i < IO_FIDS_NR; ++i)
		bp->bp_offsets[i] = IO_SEG_START_OFFSET;

	bp->bp_rfops = NULL;
	bp->bp_wfops = NULL;

	bp->bp_flock_rt.rt_name = "File Lock Resource Type";

	m0_rm_domain_init(&bp->bp_rdom);
	rc = m0_file_lock_type_register(&bp->bp_rdom, &bp->bp_flock_rt);

	M0_ASSERT(rc == 0);

}

void bulkio_params_fini(struct bulkio_params *bp)
{
	int i;

	M0_ASSERT(bp != NULL);

	m0_net_domain_fini(&bp->bp_cnetdom);
	M0_ASSERT(bp->bp_iobuf != NULL);
	io_buffers_deallocate(bp);

	for (i = 0; i < IO_FOPS_NR; ++i) {
		m0_free(bp->bp_iobuf[i]);
		m0_free(bp->bp_threads[i]);
	}
	m0_free(bp->bp_iobuf);
	m0_free(bp->bp_threads);

	M0_ASSERT(bp->bp_readbuf != NULL);
	m0_free(bp->bp_readbuf);
	M0_ASSERT(bp->bp_writebuf != NULL);
	m0_free(bp->bp_writebuf);

	M0_ASSERT(bp->bp_rfops == NULL);
	M0_ASSERT(bp->bp_wfops == NULL);

	m0_free(bp->bp_cdbname);
	m0_free(bp->bp_slogfile);
	m0_file_lock_type_deregister(&bp->bp_flock_rt);
	m0_rm_domain_fini(&bp->bp_rdom);
}

int bulkio_client_start(struct bulkio_params *bp, const char *caddr,
			const char *saddr)
{
	int			  rc;
	char			 *cdbname;
	struct m0_rpc_client_ctx *cctx;

	M0_ASSERT(bp != NULL);
	M0_ASSERT(caddr != NULL);
	M0_ASSERT(saddr != NULL);

	M0_ALLOC_PTR(cctx);
	M0_ASSERT(cctx != NULL);

	cctx->rcx_remote_addr           = saddr;
	cctx->rcx_max_rpcs_in_flight    = IO_RPC_MAX_IN_FLIGHT;
	cctx->rcx_recv_queue_min_length = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	cctx->rcx_max_rpc_msg_size	= M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	cctx->rcx_local_addr            = caddr;
	cctx->rcx_net_dom               = &bp->bp_cnetdom;
	cctx->rcx_fid                   = &g_process_fid;

	M0_ALLOC_ARR(cdbname, IO_STR_LEN);
	M0_ASSERT(cdbname != NULL);
	strcpy(cdbname, IO_CLIENT_DBNAME);

	rc = m0_rpc_client_start(cctx);
	M0_ASSERT(rc == 0);

	bp->bp_cctx    = cctx;
	bp->bp_saddr   = saddr;
	bp->bp_caddr   = caddr;
	bp->bp_cdbname = cdbname;

	return rc;
}

void bulkio_client_stop(struct m0_rpc_client_ctx *cctx)
{
	int rc;

	M0_ASSERT(cctx != NULL);

	rc = m0_rpc_client_stop(cctx);
	M0_ASSERT(rc == 0);

	m0_free(cctx);
}

void cob_attr_default_fill(struct m0_cob_attr *attr)
{
	attr->ca_atime = attr->ca_mtime = attr->ca_ctime = 1416970585;
	attr->ca_nlink = 1;
	attr->ca_lid   = 1;
	attr->ca_mode  = 0100644;
	attr->ca_uid   = attr->ca_gid = 0;
	attr->ca_size  = attr->ca_blocks = 0;
	attr->ca_valid = M0_COB_ATIME | M0_COB_MTIME  | M0_COB_CTIME |
			 M0_COB_UID   | M0_COB_GID    | M0_COB_NLINK |
			 M0_COB_SIZE  | M0_COB_BLOCKS | M0_COB_LID   |
			 M0_COB_MODE;
}
