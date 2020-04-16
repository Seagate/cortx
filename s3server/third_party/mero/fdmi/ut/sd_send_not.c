/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include "lib/memory.h"
#include "lib/types.h"
#include "rpc/conn_pool_internal.h"
#include "rpc/packet_internal.h" /* packet_item_tlist_head */
#include "rpc/rpc_machine_internal.h"
#include "ut/ut.h"
#include "fdmi/fdmi.h"
#include "fdmi/fops.h"
#include "fdmi/service.h"        /* m0_reqh_fdmi_service */
#include "fdmi/source_dock_internal.h"

#include "fdmi/ut/sd_common.h"

static struct m0_semaphore    g_sem1;
static struct m0_semaphore    g_sem2;
static char                   g_fdmi_data[] = "hello, FDMI";
static struct m0_fdmi_src_rec g_src_rec;
static struct test_rpc_env    g_rpc_env;
static struct m0_rpc_packet  *g_sent_rpc_packet;
static struct m0_fid          g_fid = M0_FID_INIT(0xFA11, 0x11AF);

static int send_notif_packet_ready(struct m0_rpc_packet *p);

static const struct m0_rpc_frm_ops send_notif_frm_ops = {
	.fo_packet_ready = send_notif_packet_ready
};

/*********** FilterC stub ***********/

static struct m0_conf_fdmi_filter g_conf_filter;
static char                      *g_var_str;

static int filterc_send_notif_start(struct m0_filterc_ctx *ctx,
				    struct m0_reqh        *reqh);

static void filterc_send_notif_stop(struct m0_filterc_ctx *ctx);

static int filterc_send_notif_open(struct m0_filterc_ctx   *ctx,
				   enum m0_fdmi_rec_type_id rec_type_id,
				   struct m0_filterc_iter  *iter);

static int filterc_send_notif_get_next(struct m0_filterc_iter      *iter,
				       struct m0_conf_fdmi_filter **out);

static void filterc_send_notif_close(struct m0_filterc_iter *iter);

const struct m0_filterc_ops filterc_send_notif_ops = {
	.fco_start    = filterc_send_notif_start,
	.fco_stop     = filterc_send_notif_stop,
	.fco_open     = filterc_send_notif_open,
	.fco_get_next = filterc_send_notif_get_next,
	.fco_close    = filterc_send_notif_close
};

static int filterc_send_notif_start(struct m0_filterc_ctx *ctx,
				    struct m0_reqh        *reqh)
{
	return 0;
}

static void filterc_send_notif_stop(struct m0_filterc_ctx *ctx)
{
}

static int filterc_send_notif_open(struct m0_filterc_ctx    *ctx,
				   enum m0_fdmi_rec_type_id  rec_type_id,
				   struct m0_filterc_iter   *iter)
{
	return 0;
}

static int filterc_send_notif_get_next(struct m0_filterc_iter      *iter,
				       struct m0_conf_fdmi_filter **out)
{
	static bool                 first_filter = true;
	int                         rc;
	struct m0_conf_fdmi_filter *conf_flt = &g_conf_filter;
	struct m0_fdmi_filter      *flt = &conf_flt->ff_filter;
	struct m0_fdmi_flt_node    *root;
	struct m0_buf               var = M0_BUF_INITS(g_var_str);

	if (first_filter) {
		root = m0_fdmi_flt_op_node_create(
			M0_FFO_OR,
			m0_fdmi_flt_bool_node_create(false),
			m0_fdmi_flt_var_node_create(&var));

		m0_fdmi_filter_init(flt);

		m0_fdmi_filter_root_set(flt, root);

		M0_ALLOC_ARR(conf_flt->ff_endpoints, 1);
		conf_flt->ff_endpoints[0] = g_rpc_env.ep_addr_remote;
		conf_flt->ff_filter_id = g_fid;
		*out = conf_flt;
		rc = 1;
		first_filter = false;
	} else {
		*out = NULL;
		rc = 0;
	}
	return rc;
}

static void filterc_send_notif_close(struct m0_filterc_iter *iter)
{
	m0_fdmi_filter_fini(&g_conf_filter.ff_filter);
}

/*********** Source definition ***********/
static int test_fs_node_eval(struct m0_fdmi_src_rec *src_rec,
			     struct m0_fdmi_flt_var_node *value_desc,
			     struct m0_fdmi_flt_operand *value)
{
	M0_UT_ASSERT(src_rec == &g_src_rec);
	M0_UT_ASSERT(src_rec->fsr_data == &g_fdmi_data);
	M0_UT_ASSERT(value_desc->ffvn_data.b_nob == strlen(g_var_str));
	M0_UT_ASSERT(value_desc->ffvn_data.b_addr == g_var_str);

	m0_fdmi_flt_bool_opnd_fill(value, true);
	return 0;
}

static int test_fs_encode(struct m0_fdmi_src_rec *src_rec,
			  struct m0_buf          *buf)
{
	M0_UT_ASSERT(src_rec == &g_src_rec);
	M0_UT_ASSERT(src_rec->fsr_data == &g_fdmi_data);

	*buf = M0_BUF_INITS(g_fdmi_data);

	return 0;
}

bool inc_ref_passed = false;

static void test_fs_get(struct m0_fdmi_src_rec *src_rec)
{
	M0_UT_ASSERT(src_rec != NULL);
	M0_UT_ASSERT(src_rec == &g_src_rec);
	inc_ref_passed = true;
}

int dec_ref_count = 0;

