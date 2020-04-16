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

#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>

#include "s3_fake_clovis_redis_kvs.h"
#include "s3_fake_clovis_redis_kvs_internal.h"
#include "s3_clovis_kvs_reader.h"
#include "s3_clovis_kvs_writer.h"
#include "s3_clovis_rw_common.h"

std::unique_ptr<S3FakeClovisRedisKvs> S3FakeClovisRedisKvs::inst;

void finalize_op(struct m0_clovis_op *op, op_stable_cb stable,
                 op_failed_cb failed) {
  s3_log(S3_LOG_DEBUG, "", "Entering");
  if (!op) return;

  s3_redis_context_obj *redis_ctx = (s3_redis_context_obj *)op->op_datum;
  if (redis_ctx->async_ops_cnt > redis_ctx->replies_cnt)
    return;  // not all replies collected yet

  op->op_datum = (void *)redis_ctx->prev_ctx;
  if (!redis_ctx->had_error) {
    stable(op);
  } else {
    op->op_rc = -ETIMEDOUT;  // fake network failure
    redis_ctx->prev_ctx->is_fake_failure = 1;
    failed(op);
  }

  free(redis_ctx);

  s3_log(S3_LOG_DEBUG, "", "Exiting");
}

// key and val are delimited with zero byte
// so key is just a begging of the buf
char *parse_key(char *kv, size_t kv_size) { return kv; }

// val starts after key and zero byte
char *parse_val(char *kv, size_t kv_size) {
  if (!kv) {
    return nullptr;
  }

  size_t klen = strlen(kv);
  if (klen + 1 < kv_size) {
    return kv + klen + 1;
  }
  return nullptr;
}

// key and val separated with zero byte
redis_key prepare_rkey(const char *key, size_t klen, const char *val,
                       size_t vlen) {
  size_t len = klen + vlen + 1 /*zero byte separator*/
               + 1 /*final byte*/;
  char *rkey = (char *)calloc(len, sizeof(char));
  if (rkey == nullptr) {
    s3_log(S3_LOG_FATAL, "", "rkey calloc failed");
  }
  memcpy(rkey, key, klen);
  memcpy(rkey + klen + 1, val, vlen);

  return {.len = len, .buf = rkey};
}

// converts key to form "[key\xFF"
// incl: true - [; false - (;
// z: true - 0xFF; false - nothing added
redis_key prepare_border(const char *str, size_t slen, bool incl, bool z) {
  size_t len = slen + 1 /*incl byte*/ + (size_t)z /*z byte*/;
  char *brdr = (char *)calloc(len, sizeof(char));
  if (brdr == nullptr) {
    s3_log(S3_LOG_FATAL, "", "brdr calloc failed");
  }
  brdr[0] = incl ? '[' : '(';
  memcpy(brdr + 1, str, slen);
  if (z) {
    // 0xFF is not allowed in utf-8, so it should be fine to use it
    brdr[1 + slen] = 0xFF;
  }

  return {.len = len, .buf = brdr};
}

// check whether libhiredis callback params are valid
// glob_redis_ctx - contex for redis async ops
// async_redis_reply - redis-server command reply data
// privdata - user context
int redis_reply_check(redisAsyncContext *glob_redis_ctx,
                      void *async_redis_reply, void *privdata,
                      std::vector<int> const &exp_types) {

  s3_redis_async_ctx *actx = (s3_redis_async_ctx *)privdata;
  if (!actx) {
    s3_log(S3_LOG_WARN, "", "Privdata is NULL. exit");
    return REPL_ERR;
  }

  s3_redis_context_obj *redis_ctx = (s3_redis_context_obj *)actx->op->op_datum;
  ++redis_ctx->replies_cnt;

  if ((glob_redis_ctx && glob_redis_ctx->err) || !async_redis_reply) {
    s3_log(S3_LOG_WARN, "", "Redis reply invalid");
    redis_ctx->had_error = true;
    return REPL_DONE;
  }

  redisReply *reply = (redisReply *)async_redis_reply;

  // check expected type
  auto it_end = std::end(exp_types);
  auto it_beg = std::begin(exp_types);
  if (std::find(it_beg, it_end, reply->type) == it_end) {
    s3_log(S3_LOG_WARN, "", "Redis reply type error. Cur type %d", reply->type);
    redis_ctx->had_error = true;
    return REPL_DONE;
  }

  return REPL_CONTINUE;
}

