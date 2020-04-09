/* -*- C -*- */
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
 * Original author: Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original creation date: 08/03/2011
 */

#include <sysexits.h>

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER
#include "lib/trace.h"

#include "lib/errno.h"           /* ETIMEDOUT */
#include "lib/memory.h"          /* m0_free */
#include "lib/getopts.h"         /* M0_GETOPTS */
#include "mero/init.h"           /* m0_init */
#include "rpc/rpclib.h"          /* m0_rpc_post_sync */
#include "fop/fop.h"
#include "fid/fid.h"             /* M0_FID_TINIT */
#include "fis/fi_command_fops.h" /* m0_fi_command_fop_init */
#include "conf/obj.h"            /* M0_CONF_PROCESS_TYPE */
#include "module/instance.h"     /* m0 */
#include "console/console.h"
#include "console/console_mesg.h"
#include "console/console_it.h"
#include "console/console_yaml.h"
#include "console/console_fop.h"

#define PROC_FT M0_CONF_PROCESS_TYPE.cot_ftype.ft_id
/**
   @addtogroup console
   @{
 */

bool m0_console_verbose;

static uint32_t timeout;

/**
 * @brief Iterate over FOP and print names of its members.
 *
 * @param opcode Item type opcode
 */
static int fop_info_show(uint32_t opcode)
{
	struct m0_fop_type *ftype;

	fprintf(stdout, "\n");
	ftype = m0_fop_type_find(opcode);
	if (ftype == NULL) {
		fprintf(stderr, "Invalid FOP opcode %.2d.\n", opcode);
		return M0_ERR(-EINVAL);
	}
	m0_cons_fop_name_print(ftype);
	fprintf(stdout, "\n");
	return m0_cons_fop_show(ftype);
}

/**
 * @brief Build the RPC item using FOP (Embedded into item) and send it.
 *
 * @param cctx RPC Client context
 * @param opcode RPC item opcode
 */
static int fop_send_and_print(struct m0_rpc_client_ctx *cctx, uint32_t opcode,
			      const char *fop_input)
{
	struct m0_fop_type *ftype;
	struct m0_rpc_item *item;
	struct m0_fop	   *fop;
	struct m0_fop	   *rfop;
	int		    rc;

	M0_ENTRY("opcode=%u fop_input=%s", opcode, fop_input);

	ftype = m0_fop_type_find(opcode);
	if (ftype == NULL)
		return M0_ERR_INFO(-EINVAL, "opcode=%u, fop type not found",
				   opcode);
	fop = m0_fop_alloc(ftype, NULL, &cctx->rcx_rpc_machine);
	if (fop == NULL)
		return M0_ERR_INFO(-ENOMEM, "opcode=%u, m0_fop_alloc() failed",
				   opcode);
	rc = fop_input == NULL ? m0_cons_fop_obj_input(fop) :
		m0_xcode_read(&M0_FOP_XCODE_OBJ(fop), fop_input);
	if (rc != 0)
		return M0_ERR_INFO(rc, "rc=%d with fop_input=%s", rc,
				   fop_input);

	printf("Sending FOP: ");
	m0_cons_fop_name_print(ftype);

	rc = m0_cons_fop_obj_output(fop);
	if (rc != 0)
		return M0_ERR(rc);

	fop->f_item.ri_nr_sent_max = timeout;
	rc = m0_rpc_post_sync(fop, &cctx->rcx_session, NULL, 0 /* deadline*/);
	if (rc != 0) {
		m0_fop_put_lock(fop);
		return M0_ERR_INFO(-EINVAL, "Sending message failed");
	}

	/* Fetch the FOP reply */
	item = &fop->f_item;
        if (item->ri_error != 0) {
		m0_fop_put_lock(fop);
		return M0_ERR_INFO(-EINVAL, "rpc item receive failed");
	}

	rfop = m0_rpc_item_to_fop(item->ri_reply);
	if(rfop == NULL) {
		m0_fop_put_lock(fop);
		return M0_ERR_INFO(-EINVAL, "RPC item reply not received");
	}

	/* Print reply */
	printf("Server replied with FOP: ");
	m0_cons_fop_name_print(rfop->f_type);

	rc = m0_cons_fop_obj_output(rfop);
	if (rc == 0)
		m0_fop_put_lock(fop);
	return M0_RC(rc);
}

