/* -*- C -*- */
/*
 * COPYRIGHT 2019 XYRATEX TECHNOLOGY LIMITED
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
 * Original author:  Abhishek Saha <abhishek.saha@seagate.com>
 * Original creation date: 17-Mar-2019
 */
#include <getopt.h>

#include "lib/memory.h"    /* m0_free() */
#include "lib/trace.h"
#include "clovis/clovis_internal.h"
#include "clovis/st/utils/clovis_helper.h"

struct clovis_grp_lock_test_args {
	struct m0_clovis_container *glt_clovis_container;
	struct m0_uint128           glt_id;
	struct m0_uint128           glt_rm_group1;
	struct m0_uint128           glt_rm_group2;
	int                         glt_released;
	int                         glt_acquired;
	int                         glt_waiting;
	int                         glt_index;
	int                         glt_total;
	struct m0_mutex             glt_cnt_lock;
	struct m0_mutex             glt_chanp_lock;
	struct m0_chan              glt_chan_parent;
	struct m0_mutex             glt_chana_lock;
	struct m0_chan              glt_chan_acqrd;
};

static struct m0_clovis               *clovis_instance = NULL;
static struct m0_clovis_container      clovis_container;
static struct m0_clovis_config         clovis_conf;
static struct m0_idx_dix_config        dix_conf;
static struct clovis_cc_io_args        writer_args;
static struct clovis_cc_io_args        reader_args;
static struct clovis_grp_lock_test_args  grp_lock_test_args;

extern struct m0_addb_ctx m0_clovis_addb_ctx;

static void clovis_grp_ref_chk(struct m0_clovis_container *clovis_container,
			       struct m0_uint128 id, int *index, int total,
			       struct m0_uint128 *grp, int *acqrd, int *watin,
			       struct m0_mutex *lock, int *relsd,
			       struct m0_chan *chan_parent,
			       struct m0_chan *chan_acqrd)
{
	int                           rc;
	int                           i;
	int64_t                       ref_count;
	struct m0_clovis_obj          obj;
	struct m0_clovis             *clovis_instance;
	struct m0_clink               cl_for_lock;
	struct m0_clink               cl_for_wait;
	struct m0_clovis_rm_lock_req  req;
	struct m0_clovis_rm_lock_ctx *ctx;

	M0_SET0(&obj);
	clovis_instance = clovis_container->co_realm.re_instance;
	m0_clovis_obj_init(&obj, &clovis_container->co_realm, &id,
			   m0_clovis_layout_id(clovis_instance));
	rc = m0_clovis_obj_lock_init(&obj, grp);
	if (rc != 0) {
		goto init_error;
	}

	m0_clink_init(&cl_for_lock, NULL);
	cl_for_lock.cl_is_oneshot = true;

	m0_clink_init(&cl_for_wait, NULL);
	cl_for_wait.cl_is_oneshot = true;
	m0_clink_add_lock(chan_acqrd, &cl_for_wait);

	m0_mutex_lock(lock);
	i = --(*index);
	m0_mutex_unlock(lock);

	m0_clovis_obj_lock_get(&obj, &req, grp, &cl_for_lock);

	m0_mutex_lock(lock);
	++(*watin);
	if (i == 0) {
		M0_ASSERT(*watin == total/2 && *acqrd == *watin);
		m0_mutex_unlock(lock);
		ctx = m0_cookie_of(&obj.ob_cookie, struct m0_clovis_rm_lock_ctx,
				   rmc_gen);
		ref_count = m0_ref_read(&ctx->rmc_ref);
		M0_ASSERT(ref_count == total);
		m0_chan_broadcast_lock(chan_acqrd);
	} else
		m0_mutex_unlock(lock);

	m0_chan_wait(&cl_for_lock);

	m0_mutex_lock(lock);
	M0_ASSERT(ergo(*acqrd == total/2, *relsd == total/2));
	--(*watin);
	++(*acqrd);
	if (*acqrd == total/2) {
		M0_ASSERT(*watin == 0);
		m0_chan_broadcast_lock(chan_parent);
	}
	m0_mutex_unlock(lock);

