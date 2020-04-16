/*
 * COPYRIGHT 2015 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#pragma once

#ifndef __S3_SERVER_S3_CLOVIS_CONTEXT_H__
#define __S3_SERVER_S3_CLOVIS_CONTEXT_H__

#include "s3_common.h"
#include "s3_log.h"

EXTERN_C_BLOCK_BEGIN

#include "mero/init.h"
#include "module/instance.h"

#include "clovis/clovis.h"

EXTERN_C_BLOCK_END

#include "s3_memory_pool.h"

struct s3_clovis_obj_context {
  struct m0_clovis_obj *objs;
  size_t obj_count;
};

// To create a basic clovis operation
struct s3_clovis_op_context {
  struct m0_clovis_op **ops;
  struct m0_clovis_op_ops *cbs;
  size_t op_count;
};

struct s3_clovis_rw_op_context {
  struct m0_indexvec *ext;
  struct m0_bufvec *data;
  struct m0_bufvec *attr;
  size_t unit_size;
  bool allocated_bufs;  // Do we own data bufs and we should free?
};

struct s3_clovis_idx_context {
  struct m0_clovis_idx *idx;
  size_t idx_count;
};

struct s3_clovis_idx_op_context {
  struct m0_clovis_op **ops;
  struct m0_clovis_op *sync_op;
  struct m0_clovis_op_ops *cbs;
  size_t op_count;
};

struct s3_clovis_kvs_op_context {
  struct m0_bufvec *keys;
  struct m0_bufvec *values;
  int *rcs;  // per key return status array
};

struct s3_clovis_obj_context *create_obj_context(size_t count);
int free_obj_context(struct s3_clovis_obj_context *ctx);

struct s3_clovis_op_context *create_basic_op_ctx(size_t op_count);
int free_basic_op_ctx(struct s3_clovis_op_context *ctx);

struct s3_clovis_rw_op_context *create_basic_rw_op_ctx(
    size_t clovis_buf_count, size_t unit_size, bool allocate_bufs = true);
int free_basic_rw_op_ctx(struct s3_clovis_rw_op_context *ctx);

struct s3_clovis_idx_context *create_idx_context(size_t idx_count);
int free_idx_context(struct s3_clovis_idx_context *ctx);

struct s3_clovis_idx_op_context *create_basic_idx_op_ctx(int op_count);
int free_basic_idx_op_ctx(struct s3_clovis_idx_op_context *ctx);

struct s3_clovis_kvs_op_context *create_basic_kvs_op_ctx(int no_of_keys);
int free_basic_kvs_op_ctx(struct s3_clovis_kvs_op_context *ctx);

struct m0_bufvec *index_bufvec_alloc(int nr);
void index_bufvec_free(struct m0_bufvec *bv);

#endif
