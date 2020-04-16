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

#pragma once

#ifndef __MERO_CLOVIS_M0CLOVIS_CLOVIS_INDEX_H__
#define __MERO_CLOVIS_M0CLOVIS_CLOVIS_INDEX_H__
#include "lib/vec.h"
#include "fid/fid.h"
#include "clovis_common.h"
/**
 * @defgroup clovis
 *
 * @{
 */

enum {
	CRT,  /* Create index.      */
	DRP,  /* Drop index.        */
	LST,  /* List index.        */
	LKP,  /* Lookup index.      */
	PUT,  /* Put record.        */
	DEL,  /* Delete record.     */
	GET,  /* Get record.        */
	NXT,  /* Next record.       */
	GENF, /* Generate FID-file. */
	GENV  /* Generate VAL-file. */
};

enum {
	INDEX_CMD_COUNT = 10,
	MAX_VAL_SIZE    = 500
};

struct index_cmd {
	int               ic_cmd;
	struct m0_fid_arr ic_fids;
	struct m0_bufvec  ic_keys;
	struct m0_bufvec  ic_vals;
	int               ic_cnt;
	int               ic_len;
	char             *ic_filename;
};

struct index_ctx
{
	struct index_cmd   ictx_cmd[INDEX_CMD_COUNT];
	int                ictx_nr;
};

int  index_execute(int argc, char** argv);
int  index_init(struct clovis_params *params);
void index_fini(void);
void index_usage(void);

/** @} end of clovis group */
#endif /* __MERO_CLOVIS_M0CLOVIS_CLOVIS_INDEX_H__ */

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
