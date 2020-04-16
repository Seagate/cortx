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
 * Copyright 2009 ClusterStor.
 *
 * Nikita Danilov.
 */

#pragma once

#ifndef __MERO_DESIM_NET_H__
#define __MERO_DESIM_NET_H__

#include "lib/tlist.h"
#include "fid/fid.h"
#include "stob/stob.h"
#include "desim/sim.h"

/**
   @addtogroup desim desim
   @{
 */

struct elevator;

struct net_conf {
	unsigned           nc_frag_size;
	unsigned           nc_rpc_size;
	sim_time_t         nc_rpc_delay_min;
	sim_time_t         nc_rpc_delay_max;
	sim_time_t         nc_frag_delay_min;
	sim_time_t         nc_frag_delay_max;
	unsigned long long nc_rate_min;
	unsigned long long nc_rate_max;
	unsigned long      nc_nob_max;
	unsigned long      nc_nob_inflight;
	unsigned long      nc_msg_max;
	unsigned long      nc_msg_inflight;
	struct sim_chan    nc_queue;
	struct cnt         nc_rpc_wait;
	struct cnt         nc_rpc_bulk_wait;
};

struct net_srv {
	unsigned            ns_nr_threads;
	unsigned            ns_nr_devices;
	sim_time_t          ns_pre_bulk_min;
	sim_time_t          ns_pre_bulk_max;
	struct sim_chan     ns_incoming;
	struct m0_tl        ns_queue;
	struct sim_thread  *ns_thread;
	struct elevator    *ns_el;
	unsigned long long  ns_file_size;
	int                 ns_shutdown;
	unsigned            ns_active;
	char               *ns_name;
};

struct net_rpc {
	struct net_srv     *nr_srv;
	struct net_conf    *nr_conf;
	struct m0_stob_id   nr_stob_id;
	unsigned long long  nr_offset;
	unsigned long       nr_todo;
	struct m0_tlink     nr_inqueue;
	struct sim_chan     nr_wait;
	struct sim_chan     nr_bulk_wait;
	struct sim_thread  *nr_srv_thread;
	uint64_t            nr_magic;
};

M0_INTERNAL void net_srv_init(struct sim *s, struct net_srv *srv);
M0_INTERNAL void net_srv_fini(struct net_srv *srv);

M0_INTERNAL void net_init(struct net_conf *net);
M0_INTERNAL void net_fini(struct net_conf *net);

M0_INTERNAL void net_rpc_init(struct net_rpc *rpc, struct net_conf *conf,
			      struct net_srv *srv, struct m0_stob_id *stob_id,
			      unsigned long long offset, unsigned long nob);
M0_INTERNAL void net_rpc_fini(struct net_rpc *rpc);
M0_INTERNAL void net_rpc_send(struct sim_thread *t, struct net_rpc *rpc);
M0_INTERNAL void net_rpc_bulk(struct sim_thread *t, struct net_rpc *rpc);
M0_INTERNAL void net_rpc_process(struct sim_thread *t,
				 struct net_conf *net, struct net_srv *srv,
				 struct m0_stob_id *stob_id,
				 unsigned long long offset,
				 unsigned long count);

#endif /* __MERO_DESIM_NET_H__ */

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
