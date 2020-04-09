/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 08/07/2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_IOSERVICE
#include "lib/trace.h"

#include <unistd.h> /* get_current_dir_name */

#include "ioservice/storage_dev.h" /* m0_storage_devs */
#include "balloc/balloc.h"         /* BALLOC_DEF_BLOCK_SHIFT */
#include "rpc/rpclib.h"            /* m0_rpc_server_ctx */
#include "lib/finject.h"
#include "stob/stob.h"
#include "ut/misc.h"               /* M0_UT_PATH */
#include "ut/ut.h"

#define SERVER_ENDPOINT_ADDR "0@lo:12345:34:1"

enum {
	CID1 = 10,
	CID2 = 12,
	CID3 = 13,
};

static int rpc_start(struct m0_rpc_server_ctx *rpc_srv)
{
	enum {
		LOG_NAME_MAX_LEN     = 128,
		EP_MAX_LEN           = 24,
		RPC_SIZE_MAX_LEN     = 32,
	};
	 const char               *confd_ep = SERVER_ENDPOINT_ADDR;

	char                log_name[LOG_NAME_MAX_LEN];
	char                full_ep[EP_MAX_LEN];
	char                max_rpc_size[RPC_SIZE_MAX_LEN];
	struct m0_net_xprt *xprt = &m0_net_lnet_xprt;

	snprintf(full_ep, EP_MAX_LEN, "lnet:%s", confd_ep);
	snprintf(max_rpc_size, RPC_SIZE_MAX_LEN,
		 "%d", M0_RPC_DEF_MAX_RPC_MSG_SIZE);

#define NAME(ext) "io_sdev" ext
	char                    *argv[] = {
		NAME(""), "-T", "AD", "-D", NAME(".db"),
		"-S", NAME(".stob"), "-A", "linuxstob:"NAME(""),
		"-w", "10", "-e", full_ep, "-H", SERVER_ENDPOINT_ADDR,
		"-f", M0_UT_CONF_PROCESS,
		"-m", max_rpc_size,
		"-c", M0_UT_PATH("conf.xc")
	};
#undef NAME

	M0_SET0(rpc_srv);

	rpc_srv->rsx_xprts         = &xprt;
	rpc_srv->rsx_xprts_nr      = 1;
	rpc_srv->rsx_argv          = argv;
	rpc_srv->rsx_argc          = ARRAY_SIZE(argv);
	snprintf(log_name, LOG_NAME_MAX_LEN, "confd_%s.log", confd_ep);
	rpc_srv->rsx_log_file_name = log_name;

	return m0_rpc_server_start(rpc_srv);
}

static int create_test_file(const char *filename)
{
	int   rc;
	FILE *file;
	char str[0x1000]="rfb";

	file = fopen(filename, "w+");
	if (file == NULL)
		return errno;
	rc = fwrite(str, 0x1000, 1, file) == 1 ? 0 : -EINVAL;
	fclose(file);
	return rc;
}

