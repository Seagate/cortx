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
 * Revision:         Abhishek Saha       <abhishek.saha@seagate.com>
 * Original creation date: 25-Sept-2018
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <getopt.h>

#include "lib/trace.h"
#include "conf/obj.h"
#include "fid/fid.h"
#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_idx.h"
#include "clovis/st/utils/clovis_helper.h"
#include "lib/getopts.h"
#include "clovis/clovis_internal.h"

/** Max number of blocks in concurrent IO per thread. */
enum { CLOVIS_MAX_BLOCK_COUNT = 100 };

extern struct m0_addb_ctx m0_clovis_addb_ctx;

static int noop_lock_init(struct m0_clovis_obj *obj,
			  const struct m0_uint128 *group)
{
	/* Do nothing */
	return 0;
}

static void noop_lock_fini(struct m0_clovis_obj *obj)
{
	/* Do nothing */
}

static int noop_lock_get(struct m0_clovis_obj *obj,
			 struct m0_clovis_rm_lock_req *req,
			 const struct m0_uint128 *group,
			 struct m0_clink *clink)
{
	/* Do nothing */
	return 0;
}

static int noop_lock_get_sync(struct m0_clovis_obj *obj,
			      struct m0_clovis_rm_lock_req *req,
			      const struct m0_uint128 *group)
{
	/* Do nothing */
	return 0;
}

static void noop_lock_put(struct m0_clovis_rm_lock_req *req)
{
	/* Do nothing */
}

const struct clovis_obj_lock_ops lock_enabled_ops = {
	.olo_lock_init     = m0_clovis_obj_lock_init,
	.olo_lock_fini     = m0_clovis_obj_lock_fini,
	.olo_lock_get      = m0_clovis_obj_lock_get,
	.olo_lock_get_sync = m0_clovis_obj_lock_get_sync,
	.olo_lock_put      = m0_clovis_obj_lock_put
};

const struct clovis_obj_lock_ops lock_disabled_ops = {
	.olo_lock_init     = noop_lock_init,
	.olo_lock_fini     = noop_lock_fini,
	.olo_lock_get      = noop_lock_get,
	.olo_lock_get_sync = noop_lock_get_sync,
	.olo_lock_put      = noop_lock_put
};

static inline uint32_t entity_sm_state(struct m0_clovis_obj *obj)
{
	return obj->ob_entity.en_sm.sm_state;
}

static int clovis_alloc_prepare_vecs(struct m0_indexvec *ext,
				     struct m0_bufvec *data,
				     struct m0_bufvec *attr,
				     uint32_t block_count, uint32_t block_size,
				     uint64_t *last_index)
{
	int      rc;
	int      i;

	rc = m0_indexvec_alloc(ext, block_count);
	if (rc != 0)
		return rc;

	/*
	 * this allocates <block_count> * <block_size>  buffers for data,
	 * and initialises the bufvec for us.
	 */
	rc = m0_bufvec_alloc(data, block_count, block_size);
	if (rc != 0) {
		m0_indexvec_free(ext);
		return rc;
	}

	rc = m0_bufvec_alloc(attr, block_count, 1);
	if (rc != 0) {
		m0_indexvec_free(ext);
		m0_bufvec_free(data);
		return rc;
	}

	for (i = 0; i < block_count; ++i) {
		ext->iv_index[i] = *last_index;
		ext->iv_vec.v_count[i] = block_size;
		*last_index += block_size;

		/* we don't want any attributes */
		attr->ov_vec.v_count[i] = 0;
	}
	return rc;
}

static void clovis_cleanup_vecs(struct m0_bufvec *data, struct m0_bufvec *attr,
			        struct m0_indexvec *ext)
{
		/* Free bufvec's and indexvec's */
		m0_indexvec_free(ext);
		m0_bufvec_free(data);
		m0_bufvec_free(attr);
}

