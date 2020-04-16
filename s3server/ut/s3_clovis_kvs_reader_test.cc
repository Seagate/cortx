/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original author:  Abrarahmed Momin  <abrar.habib@seagate.com>
 * Original creation date: 30-Jan-2017
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <functional>
#include <iostream>

#include "s3_callback_test_helpers.h"
#include "s3_clovis_kvs_reader.h"
#include "s3_option.h"

#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_request_object.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;
using ::testing::Invoke;

static void dummy_request_cb(evhtp_request_t *req, void *arg) {}
static enum m0_clovis_idx_opcode g_opcode;

int s3_kvs_test_clovis_idx_op(struct m0_clovis_idx *idx,
                              enum m0_clovis_idx_opcode opcode,
                              struct m0_bufvec *keys, struct m0_bufvec *vals,
                              int *rcs, unsigned int flags,
                              struct m0_clovis_op **op) {
  *op = (struct m0_clovis_op *)calloc(1, sizeof(struct m0_clovis_op));
  g_opcode = opcode;
  return 0;
}

void s3_kvs_test_free_op(struct m0_clovis_op *op) { free(op); }

static void s3_test_clovis_op_launch(uint64_t, struct m0_clovis_op **op,
                                     uint32_t nr, ClovisOpType type) {
  struct s3_clovis_context_obj *ctx =
      (struct s3_clovis_context_obj *)op[0]->op_datum;

  S3ClovisKVSReaderContext *app_ctx =
      (S3ClovisKVSReaderContext *)ctx->application_context;
  struct s3_clovis_idx_op_context *op_ctx = app_ctx->get_clovis_idx_op_ctx();

  if (M0_CLOVIS_IC_NEXT == g_opcode) {
    // For M0_CLOVIS_IC_NEXT op, if there are any keys to be returned to the
    // application, clovis overwrites the input key buffer ptr.
    struct s3_clovis_kvs_op_context *kvs_ctx = app_ctx->get_clovis_kvs_op_ctx();
    std::string ret_key = "random";
    kvs_ctx->keys->ov_vec.v_count[0] = ret_key.length();
    kvs_ctx->keys->ov_buf[0] = malloc(ret_key.length());
    memcpy(kvs_ctx->keys->ov_buf[0], (void *)ret_key.c_str(), ret_key.length());
  }

  for (int i = 0; i < (int)nr; i++) {
    struct m0_clovis_op *test_clovis_op = op[i];
    s3_clovis_op_stable(test_clovis_op);
    s3_kvs_test_free_op(test_clovis_op);
  }
  op_ctx->op_count = 0;
}

static void s3_test_clovis_op_launch_fail(uint64_t, struct m0_clovis_op **op,
                                          uint32_t nr, ClovisOpType type) {
  struct s3_clovis_context_obj *ctx =
      (struct s3_clovis_context_obj *)op[0]->op_datum;

  S3ClovisKVSReaderContext *app_ctx =
      (S3ClovisKVSReaderContext *)ctx->application_context;
  struct s3_clovis_idx_op_context *op_ctx = app_ctx->get_clovis_idx_op_ctx();

  for (int i = 0; i < (int)nr; i++) {
    struct m0_clovis_op *test_clovis_op = op[i];
    s3_clovis_op_failed(test_clovis_op);
    s3_kvs_test_free_op(test_clovis_op);
  }
  op_ctx->op_count = 0;
}

static void s3_test_clovis_op_launch_fail_enoent(uint64_t,
                                                 struct m0_clovis_op **op,
                                                 uint32_t nr,
                                                 ClovisOpType type) {
  struct s3_clovis_context_obj *ctx =
      (struct s3_clovis_context_obj *)op[0]->op_datum;

  S3ClovisKVSReaderContext *app_ctx =
      (S3ClovisKVSReaderContext *)ctx->application_context;
  struct s3_clovis_idx_op_context *op_ctx = app_ctx->get_clovis_idx_op_ctx();

  for (int i = 0; i < (int)nr; i++) {
    struct m0_clovis_op *test_clovis_op = op[i];
    s3_clovis_op_failed(test_clovis_op);
    s3_kvs_test_free_op(test_clovis_op);
  }
  op_ctx->op_count = 0;
}