static void storage_dev_test(void)
{
	int                      rc;
	struct m0_storage_devs   devs;
	const char              *location = "linuxstob:io_sdev";
	struct m0_stob_domain   *domain;
	struct m0_storage_dev   *dev1, *dev2, *dev3;
	struct m0_storage_space  space;
	struct m0_rpc_server_ctx rpc_srv;
	int                      total_size;
	int                      grp_size;
	char                    *cwd;
	char                    *fname1, *fname2;
	int                      block_size = 1 << BALLOC_DEF_BLOCK_SHIFT;
	struct m0_conf_sdev      sdev = {
		.sd_obj = { .co_id = M0_FID_TINIT(
				M0_CONF_SDEV_TYPE.cot_ftype.ft_id, 0, 12) },
		.sd_bsize = block_size };


	/* pre-init */
	rc = rpc_start(&rpc_srv);
	M0_UT_ASSERT(rc == 0);

	cwd = get_current_dir_name();
	M0_UT_ASSERT(cwd != NULL);
	rc = asprintf(&fname1, "%s/test1", cwd);
	M0_UT_ASSERT(rc > 0);
	rc = asprintf(&fname2, "%s/test2", cwd);
	M0_UT_ASSERT(rc > 0);
	rc = create_test_file(fname1);
	M0_UT_ASSERT(rc == 0);
	rc = create_test_file(fname2);
	M0_UT_ASSERT(rc == 0);

	/* init */
	domain = m0_stob_domain_find_by_location(location);
	rc = m0_storage_devs_init(&devs,
				  M0_STORAGE_DEV_TYPE_AD,
				  rpc_srv.rsx_mero_ctx.cc_reqh_ctx.rc_beseg,
				  domain,
				  &rpc_srv.rsx_mero_ctx.cc_reqh_ctx.rc_reqh);
	M0_UT_ASSERT(rc == 0);
	m0_storage_devs_lock(&devs);

	/* attach */
	/*
	 * Total size accounts space for reserved groups and one non-reserved
	 * group.
	 */
	grp_size = BALLOC_DEF_BLOCKS_PER_GROUP * block_size;
	total_size = grp_size;
	rc = m0_storage_dev_new(&devs, 10, fname2, total_size,
				NULL, false, &dev1);
	M0_UT_ASSERT(rc == 0);
	m0_storage_dev_attach(dev1, &devs);

	sdev.sd_size = total_size;
	sdev.sd_filename = fname1;
	sdev.sd_dev_idx = 12;
	m0_fi_enable("m0_storage_dev_new_by_conf", "no-conf-dev");
	rc = m0_storage_dev_new_by_conf(&devs, &sdev, false, &dev2);
	M0_UT_ASSERT(rc == 0);
	m0_storage_dev_attach(dev2, &devs);
	m0_fi_disable("m0_storage_dev_new_by_conf", "no-conf-dev");

	m0_fi_enable_once("m0_alloc", "fail_allocation");
	rc = m0_storage_dev_new(&devs, 13, "../../some-file", total_size,
				NULL, false, &dev3);
	M0_UT_ASSERT(rc == -ENOMEM);

	m0_fi_enable_off_n_on_m("m0_alloc", "fail_allocation", 1, 1);
	rc = m0_storage_dev_new(&devs, 13, "../../some-file", total_size,
				NULL, false, &dev3);
	m0_fi_disable("m0_alloc", "fail_allocation");
	M0_UT_ASSERT(rc == -ENOMEM);

	/* find*/
	dev1 = m0_storage_devs_find_by_cid(&devs, 10);
	M0_UT_ASSERT(dev1 != NULL);

	dev2 = m0_storage_devs_find_by_cid(&devs, 12);
	M0_UT_ASSERT(dev2 != NULL);

	dev3 = m0_storage_devs_find_by_cid(&devs, 13);
	M0_UT_ASSERT(dev3 == NULL);

	/* space */
	m0_storage_dev_space(dev1, &space);
	M0_UT_ASSERT(space.sds_block_size  == block_size);
	/*
	 * Free blocks don't include blocks for reserved groups and we have
	 * only one non-reserved group
	 */
	M0_UT_ASSERT(space.sds_free_blocks == BALLOC_DEF_BLOCKS_PER_GROUP);
	M0_UT_ASSERT(space.sds_total_size  == total_size);

	/* detach */
	m0_storage_dev_detach(dev1);
	m0_storage_dev_detach(dev2);

	/* fini */
	m0_storage_devs_unlock(&devs);
	m0_storage_devs_fini(&devs);

	m0_rpc_server_stop(&rpc_srv);
	rc = unlink(fname1);
	M0_UT_ASSERT(rc == 0);
	rc = unlink(fname2);
	M0_UT_ASSERT(rc == 0);
	free(fname1);
	free(fname2);
	free(cwd);
}