// libhiredis callback for read command
// glob_redis_ctx - contex for redis async ops
// async_redis_reply - redis-server command reply data
// privdata - user context
void kv_read_cb(redisAsyncContext *glob_redis_ctx, void *async_redis_reply,
                void *privdata) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  // during the destruction redisAsyncContext will be null
  // in this case do nothing and simply return
  if (!glob_redis_ctx) {
    s3_log(S3_LOG_DEBUG, "", "redisAsyncContext is null, do nothing");
    return;
  }
  int repl_chk = redis_reply_check(glob_redis_ctx, async_redis_reply, privdata,
                                   {REDIS_REPLY_ARRAY});
  if (repl_chk == REPL_ERR) {
    s3_log(S3_LOG_FATAL, "", "Cannot process redis reply");
  }

  redisReply *reply = (redisReply *)async_redis_reply;
  s3_redis_async_ctx *actx = (s3_redis_async_ctx *)privdata;
  s3_redis_context_obj *redis_ctx = (s3_redis_context_obj *)actx->op->op_datum;

  if (repl_chk == REPL_CONTINUE) {
    S3ClovisKVSReaderContext *read_ctx =
        (S3ClovisKVSReaderContext *)redis_ctx->prev_ctx->application_context;
    struct s3_clovis_kvs_op_context *kv = read_ctx->get_clovis_kvs_op_ctx();

    kv->rcs[actx->processing_idx] = -ENOENT;
    actx->op->op_rc = -ENOENT;

    redisReply *str_reply =
        (reply->elements == 1) ? reply->element[0] : nullptr;
    if (str_reply && str_reply->type == REDIS_REPLY_STRING) {
      char *val = parse_val(str_reply->str, str_reply->len);
      kv->rcs[actx->processing_idx] = 0;
      actx->op->op_rc = 0;
      kv->values->ov_vec.v_count[actx->processing_idx] = strlen(val);
      kv->values->ov_buf[actx->processing_idx] = strdup(val);
    }
  }

  finalize_op(actx->op);
  free(actx);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3FakeClovisRedisKvs::kv_read(struct m0_clovis_op *op) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  struct s3_clovis_context_obj *ctx =
      (struct s3_clovis_context_obj *)op->op_datum;
  S3ClovisKVSReaderContext *read_ctx =
      (S3ClovisKVSReaderContext *)ctx->application_context;
  struct s3_clovis_kvs_op_context *kv = read_ctx->get_clovis_kvs_op_ctx();
  int cnt = kv->keys->ov_vec.v_nr;

  s3_redis_context_obj *new_ctx =
      (s3_redis_context_obj *)calloc(1, sizeof(s3_redis_context_obj));
  new_ctx->prev_ctx = ctx;
  new_ctx->async_ops_cnt = cnt;
  op->op_datum = (void *)new_ctx;

  for (int i = 0; i < cnt; ++i) {
    s3_redis_async_ctx *actx =
        (s3_redis_async_ctx *)calloc(1, sizeof(s3_redis_async_ctx));
    actx->processing_idx = i;
    actx->op = op;

    redis_key min_b = prepare_border((char *)kv->keys->ov_buf[i],
                                     kv->keys->ov_vec.v_count[i], true, false);
    redis_key max_b = prepare_border((char *)kv->keys->ov_buf[i],
                                     kv->keys->ov_vec.v_count[i], false, true);
    int ret =
        redisAsyncCommand(this->redis_ctx, kv_read_cb, (void *)actx,
                          "ZRANGEBYLEX %b %b %b LIMIT 0 1",
                          &op->op_entity->en_id, sizeof(op->op_entity->en_id),
                          min_b.buf, min_b.len, max_b.buf, max_b.len);
    if (ret != REDIS_OK) {
      s3_log(S3_LOG_FATAL, "", "Redis command cannot be scheduled");
    }
    free(min_b.buf);
    free(max_b.buf);
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

// libhiredis callback for range command
// glob_redis_ctx - contex for redis async ops
// async_redis_reply - redis-server command reply data
// privdata - user context
void kv_next_cb(redisAsyncContext *glob_redis_ctx, void *async_redis_reply,
                void *privdata) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  // during the destruction redisAsyncContext will be null
  // in this case do nothing and simply return
  if (!glob_redis_ctx) {
    s3_log(S3_LOG_DEBUG, "", "redisAsyncContext is null, do nothing");
    return;
  }
  int repl_chk = redis_reply_check(glob_redis_ctx, async_redis_reply, privdata,
                                   {REDIS_REPLY_ARRAY});
  if (repl_chk == REPL_ERR) {
    s3_log(S3_LOG_FATAL, "", "Cannot process redis reply");
  }

  redisReply *reply = (redisReply *)async_redis_reply;
  s3_redis_async_ctx *actx = (s3_redis_async_ctx *)privdata;
  s3_redis_context_obj *redis_ctx = (s3_redis_context_obj *)actx->op->op_datum;

  if (repl_chk == REPL_CONTINUE) {
    S3ClovisKVSReaderContext *read_ctx =
        (S3ClovisKVSReaderContext *)redis_ctx->prev_ctx->application_context;
    struct s3_clovis_kvs_op_context *kv = read_ctx->get_clovis_kvs_op_ctx();

    actx->op->op_rc = -ENOENT;
    size_t cnt = kv->values->ov_vec.v_nr;
    for (size_t i = 0; i < cnt; ++i) {
      kv->rcs[i] = -ENOENT;
    }

    size_t repl_idx = 0;
    size_t result_idx = 0;
    redisReply *tmp_reply = (reply->elements > 0) ? reply->element[0] : nullptr;

    if (redis_ctx->skip_size > 0 && tmp_reply &&
        tmp_reply->type == REDIS_REPLY_STRING) {
      std::string key(parse_key(tmp_reply->str, tmp_reply->len));
      std::string skip(redis_ctx->skip_value, redis_ctx->skip_size);

      s3_log(S3_LOG_DEBUG, "", "check skipping key %s skip %s", key.c_str(),
             skip.c_str());

      if (key == skip) {
        s3_log(S3_LOG_DEBUG, "", "skipping");
        ++repl_idx;
        tmp_reply =
            (repl_idx < reply->elements) ? reply->element[repl_idx] : nullptr;
      }
    }

    while (tmp_reply && tmp_reply->type == REDIS_REPLY_STRING &&
           result_idx < cnt) {
      char *key = parse_key(tmp_reply->str, tmp_reply->len);
      char *val = parse_val(tmp_reply->str, tmp_reply->len);

      kv->rcs[result_idx] = 0;
      actx->op->op_rc = 0;

      kv->keys->ov_vec.v_count[result_idx] = strlen(key);
      kv->keys->ov_buf[result_idx] = strdup(key);

      kv->values->ov_vec.v_count[result_idx] = strlen(val);
      kv->values->ov_buf[result_idx] = strdup(val);

      s3_log(S3_LOG_DEBUG, "", "Got k:>%s v:>%s\n", key, val);

      ++repl_idx;
      ++result_idx;
      if (reply->type == REDIS_REPLY_ARRAY && repl_idx < reply->elements) {
        tmp_reply = reply->element[repl_idx];
      } else {
        break;
      }
    }
  }

  finalize_op(actx->op);
  free(actx);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3FakeClovisRedisKvs::kv_next(struct m0_clovis_op *op) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  struct s3_clovis_context_obj *ctx =
      (struct s3_clovis_context_obj *)op->op_datum;
  S3ClovisKVSReaderContext *read_ctx =
      (S3ClovisKVSReaderContext *)ctx->application_context;
  struct s3_clovis_kvs_op_context *kv = read_ctx->get_clovis_kvs_op_ctx();

  s3_redis_context_obj *new_ctx =
      (s3_redis_context_obj *)calloc(1, sizeof(s3_redis_context_obj));
  new_ctx->prev_ctx = ctx;
  new_ctx->async_ops_cnt = 1;
  op->op_datum = (void *)new_ctx;

  s3_redis_async_ctx *actx =
      (s3_redis_async_ctx *)calloc(1, sizeof(s3_redis_async_ctx));
  actx->processing_idx = 0;
  actx->op = op;

  int cnt = kv->keys->ov_vec.v_nr;  // number of vals to return

  if (kv->keys->ov_vec.v_count[0] > 0) {
    // first item is not empty, so start from it

    new_ctx->skip_value = (char *)kv->keys->ov_buf[0];
    new_ctx->skip_size = kv->keys->ov_vec.v_count[0];

    redis_key min_b = prepare_border((char *)kv->keys->ov_buf[0],
                                     kv->keys->ov_vec.v_count[0], false, false);
    int ret = redisAsyncCommand(
        this->redis_ctx, kv_next_cb, (void *)actx,
        "ZRANGEBYLEX %b %b + LIMIT 0 %d", &op->op_entity->en_id,
        sizeof(op->op_entity->en_id), min_b.buf, min_b.len, (cnt + 1));
    if (ret != REDIS_OK) {
      s3_log(S3_LOG_FATAL, "", "Redis command cannot be scheduled");
    }
    free(min_b.buf);

    kv->keys->ov_vec.v_count[0] = 0;
    // do not free - done in upper level
    kv->keys->ov_buf[0] = nullptr;

  } else {  // starting from the first val
    int ret = redisAsyncCommand(this->redis_ctx, kv_next_cb, (void *)actx,
                                "ZRANGEBYLEX %b - + LIMIT 0 %d",
                                &op->op_entity->en_id,
                                sizeof(op->op_entity->en_id), cnt);
    if (ret != REDIS_OK) {
      s3_log(S3_LOG_FATAL, "", "Redis command cannot be scheduled");
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

// libhiredis callback for write/delete command
// glob_redis_ctx - contex for redis async ops
// async_redis_reply - redis-server command reply data
// privdata - user context
void kv_status_cb(redisAsyncContext *glob_redis_ctx, void *async_redis_reply,
                  void *privdata) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  // during the destruction redisAsyncContext will be null
  // in this case do nothing and simply return
  if (!glob_redis_ctx) {
    s3_log(S3_LOG_DEBUG, "", "redisAsyncContext is null, do nothing");
    return;
  }
  int repl_chk = redis_reply_check(glob_redis_ctx, async_redis_reply, privdata,
                                   {REDIS_REPLY_INTEGER});
  if (repl_chk == REPL_ERR) {
    s3_log(S3_LOG_FATAL, "", "Cannot process redis reply");
  }

  redisReply *reply = (redisReply *)async_redis_reply;
  s3_redis_async_ctx *actx = (s3_redis_async_ctx *)privdata;
  s3_redis_context_obj *redis_ctx = (s3_redis_context_obj *)actx->op->op_datum;

  if (repl_chk == REPL_CONTINUE) {
    S3AsyncClovisKVSWriterContext *write_ctx =
        (S3AsyncClovisKVSWriterContext *)
        redis_ctx->prev_ctx->application_context;

    struct s3_clovis_kvs_op_context *kv = write_ctx->get_clovis_kvs_op_ctx();
    // reply->type is REDIS_REPLY_INTEGER)
    s3_log(S3_LOG_INFO, "", "Reply integer :>%lld", reply->integer);
    kv->rcs[actx->processing_idx] = (reply->integer > 0) ? 0 : -ENOENT;
    actx->op->op_rc = kv->rcs[actx->processing_idx];
  }

  finalize_op(actx->op);
  free(actx);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

static void schedule_delete_key_op(redisAsyncContext *ac,
                                   struct m0_uint128 const &oid, char *key,
                                   size_t key_len, redisCallbackFn *op_cb,
                                   void *privdata) {
  redis_key min_b = prepare_border(key, key_len, true, false);
  redis_key max_b = prepare_border(key, key_len, false, true);

  int ret = redisAsyncCommand(ac, op_cb, privdata, "ZREMRANGEBYLEX %b %b %b",
                              &oid, sizeof(struct m0_uint128), min_b.buf,
                              min_b.len, max_b.buf, max_b.len);
  if (ret != REDIS_OK) {
    s3_log(S3_LOG_FATAL, "", "Redis command cannot be scheduled");
  }
  free(min_b.buf);
  free(max_b.buf);
}

void S3FakeClovisRedisKvs::kv_write(struct m0_clovis_op *op) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  struct s3_clovis_context_obj *ctx =
      (struct s3_clovis_context_obj *)op->op_datum;
  S3AsyncClovisKVSWriterContext *write_ctx =
      (S3AsyncClovisKVSWriterContext *)ctx->application_context;
  struct s3_clovis_kvs_op_context *kv = write_ctx->get_clovis_kvs_op_ctx();
  int cnt = kv->keys->ov_vec.v_nr;

  s3_redis_context_obj *new_ctx =
      (s3_redis_context_obj *)calloc(1, sizeof(s3_redis_context_obj));
  new_ctx->prev_ctx = ctx;
  new_ctx->async_ops_cnt = cnt;
  op->op_datum = (void *)new_ctx;

  for (int i = 0; i < cnt; ++i) {
    s3_redis_async_ctx *actx =
        (s3_redis_async_ctx *)calloc(1, sizeof(s3_redis_async_ctx));
    actx->processing_idx = i;
    actx->op = op;

    // since we store concatenating key-val we cannot simply put/update value
    // we need to be sure there are no values with the same prefix key
    // so simply delete by key
    schedule_delete_key_op(this->redis_ctx, op->op_entity->en_id,
                           (char *)kv->keys->ov_buf[i],
                           kv->keys->ov_vec.v_count[i], nullptr, nullptr);

    redis_key rkey = prepare_rkey(
        (char *)kv->keys->ov_buf[i], kv->keys->ov_vec.v_count[i],
        (char *)kv->values->ov_buf[i], kv->values->ov_vec.v_count[i]);

    int ret =
        redisAsyncCommand(this->redis_ctx, kv_status_cb, (void *)actx,
                          "ZADD %b 0 %b", &op->op_entity->en_id,
                          sizeof(op->op_entity->en_id), rkey.buf, rkey.len);
    if (ret != REDIS_OK) {
      s3_log(S3_LOG_FATAL, "", "Redis command cannot be scheduled");
    }
    free(rkey.buf);
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3FakeClovisRedisKvs::kv_del(struct m0_clovis_op *op) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  struct s3_clovis_context_obj *ctx =
      (struct s3_clovis_context_obj *)op->op_datum;
  S3AsyncClovisKVSWriterContext *write_ctx =
      (S3AsyncClovisKVSWriterContext *)ctx->application_context;
  struct s3_clovis_kvs_op_context *kv = write_ctx->get_clovis_kvs_op_ctx();
  int cnt = kv->keys->ov_vec.v_nr;

  s3_redis_context_obj *new_ctx =
      (s3_redis_context_obj *)calloc(1, sizeof(s3_redis_context_obj));
  new_ctx->prev_ctx = ctx;
  new_ctx->async_ops_cnt = cnt;
  op->op_datum = (void *)new_ctx;

  for (int i = 0; i < cnt; ++i) {
    s3_redis_async_ctx *actx =
        (s3_redis_async_ctx *)calloc(1, sizeof(s3_redis_async_ctx));
    actx->processing_idx = i;
    actx->op = op;

    schedule_delete_key_op(
        this->redis_ctx, op->op_entity->en_id, (char *)kv->keys->ov_buf[i],
        kv->keys->ov_vec.v_count[i], kv_status_cb, (void *)actx);
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
