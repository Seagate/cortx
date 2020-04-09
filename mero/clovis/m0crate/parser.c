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
 * Original author: Ivan Alekhin <ivan.alekhin@seagate.com>
 * Original creation date: 30-May-2017
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <stdarg.h>

#include "clovis/m0crate/parser.h"
#include "clovis/m0crate/crate_clovis.h"

enum cr_kvs_key_len_max {
	MAX_KEY_LEN = 128
};

extern struct workload_type_ops ops;
extern struct workload_type_ops index_ops;
extern struct crate_clovis_conf *conf;

enum config_key_val {
	INVALID_OPT = -1,
	LOCAL_ADDR,
	HA_ADDR,
	CLOVIS_PROF,
	LAYOUT_ID,
	IS_OOSTORE,
	IS_READ_VERIFY,
	MAX_QUEUE_LEN,
	MAX_RPC_MSG,
	PROCESS_FID,
	IDX_SERVICE_ID,
	CASS_EP,
	CASS_KEYSPACE,
	CASS_COL_FAMILY,
	WORKLOAD,
	WORKLOAD_TYPE,
	SEED,
	NR_THREADS,
	NUM_OPS,
	NR_OBJS,
	NUM_IDX,
	NUM_KVP,
	RECORD_SIZE,
	MAX_RSIZE,
	PUT,
	GET,
	NEXT,
	DEL,
	NXRECORDS,
	OP_COUNT,
	EXEC_TIME,
	WARMUP_PUT_CNT,
	WARMUP_DEL_RATIO,
	KEY_PREFIX,
	KEY_ORDER,
	INDEX_FID,
	LOG_LEVEL,
	THREAD_OPS,
	BLOCK_SIZE,
	BLOCKS_PER_OP,
	IOSIZE,
	SOURCE_FILE,
	RAND_IO,
	OPCODE,
	START_OBJ_ID,
	MODE,
	MAX_NR_OPS,
	NR_ROUNDS,
	ADDB_INIT,
};

struct key_lookup_table {
	char *key;
	enum config_key_val   index;
};

struct key_lookup_table lookuptable[] = {
	{"MERO_LOCAL_ADDR", LOCAL_ADDR},
	{"MERO_HA_ADDR", HA_ADDR},
	{"CLOVIS_PROF", CLOVIS_PROF},
	{"LAYOUT_ID", LAYOUT_ID},
	{"IS_OOSTORE", IS_OOSTORE},
	{"IS_READ_VERIFY", IS_READ_VERIFY},
	{"CLOVIS_TM_RECV_QUEUE_MIN_LEN", MAX_QUEUE_LEN},
	{"CLOVIS_MAX_RPC_MSG_SIZE", MAX_RPC_MSG},
	{"CLOVIS_PROCESS_FID", PROCESS_FID},
	{"CLOVIS_IDX_SERVICE_ID", IDX_SERVICE_ID},
	{"CLOVIS_CASS_CLUSTER_EP", CASS_EP},
	{"CLOVIS_CASS_KEYSPACE", CASS_KEYSPACE},
	{"CLOVIS_CASS_MAX_COL_FAMILY_NUM", CASS_COL_FAMILY},
	{"WORKLOAD", WORKLOAD},
	{"WORKLOAD_TYPE", WORKLOAD_TYPE},
	{"WORKLOAD_SEED", SEED},
	{"NR_THREADS", NR_THREADS},
	{"CLOVIS_OPS", NUM_OPS},
	{"NUM_IDX", NUM_IDX},
	{"NUM_KVP", NUM_KVP},
	{"RECORD_SIZE", RECORD_SIZE},
	{"MAX_RSIZE", MAX_RSIZE},
	{"GET", GET},
	{"PUT", PUT},
	{"NEXT", NEXT},
	{"DEL", DEL},
	{"NXRECORDS", NXRECORDS},
	{"OP_COUNT", OP_COUNT},
	{"EXEC_TIME", EXEC_TIME},
	{"WARMUP_PUT_CNT", WARMUP_PUT_CNT},
	{"WARMUP_DEL_RATIO", WARMUP_DEL_RATIO},
	{"KEY_PREFIX", KEY_PREFIX},
	{"KEY_ORDER", KEY_ORDER},
	{"INDEX_FID", INDEX_FID},
	{"LOG_LEVEL", LOG_LEVEL},
	{"NR_OBJS", NR_OBJS},
	{"NR_THREADS", NR_THREADS},
	{"THREAD_OPS", THREAD_OPS},
	{"BLOCK_SIZE", BLOCK_SIZE},
	{"BLOCKS_PER_OP", BLOCKS_PER_OP},
	{"CLOVIS_IOSIZE", IOSIZE},
	{"SOURCE_FILE", SOURCE_FILE},
	{"RAND_IO", RAND_IO},
	{"OPCODE", OPCODE},
	{"STARTING_OBJ_ID", START_OBJ_ID},
	{"MODE", MODE},
	{"MAX_NR_OPS", MAX_NR_OPS},
	{"NR_ROUNDS", NR_ROUNDS},
	{"ADDB_INIT", ADDB_INIT},
};

