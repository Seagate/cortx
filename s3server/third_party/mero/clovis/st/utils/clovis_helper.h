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
 * Original author:  Rajanikant Chirmade <rajanikant.chirmade@seagate.com>
 * Original creation date: 25-Sept-2018
 */
#pragma once

#ifndef __MERO_CLOVIS_ST_UTILS_HELPER_H__
#define __MERO_CLOVIS_ST_UTILS_HELPER_H__

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>

#include "conf/obj.h"
#include "fid/fid.h"
#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"

struct clovis_cc_io_args {
	struct m0_clovis_container *cia_clovis_container;
	struct m0_uint128           cia_id;
	uint32_t                    cia_block_count;
	uint32_t                    cia_block_size;
	char                      **cia_files;
	int                         cia_index;
};

struct clovis_utility_param {
	struct m0_uint128 cup_id;
	uint64_t          cup_block_size;
	uint64_t          cup_block_count;
	int               cup_n_obj;
	bool              cup_update_mode;
	bool              cup_take_locks;
	uint64_t          cup_trunc_len;
	char             *cup_file;
};

struct clovis_copy_mt_args {
	struct clovis_utility_param *cma_utility;
	struct m0_uint128           *cma_ids;
	struct m0_mutex              cma_mutex;
	int                         *cma_rc;
	int                          cma_index;
};

struct clovis_obj_lock_ops {
	int (*olo_lock_init)(struct m0_clovis_obj *obj,
			     const struct m0_uint128 *group);

	void (*olo_lock_fini)(struct m0_clovis_obj *obj);

	int (*olo_lock_get)(struct m0_clovis_obj *obj,
			    struct m0_clovis_rm_lock_req *req,
			    const struct m0_uint128 *group,
			    struct m0_clink *clink);

	int (*olo_lock_get_sync)(struct m0_clovis_obj *obj,
				 struct m0_clovis_rm_lock_req *req,
				 const struct m0_uint128 *group);

	void (*olo_lock_put)(struct m0_clovis_rm_lock_req *req);
};

int clovis_init(struct m0_clovis_config    *config,
	        struct m0_clovis_container *clovis_container,
	        struct m0_clovis          **clovis_instance);

void clovis_fini(struct m0_clovis *clovis_instance);

int clovis_touch(struct m0_clovis_container *clovis_container,
		 struct m0_uint128 id, bool take_locks);

int clovis_write(struct m0_clovis_container *clovis_container,
		 char *src, struct m0_uint128 id, uint32_t block_size,
		 uint32_t block_count, bool update_mode, bool take_locks);

int clovis_read(struct m0_clovis_container *clovis_container,
		struct m0_uint128 id, char *dest,
		uint32_t block_size, uint32_t block_count, bool take_locks);

int clovis_truncate(struct m0_clovis_container *clovis_container,
		    struct m0_uint128 id, uint32_t block_size,
		    uint32_t trunc_count, uint32_t trunc_len, bool take_locks);

int clovis_unlink(struct m0_clovis_container *clovis_container,
		  struct m0_uint128 id, bool take_locks);
int clovis_write_cc(struct m0_clovis_container *clovis_container,
		    char **src, struct m0_uint128 id, int *index,
		    uint32_t block_size, uint32_t block_count);

int clovis_read_cc(struct m0_clovis_container *clovis_container,
		   struct m0_uint128 id, char **dest, int *index,
		   uint32_t block_size, uint32_t block_count);


int clovis_obj_id_sscanf(char *idstr, struct m0_uint128 *obj_id);

int clovis_utility_args_init(int argc, char **argv,
			     struct clovis_utility_param *params,
			     struct m0_idx_dix_config *dix_conf,
			     struct m0_clovis_config *clovis_conf,
			     void (*utility_usage) (FILE*, char*));
#endif /* __MERO_CLOVIS_ST_UTILS_HELPER_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
