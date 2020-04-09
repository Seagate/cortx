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

#include "reqh/reqh.h"
#include "fdmi/filterc.h"

static int filterc_stub_start(struct m0_filterc_ctx 	*ctx,
			      struct m0_reqh        	*reqh);

static void filterc_stub_stop(struct m0_filterc_ctx *ctx);

static int filterc_stub_open(struct m0_filterc_ctx  *ctx,
                           enum m0_fdmi_rec_type_id  rec_type_id,
                           struct m0_filterc_iter   *iter);

static int filterc_stub_get_next(struct m0_filterc_iter     *iter,
                               struct m0_conf_fdmi_filter **out);

static void filterc_stub_close(struct m0_filterc_iter *iter);

const struct m0_filterc_ops filterc_stub_ops = {
	.fco_start     = filterc_stub_start,
	.fco_stop      = filterc_stub_stop,
	.fco_open      = filterc_stub_open,
	.fco_get_next  = filterc_stub_get_next,
	.fco_close     = filterc_stub_close
};

static int filterc_stub_start(struct m0_filterc_ctx 	*ctx,
			      struct m0_reqh        	*reqh)
{
	return 0;
}

static void filterc_stub_stop(struct m0_filterc_ctx *ctx)
{
}

static int filterc_stub_open(struct m0_filterc_ctx  *ctx,
                           enum m0_fdmi_rec_type_id  rec_type_id,
                           struct m0_filterc_iter   *iter)
{
	return 0;
}

static int filterc_stub_get_next(struct m0_filterc_iter     *iter,
                               struct m0_conf_fdmi_filter **out)
{
	*out = NULL;
	return 0;
}

static void filterc_stub_close(struct m0_filterc_iter *iter)
{
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