static const char *usage_msg = "Usage: m0console "
	" { -l FOP list | -f FOP opcode }"
	" [-s server (e.g. 172.18.50.40@o2ib1:12345:34:1) ]"
	" [-c client (e.g. 172.18.50.40@o2ib1:12345:34:*) ]"
	" [-t timeout]"
	/*
	 * Please refer to xcode/xcode.h for more information on
	 * xcode read grammar
	 */
	" [-d fop_description (in xcode read grammar)] "
	" [[-i] [-y yaml_file_path]]"
	" [-h] [-v]";

static void usage(void)
{
	fprintf(stderr, "%s\n", usage_msg);
}

#ifdef CONSOLE_UT
static int console_init(void)
{
	return 0;
}

static void console_fini(void)
{
}
#else
static int console_init(void)
{
	static struct m0 instance;
	int rc;

	rc = m0_init(&instance);
	if (rc != 0)
		return M0_RC(rc);

	rc = m0_console_fop_init();
	if (rc != 0)
		m0_fini();
	return M0_RC(rc);
}

static void console_fini(void)
{
	m0_console_fop_fini();
	m0_fini();
}
#endif /* !CONSOLE_UT */

/**
 * @brief The service to connect to is specified at the command line.
 *
 *	  The fop type to be sent is specified at the command line.
 *
 *	  The values of fop fields are specified interactively. The program
 *	  locates the fop type format (m0_fop_type_format) corresponding to the
 *	  specified fop type and iterates over fop fields, prompting the user
 *	  for the field values.
 *
 *	  The program supports RECORD, SEQUENCE and UNION aggregation
 *	  types, as well as all atomic types (U32, U64, BYTE and VOID).
 *
 * @return 0 success, -errno failure.
 */
