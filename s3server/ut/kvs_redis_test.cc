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

#include "s3_test_utils.h"
#include "s3_fake_clovis_redis_kvs_internal.h"
#include "s3_clovis_kvs_reader.h"
#include "s3_clovis_kvs_writer.h"
#include "s3_clovis_rw_common.h"
#include "mock_mero_request_object.h"

TEST(RedisKvs, parse_key) {
  char kv[] = {'k', 'e', 'y', 0, 'v', 'a', 'l', 0};
  char* k = parse_key(kv, 8);
  EXPECT_EQ(strcmp(k, "key"), 0);

  k = parse_key(nullptr, 0);
  EXPECT_EQ(k, nullptr);

  std::string key_only("key_1");
  k = parse_key(const_cast<char*>(key_only.c_str()), key_only.length());
  EXPECT_EQ(strcmp(k, "key_1"), 0);
}

TEST(RedisKvs, parse_val) {
  char kv[] = {'k', 'e', 'y', 0, 'v', 'a', 'l', 0};
  char* v = parse_val(kv, 8);
  EXPECT_EQ(strcmp(v, "val"), 0);

  v = parse_val(nullptr, 0);
  EXPECT_EQ(v, nullptr);

  std::string key_only("key_1");
  v = parse_val(const_cast<char*>(key_only.c_str()), key_only.length());
  EXPECT_EQ(v, nullptr);
}

TEST(RedisKvs, prepare_rkey) {
  std::string k("key");
  std::string v("val");
  redis_key rk = prepare_rkey(k.c_str(), k.length(), v.c_str(), v.length());

  ASSERT_NE(rk.buf, nullptr);
  EXPECT_EQ(rk.len, k.length() + v.length() + 2);
  EXPECT_EQ(strcmp(parse_key(rk.buf, rk.len), k.c_str()), 0);
  EXPECT_EQ(strcmp(parse_val(rk.buf, rk.len), v.c_str()), 0);

  free(rk.buf);
}

TEST(RedisKvs, prepare_border) {
  std::string b("border");
  redis_key brdr = prepare_border(b.c_str(), b.length(), true, false);

  ASSERT_NE(brdr.buf, nullptr);
  EXPECT_EQ(brdr.len, b.length() + 1);
  EXPECT_EQ(strcmp(brdr.buf, ("[" + b).c_str()), 0);

  free(brdr.buf);
  brdr = {0, nullptr};

  brdr = prepare_border(b.c_str(), b.length(), false, false);

  ASSERT_NE(brdr.buf, nullptr);
  EXPECT_EQ(brdr.len, b.length() + 1);
  EXPECT_EQ(strcmp(brdr.buf, ("(" + b).c_str()), 0);

  free(brdr.buf);
  brdr = {0, nullptr};

  brdr = prepare_border(b.c_str(), b.length(), true, true);

  ASSERT_NE(brdr.buf, nullptr);
  EXPECT_EQ(brdr.len, b.length() + 2);
  EXPECT_EQ(strcmp(brdr.buf, ("[" + b + '\xff').c_str()), 0);

  free(brdr.buf);
  brdr = {0, nullptr};

  brdr = prepare_border(b.c_str(), b.length(), false, true);

  ASSERT_NE(brdr.buf, nullptr);
  EXPECT_EQ(brdr.len, b.length() + 2);
  EXPECT_EQ(strcmp(brdr.buf, ("(" + b + '\xff').c_str()), 0);

  free(brdr.buf);
  brdr = {0, nullptr};
}

TEST(RedisKvs, redis_reply_check) {
  struct m0_clovis_op op;
  s3_redis_context_obj rco;
  op.op_datum = &rco;

  redisAsyncContext rac;
  redisReply rr;
  s3_redis_async_ctx priv;

  EXPECT_EQ(redis_reply_check(nullptr, nullptr, nullptr, {}), REPL_ERR);
  EXPECT_EQ(redis_reply_check(&rac, &rr, nullptr, {}), REPL_ERR);

  priv.op = &op;
  rco.had_error = false;

  EXPECT_EQ(redis_reply_check(nullptr, nullptr, &priv, {}), REPL_DONE);
  EXPECT_EQ(rco.had_error, true);

  rco.had_error = false;
  rac.err = -ENOENT;
  EXPECT_EQ(redis_reply_check(&rac, &rr, &priv, {}), REPL_DONE);
  EXPECT_EQ(rco.had_error, true);

  rco.had_error = false;
  rac.err = 0;
  rr.type = 3;
  EXPECT_EQ(redis_reply_check(&rac, &rr, &priv, {1, 2, 4, 5}), REPL_DONE);
  EXPECT_EQ(rco.had_error, true);

  rco.had_error = false;
  rac.err = 0;
  rr.type = 3;
  EXPECT_EQ(redis_reply_check(&rac, &rr, &priv, {1, 2, 3, 5}), REPL_CONTINUE);
  EXPECT_EQ(rco.had_error, false);
}

