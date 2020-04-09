/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Yuriy Umanets <yuriy.umanets@seagate.com>
 * Original creation date: 1-Jun-2017
 */

#define M0_TRACE_SUBSYSTEM    M0_TRACE_SUBSYS_FDMI

#include "lib/trace.h"
#include "fdmi/module.h"
#include "module/instance.h"

static int level_fdmi_enter(struct m0_module *module);
static void level_fdmi_leave(struct m0_module *module);

static const struct m0_modlev levels_fdmi[] = {
	[M0_LEVEL_FDMI] = {
		.ml_name = "fdmi is initialised",
		.ml_enter = level_fdmi_enter,
		.ml_leave = level_fdmi_leave,
	}
};

static int level_fdmi_enter(struct m0_module *module)
{
	return 0;
}

static void level_fdmi_leave(struct m0_module *module)
{
}

M0_INTERNAL struct m0_fdmi_module *m0_fdmi_module__get(void)
{
	return &m0_get()->i_fdmi_module;
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
