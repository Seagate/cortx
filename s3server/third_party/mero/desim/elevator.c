/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "lib/assert.h"
#include "mero/magic.h"
#include "desim/elevator.h"

/**
   @addtogroup desim desim
   @{
 */

/*
 * Simple FIFO elevator.
 */

struct io_req {
	struct m0_tlink ir_linkage;
	uint64_t        ir_magic;
};

M0_TL_DESCR_DEFINE(req, "io requests", static, struct io_req,
		   ir_linkage, ir_magic, M0_DESIM_IO_REQ_MAGIC,
		   M0_DESIM_IO_REQ_HEAD_MAGIC);
M0_TL_DEFINE(req, static, struct io_req);

static void elevator_submit(struct elevator *el,
			    enum storage_req_type type,
			    sector_t sector, unsigned long count)
{
	el->e_idle = 0;
	el->e_dev->sd_submit(el->e_dev, type, sector, count);
}

/*
 * submit asynchronous requests.
 */
static void elevator_go(struct elevator *el)
{
	/* impossible for now */
	M0_IMPOSSIBLE("Elevator is not yet implemented");
}

M0_INTERNAL void el_end_io(struct storage_dev *dev)
{
	struct elevator *el;

	el = dev->sd_el;
	el->e_idle = 1;
	if (!req_tlist_is_empty(&el->e_queue))
		elevator_go(el);
	sim_chan_broadcast(&el->e_wait);
}

M0_INTERNAL void elevator_init(struct elevator *el, struct storage_dev *dev)
{
	el->e_dev  = dev;
	el->e_idle = 1;
	dev->sd_end_io = el_end_io;
	dev->sd_el     = el;
	req_tlist_init(&el->e_queue);
	sim_chan_init(&el->e_wait, "xfer-queue@%s", dev->sd_name);
}

M0_INTERNAL void elevator_fini(struct elevator *el)
{
	req_tlist_fini(&el->e_queue);
	sim_chan_fini(&el->e_wait);
}

M0_INTERNAL void elevator_io(struct elevator *el, enum storage_req_type type,
			     sector_t sector, unsigned long count)
{
	while (!el->e_idle)
		sim_chan_wait(&el->e_wait, sim_thread_current());
	elevator_submit(el, type, sector, count);
}

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