	m0_chan_wait(&cl_for_wait);
	m0_clink_fini(&cl_for_wait);

	m0_mutex_lock(lock);
	m0_clovis_obj_lock_put(&req);
	++(*relsd);
	m0_mutex_unlock(lock);

	m0_clovis_obj_lock_fini(&obj);
init_error:
	m0_clovis_entity_fini(&obj.ob_entity);
}

static void clovis_grp1_chk_launch(struct clovis_grp_lock_test_args *args)
{
	clovis_grp_ref_chk(args->glt_clovis_container, args->glt_id,
			  &args->glt_index, args->glt_total,
			  &args->glt_rm_group1, &args->glt_acquired,
			  &args->glt_waiting, &args->glt_cnt_lock,
			  &args->glt_released, &args->glt_chan_parent,
			  &args->glt_chan_acqrd);
}

static void clovis_grp2_chk_launch(struct clovis_grp_lock_test_args *args)
{
	clovis_grp_ref_chk(args->glt_clovis_container, args->glt_id,
			  &args->glt_index, args->glt_total,
			  &args->glt_rm_group2, &args->glt_acquired,
			  &args->glt_waiting, &args->glt_cnt_lock,
			  &args->glt_released, &args->glt_chan_parent,
			  &args->glt_chan_acqrd);
}

static void clovis_group_ref_check(struct clovis_grp_lock_test_args *args,
				   struct m0_thread *group1_t,
				   struct m0_thread *group2_t, int numb)
{
	int             i;
	struct m0_clink cl_for_wait;

	m0_clink_init(&cl_for_wait, NULL);
	cl_for_wait.cl_is_oneshot = true;
	m0_clink_add_lock(&args->glt_chan_parent, &cl_for_wait);

	for (i = 0; i < numb; ++i) {
		M0_THREAD_INIT(&group1_t[i], struct clovis_grp_lock_test_args *,
				NULL, &clovis_grp1_chk_launch, args,
			       "Group1: %d", i);
	}

	m0_chan_wait(&cl_for_wait);
	m0_clink_fini(&cl_for_wait);

	for (i = 0; i < numb; ++i) {
		M0_THREAD_INIT(&group2_t[i], struct clovis_grp_lock_test_args *,
				NULL, &clovis_grp2_chk_launch, args,
			       "Group2: %d", i);
	}

	for (i = 0; i < numb; ++i) {
		m0_thread_join(&group1_t[i]);
		m0_thread_join(&group2_t[i]);
	}
}

static void clovis_writer_thread_launch(struct clovis_cc_io_args *args)
{
	clovis_write_cc(args->cia_clovis_container, args->cia_files,
			args->cia_id, &args->cia_index, args->cia_block_size,
			args->cia_block_count);
}

static void clovis_reader_thread_launch(struct clovis_cc_io_args *args)
{
	clovis_read_cc(args->cia_clovis_container, args->cia_id,
		       args->cia_files, &args->cia_index, args->cia_block_size,
		       args->cia_block_count);
}

static void clovis_mt_io(struct m0_thread *writer_t,
			 struct clovis_cc_io_args writer_args,
			 struct m0_thread *reader_t,
			 struct clovis_cc_io_args reader_args,
			 int writer_numb, int reader_numb)
{
	int i;

	for (i = 0; i < writer_numb; ++i) {
		M0_THREAD_INIT(&writer_t[i], struct clovis_cc_io_args *, NULL,
			       &clovis_writer_thread_launch, &writer_args,
			       "Writer: %d", i);
	}

	for (i = 0; i < reader_numb; ++i) {
		M0_THREAD_INIT(&reader_t[i], struct clovis_cc_io_args *, NULL,
			       &clovis_reader_thread_launch, &reader_args,
			       "Reader: %d", i);
	}

	for (i = 0; i < writer_numb; ++i) {
		m0_thread_join(&writer_t[i]);
	}

	for (i = 0; i < reader_numb; ++i) {
		m0_thread_join(&reader_t[i]);
	}
}

