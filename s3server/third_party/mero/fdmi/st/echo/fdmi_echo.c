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
 * Original author:
 * Original creation date:
 */


#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/getopts.h"
#include "lib/memory.h"
#include "lib/misc.h"           /* M0_SET0 */
#include "lib/thread.h"
#include "lib/time.h"
#include "mero/init.h"
#include "reqh/reqh.h"          /* m0_reqh_rpc_mach_tl */
#include "net/net.h"
#include "net/lnet/lnet.h"
#include "fop/fop.h"            /* m0_fop_default_item_ops */
#include "rpc/rpc.h"
#include "rpc/rpclib.h"         /* m0_rpc_server_start */
#include "ut/cs_service.h"      /* m0_cs_default_stypes */
#include "fop/fom_generic.h"    /* m0_rpc_item_generic_reply_rc */
#include "reqh/reqh.h"          /* m0_reqh_rpc_mach_tl */
#include "fdmi/fdmi.h"
#include "fdmi/fops.h"
#include "fdmi/plugin_dock.h"
#include "fol/fol.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>		/* read */
#include "module/instance.h"  /* m0 */

#define TRANSPORT_NAME  "lnet"

#define SERVER_DB_FILE_NAME        "echo_plugin_server.db"
#define SERVER_STOB_FILE_NAME      "echo_plugin_server.stob"
#define SERVER_ADDB_STOB_FILE_NAME "linuxstob:echo_plugin_server_addb.stob"
#define SERVER_LOG_FILE_NAME       "echo_plugin.log"

static struct m0 instance;

const char g_filenane[] = "/tmp/fdmi_plugin.txt";

struct fdmi_plugin_ctx {
	const struct m0_fdmi_pd_ops *pdo;
	struct m0_fid                flt_fids[1];
	FILE                        *log_file;
	char                        *log_str;
	int                          log_str_size;
};

struct fdmi_plugin_ctx g_plugin_ctx;

enum {
	BUF_LEN            = 128,
	M0_LNET_PORTAL     = 34,
	MAX_RPCS_IN_FLIGHT = 32,
	MAX_RETRIES        = 10
};


static bool  verbose           = false;
static char *server_nid        = "10.0.2.15@tcp";
//static char *server_nid        = "192.168.183.187@tcp";

static int   server_tmid       = 1;
static int   tm_recv_queue_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
static int   max_rpc_msg_size  = M0_RPC_DEF_MAX_RPC_MSG_SIZE;

static char server_endpoint[M0_NET_LNET_XEP_ADDR_LEN];

static struct m0_net_xprt *xprt = &m0_net_lnet_xprt;

static int build_endpoint_addr(char *out_buf, size_t buf_size)
{
	char *ep_name;
	char *nid;
	int   tmid;

	nid = server_nid;
	ep_name = "server";
	tmid = server_tmid;

	if (buf_size > M0_NET_LNET_XEP_ADDR_LEN)
		return -1;
	else
		snprintf(out_buf, buf_size, "%s:%u:%u:%u", nid,
			 (unsigned int)M0_NET_LNET_PID,
			 (unsigned int)M0_LNET_PORTAL,
			 (unsigned int)tmid);

	if (verbose)
		printf("%s endpoint: %s\n", ep_name, out_buf);

	return 0;
}

static void quit_dialog(void)
{
	char ch;
	size_t nr;

	printf("\n########################################\n");
	printf("\n\nPlugin started. Press Enter to terminate\n\n");
	printf("\n########################################\n");
	nr = read(0, &ch, sizeof ch);
	if (nr > 0)
		printf("\n\nPlugin terminated.\n");
}

static int int2str(char *dest, size_t size, int src, int defval)
{
	int rc = snprintf(dest, size, "%d", src > 0 ? src : defval);
	return (rc < 0 || rc >= size) ? -EINVAL : 0;
}


int handle_fdmi_rec_not(struct m0_uint128  *rec_id,
			struct m0_buf      fdmi_rec,
			struct m0_fid      filter_id)
{
	struct m0_fol_rec       fol_rec;
	int                     rc;
	struct fdmi_plugin_ctx *ctx = &g_plugin_ctx;

	/** @todo Call decode for fdmi record type (frt_rec_decode) */
	m0_fol_rec_init(&fol_rec, NULL);

	m0_fol_rec_decode(&fol_rec, &fdmi_rec);

	rc = m0_fol_rec_to_str(&fol_rec, ctx->log_str, ctx->log_str_size);

	while (rc == -ENOMEM && ctx->log_str) {
		m0_free(ctx->log_str);
		M0_ALLOC_ARR(ctx->log_str, ctx->log_str_size + 1024);

		M0_ASSERT(ctx->log_str != NULL);

		rc = m0_fol_rec_to_str(&fol_rec, ctx->log_str, ctx->log_str_size);
		ctx->log_str_size += 1024;
	}

