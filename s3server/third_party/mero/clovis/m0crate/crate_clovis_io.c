/* -*- C -*- */
/*
 * COPYRIGHT 2017 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author:  Pratik Shinde <pratik.shinde@seagate.com>
 * Original creation date: 20-Jun-2016
 */
/** @defgroup crate Crate
 * Crate is a performance benchmark tool for Mero via Clovis/m0t1fs.
 */
/** @defgroup io_workload Workload module for clovis.
 * \ingroup crate
 *
 * Crate IO workload overview.
 * -------------------------------
 *
 * Crate supports two types of object I/O Operations:
 *	1. Multiple threads writing/reading on/from the same object.
 *	2. Each thread writing/reading on/from different objects.
 *
 * For each of the above task I/O can be generated sequentially or parallely.
 * For example, Two threads to share the object and write randomly:
 * ```yaml
 *	NR_THREADS: 2
 *	THREAD_OPS: 1
 *	RAND_IO: 1
 *      CLOVIS_IOSIZE: 10M
 *      BLOCK_SIZE: 32768
 * ```
 * It is means, that crate performs next operations:
 * ```
 *	Spawns two threads.
 *      Create a single object.
 *      Each thread writes 4096 bytes Randomly on a shared object.
 * ```
 *
 * Crate I/O workload detailed description.
 * ------------------------------------------
 *
 * ## Workload has following parameters:
 *
 * * WORKLOAD_TYPE: always 1 (I/O operations).
 * * WORKLOAD_SEED: initial value for pseudo-random generator
 *	(int or "tstamp").
 * * OPCODE: 0-CREATE, 1-OPEN, 2-WRITE, 3-READ, 4-DELETE.
 * * CLOVIS_IOSIZE: - total size of I/O to perform per object.
 * * BLOCK_SIZE: For performance == parity group size.
 * * BLOCKS_PER_OP: - Number of blocks per Clovis operation.
 * * RAND_IO: Random (1) or sequential (0) IO?
 * * MAX_NR_OPS: - Max number of concurrent operations per thread.
 * * NR_OBJS - Each thread will create these many objects.
 * * NR_THREADS: - Number of threads.
 * * EXEC_TIME - time limit for executing (seconds or "unlimited").
 * * NR_ROUNDS:  - How many times this workload to be executed.
 *
 * ## Measurements
 * Currently, only execution time is measured during the test. It measures with
 * `m0_time*` functions. Crate prints result to stdout when test is finished.
 * ## Logging
 * crate has own logging system, which based on `fprintf(stderr...)`.
 * (see ::crlog and see ::cr_log).
 *
 * ## Execution time limits
 * There are two ways to limit execution time. The first, is
 * operations time limit, which checks after operation executed (as described
 * in HLD).
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>

#include "lib/finject.h"
#include "lib/trace.h"
#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_idx.h"

#include "clovis/m0crate/logger.h"
#include "clovis/m0crate/workload.h"
#include "clovis/m0crate/crate_clovis.h"
#include "clovis/m0crate/crate_clovis_utils.h"


void integrity(struct m0_uint128 object_id, unsigned char **md5toverify,
		int block_count, int idx_op);
void list_index_return(struct workload *w);

struct clovis_op_context {
	m0_time_t              coc_op_launch;
	m0_time_t              coc_op_finish;
	int                    coc_index;
	int                    coc_obj_index;
	struct clovis_workload_io *coc_cwi;
	enum clovis_operations coc_op_code;
	struct clovis_task_io *coc_task;
	struct m0_bufvec      *coc_buf_vec;
	struct m0_bufvec      *coc_attr;
	struct m0_indexvec    *coc_index_vec;
};

typedef int (*cr_operation_t)(struct clovis_workload_io *cwi,
		              struct clovis_task_io     *cti,
			      struct clovis_op_context  *op_ctx,
			      struct m0_clovis_obj      *obj,
			      int                        free_slot,
			      int                        obj_idx,
			      int                        op_index);

/**
 * Get pseudo-random uint64 in positive range [0;end)
 * clovis index workload has a similar function.
 * That function should be in utility and this should be deleted.
 */
static size_t cr_rand___range_l(size_t end)
{
	size_t val_h;
	size_t val_l;
	size_t res;

	val_l = rand();
	val_h = rand();
	res = (val_h << 32) | val_l;
	return res % end;
}

void cr_time_acc(m0_time_t *t1, m0_time_t t2)
{
	*t1 = m0_time_add(*t1, t2);
}

void cr_op_stable(struct m0_clovis_op *op)
{
	struct clovis_op_context *op_context;
	struct clovis_task_io    *cti;
	m0_time_t                 op_time;

	M0_PRE(op != NULL);
	op_context = op->op_datum;
	if (op_context != NULL) {
		cti = op_context->coc_task;

		op_context->coc_op_finish = m0_time_now();
		cti->cti_op_status[op_context->coc_index] = CR_OP_COMPLETE;
		cti->cti_nr_ops_done++;
		op_time = m0_time_sub(op_context->coc_op_finish,
				      op_context->coc_op_launch);
		cr_time_acc(&cti->cti_op_acc_time, op_time);
		m0_semaphore_up(&cti->cti_max_ops_sem);
		op_context->coc_buf_vec = NULL;
	}
}

