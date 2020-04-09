/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Mikhail Antropov <mikhail.v.antropov@seagate.com>
 * Original creation date: 24-Feb-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SPIEL
#include "lib/trace.h"

#include <fcntl.h>  /* open */
#include <unistd.h> /* close */

#include "spiel/spiel.h"
#include "spiel/ut/spiel_ut_common.h"
#include "conf/cache.h"     /* m0_conf_cache_from_string */
#include "conf/obj_ops.h"   /* m0_conf_obj_create */
#include "conf/preload.h"   /* m0_confx_string_free */
#include "conf/load_fop.h"  /* m0_conf_segment_size */
#include "conf/helpers.h"   /* m0_confc_expired_cb */
#include "conf/ut/common.h"
#include "lib/finject.h"
#include "lib/fs.h"         /* m0_file_read */
#include "ut/misc.h"        /* M0_UT_PATH */
#include "ut/ut.h"

struct m0_spiel spiel;
extern struct m0_spiel_ut_reqh ut_reqh;

enum {
	SPIEL_UT_OBJ_PROFILE,
	SPIEL_UT_OBJ_POOL,
	SPIEL_UT_OBJ_PVER,
	SPIEL_UT_OBJ_PVER_F,
	SPIEL_UT_OBJ_NODE,
	SPIEL_UT_OBJ_NODE2,
	SPIEL_UT_OBJ_PROCESS,
	SPIEL_UT_OBJ_PROCESS2,
	SPIEL_UT_OBJ_SERVICE,
	SPIEL_UT_OBJ_SERVICE2,
	SPIEL_UT_OBJ_SERVICE3,
	SPIEL_UT_OBJ_SERVICE4,
	SPIEL_UT_OBJ_SERVICE5,
	SPIEL_UT_OBJ_SERVICE6,
	SPIEL_UT_OBJ_SERVICE7,
	SPIEL_UT_OBJ_SERVICE8,
	SPIEL_UT_OBJ_SERVICE9,
	SPIEL_UT_OBJ_SERVICE10,
	SPIEL_UT_OBJ_SERVICE11,
	SPIEL_UT_OBJ_SERVICE12,
	SPIEL_UT_OBJ_SERVICE13,
	SPIEL_UT_OBJ_SDEV,
	SPIEL_UT_OBJ_SDEV2,
	SPIEL_UT_OBJ_SDEV3,
	SPIEL_UT_OBJ_SDEV4,
	SPIEL_UT_OBJ_SDEV5,
	SPIEL_UT_OBJ_SDEV6,
	SPIEL_UT_OBJ_SDEV7,
	SPIEL_UT_OBJ_SDEV8,
	SPIEL_UT_OBJ_SDEV9,
	SPIEL_UT_OBJ_SDEV10,
	SPIEL_UT_OBJ_SITE,
	SPIEL_UT_OBJ_RACK,
	SPIEL_UT_OBJ_RACK2,
	SPIEL_UT_OBJ_ENCLOSURE,
	SPIEL_UT_OBJ_ENCLOSURE2,
	SPIEL_UT_OBJ_CONTROLLER,
	SPIEL_UT_OBJ_CONTROLLER2,
	SPIEL_UT_OBJ_DRIVE,
	SPIEL_UT_OBJ_DRIVE2,
	SPIEL_UT_OBJ_DRIVE3,
	SPIEL_UT_OBJ_DRIVE4,
	SPIEL_UT_OBJ_DRIVE5,
	SPIEL_UT_OBJ_DRIVE6,
	SPIEL_UT_OBJ_DRIVE7,
	SPIEL_UT_OBJ_DRIVE8,
	SPIEL_UT_OBJ_DRIVE9,
	SPIEL_UT_OBJ_DRIVE10,
	SPIEL_UT_OBJ_SITE_V,
	SPIEL_UT_OBJ_RACK_V,
	SPIEL_UT_OBJ_ENCLOSURE_V,
	SPIEL_UT_OBJ_CONTROLLER_V,
	SPIEL_UT_OBJ_DRIVE_V
};

static struct m0_fid spiel_obj_fid[] = {
	[SPIEL_UT_OBJ_PROFILE]      = M0_FID_TINIT('p', 1, 0 ),
	[SPIEL_UT_OBJ_POOL]         = M0_FID_TINIT('o', 1, 4 ),
	[SPIEL_UT_OBJ_PVER]         = M0_FID_TINIT('v', 5, 5 ),
	[SPIEL_UT_OBJ_PVER_F]       = M0_FID_TINIT('v', 6, 6 ),
	[SPIEL_UT_OBJ_NODE]         = M0_FID_TINIT('n', 1, 2 ),
	[SPIEL_UT_OBJ_NODE2]        = M0_FID_TINIT('n', 1, 48),
	[SPIEL_UT_OBJ_PROCESS]      = M0_FID_TINIT('r', 1, 5 ),
	[SPIEL_UT_OBJ_PROCESS2]     = M0_FID_TINIT('r', 1, 49),
	[SPIEL_UT_OBJ_SERVICE]      = M0_FID_TINIT('s', 1, 9 ),
	[SPIEL_UT_OBJ_SERVICE2]     = M0_FID_TINIT('s', 1, 10),
	[SPIEL_UT_OBJ_SERVICE3]     = M0_FID_TINIT('s', 1, 20),
	[SPIEL_UT_OBJ_SERVICE4]     = M0_FID_TINIT('s', 1, 21),
	[SPIEL_UT_OBJ_SERVICE5]     = M0_FID_TINIT('s', 1, 22),
	[SPIEL_UT_OBJ_SERVICE6]     = M0_FID_TINIT('s', 1, 23),
	[SPIEL_UT_OBJ_SERVICE7]     = M0_FID_TINIT('s', 1, 24),
	[SPIEL_UT_OBJ_SERVICE8]     = M0_FID_TINIT('s', 1, 25),
	[SPIEL_UT_OBJ_SERVICE9]     = M0_FID_TINIT('s', 1, 26),
	[SPIEL_UT_OBJ_SERVICE10]    = M0_FID_TINIT('s', 1, 27),
	[SPIEL_UT_OBJ_SERVICE11]    = M0_FID_TINIT('s', 1, 28),
	[SPIEL_UT_OBJ_SERVICE12]    = M0_FID_TINIT('s', 1, 29),
	[SPIEL_UT_OBJ_SERVICE13]    = M0_FID_TINIT('s', 1, 100),
	[SPIEL_UT_OBJ_SDEV]         = M0_FID_TINIT('d', 1, 15),
	[SPIEL_UT_OBJ_SDEV2]        = M0_FID_TINIT('d', 1, 71),
	[SPIEL_UT_OBJ_SDEV3]        = M0_FID_TINIT('d', 1, 72),
	[SPIEL_UT_OBJ_SDEV4]        = M0_FID_TINIT('d', 1, 73),
	[SPIEL_UT_OBJ_SDEV5]        = M0_FID_TINIT('d', 1, 74),
	[SPIEL_UT_OBJ_SDEV6]        = M0_FID_TINIT('d', 1, 51),
	[SPIEL_UT_OBJ_SDEV7]        = M0_FID_TINIT('d', 1, 83),
	[SPIEL_UT_OBJ_SDEV8]        = M0_FID_TINIT('d', 1, 84),
	[SPIEL_UT_OBJ_SDEV9]        = M0_FID_TINIT('d', 1, 85),
	[SPIEL_UT_OBJ_SDEV10]        = M0_FID_TINIT('d', 1, 86),
	[SPIEL_UT_OBJ_SITE]         = M0_FID_TINIT('S', 1, 3 ),
	[SPIEL_UT_OBJ_RACK]         = M0_FID_TINIT('a', 1, 3 ),
	[SPIEL_UT_OBJ_RACK2]        = M0_FID_TINIT('a', 1, 52),
	[SPIEL_UT_OBJ_ENCLOSURE]    = M0_FID_TINIT('e', 1, 7 ),
	[SPIEL_UT_OBJ_ENCLOSURE2]   = M0_FID_TINIT('e', 1, 53),
	[SPIEL_UT_OBJ_CONTROLLER]   = M0_FID_TINIT('c', 1, 11),
	[SPIEL_UT_OBJ_CONTROLLER2]  = M0_FID_TINIT('c', 1, 54),
	[SPIEL_UT_OBJ_DRIVE]        = M0_FID_TINIT('k', 1, 16),
	[SPIEL_UT_OBJ_DRIVE2]       = M0_FID_TINIT('k', 1, 75),
	[SPIEL_UT_OBJ_DRIVE3]       = M0_FID_TINIT('k', 1, 76),
	[SPIEL_UT_OBJ_DRIVE4]       = M0_FID_TINIT('k', 1, 77),
	[SPIEL_UT_OBJ_DRIVE5]       = M0_FID_TINIT('k', 1, 78),
	[SPIEL_UT_OBJ_DRIVE6]       = M0_FID_TINIT('k', 1, 55),
	[SPIEL_UT_OBJ_DRIVE7]       = M0_FID_TINIT('k', 1, 87),
	[SPIEL_UT_OBJ_DRIVE8]       = M0_FID_TINIT('k', 1, 88),
	[SPIEL_UT_OBJ_DRIVE9]       = M0_FID_TINIT('k', 1, 89),
	[SPIEL_UT_OBJ_DRIVE10]      = M0_FID_TINIT('k', 1, 90),
	[SPIEL_UT_OBJ_SITE_V]       = M0_FID_TINIT('j', 13, 14),
	[SPIEL_UT_OBJ_RACK_V]       = M0_FID_TINIT('j', 14, 14),
	[SPIEL_UT_OBJ_ENCLOSURE_V]  = M0_FID_TINIT('j', 15, 15),
	[SPIEL_UT_OBJ_CONTROLLER_V] = M0_FID_TINIT('j', 16, 16),
	[SPIEL_UT_OBJ_DRIVE_V]      = M0_FID_TINIT('j', 17, 17)
};