#define NKEYS (sizeof(lookuptable)/sizeof(struct key_lookup_table))

enum config_key_val get_index_from_key(char *key)
{
	int   i;
	char *s1;

	for(i = 0; i < NKEYS; i++) {
		s1 = strstr(key, lookuptable[i].key);
		if (s1 != NULL && !strcmp(s1, lookuptable[i].key)) {
			return lookuptable[i].index;
		}
	}
	return INVALID_OPT;
}

const char *get_key_from_index(const enum config_key_val key)
{
	int   i;
	const char *result = NULL;

	for(i = 0; i < NKEYS; i++) {
		if (key == lookuptable[i].index) {
			result = lookuptable[i].key;
			break;
		}
	}

	return result;
}

void parser_emit_error(const char *fmt, ...)
	__attribute__((no_exit))
	__attribute__((format(printf, 1,2)));

void parser_emit_error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	exit(EXIT_FAILURE);
}

static int parse_int_with_units(const char *value, enum config_key_val tag)
{
	unsigned long long v = getnum(value, get_key_from_index(tag));

	if (v > INT_MAX)
		parser_emit_error("Value overflow detected (value=%s, tag=%s", value, get_key_from_index(tag));

	return v;
}

static int parse_int(const char *value, enum config_key_val tag)
{
	char *endptr;
	long val = 0;

	val = strtol(value, &endptr, 10);

	if ((val == LONG_MAX || val == LONG_MIN) && errno == ERANGE) {
		parser_emit_error("Invalid int value (value '%s'='%s', err: %s).\n", get_key_from_index(tag), value, strerror(errno));
	}

	if (endptr == value) {
		parser_emit_error("Value '%s' is not a number\n", value);
	}

	return val;
}

#define SIZEOF_CWIDX sizeof(struct clovis_workload_index)
#define SIZEOF_CWIO sizeof(struct clovis_workload_io)

#define workload_index(t) (t->u.cw_clovis_index)
#define workload_io(t) (t->u.cw_clovis_io)

