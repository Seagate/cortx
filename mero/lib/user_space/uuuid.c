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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"
#include "lib/uuid.h"
#include "lib/errno.h"               /* EINVAL */

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/** path to read kmod uuid parameter */
static const char *kmod_uuid_file = "/sys/module/m0mero/parameters/node_uuid";

/**
 * Default node uuid which can be used instead of a "real" one, which is
 * obtained from kernel module; this can be handy for some utility applications
 * which don't need full functionality of libmero.so, so they can provide some
 * fake uuid.
*/
static char default_node_uuid[M0_UUID_STRLEN + 1] =
		"00000000-0000-0000-0000-000000000000"; /* nil UUID */

/** Flag, which specify whether to use a "real" node uuid or a default one. */
static bool use_default_node_uuid = false;

void m0_kmod_uuid_file_set(const char *path)
{
	kmod_uuid_file = path;
}

void m0_node_uuid_string_set(const char *uuid)
{
	use_default_node_uuid = true;
	if (uuid != NULL) {
		strncpy(default_node_uuid, uuid, M0_UUID_STRLEN);
		default_node_uuid[M0_UUID_STRLEN] = '\0';
	}
}

/**
 * Constructs the node UUID in user space by reading our kernel module's
 * node_uuid parameter.
 */
int m0_node_uuid_string_get(char buf[M0_UUID_STRLEN + 1])
{
	int fd;
	int rc = 0;

	if (use_default_node_uuid) {
		strncpy(buf, default_node_uuid, M0_UUID_STRLEN);
		buf[M0_UUID_STRLEN] = '\0';
	} else {
		fd = open(kmod_uuid_file, O_RDONLY);
		if (fd < 0)
			return M0_ERR(-EINVAL);
		if (read(fd, buf, M0_UUID_STRLEN) == M0_UUID_STRLEN) {
			rc = 0;
			buf[M0_UUID_STRLEN] = '\0';
		} else
			rc = M0_ERR(-EINVAL);
		close(fd);
	}

	return M0_RC(rc);
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
