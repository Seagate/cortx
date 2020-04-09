/* -*- C -*- */
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h> /* sqrt(3) */

#include "lib/assert.h"
#include "desim/sim.h"
#include "desim/chs.h"

/**
   @addtogroup desim desim
   @{
 */

long long int llabs(long long int j);

static void chs_submit(struct storage_dev *dev,
		       enum storage_req_type type,
		       sector_t sector, unsigned long count);

M0_INTERNAL void chs_conf_init(struct chs_conf *conf)
{
	sim_time_t track;
	sim_time_t avg;
	sim_time_t full;

	unsigned   nrcyl;
	unsigned   i;

	sector_t   sectors_track;
	sector_t   sectors_cyl;
	sector_t   sectors_cum;
	unsigned   smax;
	unsigned   smin;
	unsigned   cyl_in_zone;

	/*
	 * Assuming seek time of the form
	 *
	 *     seek_time(d) = track + alpha * (d - 1) + beta * sqrt(d - 1)
	 *
	 * where "d" is a number of cylinders to move over and "track" is a
	 * track-to-track seeking time, one obtains:
	 *
	 *     alpha = ( 4 * full - 6 * avg + 2 * track)/nrcyl, and
	 *     beta  = (-3 * full + 6 * avg - 3 * track)/sqrt(nrcyl)
	 *
	 * where "full" is "full stroke" and "avg" is average seek times as
	 * reported by drive manufacturer, and nrcyl is a number of cylinders on
	 * the disc.
	 */

	track = conf->cc_seek_track_to_track;
	avg   = conf->cc_seek_avg;
	full  = conf->cc_seek_full_stroke;
	nrcyl = conf->cc_cylinders;

	M0_ASSERT(4 * full + 2 * track >= 6 * avg);
	M0_ASSERT(3 * full + 3 * track <= 6 * avg);

	conf->cc_alpha = ( 4 * full - 6 * avg + 2 * track) / nrcyl;
	conf->cc_beta  = (-3 * full + 6 * avg - 3 * track) / sqrt(nrcyl);

	/*
	 * Setup an array used to map sector number (LBA) to CHS
	 * coordinates. Modern drives use "zoning"---a technique where number of
	 * sectors in a track depends on a cylinder. Zoning influences drive
	 * behaviour (seek and rotational delays, as well as transfer times).
	 *
	 * In the simplest case of "linear zoning" where the number of sectors
	 * grows linearly with the zone number, the total number of sectors in
	 * all cylinders up to a given one is quadratic in cylinder number
	 * (because it is sum of arithmetic progression). Hence, reverse mapping
	 * (from LBA to cylinder number) is given as a solution to quadratic
	 * equation. The resulting rounding leads to inconsistencies. For
	 * simplicity, just allocate an array with data for every cylinder. This
	 * has an advantage or being generalizable to non-linear zoning.
	 */

	conf->cc_zone = calloc(nrcyl, sizeof conf->cc_zone[0]);
	smax = conf->cc_sectors_max;
	smin = conf->cc_sectors_min;

	cyl_in_zone = conf->cc_cyl_in_zone;
	for (sectors_cum = 0, i = 0; i < nrcyl; ++i) {
		unsigned zone_start;

		/* first cylinder in zone */
		zone_start = i / cyl_in_zone * cyl_in_zone;
		/* assume linear "zoning". */
		sectors_track = smax - (smax - smin) * zone_start / (nrcyl - 1);
		sectors_cyl = sectors_track * conf->cc_heads;
		conf->cc_zone[i].track_sectors = sectors_track;
		conf->cc_zone[i].cyl_sectors   = sectors_cyl;
		conf->cc_zone[i].cyl_first     = sectors_cum;
		sectors_cum += sectors_cyl;
	}
	printf("total of %llu sectors\n", sectors_cum);
}

M0_INTERNAL void chs_conf_fini(struct chs_conf *conf)
{
	if (conf->cc_zone != NULL)
		free(conf->cc_zone);
}

M0_INTERNAL void chs_dev_init(struct chs_dev *dev, struct sim *sim,
			      struct chs_conf *conf)
{
	struct storage_dev *cd = &dev->cd_storage;
	char *name = cd->sd_name;

