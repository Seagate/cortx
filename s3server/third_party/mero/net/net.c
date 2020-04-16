/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 07/01/2010
 */

/*
 * Compile separately if not building "altogether".
 */

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/mutex.h"
#ifndef __KERNEL__
#  include "lib/string.h"  /* m0_streq */
#endif

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_NET
#include "lib/trace.h"

#include "net/net_otw_types.h"

/**
   @addtogroup net
   @{
 */

/**
   Network module global mutex.
   This mutex is used to serialize domain init and fini.
   It is defined here so that it can get initialized and fini'd
   by the general initialization mechanism.
   Transport that deal with multiple domains can rely on this mutex being held
   across their xo_dom_init() and xo_dom_fini() methods.
 */
struct m0_mutex m0_net_mutex;

/** @} net */

M0_INTERNAL int m0_net_init(void)
{
	m0_mutex_init(&m0_net_mutex);
	return 0;
}

M0_INTERNAL void m0_net_fini(void)
{
	m0_mutex_fini(&m0_net_mutex);
}

M0_INTERNAL int m0_net_desc_copy(const struct m0_net_buf_desc *from_desc,
				 struct m0_net_buf_desc *to_desc)
{
	M0_PRE(from_desc->nbd_len > 0);
	M0_ALLOC_ARR(to_desc->nbd_data, from_desc->nbd_len);
	if (to_desc->nbd_data == NULL)
		return M0_ERR(-ENOMEM);
	memcpy(to_desc->nbd_data, from_desc->nbd_data, from_desc->nbd_len);
	to_desc->nbd_len = from_desc->nbd_len;
	return 0;
}
M0_EXPORTED(m0_net_desc_copy);

M0_INTERNAL void m0_net_desc_free(struct m0_net_buf_desc *desc)
{
	if (desc->nbd_len > 0) {
		M0_PRE(desc->nbd_data != NULL);
		m0_free(desc->nbd_data);
		desc->nbd_len = 0;
	}
	desc->nbd_data = NULL;
}
M0_EXPORTED(m0_net_desc_free);

#ifndef __KERNEL__
M0_INTERNAL bool m0_net_endpoint_is_valid(const char *endpoint)
{
	char        addr[16]; /* strlen("255.255.255.255") + 1 */
	const char *networks[] = { "@lo", "@tcp", "@o2ib" };
	int32_t     n[4];
	size_t      i;
	int         rc;

	rc = sscanf(endpoint, "%15[0-9.]", addr);
	if (rc != 1)
		return M0_RC(false);
	endpoint += strlen(addr); /* skip address part */

	if (!m0_exists(j, ARRAY_SIZE(networks),
		       m0_startswith(networks[i = j], endpoint)))
		return M0_RC(false);
	endpoint += strlen(networks[i]);

	if (m0_streq(networks[i], "@lo")) {
		if (!m0_streq(addr, "0"))
			return M0_RC(false);
	} else {
		rc = sscanf(addr, "%d.%d.%d.%d", &n[0], &n[1], &n[2], &n[3]);
		if (rc != 4 ||
		    m0_exists(i, ARRAY_SIZE(n), n[i] < 0 || n[i] > 255))
			return M0_RC(false); /* invalid IPv4 address */
		if (isdigit(*endpoint))
			++endpoint; /* skip optional digit */
	}

	if (!m0_startswith(":12345", endpoint))
		return M0_RC(false);
	endpoint += 6; /* strlen(":12345") */

	for (i = 0; i < 2; ++i) {
		rc = sscanf(endpoint, ":%15[0-9]", addr);
		if (rc != 1)
			return M0_RC(false);
		endpoint += 1 + strlen(addr); /* 1 is for ':' */
	}
	return M0_RC(*endpoint == '\0');
}
#endif /* !__KERNEL__ */

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
