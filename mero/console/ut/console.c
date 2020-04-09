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

#include <unistd.h>           /* truncate(2) */
#include <sys/types.h>        /* truncate(2) */

#define CONSOLE_UT
#include "console/console.c"  /* timeout */
#include "net/lnet/lnet.h"    /* m0_net_lnet_xprt */
#include "rpc/rpclib.h"       /* m0_rpc_client_ctx */
#include "rpc/rpc_opcodes.h"  /* M0_CONS_FOP_DEVICE_OPCODE */
#include "ut/misc.h"          /* M0_UT_PATH */
#include "ut/ut.h"

/**
   @addtogroup console
   @{
 */

enum {
	COB_DOM_CLIENT_ID  = 14,
	COB_DOM_SERVER_ID = 15,
};

static const char *yaml_file = "/tmp/console_ut.yaml";
static const char *err_file = "/tmp/stderr";
static const char *out_file = "/tmp/stdout";
static const char *in_file = "/tmp/stdin";

static struct m0_ut_redirect in_redir;
static struct m0_ut_redirect out_redir;
static struct m0_ut_redirect err_redir;
static struct m0_rpc_machine cons_mach;

#define CLIENT_ENDPOINT_ADDR       "0@lo:12345:34:2"

#define SERVER_ENDPOINT_ADDR	   "0@lo:12345:34:1"
#define SERVER_ENDPOINT		   "lnet:" SERVER_ENDPOINT_ADDR
#define SERVER_DB_FILE_NAME	   "cons_server_db"
#define SERVER_STOB_FILE_NAME	   "cons_server_stob"
#define SERVER_ADDB_STOB_FILE_NAME "linuxstob:cons_server_addb_stob"
#define SERVER_LOG_FILE_NAME	   "cons_server.log"

enum {
	CLIENT_COB_DOM_ID  = 14,
	MAX_RPCS_IN_FLIGHT = 1,
	MAX_RETRIES        = 5,
};

static struct m0_net_xprt   *xprt = &m0_net_lnet_xprt;
static struct m0_net_domain  client_net_dom = { };

static struct m0_rpc_client_ctx cctx = {
	.rcx_net_dom            = &client_net_dom,
	.rcx_local_addr         = CLIENT_ENDPOINT_ADDR,
	.rcx_remote_addr        = SERVER_ENDPOINT_ADDR,
	.rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT,
	.rcx_fid                = &g_process_fid,
};

static char *server_argv[] = {
	"console_ut", "-T", "AD", "-D", SERVER_DB_FILE_NAME,
	"-S", SERVER_STOB_FILE_NAME, "-A", SERVER_ADDB_STOB_FILE_NAME,
	"-w", "10", "-e", SERVER_ENDPOINT,  "-H", SERVER_ENDPOINT_ADDR,
	"-f", M0_UT_CONF_PROCESS,
	"-c", M0_UT_PATH("conf.xc")
};

static struct m0_rpc_server_ctx sctx;

static int cons_init(void)
{
	int result;

	timeout = 10;
	result = m0_console_fop_init();
	M0_ASSERT(result == 0);

	result = m0_net_domain_init(&client_net_dom, xprt);
	M0_ASSERT(result == 0);

	m0_sm_group_init(&cons_mach.rm_sm_grp);
	return result;
}

static int cons_fini(void)
{
	m0_net_domain_fini(&client_net_dom);
	m0_console_fop_fini();
	m0_sm_group_fini(&cons_mach.rm_sm_grp);
	return 0;
}

static void file_redirect_init(void)
{
	m0_stream_redirect(stdin, in_file, &in_redir);
	m0_stream_redirect(stdout, out_file, &out_redir);
	m0_stream_redirect(stderr, err_file, &err_redir);
}

static void file_redirect_fini(void)
{
	int result;

	m0_stream_restore(&in_redir);
	m0_stream_restore(&out_redir);
	m0_stream_restore(&err_redir);

	result = remove(in_file);
	M0_UT_ASSERT(result == 0);
	result = remove(out_file);
	M0_UT_ASSERT(result == 0);
	result = remove(err_file);
	M0_UT_ASSERT(result == 0);
}