static int op_cb_called = 0;

static void op_is_stable(struct m0_clovis_op*) { op_cb_called |= 0x01; }

static void op_is_failed(struct m0_clovis_op*) { op_cb_called |= 0x02; }

TEST(RedisKvs, finalize_op) {
  finalize_op(nullptr, op_is_stable, op_is_failed);
  EXPECT_EQ(op_cb_called, 0);

  struct m0_clovis_op op = {0};
  s3_redis_context_obj* rco =
      (s3_redis_context_obj*)calloc(1, sizeof(s3_redis_context_obj));
  ASSERT_NE(rco, nullptr);
  op.op_datum = (void*)rco;
  rco->async_ops_cnt = 3;
  rco->replies_cnt = 1;
  finalize_op(&op, op_is_stable, op_is_failed);
  EXPECT_EQ(op_cb_called, 0);

  rco->replies_cnt = 3;
  rco->had_error = false;
  finalize_op(&op, op_is_stable, op_is_failed);
  EXPECT_EQ(op_cb_called, 0x01);

  op_cb_called = 0;
  rco = (s3_redis_context_obj*)calloc(1, sizeof(s3_redis_context_obj));
  ASSERT_NE(rco, nullptr);
  s3_clovis_context_obj* prev_ctx =
      (s3_clovis_context_obj*)calloc(1, sizeof(s3_clovis_context_obj));
  ASSERT_NE(prev_ctx, nullptr);
  rco->prev_ctx = prev_ctx;
  op.op_datum = (void*)rco;
  rco->async_ops_cnt = 3;
  rco->replies_cnt = 3;
  rco->had_error = true;
  finalize_op(&op, op_is_stable, op_is_failed);
  EXPECT_EQ(op_cb_called, 0x02);

  free(prev_ctx);
}

class RedisKVSBaseTest : public testing::Test {
 protected:
  s3_redis_async_ctx* actx;
  struct m0_clovis_op op;
  s3_redis_context_obj* rco;
  s3_clovis_context_obj* prev_ctx;
  std::shared_ptr<MockMeroRequestObject> request;
  redisReply rr;
  redisAsyncContext rac;

  RedisKVSBaseTest() {
    actx = (s3_redis_async_ctx*)calloc(1, sizeof(s3_redis_async_ctx));

    op = {0};
    actx->op = &op;

    rco = (s3_redis_context_obj*)calloc(1, sizeof(s3_redis_context_obj));
    op.op_datum = (void*)rco;

    prev_ctx = (s3_clovis_context_obj*)calloc(1, sizeof(s3_clovis_context_obj));
    rco->prev_ctx = prev_ctx;

    request = std::make_shared<MockMeroRequestObject>(nullptr, nullptr);

    rr = {0};
    rac = {0};
  }

  void check_state() {
    ASSERT_NE(actx, nullptr);
    ASSERT_NE(rco, nullptr);
    ASSERT_NE(prev_ctx, nullptr);
  }

  ~RedisKVSBaseTest() {
    free(rco);
    free(prev_ctx);
  }

  void SetUp() {}

  void TearDown() {}
};

class RedisKVSWriterTest : public RedisKVSBaseTest {

 public:
  RedisKVSWriterTest() : RedisKVSBaseTest() {}

 protected:
  S3AsyncClovisKVSWriterContext* w_ctx;

  void SetUp() {
    check_state();
    w_ctx = new S3AsyncClovisKVSWriterContext(request, []() {}, []() {});
    ASSERT_NE(w_ctx, nullptr);
    prev_ctx->application_context = w_ctx;
  }

  void TearDown() { delete w_ctx; }
};

class RedisKVSReaderTest : public RedisKVSBaseTest {

 public:
  RedisKVSReaderTest() : RedisKVSBaseTest() {}

 protected:
  S3ClovisKVSReaderContext* r_ctx;

  void SetUp() {
    check_state();
    r_ctx = new S3ClovisKVSReaderContext(request, []() {}, []() {});
    ASSERT_NE(r_ctx, nullptr);
    prev_ctx->application_context = r_ctx;
  }

  void TearDown() { delete r_ctx; }
};