static int spiel_copy_file(const char *source, const char* dest)
{
	char buf[1024];
	int in_fd;
	int out_fd;
	size_t result;
	int rc = 0;

	in_fd = open(source, O_RDONLY);
	if (in_fd < 0)
		return -EINVAL;

	out_fd = open(dest, O_WRONLY | O_CREAT, 0666);
	if (in_fd < 0) {
		close(in_fd);
		return -EINVAL;
	}

	while (1) {
		rc = (result = read(in_fd, &buf[0], sizeof(buf))) >= 0 ?
		     0 : -EINVAL;
		if (result == 0 || rc != 0)
			break;
		rc = write(out_fd, &buf[0], result) == result ? 0 : -EINVAL;
		if (rc != 0)
			break;
	}
	close(in_fd);
	close(out_fd);

	return rc;
}

static void spiel_conf_ut_init(void)
{
	struct m0_rconfc   *rconfc =
	      &ut_reqh.sur_confd_srv.rsx_mero_ctx.cc_reqh_ctx.rc_reqh.rh_rconfc;
	int                 rc;

	rc = m0_conf_ut_ast_thread_init();
	M0_ASSERT(rc == 0);
	spiel_copy_file(M0_UT_PATH("conf.xc"), "tmp-conf.xc");

	/* Use a copy of conf.xc file as confd path - file may have changed */
	m0_spiel__ut_init(&spiel, "tmp-conf.xc", false);
	m0_rconfc_lock(rconfc);
	rconfc->rc_expired_cb = &m0_confc_expired_cb;
	rconfc->rc_ready_cb   = &m0_confc_ready_cb;
	m0_rconfc_unlock(rconfc);
	/** @todo Use fid convert function to set kind. */
	spiel_obj_fid[SPIEL_UT_OBJ_PVER_F].f_container |=
		(uint64_t)1 << (64 - 10);
}

static void spiel_conf_ut_fini(void)
{
	int rc;

	m0_spiel__ut_fini(&spiel, false);

	rc = system("rm -rf confd");
	M0_UT_ASSERT(rc != -1);
	rc = unlink("tmp-conf.xc");
	M0_UT_ASSERT(rc == 0);
	rc = m0_conf_ut_ast_thread_fini();
	M0_UT_ASSERT(rc == 0);
}

enum spiel_conf_opts {
	SCO_NO_CHANGE,
	SCO_DROP_PROCESS = 1 << 1,               /* unused */
	SCO_DROP_SERVICE = 1 << 2,
	SCO_DROP_SDEV    = 1 << 3,               /* unused */
	SCO_ADD_PROCESS  = 1 << (1 + 16),        /* unused */
	SCO_ADD_SERVICE  = 1 << (2 + 16),
	SCO_ADD_SDEV     = 1 << (3 + 16),        /* unused */
};

