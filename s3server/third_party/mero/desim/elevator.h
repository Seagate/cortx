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
 */
/*
 * Copyright 2010 ClusterStor.
 *
 * Nikita Danilov.
 */

#pragma once

#ifndef __MERO_DESIM_ELEVATOR_H__
#define __MERO_DESIM_ELEVATOR_H__

#include "desim/sim.h"
#include "desim/storage.h"
#include "lib/tlist.h"

/**
   @addtogroup desim desim
   @{
 */

struct elevator {
	struct storage_dev *e_dev;
	int                 e_idle;
	struct m0_tl        e_queue;
	struct sim_chan     e_wait;
};

M0_INTERNAL void elevator_init(struct elevator *el, struct storage_dev *dev);
M0_INTERNAL void elevator_fini(struct elevator *el);

M0_INTERNAL void elevator_io(struct elevator *el, enum storage_req_type type,
			     sector_t sector, unsigned long count);

#endif /* __MERO_DESIM_ELEVATOR_H__ */

/** @} end of desim group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
