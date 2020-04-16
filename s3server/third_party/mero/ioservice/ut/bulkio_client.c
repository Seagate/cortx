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
 * Original creation date: 12/27/2011
 */

#include "ut/ut.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "mero/magic.h"
#include "ioservice/io_fops.h"	/* m0_io_fop */
#include "rpc/rpc.h"		/* m0_rpc_bulk, m0_rpc_bulk_buf */
#include "net/lnet/lnet.h"
#include "file/file.h"
#include "lib/finject.h"

#ifdef __KERNEL__
#include "m0t1fs/linux_kernel/m0t1fs.h"
#endif

enum {
	IO_SINGLE_BUFFER	= 1,
};

M0_TL_DESCR_DECLARE(rpcbulk, M0_EXTERN);
extern struct m0_fop_cob_rw *io_rw_get(struct m0_fop *fop);

static void bulkio_tm_cb(const struct m0_net_tm_event *ev)
{
}

static struct m0_net_tm_callbacks bulkio_ut_tm_cb = {
	.ntc_event_cb = bulkio_tm_cb
};

/*
 * This structure represents message sending/receiving entity on network
 * for either client or server. Since rpc can not be used as is,
 * this structure just tries to start transfer machines on both ends.
 * Since ultimately rpc uses transfer machine rpc, message
 * sending/receiving can be achieved easily without having to go
 * through rpc interfaces.
 * The m0_rpc_conn member is just a placeholder. It is needed for
 * m0_rpc_bulk_{store/load} APIs.
 */
struct bulkio_msg_tm {
	struct m0_rpc_machine bmt_mach;
	struct m0_rpc_conn    bmt_conn;
	const char           *bmt_addr;
};

static void bulkio_msg_tm_init(struct bulkio_msg_tm *bmt,
			       struct m0_net_domain *nd)
{
	int                        rc;
	struct m0_clink            clink;
	struct m0_net_transfer_mc *tm;

	M0_UT_ASSERT(bmt != NULL);
	M0_UT_ASSERT(nd != NULL);
	M0_UT_ASSERT(bmt->bmt_addr != NULL);
	M0_UT_ASSERT(bmt->bmt_mach.rm_tm.ntm_state == M0_NET_TM_UNDEFINED);

	tm = &bmt->bmt_mach.rm_tm;
	M0_SET0(&bmt->bmt_conn);
	bmt->bmt_conn.c_rpc_machine = &bmt->bmt_mach;

	tm->ntm_state = M0_NET_TM_UNDEFINED;
	tm->ntm_callbacks = &bulkio_ut_tm_cb;
	rc = m0_net_tm_init(tm, nd);
	M0_UT_ASSERT(rc == 0);

	m0_clink_init(&clink, NULL);
	m0_clink_add_lock(&tm->ntm_chan, &clink);
	rc = m0_net_tm_start(tm, bmt->bmt_addr);
	M0_UT_ASSERT(rc == 0);

	while (tm->ntm_state != M0_NET_TM_STARTED)
		m0_chan_wait(&clink);

	m0_clink_del_lock(&clink);
	m0_clink_fini(&clink);
}

static void bulkio_msg_tm_fini(struct bulkio_msg_tm *bmt)
{
	int             rc;
	struct m0_clink clink;

	M0_UT_ASSERT(bmt != NULL);
	M0_UT_ASSERT(bmt->bmt_addr != NULL);
	M0_UT_ASSERT(bmt->bmt_mach.rm_tm.ntm_state == M0_NET_TM_STARTED);
	M0_UT_ASSERT(bmt->bmt_conn.c_rpc_machine == &bmt->bmt_mach);

	m0_clink_init(&clink, NULL);
	m0_clink_add_lock(&bmt->bmt_mach.rm_tm.ntm_chan, &clink);

	rc = m0_net_tm_stop(&bmt->bmt_mach.rm_tm, false);
	M0_UT_ASSERT(rc == 0);

	while (bmt->bmt_mach.rm_tm.ntm_state != M0_NET_TM_STOPPED)
		m0_chan_wait(&clink);

	m0_clink_del_lock(&clink);
	m0_clink_fini(&clink);

	m0_net_tm_fini(&bmt->bmt_mach.rm_tm);
}