static void spiel_conf_create_conf_with_opt(struct m0_spiel    *spiel,
					    struct m0_spiel_tx *tx,
					    uint64_t            opts)
{
	int                           rc;
	struct m0_pdclust_attr        pdclust_attr = { .pa_N=1,
						       .pa_K=0,
						       .pa_P=1};
	const char                   *params[] = { "11111", "22222", NULL };
	const char                   *ep[] = { SERVER_ENDPOINT_ADDR, NULL };
	struct m0_spiel_service_info  service_info = {.svi_endpoints=ep};
	uint32_t                      tolerance[] = {0, 0, 0, 0, 1};
	uint32_t                      allowance[] = {0, 0, 0, 0, 1};
	struct m0_bitmap              bitmap;

	m0_bitmap_init(&bitmap, 32);
	m0_bitmap_set(&bitmap, 0, true);
	m0_bitmap_set(&bitmap, 1, true);

	m0_spiel_tx_open(spiel, tx);

	rc = m0_spiel_root_add(tx,
			       &M0_FID0,
			       &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
			       &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
			       10, params);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_profile_add(tx, &spiel_obj_fid[SPIEL_UT_OBJ_PROFILE]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_profile_pool_add(tx, &spiel_obj_fid[SPIEL_UT_OBJ_PROFILE],
				           &spiel_obj_fid[SPIEL_UT_OBJ_POOL]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_pool_add(tx, &spiel_obj_fid[SPIEL_UT_OBJ_POOL], 0);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_site_add(tx, &spiel_obj_fid[SPIEL_UT_OBJ_SITE]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_rack_add(tx, &spiel_obj_fid[SPIEL_UT_OBJ_RACK],
			           &spiel_obj_fid[SPIEL_UT_OBJ_SITE]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_rack_add(tx, &spiel_obj_fid[SPIEL_UT_OBJ_RACK2],
			           &spiel_obj_fid[SPIEL_UT_OBJ_SITE]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_enclosure_add(tx, &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE],
				        &spiel_obj_fid[SPIEL_UT_OBJ_RACK]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_enclosure_add(tx, &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE2],
				        &spiel_obj_fid[SPIEL_UT_OBJ_RACK2]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_node_add(tx,
			       &spiel_obj_fid[SPIEL_UT_OBJ_NODE],
			       256,
			       2,
			       10,
			       0xff00ff00);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_node_add(tx,
			       &spiel_obj_fid[SPIEL_UT_OBJ_NODE2],
			       256,
			       2,
			       10,
			       0xff00ff00);
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_controller_add(tx,
				     &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER],
				     &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE],
				     &spiel_obj_fid[SPIEL_UT_OBJ_NODE]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_controller_add(tx,
				     &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER2],
				     &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE2],
				     &spiel_obj_fid[SPIEL_UT_OBJ_NODE2]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx,
				&spiel_obj_fid[SPIEL_UT_OBJ_DRIVE],
				&spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx,
				&spiel_obj_fid[SPIEL_UT_OBJ_DRIVE2],
				&spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx,
				&spiel_obj_fid[SPIEL_UT_OBJ_DRIVE3],
				&spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx,
				&spiel_obj_fid[SPIEL_UT_OBJ_DRIVE4],
				&spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx,
				&spiel_obj_fid[SPIEL_UT_OBJ_DRIVE5],
				&spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx,
				&spiel_obj_fid[SPIEL_UT_OBJ_DRIVE6],
				&spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER2]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx,
				&spiel_obj_fid[SPIEL_UT_OBJ_DRIVE7],
				&spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER2]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx,
				&spiel_obj_fid[SPIEL_UT_OBJ_DRIVE8],
				&spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER2]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx,
				&spiel_obj_fid[SPIEL_UT_OBJ_DRIVE9],
				&spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER2]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx,
				&spiel_obj_fid[SPIEL_UT_OBJ_DRIVE10],
				&spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER2]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_pver_actual_add(tx,
				      &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				      &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
				      &pdclust_attr,
				      tolerance, ARRAY_SIZE(tolerance));
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_pver_formulaic_add(tx,
				         &spiel_obj_fid[SPIEL_UT_OBJ_PVER_F],
				         &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
				         1,
				         &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				         allowance, ARRAY_SIZE(allowance));
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_site_v_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE_V],
				 &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_rack_v_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_RACK_V],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE_V],
				 &spiel_obj_fid[SPIEL_UT_OBJ_RACK]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_enclosure_v_add(tx,
				      &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE_V],
				      &spiel_obj_fid[SPIEL_UT_OBJ_RACK_V],
				      &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_controller_v_add(tx,
				      &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER_V],
				      &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE_V],
				      &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_v_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE_V],
				  &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER_V],
				  &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_pool_version_done(tx, &spiel_obj_fid[SPIEL_UT_OBJ_PVER]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_process_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &spiel_obj_fid[SPIEL_UT_OBJ_NODE],
				  &bitmap, 4000, 1, 2, 3, ep[0]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_process_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS2],
				  &spiel_obj_fid[SPIEL_UT_OBJ_NODE],
				  &bitmap, 4000, 1, 2, 3, ep[0]);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_IOS;
			rc = m0_spiel_service_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_MDS;
			rc = m0_spiel_service_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE2],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_CONFD;
			rc = m0_spiel_service_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE3],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_ADDB2;
			rc = m0_spiel_service_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE4],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_RMS;
			rc = m0_spiel_service_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE5],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_HA;
			rc = m0_spiel_service_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE6],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_SNS_REP;
			rc = m0_spiel_service_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE7],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_SNS_REB;
			rc = m0_spiel_service_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE8],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_DS1;
			rc = m0_spiel_service_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE9],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_IOS;
			rc = m0_spiel_service_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE10],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS2],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	if ((opts & SCO_DROP_SERVICE) == 0) {
		service_info.svi_type = M0_CST_DS2;
		rc = m0_spiel_service_add(tx,
					  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE11],
					  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS2],
					  &service_info);
		M0_UT_ASSERT(rc == 0);
	}

	if ((opts & SCO_ADD_SERVICE) != 0) {
		service_info.svi_type = M0_CST_DS2;
		rc = m0_spiel_service_add(tx,
					  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE12],
					  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS2],
					  &service_info);
		M0_UT_ASSERT(rc == 0);
	}

	service_info.svi_type = M0_CST_FDMI;
			rc = m0_spiel_service_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE13],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_device_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "/dev/sdev2");
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_device_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV2],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE2],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "/dev/sdev2");
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_device_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV3],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE3],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "/dev/sdev3");
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_device_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV4],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE4],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "/dev/sdev4");
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_device_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV5],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE5],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "/dev/sdev5");
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_device_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV6],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE6],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "/dev/sdev0");
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_device_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV7],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE7],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "/dev/sdev1");
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_device_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV8],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE8],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "/dev/sdev2");
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_device_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV9],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE9],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "/dev/sdev3");
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_device_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV10],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE10],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "/dev/sdev4");
	M0_UT_ASSERT(rc == 0);
	m0_bitmap_fini(&bitmap);
}

/**
 * Creates configuration identical to conf.xc, i.e. having no changes.
 */
static void spiel_conf_create_configuration(struct m0_spiel    *spiel,
					    struct m0_spiel_tx *tx)
{
	spiel_conf_create_conf_with_opt(spiel, tx, SCO_NO_CHANGE);
}

#define FID_MOVE(fid, step) 	\
	&M0_FID_INIT((fid).f_container, (fid).f_key + (step))

static void spiel_conf_create_invalid_configuration(struct m0_spiel    *spiel,
						    struct m0_spiel_tx *tx)
{
	int                           rc;
	struct m0_pdclust_attr        pdclust_attr = { .pa_N=0,
						       .pa_K=0,
						       .pa_P=0};
	const char                   *params[] = { "11111", "22222", NULL };
	const char                   *ep[] = { SERVER_ENDPOINT_ADDR, NULL };
	struct m0_spiel_service_info  service_info = {.svi_endpoints=ep};
	uint32_t                      tolerance[] = {0, 0, 0, 0, 1};
	struct m0_bitmap              bitmap;

	m0_bitmap_init(&bitmap, 32);
	m0_bitmap_set(&bitmap, 0, true);
	m0_bitmap_set(&bitmap, 1, true);

	m0_spiel_tx_open(spiel, tx);
	rc = m0_spiel_root_add(tx,
			       &spiel_obj_fid[SPIEL_UT_OBJ_PROFILE],
			       &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
			       &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
			       10, params);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_profile_add(tx,
			FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_PROFILE], 1));
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_pool_add(tx,
			       FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_POOL], 3),
			       0);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_site_add(tx,
			       FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_SITE], 4));
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_rack_add(tx,
			       FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_RACK], 4),
			       &spiel_obj_fid[SPIEL_UT_OBJ_SITE]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_enclosure_add(tx,
			FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE], 5),
			&spiel_obj_fid[SPIEL_UT_OBJ_RACK]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_node_add(tx,
			       FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_NODE], 6),
			       256,
			       2,
			       10,
			       0xff00ff00);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_controller_add(tx,
				     FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER], 7),
				     &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE],
				     &spiel_obj_fid[SPIEL_UT_OBJ_NODE]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx,
				FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_DRIVE], 8),
				&spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_pver_actual_add(tx,
				      FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_PVER], 9),
				      &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
				      &pdclust_attr,
				      tolerance, ARRAY_SIZE(tolerance));
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_site_v_add(tx,
				 FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_SITE_V], 10),
				 &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_rack_v_add(tx,
				 FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_RACK_V], 10),
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE_V],
				 &spiel_obj_fid[SPIEL_UT_OBJ_RACK]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_process_add(tx,
				  FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_PROCESS], 11),
				  &spiel_obj_fid[SPIEL_UT_OBJ_NODE],
				  &bitmap, 4000, 1, 2, 3, ep[0]);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_IOS;
	rc = m0_spiel_service_add(tx,
				  FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_SERVICE], 12),
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_device_add(tx,
				 FID_MOVE(spiel_obj_fid[SPIEL_UT_OBJ_SDEV], 13),
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE],
				 2, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "fake_filename");
	M0_UT_ASSERT(rc == 0);

	m0_bitmap_fini(&bitmap);
}

static void spiel_conf_create_ok(void)
{
	struct m0_spiel_tx tx;

	spiel_conf_ut_init();
	spiel_conf_create_configuration(&spiel, &tx);
	m0_spiel_tx_close(&tx);
	spiel_conf_ut_fini();
}