int clovis_init(struct m0_clovis_config    *config,
	        struct m0_clovis_container *clovis_container,
	        struct m0_clovis          **clovis_instance)
{
	int rc;

	if (config->cc_local_addr == NULL || config->cc_ha_addr == NULL ||
	    config->cc_profile == NULL || config->cc_process_fid == NULL) {
		rc = M0_ERR(-EINVAL);
		fprintf(stderr, "config parameters not initialized.\n");
		goto err_exit;
	}

	rc = m0_clovis_init(clovis_instance, config, true);
	if (rc != 0)
		goto err_exit;

	m0_clovis_container_init(clovis_container,
				 NULL, &M0_CLOVIS_UBER_REALM,
				 *clovis_instance);
	rc = clovis_container->co_realm.re_entity.en_sm.sm_rc;

err_exit:
	return rc;
}

void clovis_fini(struct m0_clovis *clovis_instance)
{
	m0_clovis_fini(clovis_instance, true);
}

int clovis_obj_id_sscanf(char *idstr, struct m0_uint128 *obj_id)
{
	int rc;

	if (strchr(idstr, ':') == NULL) {
		obj_id->u_lo = atoi(idstr);
		return 0;
	}

	rc = m0_fid_sscanf(idstr, (struct m0_fid *)obj_id);
	if (rc != 0)
		fprintf(stderr, "can't m0_fid_sscanf() %s, rc:%d", idstr, rc);
	return rc;
}

static int  open_entity(struct m0_clovis_entity *entity)
{
	int                  rc;
	struct m0_clovis_op *ops[1] = {NULL};

	rc = m0_clovis_entity_open(entity, &ops[0]);
	if (rc != 0)
		goto cleanup;

	m0_clovis_op_launch(ops, 1);
	rc = m0_clovis_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);
	if (rc == 0)
		rc = ops[0]->op_rc;

cleanup:
	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
	ops[0] = NULL;

	return rc;
}

static int create_object(struct m0_clovis_entity *entity)
{
	int                  rc;
	struct m0_clovis_op *ops[1] = {NULL};

	rc = m0_clovis_entity_create(NULL, entity, &ops[0]);
	if (rc != 0)
		goto cleanup;

	m0_clovis_op_launch(ops, 1);
	rc = m0_clovis_op_wait(ops[0],
			       M0_BITS(M0_CLOVIS_OS_FAILED,
				       M0_CLOVIS_OS_STABLE),
				       M0_TIME_NEVER);
	if (rc == 0)
		rc = ops[0]->op_rc;

cleanup:
	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);

	return rc;
}

static int read_data_from_file(FILE *fp, struct m0_bufvec *data)
{
	int i;
	int rc;
	int nr_blocks;

	nr_blocks = data->ov_vec.v_nr;
	for (i = 0; i < nr_blocks; ++i) {
		rc = fread(data->ov_buf[i], data->ov_vec.v_count[i], 1, fp);
		if (rc != 1)
			break;

		if (feof(fp))
			break;
	}

	return i;
}

static int write_data_to_object(struct m0_clovis_obj *obj,
				struct m0_indexvec *ext,
				struct m0_bufvec *data,
				struct m0_bufvec *attr)
{
	int                  rc;
	int                  op_rc;
	int                  nr_tries = 10;
	struct m0_clovis_op *ops[1] = {NULL};

again:

	/* Create the write request */
	rc = m0_clovis_obj_op(obj, M0_CLOVIS_OC_WRITE,
			      ext, data, attr, 0, &ops[0]);
	if (rc != 0)
		return M0_ERR(rc);

	/* Launch the write request*/
	m0_clovis_op_launch(ops, 1);

	/* wait */
	rc = m0_clovis_op_wait(ops[0],
			M0_BITS(M0_CLOVIS_OS_FAILED,
				M0_CLOVIS_OS_STABLE),
			M0_TIME_NEVER);
	op_rc = ops[0]->op_sm.sm_rc;

