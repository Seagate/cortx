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

#pragma once
#ifndef __MERO_STOB_MODULE_H__
#define __MERO_STOB_MODULE_H__

#include "module/module.h"
#include "stob/type.h"

/**
 * @defgroup stob Storage object
 *
 * @{
 */

/** Levels of m0_stob_module::stm_module. */
enum {
	/** m0_stob_types_init() has been called. */
	M0_LEVEL_STOB
};

struct m0_stob_module {
	struct m0_module     stm_module;
	struct m0_stob_types stm_types;
};

M0_INTERNAL struct m0_stob_module *m0_stob_module__get(void);

struct m0_stob_ad_module {
	struct m0_tl    sam_domains;
	struct m0_mutex sam_lock;
};

/** @} end of stob group */
#endif /* __MERO_STOB_MODULE_H__ */

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