static void test_fs_put(struct m0_fdmi_src_rec *src_rec)
{
	M0_UT_ASSERT(src_rec != NULL);
	M0_UT_ASSERT(src_rec == &g_src_rec);
	++dec_ref_count;
}

static void test_fs_begin(struct m0_fdmi_src_rec *src_rec)
{
	/**
	 * Overwrite source dock FOM client connection to
	 * check FOP content.
	 */
	M0_ENTRY("* src_rec %p, assigned %p", src_rec, &g_src_rec);
	M0_UT_ASSERT(src_rec == &g_src_rec);
	M0_UT_ASSERT(src_rec->fsr_data == &g_fdmi_data);
	M0_LEAVE();
}

static void test_fs_end(struct m0_fdmi_src_rec *src_rec)
{
	M0_UT_ASSERT(src_rec == &g_src_rec);
	M0_UT_ASSERT(src_rec->fsr_data == &g_fdmi_data);
	M0_UT_ASSERT(dec_ref_count > 1);
	m0_semaphore_up(&g_sem2);
}

static struct m0_fdmi_src *src_alloc()
{
	struct m0_fdmi_src *src;
	int                 rc;

	rc = m0_fdmi_source_alloc(M0_FDMI_REC_TYPE_TEST, &src);
	M0_UT_ASSERT(rc == 0);

	src->fs_node_eval = test_fs_node_eval;
	src->fs_get       = test_fs_get;
	src->fs_put       = test_fs_put;
	src->fs_begin     = test_fs_begin;
	src->fs_end       = test_fs_end;
	src->fs_encode    = test_fs_encode;
	return src;
}

static void check_fop_content(struct m0_rpc_item *item)
{
	struct m0_fop_fdmi_record *fdmi_rec;
	struct m0_buf              buf = M0_BUF_INITS(g_fdmi_data);

	fdmi_rec = m0_fop_data(m0_rpc_item_to_fop(item));

	M0_UT_ASSERT((void *)fdmi_rec->fr_rec_id.u_lo == &g_src_rec);
	M0_UT_ASSERT(fdmi_rec->fr_rec_type == M0_FDMI_REC_TYPE_TEST);
	M0_UT_ASSERT(m0_buf_eq(&fdmi_rec->fr_payload, &buf));
	M0_UT_ASSERT(fdmi_rec->fr_matched_flts.fmf_count == 1);
	M0_UT_ASSERT(m0_fid_eq(&fdmi_rec->fr_matched_flts.fmf_flt_id[0],
			       &g_fid));
}

static int send_notif_packet_ready(struct m0_rpc_packet *p)
{
	check_fop_content(packet_item_tlist_head(&p->rp_items));
	g_sent_rpc_packet = p;

	m0_semaphore_up(&g_sem1);
	return 0;
}

void fdmi_sd_send_notif(void)
{
	struct m0_fdmi_src_dock      *src_dock;
	struct m0_fdmi_src           *src = src_alloc();
	struct fdmi_sd_fom           *sd_fom;
	struct m0_rpc_conn_pool      *conn_pool;
	struct m0_rpc_conn_pool_item *pool_item;
	int                           rc;

	fdmi_serv_start_ut(&filterc_send_notif_ops);
	g_var_str = strdup("test");
	src_dock = m0_fdmi_src_dock_get();
	sd_fom = &src_dock->fsdc_sd_fom;
	conn_pool = &sd_fom->fsf_conn_pool;
	M0_UT_ASSERT(rpc_conn_pool_items_tlist_is_empty(&conn_pool->cp_items));
	M0_ALLOC_PTR(pool_item);
	M0_UT_ASSERT(pool_item != NULL);
	rpc_conn_pool_items_tlink_init_at_tail(pool_item, &conn_pool->cp_items);
	prepare_rpc_env(&g_rpc_env, &g_sd_ut.mero.cc_reqh_ctx.rc_reqh,
			&send_notif_frm_ops, true,
			&pool_item->cpi_rpc_link.rlk_conn,
			&pool_item->cpi_rpc_link.rlk_sess);
	m0_semaphore_init(&g_sem1, 0);
	m0_semaphore_init(&g_sem2, 0);
	rc = m0_fdmi_source_register(src);
	M0_UT_ASSERT(rc == 0);

	g_src_rec = (struct m0_fdmi_src_rec) {
		.fsr_src  = src,
		.fsr_data = g_fdmi_data,
	};

	rc = M0_FDMI_SOURCE_POST_RECORD(&g_src_rec);
	M0_UT_ASSERT(rc == 0);
	/* Wait until record is sent over RPC */
	m0_semaphore_down(&g_sem1);
	fdmi_ut_packet_send_failed(&g_rpc_env.tre_rpc_machine,
				   g_sent_rpc_packet);
	/* Wait until record is released */
	m0_semaphore_down(&g_sem2);
	M0_UT_ASSERT(inc_ref_passed);
	m0_fdmi_source_deregister(src);
	m0_fdmi_source_free(src);
	M0_UT_ASSERT(rpc_conn_pool_items_tlist_head(&conn_pool->cp_items) ==
		     rpc_conn_pool_items_tlist_tail(&conn_pool->cp_items));
	unprepare_rpc_env(&g_rpc_env);
	rpc_conn_pool_items_tlink_del_fini(pool_item);
	m0_free(pool_item);
	pool_item = NULL;
	fdmi_serv_stop_ut();
	m0_semaphore_fini(&g_sem1);
	m0_semaphore_fini(&g_sem2);
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
