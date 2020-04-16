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
 * Copyright 2009, 2010 ClusterStor.
 *
 * Nikita Danilov.
 */

#pragma once

#ifndef __MERO_DESIM_CLIENT_H__
#define __MERO_DESIM_CLIENT_H__

/**
   @addtogroup desim desim
   @{
 */

#include "lib/tlist.h"

struct net_conf;
struct net_srv;

struct client {
	struct sim_thread  *cl_thread;
	struct sim_thread  *cl_pageout;
	unsigned long       cl_cached;
	unsigned long       cl_dirty;
	unsigned long       cl_io;
	unsigned long       cl_fid;
	unsigned            cl_id;
	unsigned            cl_inflight;
	struct sim_chan     cl_cache_free;
	struct sim_chan     cl_cache_busy;
	struct client_conf *cl_conf;
	struct m0_tl        cl_write_ext;
};

struct client_conf {
	unsigned           cc_nr_clients;
	unsigned           cc_nr_threads;
	unsigned long      cc_total;
	unsigned long      cc_count;
	unsigned long      cc_opt_count;
	unsigned           cc_inflight_max;
	sim_time_t         cc_delay_min;
	sim_time_t         cc_delay_max;
	unsigned long      cc_cache_max;
	unsigned long      cc_dirty_max;
	struct net_conf   *cc_net;
	struct net_srv    *cc_srv;
	struct client     *cc_client;
	struct cnt         cc_cache_free;
	struct cnt         cc_cache_busy;
	int                cc_shutdown;
};

M0_INTERNAL void client_init(struct sim *s, struct client_conf *conf);
M0_INTERNAL void client_fini(struct client_conf *conf);

#endif /* __MERO_DESIM_CLIENT_H__ */

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