static void spiel_conf_pver_check(struct m0_spiel_tx *tx)
{
	struct m0_conf_obj        *obj;
	struct m0_conf_pver       *pver;
	struct m0_conf_rack       *rack;
	struct m0_conf_enclosure  *enclosure;
	struct m0_conf_controller *controller;

	m0_mutex_lock(&tx->spt_lock);
	obj = m0_conf_cache_lookup(&tx->spt_cache,
				   &spiel_obj_fid[SPIEL_UT_OBJ_PVER]);
	pver = M0_CONF_CAST(obj, m0_conf_pver);
	M0_UT_ASSERT(pver != NULL);

	obj = m0_conf_cache_lookup(&tx->spt_cache,
				   &spiel_obj_fid[SPIEL_UT_OBJ_RACK]);
	rack = M0_CONF_CAST(obj, m0_conf_rack);
	M0_UT_ASSERT(rack != NULL);
	M0_UT_ASSERT(rack->cr_pvers != NULL);
	M0_UT_ASSERT(rack->cr_pvers[0] == pver);
	M0_UT_ASSERT(rack->cr_pvers[1] == NULL);

	obj = m0_conf_cache_lookup(&tx->spt_cache,
				   &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE]);
	enclosure = M0_CONF_CAST(obj, m0_conf_enclosure);
	M0_UT_ASSERT(enclosure != NULL);
	M0_UT_ASSERT(enclosure->ce_pvers != NULL);
	M0_UT_ASSERT(enclosure->ce_pvers[0] == pver);
	M0_UT_ASSERT(enclosure->ce_pvers[1] == NULL);

	obj = m0_conf_cache_lookup(&tx->spt_cache,
				   &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	controller = M0_CONF_CAST(obj, m0_conf_controller);
	M0_UT_ASSERT(controller != NULL);
	M0_UT_ASSERT(controller->cc_pvers != NULL);
	M0_UT_ASSERT(controller->cc_pvers[0] == pver);
	M0_UT_ASSERT(controller->cc_pvers[1] == NULL);

	obj = m0_conf_cache_lookup(&tx->spt_cache,
				   &spiel_obj_fid[SPIEL_UT_OBJ_PVER_F]);
	pver = M0_CONF_CAST(obj, m0_conf_pver);
	M0_UT_ASSERT(pver != NULL);

	m0_mutex_unlock(&tx->spt_lock);
}

static void spiel_conf_pver_delete_check(struct m0_spiel_tx *tx)
{
	struct m0_conf_obj *obj;

	m0_mutex_lock(&tx->spt_lock);
	obj = m0_conf_cache_lookup(&tx->spt_cache,
				   &spiel_obj_fid[SPIEL_UT_OBJ_PVER]);
	M0_UT_ASSERT(obj == NULL);

	obj = m0_conf_cache_lookup(&tx->spt_cache,
				   &spiel_obj_fid[SPIEL_UT_OBJ_PVER_F]);
	M0_UT_ASSERT(obj == NULL);

	obj = m0_conf_cache_lookup(&tx->spt_cache,
				   &spiel_obj_fid[SPIEL_UT_OBJ_RACK_V]);
	M0_UT_ASSERT(obj == NULL);

	obj = m0_conf_cache_lookup(&tx->spt_cache,
				   &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE_V]);
	M0_UT_ASSERT(obj == NULL);

	obj = m0_conf_cache_lookup(&tx->spt_cache,
				   &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER_V]);
	M0_UT_ASSERT(obj == NULL);

	m0_mutex_unlock(&tx->spt_lock);
}

static void spiel_conf_create_pver_tree(struct m0_spiel_tx *tx)
{
	int                    rc;
	struct m0_fid          fake_fid = spiel_obj_fid[SPIEL_UT_OBJ_PROFILE];
	struct m0_pdclust_attr pdclust_attr = { .pa_N=1, .pa_K=1, .pa_P=3};
	struct m0_pdclust_attr pdclust_attr_invalid = { .pa_N=1, .pa_K=2,
							.pa_P=3};
	uint32_t               tolerance[] = {0, 0, 0, 0, 1};
	uint32_t               allowance[] = {0, 0, 0, 0, 1};

	/* Actual Pool version */
	rc = m0_spiel_pver_actual_add(tx,
				      &fake_fid,
				      &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
				      &pdclust_attr,
				      tolerance, ARRAY_SIZE(tolerance));
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_pver_actual_add(tx,
				      &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				      &fake_fid,
				      &pdclust_attr,
				      tolerance, ARRAY_SIZE(tolerance));
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_pver_actual_add(tx,
				      &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				      &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
				      &pdclust_attr,
				      NULL,
				      0);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_pver_actual_add(tx,
				      &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				      &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
				      &pdclust_attr_invalid,
				      tolerance, ARRAY_SIZE(tolerance));
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_pver_actual_add(tx,
				      &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				      &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
				      &pdclust_attr,
				      tolerance, ARRAY_SIZE(tolerance));
	M0_UT_ASSERT(rc == 0);

	/* Formulaic pool version */
	rc = m0_spiel_pver_formulaic_add(tx,
				         &fake_fid,
				         &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
				         1,
				         &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				         allowance, ARRAY_SIZE(allowance));
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_pver_formulaic_add(tx,
					 &spiel_obj_fid[SPIEL_UT_OBJ_PVER_F],
					 &fake_fid,
				         1,
				         &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				         allowance, ARRAY_SIZE(allowance));
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_pver_formulaic_add(tx,
				         &spiel_obj_fid[SPIEL_UT_OBJ_PVER_F],
				         &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
				         1,
				         &fake_fid,
				         allowance, ARRAY_SIZE(allowance));
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_pver_formulaic_add(tx,
				         &spiel_obj_fid[SPIEL_UT_OBJ_PVER_F],
				         &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
				         1,
				         &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				         NULL, 0);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_pver_formulaic_add(tx,
				         &spiel_obj_fid[SPIEL_UT_OBJ_PVER_F],
				         &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
				         1,
				         &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				         allowance, ARRAY_SIZE(allowance));
	M0_UT_ASSERT(rc == 0);

	/* site-v */
	rc = m0_spiel_site_v_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE_V],
				 &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				 &fake_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_site_v_add(tx,
				 &fake_fid,
				 &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_site_v_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE_V],
				 &fake_fid,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_site_v_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE_V],
				 &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE]);
	M0_UT_ASSERT(rc == 0);

	/* rack-v */
	rc = m0_spiel_rack_v_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_RACK_V],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE_V],
				 &fake_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_rack_v_add(tx,
				 &fake_fid,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE_V],
				 &spiel_obj_fid[SPIEL_UT_OBJ_RACK]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_rack_v_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_RACK_V],
				 &fake_fid,
				 &spiel_obj_fid[SPIEL_UT_OBJ_RACK]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_rack_v_add(tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_RACK_V],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SITE_V],
				 &spiel_obj_fid[SPIEL_UT_OBJ_RACK]);
	M0_UT_ASSERT(rc == 0);

	/* enclosure-v */
	rc = m0_spiel_enclosure_v_add(tx,
				      &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE_V],
				      &spiel_obj_fid[SPIEL_UT_OBJ_RACK_V],
				      &fake_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_enclosure_v_add(tx,
				      &fake_fid,
				      &spiel_obj_fid[SPIEL_UT_OBJ_RACK_V],
				      &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_enclosure_v_add(tx,
				      &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE_V],
				      &fake_fid,
				      &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_enclosure_v_add(tx,
				      &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE_V],
				      &spiel_obj_fid[SPIEL_UT_OBJ_RACK_V],
				      &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE]);
	M0_UT_ASSERT(rc == 0);

	/* controller-v */
	rc = m0_spiel_controller_v_add(tx,
				      &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER_V],
				      &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE_V],
				      &fake_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_controller_v_add(tx,
				      &fake_fid,
				      &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE_V],
				      &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_controller_v_add(tx,
				      &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER_V],
				      &fake_fid,
				      &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_controller_v_add(tx,
				      &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER_V],
				      &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE_V],
				      &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	M0_UT_ASSERT(rc == 0);

	/* drive-v */
	rc = m0_spiel_drive_v_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE_V],
				  &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER_V],
				  &fake_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_drive_v_add(tx,
				  &fake_fid,
				  &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER_V],
				  &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_drive_v_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE_V],
				  &fake_fid,
				  &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_drive_v_add(tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE_V],
				  &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER_V],
				  &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE]);
	M0_UT_ASSERT(rc == 0);
}