TEST_F(RedisKVSWriterTest, kv_status_cb_val_succ) {
  actx->processing_idx = 0;

  rco->async_ops_cnt = 3;
  rco->replies_cnt = 0;
  rco->had_error = false;

  w_ctx->init_kvs_write_op_ctx(1);
  ASSERT_NE(w_ctx->get_clovis_kvs_op_ctx(), nullptr);
  w_ctx->get_clovis_kvs_op_ctx()->rcs[0] = 11;

  rr.type = REDIS_REPLY_INTEGER;
  rr.integer = 1;

  kv_status_cb(&rac, &rr, actx);
  EXPECT_EQ(rco->replies_cnt, 1);
  EXPECT_EQ(op.op_rc, 0);
  struct s3_clovis_kvs_op_context* kv = w_ctx->get_clovis_kvs_op_ctx();
  EXPECT_EQ(kv->rcs[0], 0);
}

TEST_F(RedisKVSWriterTest, kv_status_cb_val_failed) {
  actx->processing_idx = 0;

  rco->async_ops_cnt = 3;
  rco->replies_cnt = 0;
  rco->had_error = false;

  w_ctx->init_kvs_write_op_ctx(1);
  ASSERT_NE(w_ctx->get_clovis_kvs_op_ctx(), nullptr);
  w_ctx->get_clovis_kvs_op_ctx()->rcs[0] = 11;

  rr.type = REDIS_REPLY_INTEGER;
  rr.integer = 0;

  kv_status_cb(&rac, &rr, actx);
  EXPECT_EQ(rco->replies_cnt, 1);
  EXPECT_EQ(op.op_rc, -ENOENT);
  struct s3_clovis_kvs_op_context* kv = w_ctx->get_clovis_kvs_op_ctx();
  EXPECT_EQ(kv->rcs[0], -ENOENT);
}

TEST_F(RedisKVSReaderTest, kv_read_cb_empty) {
  actx->processing_idx = 0;

  rco->async_ops_cnt = 3;
  rco->replies_cnt = 0;
  rco->had_error = false;

  r_ctx->init_kvs_read_op_ctx(1);
  ASSERT_NE(r_ctx->get_clovis_kvs_op_ctx(), nullptr);
  r_ctx->get_clovis_kvs_op_ctx()->rcs[0] = 11;

  rr.type = REDIS_REPLY_ARRAY;
  rr.elements = 0;

  kv_read_cb(&rac, &rr, actx);
  EXPECT_EQ(rco->replies_cnt, 1);
  EXPECT_EQ(op.op_rc, -ENOENT);
  struct s3_clovis_kvs_op_context* kv = r_ctx->get_clovis_kvs_op_ctx();
  EXPECT_EQ(kv->rcs[0], -ENOENT);
}

TEST_F(RedisKVSReaderTest, kv_read_cb_one) {
  actx->processing_idx = 1;

  rco->async_ops_cnt = 3;
  rco->replies_cnt = 0;
  rco->had_error = false;

  r_ctx->init_kvs_read_op_ctx(2);
  ASSERT_NE(r_ctx->get_clovis_kvs_op_ctx(), nullptr);
  struct s3_clovis_kvs_op_context* kv = r_ctx->get_clovis_kvs_op_ctx();
  kv->rcs[0] = kv->rcs[1] = 11;

  rr.type = REDIS_REPLY_ARRAY;
  rr.elements = 1;

  redisReply val0;
  val0.type = REDIS_REPLY_STRING;
  val0.str = (char*)"key0\0val0";
  val0.len = 10;

  redisReply* vals = &val0;
  rr.element = &vals;

  kv_read_cb(&rac, &rr, actx);
  EXPECT_EQ(rco->replies_cnt, 1);
  EXPECT_EQ(op.op_rc, 0);

  EXPECT_EQ(kv->rcs[0], 11);
  EXPECT_EQ(kv->rcs[1], 0);
  std::string read_val((char*)kv->values->ov_buf[1],
                       kv->values->ov_vec.v_count[1]);
  EXPECT_TRUE(read_val == "val0");
}

TEST_F(RedisKVSReaderTest, kv_read_cb_several) {
  actx->processing_idx = 1;

  rco->async_ops_cnt = 3;
  rco->replies_cnt = 0;
  rco->had_error = false;

  r_ctx->init_kvs_read_op_ctx(2);
  ASSERT_NE(r_ctx->get_clovis_kvs_op_ctx(), nullptr);
  struct s3_clovis_kvs_op_context* kv = r_ctx->get_clovis_kvs_op_ctx();
  kv->rcs[0] = kv->rcs[1] = 11;

  rr.type = REDIS_REPLY_ARRAY;
  rr.elements = 3;
  rr.element = nullptr;

  kv_read_cb(&rac, &rr, actx);
  EXPECT_EQ(rco->replies_cnt, 1);
  EXPECT_EQ(op.op_rc, -ENOENT);

  EXPECT_EQ(kv->rcs[0], 11);
  EXPECT_EQ(kv->rcs[1], -ENOENT);
}