#ifdef CONSOLE_UT
int console_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
	int                   result;
	uint32_t              opcode         = 0;
	bool                  show           = false;
	bool                  input          = false;
	const char           *client         = NULL;
	const char           *server         = NULL;
	const char           *yaml_path      = NULL;
	const char           *fop_desc       = NULL;
	struct m0_net_domain  client_net_dom = {};
	uint32_t              tm_recv_queue_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	uint32_t              max_rpc_msg_size  = M0_RPC_DEF_MAX_RPC_MSG_SIZE;

	struct m0_rpc_client_ctx cctx = {
		.rcx_net_dom               = &client_net_dom,
		.rcx_local_addr            = "0@lo:12345:34:*",
		.rcx_remote_addr           = "0@lo:12345:34:1",
		.rcx_max_rpcs_in_flight    = 1,
		.rcx_fid                   = &M0_FID_TINIT(PROC_FT, 0, 1),
	};

	m0_console_verbose = false;
	yaml_support = false;
	timeout = 10;

	result = console_init();
	if (result != 0)
		return EX_CONFIG;

	/*
	 * Gets the info to connect to the service and type of fop to be send.
	 */
	result = M0_GETOPTS("m0console", argc, argv,
			    M0_HELPARG('h'),
			    M0_FLAGARG('l', "show list of fops", &show),
			    M0_FORMATARG('f', "fop type", "%u", &opcode),
			    M0_STRINGARG('s', "server",
					 LAMBDA(void, (const char *name) {
							 server =  name;
						 })),
			    M0_STRINGARG('c', "client",
					 LAMBDA(void, (const char *name) {
							 client = name;
						 })),
			    M0_FORMATARG('t', "wait time(in seconds)",
					 "%u", &timeout),
			    M0_FLAGARG('i', "yaml input", &input),
			    M0_FORMATARG('q', "minimum TM receive queue length",
					 "%i", &tm_recv_queue_len),
			    M0_FORMATARG('m', "max rpc msg size", "%i",
					 &max_rpc_msg_size),
			    M0_STRINGARG('y', "yaml file path",
					 LAMBDA(void, (const char *name) {
							 yaml_path = name;
						 })),
			    M0_STRINGARG('d', "fop description",
					 LAMBDA(void, (const char *fd) {
							 fop_desc = fd;
						 })),
			    M0_FLAGARG('v', "verbose", &m0_console_verbose));
	if (result != 0)
		/*
		 * No need to print "usage" here, M0_GETOPTS will automatically
		 * do it for us
		 */
		return EX_USAGE;

	if (argc == 1 || (m0_console_verbose && argc == 2)) {
		usage();
		return EX_USAGE;
	}

	/* Input is false but yaml is assigned path or
	 * both yaml and fop description is provided */
	if ((!input && yaml_path != NULL) ||
	    (input && yaml_path == NULL)  ||
	    (fop_desc != NULL && yaml_path != NULL)) {
		usage();
		return EX_USAGE;
	}

	/* Init YAML info */
	if (input) {
		result = m0_cons_yaml_init(yaml_path);
		if (result != 0) {
			fprintf(stderr, "YAML Init failed\n");
			return EX_NOINPUT;
		}

		server = m0_cons_yaml_get_value("server");
		if (server == NULL) {
			fprintf(stderr, "Server assignment failed\n");
			result = EX_DATAERR;
			goto end;
		}
		client = m0_cons_yaml_get_value("client");
		if (client == NULL) {
			fprintf(stderr, "Client assignment failed\n");
			result = EX_DATAERR;
			goto end;
		}
	}

	if (show && opcode <= 0) {
		m0_cons_fop_list_show();
		usage();
		result = EX_USAGE;
		goto end;
	}

	if (show && opcode > 0) {
		result = fop_info_show(opcode);
		if (result == 0)
			result = EX_OK;
		goto end;
	}

	result = m0_net_domain_init(&client_net_dom, &m0_net_lnet_xprt);
	M0_ASSERT(result == 0);

	/* Init the console members from CLI input */
	if (server != NULL)
		cctx.rcx_remote_addr = server;
	if (client != NULL)
		cctx.rcx_local_addr = client;
	cctx.rcx_recv_queue_min_length = tm_recv_queue_len;
	cctx.rcx_max_rpc_msg_size      = max_rpc_msg_size;
	cctx.rcx_abs_timeout = m0_time_from_now(M0_RPCLIB_UTIL_CONN_TIMEOUT, 0);

	printf("connecting from %s to %s\n", cctx.rcx_local_addr,
	       cctx.rcx_remote_addr);
	result = m0_rpc_client_start(&cctx);
	if (result != 0) {
		fprintf(stderr, "m0_rpc_client_start failed\n");
		result = EX_SOFTWARE;
		goto end;
	}
	printf("connected\n");

	printf("Console Address = %s\n", cctx.rcx_local_addr);
	printf("Server Address = %s\n", cctx.rcx_remote_addr);

	/*
	 * Need to init fic fop explicitly here as normally it gets initialised
	 * at FI Service start (see fis_start()).
	 */
	m0_fi_command_fop_init();
	/* Build the fop/fom/item and send */
	result = fop_send_and_print(&cctx, opcode, fop_desc);
	if (result != 0) {
		fprintf(stderr, "fop_send_and_print failed\n");
		result = EX_SOFTWARE;
		goto cleanup;
	}
cleanup:
	m0_fi_command_fop_fini();
	result = m0_rpc_client_stop(&cctx);
	M0_ASSERT(result == 0);
end:
	console_fini();
	if (input)
		m0_cons_yaml_fini();

	return result;
}

/** @} end of console group */
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
