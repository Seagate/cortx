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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 05/19/2012
 */

#include "ut/ut.h"		/* m0_ut_suite */

#include "net/test/initfini.h"	/* m0_net_test_init */

extern void m0_net_test_ringbuf_ut(void);

extern void m0_net_test_serialize_ut(void);

extern void m0_net_test_str_ut(void);

extern void m0_net_test_slist_ut(void);

extern void m0_net_test_stats_ut(void);
extern void m0_net_test_timestamp_ut(void);

extern void m0_net_test_service_ut(void);

extern void m0_net_test_network_ut_buf_desc(void);
extern void m0_net_test_network_ut_ping(void);
extern void m0_net_test_network_ut_bulk(void);

extern void m0_net_test_cmd_ut_single(void);
extern void m0_net_test_cmd_ut_multiple(void);
extern void m0_net_test_cmd_ut_multiple2(void);

extern void m0_net_test_client_server_stub_ut(void);
extern void m0_net_test_client_server_ping_ut(void);
extern void m0_net_test_client_server_bulk_ut(void);

static int net_test_fini(void)
{
	m0_net_test_fini();
	return 0;
}

struct m0_ut_suite m0_net_test_ut = {
	.ts_name = "net-test",
	.ts_init = m0_net_test_init,
	.ts_fini = net_test_fini,
	.ts_tests = {
		{ "ringbuf",		m0_net_test_ringbuf_ut		  },
		{ "serialize",		m0_net_test_serialize_ut	  },
		{ "str",		m0_net_test_str_ut		  },
		{ "slist",		m0_net_test_slist_ut		  },
		{ "stats",		m0_net_test_stats_ut		  },
		{ "timestamp",		m0_net_test_timestamp_ut	  },
		{ "service",		m0_net_test_service_ut		  },
		{ "network-buf-desc",	m0_net_test_network_ut_buf_desc	  },
		{ "network-ping",	m0_net_test_network_ut_ping	  },
		{ "network-bulk",	m0_net_test_network_ut_bulk	  },
		{ "cmd-single",		m0_net_test_cmd_ut_single	  },
		{ "cmd-multiple",	m0_net_test_cmd_ut_multiple	  },
		{ "cmd-multiple2",	m0_net_test_cmd_ut_multiple2	  },
		{ "client-server-stub",	m0_net_test_client_server_stub_ut },
		/* XXX temporarily disabled. See MERO-2267 */
#if 0
		{ "client-server-ping",	m0_net_test_client_server_ping_ut },
#endif
		{ "client-server-bulk",	m0_net_test_client_server_bulk_ut },
		{ NULL,			NULL				  }
	}
};
M0_EXPORTED(m0_net_test_ut);

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