	/* fini and release */
	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);

	if (op_rc == -EINVAL && nr_tries != 0) {
		nr_tries--;
		ops[0] = NULL;
		sleep(5);
		goto again;
	}

	return rc;
}

int clovis_touch(struct m0_clovis_container *clovis_container,
		 struct m0_uint128 id, bool take_locks)
{
	int                               rc = 0;
	struct m0_clovis_obj              obj;
	struct m0_clovis                 *clovis_instance;
	struct m0_clovis_rm_lock_req      req;
	const struct clovis_obj_lock_ops *lock_ops;
	struct m0_clink                   clink;

	M0_SET0(&obj);
	M0_SET0(&req);
	lock_ops = take_locks ? &lock_enabled_ops : &lock_disabled_ops;
	clovis_instance = clovis_container->co_realm.re_instance;
	m0_clovis_obj_init(&obj, &clovis_container->co_realm, &id,
			   m0_clovis_layout_id(clovis_instance));
	rc = lock_ops->olo_lock_init(&obj, NULL);
	if (rc != 0)
		goto init_error;

	m0_clink_init(&clink, NULL);
	clink.cl_is_oneshot = true;
	lock_ops->olo_lock_get(&obj, &req, NULL, &clink);
	if (take_locks)
		m0_chan_wait(&clink);
	m0_clink_fini(&clink);
	rc = req.rlr_rc;
	if (rc != 0)
		goto get_error;

	rc = create_object(&obj.ob_entity);

	/* fini and release */
	lock_ops->olo_lock_put(&req);
get_error:
	lock_ops->olo_lock_fini(&obj);
init_error:
	m0_clovis_entity_fini(&obj.ob_entity);

	return rc;
}

int clovis_write(struct m0_clovis_container *clovis_container,
		 char *src, struct m0_uint128 id, uint32_t block_size,
		 uint32_t block_count, bool update_mode, bool take_locks)
{
	int                               rc;
	struct m0_indexvec                ext;
	struct m0_bufvec                  data;
	struct m0_bufvec                  attr;
	uint32_t                          bcount;
	uint64_t                          last_index;
	FILE                             *fp;
	struct m0_clovis_obj              obj;
	struct m0_clovis                 *clovis_instance;
	struct m0_clovis_rm_lock_req      req;
	const struct clovis_obj_lock_ops *lock_ops;

	/* Open source file */
	fp = fopen(src, "r");
	if (fp == NULL)
		return -EPERM;
	M0_SET0(&obj);
	lock_ops = take_locks ? &lock_enabled_ops : &lock_disabled_ops;
	clovis_instance = clovis_container->co_realm.re_instance;
	m0_clovis_obj_init(&obj, &clovis_container->co_realm, &id,
			   m0_clovis_layout_id(clovis_instance));
	rc = lock_ops->olo_lock_init(&obj, NULL);
	if (rc != 0)
		goto init_error;
	rc = lock_ops->olo_lock_get_sync(&obj, &req, NULL);
	if (rc != 0)
		goto get_error;
	if (update_mode)
		rc = open_entity(&obj.ob_entity);
	else
		rc = create_object(&obj.ob_entity);
	if (entity_sm_state(&obj) != M0_CLOVIS_ES_OPEN || rc != 0)
		goto cleanup;

	last_index = 0;
	while (block_count > 0) {
		bcount = (block_count > CLOVIS_MAX_BLOCK_COUNT)?
			  CLOVIS_MAX_BLOCK_COUNT:block_count;
		rc = clovis_alloc_prepare_vecs(&ext, &data, &attr, bcount,
					       block_size, &last_index);
		if (rc != 0)
			goto cleanup;

		/* Read data from source file. */
		rc = read_data_from_file(fp, &data);
		M0_ASSERT(rc == bcount);

		/* Copy data to the object*/
		rc = write_data_to_object(&obj, &ext, &data, &attr);
		if (rc != 0) {
			fprintf(stderr, "Writing to object failed!\n");
			clovis_cleanup_vecs(&data, &attr, &ext);
			goto cleanup;
		}
		clovis_cleanup_vecs(&data, &attr, &ext);
		block_count -= bcount;
	}

	/* fini and release */
cleanup:
	lock_ops->olo_lock_put(&req);
get_error:
	lock_ops->olo_lock_fini(&obj);
init_error:
	m0_clovis_entity_fini(&obj.ob_entity);
	fclose(fp);
	return rc;
}

