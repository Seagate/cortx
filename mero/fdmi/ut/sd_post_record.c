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
#include "fdmi/service.h" /* m0_reqh_fdmi_service */
#include "fdmi/source_dock_internal.h"

#include "fdmi/ut/sd_common.h"

static struct m0_semaphore     g_sem;
static struct m0_fdmi_src_rec  g_src_rec;

static int test_fs_node_eval(
	        struct m0_fdmi_src_rec *src_rec,
		struct m0_fdmi_flt_var_node *value_desc,
		struct m0_fdmi_flt_operand *value)
{
	M0_UT_ASSERT(false);
	return 0;
}

static int test_fs_encode(struct m0_fdmi_src_rec *src_rec,
			  struct m0_buf          *buf)
{
	M0_UT_ASSERT(false);
	return 0;
}

static int test_fs_decode(struct m0_buf *buf, void **handle)
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
	src->fs_decode     = test_fs_decode;
	return src;
}

void fdmi_sd_post_record(void)
{
	struct m0_fdmi_src             *src = src_alloc();
	int                             rc;

	static char fdmi_data[] = "hello, FDMI";

	M0_ENTRY();
	fdmi_serv_start_ut(&filterc_stub_ops);
	m0_semaphore_init(&g_sem, 0);
	rc = m0_fdmi_source_register(src);
	M0_UT_ASSERT(rc == 0);
	g_src_rec = (struct m0_fdmi_src_rec) {
		.fsr_src    = src,
		.fsr_data   = fdmi_data,
	};
	rc = M0_FDMI_SOURCE_POST_RECORD(&g_src_rec);
	M0_UT_ASSERT(rc == 0);
	/* Wait until record is processed and released */
	m0_semaphore_down(&g_sem);
	m0_fdmi_source_deregister(src);
	M0_UT_ASSERT(src->fs_record_post == NULL);
	m0_fdmi_source_free(src);
	m0_semaphore_fini(&g_sem);
	fdmi_serv_stop_ut();
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
