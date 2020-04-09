/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 26-Apr-2016
 */


/**
 * @addtogroup clovis
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include <stdio.h>                    /* FILE */
#include <uuid/uuid.h>                /* uuid_generate */
#include "lib/assert.h"               /* M0_ASSERT */
#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/vec.h"
#include "lib/trace.h"                /* M0_ERR */
#include "clovis_index.h"
#include "clovis_index_parser.h"
#include "clovis_index_op.h"
#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"

struct clovis_instance {
	char             *ci_laddr;
	char             *ci_ha_addr;
	char             *ci_confd_addr;
	char             *ci_prof;
	struct m0_clovis *ci_instance;
};

struct clovis_ctx {
	struct clovis_params      *cc_params;
	struct m0_clovis_config    cc_conf;
	struct clovis_instance     cc_instance;
	struct m0_clovis_container cc_parent;
};

static struct clovis_ctx cc_ctx;

struct m0_clovis *clovis_instance()
{
	return cc_ctx.cc_instance.ci_instance;
}

static struct m0_fid ifid(uint64_t x, uint64_t y)
{
	return M0_FID_TINIT('x', x, y);
}

static int instance_init(struct clovis_params *params)
{
	int                       rc;
	struct m0_clovis         *instance = NULL;
	struct m0_idx_dix_config  config = { .kc_create_meta = false };

	M0_PRE(params != NULL);
	cc_ctx = (struct clovis_ctx) {
		.cc_params = params,
		.cc_conf = {
			.cc_is_oostore     = true,
			.cc_is_read_verify = false,
			.cc_local_addr     = params->cp_local_addr,
			.cc_ha_addr        = params->cp_ha_addr,
			.cc_profile        = params->cp_prof,
			.cc_process_fid    = params->cp_proc_fid,

			.cc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN,
			.cc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE,

			.cc_idx_service_id   = M0_CLOVIS_IDX_DIX,
			.cc_idx_service_conf = &config
		}
	};
	rc = m0_clovis_init(&instance, &cc_ctx.cc_conf, true);
	if (rc == 0)
		cc_ctx.cc_instance.ci_instance = instance;
	return M0_RC(rc);
}

static void instance_fini(void)
{
	struct m0_clovis *instance;

	instance = clovis_instance();
	m0_clovis_fini(instance, true);
}

static int genf(char *filename, int cnt)
{
	FILE          *f;
	int            i;
	struct m0_fid  fid;

	M0_PRE(filename != NULL);
	M0_PRE(cnt != 0);
	f = fopen(filename, "w");
	if (f == NULL)
		return M0_ERR(-ENOENT);
	for (i = 0; i < cnt; i++) {
		fid = ifid(10, i + 1);
		fprintf(f, FID_F"\n", FID_P(&fid));
	}
	fclose(f);
	return 0;
}

static int genv(char *filename, int cnt, int size)
{
	FILE     *f;
	uint64_t  i;
	uint64_t  j;
	int       len;
	int       val_size;
	uuid_t    uuid;
	char      str_uuid[16];
	char      buf[20];

	M0_PRE(filename != NULL);
	M0_PRE(cnt != 0);

	len = sizeof(str_uuid);
	f = fopen(filename, "w");
	if (f == NULL)
		return M0_ERR(-ENOENT);
	val_size = sprintf(buf, "[0x%x:", size);
	val_size += size * 5; /* len of all "0x%02x," and "0x%02x]" */
	for (i = 0; i < cnt; ++i) {
		uuid_generate_random(uuid);
		uuid_unparse(uuid, str_uuid);
		fprintf(f, "%d ", val_size);
		fprintf(f, "[0x%x:", size);
			m0_console_printf("%lu\n", i);
		for (j = 0; j < len; ++j)
			fprintf(f, "0x%02x,", str_uuid[j]);
		for (j = 0; j < size - len - 1; j++)
			fprintf(f, "0x%02x,", 1);
		fprintf(f, "0x%02x]\n", 1);
	}
	fclose(f);
	return 0;
}

static void log_hex_val(const char *tag, void *buf, int size)
{
	int i;

	m0_console_printf("/%s/ ", tag);
	for (i = 0; i < size; i++)
		m0_console_printf("0x%02x ", *((char *)buf+i));
}
static void log_keys_vals(struct m0_bufvec *keys, struct m0_bufvec *vals)
{
	int i;

	for (i = 0; i < keys->ov_vec.v_nr; i++) {
		m0_console_printf("[%d]:\n", i);
		log_hex_val("KEY", keys->ov_buf[i], keys->ov_vec.v_count[i]);
		m0_console_printf("\n");
		log_hex_val("VAL", vals->ov_buf[i], vals->ov_vec.v_count[i]);
		m0_console_printf("\n");
	}
	m0_console_printf("selected %d records\n",vals->ov_vec.v_nr);
}
static void log_fids(struct m0_fid_arr *fids, struct m0_bufvec *vals)
{
	int i;

	for (i = 0; i < fids->af_count; i++)
		if (vals != NULL)
			m0_console_printf(FID_F" found rc %i\n",
				       FID_P(&fids->af_elems[i]),
				       *(int*)vals->ov_buf[i]);
		else
			m0_console_printf(FID_F"\n", FID_P(&fids->af_elems[i]));
}