static int generate_yaml_file(const char *name)
{
	FILE *fp;

	M0_PRE(name != NULL);

        fp = fopen(name, "w");
        if (fp == NULL) {
                fprintf(stderr, "Failed to create yaml file\n");
                return -errno;
        }

	fprintf(fp, "# Generated yaml file for console UT\n\n");
	fprintf(fp, "server  : localhost\n");
	fprintf(fp, "sport   : 23125\n");
	fprintf(fp, "client  : localhost\n");
	fprintf(fp, "cport   : 23126\n");
	fprintf(fp, "\n\n");
	fprintf(fp, "Test FOP:\n");
	fprintf(fp, "  - cons_test_type : A\n");
	fprintf(fp, "    cons_test_id : 495\n");
	fprintf(fp, "    cons_seq : 144\n");
	fprintf(fp, "    cons_oid : 233\n");
	fprintf(fp, "    cons_size : 5\n");
	fprintf(fp, "    cons_buf : abcde\n");

	fclose(fp);
	return 0;
}

static void check_values(struct m0_fop *fop)
{
	struct m0_cons_fop_test *test = m0_fop_data(fop);

	M0_UT_ASSERT(test->cons_test_type == 'A');
	M0_UT_ASSERT(test->cons_test_id == 495);
	M0_UT_ASSERT(test->cons_id.cons_seq == 144);
	M0_UT_ASSERT(test->cons_id.cons_oid == 233);
	M0_UT_ASSERT(test->cons_test_buf.cons_size == 5);
	M0_UT_ASSERT(strncmp("abcde", (char *)test->cons_test_buf.cons_buf,
	                     test->cons_test_buf.cons_size) == 0);
}

static void yaml_basic_test(void)
{
	int result;

	result = generate_yaml_file(yaml_file);
	M0_UT_ASSERT(result == 0);
	result = m0_cons_yaml_init(yaml_file);
	M0_UT_ASSERT(result == 0);

	/* Init and Fini */
	m0_cons_yaml_fini();
	result = m0_cons_yaml_init(yaml_file);
	M0_UT_ASSERT(result == 0);
	m0_cons_yaml_fini();

	result = remove(yaml_file);
	M0_UT_ASSERT(result == 0);
}

static void input_test(void)
{
        struct m0_fop *fop;
	int            result;

	file_redirect_init();
	result = generate_yaml_file(yaml_file);
	M0_UT_ASSERT(result == 0);
	result = m0_cons_yaml_init(yaml_file);
	M0_UT_ASSERT(result == 0);

        fop = m0_fop_alloc(&m0_cons_fop_test_fopt, NULL, &cons_mach);
        M0_UT_ASSERT(fop != NULL);

        m0_cons_fop_obj_input(fop);
	check_values(fop);
	m0_fop_put_lock(fop);
	m0_cons_yaml_fini();
	result = remove(yaml_file);
	M0_UT_ASSERT(result == 0);
	file_redirect_fini();
}

static void file_compare(const char *in, const char *out)
{
	FILE *infp;
	FILE *outfp;
	int   inc;
	int   outc;

	infp = fopen(in, "r");
	M0_UT_ASSERT(infp != NULL);
	outfp = fopen(out, "r");
	M0_UT_ASSERT(outfp != NULL);

	while ((inc = fgetc(infp)) != EOF &&
	       (outc = fgetc(outfp)) != EOF) {
	       M0_UT_ASSERT(inc == outc);
	}

	fclose(infp);
	fclose(outfp);
}

static void output_test(void)
{
        struct m0_fop *f;
	int	       result;

	m0_console_verbose = true;
	result = generate_yaml_file(yaml_file);
	M0_UT_ASSERT(result == 0);
	result = m0_cons_yaml_init(yaml_file);
	M0_UT_ASSERT(result == 0);

        f = m0_fop_alloc(&m0_cons_fop_test_fopt, NULL, &cons_mach);
        M0_UT_ASSERT(f != NULL);

	file_redirect_init();

        m0_cons_fop_obj_input(f);
	m0_cons_fop_obj_output(f);

	file_compare(in_file, out_file);
	file_redirect_fini();

	m0_console_verbose = false;
	m0_fop_put_lock(f);
	m0_cons_yaml_fini();
	result = remove(yaml_file);
	M0_UT_ASSERT(result == 0);
}

static void yaml_file_test(void)
{
	int result;

	file_redirect_init();
	result = m0_cons_yaml_init(yaml_file);
	M0_UT_ASSERT(result != 0);
	file_redirect_fini();
}

