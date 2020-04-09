/* -*- C -*- */
/*
 * COPYRIGHT 2017 SEAGATE TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * https://www.seagate.com/contacts
 *
 * Original author: Nachiket Sahasrabudhe <nachiket.sahasrabuddhe@seagate.com>
 * Original creation: 27 Nov 2017
 */

#pragma once

#ifndef __MERO_ISCSERVICE_UT_COMMON_H__
#define __MERO_ISCSERVICE_UT_COMMON_H__

#include "lib/thread.h"
#include "ut/ut.h"
#include "ut/misc.h"
enum {
	THR_NR = 5,
};

struct thr_args {
	void                *ta_data;
	struct m0_semaphore *ta_barrier;
	int                  ta_rc;
};

struct cnc_cntrl_block {
	struct m0_thread    ccb_threads[THR_NR];
	struct m0_semaphore ccb_barrier;
	struct thr_args     ccb_args[THR_NR];
};

M0_INTERNAL void cc_block_init(struct cnc_cntrl_block *cc_block, size_t size,
			       void (*t_data_init)(void *, int));

M0_INTERNAL void cc_block_launch(struct cnc_cntrl_block *cc_block,
				 void (*t_op)(void *));

#endif
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