static void spiel_conf_create_fail(void)
{
	struct m0_spiel_tx            tx;
	const char                   *ep[] = { SERVER_ENDPOINT_ADDR, NULL };
	int                           rc;
	const char                   *params[] = { "11111", "22222", NULL };
	struct m0_spiel_service_info  service_info = {
		.svi_endpoints = params };
	struct m0_fid                 fake_profile_fid =
					spiel_obj_fid[SPIEL_UT_OBJ_NODE];
	struct m0_fid                 fake_fid =
					spiel_obj_fid[SPIEL_UT_OBJ_PROFILE];
	struct m0_bitmap              bitmap;
	uint64_t                      zero_mask[] = {0, 0};
	struct m0_bitmap              zero_bitmap = {
					.b_nr = 2,
					.b_words = zero_mask
					};

	spiel_conf_ut_init();
	m0_bitmap_init(&bitmap, 32);
	m0_bitmap_set(&bitmap, 0, true);
	m0_bitmap_set(&bitmap, 1, true);

	m0_spiel_tx_open(&spiel, &tx);
	/**
	 * alloc fail for rt_params.
	 * Note: 'm0_fi_enable_once' is not used here as some other
	 * threads were consuming this fault injection point by calling
	 * 'm0_alloc' before this thread could call that function leading
	 * to the assert condition below getting hit. With 'm0_fi_enable',
	 * those threads can still get -ENOMEM in the interim but that
	 * is acceptable for now.
	 **/
	m0_fi_enable("m0_alloc", "fail_allocation");
	rc = m0_spiel_root_add(&tx,
			       &spiel_obj_fid[SPIEL_UT_OBJ_PROFILE],
			       &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
			       &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
			       10, params);
	m0_fi_disable("m0_alloc", "fail_allocation");
	M0_UT_ASSERT(rc == -ENOMEM);

	/* alloc fail for rt_params */
	m0_fi_enable_off_n_on_m("m0_strings_dup",
				"strdup_failed", 1, 1);
	rc = m0_spiel_root_add(&tx,
			       &spiel_obj_fid[SPIEL_UT_OBJ_PROFILE],
			       &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
			       &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
			       10, params);
	m0_fi_disable("m0_strings_dup", "strdup_failed");
	M0_UT_ASSERT(rc == -ENOMEM);

	/* Check that M0_FID0 can be passed as imeta_pver. */
	rc = m0_spiel_root_add(&tx,
			       &spiel_obj_fid[SPIEL_UT_OBJ_PROFILE],
			       &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
			       &M0_FID0,
			       10, params);
	M0_UT_ASSERT(rc == 0);

	/* Profile */
	rc = m0_spiel_profile_add(&tx, &fake_profile_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_profile_add(&tx, NULL);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_profile_add(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_NODE]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_profile_add(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_PROFILE]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_profile_add(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_PROFILE]);
	M0_UT_ASSERT(rc == -EEXIST);

	rc = m0_spiel_root_add(&tx,
			       &spiel_obj_fid[SPIEL_UT_OBJ_PROFILE],
			       &fake_fid,
			       &spiel_obj_fid[SPIEL_UT_OBJ_PVER],
			       10, params);
	M0_UT_ASSERT(rc == -EINVAL);
	rc = m0_spiel_root_add(&tx,
			       &spiel_obj_fid[SPIEL_UT_OBJ_PROFILE],
			       &spiel_obj_fid[SPIEL_UT_OBJ_POOL],
			       &fake_fid,
			       10, params);
	M0_UT_ASSERT(rc == -EINVAL);

	/* Pool */
	rc = m0_spiel_pool_add(&tx, &fake_fid, 0);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_pool_add(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_POOL], 0);
	M0_UT_ASSERT(rc == 0);

	/* Pool at profile */
	rc = m0_spiel_profile_pool_add(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_PROFILE],
				            &spiel_obj_fid[SPIEL_UT_OBJ_POOL]);
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_profile_pool_add(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_PROFILE],
				            &spiel_obj_fid[SPIEL_UT_OBJ_POOL]);
	M0_UT_ASSERT(rc == -EEXIST);

	/* Site */
	rc = m0_spiel_site_add(&tx, &fake_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_site_add(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_SITE]);
	M0_UT_ASSERT(rc == 0);

	/* Rack */
	rc = m0_spiel_rack_add(&tx, &fake_fid,
			       &spiel_obj_fid[SPIEL_UT_OBJ_SITE]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_rack_add(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_RACK],
			       &fake_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_rack_add(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_RACK],
			            &spiel_obj_fid[SPIEL_UT_OBJ_SITE]);
	M0_UT_ASSERT(rc == 0);

	/* Enclosure */
	rc = m0_spiel_enclosure_add(&tx,
				    &fake_fid,
				    &spiel_obj_fid[SPIEL_UT_OBJ_RACK]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_enclosure_add(&tx,
				    &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE],
				    &fake_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_enclosure_add(&tx,
				    &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE],
				    &spiel_obj_fid[SPIEL_UT_OBJ_RACK]);
	M0_UT_ASSERT(rc == 0);

	/* Node */
	rc = m0_spiel_node_add(&tx,
			       &fake_fid,
			       256, 2, 10, 0xff00ff00);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_node_add(&tx,
			       &spiel_obj_fid[SPIEL_UT_OBJ_NODE],
			       256, 2, 10, 0xff00ff00);
	M0_UT_ASSERT(rc == 0);

	/* Controller */
	rc = m0_spiel_controller_add(&tx,
				     &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER],
				     &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE],
				     &fake_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_controller_add(&tx,
				     &fake_fid,
				     &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE],
				     &spiel_obj_fid[SPIEL_UT_OBJ_NODE]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_controller_add(&tx,
				     &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER],
				     &fake_fid,
				     &spiel_obj_fid[SPIEL_UT_OBJ_NODE]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_controller_add(&tx,
				     &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER],
				     &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE],
				     &spiel_obj_fid[SPIEL_UT_OBJ_NODE]);
	M0_UT_ASSERT(rc == 0);

	/* drive */
	rc = m0_spiel_drive_add(&tx,
			       &fake_fid,
			       &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_drive_add(&tx,
			       &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE],
			       &fake_fid);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_drive_add(&tx,
			       &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE],
			       &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	M0_UT_ASSERT(rc == 0);

	/* Finish pool version */
	/* Pver done error */
	spiel_conf_create_pver_tree(&tx);
	m0_fi_enable_off_n_on_m("spiel_pver_add", "fail_allocation", 1, 1);
	rc = m0_spiel_pool_version_done(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_PVER]);
	m0_fi_disable("spiel_pver_add", "fail_allocation");
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_spiel_element_del(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_PVER_F]);

	spiel_conf_pver_delete_check(&tx);

	/* Pver done OK */
	spiel_conf_create_pver_tree(&tx);
	rc = m0_spiel_pool_version_done(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_PVER]);
	M0_UT_ASSERT(rc == 0);
	spiel_conf_pver_check(&tx);

	/* Process */
	rc = m0_spiel_process_add(&tx,
				  &fake_fid,
				  &spiel_obj_fid[SPIEL_UT_OBJ_NODE],
				  &bitmap, 4000, 1, 2, 3, ep[0]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_process_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &fake_fid,
				  &bitmap, 4000, 1, 2, 3, ep[0]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_process_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &spiel_obj_fid[SPIEL_UT_OBJ_NODE],
				  NULL, 4000, 1, 2, 3, ep[0]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_process_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &spiel_obj_fid[SPIEL_UT_OBJ_NODE],
				  &bitmap, 4000, 1, 2, 3, NULL);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_process_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &spiel_obj_fid[SPIEL_UT_OBJ_NODE],
				  &zero_bitmap, 4000, 1, 2, 3, ep[0]);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_process_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &spiel_obj_fid[SPIEL_UT_OBJ_NODE],
				  &bitmap, 4000, 1, 2, 3, ep[0]);
	M0_UT_ASSERT(rc == 0);

	/* Service */
	service_info.svi_endpoints = ep;
	service_info.svi_type = M0_CST_CONFD;
	rc = m0_spiel_service_add(&tx,
				  &fake_fid,
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_service_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				  &fake_fid,
				  &service_info);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_service_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  NULL);
	M0_UT_ASSERT(rc == -EINVAL);

	/* Check copy endpoints parameter */
	m0_fi_enable("m0_alloc", "fail_allocation");
	rc = m0_spiel_service_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == -ENOMEM);
	m0_fi_disable("m0_alloc", "fail_allocation");

	/* Check copy cs_u parameter & switch by type */
	service_info.svi_type = M0_CST_MDS;
	rc = m0_spiel_service_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_IOS;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_container += 0x0100;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_key += 0x0100;
	rc = m0_spiel_service_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_CONFD;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_container += 0x0100;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_key += 0x0100;
	rc = m0_spiel_service_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_RMS;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_container += 0x0100;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_key += 0x0100;
	rc = m0_spiel_service_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_SSS;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_container += 0x0100;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_key += 0x0100;
	rc = m0_spiel_service_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_HA;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_container += 0x0100;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_key += 0x0100;
	rc = m0_spiel_service_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	service_info.svi_type = M0_CST_NR;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_container += 0x0100;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_key += 0x0100;
	rc = m0_spiel_service_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == -EINVAL);

	/* Normal */
	service_info.svi_type = M0_CST_CONFD;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_container += 0x0100;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_key += 0x0100;
	rc = m0_spiel_service_add(&tx,
				  &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
				  &service_info);
	M0_UT_ASSERT(rc == 0);

	/* Device */
	rc = m0_spiel_device_add(&tx,
				 &fake_fid,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "fake_filename");
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_device_add(&tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV],
				 &fake_fid,
				 &fake_fid,
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "fake_filename");
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_device_add(&tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE],
				 1, M0_CFG_DEVICE_INTERFACE_NR,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "fake_filename");
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_device_add(&tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_NR,
				 1024, 512, 123, 0x55, "fake_filename");
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_device_add(&tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, NULL);
	M0_UT_ASSERT(rc == -EINVAL);

	rc = m0_spiel_device_add(&tx,
				 &spiel_obj_fid[SPIEL_UT_OBJ_SDEV],
				 &spiel_obj_fid[SPIEL_UT_OBJ_SERVICE],
				 &spiel_obj_fid[SPIEL_UT_OBJ_DRIVE],
				 1, M0_CFG_DEVICE_INTERFACE_SCSI,
				 M0_CFG_DEVICE_MEDIA_SSD,
				 1024, 512, 123, 0x55, "fake_filename");
	M0_UT_ASSERT(rc == 0);

	m0_spiel_tx_close(&tx);

	m0_bitmap_fini(&bitmap);
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_container -= 0x0700;
	spiel_obj_fid[SPIEL_UT_OBJ_SERVICE].f_key -= 0x0700;

	spiel_conf_ut_fini();
}