int clovis_read(struct m0_clovis_container *clovis_container,
		struct m0_uint128 id, char *dest,
		uint32_t block_size, uint32_t block_count, bool take_locks)
{
	int                               i;
	int                               j;
	int                               rc;
	uint64_t                          bytes_read= 0;
	uint64_t                          last_index = 0;
	struct m0_clovis_op              *ops[1] = {NULL};
	struct m0_clovis_obj              obj;
	struct m0_indexvec                ext;
	struct m0_bufvec                  data;
	struct m0_bufvec                  attr;
	FILE                             *fp = NULL;
	struct m0_clovis                 *clovis_instance;
	struct m0_clovis_rm_lock_req      req;
	const struct clovis_obj_lock_ops *lock_ops;

	lock_ops = take_locks ? &lock_enabled_ops : &lock_disabled_ops;
	rc = clovis_alloc_prepare_vecs(&ext, &data, &attr, block_count,
				       block_size, &last_index);
	if (rc != 0)
		return rc;
	clovis_instance = clovis_container->co_realm.re_instance;

	/* Read the requisite number of blocks from the entity */
	M0_SET0(&obj);
	m0_clovis_obj_init(&obj, &clovis_container->co_realm, &id,
			   m0_clovis_layout_id(clovis_instance));
	rc = lock_ops->olo_lock_init(&obj, NULL);
	if (rc != 0)
		goto init_error;
	rc = lock_ops->olo_lock_get_sync(&obj, &req, NULL);
	if (rc != 0)
		goto get_error;
	rc = open_entity(&obj.ob_entity);
	if (entity_sm_state(&obj) != M0_CLOVIS_ES_OPEN || rc != 0) {
		m0_clovis_obj_lock_put(&req);
		goto get_error;
	}

	/* Create the read request */
	rc = m0_clovis_obj_op(&obj, M0_CLOVIS_OC_READ, &ext,
			      &data, &attr, 0, &ops[0]);
	if (rc != 0) {
		m0_clovis_obj_lock_put(&req);
		M0_ERR(rc);
		goto get_error;
	}
	M0_ASSERT(ops[0]->op_sm.sm_rc == 0);
	m0_clovis_op_launch(ops, 1);

	/* wait */
	rc = m0_clovis_op_wait(ops[0],
			       M0_BITS(M0_CLOVIS_OS_FAILED,
				       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);
	M0_ASSERT(rc == 0);
	M0_ASSERT(ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);
	M0_ASSERT(ops[0]->op_sm.sm_rc == 0);
	lock_ops->olo_lock_put(&req);
	if (dest != NULL) {
		fp = fopen(dest, "w");
		if (fp == NULL) {
			rc = -EPERM;
			goto cleanup;
		}
		for (i = 0; i < block_count; ++i) {
			bytes_read += fwrite(data.ov_buf[i], sizeof(char),
					     data.ov_vec.v_count[i], fp);
		}
		fclose(fp);
		if (bytes_read != block_count * block_size) {
			rc = -EIO;
			goto cleanup;
		}
	} else {
	/* putchar the output */
		for (i = 0; i < block_count; ++i) {
			for (j = 0; j < data.ov_vec.v_count[i]; ++j)
				putchar(((char *)data.ov_buf[i])[j]);
		}
	}

cleanup:

	/* fini and release */
	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
get_error:
	lock_ops->olo_lock_fini(&obj);
init_error:
	m0_clovis_entity_fini(&obj.ob_entity);
	clovis_cleanup_vecs(&data, &attr, &ext);
	return rc;
}

int clovis_truncate(struct m0_clovis_container *clovis_container,
		    struct m0_uint128 id, uint32_t block_size,
		    uint32_t trunc_count, uint32_t trunc_len, bool take_locks)
{
	int                               i;
	int                               rc;
	struct m0_clovis_op              *ops[1] = {NULL};
	struct m0_clovis_obj              obj;
	uint64_t                          last_index;
	struct m0_indexvec                ext;
	struct m0_clovis                 *clovis_instance;
	struct m0_clovis_rm_lock_req      req;
	const struct clovis_obj_lock_ops *lock_ops;

	lock_ops = take_locks ? &lock_enabled_ops : &lock_disabled_ops;
	rc = m0_indexvec_alloc(&ext, trunc_len);
	if (rc != 0)
		return rc;

	last_index = trunc_count * block_size;
	for (i = 0; i < trunc_len; ++i) {
		ext.iv_index[i] = last_index;
		ext.iv_vec.v_count[i] = block_size;
		last_index += block_size;
	}

	clovis_instance = clovis_container->co_realm.re_instance;
	/* Read the requisite number of blocks from the entity */
	M0_SET0(&obj);
	m0_clovis_obj_init(&obj, &clovis_container->co_realm, &id,
			   m0_clovis_layout_id(clovis_instance));

	rc = lock_ops->olo_lock_init(&obj, NULL);
	if (rc != 0)
		goto init_error;
	rc = lock_ops->olo_lock_get_sync(&obj, &req, NULL);
	if (rc != 0)
		goto get_error;

	rc = open_entity(&obj.ob_entity);
	if (entity_sm_state(&obj) != M0_CLOVIS_ES_OPEN || rc != 0)
		goto open_entity_error;

	/* Create the read request */
	rc = m0_clovis_obj_op(&obj, M0_CLOVIS_OC_FREE, &ext,
			      NULL, NULL, 0, &ops[0]);
	if (rc != 0) {
		M0_ERR(rc);
		goto open_entity_error;
	}
	M0_ASSERT(ops[0]->op_sm.sm_rc == 0);

	m0_clovis_op_launch(ops, 1);

	/* wait */
	rc = m0_clovis_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);
	M0_ASSERT(rc == 0);
	M0_ASSERT(ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);
	M0_ASSERT(ops[0]->op_sm.sm_rc == 0);

	/* fini and release */
	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
open_entity_error:
	lock_ops->olo_lock_put(&req);
get_error:
	lock_ops->olo_lock_fini(&obj);
init_error:
	m0_clovis_entity_fini(&obj.ob_entity);
	m0_indexvec_free(&ext);
	return rc;
}

