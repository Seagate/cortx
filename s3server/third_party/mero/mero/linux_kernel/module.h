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
 * Original author: Dima Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 7-Aug-2013
 */

#pragma once

#ifndef __MERO_MERO_LINUX_KERNEL_MODULE_H__
#define __MERO_MERO_LINUX_KERNEL_MODULE_H__

#include <linux/version.h> /* LINUX_VERSION_CODE */

M0_INTERNAL const struct module *m0_mero_ko_get_module(void);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,5,0)
#define M0_MERO_KO_BASE(module) ((module)->core_layout.base)
#define M0_MERO_KO_SIZE(module) ((module)->core_layout.size)
#else
#define M0_MERO_KO_BASE(module) ((module)->module_core)
#define M0_MERO_KO_SIZE(module) ((module)->core_size)
#endif

#endif /* __MERO_MERO_LINUX_KERNEL_MODULE_H__ */

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
