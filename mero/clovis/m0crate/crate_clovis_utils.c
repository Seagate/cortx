/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author:  Pratik Shinde <pratik.shinde@seagate.com>
 * Original creation date: 20-Jun-2016
 */

/**
 * @addtogroup crate_clovis_utils
 *
 * @{
 */

#include <errno.h>
#include "lib/trace.h"
#include "clovis/m0crate/logger.h"
#include "clovis/clovis_internal.h" /* clovis_instance */
#include "clovis/m0crate/crate_clovis_utils.h"

#define LOG_PREFIX "clovis_utils:"

struct crate_clovis_conf	*conf = NULL;
struct m0_clovis_config		clovis_conf = {};
struct m0_idx_cass_config	cass_conf = {};
static int			num_clovis_workloads = 0;
struct m0_clovis		*clovis_instance = NULL;
struct m0_clovis_container	clovis_container = {};
static struct m0_clovis_realm	clovis_uber_realm = {};
static struct m0_idx_dix_config dix_conf = {};

struct m0_clovis_realm *crate_clovis_uber_realm()
{
	M0_PRE(clovis_uber_realm.re_instance != NULL);
	return &clovis_uber_realm;
}

int adopt_mero_thread(struct clovis_workload_task *task)
{
	int		  rc = 0;

	M0_PRE(crate_clovis_uber_realm() != NULL);
	M0_PRE(crate_clovis_uber_realm()->re_instance != NULL);

	if (m0_thread_tls() != NULL)
		return M0_RC(rc);

	rc = m0_thread_adopt(&task->mthread, clovis_instance->m0c_mero);
	if (rc < 0) {
		crlog(CLL_ERROR, "Mero adoptation failed");
	}

	return M0_RC(rc);
}

void release_mero_thread(struct clovis_workload_task *task)
{
	m0_thread_shun();
}

static void dix_config_init(struct m0_idx_dix_config *conf)
{
	conf->kc_create_meta = false;
}

int clovis_init(struct workload *w)
{
	int rc;

	num_clovis_workloads++;

	if (num_clovis_workloads != 1) {
		rc = 0;
		goto do_exit;
	}

	clovis_conf.cc_is_addb_init          = conf->is_addb_init;
	clovis_conf.cc_is_oostore            = conf->is_oostrore;
	clovis_conf.cc_is_read_verify        = conf->is_read_verify;
	clovis_conf.cc_local_addr            = conf->clovis_local_addr;
	clovis_conf.cc_ha_addr               = conf->clovis_ha_addr;
	clovis_conf.cc_profile               = conf->clovis_prof;
	clovis_conf.cc_process_fid           = conf->clovis_process_fid;
	clovis_conf.cc_tm_recv_queue_min_len = conf->tm_recv_queue_min_len ?:
	                                       M0_NET_TM_RECV_QUEUE_DEF_LEN;
	clovis_conf.cc_max_rpc_msg_size      = conf->max_rpc_msg_size ?:
	                                       M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	clovis_conf.cc_layout_id             = conf->layout_id;
	clovis_conf.cc_idx_service_id        = conf->index_service_id;

	if (clovis_conf.cc_idx_service_id == M0_CLOVIS_IDX_CASS) {
		cass_conf.cc_cluster_ep              = conf->cass_cluster_ep;
		cass_conf.cc_keyspace                = conf->cass_keyspace;
		cass_conf.cc_max_column_family_num   = conf->col_family;
		clovis_conf.cc_idx_service_conf = &cass_conf;
        } else if (clovis_conf.cc_idx_service_id == M0_CLOVIS_IDX_DIX ||
		   clovis_conf.cc_idx_service_id == M0_CLOVIS_IDX_MOCK) {
                dix_config_init(&dix_conf);
                clovis_conf.cc_idx_service_conf = &dix_conf;
        } else {
		rc = -EINVAL;
		cr_log(CLL_ERROR, "Unknown index service id: %d!\n",
		       clovis_conf.cc_idx_service_id);
		goto do_exit;
	}

	/* Clovis instance */
	rc = m0_clovis_init(&clovis_instance, &clovis_conf, true);
	if (rc != 0) {
		cr_log(CLL_ERROR, "Failed to initialise Clovis: %d\n", rc);
		goto do_exit;
	}

	M0_POST(clovis_instance != NULL);

	/* And finally, clovis root realm */
	m0_clovis_container_init(&clovis_container,
				 NULL, &M0_CLOVIS_UBER_REALM,
				 clovis_instance);

	rc = clovis_container.co_realm.re_entity.en_sm.sm_rc;
	if (rc != 0) {
		cr_log(CLL_ERROR, "Failed to open uber realm\n");
		goto do_exit;
	}

	M0_POST(clovis_container.co_realm.re_instance != NULL);
	clovis_uber_realm = clovis_container.co_realm;

do_exit:
	return rc;
}

void free_clovis_conf()
{
	m0_free(conf->clovis_local_addr);
	m0_free(conf->clovis_ha_addr);
	m0_free(conf->clovis_prof);
	m0_free(conf->clovis_process_fid);
	m0_free(conf->cass_cluster_ep);
	m0_free(conf->cass_keyspace);
	m0_free(conf);
}

int clovis_fini(struct workload *w)
{
	num_clovis_workloads--;
	if(num_clovis_workloads == 0) {
		m0_clovis_fini(clovis_instance, true);
		free_clovis_conf();
	}
	return 0;
}

void clovis_check(struct workload *w)
{
}

/** @} end of crate_clovis_utils group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