int clovis_unlink(struct m0_clovis_container *clovis_container,
		  struct m0_uint128 id, bool take_locks)
{
	int                               rc;
	struct m0_clovis_op              *ops[1] = {NULL};
	struct m0_clovis_obj              obj;
	struct m0_clovis                 *clovis_instance;
	struct m0_clovis_rm_lock_req      req;
	const struct clovis_obj_lock_ops *lock_ops;

	clovis_instance = clovis_container->co_realm.re_instance;
	lock_ops = take_locks ? &lock_enabled_ops : &lock_disabled_ops;

	/* Delete an entity */
	M0_SET0(&obj);
	m0_clovis_obj_init(&obj, &clovis_container->co_realm, &id,
			   m0_clovis_layout_id(clovis_instance));
	rc = lock_ops->olo_lock_init(&obj, NULL);
	if (rc != 0)
		goto init_error;
	rc = lock_ops->olo_lock_get_sync(&obj, &req, NULL);
	if (rc != 0)
		goto get_error;
	rc = open_entity(&obj.ob_entity);
	if (entity_sm_state(&obj) != M0_CLOVIS_ES_OPEN || rc != 0)
		goto open_entity_error;

	m0_clovis_entity_delete(&obj.ob_entity, &ops[0]);
	m0_clovis_op_launch(ops, 1);
	rc = m0_clovis_op_wait(ops[0],
			       M0_BITS(M0_CLOVIS_OS_FAILED,
				       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);

	/* fini and release */
	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
open_entity_error:
	lock_ops->olo_lock_put(&req);
get_error:
	lock_ops->olo_lock_fini(&obj);
init_error:
	m0_clovis_entity_fini(&obj.ob_entity);
	return rc;
}

/*
 * XXX:The following functions are used by clovis_cc_cp_cat.c
 *     to perform concurrent IO.
 *     An array of files are passed and the threads use the files in the
 *     order in which they get the locks.
 * TODO:Reduce code duplication caused by the following functions.
 *      Find a way to incorporate the functionalities of the following
 *      functions, into existing helper functions.
 */
int clovis_write_cc(struct m0_clovis_container *clovis_container,
		    char **src, struct m0_uint128 id, int *index,
		    uint32_t block_size, uint32_t block_count)
{
	int                           rc;
	struct m0_indexvec            ext;
	struct m0_bufvec              data;
	struct m0_bufvec              attr;
	uint32_t                      bcount;
	uint64_t                      last_index;
	FILE                         *fp;
	struct m0_clovis_obj          obj;
	struct m0_clovis             *clovis_instance;
	struct m0_clovis_rm_lock_req  req;

	M0_SET0(&obj);
	clovis_instance = clovis_container->co_realm.re_instance;
	m0_clovis_obj_init(&obj, &clovis_container->co_realm, &id,
			   m0_clovis_layout_id(clovis_instance));
	rc = m0_clovis_obj_lock_init(&obj, NULL);
	if (rc != 0)
		goto init_error;
	rc = m0_clovis_obj_lock_get_sync(&obj, &req, NULL);
	if (rc != 0)
		goto get_error;
	fp = fopen(src[(*index)++], "r");
	if (fp == NULL) {
		rc = -EPERM;
		goto file_error;
	}
	rc = create_object(&obj.ob_entity);
	if (rc != 0)
		goto cleanup;

	last_index = 0;
	while (block_count > 0) {
		bcount = (block_count > CLOVIS_MAX_BLOCK_COUNT)?
			  CLOVIS_MAX_BLOCK_COUNT:block_count;
		rc = clovis_alloc_prepare_vecs(&ext, &data, &attr, bcount,
					       block_size, &last_index);
		if (rc != 0)
			goto cleanup;

		/* Read data from source file. */
		rc = read_data_from_file(fp, &data);
		M0_ASSERT(rc == bcount);

		/* Copy data to the object*/
		rc = write_data_to_object(&obj, &ext, &data, &attr);
		if (rc != 0) {
			fprintf(stderr, "Writing to object failed!\n");
			clovis_cleanup_vecs(&data, &attr, &ext);
			goto cleanup;
		}
		clovis_cleanup_vecs(&data, &attr, &ext);
		block_count -= bcount;
	}

	/* fini and release */
cleanup:
	fclose(fp);
file_error:
	m0_clovis_obj_lock_put(&req);
get_error:
	m0_clovis_obj_lock_fini(&obj);
init_error:
	m0_clovis_entity_fini(&obj.ob_entity);
	return rc;
}

int clovis_read_cc(struct m0_clovis_container *clovis_container,
		   struct m0_uint128 id, char **dest, int *index,
		   uint32_t block_size, uint32_t block_count)
{
	int                           i;
	int                           j;
	int                           rc;
	uint64_t                      last_index = 0;
	struct m0_clovis_op          *ops[1] = {NULL};
	struct m0_clovis_obj          obj;
	struct m0_indexvec            ext;
	struct m0_bufvec              data;
	struct m0_bufvec              attr;
	FILE                         *fp = NULL;
	struct m0_clovis             *clovis_instance;
	struct m0_clovis_rm_lock_req  req;

	rc = clovis_alloc_prepare_vecs(&ext, &data, &attr, block_count,
				       block_size, &last_index);
	if (rc != 0)
		return rc;
	clovis_instance = clovis_container->co_realm.re_instance;

	/* Read the requisite number of blocks from the entity */
	M0_SET0(&obj);
	m0_clovis_obj_init(&obj, &clovis_container->co_realm, &id,
			   m0_clovis_layout_id(clovis_instance));
	rc = m0_clovis_obj_lock_init(&obj, NULL);
	if (rc != 0)
		goto init_error;
	rc = m0_clovis_obj_lock_get_sync(&obj, &req, NULL);
	if (rc != 0)
		goto get_error;
	rc = open_entity(&obj.ob_entity);
	if (entity_sm_state(&obj) != M0_CLOVIS_ES_OPEN || rc != 0) {
		m0_clovis_obj_lock_put(&req);
		goto get_error;
	}

	/* Create the read request */
	rc = m0_clovis_obj_op(&obj, M0_CLOVIS_OC_READ, &ext,
			 &data, &attr, 0, &ops[0]);
	if (rc != 0) {
		m0_clovis_obj_lock_put(&req);
		M0_ERR(rc);
		goto get_error;
	}
	M0_ASSERT(ops[0]->op_sm.sm_rc == 0);
	m0_clovis_op_launch(ops, 1);

	/* wait */
	rc = m0_clovis_op_wait(ops[0],
			       M0_BITS(M0_CLOVIS_OS_FAILED,
				       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER);
	M0_ASSERT(rc == 0);
	M0_ASSERT(ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);
	M0_ASSERT(ops[0]->op_sm.sm_rc == 0);
	m0_clovis_obj_lock_put(&req);
	if (dest != NULL) {
		fp = fopen(dest[(*index)++], "w");
		if (fp == NULL) {
			rc = -EPERM;
			goto cleanup;
		}
		for (i = 0; i < block_count; ++i) {
			rc = fwrite(data.ov_buf[i], sizeof(char),
				    data.ov_vec.v_count[i], fp);
		}
		fclose(fp);
		if (rc != block_count) {
			rc = -1;
			goto cleanup;
		}
	} else {
	/* putchar the output */
		for (i = 0; i < block_count; ++i) {
			for (j = 0; j < data.ov_vec.v_count[i]; ++j)
				putchar(((char *)data.ov_buf[i])[j]);
		}
	}

	/* fini and release */
cleanup:
	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
get_error:
	m0_clovis_obj_lock_fini(&obj);
init_error:
	m0_clovis_entity_fini(&obj.ob_entity);
	clovis_cleanup_vecs(&data, &attr, &ext);
	return rc;
}

int clovis_utility_args_init(int argc, char **argv,
			     struct clovis_utility_param *params,
			     struct m0_idx_dix_config *dix_conf,
			     struct m0_clovis_config *clovis_conf,
			     void (*utility_usage) (FILE*, char*))
{
        int      option_index = 0;
        uint32_t temp;
        int      c;

	M0_SET0(params);
	params->cup_id = M0_CLOVIS_ID_APP;
	params->cup_n_obj = 1;
	params->cup_take_locks = false;
	clovis_conf->cc_is_read_verify = false;
	clovis_conf->cc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	clovis_conf->cc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;

	/*
	 * TODO This arguments parsing is common for all the clovis utilities.
	 * Every option here is not supported by every utility, for ex-
	 * block_count and block_size are not supported by c0unlink and
	 * c0touch. So if someone uses '-c' or '-s' with either of those
	 * without referring to help, (s)he won't get any error regarding
	 * 'unsupported option'.
	 * This need to be handle.
	 */
	static struct option l_opts[] = {
				{"local",        required_argument, NULL, 'l'},
				{"ha",           required_argument, NULL, 'H'},
				{"profile",      required_argument, NULL, 'p'},
				{"process",      required_argument, NULL, 'P'},
				{"object",       required_argument, NULL, 'o'},
				{"block-size",   required_argument, NULL, 's'},
				{"block-count",  required_argument, NULL, 'c'},
				{"trunc-len",    required_argument, NULL, 't'},
				{"layout-id",    required_argument, NULL, 'L'},
				{"n_obj",        required_argument, NULL, 'n'},
				{"msg_size",     required_argument, NULL, 'S'},
				{"min_queue",    required_argument, NULL, 'q'},
				{"enable-locks", no_argument,       NULL, 'e'},
				{"update-mode",  no_argument,       NULL, 'u'},
				{"read-verify",  no_argument,       NULL, 'r'},
				{"help",         no_argument,       NULL, 'h'},
				{0,             0,                 0,     0 }};

        while ((c = getopt_long(argc, argv, ":l:H:p:P:o:s:c:t:L:n:S:q:eurh",
				l_opts, &option_index)) != -1)
	{
		switch (c) {
			case 'l': clovis_conf->cc_local_addr = optarg;
				  continue;
			case 'H': clovis_conf->cc_ha_addr = optarg;
				  continue;
			case 'p': clovis_conf->cc_profile = optarg;
				  continue;
			case 'P': clovis_conf->cc_process_fid = optarg;
				  continue;
			case 'o': if (clovis_obj_id_sscanf(optarg,
							   &params->cup_id) < 0) {
					utility_usage(stderr, basename(argv[0]));
					exit(EXIT_FAILURE);
				  } else if (!clovis_entity_id_is_valid(
					     &params->cup_id)) {
					utility_usage(stderr, basename(argv[0]));
					exit(EXIT_FAILURE);
				  }
				  continue;
			case 's': if (m0_bcount_get(optarg,
						    &params->cup_block_size) < 0) {
					utility_usage(stderr, basename(argv[0]));
					exit(EXIT_FAILURE);
				  }
				  continue;
			case 'c': if (m0_bcount_get(optarg,
						    &params->cup_block_count) < 0) {
					utility_usage(stderr, basename(argv[0]));
					exit(EXIT_FAILURE);
				  }
				  continue;
			case 't': if (m0_bcount_get(optarg,
						    &params->cup_trunc_len) < 0) {
					utility_usage(stderr, basename(argv[0]));
					exit(EXIT_FAILURE);
				  }
				  continue;
			case 'L': clovis_conf->cc_layout_id = atoi(optarg);
				  if (clovis_conf->cc_layout_id <= 0 ||
					clovis_conf->cc_layout_id >= 15) {
					utility_usage(stderr,
						      basename(argv[0]));
					exit(EXIT_FAILURE);
				  }
				  continue;
			case 'n': params->cup_n_obj = atoi(optarg);
				  continue;
			case 'e': params->cup_take_locks = true;
				  continue;
			case 'u': params->cup_update_mode = true;
				  continue;
			case 'r': clovis_conf->cc_is_read_verify = true;
				  continue;
			case 'S': temp = atoi(optarg);
				  clovis_conf->cc_max_rpc_msg_size = temp;
				  continue;
			case 'q': temp = atoi(optarg);
				  clovis_conf->cc_tm_recv_queue_min_len = temp;
				  continue;
			case 'h': utility_usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			case '?': fprintf(stderr, "Unsupported option '%c'\n",
					  optopt);
				  utility_usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			case ':': fprintf(stderr, "No argument given for '%c'\n",
				          optopt);
				  utility_usage(stderr, basename(argv[0]));
				  exit(EXIT_FAILURE);
			default:  fprintf(stderr, "Unsupported option '%c'\n", c);
		}
	}

	clovis_conf->cc_is_oostore            = true;
	clovis_conf->cc_idx_service_conf      = dix_conf;
	dix_conf->kc_create_meta              = false;
	clovis_conf->cc_idx_service_id        = M0_CLOVIS_IDX_DIX;

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