static void storage_dev_linux(void)
{
	struct m0_rpc_server_ctx *rpc_srv;
	struct m0_storage_devs   *devs;
	struct m0_storage_dev    *dev1;
	struct m0_storage_dev    *dev2;
	struct m0_stob_id         stob_id;
	struct m0_stob           *stob;
	char                     *cwd;
	char                     *path1;
	char                     *path2;
	char                     *location1;
	char                     *location2;
	int                       rc;

	cwd = get_current_dir_name();
	M0_UT_ASSERT(cwd != NULL);
	rc = asprintf(&path1, "%s/test1", cwd);
	M0_UT_ASSERT(rc > 0);
	rc = asprintf(&path2, "%s/test2", cwd);
	M0_UT_ASSERT(rc > 0);

	M0_ALLOC_PTR(devs);
	M0_ALLOC_PTR(rpc_srv);
	rc = rpc_start(rpc_srv);
	M0_UT_ASSERT(rc == 0);
	rc = m0_storage_devs_init(devs, M0_STORAGE_DEV_TYPE_LINUX, NULL, NULL,
				  &rpc_srv->rsx_mero_ctx.cc_reqh_ctx.rc_reqh);
	M0_UT_ASSERT(rc == 0);

	rc = m0_storage_dev_new(devs, CID1, path1, 0, NULL, false, &dev1);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(dev1 != NULL);
	rc = m0_storage_dev_new(devs, CID2, path2, 0, NULL, false, &dev2);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(dev2 != NULL);
	m0_storage_devs_lock(devs);
	m0_storage_dev_attach(dev1, devs);
	m0_storage_dev_attach(dev2, devs);
	M0_UT_ASSERT(m0_storage_devs_find_by_cid(devs, CID1) != NULL);
	M0_UT_ASSERT(m0_storage_devs_find_by_cid(devs, CID2) != NULL);
	m0_storage_devs_unlock(devs);

	location1 = m0_strdup(m0_stob_domain_location_get(dev1->isd_domain));
	location2 = m0_strdup(m0_stob_domain_location_get(dev2->isd_domain));

	m0_stob_id_make(0, 1, &dev1->isd_domain->sd_id, &stob_id);
	rc = m0_storage_dev_stob_create(devs, &stob_id, NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_storage_dev_stob_find(devs, &stob_id, &stob);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(dev1->isd_domain == m0_stob_dom_get(stob));
	M0_UT_ASSERT(m0_stob_state_get(stob) == CSS_EXISTS);
	m0_storage_dev_stob_put(devs, stob);

	m0_storage_devs_lock(devs);
	m0_storage_dev_detach(dev1);
	M0_UT_ASSERT(m0_storage_devs_find_by_cid(devs, CID1) == NULL);
	m0_storage_devs_unlock(devs);
	rc = m0_storage_dev_stob_find(devs, &stob_id, &stob);
	M0_UT_ASSERT(rc != 0);

	rc = m0_storage_dev_new(devs, CID1, path1, 0, NULL, false, &dev1);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(dev1 != NULL);
	m0_storage_devs_lock(devs);
	m0_storage_dev_attach(dev1, devs);
	M0_UT_ASSERT(m0_storage_devs_find_by_cid(devs, CID1) != NULL);
	m0_storage_devs_unlock(devs);
	rc = m0_storage_dev_stob_find(devs, &stob_id, &stob);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(dev1->isd_domain == m0_stob_dom_get(stob));
	/* Mark the stob state as CSS_DELETE */
	m0_stob_delete_mark(stob);
	rc = m0_storage_dev_stob_destroy(devs, stob, NULL);
	M0_UT_ASSERT(rc == 0);

	/* force option */
	rc = m0_storage_dev_stob_create(devs, &stob_id, NULL);
	M0_UT_ASSERT(rc == 0);
	rc = m0_storage_dev_stob_find(devs, &stob_id, &stob);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_stob_state_get(stob) == CSS_EXISTS);
	m0_storage_dev_stob_put(devs, stob);
	m0_storage_devs_lock(devs);
	m0_storage_dev_detach(dev1);
	m0_storage_devs_unlock(devs);
	rc = m0_storage_dev_new(devs, CID1, path1, 0, NULL, true, &dev1);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(dev1 != NULL);
	m0_storage_devs_lock(devs);
	m0_storage_dev_attach(dev1, devs);
	m0_storage_devs_unlock(devs);
	rc = m0_storage_dev_stob_find(devs, &stob_id, &stob);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(m0_stob_state_get(stob) == CSS_NOENT);
	m0_storage_dev_stob_put(devs, stob);

	m0_storage_devs_lock(devs);
	m0_storage_devs_detach_all(devs);
	m0_storage_devs_unlock(devs);
	rc = m0_stob_domain_destroy_location(location1);
	M0_UT_ASSERT(rc == 0);
	rc = m0_stob_domain_destroy_location(location2);
	M0_UT_ASSERT(rc == 0);
	m0_free(location1);
	m0_free(location2);
	m0_storage_devs_fini(devs);
	m0_rpc_server_stop(rpc_srv);
	m0_free(rpc_srv);
	m0_free(devs);

	free(path1);
	free(path2);
	free(cwd);
}

struct m0_ut_suite storage_dev_ut = {
	.ts_name = "storage-dev-ut",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "storage-dev-test",  storage_dev_test  },
		{ "storage-dev-linux", storage_dev_linux },
		{ NULL, NULL },
	},
};
M0_EXPORTED(storage_dev_ut);

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
