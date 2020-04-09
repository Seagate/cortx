/* -*- C -*- */
/*
 * COPYRIGHT 2019 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 1-Nov-2019
 */

/**
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "ha/cookie.h"
#include "ut/ut.h"

#include "lib/memory.h" /* M0_ALLOC_PTR */


void m0_ha_ut_cookie(void)
{
	struct m0_ha_cookie_xc *hc_xc;
	struct m0_ha_cookie    *a;
	struct m0_ha_cookie    *b;

	M0_ALLOC_PTR(a);
	M0_UT_ASSERT(a != NULL);
	m0_ha_cookie_init(a);
	M0_UT_ASSERT(m0_ha_cookie_is_eq(a, &m0_ha_cookie_no_record));
	m0_ha_cookie_record(a);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, &m0_ha_cookie_no_record));
	m0_ha_cookie_fini(a);
	m0_free(a);

	M0_ALLOC_PTR(hc_xc);
	M0_UT_ASSERT(hc_xc != NULL);
	M0_ALLOC_PTR(a);
	M0_UT_ASSERT(a != NULL);
	M0_ALLOC_PTR(b);
	M0_UT_ASSERT(b != NULL);

	m0_ha_cookie_init(a);
	m0_ha_cookie_init(b);

	M0_UT_ASSERT(m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_to_xc(a, hc_xc);
	m0_ha_cookie_record(a);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_from_xc(a, hc_xc);
	M0_UT_ASSERT(m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_from_xc(b, hc_xc);
	M0_UT_ASSERT(m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_record(a);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_record(b);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_from_xc(a, hc_xc);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_from_xc(b, hc_xc);
	M0_UT_ASSERT(m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_record(a);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_record(b);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_to_xc(a, hc_xc);
	m0_ha_cookie_from_xc(b, hc_xc);
	M0_UT_ASSERT(m0_ha_cookie_is_eq(a, b));
	*a = m0_ha_cookie_no_record;
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));
	m0_ha_cookie_record(a);
	M0_UT_ASSERT(!m0_ha_cookie_is_eq(a, b));

	m0_ha_cookie_fini(b);
	m0_ha_cookie_fini(a);

	m0_free(b);
	m0_free(a);
	m0_free(hc_xc);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of ha group */

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
