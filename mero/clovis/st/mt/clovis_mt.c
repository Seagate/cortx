/* -*- C -*- */
/*
 * COPYRIGHT 2018 SEAGATE LLC
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
 * Original author:  Anatoliy Bilenko <Anatoliy.Bilenko@seagate.com>
 * Original creation date: 8-Sep-2018
 */
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"                /* M0_ERR */
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"
#include "lib/getopts.h"	/* M0_GETOPTS */
#include "module/instance.h"	/* m0 */


/* Clovis parameters */
static char *clovis_local_addr;
static char *clovis_ha_addr;
static char *clovis_prof;
static char *clovis_proc_fid;

static struct m0_clovis          *clovis_instance = NULL;
static struct m0_clovis_container clovis_container;
static struct m0_clovis_realm     clovis_uber_realm;
static struct m0_clovis_config    clovis_conf;
static bool                       clovis_ls = false;
static struct m0_idx_dix_config   dix_conf;
static struct m0 instance;

extern void clovis_st_mt_inst(struct m0_clovis *clovis);
void clovis_st_lsfid_inst(struct m0_clovis *clovis,
			  void (*print)(struct m0_fid*));

static void ls_print(struct m0_fid* fid)
{
	m0_console_printf(FID_F"\n", FID_P(fid));
}

static int init_clovis(void)
{
	int rc;

	clovis_conf.cc_is_addb_init          = true;
	clovis_conf.cc_is_oostore            = true;
	clovis_conf.cc_is_read_verify        = false;
	clovis_conf.cc_local_addr            = clovis_local_addr;
	clovis_conf.cc_ha_addr               = clovis_ha_addr;
	clovis_conf.cc_profile               = clovis_prof;
	clovis_conf.cc_process_fid           = clovis_proc_fid;
	clovis_conf.cc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	clovis_conf.cc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	clovis_conf.cc_idx_service_id        = M0_CLOVIS_IDX_DIX;

	dix_conf.kc_create_meta = false;
	clovis_conf.cc_idx_service_conf      = &dix_conf;

	/* Clovis instance */
	rc = m0_clovis_init(&clovis_instance, &clovis_conf, true);
	if (rc != 0) {
		m0_console_printf("Failed to initialise Clovis\n");
		goto err_exit;
	}

	/* And finally, clovis root realm */
	m0_clovis_container_init(&clovis_container,
				 NULL, &M0_CLOVIS_UBER_REALM,
				 clovis_instance);
	rc = clovis_container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0) {
		m0_console_printf("Failed to open uber realm\n");
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

int main(int argc, char **argv)
{
	int rc;

	m0_instance_setup(&instance);
	rc = m0_module_init(&instance.i_self, M0_LEVEL_INST_ONCE);
	if (rc != 0) {
		m0_console_printf("Cannot init module %i\n", rc);
		return rc;
	}

	rc = M0_GETOPTS("m0mt", argc, argv,
			M0_VOIDARG('s', "List all fids clovis can see",
					LAMBDA(void, (void) {
					clovis_ls = true;
					})),
			M0_STRINGARG('l', "Local endpoint address",
					LAMBDA(void, (const char *str) {
					clovis_local_addr = (char*)str;
					})),
			M0_STRINGARG('h', "HA address",
					LAMBDA(void, (const char *str) {
					clovis_ha_addr = (char*)str;
					})),
			M0_STRINGARG('f', "Process FID",
					LAMBDA(void, (const char *str) {
					clovis_proc_fid = (char*)str;
					})),
			M0_STRINGARG('p', "Profile options for Clovis",
					LAMBDA(void, (const char *str) {
					clovis_prof = (char*)str;
					})));
	if (rc != 0) {
		m0_console_printf("Usage: c0mt -l laddr -h ha_addr "
				  "-p prof_opt -f proc_fid [-s]\n");
		goto mod;
	}

	/* Initialise mero and Clovis */
	rc = init_clovis();
	if (rc < 0) {
		m0_console_printf("clovis_init failed!\n");
		goto mod;
	}

	if (clovis_ls) {
		m0_console_printf("FIDs seen with clovis:\n");
		clovis_st_lsfid_inst(clovis_instance, ls_print); /* Ls test */
	} else {
		m0_console_printf("Up to 500 clovis requests pending, ~100s on devvm:\n");
		clovis_st_mt_inst(clovis_instance); 	         /* Load test */
	}

	/* Clean-up */
	fini_clovis();
mod:
	m0_module_fini(&instance.i_self, M0_MODLEV_NONE);

	return rc;
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