TEST_F(RedisKVSReaderTest, kv_next_cb_empty) {
  actx->processing_idx = 0;

  rco->async_ops_cnt = 3;
  rco->replies_cnt = 0;
  rco->had_error = false;

  r_ctx->init_kvs_read_op_ctx(2);
  ASSERT_NE(r_ctx->get_clovis_kvs_op_ctx(), nullptr);
  struct s3_clovis_kvs_op_context* kv = r_ctx->get_clovis_kvs_op_ctx();
  kv->rcs[0] = kv->rcs[1] = 11;

  rr.type = REDIS_REPLY_ARRAY;
  rr.elements = 0;
  rr.element = nullptr;

  kv_next_cb(&rac, &rr, actx);
  EXPECT_EQ(rco->replies_cnt, 1);
  EXPECT_EQ(op.op_rc, -ENOENT);

  EXPECT_EQ(kv->rcs[0], -ENOENT);
  EXPECT_EQ(kv->rcs[1], -ENOENT);
}

TEST_F(RedisKVSReaderTest, kv_next_cb_empty_skip) {
  actx->processing_idx = 0;

  rco->async_ops_cnt = 3;
  rco->replies_cnt = 0;
  rco->had_error = false;
  rco->skip_value = (char*)"aaa";
  rco->skip_size = 3;

  r_ctx->init_kvs_read_op_ctx(2);
  ASSERT_NE(r_ctx->get_clovis_kvs_op_ctx(), nullptr);
  struct s3_clovis_kvs_op_context* kv = r_ctx->get_clovis_kvs_op_ctx();
  kv->rcs[0] = kv->rcs[1] = 11;

  rr.type = REDIS_REPLY_ARRAY;
  rr.elements = 0;
  rr.element = nullptr;

  kv_next_cb(&rac, &rr, actx);
  EXPECT_EQ(rco->replies_cnt, 1);
  EXPECT_EQ(op.op_rc, -ENOENT);

  EXPECT_EQ(kv->rcs[0], -ENOENT);
  EXPECT_EQ(kv->rcs[1], -ENOENT);
}

TEST_F(RedisKVSReaderTest, kv_next_cb_one_skip) {
  actx->processing_idx = 0;

  rco->async_ops_cnt = 3;
  rco->replies_cnt = 0;
  rco->had_error = false;
  rco->skip_value = (char*)"key0";
  rco->skip_size = 4;

  r_ctx->init_kvs_read_op_ctx(2);
  ASSERT_NE(r_ctx->get_clovis_kvs_op_ctx(), nullptr);
  struct s3_clovis_kvs_op_context* kv = r_ctx->get_clovis_kvs_op_ctx();
  kv->rcs[0] = kv->rcs[1] = 11;

  rr.type = REDIS_REPLY_ARRAY;
  rr.elements = 1;

  redisReply val0;
  val0.type = REDIS_REPLY_STRING;
  val0.str = (char*)"key0\0val0";
  val0.len = 10;

  redisReply* vals = &val0;
  rr.element = &vals;

  kv_next_cb(&rac, &rr, actx);
  EXPECT_EQ(rco->replies_cnt, 1);
  EXPECT_EQ(op.op_rc, -ENOENT);

  EXPECT_EQ(kv->rcs[0], -ENOENT);
  EXPECT_EQ(kv->rcs[1], -ENOENT);
}