void cr_op_failed(struct m0_clovis_op *op)
{
	int                       op_idx;
	struct clovis_op_context *op_context;

	M0_PRE(op != NULL);
	op_context = op->op_datum;

	cr_log(CLL_DEBUG, "Operation is failed:%d", op->op_sm.sm_rc);
	if (op_context != NULL) {
		op_idx = op_context->coc_index;
		op_context->coc_op_finish = m0_time_now();
		op_context->coc_task->cti_op_status[op_idx] = CR_OP_COMPLETE;
		m0_semaphore_up(&op_context->coc_task->cti_max_ops_sem);
	}
}

static void cti_cleanup_op(struct clovis_task_io *cti, int i)
{
	struct m0_clovis_op      *op = cti->cti_ops[i];
	struct clovis_op_context *op_ctx = op->op_datum;

	m0_clovis_op_fini(op);
	m0_clovis_op_free(op);
	cti->cti_ops[i] = NULL;
	if (op_ctx->coc_op_code == CR_WRITE ||
	    op_ctx->coc_op_code == CR_READ) {
		m0_bufvec_free(op_ctx->coc_buf_vec);
		m0_bufvec_free(op_ctx->coc_attr);
		m0_indexvec_free(op_ctx->coc_index_vec);
		m0_free(op_ctx->coc_buf_vec);
		m0_free(op_ctx->coc_attr);
		m0_free(op_ctx->coc_index_vec);
	}
	m0_free(op_ctx);
	cti->cti_op_status[i] = CR_OP_NEW;
}

int cr_free_op_idx(struct clovis_task_io *cti, uint32_t nr_ops)
{
	int i;

	for (i = 0; i < nr_ops; i++) {
		if (cti->cti_op_status[i] == CR_OP_NEW ||
		    cti->cti_op_status[i] == CR_OP_COMPLETE)
			break;
	}

	M0_ASSERT(i < nr_ops);

	if (cti->cti_op_status[i] == CR_OP_COMPLETE)
		cti_cleanup_op(cti, i);

	return i;
}

static void cr_cti_cleanup(struct clovis_task_io *cti, int nr_ops)
{
	int i;

	for (i = 0; i < nr_ops; i++)
		if (cti->cti_op_status[i] == CR_OP_COMPLETE)
			cti_cleanup_op(cti, i);
}

int cr_io_vector_prep(struct clovis_workload_io *cwi,
		      struct clovis_task_io     *cti,
		      struct clovis_op_context  *op_ctx,
		      int                        obj_idx,
		      int                        op_index)
{
	int                 i;
	int                 rc;
	uint64_t            bitmap_index;
	uint64_t            nr_segments;
	uint64_t            start_offset;
	uint64_t            op_start_offset = 0;
	size_t              rand_offset;
	uint64_t            io_size = cwi->cwi_io_size;
	uint64_t            offset;
	struct m0_bufvec   *buf_vec = NULL;
	struct m0_bufvec   *attr    = NULL;
	struct m0_indexvec *index_vec = NULL;
	struct m0_bitmap    segment_indices;

	index_vec = m0_alloc(sizeof *index_vec);
	attr = m0_alloc(sizeof *attr);
	if (index_vec == NULL || attr == NULL)
		goto enomem;

	rc = m0_indexvec_alloc(index_vec, cwi->cwi_bcount_per_op) ?:
	     m0_bufvec_alloc(attr, cwi->cwi_bcount_per_op, 1);
	if (rc != 0)
		goto enomem;
	/*
	 * When io_size is not a multiple of cwi_bs then bitmap_index can
	 * become equal to io_size/cwi->cwi_bs, hence '+1' to the size
	 * of bitmap.
	 */
	nr_segments = io_size/cwi->cwi_bs + 1;
	rc = m0_bitmap_init(&segment_indices, nr_segments);
	if (rc != 0)
		goto enomem;

	if (!cwi->cwi_random_io)
		op_start_offset = op_index * cwi->cwi_bs *
			cwi->cwi_bcount_per_op;

	if (op_ctx->coc_op_code == CR_READ)
		buf_vec = &cti->cti_rd_bufvec[obj_idx];
	else
		buf_vec = &cti->cti_bufvec[obj_idx];

	M0_ASSERT(buf_vec != NULL);
	M0_ASSERT(nr_segments > cwi->cwi_bcount_per_op);
	for (i = 0; i < cwi->cwi_bcount_per_op; i ++) {
		if (cwi->cwi_random_io) {
			do {
				/* Generate the random offset. */
				rand_offset = cr_rand___range_l(io_size);
				/*
				 * m0_round_down() would prevent partially
				 * overlapping indexvec segments.
				 */
				offset = m0_round_down(rand_offset,
						       cwi->cwi_bs);
				bitmap_index = offset / cwi->cwi_bs;
			} while (m0_bitmap_get(&segment_indices, bitmap_index));

			m0_bitmap_set(&segment_indices, bitmap_index, true);
		} else
			offset = op_start_offset + cwi->cwi_bs * i;

		/* If writing on shared object, start from the alloted range. */
		if (cwi->cwi_share_object) {
			start_offset = cti->cti_task_idx * io_size;
			index_vec->iv_index[i] = start_offset + offset;
		} else
			index_vec->iv_index[i] = offset;

		index_vec->iv_vec.v_count[i] = cwi->cwi_bs;
	}

	m0_bitmap_fini(&segment_indices);
	op_ctx->coc_buf_vec = buf_vec;
	op_ctx->coc_index_vec = index_vec;
	op_ctx->coc_attr = attr;

	return 0;
enomem:
	if (index_vec != NULL)
		m0_indexvec_free(index_vec);
	m0_bufvec_free(buf_vec);
	m0_free(attr);
	m0_free(index_vec);
	m0_free(buf_vec);
	return -ENOMEM;
}

