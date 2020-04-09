/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 20-Mar-2014
 */

#pragma once

#ifndef __MERO_UT_STOB_H__
#define __MERO_UT_STOB_H__

#include "lib/types.h"		/* uint64_t */
#include "module/module.h"	/* m0_module */

/**
 * @defgroup utstob
 *
 * @{
 */

struct m0_stob;
struct m0_stob_domain;
struct m0_be_tx_credit;
struct ut_stob_module;
struct m0_stob_id;
struct m0_be_domain;

enum {
	M0_LEVEL_UT_STOB,
};

struct m0_ut_stob_module {
	struct m0_module       usm_module;
	struct ut_stob_module *usm_private;
};

extern struct m0_modlev m0_levels_ut_stob[];
extern const unsigned m0_levels_ut_stob_nr;

M0_INTERNAL struct m0_stob *m0_ut_stob_linux_get(void);
M0_INTERNAL struct m0_stob *m0_ut_stob_linux_create(char *stob_create_cfg);
M0_INTERNAL struct m0_stob *m0_ut_stob_linux_get_by_key(uint64_t stob_key);
M0_INTERNAL void m0_ut_stob_put(struct m0_stob *stob, bool destroy);

M0_INTERNAL int m0_ut_stob_create(struct m0_stob *stob, const char *str_cfg,
				  struct m0_be_domain *be_dom);
M0_INTERNAL int m0_ut_stob_destroy(struct m0_stob *stob,
				   struct m0_be_domain *be_dom);
M0_INTERNAL struct m0_stob *m0_ut_stob_open(struct m0_stob_domain *dom,
					    uint64_t stob_key,
					    const char *str_cfg);
M0_INTERNAL void m0_ut_stob_close(struct m0_stob *stob, bool destroy);

M0_INTERNAL int m0_ut_stob_create_by_stob_id(struct m0_stob_id *stob_id,
					     const char *str_cfg);
M0_INTERNAL int m0_ut_stob_destroy_by_stob_id(struct m0_stob_id *stob_id);

/* XXX move somewhere else */
M0_INTERNAL struct m0_dtx *m0_ut_dtx_open(struct m0_be_tx_credit *cred,
					  struct m0_be_domain    *be_dom);
M0_INTERNAL void m0_ut_dtx_close(struct m0_dtx *dtx);

M0_INTERNAL int m0_ut_stob_init(void);
M0_INTERNAL void m0_ut_stob_fini(void);

/** @} end of utstob group */
#endif /* __MERO_UT_STOB_H__ */

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
