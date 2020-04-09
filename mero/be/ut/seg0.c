/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 * Original creation date: 28-Mar-2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "lib/memory.h"         /* M0_ALLOC_PTR, m0_free */
#include "lib/buf.h"            /* m0_buf */
#include "lib/string.h"         /* m0_streq */
#include "be/ut/helper.h"       /* m0_be_ut_backend */
#include "be/seg0.h"
#include "format/format.h"      /* m0_format_header */
#include "ut/ut.h"

static const char          *be_ut_0type_suffix = "some test suffix";
static char                 be_ut_0type_data[10000];
static const struct m0_buf  be_ut_0type_data_buf =
				M0_BUF_INIT(sizeof(be_ut_0type_data),
					    &be_ut_0type_data);

static int be_ut_0type_test_init(struct m0_be_domain *dom,
				 const char          *suffix,
				 const struct m0_buf *data)
{
	M0_UT_ASSERT(m0_streq(suffix, be_ut_0type_suffix));
	M0_UT_ASSERT(m0_buf_eq(data, &be_ut_0type_data_buf));
	return 0;
}

static void be_ut_0type_test_fini(struct m0_be_domain *dom,
				  const char          *suffix,
				  const struct m0_buf *data)
{
	M0_UT_ASSERT(m0_streq(suffix, be_ut_0type_suffix));
	M0_UT_ASSERT(m0_buf_eq(data, &be_ut_0type_data_buf));
}

static struct m0_be_0type be_ut_0type_test = {
	.b0_name = "M0_BE:0type_test",
	.b0_init = &be_ut_0type_test_init,
	.b0_fini = &be_ut_0type_test_fini,
};

static void be_ut_0type_op_test(struct m0_be_ut_backend  *ut_be,
				struct m0_be_0type       *zt,
				const char               *suffix,
				const struct m0_buf      *data,
				bool                      add)
{
	struct m0_be_tx_credit   credit = {};
	struct m0_be_domain     *dom    = &ut_be->but_dom;
	struct m0_be_tx          tx     = {};
	int                      rc;

	m0_be_0type_register(dom, &be_ut_0type_test);
	m0_be_ut_tx_init(&tx, ut_be);
	if (add) {
		m0_be_0type_add_credit(dom, zt, suffix, data, &credit);
	} else {
		m0_be_0type_del_credit(dom, zt, suffix, &credit);
	}
	m0_be_tx_prep(&tx, &credit);
	m0_be_tx_exclusive_open_sync(&tx);
	if (add) {
		rc = m0_be_0type_add(zt, dom, &tx, suffix, data);
	} else {
		rc = m0_be_0type_del(zt, dom, &tx, suffix);
	}
	M0_UT_ASSERT(rc == 0);
	m0_be_tx_close_sync(&tx);
	m0_be_tx_fini(&tx);
	m0_be_0type_unregister(dom, &be_ut_0type_test);
}

void m0_be_ut_seg0_test(void)
{
	struct m0_be_ut_backend  ut_be = {};

	m0_be_ut_backend_init(&ut_be);
	be_ut_0type_op_test(&ut_be, &be_ut_0type_test, be_ut_0type_suffix,
			    &be_ut_0type_data_buf, true);
	be_ut_0type_op_test(&ut_be, &be_ut_0type_test, be_ut_0type_suffix,
			    NULL, false);
	m0_be_ut_backend_fini(&ut_be);
}

void m0_be_ut_obj_test(void)
{
	static const struct {
		struct m0_format_tag    t;
		struct m0_format_header h;
	} data[] = {
		{
			.t = { 1, 2, { 3 } },
			.h = { .hd_bits = 0x0001000200000003 }
		},
		{
			.t = { 0x1111, 0x2222, { 0x3333 } },
			.h = { .hd_bits = 0x1111222200003333 }
		}
	};
	struct m0_format_header h;
	struct m0_format_tag    t;
	unsigned                i;

	for (i = 0; i < ARRAY_SIZE(data); ++i) {
		m0_format_header_pack(&h, &data[i].t);
		M0_UT_ASSERT(h.hd_bits == data[i].h.hd_bits);

		m0_format_header_unpack(&t, &data[i].h);
		M0_UT_ASSERT(t.ot_version == data[i].t.ot_version);
		M0_UT_ASSERT(t.ot_type == data[i].t.ot_type);
		M0_UT_ASSERT(t.ot_size == data[i].t.ot_size);
	}
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