int cr_namei_create(struct clovis_workload_io *cwi,
		    struct clovis_task_io     *cti,
		    struct clovis_op_context  *op_ctx,
		    struct m0_clovis_obj      *obj,
		    int                        free_slot,
		    int                        obj_idx,
		    int                        op_index)
{
	return m0_clovis_entity_create(NULL, &obj->ob_entity,
				       &cti->cti_ops[free_slot]);
}

/**
 * Open a created object,
 * Refer  EOS-285
 */
int cr_namei_open(struct clovis_workload_io *cwi,
		  struct clovis_task_io     *cti,
		  struct clovis_op_context  *op_ctx,
		  struct m0_clovis_obj      *obj,
		  int                        free_slot,
		  int                        obj_idx,
		  int                        op_index)
{
        const struct m0_uint128 *id = &cti->cti_ids[op_index];
	M0_PRE(obj != NULL);
	M0_SET0(obj);
	m0_clovis_obj_init(obj,
			   crate_clovis_uber_realm(),
			   id, cwi->cwi_layout_id);
	return m0_clovis_entity_open(&obj->ob_entity,
				     &cti->cti_ops[free_slot]);
}


int cr_namei_delete(struct clovis_workload_io *cwi,
		    struct clovis_task_io     *cti,
		    struct clovis_op_context  *op_ctx,
		    struct m0_clovis_obj      *obj,
		    int                        free_slot,
		    int                        obj_idx,
		    int                        op_index)
{
	return m0_clovis_entity_delete(&obj->ob_entity,
				       &cti->cti_ops[free_slot]);
}

int cr_io_write(struct clovis_workload_io    *cwi,
		struct clovis_task_io    *cti,
		struct clovis_op_context *op_ctx,
		struct m0_clovis_obj     *obj,
		int                       free_slot,
		int                       obj_idx,
		int                       op_index)
{
	int rc;

	op_ctx->coc_op_code = CR_WRITE;
	/** Create object index and buffer vectors. */
	rc = cr_io_vector_prep(cwi, cti, op_ctx, obj_idx, op_index);
	if (rc != 0)
		return rc;
	rc = m0_clovis_obj_op(obj, M0_CLOVIS_OC_WRITE,
			      op_ctx->coc_index_vec, op_ctx->coc_buf_vec,
			      op_ctx->coc_attr, 0, &cti->cti_ops[free_slot]);
	if (rc != 0)
		M0_ERR(rc);
	return rc;
}

int cr_io_read(struct clovis_workload_io   *cwi,
	       struct clovis_task_io    *cti,
	       struct clovis_op_context *op_ctx,
	       struct m0_clovis_obj     *obj,
	       int                       free_slot,
	       int                       obj_idx,
	       int                       op_index)
{
	int rc;

	op_ctx->coc_op_code = CR_READ;
	/** Create object index and buffer vectors. */
	rc = cr_io_vector_prep(cwi, cti, op_ctx, obj_idx, op_index);
	if (rc != 0)
		return rc;
	rc = m0_clovis_obj_op(obj, M0_CLOVIS_OC_READ,
			      op_ctx->coc_index_vec, op_ctx->coc_buf_vec,
			      op_ctx->coc_attr, 0, &cti->cti_ops[free_slot]);
	if (rc != 0)
		M0_ERR(rc);
	return rc;
}

cr_operation_t opcode_operation_map [] = {
	[CR_CREATE]         = cr_namei_create,
	[CR_OPEN]           = cr_namei_open,
	[CR_WRITE]          = cr_io_write,
	[CR_READ]           = cr_io_read,
	[CR_DELETE]         = cr_namei_delete
};

