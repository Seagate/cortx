/*
 * COPYRIGHT 2019 SEAGATE LLC
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
 * Original author:  Dmitrii Surnin <dmitrii.surnin@seagate.com>
 * Original creation date: 19-September-2019
 */

#pragma once

#ifndef __S3_SERVER_FAKE_CLOVIS_REDIS_INTERNAL__H__
#define __S3_SERVER_FAKE_CLOVIS_REDIS_INTERNAL__H__

#include "s3_fake_clovis_redis_kvs.h"
#include "s3_clovis_rw_common.h"

// key and val are delimited with zero byte
// so key is just a begging of the buf
char *parse_key(char *kv, size_t kv_size);

// val starts after key and zero byte
char *parse_val(char *kv, size_t kv_size);

typedef struct {
  size_t len;
  char *buf;
} redis_key;

// key and val separated with zero byte
redis_key prepare_rkey(const char *key, size_t klen, const char *val,
                       size_t vlen);

// converts key to form "[key\xFF"
// incl: true - [; false - (;
// z: true - 0xFF; false - nothing added
redis_key prepare_border(const char *str, size_t slen, bool incl, bool z);

typedef struct {
  struct s3_clovis_context_obj *prev_ctx;  // previous m0_clovis_op::op_datum
  int async_ops_cnt;  // number of async ops run for current m0_clovis_op
  int replies_cnt;    // number of replies received so far; replies_cnt ==
                      // async_ops_cnt means op finished
  bool had_error;     // if some of resps failed

  // on next_kv operation mero interface allows skip or include search initial
  // key. on s3server side we always skip it, so in result set it is not incl.
  // due to range requests and key-value concatenation it is hard to filter
  // initial value with single req, so it should be filtered manually
  char *skip_value;  // if first value should not be included
  size_t skip_size;  // skip_value buf size;
} s3_redis_context_obj;

typedef struct {
  int processing_idx;       // idx of the processing elem inside m0_bufvec
  struct m0_clovis_op *op;  // current op
} s3_redis_async_ctx;

enum RedisRequestState {
  REPL_ERR,      // reply cannot be processed
  REPL_DONE,     // reply processing finished
  REPL_CONTINUE  // processing of the reply object could be continued
};

// check whether libhiredis callback params are valid
// glob_redis_ctx - contex for redis async ops
// async_redis_reply - redis-server command reply data
// privdata - user context
int redis_reply_check(redisAsyncContext *glob_redis_ctx,
                      void *async_redis_reply, void *privdata,
                      std::vector<int> const &exp_types);

typedef void op_stable_cb(struct m0_clovis_op *op);
typedef void op_failed_cb(struct m0_clovis_op *op);
void finalize_op(struct m0_clovis_op *op,
                 op_stable_cb stable = s3_clovis_op_stable,
                 op_failed_cb failed = s3_clovis_op_failed);

void kv_status_cb(redisAsyncContext *glob_redis_ctx, void *async_redis_reply,
                  void *privdata);

void kv_read_cb(redisAsyncContext *glob_redis_ctx, void *async_redis_reply,
                void *privdata);

void kv_next_cb(redisAsyncContext *glob_redis_ctx, void *async_redis_reply,
                void *privdata);

#endif