static void spiel_conf_delete(void)
{
	struct m0_spiel_tx        tx;
	int                       rc;
	struct m0_conf_obj       *obj;
	struct m0_conf_enclosure *enclosure;

	spiel_conf_ut_init();

	spiel_conf_create_configuration(&spiel, &tx);
	obj = m0_conf_cache_lookup(&tx.spt_cache,
				   &spiel_obj_fid[SPIEL_UT_OBJ_ENCLOSURE]);
	enclosure = M0_CONF_CAST(obj, m0_conf_enclosure);
	M0_UT_ASSERT(enclosure != NULL);

	rc = m0_spiel_element_del(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_CONTROLLER]);
	M0_UT_ASSERT(rc == 0);

	M0_UT_ASSERT(enclosure->ce_pvers[0] != NULL);
	rc = m0_spiel_element_del(&tx, &spiel_obj_fid[SPIEL_UT_OBJ_PVER]);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(enclosure->ce_pvers[0] == NULL);

	m0_spiel_tx_close(&tx);

	spiel_conf_ut_fini();
}

/**
 * spiel-conf-file create tree
 *
 * Create conf file equvalent to ut/conf.xc
 */
static void spiel_conf_file_create_tree(struct m0_spiel_tx *tx)
{
	int                           rc;
	const char                   *ep[] = { SERVER_ENDPOINT_ADDR,
					       NULL };
	const char                   *ep_service1[] = { "addr-0",
						        "addr-1",
						        "addr-2",
						        NULL };
	const char                   *ep_service2[] = { "addr-3", NULL };
	const char                   *ep_service3[] = { "addr-3", NULL };
	struct m0_pdclust_attr        pdclust_attr = { 0, 0, 0, 0 };
	const char                   *params[] = { "param-0",
						   "param-1",
						   "param-2",
						   NULL };
	struct m0_spiel_service_info  service_info1 = {
		.svi_endpoints = ep_service1 };
	struct m0_spiel_service_info  service_info2 = {
		.svi_endpoints = ep_service2 };
	struct m0_spiel_service_info  service_info3 = {
		.svi_endpoints = ep_service3 };
	/* @todo Add support for fdmi objects (filters group and filters) */
	struct m0_fid    fid_profile      = M0_FID_TINIT('p', 1,  0);
	struct m0_fid    fid_node         = M0_FID_TINIT('n', 1,  2);
	struct m0_fid    fid_site         = M0_FID_TINIT('S', 1,  3);
	struct m0_fid    fid_rack         = M0_FID_TINIT('a', 1,  3);
	struct m0_fid    fid_pool         = M0_FID_TINIT('o', 1,  4);
	struct m0_fid    fid_process1     = M0_FID_TINIT('r', 1,  5);
	struct m0_fid    fid_process2     = M0_FID_TINIT('r', 1,  6);
	struct m0_fid    fid_process3     = M0_FID_TINIT('r', 1,  7);
	struct m0_fid    fid_enclosure    = M0_FID_TINIT('e', 1,  8);
	struct m0_fid    fid_pver         = M0_FID_TINIT('v', 1,  9);
	struct m0_fid    fid_service1     = M0_FID_TINIT('s', 1, 10);
	struct m0_fid    fid_service2     = M0_FID_TINIT('s', 1, 11);
	struct m0_fid    fid_service3     = M0_FID_TINIT('s', 1, 12);
	struct m0_fid    fid_controller   = M0_FID_TINIT('c', 1, 13);
	struct m0_fid    fid_drive1       = M0_FID_TINIT('k', 1, 14);
	struct m0_fid    fid_drive2       = M0_FID_TINIT('k', 1, 15);
	struct m0_fid    fid_drive3       = M0_FID_TINIT('k', 1, 16);
	struct m0_fid    fid_site_v       = M0_FID_TINIT('j', 2, 17);
	struct m0_fid    fid_rack_v       = M0_FID_TINIT('j', 1, 17);
	struct m0_fid    fid_sdev1        = M0_FID_TINIT('d', 1, 18);
	struct m0_fid    fid_sdev2        = M0_FID_TINIT('d', 1, 19);
	struct m0_fid    fid_sdev3        = M0_FID_TINIT('d', 1, 20);
	struct m0_fid    fid_enclosure_v  = M0_FID_TINIT('j', 1, 21);
	struct m0_fid    fid_controller_v = M0_FID_TINIT('j', 1, 22);
	struct m0_fid    fid_drive_v1     = M0_FID_TINIT('j', 1, 23);
	struct m0_fid    fid_drive_v2     = M0_FID_TINIT('j', 1, 24);
	struct m0_fid    fid_drive_v3     = M0_FID_TINIT('j', 1, 25);
	uint32_t         tolerance[]      = {0, 0, 0, 0, 1};
	struct m0_bitmap bitmap;

	m0_bitmap_init(&bitmap, 32);
	m0_bitmap_set(&bitmap, 0, true);
	m0_bitmap_set(&bitmap, 1, true);

	rc = m0_spiel_root_add(tx, &fid_profile, &fid_pool,
			       &fid_pver, 41212, params);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_profile_add(tx, &fid_profile);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_pool_add(tx, &fid_pool, 0);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_site_add(tx, &fid_site);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_rack_add(tx, &fid_rack, &fid_site);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_enclosure_add(tx, &fid_enclosure, &fid_rack);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_node_add(tx, &fid_node, 16000, 2, 3, 2);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_controller_add(tx, &fid_controller, &fid_enclosure,
				     &fid_node);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx, &fid_drive1, &fid_controller);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx, &fid_drive2, &fid_controller);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_add(tx, &fid_drive3, &fid_controller);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_pver_actual_add(tx, &fid_pver, &fid_pool, &pdclust_attr,
				      tolerance, ARRAY_SIZE(tolerance));
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_site_v_add(tx, &fid_site_v, &fid_pver, &fid_site);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_rack_v_add(tx, &fid_rack_v, &fid_site_v, &fid_rack);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_enclosure_v_add(tx, &fid_enclosure_v, &fid_rack_v,
				      &fid_enclosure);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_controller_v_add(tx, &fid_controller_v, &fid_enclosure_v,
				       &fid_controller);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_v_add(tx, &fid_drive_v1, &fid_controller_v,
				  &fid_drive1);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_v_add(tx, &fid_drive_v2, &fid_controller_v,
				  &fid_drive2);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_drive_v_add(tx, &fid_drive_v3, &fid_controller_v,
				  &fid_drive3);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_pool_version_done(tx, &fid_pver);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_process_add(tx, &fid_process1, &fid_node,
				  &bitmap, 4000, 1, 2, 3, ep[0]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_process_add(tx, &fid_process2, &fid_node,
				  &bitmap, 4000, 1, 2, 3, ep[0]);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_process_add(tx, &fid_process3, &fid_node,
				  &bitmap, 4000, 1, 2, 3, ep[0]);
	M0_UT_ASSERT(rc == 0);

	service_info1.svi_type = M0_CST_CONFD;
	rc = m0_spiel_service_add(tx, &fid_service1, &fid_process1,
				  &service_info1);
	M0_UT_ASSERT(rc == 0);

	service_info2.svi_type = M0_CST_MDS;
	rc = m0_spiel_service_add(tx, &fid_service2, &fid_process2,
				  &service_info2);
	M0_UT_ASSERT(rc == 0);

	service_info3.svi_type = M0_CST_FDMI;
	rc = m0_spiel_service_add(tx, &fid_service3, &fid_process3,
				  &service_info3);
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_device_add(tx, &fid_sdev1, &fid_service1, &fid_drive1, 1,
				 4, 1, 4096, 596000000000, 3, 4, "/dev/sdev0");
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_device_add(tx, &fid_sdev2, &fid_service1, &fid_drive2, 2,
				 4, 1, 4096, 596000000000, 3, 4, "/dev/sdev1");
	M0_UT_ASSERT(rc == 0);

	rc = m0_spiel_device_add(tx, &fid_sdev3, &fid_service2, &fid_drive3, 3,
				 7, 2, 8192, 320000000000, 2, 4, "/dev/sdev2");
	M0_UT_ASSERT(rc == 0);

	m0_bitmap_fini(&bitmap);
}