static void yaml_parser_test(void)
{
	FILE *fp;
	int   result;

	file_redirect_init();
        fp = fopen(yaml_file, "w");
        M0_UT_ASSERT(fp != NULL);
	fprintf(fp, "# Generated yaml file for console UT\n\n");
	fprintf(fp, "server  : localhost\n");
	fprintf(fp, "sport   : 23125\n");
	fprintf(fp, "client  : localhost\n");
	fprintf(fp, "cport   : 23126\n");
	fprintf(fp, "\n\n");
	fprintf(fp, "Test FOP:\n");
	fprintf(fp, "  - cons_seq : 1\n");
	/* Error introduced here */
	fprintf(fp, "cons_oid : 2\n");
	fprintf(fp, "    cons_test_type : d\n");
	fprintf(fp, "    cons_test_id : 64\n");
	fclose(fp);

	result = m0_cons_yaml_init(yaml_file);
	M0_UT_ASSERT(result != 0);
	result = remove(yaml_file);
	M0_UT_ASSERT(result == 0);
	file_redirect_fini();
}

static void yaml_root_get_test(void)
{
	FILE *fp;
	int   result;

	file_redirect_init();
        fp = fopen(yaml_file, "w");
        M0_UT_ASSERT(fp != NULL);
	fclose(fp);

	result = m0_cons_yaml_init(yaml_file);
	M0_UT_ASSERT(result != 0);
	result = remove(yaml_file);
	M0_UT_ASSERT(result == 0);
	file_redirect_fini();
}

static void yaml_get_value_test(void)
{
	uint32_t number;
	int	 result;
	char	*value;

	result = generate_yaml_file(yaml_file);
	M0_UT_ASSERT(result == 0);
	result = m0_cons_yaml_init(yaml_file);
	M0_UT_ASSERT(result == 0);

	value = m0_cons_yaml_get_value("server");
	M0_UT_ASSERT(value != NULL);
	M0_UT_ASSERT(strcmp("localhost", value) == 0);

	value = m0_cons_yaml_get_value("sport");
	M0_UT_ASSERT(value != NULL);
	number = strtoul(value, NULL, 10);
	M0_UT_ASSERT(number == 23125);

	value = m0_cons_yaml_get_value("client");
	M0_UT_ASSERT(value != NULL);
	M0_UT_ASSERT(strcmp("localhost", value) == 0);

	value = m0_cons_yaml_get_value("cport");
	M0_UT_ASSERT(value != NULL);
	number = strtoul(value, NULL, 10);
	M0_UT_ASSERT(number == 23126);

	value = m0_cons_yaml_get_value("cons_test_type");
	M0_UT_ASSERT(value != NULL);
	M0_UT_ASSERT(value[0] == 'A');

	value = m0_cons_yaml_get_value("cons_test_id");
	M0_UT_ASSERT(value != NULL);
	number = strtoul(value, NULL, 10);
	M0_UT_ASSERT(number == 495);

	value = m0_cons_yaml_get_value("cons_seq");
	M0_UT_ASSERT(value != NULL);
	number = strtoul(value, NULL, 10);
	M0_UT_ASSERT(number == 144);

	value = m0_cons_yaml_get_value("cons_oid");
	M0_UT_ASSERT(value != NULL);
	number = strtoul(value, NULL, 10);
	M0_UT_ASSERT(number == 233);

	value = m0_cons_yaml_get_value("cons_size");
	M0_UT_ASSERT(value != NULL);
	number = strtoul(value, NULL, 10);
	M0_UT_ASSERT(number == 5);

	value = m0_cons_yaml_get_value("cons_buf");
	M0_UT_ASSERT(value != NULL);
	M0_UT_ASSERT(strcmp("abcde", value) == 0);

	value = m0_cons_yaml_get_value("xxxx");
	M0_UT_ASSERT(value == NULL);

	m0_cons_yaml_fini();
	result = remove(yaml_file);
	M0_UT_ASSERT(result == 0);
}


static int device_yaml_file(const char *name)
{
	FILE *fp;

	M0_PRE(name != NULL);

        fp = fopen(name, "w");
        if (fp == NULL) {
                fprintf(stderr, "Failed to create yaml file\n");
                return -errno;
        }

	fprintf(fp, "# Generated yaml file for console UT\n\n");
	fprintf(fp, "server  : localhost\n");
	fprintf(fp, "sport   : 23125\n");
	fprintf(fp, "client  : localhost\n");
	fprintf(fp, "cport   : 23126\n");
	fprintf(fp, "\n\n");
	fprintf(fp, "Test FOP:\n");
	fprintf(fp, "  - cons_seq : 1\n");
	fprintf(fp, "    cons_oid : 2\n");
	fprintf(fp, "    cons_notify_type : 0\n");
	fprintf(fp, "    cons_dev_id : 64\n");
	fprintf(fp, "    cons_size : 8\n");
	fprintf(fp, "    cons_buf  : console\n");

	fclose(fp);
	return 0;
}

