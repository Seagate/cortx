/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 19-May-2015
 */


/**
 * @addtogroup uuid
 *
 * @{
 */

#include <linux/moduleparam.h> /* module_param */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"
#include "lib/uuid.h"

static char *node_uuid = "00000000-0000-0000-0000-000000000000"; /* nil UUID */
module_param(node_uuid, charp, S_IRUGO);
MODULE_PARM_DESC(node_uuid, "UUID of Mero node");

/**
 * Return the value of the kernel node_uuid parameter.
 */
static const char *m0_param_node_uuid_get(void)
{
	return node_uuid;
}

/**
 * Construct the node uuid from the kernel's node_uuid parameter.
 */
int m0_node_uuid_string_get(char buf[M0_UUID_STRLEN + 1])
{
	const char *s;

	s = m0_param_node_uuid_get();
	if (s == NULL)
		return M0_ERR(-EINVAL);
	strncpy(buf, s, M0_UUID_STRLEN);
	buf[M0_UUID_STRLEN] = '\0';
	return 0;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of uuid group */

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