int cr_execute_ops(struct clovis_workload_io *cwi, struct clovis_task_io *cti,
		   struct m0_clovis_obj *obj, struct m0_clovis_op_ops  *cbs,
		   enum clovis_operations op_code, int obj_idx)
{
	int rc = 0;
	int i;
	int idx;
	struct clovis_op_context *op_ctx;
	cr_operation_t            spec_op;

	for (i = 0; i < cti->cti_nr_ops; i++) {
		m0_semaphore_down(&cti->cti_max_ops_sem);
		/* We can launch at least one more operation. */
		idx = cr_free_op_idx(cti, cwi->cwi_max_nr_ops);
		op_ctx = m0_alloc(sizeof *op_ctx);
		M0_ASSERT(op_ctx != NULL);

		op_ctx->coc_index = idx;
		op_ctx->coc_obj_index = obj_idx;
		op_ctx->coc_task = cti;
		op_ctx->coc_cwi = cwi;
		op_ctx->coc_op_code = op_code;

		spec_op = opcode_operation_map[op_code];
		rc = spec_op(cwi, cti, op_ctx, obj, idx, obj_idx, i);
		if (rc != 0)
			break;

		M0_ASSERT(cti->cti_ops[idx] != NULL);
		cti->cti_ops[idx]->op_datum = op_ctx;
		m0_clovis_op_setup(cti->cti_ops[idx], cbs, 0);
		cti->cti_op_status[idx] = CR_OP_EXECUTING;
		op_ctx->coc_op_launch = m0_time_now();
		m0_clovis_op_launch(&cti->cti_ops[idx], 1);
	}
	return rc;
}

void cr_cti_report(struct clovis_task_io *cti, enum clovis_operations op_code)
{
	struct clovis_workload_io *cwi = cti->cti_cwi;

	m0_mutex_lock(&cwi->cwi_g.cg_mutex);
	cr_time_acc(&cwi->cwi_g.cg_cwi_acc_time[op_code], cti->cti_op_acc_time);
	cwi->cwi_ops_done[op_code] += cti->cti_nr_ops_done;
	m0_mutex_unlock(&cwi->cwi_g.cg_mutex);

	cti->cti_op_acc_time = 0;
	cti->cti_nr_ops_done = 0;
}

int cr_op_namei(struct clovis_workload_io  *cwi, struct clovis_task_io *cti,
		enum clovis_operations op_code)
{
	int                       i;
	int                       idx;
	m0_time_t                 stime;
	m0_time_t                 etime;
	struct clovis_op_context *op_ctx;
	struct m0_clovis_op_ops  *cbs;
	cr_operation_t            spec_op;

	cbs = m0_alloc(sizeof *cbs);
	M0_ASSERT(cbs != NULL);
	cbs->oop_executed = NULL;
	cbs->oop_stable = cr_op_stable;
	cbs->oop_failed = cr_op_failed;

	cr_log(CLL_TRACE, TIME_F" t%02d: %s objects...\n",
	       TIME_P(m0_time_now()), cti->cti_task_idx,
	       op_code == CR_CREATE ? "Creating" : "Deleting");
	m0_semaphore_init(&cti->cti_max_ops_sem, cwi->cwi_max_nr_ops);
	stime = m0_time_now();

	for (i = 0; i < cwi->cwi_nr_objs; i++) {
		m0_semaphore_down(&cti->cti_max_ops_sem);
		/* We can launch at least one more operation. */
		idx = cr_free_op_idx(cti, cwi->cwi_max_nr_ops);
		op_ctx = m0_alloc(sizeof *op_ctx);
		M0_ASSERT(op_ctx != NULL);

		op_ctx->coc_index = idx;
		op_ctx->coc_task = cti;
		op_ctx->coc_op_code = op_code;
		spec_op = opcode_operation_map[op_code];
		spec_op(cwi, cti, op_ctx, &cti->cti_objs[i], idx, 0, i);

		cti->cti_ops[idx]->op_datum = op_ctx;
		m0_clovis_op_setup(cti->cti_ops[idx], cbs, 0);
		cti->cti_op_status[idx] = CR_OP_EXECUTING;
		op_ctx->coc_op_launch = m0_time_now();
		m0_clovis_op_launch(&cti->cti_ops[idx], 1);
	}

	/* Task is done. Wait for all operations to complete. */
	for (i = 0; i < cwi->cwi_max_nr_ops; i++)
		m0_semaphore_down(&cti->cti_max_ops_sem);