class S3ClovisKvsReaderTest : public testing::Test {
 protected:
  S3ClovisKvsReaderTest() {
    evbase = event_base_new();
    req = evhtp_request_new(dummy_request_cb, evbase);
    EvhtpWrapper *evhtp_obj_ptr = new EvhtpWrapper();
    ptr_mock_s3request =
        std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    ptr_mock_s3clovis = std::make_shared<MockS3Clovis>();
    EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_rc(_)).WillRepeatedly(Return(0));
    ptr_cloviskvs_reader = std::make_shared<S3ClovisKVSReader>(
        ptr_mock_s3request, ptr_mock_s3clovis);
    index_oid = {0ULL, 0ULL};
  }

  ~S3ClovisKvsReaderTest() { event_base_free(evbase); }

  evbase_t *evbase;
  evhtp_request_t *req;
  std::shared_ptr<MockS3RequestObject> ptr_mock_s3request;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3clovis;
  std::shared_ptr<S3ClovisKVSReader> ptr_cloviskvs_reader;

  struct m0_uint128 index_oid;
  std::string test_key;
  std::string index_name;
  int nr_kvp;
};

TEST_F(S3ClovisKvsReaderTest, Constructor) {
  EXPECT_EQ(ptr_cloviskvs_reader->get_state(), S3ClovisKVSReaderOpState::start);
  EXPECT_EQ(ptr_cloviskvs_reader->request, ptr_mock_s3request);
  EXPECT_EQ(ptr_cloviskvs_reader->last_value, "");
  EXPECT_EQ(ptr_cloviskvs_reader->iteration_index, 0);
  EXPECT_EQ(ptr_cloviskvs_reader->last_result_keys_values.empty(), true);
  EXPECT_EQ(ptr_cloviskvs_reader->idx_ctx, nullptr);
}

TEST_F(S3ClovisKvsReaderTest, CleanupContexts) {
  ptr_cloviskvs_reader->idx_ctx = (struct s3_clovis_idx_context *)calloc(
      1, sizeof(struct s3_clovis_idx_context));
  ptr_cloviskvs_reader->idx_ctx->idx =
      (struct m0_clovis_idx *)calloc(2, sizeof(struct m0_clovis_idx));
  ptr_cloviskvs_reader->idx_ctx->idx_count = 2;
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(2);
  ptr_cloviskvs_reader->clean_up_contexts();
  EXPECT_EQ(nullptr, ptr_cloviskvs_reader->reader_context);
  EXPECT_EQ(nullptr, ptr_cloviskvs_reader->idx_ctx);
}

TEST_F(S3ClovisKvsReaderTest, GetKeyvalTest) {
  S3CallBack s3cloviskvscallbackobj;

  test_key = "utTestKey";

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_kvs_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch));

  ptr_cloviskvs_reader->get_keyval(
      index_oid, test_key,
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsReaderTest, GetKeyvalFailTest) {
  S3CallBack s3cloviskvscallbackobj;

  test_key = "utTestKey";

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillRepeatedly(Return(-1));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _)).Times(0);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _)).Times(0);

  ptr_cloviskvs_reader->get_keyval(
      index_oid, test_key,
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_FALSE(s3cloviskvscallbackobj.success_called);
  EXPECT_TRUE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsReaderTest, GetKeyvalIdxPresentTest) {
  S3CallBack s3cloviskvscallbackobj;

  test_key = "utTestKey";

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_kvs_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch));

  ptr_cloviskvs_reader->idx_ctx = (struct s3_clovis_idx_context *)calloc(
      1, sizeof(struct s3_clovis_idx_context));
  ptr_cloviskvs_reader->idx_ctx->idx =
      (struct m0_clovis_idx *)calloc(2, sizeof(struct m0_clovis_idx));
  ptr_cloviskvs_reader->idx_ctx->idx_count = 2;
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(3);

  ptr_cloviskvs_reader->get_keyval(
      index_oid, test_key,
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_EQ(1, ptr_cloviskvs_reader->idx_ctx->idx_count);
  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsReaderTest, GetKeyvalTestEmpty) {
  S3CallBack s3cloviskvscallbackobj;

  test_key = "";  // Empty key string

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_kvs_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch));

  ptr_cloviskvs_reader->get_keyval(
      index_oid, test_key,
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsReaderTest, GetKeyvalSuccessfulTest) {
  S3CallBack s3cloviskvscallbackobj;
  test_key = "utTestKey";

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_kvs_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch));

  ptr_cloviskvs_reader->get_keyval(
      index_oid, test_key,
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  ptr_cloviskvs_reader->get_keyval_successful();
  EXPECT_TRUE(ptr_cloviskvs_reader->get_state() ==
              S3ClovisKVSReaderOpState::present);
  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsReaderTest, GetKeyvalFailedTest) {
  S3CallBack s3cloviskvscallbackobj;
  test_key = "utTestKey";

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_kvs_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch_fail));
  ptr_cloviskvs_reader->reader_context.reset(new S3ClovisKVSReaderContext(
      ptr_mock_s3request, NULL, NULL, ptr_mock_s3clovis));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_rc(_))
      .WillRepeatedly(Return(-EPERM));
  ptr_cloviskvs_reader->get_keyval(
      index_oid, test_key,
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  ptr_cloviskvs_reader->get_keyval_failed();
  EXPECT_TRUE(ptr_cloviskvs_reader->get_state() ==
              S3ClovisKVSReaderOpState::failed);
  EXPECT_TRUE(s3cloviskvscallbackobj.fail_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.success_called);
}

