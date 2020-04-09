/* -*- C -*- */
/*
 * COPYRIGHT 2017 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Authors: Sining Wu       <sining.wu@seagate.com>
 *	    Pratik Shinde   <pratik.shinde@seagate.com>
 *	    Vishwas Bhat    <vishwas.bhat@seagate.com>
 * Original creation date: 27-June-2017
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <assert.h>

#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_idx.h"
#include "clovis/clovis_layout.h"


/* Clovis parameters */
static char *clovis_local_addr;
static char *clovis_ha_addr;
static char *clovis_prof;
static char *clovis_proc_fid;

static struct m0_clovis          *clovis_instance = NULL;
static struct m0_clovis_container clovis_container;
static struct m0_clovis_realm     clovis_uber_realm;
static struct m0_clovis_config    clovis_conf;
static struct m0_idx_dix_config   dix_conf;

extern struct m0_addb_ctx m0_clovis_addb_ctx;

static int init_clovis(void)
{
	int rc;

	clovis_conf.cc_is_oostore            = true;
	clovis_conf.cc_is_read_verify        = false;
	clovis_conf.cc_local_addr            = clovis_local_addr;
	clovis_conf.cc_ha_addr               = clovis_ha_addr;
	clovis_conf.cc_profile               = clovis_prof;
	clovis_conf.cc_process_fid           = clovis_proc_fid;
	clovis_conf.cc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	clovis_conf.cc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	clovis_conf.cc_layout_id	     = 0;

	/* Use dix index services. */
	clovis_conf.cc_idx_service_id        = M0_CLOVIS_IDX_DIX;
	dix_conf.kc_create_meta              = false;
	clovis_conf.cc_idx_service_conf      = &dix_conf;

	/* Clovis instance */
	rc = m0_clovis_init(&clovis_instance, &clovis_conf, true);
	if (rc != 0) {
		fprintf(stderr, "Failed to initialise Clovis\n");
		goto err_exit;
	}

	/* And finally, clovis root realm */
	m0_clovis_container_init(&clovis_container,
				 NULL, &M0_CLOVIS_UBER_REALM,
				 clovis_instance);
	rc = clovis_container.co_realm.re_entity.en_sm.sm_rc;
	if (rc != 0) {
		fprintf(stderr, "Failed to open uber realm\n");
		goto err_exit;
	}
	clovis_uber_realm = clovis_container.co_realm;
	return 0;

err_exit:
	return rc;
}

static void fini_clovis(void)
{
	m0_clovis_fini(clovis_instance, true);
}

static int create_index(struct m0_fid fid)
{
	int                     rc;
	struct m0_clovis_op    *ops[1] = {NULL};
	struct m0_clovis_idx    idx;

	memset(&idx, 0, sizeof idx);
	ops[0] = NULL;

	/* Set an index creation operation. */
	m0_clovis_idx_init(&idx,
		&clovis_container.co_realm, (struct m0_uint128 *)&fid);
	m0_clovis_entity_create(NULL, &idx.in_entity, &ops[0]);

	/* Launch and wait for op to complete */
	m0_clovis_op_launch(ops, 1);
	rc = m0_clovis_op_wait(ops[0],
		    M0_BITS(M0_CLOVIS_OS_FAILED,
			    M0_CLOVIS_OS_STABLE),
		    M0_TIME_NEVER);
	if (rc < 0) return rc;

	rc = ops[0]->op_sm.sm_rc;
	if (rc < 0) return rc;

	/* fini and release */
	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
	m0_clovis_entity_fini(&idx.in_entity);

	return rc;
}

static int delete_index(struct m0_fid fid)
{
	int                     rc;
	struct m0_clovis_op    *ops[1] = {NULL};
	struct m0_clovis_idx    idx;

	memset(&idx, 0, sizeof idx);
	ops[0] = NULL;

	/* Set an index creation operation. */
	m0_clovis_idx_init(&idx,
		&clovis_container.co_realm, (struct m0_uint128 *)&fid);
	m0_clovis_entity_delete(&idx.in_entity, &ops[0]);

	/* Launch and wait for op to complete */
	m0_clovis_op_launch(ops, 1);
	rc = m0_clovis_op_wait(ops[0],
		    M0_BITS(M0_CLOVIS_OS_FAILED,
			    M0_CLOVIS_OS_STABLE),
		    M0_TIME_NEVER);
	rc = (rc != 0)?rc:ops[0]->op_sm.sm_rc;

	/* fini and release */
	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
	m0_clovis_entity_fini(&idx.in_entity);

	return rc;
}

int main(int argc, char **argv)
{
	int rc;

	/* Get input parameters */
	if (argc < 5) {
		fprintf(stderr,
			"Usage: c0composite laddr ha_addr prof_opt proc_fid\n");
		return -1;
	}
	clovis_local_addr = argv[1];
	clovis_ha_addr = argv[2];
	clovis_prof = argv[3];
	clovis_proc_fid = argv[4];

	/* Initialise mero and Clovis */
	rc = init_clovis();
	if (rc < 0) {
		fprintf(stderr, "clovis_init failed!\n");
		return rc;
	}

	/* Create global extent indices for composite layouts. */
	rc = create_index(composite_extent_rd_idx_fid);
	if (rc != 0) {
		fprintf(stderr,
			"Can't create composite RD extent index, rc=%d!\n", rc);
		return rc;
	}
	rc = create_index(composite_extent_wr_idx_fid);
	if (rc != 0) {
		fprintf(stderr, "Can't create composite RD extent index!\n");
		delete_index(composite_extent_rd_idx_fid);
		return rc;
	}

	/* Clean-up */
	fini_clovis();

	return 0;
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
