/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 27-Apr-2016
 */


/**
 * @addtogroup ha
 *
 * @{
 */

#include "ut/ut.h"

extern void m0_ha_ut_cookie(void);

extern void m0_ha_ut_msg_queue(void);

extern void m0_ha_ut_lq(void);
extern void m0_ha_ut_lq_mark_delivered(void);

extern void m0_ha_ut_link_usecase(void);
extern void m0_ha_ut_link_multithreaded(void);
extern void m0_ha_ut_link_reconnect_simple(void);
extern void m0_ha_ut_link_reconnect_multiple(void);

extern void m0_ha_ut_entrypoint_usecase(void);
extern void m0_ha_ut_entrypoint_client(void);

extern void m0_ha_ut_ha_usecase(void);

struct m0_ut_suite ha_ut = {
	.ts_name = "ha-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "cookie",                 &m0_ha_ut_cookie                  },
		{ "msg_queue",              &m0_ha_ut_msg_queue               },
		{ "lq",                     &m0_ha_ut_lq                      },
		{ "lq-mark_delivered",      &m0_ha_ut_lq_mark_delivered       },
		{ "link-usecase",           &m0_ha_ut_link_usecase            },
		{ "link-multithreaded",     &m0_ha_ut_link_multithreaded      },
		{ "link-reconnect_simple",  &m0_ha_ut_link_reconnect_simple   },
		{ "link-reconnect_multiple",&m0_ha_ut_link_reconnect_multiple },
		{ "entrypoint-usecase",     &m0_ha_ut_entrypoint_usecase      },
		{ "entrypoint-client",      &m0_ha_ut_entrypoint_client       },
		{ "ha-usecase",             &m0_ha_ut_ha_usecase              },
		{ NULL, NULL },
	},
};

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