	etime = m0_time_sub(m0_time_now(), stime);
	m0_mutex_lock(&cwi->cwi_g.cg_mutex);
	if (etime > cwi->cwi_time[op_code])
		cwi->cwi_time[op_code] = etime;
	m0_mutex_unlock(&cwi->cwi_g.cg_mutex);
	cr_cti_report(cti, op_code);
	cr_cti_cleanup(cti, cwi->cwi_max_nr_ops);
	m0_semaphore_fini(&cti->cti_max_ops_sem);
	m0_free(cbs);
	/*
	 * Create - creates an object and close it.
	 * Open *should* be called before any further operation.
	 * Open call would initialise clovis object, object would
	 * remain opened till all IO operation are complete.
	 */
	if (op_code == CR_CREATE && cti->cti_objs != NULL)
		/** Close object only for create opcode */
		for (i = 0; i < cti->cti_cwi->cwi_nr_objs; i++)
			m0_clovis_obj_fini(&cti->cti_objs[i]);
	cr_log(CLL_TRACE, TIME_F" t%02d: %s done.\n",
	       TIME_P(m0_time_now()), cti->cti_task_idx,
	       op_code == CR_CREATE ? "Creation" : "Deletion");

	return 0;
}

int cr_op_io(struct clovis_workload_io  *cwi, struct clovis_task_io *cti,
	     enum clovis_operations op_code)
{
	int       rc = 0;
	int       i;
	m0_time_t stime;
	m0_time_t etime;
	struct m0_clovis_op_ops *cbs;

	M0_ALLOC_PTR(cbs);
	if (cbs == NULL) {
		m0_free(cbs);
		return -ENOMEM;
	}

	cbs->oop_executed = NULL;
	cbs->oop_stable   = cr_op_stable;
	cbs->oop_failed   = cr_op_failed;
	cr_log(CLL_TRACE, TIME_F" t%02d: %s objects...\n",
	       TIME_P(m0_time_now()), cti->cti_task_idx,
	       op_code == CR_WRITE ? "Writing" : "Reading");
	m0_semaphore_init(&cti->cti_max_ops_sem, cwi->cwi_max_nr_ops);
	stime = m0_time_now();

	for (i = 0; i < cwi->cwi_nr_objs; i++) {
		rc = cr_execute_ops(cwi, cti, &cti->cti_objs[i], cbs, op_code,
				    i);
		if (rc != 0)
			break;

	}
	/* Wait for all operations to complete. */
	for (i = 0; i < cwi->cwi_max_nr_ops; i++)
		m0_semaphore_down(&cti->cti_max_ops_sem);

	etime = m0_time_sub(m0_time_now(), stime);
	m0_mutex_lock(&cwi->cwi_g.cg_mutex);
	if (etime > cwi->cwi_time[op_code])
		cwi->cwi_time[op_code] = etime;
	m0_mutex_unlock(&cwi->cwi_g.cg_mutex);
	cr_cti_report(cti, op_code);
	cr_cti_cleanup(cti, cwi->cwi_max_nr_ops);
	m0_semaphore_fini(&cti->cti_max_ops_sem);
	m0_free(cbs);
	cr_log(CLL_TRACE, TIME_F" t%02d: %s done.\n",
	       TIME_P(m0_time_now()), cti->cti_task_idx,
	       op_code == CR_WRITE ? "Write" : "Read");

	return rc;
}

/**
 * Creates a global object shared by all the tasks in the workload.
 * Object operation supported is READ/WRITE.
 */
int cr_task_share_execute(struct clovis_task_io *cti)
{
	int                        rc;
	struct clovis_workload_io *cwi;

	cwi = cti->cti_cwi;

	/** Only parallel writes are supported. */
	M0_ASSERT(cwi->cwi_opcode == CR_WRITE);
	m0_mutex_lock(&cwi->cwi_g.cg_mutex);
	if (!cwi->cwi_g.cg_created) {
		rc = cr_op_namei(cti->cti_cwi, cti, CR_CREATE);
		if (rc != 0) {
			m0_mutex_unlock(&cwi->cwi_g.cg_mutex);
			return rc;
		}
		cwi->cwi_g.cg_created = true;
	}
	m0_mutex_unlock(&cwi->cwi_g.cg_mutex);

	cr_op_io(cti->cti_cwi, cti, cwi->cwi_opcode);

	m0_mutex_lock(&cwi->cwi_g.cg_mutex);
	cwi->cwi_g.cg_nr_tasks--;
	if (cwi->cwi_g.cg_nr_tasks == 0) {
		cr_op_namei(cti->cti_cwi, cti, CR_DELETE);
		cwi->cwi_g.cg_created = false;
	}
	m0_mutex_unlock(&cwi->cwi_g.cg_mutex);

	return 0;
}