static void cons_client_init(struct m0_rpc_client_ctx *cctx)
{
	int result;

	/* Init Test */
	result = device_yaml_file(yaml_file);
	M0_UT_ASSERT(result == 0);
	result = m0_cons_yaml_init(yaml_file);
	M0_UT_ASSERT(result == 0);
	result = m0_rpc_client_start(cctx);
	M0_UT_ASSERT(result == 0);
}

static void cons_client_fini(struct m0_rpc_client_ctx *cctx)
{
	int result;

	/* Fini Test */
	result = m0_rpc_client_stop(cctx);
	M0_UT_ASSERT(result == 0);
	m0_cons_yaml_fini();
	result = remove(yaml_file);
	M0_UT_ASSERT(result == 0);
}

static void cons_server_init(struct m0_rpc_server_ctx *sctx)
{
	int result;

	*sctx = (struct m0_rpc_server_ctx){
		.rsx_xprts            = &xprt,
		.rsx_xprts_nr         = 1,
		.rsx_argv             = server_argv,
		.rsx_argc             = ARRAY_SIZE(server_argv),
		.rsx_log_file_name    = SERVER_LOG_FILE_NAME,
	};
	result = m0_rpc_server_start(sctx);
	M0_UT_ASSERT(result == 0);
}

static void cons_server_fini(struct m0_rpc_server_ctx *sctx)
{
	m0_rpc_server_stop(sctx);
}

static void conn_basic_test(void)
{
	cons_server_init(&sctx);
	cons_client_init(&cctx);
	cons_client_fini(&cctx);
	cons_server_fini(&sctx);
}

static void success_client(int dummy)
{
	cons_client_init(&cctx);
	cons_client_fini(&cctx);
}

static void conn_success_test(void)
{
	struct m0_thread client_handle;
	int		 result;

	cons_server_init(&sctx);
	M0_SET0(&client_handle);
	result = M0_THREAD_INIT(&client_handle, int, NULL, &success_client,
				0, "console-client");
	M0_UT_ASSERT(result == 0);
	m0_thread_join(&client_handle);
	m0_thread_fini(&client_handle);
	cons_server_fini(&sctx);
}

static void mesg_send_client(int dummy)
{
	struct m0_fop_type *ftype;
	struct m0_fop	   *fop;
	int		    result;

	cons_client_init(&cctx);

	ftype = m0_fop_type_find(M0_CONS_FOP_DEVICE_OPCODE);
	M0_UT_ASSERT(ftype != NULL);

	m0_cons_fop_name_print(ftype);
	printf("\n");
	fop = m0_fop_alloc(ftype, NULL, &cons_mach);
	M0_UT_ASSERT(fop != NULL);
	m0_cons_fop_obj_input(fop);
	fop->f_item.ri_nr_sent_max = MAX_RETRIES;
	result = m0_rpc_post_sync(fop, &cctx.rcx_session, NULL,
				  0 /* deadline */);
	M0_UT_ASSERT(result == 0);
	result = m0_rpc_item_wait_for_reply(&fop->f_item, M0_TIME_NEVER);
	M0_UT_ASSERT(result == 0);
	m0_fop_put_lock(fop);
	cons_client_fini(&cctx);
}

static void mesg_send_test(void)
{
	struct m0_thread client_handle;
	int		 result;

	file_redirect_init();
	cons_server_init(&sctx);
	M0_SET0(&client_handle);
	result = M0_THREAD_INIT(&client_handle, int, NULL, &mesg_send_client,
				0, "console-client");
	M0_UT_ASSERT(result == 0);
	m0_thread_join(&client_handle);
	m0_thread_fini(&client_handle);
	cons_server_fini(&sctx);
	file_redirect_fini();
}

static int console_cmd(const char *name, ...)
{
        va_list      list;
        va_list      clist;
        int          argc = 0;
	int	     result;
        const char **argv;
        const char **argp;
        const char  *arg;

        va_start(list, name);
        va_copy(clist, list);

        /* Count number of arguments */
        do {
                arg = va_arg(clist, const char *);
                argc++;
        } while(arg);
        va_end(clist);

        /* Allocate memory for pointer array */
        argp = argv = m0_alloc((argc + 1) * sizeof(const char *));
        M0_UT_ASSERT(argv != NULL);
	argv[argc] = NULL;

        /* Init list to array */
        *argp++ = name;
        do {
                arg = va_arg(list, const char *);
                *argp++ = arg;
        } while (arg);
        va_end(list);

        result = console_main(argc, (char **)argv);

	/* free memory allocated for argv */
	m0_free(argv);
	return result;
}