static void bulkclient_test(void)
{
	int			 rc;
	int                      i     = 0;
	char			*sbuf;
	int32_t			 max_segs;
	m0_bcount_t		 max_seg_size;
	m0_bcount_t		 max_buf_size;
	struct m0_fid            fid;
	struct m0_clink          s_clink;
	struct m0_clink          c_clink;
	struct m0_io_fop	*iofop;
	struct m0_net_xprt	*xprt;
	struct m0_rpc_bulk	*rbulk;
	struct m0_rpc_bulk	*sbulk;
	struct m0_fop_cob_rw	*rw;
	struct m0_net_domain	 nd;
	struct m0_net_buffer	*nb;
	struct m0_net_buffer   **nbufs;
	struct m0_rpc_bulk_buf	*rbuf;
	struct m0_rpc_bulk_buf	*rbuf1;
	struct m0_rpc_bulk_buf	*rbuf2;
	const char              *caddr = "0@lo:12345:34:*";
	const char		*saddr = "0@lo:12345:34:8";
	enum m0_net_queue_type	 q;
	struct bulkio_msg_tm    *ctm;
	struct bulkio_msg_tm    *stm;
	struct m0_rm_domain     *rm_dom;
	struct m0_file           file;

	struct m0_rm_resource_type flock_rt = {
		.rt_name = "File Lock Resource Type"
	};

	M0_ALLOC_PTR(iofop);
	M0_ASSERT(iofop != NULL);
	M0_SET0(iofop);
	M0_SET0(&nd);

	M0_ALLOC_PTR(rm_dom);
	M0_ASSERT(rm_dom != NULL);
	m0_rm_domain_init(rm_dom);
	rc = m0_file_lock_type_register(rm_dom, &flock_rt);
	M0_ASSERT(rc == 0);

	xprt = &m0_net_lnet_xprt;
	rc = m0_net_domain_init(&nd, xprt);
	M0_UT_ASSERT(rc == 0);

	fid.f_container = 1;
	fid.f_key       = 4;
	M0_SET0(&file);
	m0_file_init(&file, &fid, rm_dom, M0_DI_NONE);

	/* Test : m0_io_fop_init() */
	rc = m0_io_fop_init(iofop, &fid, &m0_fop_cob_writev_fopt, NULL);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(iofop->if_magic == M0_IO_FOP_MAGIC);
	M0_UT_ASSERT(iofop->if_fop.f_type != NULL);
	M0_UT_ASSERT(iofop->if_fop.f_item.ri_type != NULL);
	M0_UT_ASSERT(iofop->if_fop.f_item.ri_ops  != NULL);

	M0_UT_ASSERT(iofop->if_rbulk.rb_buflist.t_magic == M0_RPC_BULK_MAGIC);
	M0_UT_ASSERT(iofop->if_rbulk.rb_magic == M0_RPC_BULK_MAGIC);
	M0_UT_ASSERT(iofop->if_rbulk.rb_bytes == 0);
	M0_UT_ASSERT(iofop->if_rbulk.rb_rc    == 0);

	/* Test : m0_fop_to_rpcbulk() */
	rbulk = m0_fop_to_rpcbulk(&iofop->if_fop);
	M0_UT_ASSERT(rbulk != NULL);
	M0_UT_ASSERT(rbulk == &iofop->if_rbulk);

	/* Test : m0_rpc_bulk_buf_add() */
	rc = m0_rpc_bulk_buf_add(rbulk, IO_SINGLE_BUFFER, 0, &nd, NULL, &rbuf);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rbuf != NULL);

	/* Test : m0_rpc_bulk_buf structure. */
	M0_UT_ASSERT(m0_tlink_is_in(&rpcbulk_tl, rbuf));
	M0_UT_ASSERT(rbuf->bb_magic == M0_RPC_BULK_BUF_MAGIC);
	M0_UT_ASSERT(rbuf->bb_rbulk == rbulk);
	M0_UT_ASSERT(rbuf->bb_nbuf  != NULL);

	/*
	 * Since no external net buffer was passed to m0_rpc_bulk_buf_add(),
	 * it should allocate a net buffer internally and m0_rpc_bulk_buf::
	 * bb_flags should be M0_RPC_BULK_NETBUF_ALLOCATED.
	 */
	M0_UT_ASSERT(rbuf->bb_flags == M0_RPC_BULK_NETBUF_ALLOCATED);

	/* Test : m0_rpc_bulk_buf_add() - Error case. */
	max_segs = m0_net_domain_get_max_buffer_segments(&nd);
	rc = m0_rpc_bulk_buf_add(rbulk, max_segs + 1, 0, &nd, NULL, &rbuf1);
	M0_UT_ASSERT(rc == -EMSGSIZE);

	/* Test : m0_rpc_bulk_buf_databuf_add(). */
	sbuf = m0_alloc_aligned(M0_0VEC_ALIGN, M0_0VEC_SHIFT);
	M0_UT_ASSERT(sbuf != NULL);
	memset(sbuf, 'a', M0_0VEC_ALIGN);
	rc = m0_rpc_bulk_buf_databuf_add(rbuf, sbuf, M0_0VEC_ALIGN, 0, &nd);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_vec_count(&rbuf->bb_zerovec.z_bvec.ov_vec) ==
		     M0_0VEC_ALIGN);
	M0_UT_ASSERT(m0_vec_count(&rbuf->bb_zerovec.z_bvec.ov_vec) ==
		     m0_vec_count(&rbuf->bb_nbuf->nb_buffer.ov_vec));

	/* Test : m0_rpc_bulk_buf_databuf_add() - Error case. */
	max_seg_size = m0_net_domain_get_max_buffer_segment_size(&nd);
	rc = m0_rpc_bulk_buf_databuf_add(rbuf, sbuf, max_seg_size + 1, 0, &nd);
	/* Segment size bigger than permitted segment size. */
	M0_UT_ASSERT(rc == -EMSGSIZE);

	max_buf_size = m0_net_domain_get_max_buffer_size(&nd);
	rc = m0_rpc_bulk_buf_databuf_add(rbuf, sbuf, max_buf_size + 1, 0, &nd);
	/* Max buffer size greater than permitted max buffer size. */
	M0_UT_ASSERT(rc == -EMSGSIZE);

	/* Test : m0_rpc_bulk_buflist_empty() */
	m0_rpc_bulk_buflist_empty(rbulk);
	M0_UT_ASSERT(m0_tlist_is_empty(&rpcbulk_tl, &rbulk->rb_buflist));

	rc = m0_rpc_bulk_buf_add(rbulk, IO_SINGLE_BUFFER, 0, &nd, NULL, &rbuf);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rbuf != NULL);
	rc = m0_rpc_bulk_buf_databuf_add(rbuf, sbuf, M0_0VEC_ALIGN, 0, &nd);
	M0_UT_ASSERT(rc == 0);

	/* Test : m0_rpc_bulk_buf_add(nb != NULL)*/
	M0_ALLOC_PTR(nb);
	M0_UT_ASSERT(nb != NULL);
	rc = m0_bufvec_alloc_aligned(&nb->nb_buffer, IO_SINGLE_BUFFER,
				     M0_0VEC_ALIGN, M0_0VEC_SHIFT);
	M0_UT_ASSERT(rc == 0);
	memset(nb->nb_buffer.ov_buf[IO_SINGLE_BUFFER - 1], 'a', M0_0VEC_ALIGN);

	rc = m0_rpc_bulk_buf_add(rbulk, IO_SINGLE_BUFFER, 0,
				 &nd, nb, &rbuf1);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(rbuf1 != NULL);
	M0_UT_ASSERT(rbuf1->bb_nbuf == nb);
	M0_UT_ASSERT(!(rbuf1->bb_flags & M0_RPC_BULK_NETBUF_ALLOCATED));

	M0_UT_ASSERT(rbuf->bb_nbuf != NULL);
	/* In normal code path, an io_request is allocated. io_request embeds io_fop.
	 * In this UT, only io_fop is allocated here. So, skip di_prepare here. */
	m0_fi_enable("io_fop_di_prepare", "skip_di_for_ut");
	rc = m0_io_fop_prepare(&iofop->if_fop);
	M0_UT_ASSERT(rc == 0);
	m0_fi_disable("io_fop_di_prepare", "skip_di_for_ut");
	rw = io_rw_get(&iofop->if_fop);
	M0_UT_ASSERT(rw != NULL);

	M0_ALLOC_PTR(ctm);
	M0_UT_ASSERT(ctm != NULL);
	ctm->bmt_addr = caddr;
	bulkio_msg_tm_init(ctm, &nd);

	rc = m0_rpc_bulk_store(rbulk, &ctm->bmt_conn, rw->crw_desc.id_descs,
			       &m0_rpc__buf_bulk_cb);
	M0_UT_ASSERT(rc == 0);

	/*
	 * There is no ACTIVE side _yet_ to start the bulk transfer and
	 * hence the buffer is guaranteed to stay put in the
	 * PASSIVE_BULK_SEND queue of TM.
	 */
	m0_mutex_lock(&rbulk->rb_mutex);
	m0_tl_for(rpcbulk, &rbulk->rb_buflist, rbuf) {
		M0_UT_ASSERT(rbuf->bb_nbuf->nb_callbacks ==
			     &m0_rpc__buf_bulk_cb);
		M0_UT_ASSERT(rbuf->bb_nbuf != NULL);
		M0_UT_ASSERT(rbuf->bb_nbuf->nb_app_private == rbuf);
		M0_UT_ASSERT(rbuf->bb_nbuf->nb_flags & M0_NET_BUF_REGISTERED);
		M0_UT_ASSERT(rbuf->bb_nbuf->nb_flags & M0_NET_BUF_QUEUED);
		rc = memcmp(rbuf->bb_nbuf->nb_desc.nbd_data,
			    rw->crw_desc.id_descs[i].bdd_desc.nbd_data,
			    rbuf->bb_nbuf->nb_desc.nbd_len);
		M0_UT_ASSERT(rc == 0);
		++i;
	} m0_tl_endfor;
	M0_UT_ASSERT(rbulk->rb_bytes ==  2 * M0_0VEC_ALIGN);
	/* Register a clink for client side rbulk. */
	m0_clink_init(&c_clink, NULL);
	m0_clink_add(&rbulk->rb_chan, &c_clink);
	m0_mutex_unlock(&rbulk->rb_mutex);

	/* Start server side TM. */
	M0_ALLOC_PTR(stm);
	M0_UT_ASSERT(stm != NULL);
	stm->bmt_addr = saddr;
	bulkio_msg_tm_init(stm, &nd);

	/*
	 * Bulk server (receiving side) typically uses m0_rpc_bulk structure
	 * without having to use m0_io_fop.
	 */
	M0_ALLOC_PTR(sbulk);
	M0_UT_ASSERT(sbulk != NULL);

	/*
	 * Pretends that io fop is received and starts zero copy.
	 * Actual fop can not be sent since rpc server hands over any
	 * incoming fop to associated request handler. And request
	 * handler does not work in kernel space at the moment.
	 */
	m0_rpc_bulk_init(sbulk);

	M0_ALLOC_ARR(nbufs, rw->crw_desc.id_nr);
	M0_UT_ASSERT(nbufs != NULL);
	for (i = 0; i < rw->crw_desc.id_nr; ++i) {
		M0_ALLOC_PTR(nbufs[i]);
		M0_UT_ASSERT(nbufs[i] != NULL);
		rc = m0_bufvec_alloc_aligned(&nbufs[i]->nb_buffer, 1,
					     M0_0VEC_ALIGN, M0_0VEC_SHIFT);
		M0_UT_ASSERT(rc == 0);
	}

	for (i = 0; i < rw->crw_desc.id_nr; ++i) {
		rc = m0_rpc_bulk_buf_add(sbulk, 1, 0, &nd, nbufs[i],
					 &rbuf2);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(rbuf2 != NULL);
	}

	m0_mutex_lock(&sbulk->rb_mutex);
	q = m0_is_read_fop(&iofop->if_fop) ? M0_NET_QT_ACTIVE_BULK_SEND :
	    M0_NET_QT_ACTIVE_BULK_RECV;
	m0_rpc_bulk_qtype(sbulk, q);
	m0_clink_init(&s_clink, NULL);
	m0_clink_add(&sbulk->rb_chan, &s_clink);
	m0_mutex_unlock(&sbulk->rb_mutex);

	rc = m0_rpc_bulk_load(sbulk, &stm->bmt_conn, rw->crw_desc.id_descs,
			      &m0_rpc__buf_bulk_cb);

	/*
	 * Buffer completion callbacks also wait to acquire the
	 * m0_rpc_bulk::rb_mutex and in any case asserts inside the loop
	 * are protected from buffer completion callbacks which do some
	 * cleanup due to the lock.
	 */
	m0_mutex_lock(&sbulk->rb_mutex);
	m0_tl_for(rpcbulk, &sbulk->rb_buflist, rbuf2) {
		M0_UT_ASSERT(rbuf2->bb_nbuf != NULL);
		M0_UT_ASSERT(rbuf2->bb_nbuf->nb_flags & M0_NET_BUF_REGISTERED);
		M0_UT_ASSERT(rbuf2->bb_nbuf->nb_app_private == rbuf2);
		M0_UT_ASSERT(rbuf2->bb_rbulk == sbulk);
		M0_UT_ASSERT(!(rbuf2->bb_flags & M0_RPC_BULK_NETBUF_ALLOCATED));
		M0_UT_ASSERT(rbuf2->bb_flags & M0_RPC_BULK_NETBUF_REGISTERED);
	} m0_tl_endfor;
	M0_UT_ASSERT(sbulk->rb_bytes == 2 * M0_0VEC_ALIGN);
	m0_mutex_unlock(&sbulk->rb_mutex);
	/* Waits for zero copy to complete. */
	M0_UT_ASSERT(rc == 0);
	m0_chan_wait(&s_clink);
	m0_chan_wait(&c_clink);

	m0_mutex_lock(&sbulk->rb_mutex);
	m0_mutex_lock(&rbulk->rb_mutex);
	M0_UT_ASSERT(m0_tlist_is_empty(&rpcbulk_tl, &sbulk->rb_buflist));
	m0_clink_del(&s_clink);
	m0_clink_del(&c_clink);
	m0_mutex_unlock(&rbulk->rb_mutex);
	m0_mutex_unlock(&sbulk->rb_mutex);

	m0_clink_fini(&s_clink);
	m0_clink_fini(&c_clink);
	m0_rpc_bulk_fini(sbulk);
	for (i = 0; i < rw->crw_desc.id_nr; ++i) {
		rc = memcmp(nbufs[i]->nb_buffer.ov_buf[IO_SINGLE_BUFFER - 1],
			    sbuf, M0_0VEC_ALIGN);
		M0_UT_ASSERT(rc == 0);
	}

	/* Rpc bulk op timeout */

	rc = m0_rpc_bulk_buf_add(rbulk, IO_SINGLE_BUFFER, 0, &nd, nb, &rbuf1);
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_bulk_store(rbulk, &ctm->bmt_conn, rw->crw_desc.id_descs,
			       &m0_rpc__buf_bulk_cb);
	M0_UT_ASSERT(rc == 0);
	m0_mutex_lock(&rbulk->rb_mutex);
	m0_clink_init(&c_clink, NULL);
	m0_clink_add(&rbulk->rb_chan, &c_clink);
	rc = m0_rpc_bulk_store_del_unqueued(rbulk);
	M0_UT_ASSERT(rc == 0);
	m0_mutex_unlock(&rbulk->rb_mutex);
	m0_rpc_bulk_store_del(rbulk);
	m0_chan_wait(&c_clink);
	m0_mutex_lock(&rbulk->rb_mutex);
	m0_clink_del(&c_clink);
	m0_mutex_unlock(&rbulk->rb_mutex);
	m0_clink_fini(&c_clink);

	m0_rpc_bulk_init(sbulk);
	for (i = 0; i < rw->crw_desc.id_nr; ++i) {
		rc = m0_rpc_bulk_buf_add(sbulk, 1, 0, &nd, nbufs[i], &rbuf2);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(rbuf2 != NULL);
	}

	m0_mutex_lock(&sbulk->rb_mutex);
	m0_rpc_bulk_qtype(sbulk, q);
	m0_clink_init(&s_clink, NULL);
	m0_clink_add(&sbulk->rb_chan, &s_clink);
	m0_mutex_unlock(&sbulk->rb_mutex);
	m0_fi_enable("rpc_bulk_op", "timeout_2s");
	rc = m0_rpc_bulk_load(sbulk, &stm->bmt_conn, rw->crw_desc.id_descs,
			      &m0_rpc__buf_bulk_cb);
	M0_UT_ASSERT(rc == 0);
	m0_chan_wait(&s_clink);
	m0_fi_disable("rpc_bulk_op", "timeout_2s");
	m0_mutex_lock(&sbulk->rb_mutex);
	M0_UT_ASSERT(m0_tlist_is_empty(&rpcbulk_tl, &sbulk->rb_buflist));
	m0_clink_del(&s_clink);
	m0_mutex_unlock(&sbulk->rb_mutex);

	m0_clink_fini(&s_clink);


	bulkio_msg_tm_fini(ctm);
	bulkio_msg_tm_fini(stm);
	m0_free(ctm);
	m0_free(stm);

	for (i = 0; i < rw->crw_desc.id_nr; ++i) {
		m0_bufvec_free_aligned(&nbufs[i]->nb_buffer, M0_0VEC_SHIFT);
		m0_free(nbufs[i]);
	}
	m0_free(nbufs);
	m0_free(sbulk);

	m0_io_fop_destroy(&iofop->if_fop);
	M0_UT_ASSERT(rw->crw_desc.id_descs  == NULL);
	M0_UT_ASSERT(rw->crw_desc.id_nr     == 0);
	M0_UT_ASSERT(rw->crw_ivec.ci_iosegs == NULL);
	M0_UT_ASSERT(rw->crw_ivec.ci_nr     == 0);

	m0_rpc_bulk_buflist_empty(rbulk);
	M0_UT_ASSERT(m0_tlist_is_empty(&rpcbulk_tl, &rbulk->rb_buflist));

	m0_bufvec_free_aligned(&nb->nb_buffer, M0_0VEC_SHIFT);
	m0_free(nb);

	/* Cleanup. */
	m0_free_aligned(sbuf, M0_0VEC_ALIGN, M0_0VEC_SHIFT);
	m0_io_fop_fini(iofop);
	m0_free(iofop);
	m0_net_domain_fini(&nd);
	m0_file_fini(&file);
	m0_file_lock_type_deregister(&flock_rt);
	m0_rm_domain_fini(rm_dom);
	m0_free(rm_dom);
}

struct m0_ut_suite bulkio_client_ut = {
	.ts_name = "bulk-client-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "bulkclient_test", bulkclient_test},
		{ NULL, NULL }
	}
};
M0_EXPORTED(bulkio_client_ut);