int cr_task_execute(struct clovis_task_io *cti)
{
	struct clovis_workload_io  *cwi = cti->cti_cwi;
	/**
	 * IO workflow :
	 * Create an object - creates an object and close it.
	 * For io operation - open object, perform io and delete it.
	 */
	switch (cwi->cwi_opcode) {
		case CR_CREATE:
			cr_op_namei(cwi, cti, CR_CREATE);
			cr_op_namei(cwi, cti, CR_OPEN);
			cr_op_namei(cwi, cti, CR_DELETE);
			break;
		case CR_OPEN:
			cr_op_namei(cwi, cti, CR_CREATE);
			cr_op_namei(cwi, cti, CR_OPEN);
			cr_op_namei(cwi, cti, CR_DELETE);
			break;
		case CR_WRITE:
			cr_op_namei(cwi, cti, CR_CREATE);
			cr_op_namei(cwi, cti, CR_OPEN);
			cr_op_io(cwi, cti, CR_WRITE);
			cr_op_namei(cwi, cti, CR_DELETE);
			break;
		case CR_READ:
			cr_op_namei(cwi, cti, CR_CREATE);
			cr_op_namei(cwi, cti, CR_OPEN);
			cr_op_io(cwi, cti, CR_WRITE);
			cr_op_io(cwi, cti, CR_READ);
			cr_op_namei(cwi, cti, CR_DELETE);
			break;
		case CR_DELETE:
			cr_op_namei(cwi, cti, CR_CREATE);
			cr_op_namei(cwi, cti, CR_OPEN);
			cr_op_namei(cwi, cti, CR_DELETE);
			break;
		case CR_POPULATE:
			cr_op_namei(cwi, cti, CR_CREATE);
			cr_op_namei(cwi, cti, CR_OPEN);
			cr_op_io(cwi, cti, CR_WRITE);
			break;
		case CR_CLEANUP:
			cr_op_namei(cwi, cti, CR_OPEN);
			cr_op_namei(cwi, cti, CR_DELETE);
			break;
	}
	return 0;
}


static int cr_adopt_mero_thread(struct clovis_task_io *cti)
{
	int               rc = 0;
	struct m0_thread *mthread;

	cti->cti_mthread = NULL;
	if (m0_thread_tls() == NULL) {
		mthread = m0_alloc(sizeof(struct m0_thread));
		if (mthread == NULL)
			return -ENOMEM;

		memset(mthread, 0, sizeof(struct m0_thread));
		rc = m0_thread_adopt(mthread, clovis_instance->m0c_mero);
		cti->cti_mthread = mthread;
	}
	return rc;
}

static int cr_release_mero_thread(struct clovis_task_io *cti)
{
	if (cti->cti_mthread) {
		m0_thread_shun();
		m0_free(cti->cti_mthread);
	}
	return 0;
}

int cr_buffer_read(char *buffer, const char *filename, uint64_t size)
{
	FILE  *fp;
	size_t bytes;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		cr_log(CLL_ERROR, "Unable to open a file: %s\n", filename);
		return -errno;
	}

	bytes = fread(buffer, 1, size, fp);
	if (bytes < size) {
		fclose(fp);
		return -EINVAL;
	}
	fclose(fp);
	return 0;
}

static uint64_t nz_rand(void)
{
	uint64_t r;

	do {
		r = ((uint64_t)rand() << 32) | rand();
	} while (r == 0);

	return r;
}

void cr_get_oids(struct m0_uint128 *ids, uint32_t nr_objs)
{
	int i;
	for (i = 0; i < nr_objs; i++) {
		ids[i].u_lo = nz_rand();
		ids[i].u_hi = nz_rand();
		/* Highest 8 bits are left for Mero. */
		ids[i].u_hi = ids[i].u_hi & ~(0xFFUL << 56);
		cr_log(CLL_TRACE, "oid %016lx:%016lx\n",
		       ids[i].u_hi, ids[i].u_lo);
	}
}

void cr_task_bufs_free(struct clovis_task_io *cti, int idx)
{
	m0_bufvec_free_aligned(&cti->cti_bufvec[idx],
			       CLOVIS_DEFAULT_BUF_SHIFT);
	m0_bufvec_free_aligned(&cti->cti_rd_bufvec[idx],
			       CLOVIS_DEFAULT_BUF_SHIFT);
}

void cr_task_io_cleanup(struct clovis_task_io **cti_p)
{
	int i;
	struct clovis_task_io     *cti = *cti_p;
	struct clovis_workload_io *cwi = cti->cti_cwi;

	if (cti->cti_objs != NULL)
		for (i = 0; i < cti->cti_cwi->cwi_nr_objs; i++) {
			m0_clovis_obj_fini(&cti->cti_objs[i]);
			if (cwi->cwi_opcode != CR_CLEANUP)
				cr_task_bufs_free(cti, i);
		}
	m0_free(cti->cti_objs);
	m0_free(cti->cti_ops);
	m0_free(cti->cti_bufvec);
	m0_free(cti->cti_rd_bufvec);
	m0_free(cti->cti_op_status);
	m0_free0(cti_p);
}

int cr_task_prep_bufs(struct clovis_workload_io *cwi,
		      struct clovis_task_io     *cti)
{
	int i;
	int k;
	int rc;

	M0_ALLOC_ARR(cti->cti_bufvec, cwi->cwi_nr_objs);
	M0_ALLOC_ARR(cti->cti_rd_bufvec, cwi->cwi_nr_objs);

