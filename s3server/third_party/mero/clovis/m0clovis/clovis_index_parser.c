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
#include <string.h>
#include <stdio.h>               /* FILE */
#include <stdlib.h>              /* strtol */
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/trace.h"           /* M0_ERR */
#include "lib/buf_xc.h"
#include "xcode/xcode.h"         /* m0_xcode_read */
#include "lib/string.h"          /* m0_fop_str */
#include "lib/user_space/misc.h" /* ARRAY_SIZE */
#include "clovis_index_parser.h"
#include "clovis_common.h"
#include "clovis_index.h"

struct command_descr {
	int         cd_id;
	const char *cd_name;
	const char *cd_help_descr;
};

static const struct command_descr commands[] = {
	{ CRT,  "create", "create FID_PARAM, create index" },
	{ DRP,  "drop",   "drop FID_PARAM, drop existing index"},
	{ LST,  "list",   "list FID NUM, get indicies" },
	{ LKP,  "lookup", "lookup FID_PARAM, lookup index in storage" },
	{ PUT,  "put",    "put FID_PARAM KEY_PARAM VAL_PARAM, put record" },
	{ DEL,  "del",    "del FID_PARAM KEY_PARAM, delete record" },
	{ GET,  "get",    "get FID KEY_PARAM, lookup and returns values by key" },
	{ NXT,  "next",   "next FID KEY CNT, returns records larger than KEY " },
	{ GENF, "genf",   "genf CNT FILE, generate file with several FID" },
	{ GENV, "genv",   "genv CNT SIZE FILE, generate file with several "
			  "KEY_PARAM/VAL_PARAM. Note: SIZE > 16" },
};

static int command_id(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); ++i) {
		if (!strcmp(name, commands[i].cd_name))
			return commands[i].cd_id;
	}
	return M0_ERR(-EINVAL);
}

static int file_lines_count(const char *filename)
{
	FILE *f;
	int   c;
	int   count = 0;

	f = fopen(filename, "r");
	if (f == NULL)
		return M0_ERR(-errno);
	while ((c = fgetc(f)) != EOF) {
		if (c == '\n' )
			count++;
	}
	fclose(f);
	return count;
}

static int fids_load(const char *val, struct m0_fid_arr *fids)
{
	FILE *f;
	int   i;
	int   rc;
	char  buf[255];
	int   fids_nr;

	if (val == NULL)
		return -EINVAL;
	/* Create array for fids. */
	fids_nr = val[0] == '@' ? file_lines_count(&val[1]) : 1;
	if (fids_nr < 0)
		return M0_ERR(fids_nr);
	M0_ALLOC_ARR(fids->af_elems, fids_nr);
	fids->af_count = fids_nr;
	if (fids->af_elems == NULL)
		return M0_ERR(-ENOMEM);
	if (val[0] == '@') {
		/* CmdLine contains FID-FILE value. */
		f = fopen(&val[1], "r");
		if (f == NULL)
			return M0_ERR(-errno);
		rc = 0;
		for (i = 0; i < fids_nr && rc == 0 &&
			    fgets(buf, sizeof buf, f) != NULL;
			    ++i) {
			rc = m0_fid_sscanf(buf, &fids->af_elems[i]);
		}
		fclose(f);
	} else {
		/* CmdLine contains FID value. */
		rc = m0_fid_sscanf(&val[0], &fids->af_elems[0]);
	}
	return rc;
}

static int vals_xcode(const char *value, void *buf, m0_bcount_t *size)
{
	struct m0_buf tmp_buf;
	int           rc;

	tmp_buf.b_addr = buf;
	tmp_buf.b_nob  = 0;
	rc = m0_xcode_read(&M0_XCODE_OBJ(m0_buf_xc, &tmp_buf), value);
	*size= tmp_buf.b_nob;
	return rc;
}

static int item_load(FILE *f, char **item, int *size)
{
	char  buf[100];
	char *next;
	char *target;
	int   pos;

	if (fgets(buf, sizeof buf, f) == NULL)
		return 1;
	/* Get first int value from buffer - it's a size of payload. */
	*size = strtol(buf, &next, 0);
	if (*size == 0)
		return M0_ERR(-EPROTO);
	next++;
	pos = next - &buf[0]  + 1;
	/* Allocate buffer and load whole value from file. */
	*item = target = m0_alloc(*size * 2);
	if (target == NULL)
		return M0_ERR(-ENOMEM);
	if (*size >= sizeof buf) {
		memcpy(target, next, sizeof buf - pos);
		/* Addition file operation for load whole value. */
		if (fgets(target + (sizeof buf - pos),
			  *size - (sizeof buf - pos) + 2, f) == NULL)
			return -1;
	} else {
		memcpy(target, next, *size - pos);
	}
	return 0;
}

