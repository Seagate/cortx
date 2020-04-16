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
 * Copyright 2009 ClusterStor.
 *
 * Nikita Danilov.
 */

#include <stdio.h>
#include <math.h> /* sqrt */

#include "mero/init.h"

#include "desim/sim.h"
#include "desim/storage.h"
#include "desim/chs.h"
#include "desim/net.h"
#include "desim/client.h"
#include "desim/elevator.h"

/**
   @addtogroup desim desim
   @{
 */

static struct net_conf net = {
	.nc_frag_size      = 4*1024,
	.nc_rpc_size       =   1024,
	.nc_rpc_delay_min  =   1000, /* microsecond */
	.nc_rpc_delay_max  =   5000,
	.nc_frag_delay_min =    100,
	.nc_frag_delay_max =   1000,
	.nc_rate_min       =  750000000,
	.nc_rate_max       = 1000000000, /* 1GB/sec QDR Infiniband */
	.nc_nob_max        =     ~0UL,
	.nc_msg_max        =     ~0UL
};

static struct net_srv  srv = {
	.ns_nr_threads     =      64,
	.ns_nr_devices     =       1,
	.ns_pre_bulk_min   =       0,
	.ns_pre_bulk_max   =    1000
};

static struct client_conf client = {
	.cc_nr_clients   =             0,
	.cc_nr_threads   =             0,
	.cc_total        = 100*1024*1024,
	.cc_count        =     1024*1024,
	.cc_opt_count    =     1024*1024,
	.cc_inflight_max =             8,
	.cc_delay_min    =             0,
	.cc_delay_max    =       1000000, /* millisecond */
	.cc_cache_max    =            ~0UL,
	.cc_dirty_max    =  32*1024*1024,
	.cc_net          = &net,
	.cc_srv          = &srv
};

static struct chs_conf ST31000640SS = { /* Barracuda ES.2 */
	.cc_storage = {
		.sc_sector_size = 512,
	},
	.cc_heads          = 4*2,    /* sginfo */
	.cc_cylinders      = 153352, /* sginfo */
	.cc_track_skew     = 160,    /* sginfo */
	.cc_cylinder_skew  = 76,     /* sginfo */
	.cc_sectors_min    = 1220,   /* sginfo */
	.cc_sectors_max    = 1800,   /* guess */
	.cc_cyl_in_zone    = 48080,  /* sginfo */

	.cc_seek_avg            =  8500000, /* data sheet */
	.cc_seek_track_to_track =   800000, /* data sheet */
	.cc_seek_full_stroke    = 16000000, /* guess */
	.cc_write_settle        =   500000, /* guess */
	.cc_head_switch         =   500000, /* guess */
	.cc_command_latency     =        0, /* unknown */

	.cc_rps                 = 7200/60
};

struct chs_dev disc;

static void workload_init(struct sim *s, int argc, char **argv)
{
	chs_conf_init(&ST31000640SS);
	net_init(&net);
	net_srv_init(s, &srv);
	chs_dev_init(&disc, s, &ST31000640SS);
	elevator_init(&srv.ns_el[0], &disc.cd_storage);
	client_init(s, &client);
}

static void workload_fini(void)
{
	client_fini(&client);
	net_srv_fini(&srv);
	net_fini(&net);
	elevator_fini(&srv.ns_el[0]);
	chs_dev_fini(&disc);
	chs_conf_fini(&ST31000640SS);
}

int main(int argc, char **argv)
{
	struct sim s;
	unsigned clients = atoi(argv[1]);
	unsigned threads = atoi(argv[2]);
	unsigned long long filesize;

	int result;

	result = m0_init(NULL);
	if (result == 0) {
		client.cc_nr_clients = clients;
		client.cc_nr_threads = threads;
		srv.ns_file_size = filesize = threads * client.cc_total;
		sim_init(&s);
		workload_init(&s, argc, argv);
		sim_run(&s);
		cnt_dump_all();
		workload_fini();
		sim_log(&s, SLL_WARN, "%5i %5i %10.2f\n", clients, threads,
			1000.0 * filesize * clients / s.ss_bolt);
		sim_fini(&s);
		m0_fini();
	}
	return result;
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