static void spiel_conf_file(void)
{
	int                   rc;
	struct m0_spiel_tx    tx;
	char                 *confstr = NULL;
	const char            filename[] = "spiel-conf.xc";
	const int             ver_forced = 10;
	struct m0_conf_cache  cache;
	struct m0_mutex       lock;

	spiel_conf_ut_init();

	m0_spiel_tx_open(&spiel, &tx);
	spiel_conf_file_create_tree(&tx);

	/* Convert to file */
	rc = m0_spiel_tx_dump(&tx, ver_forced, filename);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_conf_version(&tx.spt_cache) == ver_forced);
	m0_spiel_tx_close(&tx);

	/* Load file */
	rc = m0_file_read(filename, &confstr);
	M0_UT_ASSERT(rc == 0);
	rc = unlink(filename);
	M0_UT_ASSERT(rc == 0);

	m0_mutex_init(&lock);
	m0_conf_cache_init(&cache, &lock);

	m0_mutex_lock(&lock);

	rc = m0_conf_cache_from_string(&cache, confstr);
	m0_free(confstr);
	M0_UT_ASSERT(rc == 0);
	m0_mutex_unlock(&lock);

	m0_conf_cache_fini(&cache);
	m0_mutex_fini(&lock);
	spiel_conf_ut_fini();
}

static void spiel_conf_cancel(void)
{
	struct m0_spiel_tx  tx;

	spiel_conf_ut_init();
	spiel_conf_create_configuration(&spiel, &tx);
	m0_spiel_tx_close(&tx);
	spiel_conf_ut_fini();
}

/**
 * send Load command to confd
 */
static void spiel_conf_load_send(void)
{
	struct m0_spiel_tx tx;
	int                i;
	int                rc;

	spiel_conf_ut_init();
	/*
	 * Check that the second commit is possible and there is no panic.
	 * See MERO-2311.
	 */
	m0_fi_enable("storage_devs_conf_expired_cb", "skip_storage_devs_expired_cb");
	m0_fi_enable("storage_devs_conf_ready_async_cb",
		     "skip_storage_devs_ready_cb");
	for (i = 0; i < 2; i++) {
		spiel_conf_create_configuration(&spiel, &tx);
		rc = m0_spiel_tx_commit(&tx);
		M0_UT_ASSERT(rc == 0);
		m0_spiel_tx_close(&tx);
	}
	m0_fi_disable("storage_devs_conf_expired_cb", "skip_storage_devs_expired_cb");
	m0_fi_disable("storage_devs_conf_ready_async_cb",
		      "skip_storage_devs_ready_cb");
	spiel_conf_ut_fini();
}

/**
 * send Load command to confd having one service dropped
 */
static void spiel_conf_drop_svc(void)
{
	struct m0_spiel_tx tx;
	int                rc;

	spiel_conf_ut_init();
	spiel_conf_create_conf_with_opt(&spiel, &tx, SCO_DROP_SERVICE);
	rc = m0_spiel_tx_commit(&tx);
	M0_UT_ASSERT(rc == 0);
	m0_spiel_tx_close(&tx);
	spiel_conf_ut_fini();
}

/**
 * send Load command to confd having one service added
 */
static void spiel_conf_add_svc(void)
{
	struct m0_spiel_tx tx;
	int                rc;

	spiel_conf_ut_init();
	spiel_conf_create_conf_with_opt(&spiel, &tx, SCO_ADD_SERVICE);
	rc = m0_spiel_tx_commit(&tx);
	M0_UT_ASSERT(rc == 0);
	m0_spiel_tx_close(&tx);
	spiel_conf_ut_fini();
}

/**
 * send Load command using rpc bulk mechanism
 */
static void spiel_conf_big_db(void)
{
#define SVC_EP "192.168.252.132@tcp:12345:41:201"
	struct m0_spiel_tx tx;
	int                rc;
	int                i;
	int                svc_nr;
	struct m0_fid      fid = spiel_obj_fid[SPIEL_UT_OBJ_SERVICE11];
	const char        *svc_ep[] = { SVC_EP, NULL };
	struct m0_spiel_service_info svc_info = {
		.svi_type = M0_CST_DS1,
		.svi_endpoints = svc_ep
	};
	m0_bcount_t        seg_size;
	char              *cache_str;
	uint32_t           svc_str_size =
		sizeof("{0x73|((^s|1:0), @M0_CST_MDS, [1: "SVC_EP"], [0],"
		       " [0])},") - 1;

	spiel_conf_ut_init();
	spiel_conf_create_configuration(&spiel, &tx);
	/**
	 * Add sufficient number of services, so resulting conf db string
	 * after encoding doesn't fit into one RPC bulk segment.
	 */
	m0_fi_enable("m0_conf_segment_size", "const_size");
	seg_size = m0_conf_segment_size(NULL);
	svc_nr = seg_size/svc_str_size + 1;
	for (i = 0; i < svc_nr; i++) {
		fid.f_key++;
		rc = m0_spiel_service_add(&tx, &fid,
					  &spiel_obj_fid[SPIEL_UT_OBJ_PROCESS],
					  &svc_info);
		M0_UT_ASSERT(rc == 0);
	}
	rc = m0_conf_cache_to_string(&tx.spt_cache, &cache_str, false);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(strlen(cache_str) > seg_size);
	rc = m0_spiel_tx_commit(&tx);
	M0_UT_ASSERT(rc == 0);
	m0_fi_disable("m0_conf_segment_size", "const_size");
	m0_confx_string_free(cache_str);
	m0_spiel_tx_close(&tx);
	spiel_conf_ut_fini();
}