static int vals_load(const char *value, struct m0_bufvec *vals)
{
	FILE *f;
	int   vals_nr;
	int   i;
	int   rc;
	int   size;
	char *buf;

	if (value == NULL)
		return M0_ERR(-EINVAL);
	vals_nr = value[0] == '@' ? file_lines_count(&value[1]) : 1;
	if (vals_nr < 0)
		return vals_nr;
	rc = m0_bufvec_empty_alloc(vals, vals_nr);
	if (rc < 0)
		return M0_ERR(rc);
	if (value[0] == '@') {
		/* CmdLine contains VAL-FILE value. */
		f = fopen(&value[1], "r");
		if (f == NULL)
			return M0_ERR(-errno);
		rc = 0;
		buf = NULL;
		for (i = 0; rc == 0 && i < vals_nr &&
			   (rc = item_load(f, &buf, &size)) == 0;
		     ++i) {
			vals->ov_buf[i] = m0_alloc(size);
			rc = vals_xcode(buf, vals->ov_buf[i],
					&vals->ov_vec.v_count[i]);
			m0_free(buf);
		}
		/* Drop rc==1 to zero. */
		rc = (rc == 1) ? 0 : rc;
		fclose(f);
	} else {
		/* CmdLine contains VAL. */
		vals->ov_buf[0] = m0_alloc(MAX_VAL_SIZE);
		rc = vals_xcode(&value[0], vals->ov_buf[0],
				&vals->ov_vec.v_count[0]);
	}

	return rc;
}

static int command_assign(struct index_cmd *cmd, int *argc, char ***argv)
{
	char ***params;
	int     rc;

	params = argv;
	rc = command_id(**params);
	if (rc < 0 )
		return M0_ERR(rc);
	++*params;
	--*argc;
	cmd->ic_cmd = rc;
	rc = 0;
	switch (cmd->ic_cmd) {
	case CRT:
	case DRP:
	case LKP:
		rc = fids_load(**params, &cmd->ic_fids);
		++*params;
		--*argc;
		break;
	case LST:
		/* Check start and cnt params in cmdline. */
		if (**params == NULL)
			rc = M0_ERR(-EINVAL);
		else {
			rc = fids_load(**params, &cmd->ic_fids);
			++*params;
			--*argc;
		}
		if (rc != 0 && **params == NULL)
			rc = M0_ERR(-EINVAL);
		else {
			cmd->ic_cnt = strtol(**params, (char **)(NULL), 10);
			rc = cmd->ic_cnt == 0 ? M0_ERR(-EINVAL) : 0;
		}
		++*params;
		--*argc;
		break;
	case PUT:
		if (*argc < 3)
			return M0_ERR(-EINVAL);
		rc = fids_load(**params, &cmd->ic_fids);
		if (rc < 0)
			return M0_ERR(rc);
		++*params;
		rc = vals_load(**params, &cmd->ic_keys);
		if (rc < 0)
			return M0_ERR(rc);
		++*params;
		rc = vals_load(**params, &cmd->ic_vals);
		if (rc < 0)
			return M0_ERR(rc);
		++*params;
		*argc -= 3;
		break;
	case DEL:
		if (*argc < 2)
			return M0_ERR(-EINVAL);
		rc = fids_load(**params, &cmd->ic_fids);
		++*params;
		if (rc < 0)
			return M0_ERR(rc);
		rc = vals_load(**params, &cmd->ic_keys);
		if (rc < 0)
			return M0_ERR(rc);
		++*params;
		*argc -= 2;
		break;
	case GET:
		if (*argc < 2)
			return M0_ERR(-EINVAL);
		if (**params[0]=='@')
			return M0_ERR(-EINVAL);
		rc = fids_load(**params, &cmd->ic_fids);
		++*params;
		if (rc < 0)
			return M0_ERR(rc);
		rc = vals_load(**params, &cmd->ic_keys);
		if (rc < 0)
			return M0_ERR(rc);
		++*params;
		*argc -= 2;
		break;
	case NXT:
		if (*argc < 3)
			return M0_ERR(-EINVAL);
		if (**params[0]=='@')
			return M0_ERR(-EINVAL);
		rc = fids_load(**params, &cmd->ic_fids);
		++*params;
		--*argc;
		if (rc < 0)
			return M0_ERR(rc);
		if (**params[0]=='@')
			return M0_ERR(-EINVAL);
		rc = vals_load(**params, &cmd->ic_keys);
		if (rc < 0)
			return M0_ERR(rc);
		++*params;
		--*argc;
		cmd->ic_cnt = strtol(**params,
				       (char **)(NULL), 10);
		rc = cmd->ic_cnt == 0 ? M0_ERR(-EINVAL) : 0;
		++*params;
		--*argc;
		break;
	case GENF:
		if (*argc < 2)
			return M0_ERR(-EINVAL);
		cmd->ic_cnt = strtol(**params, (char **)(NULL), 10);
		rc = cmd->ic_cnt == 0 ? M0_ERR(-EINVAL) : 0;
		++*params;
		if (rc < 0)
			return rc;
		cmd->ic_filename = **params;
		++*params;
		*argc -= 2;
		break;
	case GENV:
		if (*argc < 3)
			return M0_ERR(-EINVAL);
		cmd->ic_cnt = strtol(**params, (char **)(NULL), 10);
		rc = cmd->ic_cnt == 0 ? M0_ERR(-EINVAL) : 0;
		++*params;
		if (rc < 0)
			return rc;
		cmd->ic_len = strtol(**params, (char **)(NULL), 10);
		rc = cmd->ic_len == 0 ? M0_ERR(-EINVAL) : 0;
		++*params;
		if (rc < 0)
			return rc;
		cmd->ic_filename = **params;
		++*params;
		*argc -= 3;
		break;
	default:
		M0_IMPOSSIBLE("Wrong command");
	}
	*argv = *params;
	return rc;
}

