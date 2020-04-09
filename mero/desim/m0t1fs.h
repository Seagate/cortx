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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 2012-Jun-16
 */

#pragma once

#ifndef __MERO_DESIM_M0T1FS_H__
#define __MERO_DESIM_M0T1FS_H__

/**
   @addtogroup desim desim
   @{
 */

#include "lib/tlist.h"
#include "lib/types.h"                   /* m0_bcount_t */

#include "pool/pool.h"
#include "layout/layout.h"
#include "layout/pdclust.h"

struct net_conf;
struct net_srv;
struct m0t1fs_thread;
struct m0t1fs_client;

struct m0t1fs_thread {
	struct sim_thread          cth_thread;
	struct m0t1fs_client      *cth_client;
	unsigned                   cth_id;
	struct m0_layout_instance *cth_layout_instance;
};

struct m0t1fs_client {
	struct m0t1fs_thread *cc_thread;
	unsigned              cc_id;
	struct m0t1fs_conn {
		unsigned        cs_inflight;
		struct sim_chan cs_wakeup;
	} *cc_srv;
	struct m0t1fs_conf   *cc_conf;
};

struct m0t1fs_conf {
	unsigned                  ct_nr_clients;
	unsigned                  ct_nr_threads;
	unsigned                  ct_nr_servers;
	unsigned                  ct_nr_devices;
	struct m0_layout_domain   ct_l_dom;
	struct m0_pdclust_layout *ct_pdclust;
	struct m0_pool            ct_pool;
	struct m0_pool_version    ct_pool_version;
	uint32_t                  ct_N;
	uint32_t                  ct_K;
	uint64_t                  ct_unitsize;
	unsigned long             ct_client_step;
	unsigned long             ct_thread_step;
	unsigned                  ct_inflight_max;
	m0_bcount_t               ct_total;
	sim_time_t                ct_delay_min;
	sim_time_t                ct_delay_max;
	struct net_conf          *ct_net;
	struct net_srv           *ct_srv0;
	struct net_srv           *ct_srv;
	struct m0t1fs_client     *ct_client;
};

M0_INTERNAL void m0t1fs_init(struct sim *s, struct m0t1fs_conf *conf);
M0_INTERNAL void m0t1fs_fini(struct m0t1fs_conf *conf);

#endif /* __MERO_DESIM_M0T1FS_H__ */

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