	for (i = 0; i < cwi->cwi_nr_objs; i++) {
		rc = m0_bufvec_alloc_aligned(&cti->cti_bufvec[i],
					     cwi->cwi_bcount_per_op,
					     cwi->cwi_bs,
					     CLOVIS_DEFAULT_BUF_SHIFT) ?:
		     m0_bufvec_alloc_aligned(&cti->cti_rd_bufvec[i],
					     cwi->cwi_bcount_per_op,
					     cwi->cwi_bs,
					     CLOVIS_DEFAULT_BUF_SHIFT);
		if (rc != 0)
			return rc;
		for (k = 0; k < cwi->cwi_bcount_per_op; k++) {
			rc = cr_buffer_read(cti->cti_bufvec[i].ov_buf[k],
					    cwi->cwi_filename, cwi->cwi_bs);
			if (rc != 0)
				return rc;
		}
	}
	return 0;
}

int cr_task_prep_one(struct clovis_workload_io *cwi,
		     struct clovis_task_io    **cti_out)
{
	int rc;
	int i;
	struct clovis_task_io *cti;

	if (M0_ALLOC_PTR(*cti_out) == NULL)
		return -ENOMEM;
	cti = *cti_out;

	cti->cti_cwi = cwi;
	cti->cti_progress = 0;

	if (cwi->cwi_opcode != CR_CLEANUP) {
		cti->cti_nr_ops = (cwi->cwi_io_size /
				  (cwi->cwi_bs * cwi->cwi_bcount_per_op)) ?: 1;
		rc = cr_task_prep_bufs(cwi, cti);
		if (rc != 0)
			goto error_rc;
	}

	M0_ALLOC_ARR(cti->cti_ids, cwi->cwi_nr_objs);
	if (cti->cti_ids == NULL)
		goto enomem;

	if (cwi->cwi_share_object) {
		cti->cti_ids[0] = cwi->cwi_g.cg_oid;
	} else if (M0_IN(cwi->cwi_opcode, (CR_POPULATE, CR_CLEANUP))) {
		int i;
		for (i = 0; i< cwi->cwi_nr_objs; i++) {
			cwi->cwi_start_obj_id.u_lo++;
			cti->cti_ids[i] = cwi->cwi_start_obj_id;
		}
	} else {
		cti->cti_start_offset = 0;
		cr_get_oids(cti->cti_ids, cwi->cwi_nr_objs);
	}

	M0_ALLOC_ARR(cti->cti_objs, cwi->cwi_nr_objs);
	if (cti->cti_objs == NULL)
		goto enomem;

	for (i = 0; i < cwi->cwi_nr_objs; i++)
		m0_clovis_obj_init(&cti->cti_objs[i],
				   crate_clovis_uber_realm(),
				   &cti->cti_ids[i], cwi->cwi_layout_id);

	M0_ALLOC_ARR(cti->cti_ops, cwi->cwi_max_nr_ops);
	if (cti->cti_ops == NULL)
		goto enomem;

	M0_ALLOC_ARR(cti->cti_op_status, cwi->cwi_max_nr_ops);
	if (cti->cti_op_status == NULL)
		goto enomem;

	return 0;
enomem:
	rc = -ENOMEM;
error_rc:
	cr_task_io_cleanup(cti_out);
	return rc;
}

int cr_tasks_prepare(struct workload *w, struct workload_task *tasks)
{
	int                        i;
	int                        rc;
	uint32_t                   nr_tasks;
	struct clovis_workload_io *cwi = w->u.cw_clovis_io;
	struct clovis_task_io    **cti;

	nr_tasks = w->cw_nr_thread;
	if (cwi->cwi_opcode == CR_CLEANUP)
		cwi->cwi_share_object = false;

	if (cwi->cwi_share_object) {
		/* Generate only one id */
		cwi->cwi_nr_objs = 1;
		cr_get_oids(&cwi->cwi_g.cg_oid, 1);
		cwi->cwi_g.cg_nr_tasks = nr_tasks;
		cwi->cwi_g.cg_created  = false;
	}

	for (i = 0; i < nr_tasks; i++) {
		cti = (struct clovis_task_io **)&tasks[i].u.clovis_task;
		rc = cr_task_prep_one(cwi, cti);
		if (rc != 0) {
			cti = NULL;
			return rc;
		}
		M0_ASSERT(*cti != NULL);

		(*cti)->cti_task_idx = i;
	}
	return 0;
}

int cr_tasks_release(struct workload *w, struct workload_task *tasks)
{
	int                    i;
	uint32_t               nr_tasks;
	struct clovis_task_io *cti;

	nr_tasks = w->cw_nr_thread;

	for (i = 0; i < nr_tasks; i++) {
		cti = tasks[i].u.clovis_task;
		if (cti != NULL)
			cr_task_io_cleanup(&cti);
	}

	return 0;
}

bool cr_time_not_expired(struct workload *w)
{
	struct clovis_workload_io *cwi;
	m0_time_t time_now;

	cwi = w->u.cw_clovis_io;
	time_now = m0_time_now();

	if (cwi->cwi_execution_time == M0_TIME_NEVER)
		return false;
	return	m0_time_sub(time_now, cwi->cwi_start_time) <
		cwi->cwi_execution_time ? false : true;
}