static bool command_is_valid(struct index_cmd *cmd)
{
	bool rc;

	switch(cmd->ic_cmd) {
	case CRT:
	case DRP:
	case LKP:
		rc = cmd->ic_fids.af_count != 0;
		break;
	case LST:
		/* Do nothing: fids and CNT can be absent. */
		rc = true;
		break;
	case PUT:
		rc = cmd->ic_fids.af_count != 0 &&
		     cmd->ic_keys.ov_vec.v_nr != 0 &&
		     cmd->ic_keys.ov_vec.v_nr == cmd->ic_vals.ov_vec.v_nr;
		break;
	case DEL:
		rc = cmd->ic_fids.af_count != 0 &&
		     cmd->ic_keys.ov_vec.v_nr != 0;
		break;
	case GET:
		rc = cmd->ic_fids.af_count == 1 &&
		     cmd->ic_keys.ov_vec.v_nr != 0;
		break;
	case NXT:
		rc = cmd->ic_fids.af_count == 1 &&
		     cmd->ic_keys.ov_vec.v_nr == 1 &&
		     cmd->ic_cnt != 0;
		break;
	case GENF:
		rc = cmd->ic_filename != NULL && cmd->ic_cnt != 0;
		break;
	case GENV:
		rc = cmd->ic_filename != NULL &&
		     cmd->ic_cnt != 0 &&
		     cmd->ic_len != 0;
		break;
	default:
		M0_IMPOSSIBLE("Wrong command.");
	}
	return rc;
}

int index_parser_args_process(struct index_ctx *ctx, int argc, char **argv)
{
	char **params;
	int    i;
	int    rc;

	if (argc < 2 )
		return M0_ERR(-EINVAL);
	params = &argv[0];
	i = 0;
	do {
		rc = command_assign(&ctx->ictx_cmd[i], &argc, &params);
		if (rc == 0)
			if(!command_is_valid(&ctx->ictx_cmd[i]))
				rc = -EINVAL;
		++i;
	} while (argc != 0 && *params != NULL &&
		 rc == 0 && i < INDEX_CMD_COUNT);
	if (rc == 0)
		ctx->ictx_nr = i;
	else
		m0_console_printf("Invalid params for [%s]\n",
				commands[ctx->ictx_cmd[i-1].ic_cmd].cd_name);
	return rc;
}

void index_parser_print_command_help(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); ++i)
		if (commands[i].cd_help_descr != NULL)
			m0_console_printf("\t        -'%s'\n",
					  commands[i].cd_help_descr);
	m0_console_printf(
		"\tNOTE:\n"
		"\t- FID_PARAM - single FID value or @FIDFILENAME\n"
		"\t- KEY_PARAM - single KEY value or @VFILENAME\n"
		"\t- VAL_PARAM - single VALUE param or @VFILENAME\n"
		"\t- FID - value in m0_fid_sscanf format, "
		"e.g. '<0x780000000000000b:1>', '1:5' and etc\n"
		"\t- KEY or VALUE - value in m0_xcode_read format, e.g. "
		"'[0xa:0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a]'\n"
		"\tExample: \n"
		"\t\t>m0clovis [common args] index genf 10 @fid.txt \n"
		"\t\t>m0clovis [common args] index create \"1:5\" \n"
		"\t\t>m0clovis [common args] index list \"1:5\" 2 \n"
		"\t\t>m0clovis [common args] index genv 10 20 @keys.txt \n"
		"\t\t>m0clovis [common args] index put \"1:5\" @keys.txt "
		"@vals.txt \n"
		"\t\t>m0clovis [common args] index get \"1:5\" @keys.txt \n"
		"\t\t>m0clovis [common args] index next \"1:5\" "
		"'[0x02:0x01,0x02]' 3 \n"
		"\tPossible to supply multiple commands on command line e.g.:\n"
		"\t\t>m0clovis [common args] index create \"1:5\" put \"1:5\""
		" \"[0x02:0x01,0x02]\" \"[0x09:0x01,0x02,0x03,0x04,0x05,0x06,"
		"0x07,0x08,0x09]\"\n"
		"\t\t>m0clovis [common args] index drop \"1:5\" create \"1:5\" "
		"put \"1:5\" @keys.txt @vals.txt\n");
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
