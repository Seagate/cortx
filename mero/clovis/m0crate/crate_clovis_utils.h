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
 * Original author:  Pratik Shinde <pratik.shinde@seagate.com>
 * Original creation date: 20-Jun-2016
 */

#pragma once

#ifndef __MERO_CLOVIS_M0CRATE_CRATE_CLOVIS_UTILS_H__
#define __MERO_CLOVIS_M0CRATE_CRATE_CLOVIS_UTILS_H__

#include "clovis/m0crate/crate_clovis.h"

/**
 * @defgroup crate_clovis_utils
 *
 * @{
 */
int adopt_mero_thread(struct clovis_workload_task *task);

void release_mero_thread(struct clovis_workload_task *task);

struct m0_clovis_realm *crate_clovis_uber_realm();

extern struct m0_clovis		*clovis_instance;

int clovis_init(struct workload *w);
int clovis_fini(struct workload *w);
void clovis_check(struct workload *w);

/** @} end of crate_clovis_utils group */
#endif /* __MERO_CLOVIS_M0CRATE_CRATE_CLOVIS_UTILS_H__ */

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