TEST_F(RedisKVSReaderTest, kv_next_cb_one) {
  actx->processing_idx = 0;

  rco->async_ops_cnt = 3;
  rco->replies_cnt = 0;
  rco->had_error = false;

  r_ctx->init_kvs_read_op_ctx(2);
  ASSERT_NE(r_ctx->get_clovis_kvs_op_ctx(), nullptr);
  struct s3_clovis_kvs_op_context* kv = r_ctx->get_clovis_kvs_op_ctx();
  kv->rcs[0] = kv->rcs[1] = 11;

  rr.type = REDIS_REPLY_ARRAY;
  rr.elements = 1;

  redisReply val0;
  val0.type = REDIS_REPLY_STRING;
  val0.str = (char*)"key0\0val0";
  val0.len = 10;

  redisReply* vals = &val0;
  rr.element = &vals;

  kv_next_cb(&rac, &rr, actx);
  EXPECT_EQ(rco->replies_cnt, 1);
  EXPECT_EQ(op.op_rc, 0);

  EXPECT_EQ(kv->rcs[0], 0);
  EXPECT_EQ(kv->rcs[1], -ENOENT);

  std::string read_val((char*)kv->values->ov_buf[0],
                       kv->values->ov_vec.v_count[0]);
  EXPECT_TRUE(read_val == "val0");
  std::string read_key((char*)kv->keys->ov_buf[0], kv->keys->ov_vec.v_count[0]);
  EXPECT_TRUE(read_key == "key0");
}

TEST_F(RedisKVSReaderTest, kv_next_cb_several) {
  actx->processing_idx = 0;

  rco->async_ops_cnt = 3;
  rco->replies_cnt = 0;
  rco->had_error = false;

  r_ctx->init_kvs_read_op_ctx(2);
  ASSERT_NE(r_ctx->get_clovis_kvs_op_ctx(), nullptr);
  struct s3_clovis_kvs_op_context* kv = r_ctx->get_clovis_kvs_op_ctx();
  kv->rcs[0] = kv->rcs[1] = 11;

  rr.type = REDIS_REPLY_ARRAY;
  rr.elements = 2;

  redisReply val0;
  val0.type = REDIS_REPLY_STRING;
  val0.str = (char*)"key0\0val0";
  val0.len = 10;

  redisReply val1;
  val1.type = REDIS_REPLY_STRING;
  val1.str = (char*)"key1\0val1";
  val1.len = 10;

  redisReply* vals[2] = {&val0, &val1};
  rr.element = (redisReply**)&vals;

  kv_next_cb(&rac, &rr, actx);
  EXPECT_EQ(rco->replies_cnt, 1);
  EXPECT_EQ(op.op_rc, 0);

  EXPECT_EQ(kv->rcs[0], 0);
  EXPECT_EQ(kv->rcs[1], 0);

  std::string read_val0((char*)kv->values->ov_buf[0],
                        kv->values->ov_vec.v_count[0]);
  EXPECT_TRUE(read_val0 == "val0");
  std::string read_key0((char*)kv->keys->ov_buf[0],
                        kv->keys->ov_vec.v_count[0]);
  EXPECT_TRUE(read_key0 == "key0");

  std::string read_val1((char*)kv->values->ov_buf[1],
                        kv->values->ov_vec.v_count[1]);
  EXPECT_TRUE(read_val1 == "val1");
  std::string read_key1((char*)kv->keys->ov_buf[1],
                        kv->keys->ov_vec.v_count[1]);
  EXPECT_TRUE(read_key1 == "key1");
}

TEST_F(RedisKVSReaderTest, kv_next_cb_several_skip) {
  actx->processing_idx = 0;

  rco->async_ops_cnt = 3;
  rco->replies_cnt = 0;
  rco->had_error = false;
  rco->skip_value = (char*)"key0";
  rco->skip_size = 4;

  r_ctx->init_kvs_read_op_ctx(2);
  ASSERT_NE(r_ctx->get_clovis_kvs_op_ctx(), nullptr);
  struct s3_clovis_kvs_op_context* kv = r_ctx->get_clovis_kvs_op_ctx();
  kv->rcs[0] = kv->rcs[1] = 11;

  rr.type = REDIS_REPLY_ARRAY;
  rr.elements = 2;

  redisReply val0;
  val0.type = REDIS_REPLY_STRING;
  val0.str = (char*)"key0\0val0";
  val0.len = 10;

  redisReply val1;
  val1.type = REDIS_REPLY_STRING;
  val1.str = (char*)"key1\0val1";
  val1.len = 10;

  redisReply* vals[2] = {&val0, &val1};
  rr.element = (redisReply**)&vals;

  kv_next_cb(&rac, &rr, actx);
  EXPECT_EQ(rco->replies_cnt, 1);
  EXPECT_EQ(op.op_rc, 0);

  EXPECT_EQ(kv->rcs[0], 0);
  EXPECT_EQ(kv->rcs[1], -ENOENT);

  std::string read_val((char*)kv->values->ov_buf[0],
                       kv->values->ov_vec.v_count[0]);
  EXPECT_TRUE(read_val == "val1");
  std::string read_key((char*)kv->keys->ov_buf[0], kv->keys->ov_vec.v_count[0]);
  EXPECT_TRUE(read_key == "key1");
}