	if (rc == 0) {
		fprintf(ctx->log_file, "#####################\n");
		fprintf(ctx->log_file, "%s\n", ctx->log_str);
		fprintf(ctx->log_file, "#####################\n");
	}

	m0_fol_rec_fini(&fol_rec);

	/** @todo Consider special ret code if processing is complete ? */
	return -EINVAL;
}

static int init_plugin(struct fdmi_plugin_ctx *ctx,
	               const char             *fname)
{
	int                              rc;
	const struct m0_fdmi_plugin_ops  pcb = {
		.po_fdmi_rec = handle_fdmi_rec_not
	};

	const struct m0_fdmi_filter_desc fd;

	ctx->log_file = fopen(fname, "w+");

	if (ctx->log_file == NULL)
		return -errno;

	M0_ALLOC_ARR(ctx->log_str, 1024);

	if (ctx->log_str == NULL) {
		rc = -ENOMEM;
		goto str_alloc_fail;
	}

	ctx->log_str_size = 1024;

	ctx->pdo = m0_fdmi_plugin_dock_api_get();

	/*.f_container = 0xB5C0A99B8817440B,*/
	/*.f_key       = 0x877A9A5FAFFD6BB7,*/
	ctx->flt_fids[0].f_container = 11;
	ctx->flt_fids[0].f_key       = 11;

	ctx->pdo = m0_fdmi_plugin_dock_api_get();

	rc = ctx->pdo->fpo_register_filter(&ctx->flt_fids[0], &fd, &pcb);

	if (rc != 0) {
		goto register_fail;
	}

	ctx->pdo->fpo_enable_filters(true, ctx->flt_fids,
		                     ARRAY_SIZE(ctx->flt_fids));

	return rc;

register_fail:
	m0_free(ctx->log_str);
str_alloc_fail:
	fclose(ctx->log_file);
	return rc;
}

static void deinit_plugin(struct fdmi_plugin_ctx *ctx)
{
	ctx->pdo->fpo_enable_filters(false, ctx->flt_fids,
		                     ARRAY_SIZE(ctx->flt_fids));

	ctx->pdo->fpo_deregister_plugin(
			ctx->flt_fids, ARRAY_SIZE(ctx->flt_fids));

	fclose(ctx->log_file);
}

static int run_server(void)
{
	enum { STRING_LEN = 16 };
	static char tm_len[STRING_LEN];
	static char rpc_size[STRING_LEN];
	int	    rc;
	char       *argv[] = {
		"rpclib_ut", "-T", "AD", "-D", SERVER_DB_FILE_NAME,
		"-S", SERVER_STOB_FILE_NAME, "-e", server_endpoint,
		"-A", SERVER_ADDB_STOB_FILE_NAME, "-w", "5",
		"-s", "ds1", "-s", "ds2", "-s", "fdmi", "-q", tm_len, "-m", rpc_size,
	};
	struct m0_rpc_server_ctx sctx = {
		.rsx_xprts            = &xprt,
		.rsx_xprts_nr         = 1,
		.rsx_argv             = argv,
		.rsx_argc             = ARRAY_SIZE(argv),
		.rsx_log_file_name    = SERVER_LOG_FILE_NAME
	};

	rc = int2str(tm_len, sizeof tm_len, tm_recv_queue_len,
		     M0_NET_TM_RECV_QUEUE_DEF_LEN) ?:
	     int2str(rpc_size, sizeof rpc_size, max_rpc_msg_size,
		     M0_RPC_DEF_MAX_RPC_MSG_SIZE);
	if (rc != 0)
		return rc;

	rc = m0_init(&instance);
	if (rc != 0)
		return rc;

	rc = m0_cs_default_stypes_init();
	if (rc != 0)
		goto m0_fini;

	rc = init_plugin(&g_plugin_ctx, g_filenane);
	if (rc != 0)
		goto fop_fini;

	/*
	 * Prepend transport name to the beginning of endpoint,
	 * as required by mero-setup.
	 */
	strcpy(server_endpoint, TRANSPORT_NAME ":");

	rc = build_endpoint_addr(
		server_endpoint + strlen(server_endpoint),
		sizeof(server_endpoint) - strlen(server_endpoint));
	if (rc != 0)
		goto plugin_fini;

	rc = m0_rpc_server_start(&sctx);
	if (rc != 0)
		goto plugin_fini;

	quit_dialog();

	m0_rpc_server_stop(&sctx);

plugin_fini:
	deinit_plugin(&g_plugin_ctx);
fop_fini:
	m0_cs_default_stypes_fini();
m0_fini:
	m0_fini();
	return rc;
}

int main(int argc, char *argv[])
{
	int rc;

	rc = run_server();

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