int copy_value(struct workload *load, int max_workload, int *index,
		char *key, char *value)
{
	struct workload              *w = NULL;
	struct m0_fid                *obj_fid;
	struct clovis_workload_io    *cw;
	struct clovis_workload_index *ciw;
	int                           value_len = strlen(value);

	if (!strcmp(value, "MERO_CONFIG")) {
		if (conf != NULL) {
			cr_log(CLL_ERROR, "YAML file error. More than one config sections");
			return -EINVAL;
		}

		conf = m0_alloc(sizeof(struct crate_clovis_conf));
		if (conf == NULL)
			return -ENOMEM;
	}

	switch(get_index_from_key(key)) {
		case LOCAL_ADDR:
			conf->clovis_local_addr = m0_alloc(value_len + 1);
			if (conf->clovis_local_addr == NULL)
				return -ENOMEM;
			strcpy(conf->clovis_local_addr, value);
			break;
		case HA_ADDR:
			conf->clovis_ha_addr = m0_alloc(value_len + 1);
			if (conf->clovis_ha_addr == NULL)
				return -ENOMEM;
			strcpy(conf->clovis_ha_addr, value);
			break;
		case CLOVIS_PROF:
			conf->clovis_prof = m0_alloc(value_len + 1);
			if (conf->clovis_prof == NULL)
				return -ENOMEM;
			strcpy(conf->clovis_prof, value);
			break;
		case MAX_QUEUE_LEN:
			conf->tm_recv_queue_min_len = atoi(value);
			break;
		case MAX_RPC_MSG:
			conf->max_rpc_msg_size = atoi(value);
			break;
		case PROCESS_FID:
			conf->clovis_process_fid = m0_alloc(value_len + 1);
			if (conf->clovis_process_fid == NULL)
				return -ENOMEM;
			strcpy(conf->clovis_process_fid, value);
			break;
		case LAYOUT_ID:
			conf->layout_id = atoi(value);
			break;
		case IS_OOSTORE:
			conf->is_oostrore = atoi(value);
			break;
		case IS_READ_VERIFY:
			conf->is_read_verify = atoi(value);
			break;
		case IDX_SERVICE_ID:
			conf->index_service_id = atoi(value);
			break;
		case CASS_EP:
			conf->cass_cluster_ep = m0_alloc(value_len + 1);
			if (conf->cass_cluster_ep == NULL)
				return -ENOMEM;
			strcpy(conf->cass_cluster_ep, value);
			break;
		case CASS_KEYSPACE:
			conf->cass_keyspace = m0_alloc(value_len + 1);
			if ( conf->cass_keyspace == NULL)
				return -ENOMEM;

			strcpy(conf->cass_keyspace, value);
			break;
		case CASS_COL_FAMILY:
			conf->col_family = atoi(value);
			break;
		case WORKLOAD:
			break;
		case LOG_LEVEL:
			conf->log_level = parse_int(value, LOG_LEVEL);
			break;
		case WORKLOAD_TYPE:
			(*index)++;
			w = &load[*index];
			if (atoi(value) == INDEX) {
				w->cw_type = CWT_CLOVIS_INDEX;
				w->u.cw_clovis_index = m0_alloc(SIZEOF_CWIDX);
				if (w->u.cw_clovis_io == NULL)
					return -ENOMEM;
			} else {
				w->cw_type = CWT_CLOVIS_IO;
				w->u.cw_clovis_io = m0_alloc(SIZEOF_CWIO);
				if (w->u.cw_clovis_io == NULL)
					return -ENOMEM;
			}
                        return workload_init(w, w->cw_type);
		case SEED:
			w = &load[*index];
			if (w->cw_type == CWT_CLOVIS_IO) {
				cw = workload_io(w);
				if (strcmp(value, "tstamp"))
					w->cw_rstate = atoi(value);
			} else {
				ciw = workload_index(w);
				if (!strcmp(value, "tstamp"))
					ciw->seed = time(NULL);
				else
					ciw->seed = parse_int(value, SEED);
			}
			break;
		case NR_THREADS:
			w = &load[*index];
			w->cw_nr_thread = atoi(value);
			break;
		case NUM_OPS:
			w = &load[*index];
			w->cw_ops = atoi(value);
			break;
		case NUM_IDX:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->num_index = atoi(value);
			break;
		case NUM_KVP:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->num_kvs = atoi(value);
			break;
		case RECORD_SIZE:
			w = &load[*index];
			ciw = workload_index(w);
			if (!strcmp(value, "random"))
				ciw->record_size = -1;
			else
				ciw->record_size = parse_int_with_units(value, RECORD_SIZE);
			break;
		case MAX_RSIZE:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->max_record_size = parse_int_with_units(value, MAX_RSIZE);
			break;
		case PUT:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->opcode_prcnt[CRATE_OP_PUT] = parse_int(value, PUT);
			break;
		case GET:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->opcode_prcnt[CRATE_OP_GET] = parse_int(value, GET);
			break;
		case NEXT:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->opcode_prcnt[CRATE_OP_NEXT] = parse_int(value, NEXT);
			break;
		case DEL:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->opcode_prcnt[CRATE_OP_DEL] = parse_int(value, DEL);
			break;
		case NXRECORDS:
			w = &load[*index];
			ciw = workload_index(w);
			if (!strcmp(value, "default"))
				ciw->next_records = -1;
			else
				ciw->next_records = parse_int_with_units(value, NXRECORDS);
			break;
		case OP_COUNT:
			w = &load[*index];
			ciw = workload_index(w);
			if (!strcmp(value, "unlimited"))
				ciw->op_count = -1;
			else
				ciw->op_count = parse_int_with_units(value, OP_COUNT);
			break;
		case EXEC_TIME:
			w = &load[*index];
			if (w->cw_type == CWT_CLOVIS_INDEX) {
				ciw = workload_index(w);
				if (!strcmp(value, "unlimited"))
					ciw->exec_time = -1;
				else
					ciw->exec_time = parse_int(value,
							           EXEC_TIME);
			} else {
				cw = workload_io(w);
				if (!strcmp(value, "unlimited"))
					cw->cwi_execution_time = M0_TIME_NEVER;
				else
					cw->cwi_execution_time = parse_int(value,
							           EXEC_TIME);
			}
			break;
		case KEY_PREFIX:
			w = &load[*index];
			ciw = workload_index(w);
			if (!strcmp(value, "random"))
				ciw->key_prefix.f_container = -1;
			else
			    ciw->key_prefix.f_container = parse_int(value, KEY_PREFIX);
			break;
		case KEY_ORDER:
			w = &load[*index];
			ciw = workload_index(w);
			if (!strcmp(value, "ordered"))
				ciw->keys_ordered = true;
			else if (!strcmp(value, "random"))
				ciw->keys_ordered = false;
			else
				parser_emit_error("Unkown key ordering: '%s'", value);
			break;
		case INDEX_FID:
			w = &load[*index];
			ciw = workload_index(w);
			if (0 != m0_fid_sscanf(value, &ciw->index_fid)) {
				parser_emit_error("Unable to parse fid: %s", value);
			}
			break;
		case WARMUP_PUT_CNT:
			w = &load[*index];
			ciw = workload_index(w);
			if (!strcmp(value, "all"))
				ciw->warmup_put_cnt = -1;
			else
				ciw->warmup_put_cnt = parse_int(value, WARMUP_PUT_CNT);
			break;
		case WARMUP_DEL_RATIO:
			w = &load[*index];
			ciw = workload_index(w);
			ciw->warmup_del_ratio = parse_int(value, WARMUP_DEL_RATIO);
			break;
		case THREAD_OPS:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_share_object = atoi(value);
			break;
		case BLOCK_SIZE:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_bs = getnum(value, "block size");
			break;
		case BLOCKS_PER_OP:
			w  = &load[*index];
			cw = workload_io(w);
			cw->cwi_bcount_per_op = atol(value);
			break;
		case NR_OBJS:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_nr_objs = atoi(value);
			break;
		case MAX_NR_OPS:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_max_nr_ops = atoi(value);
			break;
		case IOSIZE:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_io_size = getnum(value, "io size");
			break;
		case SOURCE_FILE:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_filename = m0_alloc(value_len + 1);
			strcpy(cw->cwi_filename, value);
			break;
		case RAND_IO:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_random_io = atoi(value);
			break;
		case OPCODE:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_opcode = atoi(value);
			if (conf->layout_id <= 0) {
				cr_log(CLL_ERROR, "LAYOUT_ID is not set\n");
				return -EINVAL;
			}
			cw->cwi_layout_id = conf->layout_id;
			break;
		case START_OBJ_ID:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_start_obj_id = M0_CLOVIS_ID_APP;
			if (strchr(value, ':') == NULL) {
				cw->cwi_start_obj_id.u_lo = atoi(value);
				break;
			}
			obj_fid = (struct m0_fid *)&cw->cwi_start_obj_id;
			m0_fid_sscanf(value, obj_fid);
			break;
		case MODE:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_mode = atoi(value);
			break;
		case NR_ROUNDS:
			w = &load[*index];
			cw = workload_io(w);
			cw->cwi_rounds = atoi(value);
			break;
		case ADDB_INIT:
			conf->is_addb_init = atoi(value);
			break;
		default:
			break;
	}
	return 0;
}

