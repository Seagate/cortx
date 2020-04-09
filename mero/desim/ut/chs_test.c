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

#if 0
/*
 * Seagate Cheetah 15K.7 SAS ST3450857SS
 *
 * http://www.seagate.com/staticfiles/support/disc/manuals/enterprise/cheetah/15K.7/100516226a.pdf
 *
 * Heads:             3*2
 * Cylinders:         107500
 * Sectors per track: 680--2040 (680 + (i << 7)/10000)
 * Rotational speed:  250 revolutions/sec
 *
 * Avg rotational latency: 2ms
 *
 * Seek:                read write
 *       average:        3.4 3.9
 *       track-to-track: 0.2 0.44
 *       full stroke:    6.6 7.4
 */
static struct chs_conf cheetah = {
	.cc_storage = {
		.sc_sector_size = 512,
	},
	.cc_heads          = 3*2,
	.cc_cylinders      = 107500,
	.cc_track_skew     = 0,
	.cc_cylinder_skew  = 0,
	.cc_sectors_min    =  680,
	.cc_sectors_max    = 2040,

	.cc_seek_avg            = 3400000,
	.cc_seek_track_to_track =  200000,
	.cc_seek_full_stroke    = 6600000,
	.cc_write_settle        =  220000,
	.cc_head_switch         =       0, /* unknown */
	.cc_command_latency     =       0, /* unknown */

	.cc_rps                 = 250
};
#endif

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

static struct chs_dev disc;
static struct elevator el;

static struct sim_thread seek_thr;

static double seekto(struct sim *s, int64_t sector, int sectors)
{
	sim_time_t now;

	now = s->ss_bolt;
	elevator_io(&el, SRT_READ, sector, sectors);
	return (s->ss_bolt - now)/1000;
}

enum {
	LBA_D   =   10,
	ROUNDS  =   10,
	TRACK_D =    8,
	TRACK_S = 2500
};

static void seek_test_thread(struct sim *s, struct sim_thread *t, void *arg)
{
	int64_t in_num_sect = -1;
	int i;
	int j;
	int k;
	int round;
	int sectors;
	int64_t sector;
	double latency;
	double seeklat[LBA_D][LBA_D];
	double seeksqr[LBA_D][LBA_D];


	in_num_sect = 1953525168;

	/*
	 * repeated read.
	 */
	for (i = 0; i < LBA_D; ++i) {
		sector = in_num_sect * i / LBA_D;
		for (sectors = 1; sectors <= (1 << 16); sectors *= 2) {
			double avg;
			double sqr;

			seekto(s, sector, sectors);
			for (avg = sqr = 0.0, round = 0; round < ROUNDS; ++round) {
				latency = seekto(s, sector, sectors);
				avg += latency;
				sqr += latency*latency;
			}
			avg /= ROUNDS;
			printf("reading %4i sectors at %i/%i: %6.0f (%6.0f)\n",
			       sectors, i, LBA_D, avg,
			       sqrt(sqr/ROUNDS - avg*avg));
		}
	}

	/*
	 * seeks
	 */
	for (round = 0; round < ROUNDS; ++round) {
		for (i = 0; i < LBA_D; ++i) {
			for (j = 0; j < LBA_D; ++j) {
				int64_t sector_from;
				int64_t sector_to;

				sector_from = in_num_sect * i / LBA_D;
				sector_to = in_num_sect * j / LBA_D;
				/*
				 * another loop to average rotational latency
				 * out.
				 */
				for (k = 0; k < TRACK_D; ++k) {
					seekto(s, sector_from +
					       TRACK_S*k/TRACK_D, 1);
					latency = seekto(s, sector_to +
							 TRACK_S*round/ROUNDS,
							 1);
					seeklat[i][j] += latency;
					seeksqr[i][j] += latency*latency;
				}
			}
			printf(".");
		}
		printf("\n");
	}
	for (i = 0; i < LBA_D; ++i) {
		for (j = 0; j < LBA_D; ++j) {
			latency = seeklat[i][j] / ROUNDS / TRACK_D;
			printf("[%6.0f %4.0f]", latency,
			       sqrt(seeksqr[i][j] / ROUNDS / TRACK_D -
				    latency*latency));
		}
		printf("\n");
	}
	for (i = 0; i < LBA_D; ++i) {
		for (j = 0; j < LBA_D; ++j)
			printf("%6.0f, ", seeklat[i][j] / ROUNDS / TRACK_D);
		printf("\n");
	}

	sim_thread_exit(t);
}

static int seek_test_start(struct sim_callout *co)
{
	sim_thread_init(co->sc_sim, &seek_thr, 0, seek_test_thread, NULL);
	return 1;
}

int main(int argc, char **argv)
{
	struct sim s;
	int result;

	result = m0_init(NULL);
	if (result == 0) {
		chs_conf_init(&ST31000640SS);
		chs_dev_init(&disc, &s, &ST31000640SS);
		elevator_init(&el, &disc.cd_storage);
		sim_init(&s);

		sim_timer_add(&s, 0, seek_test_start, NULL);
		sim_run(&s);

		cnt_dump_all();
		sim_log(&s, SLL_WARN, "done\n");
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