	cd->sd_sim    = dev->cd_todo.sc_sim = sim;
	cd->sd_conf   = &conf->cc_storage;
	cd->sd_submit = &chs_submit;
	dev->cd_conf  = conf;
	dev->cd_state = CDS_IDLE;
	cnt_init(&dev->cd_seek_time, NULL, "seek-time@%s", name);
	cnt_init(&dev->cd_rotation_time, NULL, "rotation-time@%s", name);
	cnt_init(&dev->cd_xfer_time, NULL, "xfer-time@%s", name);
	cnt_init(&dev->cd_read_size, NULL, "read-size@%s", name);
	cnt_init(&dev->cd_write_size, NULL, "write-size@%s", name);
}

M0_INTERNAL void chs_dev_fini(struct chs_dev *dev)
{
	cnt_fini(&dev->cd_write_size);
	cnt_fini(&dev->cd_read_size);
	cnt_fini(&dev->cd_xfer_time);
	cnt_fini(&dev->cd_rotation_time);
	cnt_fini(&dev->cd_seek_time);
}

/*
 * Number of sectors at a track in a given cylinder
 */
static unsigned chs_tracks(struct chs_conf *conf, unsigned cyl)
{
	M0_ASSERT(cyl < conf->cc_cylinders);
	return conf->cc_zone[cyl].track_sectors;
}

/*
 * Total number of sectors in cylinders up to, but not including given one.
 */
static sector_t chs_cylinder_sectors(struct chs_conf *conf, unsigned cyl)
{
	M0_ASSERT(cyl < conf->cc_cylinders);
	return conf->cc_zone[cyl].cyl_first;
}

/*
 * A cylinder containing a given sector.
 */
static unsigned chs_sector_cylinder(struct chs_conf *conf, sector_t sector)
{
	unsigned i;
	unsigned j;
	unsigned h;

	M0_ASSERT(sector <= conf->cc_zone[conf->cc_cylinders - 1].cyl_first +
	                 conf->cc_zone[conf->cc_cylinders - 1].cyl_sectors);

	i = 0;
	j = conf->cc_cylinders;
	while (i + 1 != j) {
		h = (i + j) / 2;
		if (conf->cc_zone[h].cyl_first <= sector)
			i = h;
		else
			j = h;
	}
	M0_ASSERT(conf->cc_zone[i].cyl_first <= sector);
	M0_ASSERT(i == conf->cc_cylinders - 1 ||
	       sector < conf->cc_zone[i + 1].cyl_first);
	return i;
}

/*
 * Convert LBA into CHS base.
 */
static void chs_sector_to_chs(struct chs_conf *conf, sector_t sector,
			      unsigned *head, unsigned *cylinder,
			      sector_t *sect_in_track)
{
	unsigned cyl_sects;
	unsigned track_sects;

	*cylinder = chs_sector_cylinder(conf, sector);
	M0_ASSERT(*cylinder < conf->cc_cylinders);

	track_sects = chs_tracks(conf, *cylinder);
	/* sectors in a cylinder */
	cyl_sects = track_sects * conf->cc_heads;
	M0_ASSERT(cyl_sects == conf->cc_zone[*cylinder].cyl_sectors);
	sector -= chs_cylinder_sectors(conf, *cylinder);
	M0_ASSERT(sector < cyl_sects);
	*head = sector / track_sects;
	M0_ASSERT(*head < conf->cc_heads);
	sector -= *head * track_sects;
	*sect_in_track = sector;
	M0_ASSERT(*sect_in_track < track_sects);
}

/*
 * Time to rotate over sectors in a track with track_sects sectors.
 */
static sim_time_t chs_sect_time(struct chs_conf *conf, unsigned track_sects,
				unsigned sectors)
{
	/* rotational delay = sectors / speed */
	return sectors * 1000000000ULL / (track_sects * conf->cc_rps);
}

/*
 * Simulate request processing.
 */