int parse_yaml_file(struct workload *load, int max_workload, int *index,
		    char *config_file)
{
	FILE *fh;
	int   is_key = 0;
	char  key[MAX_KEY_LEN];
	char *scalar_value;
	int  rc;

	yaml_parser_t parser;
	yaml_token_t  token;

	if (!yaml_parser_initialize(&parser)) {
		cr_log(CLL_ERROR, "Failed to initialize parser!\n");
		return -1;
	}

	fh = fopen(config_file, "r");
	if (fh == NULL) {
		cr_log(CLL_ERROR, "Failed to open file!\n");
		return -1;
	}

	yaml_parser_set_input_file(&parser, fh);

	do {
		rc = 0;
		yaml_parser_scan(&parser, &token);
		switch (token.type) {
			case YAML_KEY_TOKEN:
				is_key = 1;
				break;
			case YAML_VALUE_TOKEN:
				is_key = 0;
				break;
			case YAML_SCALAR_TOKEN:
				scalar_value = (char *)token.data.scalar.value;
				if (is_key) {
					strcpy(key, scalar_value);
				} else {
					rc = copy_value(load, max_workload, index,
							key, scalar_value);
				}
				break;
			case YAML_NO_TOKEN:
				rc = -EINVAL;
				break;
			default:
				break;
		}

		if (rc != 0) {
			fclose(fh);
			cr_log(CLL_ERROR, "Failed to parse %s\n", key);
			return rc;
		}

		if (token.type != YAML_STREAM_END_TOKEN)
			yaml_token_delete(&token);

	} while (token.type != YAML_STREAM_END_TOKEN);

	yaml_token_delete(&token);

	yaml_parser_delete(&parser);
	/*fclose(fh);*/
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
