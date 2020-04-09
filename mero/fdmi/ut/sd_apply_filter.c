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
#include "ut/ut.h"
#include "fdmi/fdmi.h"
#include "fdmi/service.h"               /* m0_reqh_fdmi_service */
#include "fdmi/source_dock_internal.h"

#include "fdmi/ut/sd_common.h"

static struct m0_semaphore    g_sem;
static char                   g_fdmi_data[] = "hello, FDMI";
static struct m0_fdmi_src_rec g_src_rec;

/*********** FilterC stub ***********/

static struct m0_conf_fdmi_filter g_conf_filter;
static char *g_var_str;

static int filterc_apply_flt_start(struct m0_filterc_ctx *ctx,
				   struct m0_reqh        *reqh);

static void filterc_apply_flt_stop(struct m0_filterc_ctx *ctx);

static int filterc_apply_flt_open(struct m0_filterc_ctx   *ctx,
				  enum m0_fdmi_rec_type_id rec_type_id,
				  struct m0_filterc_iter  *iter);

static int filterc_apply_flt_get_next(struct m0_filterc_iter     *iter,
				      struct m0_conf_fdmi_filter **out);

static void filterc_apply_flt_close(struct m0_filterc_iter *iter);

const struct m0_filterc_ops filterc_apply_flt_ops = {
	.fco_start     = filterc_apply_flt_start,
	.fco_stop      = filterc_apply_flt_stop,
	.fco_open      = filterc_apply_flt_open,
	.fco_get_next  = filterc_apply_flt_get_next,
	.fco_close     = filterc_apply_flt_close
};

static int filterc_apply_flt_start(struct m0_filterc_ctx *ctx,
				   struct m0_reqh        *reqh)
{
	return 0;
}

static void filterc_apply_flt_stop(struct m0_filterc_ctx *ctx)
{
}

static int filterc_apply_flt_open(struct m0_filterc_ctx   *ctx,
				  enum m0_fdmi_rec_type_id rec_type_id,
				  struct m0_filterc_iter  *iter)
{
	return 0;
}

static int filterc_apply_flt_get_next(struct m0_filterc_iter     *iter,
				      struct m0_conf_fdmi_filter **out)
{
	int                     rc;
	static bool             first_filter = true;
	struct m0_fdmi_filter   *flt = &g_conf_filter.ff_filter;
	struct m0_fdmi_flt_node *root;
	struct m0_buf           var = M0_BUF_INITS(g_var_str);

	if (first_filter) {
		root = m0_fdmi_flt_op_node_create(
		         M0_FFO_OR,
			 m0_fdmi_flt_bool_node_create(false),
			 m0_fdmi_flt_var_node_create(&var));

		m0_fdmi_filter_init(flt);

		m0_fdmi_filter_root_set(flt, root);

		*out = &g_conf_filter;
		rc = 1;
		first_filter = false;
	} else {
		*out = NULL;
		rc = 0;
	}
	return rc;
}

static void filterc_apply_flt_close(struct m0_filterc_iter *iter)
{
	m0_fdmi_filter_fini(&g_conf_filter.ff_filter);
}

/*********** Source definition ***********/
static int test_fs_node_eval(
		struct m0_fdmi_src_rec *src_rec,
		struct m0_fdmi_flt_var_node *value_desc,
		struct m0_fdmi_flt_operand *value)
{
	M0_UT_ASSERT(src_rec == &g_src_rec);
	M0_UT_ASSERT(src_rec->fsr_data == &g_fdmi_data);
	M0_UT_ASSERT(value_desc->ffvn_data.b_nob == strlen(g_var_str));
	M0_UT_ASSERT(value_desc->ffvn_data.b_addr == g_var_str);

	m0_fdmi_flt_bool_opnd_fill(value, false);
	return 0;
}

static int test_fs_encode(struct m0_fdmi_src_rec *src_rec,
			  struct m0_buf          *buf)
{
	M0_UT_ASSERT(false);
	return 0;
}


static void test_fs_get(struct m0_fdmi_src_rec *src_rec)
{
	M0_UT_ASSERT(false);
}

static void test_fs_put(struct m0_fdmi_src_rec *src_rec)
{
	M0_UT_ASSERT(src_rec != NULL);
	M0_UT_ASSERT(src_rec == &g_src_rec);
}

static void test_fs_end(struct m0_fdmi_src_rec *src_rec)
{
	M0_UT_ASSERT(src_rec != NULL);
	M0_UT_ASSERT(src_rec == &g_src_rec);
	/* Calling of this function is a sign for fdmi_sd_post_record UT
	 * that FDMI finished record processing */
	m0_semaphore_up(&g_sem);
}

static struct m0_fdmi_src *src_alloc()
{
	struct m0_fdmi_src *src;
	int                 rc;

	rc = m0_fdmi_source_alloc(M0_FDMI_REC_TYPE_TEST, &src);
	M0_UT_ASSERT(rc == 0);

	src->fs_node_eval  = test_fs_node_eval;
	src->fs_get        = test_fs_get;
	src->fs_put        = test_fs_put;
	src->fs_end        = test_fs_end;
	src->fs_encode     = test_fs_encode;
	return src;
}

static void fdmi_sd_apply_filter_internal(const struct m0_filterc_ops *ops)
{
	struct m0_fdmi_src             *src = src_alloc();
	int                             rc;

	M0_ENTRY();

	fdmi_serv_start_ut(ops);
	g_var_str = strdup("test");
	m0_semaphore_init(&g_sem, 0);
	rc = m0_fdmi_source_register(src);
	M0_UT_ASSERT(rc == 0);
	g_src_rec = (struct m0_fdmi_src_rec) {
		.fsr_src    = src,
		.fsr_data   = g_fdmi_data,
	};
	rc = M0_FDMI_SOURCE_POST_RECORD(&g_src_rec);
	M0_UT_ASSERT(rc == 0);
	/* Wait until record is processed and released */
	m0_semaphore_down(&g_sem);
	m0_fdmi_source_deregister(src);
	m0_fdmi_source_free(src);
	m0_semaphore_fini(&g_sem);
	fdmi_serv_stop_ut();
	M0_LEAVE();
}

void fdmi_sd_apply_filter(void)
{
	M0_ENTRY();
	fdmi_sd_apply_filter_internal(&filterc_apply_flt_ops);
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