static sim_time_t chs_req(struct chs_dev *dev, enum storage_req_type type,
			  sector_t sector, long count)
{
	struct chs_conf *conf;

	sim_time_t seek;
	sim_time_t rotation;
	sim_time_t xfer;

	unsigned   head;
	unsigned   cylinder;
	sector_t   sector_target;
	sector_t   sector_target0;
	unsigned   armmove;
	sector_t   track_sects;
	sector_t   sector_at;
	long       sects_dist;

	M0_ASSERT(count >= 0);

	/*
	 * Request processing consists of:
	 *
	 *    - controller command overhead (constant);
	 *    - optional seek to the target cylinder;
	 *    - optional head switch;
	 *    - write settle for write requests;
	 *    - rotational delay;
	 *    - transfer loop:
	 *        * cylinder transfer loop:
	 *            . track transfer;
	 *            . head switch;
	 *        * track-to-track seek;
	 *        * write settle for write requests;
	 */

	conf = dev->cd_conf;
	chs_sector_to_chs(conf, sector, &head, &cylinder, &sector_target);
	sector_target0 = sector_target;
	armmove = abs((int)(cylinder - dev->cd_cylinder));

	/* Cylinder seek. */
	if (armmove != 0) {
		seek =
			conf->cc_seek_track_to_track +
			conf->cc_alpha * (armmove - 1) +
			conf->cc_beta * sqrt(armmove - 1);

		if (type == SRT_WRITE)
			seek += conf->cc_write_settle;
	} else
		seek = 0;

	/* Head switch */
	if (head != dev->cd_head)
		seek += conf->cc_head_switch;

	cnt_mod(&dev->cd_seek_time, seek);

	/* Rotational delay */
	track_sects = chs_tracks(conf, cylinder);

	/* sector currently under the head, ignoring skew */
	sector_at =
		/* linear speed in tracks per second for this cylinder */
		(track_sects * conf->cc_rps *
		 /* time since the beginning of simulation in seconds */
		 dev->cd_storage.sd_sim->ss_bolt / 1000000000) % track_sects;

	sects_dist = sector_at - sector_target;
	if (sects_dist < 0)
		sects_dist += track_sects;

	rotation = chs_sect_time(conf, track_sects, sects_dist);
	cnt_mod(&dev->cd_rotation_time, rotation);

	/* Target sector reached. Start transfer. */

	/* loop over cylinders */
	for (xfer = 0; cylinder < conf->cc_cylinders; cylinder++) {
		/* loop over tracks in a cylinder */
		for (; head < conf->cc_heads; head++) {
			/* transfer everything there is at the reached track. */
			sects_dist = track_sects - sector_target;
			if (sects_dist > count)
				sects_dist = count;
			xfer += chs_sect_time(conf, track_sects, sects_dist);
			count -= sects_dist;
			sector_target = 0;
			if (count > 0)
				xfer += conf->cc_head_switch;
			else
				break;
		}
		if (count > 0) {
			xfer += conf->cc_seek_track_to_track;
			if (type == SRT_WRITE)
				xfer += conf->cc_write_settle;
			head = 0;
			track_sects = chs_tracks(conf, cylinder);
		} else
			break;
	}
	cnt_mod(&dev->cd_xfer_time, xfer);

	sim_log(dev->cd_storage.sd_sim,
		SLL_TRACE, "D%s: [%4u:%u:%4llu] -> [%4u:%u:%4llu] %10llu "
		"%llu+%llu+%llu\n",
		dev->cd_storage.sd_name,
		dev->cd_cylinder, dev->cd_head, sector_at,
		cylinder, head, sector_target0, sector,
		seek, rotation, xfer);

	M0_ASSERT(count == 0);
	M0_ASSERT(cylinder < conf->cc_cylinders);
	M0_ASSERT(head     < conf->cc_heads);
	dev->cd_cylinder = cylinder;
	dev->cd_head     = head;

	return conf->cc_command_latency + seek + rotation + xfer;
}

static int chs_req_done(struct sim_callout *call)
{
	struct chs_dev *dev = call->sc_datum;

	M0_ASSERT(dev->cd_state == CDS_XFER);

	dev->cd_state = CDS_IDLE;
	if (dev->cd_storage.sd_end_io != NULL)
		dev->cd_storage.sd_end_io(&dev->cd_storage);
	return 0;
}

static void chs_submit(struct storage_dev *sdev,
		       enum storage_req_type type,
		       sector_t sector, unsigned long count)
{
	sim_time_t      reqtime;
	struct chs_dev *dev = container_of(sdev, struct chs_dev, cd_storage);

	M0_ASSERT(dev->cd_state == CDS_IDLE);

	reqtime = chs_req(dev, type, sector, count);
	sim_timer_rearm(&dev->cd_todo, reqtime, chs_req_done, dev);
	dev->cd_state = CDS_XFER;
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
