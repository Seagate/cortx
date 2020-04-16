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

#ifndef __S3_SERVER_FAKE_CLOVIS_REDIS__H__
#define __S3_SERVER_FAKE_CLOVIS_REDIS__H__

#include "s3_clovis_context.h"
#include "s3_log.h"
#include "s3_option.h"

#include <memory>

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>

class S3FakeClovisRedisKvs {
 private:
  redisAsyncContext *redis_ctx = nullptr;

 private:
  static std::unique_ptr<S3FakeClovisRedisKvs> inst;

  static void connect_cb(const redisAsyncContext *c, int status) {
    s3_log(S3_LOG_DEBUG, "", "Entering");
    if (status != REDIS_OK) {
      auto opts = S3Option::get_instance();
      s3_log(S3_LOG_FATAL, "", "Redis@%s:%d connect error: %s",
             opts->get_redis_srv_addr().c_str(), opts->get_redis_srv_port(),
             c->errstr);
    }

    s3_log(S3_LOG_DEBUG, "", "Exiting status %d", status);
  }

 private:
  S3FakeClovisRedisKvs() {
    s3_log(S3_LOG_DEBUG, "", "Entering");
    auto opts = S3Option::get_instance();
    redis_ctx = redisAsyncConnect(opts->get_redis_srv_addr().c_str(),
                                  opts->get_redis_srv_port());
    if (redis_ctx->err) {
      s3_log(S3_LOG_FATAL, "", "Redis alloc error %s", redis_ctx->errstr);
    }

    redisLibeventAttach(redis_ctx, opts->get_eventbase());
    redisAsyncSetConnectCallback(redis_ctx, connect_cb);
    s3_log(S3_LOG_DEBUG, "", "Exiting");
  }

  void close() {
    if (redis_ctx) {
      redisAsyncFree(redis_ctx);
      redis_ctx = nullptr;
    }
  }

 public:
  ~S3FakeClovisRedisKvs() { close(); }

  void kv_read(struct m0_clovis_op *op);

  void kv_next(struct m0_clovis_op *op);

  void kv_write(struct m0_clovis_op *op);

  void kv_del(struct m0_clovis_op *op);

 public:
  static S3FakeClovisRedisKvs *instance() {
    if (!inst) {
      inst = std::unique_ptr<S3FakeClovisRedisKvs>(new S3FakeClovisRedisKvs());
    }

    return inst.get();
  }

  static void destroy_instance() {
    if (inst) {
      inst->close();
    }
  }
};

#endif