static void console_input_test(void)
{
	int  result;
	char buf[35];

	file_redirect_init();
	/* starts UT test for console main */
	result = console_cmd("no_input", NULL);
	M0_UT_ASSERT(result == EX_USAGE);
	M0_UT_ASSERT(m0_error_mesg_match(stderr, usage_msg));
	result = truncate(err_file, 0L);
	M0_UT_ASSERT(result == 0);
	fseek(stderr, 0L, SEEK_SET);

	result = console_cmd("no_input", "-v", NULL);
	M0_UT_ASSERT(result == EX_USAGE);
	M0_UT_ASSERT(m0_error_mesg_match(stderr, usage_msg));
	result = truncate(err_file, 0L);
	M0_UT_ASSERT(result == 0);
	fseek(stderr, 0L, SEEK_SET);

	fseek(stdout, 0L, SEEK_SET);
	result = console_cmd("list_fops", "-l", NULL);
	M0_UT_ASSERT(result == EX_USAGE);
	M0_UT_ASSERT(m0_error_mesg_match(stdout, "List of FOP's:"));
	result = truncate(out_file, 0L);
	M0_UT_ASSERT(result == 0);
	fseek(stdout, 0L, SEEK_SET);

	sprintf(buf, "%d", M0_CONS_FOP_DEVICE_OPCODE);
	result = console_cmd("show_fops", "-l", "-f", buf, NULL);
	M0_UT_ASSERT(result == EX_OK);
	sprintf(buf, "%.2d Device Failed",
		     M0_CONS_FOP_DEVICE_OPCODE);
	M0_UT_ASSERT(m0_error_mesg_match(stdout, buf));
	result = truncate(out_file, 0L);
	M0_UT_ASSERT(result == 0);
	fseek(stdout, 0L, SEEK_SET);

	sprintf(buf, "%d", M0_CONS_FOP_REPLY_OPCODE);
	result = console_cmd("show_fops", "-l", "-f", buf, NULL);
	M0_UT_ASSERT(result == EX_OK);
	sprintf(buf, "%.2d Console Reply",
		     M0_CONS_FOP_REPLY_OPCODE);
	M0_UT_ASSERT(m0_error_mesg_match(stdout, buf));
	result = truncate(out_file, 0L);
	M0_UT_ASSERT(result == 0);
	fseek(stdout, 0L, SEEK_SET);

	result = console_cmd("show_fops", "-l", "-f", 0, NULL);
	M0_UT_ASSERT(result == EX_USAGE);
	M0_UT_ASSERT(m0_error_mesg_match(stderr, usage_msg));
	result = truncate(err_file, 0L);
	M0_UT_ASSERT(result == 0);
	fseek(stderr, 0L, SEEK_SET);

	result = console_cmd("yaml_input", "-i", NULL);
	M0_UT_ASSERT(result == EX_USAGE);
	M0_UT_ASSERT(m0_error_mesg_match(stderr, usage_msg));
	result = truncate(err_file, 0L);
	M0_UT_ASSERT(result == 0);
	fseek(stderr, 0L, SEEK_SET);

	result = console_cmd("yaml_input", "-y", yaml_file, NULL);
	M0_UT_ASSERT(result == EX_USAGE);
	M0_UT_ASSERT(m0_error_mesg_match(stderr, usage_msg));
	result = truncate(err_file, 0L);
	M0_UT_ASSERT(result == 0);
	fseek(stderr, 0L, SEEK_SET);

	/* last UT test for console main */
	result = console_cmd("yaml_input", "-i", "-y", yaml_file, NULL);
	M0_UT_ASSERT(result == EX_NOINPUT);
	M0_UT_ASSERT(m0_error_mesg_match(stderr, "YAML Init failed"));

	file_redirect_fini();
}

struct m0_ut_suite console_ut = {
        .ts_name = "libconsole-ut",
        .ts_init = cons_init,
        .ts_fini = cons_fini,
        .ts_tests = {
		{ "yaml_basic_test", yaml_basic_test },
                { "input_test", input_test },
                { "console_input_test", console_input_test },
                { "output_test", output_test },
                { "yaml_file_test", yaml_file_test },
                { "yaml_parser_test", yaml_parser_test },
                { "yaml_root_get_test", yaml_root_get_test },
                { "yaml_get_value_test", yaml_get_value_test },
                { "conn_basic_test", conn_basic_test },
                { "conn_success_test", conn_success_test },
                { "mesg_send_test", mesg_send_test },
		{ NULL, NULL }
	}
};

/** @} end of console group */
#undef CONSOLE_UT

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
