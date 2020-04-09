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
 * Original creation date: 3-Mar-2014
 */

#include "ut/ut.h"

#include "module/instance.h"	/* m0_get */
#include "module/module.h"
#include "stob/module.h"	/* m0_stob_module__get */

extern void m0_stob_ut_cache(void);
extern void m0_stob_ut_cache_idle_size0(void);
extern void m0_stob_ut_stob_domain_null(void);
extern void m0_stob_ut_stob_null(void);
extern void m0_stob_ut_stob_domain_linux(void);
extern void m0_stob_ut_stob_linux(void);
extern void m0_stob_ut_adieu_linux(void);
extern void m0_stob_ut_stobio_linux(void);
extern void m0_stob_ut_stob_domain_perf(void);
extern void m0_stob_ut_stob_domain_perf_null(void);
extern void m0_stob_ut_stob_perf(void);
extern void m0_stob_ut_stob_perf_null(void);
extern void m0_stob_ut_adieu_perf(void);
extern void m0_stob_ut_stobio_perf(void);
extern void m0_stob_ut_stob_domain_ad(void);
extern void m0_stob_ut_stob_ad(void);
extern void m0_stob_ut_adieu_ad(void);

struct m0_ut_suite stob_ut = {
	.ts_name = "stob-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "cache",		m0_stob_ut_cache		},
		{ "cache-idle-size0",	m0_stob_ut_cache_idle_size0	},
#ifndef __KERNEL__
		{ "null-stob-domain",	m0_stob_ut_stob_domain_null	},
		{ "null-stob",		m0_stob_ut_stob_null		},
		{ "linux-stob-domain",	m0_stob_ut_stob_domain_linux	},
		{ "linux-stob",		m0_stob_ut_stob_linux		},
		{ "linux-adieu",	m0_stob_ut_adieu_linux		},
		{ "linux-stobio",	m0_stob_ut_stobio_linux		},
		{ "perf-stob-domain",	m0_stob_ut_stob_domain_perf	},
		{ "perf-stob-domain-null", m0_stob_ut_stob_domain_perf_null },
		{ "perf-stob",		m0_stob_ut_stob_perf		},
		{ "perf-stob-null",	m0_stob_ut_stob_perf_null	},
		{ "perf-adieu",		m0_stob_ut_adieu_perf		},
		{ "perf-stobio",	m0_stob_ut_stobio_perf		},
		{ "ad-stob-domain",	m0_stob_ut_stob_domain_ad	},
		{ "ad-stob",		m0_stob_ut_stob_ad		},
		{ "ad-adieu",		m0_stob_ut_adieu_ad		},
#endif  /* __KERNEL__ */
		{ NULL, NULL }
	}
};

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
