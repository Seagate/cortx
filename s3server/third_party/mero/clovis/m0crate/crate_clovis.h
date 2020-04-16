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

#pragma once

#ifndef __MERO_CLOVIS_M0CRATE_CRATE_CLOVIS_H__
#define __MERO_CLOVIS_M0CRATE_CRATE_CLOVIS_H__

/**
 * @defgroup crate_clovis
 *
 * @{
 */

#include "fid/fid.h"
#include "clovis/clovis.h"
#include "clovis/m0crate/workload.h"
#include "clovis/m0crate/crate_utils.h"

struct crate_clovis_conf {
        /* Clovis parameters */
        bool is_addb_init;
        bool is_oostrore;
        bool is_read_verify;
        char *clovis_local_addr;
        char *clovis_ha_addr;
        char *clovis_prof;
        char *clovis_process_fid;
        int layout_id;
        char *clovis_index_dir;
        int index_service_id;
        char *cass_cluster_ep;
        char *cass_keyspace;
	int tm_recv_queue_min_len;
	int max_rpc_msg_size;
        int col_family;
	int log_level;
};

enum clovis_operation_type {
	INDEX,
	IO
};

enum cr_opcode {
	CRATE_OP_PUT,
	CRATE_OP_GET,
	CRATE_OP_NEXT,
	CRATE_OP_DEL,
	CRATE_OP_TYPES,
	CRATE_OP_NR = CRATE_OP_TYPES,
	CRATE_OP_START = CRATE_OP_PUT,
	CRATE_OP_INVALID = CRATE_OP_TYPES,
};

struct clovis_workload_index {
	struct m0_uint128      *ids;
	int		       *op_status;
	int			num_index;
	int			num_kvs;
	int			mode;
	int			opcode;
	int			record_size;
	int			opcode_prcnt[CRATE_OP_TYPES];
	int			next_records;

	/** Total count for all operaions.
	 * If op_count == -1, then operations count is unlimited.
	 **/
	int			op_count;

	/** Maximum number of seconds to execute test.
	 * exec_time == -1 means "unlimited".
	 **/
	int			exec_time;

	/** Insert 'warmup_put_cnt' records before test. */
	int			warmup_put_cnt;
	/** Delete every 'warmup_del_ratio' records before test. */
	int			warmup_del_ratio;

	struct m0_fid		key_prefix;
	int			keys_count;

	bool			keys_ordered;

	struct m0_fid		index_fid;

	int			max_record_size;
	uint64_t		seed;
};

struct clovis_workload_task {
	int                    task_idx;
	int		      *op_status;
	struct m0_clovis_obj  *objs;
	struct m0_clovis_op  **ops;
	struct timeval        *op_list_time;
	struct m0_thread       mthread;
};

enum clovis_operations {
	CR_CREATE,
	CR_OPEN,
	CR_WRITE,
	CR_READ,
	CR_DELETE,
	CR_POPULATE,
	CR_CLEANUP,
	CR_OPS_NR
};

enum clovis_operation_status {
	CR_OP_NEW,
	CR_OP_EXECUTING,
	CR_OP_COMPLETE
};

enum clovis_thread_operation {
	CR_WRITE_TO_SAME = 0,
	CR_WRITE_TO_DIFF
};

struct cwi_global {
	struct m0_uint128 cg_oid;
	bool              cg_created;
	int               cg_nr_tasks;
	m0_time_t         cg_cwi_acc_time[CR_OPS_NR];
	struct m0_mutex   cg_mutex;
};

struct clovis_workload_io {
	/** Clovis Workload global context. */
	struct cwi_global cwi_g;
	uint32_t          cwi_layout_id;
	/** IO Block Size */
	uint64_t          cwi_bs;
	/**
	 * Number of blocks per IO operation. (Each thread
	 * can run several IO operations concurrently.)
	 */
	uint32_t          cwi_bcount_per_op;
	uint32_t          cwi_pool_id;
	uint64_t          cwi_io_size;
	uint64_t          cwi_ops_done[CR_OPS_NR];
	uint32_t          cwi_max_nr_ops;
	int32_t           cwi_mode;
	int32_t           cwi_nr_objs;
	uint32_t          cwi_rounds;
	bool              cwi_random_io;
	bool              cwi_share_object;
	int32_t	          cwi_opcode;
	struct m0_uint128 cwi_start_obj_id;
	m0_time_t         cwi_start_time;
	m0_time_t         cwi_finish_time;
	m0_time_t         cwi_execution_time;
	m0_time_t         cwi_time[CR_OPS_NR];
	char             *cwi_filename;
};

struct cti_global {
	struct m0_clovis_obj obj;
};

struct clovis_task_io {
	struct clovis_workload_io *cti_cwi;
	int                        cti_task_idx;
	uint32_t		  *cti_op_status;
	int32_t                    cti_progress;
	uint64_t                   cti_start_offset;
	struct m0_clovis_obj      *cti_objs;
	struct m0_clovis_op      **cti_ops;
	uint64_t                   cti_nr_ops;
	uint64_t                   cti_nr_ops_done;
	struct timeval            *cti_op_list_time;
	struct m0_thread          *cti_mthread;
	struct m0_bufvec          *cti_bufvec;
	struct m0_bufvec          *cti_rd_bufvec;
	struct m0_uint128         *cti_ids;
	m0_time_t                  cti_op_acc_time;
	struct cti_global          cti_g;
	/** Limit op_launch to max_nr_ops */
	struct m0_semaphore        cti_max_ops_sem;
};

int parse_crate(int argc, char **argv, struct workload *w);
void clovis_run(struct workload *w, struct workload_task *task);
void clovis_op_run(struct workload *w, struct workload_task *task,
		   const struct workload_op *op);
void clovis_run_index(struct workload *w, struct workload_task *tasks);
void clovis_op_run_index(struct workload *w, struct workload_task *task,
			 const struct workload_op *op);


/** @} end of crate_clovis group */
#endif /* __MERO_CLOVIS_M0CRATE_CRATE_CLOVIS_H__ */

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