/** Returns bandwidth in bytes / sec. */
static uint64_t bw(uint64_t bytes, m0_time_t time)
{
	return bytes * M0_TIME_ONE_MSEC / (time / 1000);
}

void clovis_run(struct workload *w, struct workload_task *tasks)
{
	int        i;
	uint64_t   written;
	uint64_t   read;
	struct clovis_workload_io *cwi = w->u.cw_clovis_io;

	m0_mutex_init(&cwi->cwi_g.cg_mutex);
	cwi->cwi_start_time = m0_time_now();
	if (M0_IN(cwi->cwi_opcode, (CR_POPULATE, CR_CLEANUP)) &&
	    !clovis_entity_id_is_valid(&cwi->cwi_start_obj_id))
		cwi->cwi_start_obj_id = M0_CLOVIS_ID_APP;
	for (i = 0; i < cwi->cwi_rounds || cr_time_not_expired(w); i++) {
		cr_tasks_prepare(w, tasks);
		workload_start(w, tasks);
		workload_join(w, tasks);
		cr_tasks_release(w, tasks);
	}
	m0_mutex_fini(&cwi->cwi_g.cg_mutex);
	cwi->cwi_finish_time = m0_time_now();

	cr_log(CLL_INFO, "I/O workload is finished.\n");
	cr_log(CLL_INFO, "Total: time="TIME_F" objs=%d ops=%lu\n",
	       TIME_P(m0_time_sub(cwi->cwi_finish_time, cwi->cwi_start_time)),
	       cwi->cwi_nr_objs * w->cw_nr_thread,
	       cwi->cwi_ops_done[CR_WRITE] + cwi->cwi_ops_done[CR_READ]);
	if (cwi->cwi_ops_done[CR_CREATE] != 0)
		cr_log(CLL_INFO, "C: "TIME_F" ("TIME_F" per op)\n",
		       TIME_P(cwi->cwi_time[CR_CREATE]),
		       TIME_P(cwi->cwi_g.cg_cwi_acc_time[CR_CREATE] /
			      cwi->cwi_ops_done[CR_CREATE]));
	if (cwi->cwi_ops_done[CR_OPEN] != 0)
		cr_log(CLL_INFO, "O: "TIME_F" ("TIME_F" per op)\n",
		       TIME_P(cwi->cwi_time[CR_OPEN]),
		       TIME_P(cwi->cwi_g.cg_cwi_acc_time[CR_OPEN] /
			      cwi->cwi_ops_done[CR_OPEN]));
	if (cwi->cwi_ops_done[CR_DELETE] != 0)
		cr_log(CLL_INFO, "D: "TIME_F" ("TIME_F" per op)\n",
		       TIME_P(cwi->cwi_time[CR_DELETE]),
		       TIME_P(cwi->cwi_g.cg_cwi_acc_time[CR_DELETE] /
			      cwi->cwi_ops_done[CR_DELETE]));
	if (cwi->cwi_ops_done[CR_WRITE] == 0)
		return;
	written = cwi->cwi_bs * cwi->cwi_bcount_per_op *
	          cwi->cwi_ops_done[CR_WRITE];
	cr_log(CLL_INFO, "W: "TIME_F" ("TIME_F" per op), "
	       "%lu KiB, %lu KiB/s\n", TIME_P(cwi->cwi_time[CR_WRITE]),
	       TIME_P(cwi->cwi_g.cg_cwi_acc_time[CR_WRITE] /
		      cwi->cwi_ops_done[CR_WRITE]), written/1024,
	       bw(written, cwi->cwi_time[CR_WRITE]) /1024);
	if (cwi->cwi_ops_done[CR_READ] == 0)
		return;
	read = cwi->cwi_bs * cwi->cwi_bcount_per_op *
	       cwi->cwi_ops_done[CR_READ];
	cr_log(CLL_INFO, "R: "TIME_F" ("TIME_F" per op), "
	       "%lu KiB, %lu KiB/s\n", TIME_P(cwi->cwi_time[CR_READ]),
	       TIME_P(cwi->cwi_g.cg_cwi_acc_time[CR_READ] /
		      cwi->cwi_ops_done[CR_READ]), read/1024,
	       bw(read, cwi->cwi_time[CR_READ]) /1024);
}

void clovis_op_run(struct workload *w, struct workload_task *task,
		   const struct workload_op *op)
{
	struct clovis_task_io *cti = task->u.clovis_task;
	int                    rc;

	/** Task is not prepared. */
	if (cti == NULL)
		return;

	rc = cr_adopt_mero_thread(cti);
	if (rc < 0)
		cr_log(CLL_ERROR, "Mero adoption failed with rc=%d", rc);

	if (cti->cti_cwi->cwi_share_object)
		cr_task_share_execute(cti);
	else
		cr_task_execute(cti);
	cr_release_mero_thread(cti);
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
