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
 * Original author: Dmytro Podgornyi <dmytro_podgornyi@xyratex.com>
 * Original creation date: 11-Mar-2014
 */

#include "stob/module.h"
#include "module/instance.h"

static int level_stob_enter(struct m0_module *module);
static void level_stob_leave(struct m0_module *module);

static const struct m0_modlev levels_stob[] = {
	[M0_LEVEL_STOB] = {
		.ml_name = "stob is initialised",
		.ml_enter = level_stob_enter,
		.ml_leave = level_stob_leave,
	}
};

static int level_stob_enter(struct m0_module *module)
{
	return m0_stob_types_init();
}

static void level_stob_leave(struct m0_module *module)
{
	m0_stob_types_fini();
}

M0_INTERNAL struct m0_stob_module *m0_stob_module__get(void)
{
	return &m0_get()->i_stob_module;
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
