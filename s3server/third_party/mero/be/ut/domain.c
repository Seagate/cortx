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
 * Original author: Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 18-Jul-2013
 */

#include "be/domain.h"

#include "ut/ut.h"
#include "stob/stob.h"          /* m0_stob_id */

#include "be/ut/helper.h"       /* m0_be_ut_backend */

void m0_be_ut_mkfs(void)
{
	struct m0_be_ut_backend  ut_be = {};
	struct m0_be_domain_cfg  cfg = {};
	struct m0_be_domain     *dom = &ut_be.but_dom;
	struct m0_be_seg        *seg;
	void                    *addr;
	void                    *addr2;
	int                      rc;

	m0_be_ut_backend_cfg_default(&cfg);
	/* mkfs mode start */
	rc = m0_be_ut_backend_init_cfg(&ut_be, &cfg, true);
	M0_UT_ASSERT(rc == 0);
	m0_be_ut_backend_seg_add2(&ut_be, 0x10000, true, NULL, &seg);
	addr = seg->bs_addr;
	m0_be_ut_backend_fini(&ut_be);

	M0_SET0(&ut_be);
	/* normal mode start */
	rc = m0_be_ut_backend_init_cfg(&ut_be, &cfg, false);
	M0_UT_ASSERT(rc == 0);
	seg = m0_be_domain_seg(dom, addr);
	addr2 = seg->bs_addr;
	M0_ASSERT_INFO(addr == addr2, "addr = %p, addr2 = %p", addr, addr2);
	m0_be_ut_backend_seg_del(&ut_be, seg);
	m0_be_ut_backend_fini(&ut_be);

	M0_SET0(&ut_be);
	/* normal mode start */
	rc = m0_be_ut_backend_init_cfg(&ut_be, &cfg, false);
	M0_UT_ASSERT(rc == 0);
	seg = m0_be_domain_seg(dom, addr);
	M0_ASSERT_INFO(seg == NULL, "seg = %p", seg);
	m0_be_ut_backend_fini(&ut_be);
}

enum {
	BE_UT_MKFS_MULTISEG_SEG_NR   = 0x10,
	BE_UT_MKFS_MULTISEG_SEG_SIZE = 1 << 24,
};

M0_INTERNAL void m0_be_ut_mkfs_multiseg(void)
{
	struct m0_be_0type_seg_cfg  segs_cfg[BE_UT_MKFS_MULTISEG_SEG_NR];
	struct m0_be_domain_cfg     dom_cfg = {};
	struct m0_be_ut_backend     ut_be = {};
	m0_bcount_t                 size;
	unsigned                    i;
	void                       *addr;
	int                         rc;

	for (i = 0; i < ARRAY_SIZE(segs_cfg); ++i) {
		size = BE_UT_MKFS_MULTISEG_SEG_SIZE;
		addr = m0_be_ut_seg_allocate_addr(size);
		segs_cfg[i] = (struct m0_be_0type_seg_cfg){
			.bsc_stob_key        = m0_be_ut_seg_allocate_id(),
			.bsc_size            = size,
			.bsc_preallocate     = false,
			.bsc_addr            = addr,
			.bsc_stob_create_cfg = NULL,
		};
	}
	m0_be_ut_backend_cfg_default(&dom_cfg);
	dom_cfg.bc_mkfs_mode = true;
	dom_cfg.bc_seg_cfg   = segs_cfg;
	dom_cfg.bc_seg_nr    = ARRAY_SIZE(segs_cfg);

	rc = m0_be_ut_backend_init_cfg(&ut_be, &dom_cfg, true);
	M0_ASSERT(rc == 0);
	m0_be_ut_backend_fini(&ut_be);
}

void m0_be_ut_domain(void)
{
	struct m0_be_ut_backend ut_be = {};

	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_backend_fini(&ut_be);
}

void m0_be_ut_domain_is_stob(void)
{
	struct m0_be_ut_backend  ut_be = {};
	struct m0_be_domain     *dom;
	struct m0_stob_id        stob_id = {};
	bool                     is_stob;

	m0_be_ut_backend_init(&ut_be);
	dom = &ut_be.but_dom;
	is_stob = m0_be_domain_is_stob_log(dom, &stob_id);
	M0_UT_ASSERT(!is_stob);
	is_stob = m0_be_domain_is_stob_seg(dom, &stob_id);
	M0_UT_ASSERT(!is_stob);
	/*
	 * TODO add more cases after domain interfaces allow to enumerate stobs
	 * used by segments and log.
	 */
	m0_be_ut_backend_fini(&ut_be);
}

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