static void usage(FILE *file, char *prog_name)
{
	fprintf(file, "Usage: %s [OPTION]... SOURCE... DESTINATION...\n"
"Launch multithreaded concurrent Read/Write\n"
"\n"
"Mandatory arguments to long options are mandatory for short options too.\n"
"  -l, --local            ADDR      local endpoint address\n"
"  -H, --ha               ADDR      HA endpoint address\n"
"  -p, --profile          FID       profile FID\n"
"  -P, --process          FID       process FID\n"
"  -o, --object           FID       ID of the mero object\n"
"  -W, --writer_numb      INT       number of writer threads\n"
"  -R, --reader_numb      INT       number of reader threads\n"
"  -s, --block-size       INT       block size multiple of 4k in bytes or " \
				   "with suffix b/k/m/g Ex: 1k=1024, " \
				   "1m=1024*1024\n"
"  -c, --block-count      INT       number of blocks to copy, can give with " \
				   "suffix b/k/m/g/K/M/G. Ex: 1k=1024, " \
				   "1m=1024*1024, 1K=1000 1M=1000*1000\n"
"  -r, --read-verify                verify parity after reading the data\n"
"  -h, --help                       shows this help text and exit\n"
, prog_name);
}

int main(int argc, char **argv)
{
	int                rc;
	struct m0_uint128  id = M0_CLOVIS_ID_APP;
	struct m0_thread  *writer_t;
	struct m0_thread  *reader_t;
	struct m0_thread  *group1_t;
	struct m0_thread  *group2_t;
	char             **dest_fnames = NULL;
	char             **src_fnames = NULL;
	uint32_t           block_size = 0;
	uint32_t           block_count = 0;
	int                c;
	int                i;
	int                option_index = 0;
	int                writer_numb = 0;
	int                reader_numb = 0;

	static struct option l_opts[] = {
				{"local",        required_argument, NULL, 'l'},
				{"ha",           required_argument, NULL, 'H'},
				{"profile",      required_argument, NULL, 'p'},
				{"process",      required_argument, NULL, 'P'},
				{"object",       required_argument, NULL, 'o'},
				{"writer-numb",  required_argument, NULL, 'W'},
				{"reader-numb",  required_argument, NULL, 'R'},
				{"block-size",   required_argument, NULL, 's'},
				{"block-count",  required_argument, NULL, 'c'},
				{"read-verify",  no_argument,       NULL, 'r'},
				{"help",         no_argument,       NULL, 'h'},
				{0,              0,                 0,     0 }};

	while ((c = getopt_long(argc, argv, ":l:H:p:P:o:W:R:s:c:rh", l_opts,
		       &option_index)) != -1) {
		switch (c) {
			case 'l': clovis_conf.cc_local_addr = optarg;
				  continue;
			case 'H': clovis_conf.cc_ha_addr = optarg;
				  continue;
			case 'p': clovis_conf.cc_profile = optarg;
				  continue;
			case 'P': clovis_conf.cc_process_fid = optarg;
				  continue;
			case 'o': id.u_lo = atoi(optarg);
				  continue;
			case 'W': writer_numb = atoi(optarg);
				  continue;
			case 'R': reader_numb  = atoi(optarg);
				  continue;
			case 's': block_size = atoi(optarg);
				  continue;
			case 'c': block_count = atoi(optarg);
				  continue;
			case 'r': clovis_conf.cc_is_read_verify = true;
				  continue;
			case 'h': usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			case '?': fprintf(stderr, "Unsupported option '%c'\n",
					  optopt);
				  usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			case ':': fprintf(stderr, "No argument given for '%c'\n",
				          optopt);
				  usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			default:  fprintf(stderr, "Unsupported option '%c'\n", c);
		}
	}

	clovis_conf.cc_is_oostore            = true;
	clovis_conf.cc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	clovis_conf.cc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	clovis_conf.cc_idx_service_conf      = &dix_conf;
	dix_conf.kc_create_meta              = false;
	clovis_conf.cc_idx_service_id        = M0_CLOVIS_IDX_DIX;

	rc = clovis_init(&clovis_conf, &clovis_container,
			 &clovis_instance);
	if (rc < 0) {
		fprintf(stderr, "clovis_init failed! rc = %d\n", rc);
		exit(EXIT_FAILURE);
	}

	M0_ALLOC_ARR(src_fnames, writer_numb);
	M0_ALLOC_ARR(dest_fnames, reader_numb);
	M0_ALLOC_ARR(writer_t, writer_numb);
	M0_ALLOC_ARR(reader_t, reader_numb);
	M0_ALLOC_ARR(group1_t, writer_numb);
	M0_ALLOC_ARR(group2_t, writer_numb);

	for (i = 0; i < writer_numb; ++i, ++optind) {
		src_fnames[i] = strdup(argv[optind]);
	}

	for (i = 0; i < reader_numb; ++i, ++optind) {
		dest_fnames[i] = strdup(argv[optind]);
	}

	writer_args.cia_clovis_container = &clovis_container;
	writer_args.cia_id               = id;
	writer_args.cia_block_count      = block_count;
	writer_args.cia_block_size       = block_size;
	writer_args.cia_files            = src_fnames;
	writer_args.cia_index            = 0;

	reader_args.cia_clovis_container = &clovis_container;
	reader_args.cia_id               = id;
	reader_args.cia_block_count      = block_count;
	reader_args.cia_block_size       = block_size;
	reader_args.cia_files            = dest_fnames;
	reader_args.cia_index            = 0;

	clovis_mt_io(writer_t, writer_args, reader_t, reader_args,
		     writer_numb, reader_numb);

	grp_lock_test_args.glt_clovis_container = &clovis_container;
	grp_lock_test_args.glt_id               = id;
	grp_lock_test_args.glt_rm_group1        = M0_UINT128(0, 1);
	grp_lock_test_args.glt_rm_group2        = M0_UINT128(0, 2);
	grp_lock_test_args.glt_total            = 2 * writer_numb;
	grp_lock_test_args.glt_index            = grp_lock_test_args.glt_total;
	grp_lock_test_args.glt_acquired         = 0;
	grp_lock_test_args.glt_waiting          = 0;
	grp_lock_test_args.glt_released         = 0;
	m0_mutex_init(&grp_lock_test_args.glt_cnt_lock);
	m0_mutex_init(&grp_lock_test_args.glt_chanp_lock);
	m0_chan_init(&grp_lock_test_args.glt_chan_parent,
		     &grp_lock_test_args.glt_chanp_lock);
	m0_mutex_init(&grp_lock_test_args.glt_chana_lock);
	m0_chan_init(&grp_lock_test_args.glt_chan_acqrd,
		     &grp_lock_test_args.glt_chana_lock);

	fprintf(stderr, "Checking ref count and group lock\n");
	clovis_group_ref_check(&grp_lock_test_args, group1_t, group2_t,
				writer_numb);

	clovis_fini(clovis_instance);

	m0_chan_fini_lock(&grp_lock_test_args.glt_chan_acqrd);
	m0_mutex_fini(&grp_lock_test_args.glt_chana_lock);
	m0_chan_fini_lock(&grp_lock_test_args.glt_chan_parent);
	m0_mutex_fini(&grp_lock_test_args.glt_chanp_lock);
	m0_mutex_fini(&grp_lock_test_args.glt_cnt_lock);

	for (i = 0; i < writer_numb; ++i) {
		m0_free(src_fnames[i]);
		m0_thread_fini(&writer_t[i]);
		m0_thread_fini(&group1_t[i]);
		m0_thread_fini(&group2_t[i]);
	}

	for (i = 0; i < reader_numb; ++i) {
		m0_free(dest_fnames[i]);
		m0_thread_fini(&reader_t[i]);
	}

	m0_free(group1_t);
	m0_free(group2_t);
	m0_free(writer_t);
	m0_free(reader_t);
	m0_free(src_fnames);
	m0_free(dest_fnames);
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