TEST_F(S3ClovisKvsReaderTest, GetKeyvalFailedTestMissing) {
  S3CallBack s3cloviskvscallbackobj;
  test_key = "utTestKey";

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_kvs_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch_fail_enoent));
  ptr_cloviskvs_reader->reader_context.reset(new S3ClovisKVSReaderContext(
      ptr_mock_s3request, NULL, NULL, ptr_mock_s3clovis));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_rc(_))
      .WillRepeatedly(Return(-ENOENT));
  ptr_cloviskvs_reader->get_keyval(
      index_oid, test_key,
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  ptr_cloviskvs_reader->get_keyval_failed();
  EXPECT_TRUE(ptr_cloviskvs_reader->get_state() ==
              S3ClovisKVSReaderOpState::missing);
  EXPECT_TRUE(s3cloviskvscallbackobj.fail_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.success_called);
}

TEST_F(S3ClovisKvsReaderTest, NextKeyvalTest) {
  S3CallBack s3cloviskvscallbackobj;
  test_key = "utTestKey";
  nr_kvp = 5;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_kvs_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch));
  ptr_cloviskvs_reader->next_keyval(
      index_oid, test_key, nr_kvp,
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsReaderTest, NextKeyvalIdxPresentTest) {
  S3CallBack s3cloviskvscallbackobj;
  test_key = "utTestKey";
  nr_kvp = 5;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_kvs_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch));

  ptr_cloviskvs_reader->idx_ctx = (struct s3_clovis_idx_context *)calloc(
      1, sizeof(struct s3_clovis_idx_context));
  ptr_cloviskvs_reader->idx_ctx->idx =
      (struct m0_clovis_idx *)calloc(2, sizeof(struct m0_clovis_idx));
  ptr_cloviskvs_reader->idx_ctx->idx_count = 2;
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(3);

  ptr_cloviskvs_reader->next_keyval(
      index_oid, test_key, nr_kvp,
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_EQ(1, ptr_cloviskvs_reader->idx_ctx->idx_count);
  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsReaderTest, NextKeyvalSuccessfulTest) {
  S3CallBack s3cloviskvscallbackobj;

  index_name = "ACCOUNT/s3ut_test";
  test_key = "utTestKey";
  nr_kvp = 5;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_kvs_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch));

  ptr_cloviskvs_reader->next_keyval(
      index_oid, test_key, nr_kvp,
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_TRUE(ptr_cloviskvs_reader->get_state() ==
              S3ClovisKVSReaderOpState::present);
  EXPECT_EQ(ptr_cloviskvs_reader->last_result_keys_values.size(), 1);
  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsReaderTest, NextKeyvalFailedTest) {
  S3CallBack s3cloviskvscallbackobj;
  index_name = "ACCOUNT/s3ut_test";
  test_key = "utTestKey";
  nr_kvp = 5;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_kvs_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch_fail));

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_rc(_))
      .WillRepeatedly(Return(-EPERM));
  ptr_cloviskvs_reader->reader_context.reset(new S3ClovisKVSReaderContext(
      ptr_mock_s3request, NULL, NULL, ptr_mock_s3clovis));
  ptr_cloviskvs_reader->next_keyval(
      index_oid, test_key, nr_kvp,
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  ptr_cloviskvs_reader->next_keyval_failed();
  EXPECT_TRUE(ptr_cloviskvs_reader->get_state() ==
              S3ClovisKVSReaderOpState::failed);
  EXPECT_TRUE(s3cloviskvscallbackobj.fail_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.success_called);
}

TEST_F(S3ClovisKvsReaderTest, NextKeyvalFailedTestMissing) {
  S3CallBack s3cloviskvscallbackobj;
  index_name = "ACCOUNT/s3ut_test";
  test_key = "utTestKey";
  nr_kvp = 5;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_kvs_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch_fail_enoent));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_rc(_))
      .WillRepeatedly(Return(-ENOENT));
  ptr_cloviskvs_reader->next_keyval(
      index_oid, test_key, nr_kvp,
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  ptr_cloviskvs_reader->next_keyval_failed();
  EXPECT_TRUE(ptr_cloviskvs_reader->get_state() ==
              S3ClovisKVSReaderOpState::missing);
  EXPECT_TRUE(s3cloviskvscallbackobj.fail_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.success_called);
}