static int cmd_exec(struct index_cmd *cmd)
{
	int rc;

	switch (cmd->ic_cmd) {
	case CRT:
		rc = clovis_index_create(&cc_ctx.cc_parent.co_realm,
					 &cmd->ic_fids);
		m0_console_printf("create done, rc: %i\n", rc);
		break;
	case DRP:
		rc = clovis_index_drop(&cc_ctx.cc_parent.co_realm,
				       &cmd->ic_fids);
		m0_console_printf("drop done, rc: %i\n", rc);
		break;
	case LST:
		rc = clovis_index_list(&cc_ctx.cc_parent.co_realm,
				       &cmd->ic_fids.af_elems[0], cmd->ic_cnt,
				       &cmd->ic_keys);
		if (rc == 0) {
			int i;
			struct m0_fid *f;

			for (i = 0; i < cmd->ic_keys.ov_vec.v_nr; i++) {
				f = (struct m0_fid*)cmd->ic_keys.ov_buf[i];
				m0_console_printf(FID_F"\n", FID_P(f));
			}
		}
		m0_console_printf("list done, rc: %i\n", rc);
		break;
	case LKP:
		rc = clovis_index_lookup(&cc_ctx.cc_parent.co_realm,
					 &cmd->ic_fids, &cmd->ic_vals);
		if (rc == 0)
			log_fids(&cmd->ic_fids, &cmd->ic_vals);
		m0_console_printf("lookup done, rc: %i\n", rc);
		break;
	case PUT:
		rc = clovis_index_put(&cc_ctx.cc_parent.co_realm,
				      &cmd->ic_fids, &cmd->ic_keys,
				      &cmd->ic_vals);
		m0_console_printf("put done, rc: %i\n", rc);
		break;
	case DEL:
		rc = clovis_index_del(&cc_ctx.cc_parent.co_realm,
				      &cmd->ic_fids, &cmd->ic_keys);
		m0_console_printf("del done, rc: %i\n", rc);
		break;
	case GET:
		rc = clovis_index_get(&cc_ctx.cc_parent.co_realm,
				      &cmd->ic_fids.af_elems[0], &cmd->ic_keys,
				      &cmd->ic_vals);
		if (rc == 0)
			log_keys_vals(&cmd->ic_keys, &cmd->ic_vals);
		m0_console_printf("get done, rc: %i\n", rc);
		break;
	case NXT:
		rc = clovis_index_next(&cc_ctx.cc_parent.co_realm,
				       &cmd->ic_fids.af_elems[0],
				       &cmd->ic_keys, cmd->ic_cnt,
				       &cmd->ic_vals);
		if (rc == 0)
			log_keys_vals(&cmd->ic_keys, &cmd->ic_vals);
		m0_console_printf("next done, rc: %i\n", rc);
		break;
	case GENF:
		rc = genf(cmd->ic_filename, cmd->ic_cnt);
		break;
	case GENV:
		rc = genv(cmd->ic_filename, cmd->ic_cnt, cmd->ic_len);
		break;
	default:
		rc = M0_ERR(-EINVAL);
		M0_ASSERT(0);
	}
	return rc;
}

static void ctx_init(struct index_ctx *ctx)
{
	M0_SET0(ctx);
}

static void ctx_fini(struct index_ctx *ctx)
{
	int i;

	for (i = 0; i < ctx->ictx_nr; ++i) {
		m0_free(ctx->ictx_cmd[i].ic_fids.af_elems);
		m0_bufvec_free(&ctx->ictx_cmd[i].ic_keys);
		m0_bufvec_free(&ctx->ictx_cmd[i].ic_vals);
	}
}

int index_execute(int argc, char **argv)
{
	struct index_ctx ctx;
	int              rc;
	int              i;

	ctx_init(&ctx);
	rc = index_parser_args_process(&ctx, argc, argv);
	if (rc == 0) {
		for (i = 0; i < ctx.ictx_nr && rc == 0; i++)
			rc = cmd_exec(&ctx.ictx_cmd[i]);
	}
	ctx_fini(&ctx);
	return M0_RC(rc);
}

int index_init(struct clovis_params *params)
{
	int rc;

	M0_SET0(&cc_ctx);
	rc = instance_init(params);
	if (rc == 0) {
		m0_clovis_container_init(&cc_ctx.cc_parent,
					 NULL, &M0_CLOVIS_UBER_REALM,
					 clovis_instance());
		rc = cc_ctx.cc_parent.co_realm.re_entity.en_sm.sm_rc;
	}
	return M0_RC(rc);
}

void index_fini(void)
{
	instance_fini();
}

void index_usage(void)
{
	m0_console_printf(
		"\n\tINDEX subsystem\n"
		"\t     'index [commands]'- name of subsystem\n"
		"\t     Commands:\n");
	index_parser_print_command_help();
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of clovis group */

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