static void spiel_conf_flip_fail(void)
{
	struct m0_spiel_tx  tx;
	int                 rc;

	spiel_conf_ut_init();
	spiel_conf_create_configuration(&spiel, &tx);
	m0_fi_enable_once("conf_flip_confd_config_save", "fcreate_failed");
	rc = m0_spiel_tx_commit(&tx);
	M0_UT_ASSERT(rc == -ENOENT);
	m0_spiel_tx_close(&tx);
	spiel_conf_ut_fini();
}

static void spiel_conf_check_fail(void)
{
	struct m0_spiel_tx    tx;
	int                   rc;
	struct m0_conf_obj   *obj = NULL;
	struct m0_conf_obj   *obj_parent;
	struct m0_conf_cache *cache = &tx.spt_cache;
	struct m0_fid         fake_fid = M0_FID_TINIT('n', 6, 600 );

	spiel_conf_ut_init();
	spiel_conf_create_configuration(&spiel, &tx);

	m0_tl_for(m0_conf_cache, &cache->ca_registry, obj) {
		if (m0_conf_obj_type(obj) == &M0_CONF_PROCESS_TYPE)
			break;
	} m0_tl_endfor;

	/* spiel_check_cache(tx) test */
	obj->co_status = M0_CS_MISSING;
	rc = m0_spiel_tx_commit(&tx);
	obj->co_status = M0_CS_READY;
	M0_UT_ASSERT(rc == -EBUSY);

	obj_parent = obj->co_parent;

	obj->co_parent = NULL;
	rc = m0_spiel_tx_commit(&tx);
	M0_UT_ASSERT(rc == -ENOENT);

	obj->co_parent = m0_conf_obj_create(&fake_fid, cache);
	rc = m0_spiel_tx_commit(&tx);
	M0_UT_ASSERT(rc == -ENOENT);

	obj->co_parent->co_ops->coo_delete(obj->co_parent);
	obj->co_parent = obj_parent;

	/* conf_cache_encode test */
	m0_fi_enable_once("m0_spiel_tx_commit_forced", "encode_fail");
	rc = m0_spiel_tx_commit(&tx);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_spiel_tx_close(&tx);
	spiel_conf_ut_fini();
}

static void spiel_conf_load_fail(void)
{
	struct m0_spiel_tx  tx;
	int                 rc;

	spiel_conf_ut_init();
	spiel_conf_create_configuration(&spiel, &tx);
	m0_fi_enable_once("m0_spiel_tx_commit_forced", "cmd_alloc_fail");
	rc = m0_spiel_tx_commit(&tx);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_spiel_tx_close(&tx);
	spiel_conf_ut_fini();
}

static void spiel_conf_dump(void)
{
	struct m0_spiel_tx  tx;
	struct m0_spiel_tx  tx_bad;
	const char         *filename = "config.xc";
	const char         *filename_bad = "config_b.xc";
	const int           ver_forced = 10;
	int                 rc;

	spiel_conf_ut_init();

	spiel_conf_create_configuration(&spiel, &tx);
	rc = m0_spiel_tx_validate(&tx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_tx_dump(&tx, ver_forced, filename);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_conf_version(&tx.spt_cache) == ver_forced);

	spiel_conf_create_invalid_configuration(&spiel, &tx_bad);
	rc = m0_spiel_tx_validate(&tx_bad);
	M0_UT_ASSERT(rc != 0);
	rc = m0_spiel_tx_dump_debug(&tx_bad, 2, filename_bad);
	M0_UT_ASSERT(rc == 0);

	spiel_conf_ut_fini();
}

static void spiel_conf_tx_invalid(void)
{
	struct m0_spiel_tx tx;
	int                rc;

	spiel_conf_ut_init();
	spiel_conf_create_invalid_configuration(&spiel, &tx);
	rc = m0_spiel_tx_validate(&tx);
	M0_UT_ASSERT(rc != 0);
	spiel_conf_ut_fini();
}

static void spiel_conf_tx_no_spiel(void)
{
	struct m0_spiel_tx  tx;
	const int           ver_forced = 10;
	char               *local_conf;
	int                 rc;

	spiel_conf_create_configuration(NULL, &tx);
	rc = m0_spiel_tx_validate(&tx);
	M0_UT_ASSERT(rc == 0);
	rc = m0_spiel_tx_to_str(&tx, ver_forced, &local_conf);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_conf_version(&tx.spt_cache) == ver_forced);
	m0_spiel_tx_str_free(local_conf);
	m0_spiel_tx_close(&tx);
}

static void spiel_conf_expired(void)
{
	struct m0_confc         *confc = m0_mero2confc(
					   &ut_reqh.sur_confd_srv.rsx_mero_ctx);
	struct m0_spiel_tx       tx;
	struct m0_rm_ha_tracker  tracker;
	char                    *rm_ep = "0@lo:12345:34:1";
	int                      rc;

	m0_fi_enable("rm_ha_sbscr_diter_next", "subscribe");
	spiel_conf_ut_init();
	m0_rm_ha_tracker_init(&tracker, NULL);
	m0_rm_ha_subscribe_sync(confc, rm_ep, &tracker);
	spiel_conf_create_configuration(&spiel, &tx);
	rc = m0_spiel_tx_commit(&tx);
	M0_UT_ASSERT(rc == 0);
	m0_spiel_tx_close(&tx);
	m0_rm_ha_tracker_fini(&tracker);
	spiel_conf_ut_fini();
	m0_fi_disable("rm_ha_sbscr_diter_next", "subscribe");
}

static void spiel_conf_wlock_get_fail(void)
{
	struct m0_spiel_tx tx;
	int                rc;

	spiel_conf_ut_init();
	spiel_conf_create_configuration(&spiel, &tx);
	m0_fi_enable_once("spiel_tx_write_lock_get", "borrow-request-failure");
	rc = m0_spiel_tx_commit(&tx);
	M0_UT_ASSERT(rc == -EIO);
	m0_spiel_tx_close(&tx);
	spiel_conf_ut_fini();
}

struct m0_ut_suite spiel_conf_ut = {
	.ts_name = "spiel-conf-ut",
	.ts_tests = {
		{ "create-ok",      spiel_conf_create_ok      },
		{ "create-fail",    spiel_conf_create_fail    },
		{ "delete",         spiel_conf_delete         },
		{ "file",           spiel_conf_file           },
		{ "cancel",         spiel_conf_cancel         },
		{ "load-send",      spiel_conf_load_send      },
		{ "big-db",         spiel_conf_big_db         },
		{ "flip-fail",      spiel_conf_flip_fail      },
		{ "check-fail",     spiel_conf_check_fail     },
		{ "load-fail",      spiel_conf_load_fail      },
		{ "dump",           spiel_conf_dump           },
		{ "tx-invalid",     spiel_conf_tx_invalid     },
		{ "tx-no-spiel",    spiel_conf_tx_no_spiel    },
		{ "drop-svc",       spiel_conf_drop_svc       },
		{ "add-svc",        spiel_conf_add_svc        },
		{ "conf-expired",   spiel_conf_expired        },
		{ "wlock-get-fail", spiel_conf_wlock_get_fail },
		{ NULL, NULL },
	},
};
M0_EXPORTED(spiel_conf_ut);

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
