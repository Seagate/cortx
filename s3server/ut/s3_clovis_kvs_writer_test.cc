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
 * Original author:  Rajesh Nambiar <rajesh.nambiar@seagate.com>
 * Original creation date: 27-Nov-2015
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <functional>
#include <iostream>

#include "s3_callback_test_helpers.h"
#include "s3_clovis_kvs_writer.h"
#include "s3_option.h"
#include "s3_ut_common.h"

#include "mock_s3_clovis_kvs_writer.h"
#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_request_object.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::AtLeast;

static void dummy_request_cb(evhtp_request_t *req, void *arg) {}

int s3_test_alloc_op(struct m0_clovis_entity *entity,
                     struct m0_clovis_op **op) {
  *op = (struct m0_clovis_op *)calloc(1, sizeof(struct m0_clovis_op));
  return 0;
}

int s3_test_alloc_sync_op(struct m0_clovis_op **sync_op) {
  *sync_op = (struct m0_clovis_op *)calloc(1, sizeof(struct m0_clovis_op));
  return 0;
}

int s3_test_clovis_idx_op(struct m0_clovis_idx *idx,
                          enum m0_clovis_idx_opcode opcode,
                          struct m0_bufvec *keys, struct m0_bufvec *vals,
                          int *rcs, unsigned int flags,
                          struct m0_clovis_op **op) {
  *op = (struct m0_clovis_op *)calloc(1, sizeof(struct m0_clovis_op));
  return 0;
}

void s3_test_free_clovis_op(struct m0_clovis_op *op) { free(op); }

static void s3_test_clovis_op_launch(uint64_t, struct m0_clovis_op **op,
                                     uint32_t nr, ClovisOpType type) {
  struct s3_clovis_context_obj *ctx =
      (struct s3_clovis_context_obj *)op[0]->op_datum;

  S3AsyncClovisKVSWriterContext *app_ctx =
      (S3AsyncClovisKVSWriterContext *)ctx->application_context;
  struct s3_clovis_idx_op_context *op_ctx = app_ctx->get_clovis_idx_op_ctx();

  for (int i = 0; i < (int)nr; i++) {
    struct m0_clovis_op *test_clovis_op = op[i];
    s3_clovis_op_stable(test_clovis_op);
    s3_test_free_clovis_op(test_clovis_op);
  }
  op_ctx->op_count = 0;
  *op = NULL;
}

static void s3_test_clovis_op_launch_fail(uint64_t, struct m0_clovis_op **op,
                                          uint32_t nr, ClovisOpType type) {
  struct s3_clovis_context_obj *ctx =
      (struct s3_clovis_context_obj *)op[0]->op_datum;

  S3AsyncClovisKVSWriterContext *app_ctx =
      (S3AsyncClovisKVSWriterContext *)ctx->application_context;
  struct s3_clovis_idx_op_context *op_ctx = app_ctx->get_clovis_idx_op_ctx();

  for (int i = 0; i < (int)nr; i++) {
    struct m0_clovis_op *test_clovis_op = op[i];
    s3_clovis_op_failed(test_clovis_op);
    s3_test_free_clovis_op(test_clovis_op);
  }
  op_ctx->op_count = 0;
}

static void s3_test_clovis_op_launch_fail_exists(uint64_t,
                                                 struct m0_clovis_op **op,
                                                 uint32_t nr,
                                                 ClovisOpType type) {
  struct s3_clovis_context_obj *ctx =
      (struct s3_clovis_context_obj *)op[0]->op_datum;

  S3AsyncClovisKVSWriterContext *app_ctx =
      (S3AsyncClovisKVSWriterContext *)ctx->application_context;
  struct s3_clovis_idx_op_context *op_ctx = app_ctx->get_clovis_idx_op_ctx();

  for (int i = 0; i < (int)nr; i++) {
    struct m0_clovis_op *test_clovis_op = op[i];
    s3_clovis_op_failed(test_clovis_op);
    s3_test_free_clovis_op(test_clovis_op);
  }
  op_ctx->op_count = 0;
}

class S3ClovisKvsWritterTest : public testing::Test {
 protected:
  S3ClovisKvsWritterTest() {
    evbase = event_base_new();
    req = evhtp_request_new(dummy_request_cb, evbase);
    EvhtpWrapper *evhtp_obj_ptr = new EvhtpWrapper();
    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    ptr_mock_s3clovis = std::make_shared<MockS3Clovis>();
    EXPECT_CALL(*ptr_mock_s3clovis, m0_h_ufid_next(_))
        .WillRepeatedly(Invoke(dummy_helpers_ufid_next));

    EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_rc(_)).WillRepeatedly(Return(0));

    action_under_test = std::make_shared<S3ClovisKVSWriter>(ptr_mock_request,
                                                            ptr_mock_s3clovis);
    oid = {0xffff, 0xfff1f};
  }

  ~S3ClovisKvsWritterTest() { event_base_free(evbase); }

  evbase_t *evbase;
  evhtp_request_t *req;
  struct m0_uint128 oid;
  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3clovis;
  std::shared_ptr<S3ClovisKVSWriter> action_under_test;
  S3ClovisKVSWriter *p_cloviskvs;
};

TEST_F(S3ClovisKvsWritterTest, Constructor) {
  EXPECT_EQ(S3ClovisKVSWriterOpState::start, action_under_test->get_state());
  EXPECT_EQ(action_under_test->request, ptr_mock_request);
  EXPECT_TRUE(action_under_test->idx_ctx == nullptr);
  EXPECT_EQ(0, action_under_test->oid_list.size());
}

TEST_F(S3ClovisKvsWritterTest, CleanupContexts) {
  action_under_test->idx_ctx = (struct s3_clovis_idx_context *)calloc(
      1, sizeof(struct s3_clovis_idx_context));
  action_under_test->idx_ctx->idx =
      (struct m0_clovis_idx *)calloc(2, sizeof(struct m0_clovis_idx));
  action_under_test->idx_ctx->idx_count = 2;
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(2);
  action_under_test->clean_up_contexts();
  EXPECT_TRUE(action_under_test->sync_context == nullptr);
  EXPECT_TRUE(action_under_test->writer_context == nullptr);
  EXPECT_TRUE(action_under_test->idx_ctx == nullptr);
}

TEST_F(S3ClovisKvsWritterTest, CreateIndex) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_entity_create(_, _))
      .WillOnce(Invoke(s3_test_alloc_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillRepeatedly(Invoke(s3_test_clovis_op_launch));

  action_under_test->create_index(
      "TestIndex", std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_EQ(1, action_under_test->oid_list.size());
  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, CreateIndexIdxPresent) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_entity_create(_, _))
      .WillOnce(Invoke(s3_test_alloc_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillRepeatedly(Invoke(s3_test_clovis_op_launch));

  action_under_test->idx_ctx = (struct s3_clovis_idx_context *)calloc(
      1, sizeof(struct s3_clovis_idx_context));
  action_under_test->idx_ctx->idx =
      (struct m0_clovis_idx *)calloc(2, sizeof(struct m0_clovis_idx));
  action_under_test->idx_ctx->idx_count = 2;
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(3);

  action_under_test->create_index(
      "TestIndex", std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_EQ(1, action_under_test->oid_list.size());
  EXPECT_EQ(1, action_under_test->idx_ctx->idx_count);
  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, CreateIndexSuccessful) {
  S3CallBack s3cloviskvscallbackobj;
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  action_under_test->idx_ctx = (struct s3_clovis_idx_context *)calloc(
      1, sizeof(struct s3_clovis_idx_context));
  action_under_test->idx_ctx->idx =
      (struct m0_clovis_idx *)calloc(1, sizeof(struct m0_clovis_idx));
  action_under_test->idx_ctx->idx_count = 1;

  action_under_test->handler_on_success =
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj);
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj);

  action_under_test->create_index_successful();

  EXPECT_EQ(S3ClovisKVSWriterOpState::created, action_under_test->get_state());
  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, CreateIndexEntityCreateFailed) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_entity_create(_, _))
      .WillOnce(Return(-1));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);

  action_under_test->create_index(
      "TestIndex", std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_EQ(1, action_under_test->oid_list.size());
  EXPECT_FALSE(s3cloviskvscallbackobj.success_called);
  EXPECT_TRUE(s3cloviskvscallbackobj.fail_called);
  EXPECT_EQ(S3ClovisKVSWriterOpState::failed_to_launch,
            action_under_test->get_state());
}

TEST_F(S3ClovisKvsWritterTest, CreateIndexFail) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_entity_create(_, _))
      .WillOnce(Invoke(s3_test_alloc_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch_fail));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_rc(_))
      .WillRepeatedly(Return(-EPERM));
  action_under_test->create_index(
      "BUCKET/seagate_bucket",
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));
  action_under_test->create_index_failed();

  EXPECT_EQ(S3ClovisKVSWriterOpState::failed, action_under_test->get_state());
  EXPECT_TRUE(s3cloviskvscallbackobj.fail_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.success_called);
}

TEST_F(S3ClovisKvsWritterTest, CreateIndexFailExists) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_entity_create(_, _))
      .WillOnce(Invoke(s3_test_alloc_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch_fail_exists));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_rc(_))
      .WillRepeatedly(Return(-EEXIST));
  action_under_test->create_index(
      "BUCKET/seagate_bucket",
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));
  action_under_test->create_index_failed();

  EXPECT_EQ(S3ClovisKVSWriterOpState::exists, action_under_test->get_state());
  EXPECT_TRUE(s3cloviskvscallbackobj.fail_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.success_called);
}

TEST_F(S3ClovisKvsWritterTest, PutKeyVal) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillRepeatedly(Invoke(s3_test_clovis_op_launch));

  action_under_test->put_keyval(
      oid, "3kfile",
      "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"3kfile\"}",
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, PutKeyValEmpty) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillRepeatedly(Invoke(s3_test_clovis_op_launch));

  action_under_test->put_keyval(
      oid, "", "", std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, PutKeyValSuccessful) {
  S3CallBack s3cloviskvscallbackobj;

  action_under_test->writer_context.reset(
      new S3AsyncClovisKVSWriterContext(ptr_mock_request, NULL, NULL));

  action_under_test->handler_on_success =
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj);
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj);

  action_under_test->put_keyval_successful();

  EXPECT_EQ(S3ClovisKVSWriterOpState::created, action_under_test->get_state());
  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, PutKeyValFailed) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch_fail));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_rc(_))
      .WillRepeatedly(Return(-EPERM));
  action_under_test->writer_context.reset(new S3AsyncClovisKVSWriterContext(
      ptr_mock_request, NULL, NULL, 1, ptr_mock_s3clovis));
  action_under_test->put_keyval(
      oid, "3kfile",
      "{\"ACL\":\"\",\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":"
      "\"3kfile\"}",
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));
  action_under_test->put_keyval_failed();

  EXPECT_EQ(S3ClovisKVSWriterOpState::failed, action_under_test->get_state());
  EXPECT_FALSE(s3cloviskvscallbackobj.success_called);
  EXPECT_TRUE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, DelKeyVal) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillRepeatedly(Invoke(s3_test_clovis_op_launch));

  action_under_test->delete_keyval(
      oid, "3kfile",
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, DelKeyValEmpty) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillRepeatedly(Invoke(s3_test_clovis_op_launch));

  action_under_test->delete_keyval(
      oid, "", std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, DelKeyValSuccess) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillRepeatedly(Invoke(s3_test_clovis_op_launch));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_sync_op_init(_))
      .WillRepeatedly(Invoke(s3_test_alloc_sync_op));

  action_under_test->delete_keyval(
      oid, "3kfile",
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));
  action_under_test->delete_keyval_successful();

  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, DelKeyValFailed) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_op(_, _, _, _, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_idx_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillOnce(Invoke(s3_test_clovis_op_launch_fail));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_rc(_))
      .WillRepeatedly(Return(-EPERM));
  action_under_test->delete_keyval(
      oid, "3kfile",
      std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  action_under_test->delete_keyval_failed();
  EXPECT_EQ(S3ClovisKVSWriterOpState::failed, action_under_test->get_state());
  EXPECT_FALSE(s3cloviskvscallbackobj.success_called);
  EXPECT_TRUE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, DelIndex) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_entity_delete(_, _))
      .WillOnce(Invoke(s3_test_alloc_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillRepeatedly(Invoke(s3_test_clovis_op_launch));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_entity_open(_, _));

  action_under_test->delete_index(
      oid, std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, DelIndexIdxPresent) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_entity_delete(_, _))
      .WillOnce(Invoke(s3_test_alloc_op));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_setup(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_launch(_, _, _, _))
      .WillRepeatedly(Invoke(s3_test_clovis_op_launch));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_entity_open(_, _));

  action_under_test->idx_ctx = (struct s3_clovis_idx_context *)calloc(
      1, sizeof(struct s3_clovis_idx_context));
  action_under_test->idx_ctx->idx =
      (struct m0_clovis_idx *)calloc(2, sizeof(struct m0_clovis_idx));
  action_under_test->idx_ctx->idx_count = 2;
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(3);

  action_under_test->delete_index(
      oid, std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_TRUE(s3cloviskvscallbackobj.success_called);
  EXPECT_FALSE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, DelIndexEntityDeleteFailed) {
  S3CallBack s3cloviskvscallbackobj;

  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_init(_, _, _));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_entity_delete(_, _))
      .WillOnce(Return(-1));
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_idx_fini(_)).Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_entity_open(_, _));

  action_under_test->delete_index(
      oid, std::bind(&S3CallBack::on_success, &s3cloviskvscallbackobj),
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj));

  EXPECT_FALSE(s3cloviskvscallbackobj.success_called);
  EXPECT_TRUE(s3cloviskvscallbackobj.fail_called);
}

TEST_F(S3ClovisKvsWritterTest, DelIndexFailed) {
  S3CallBack s3cloviskvscallbackobj;
  action_under_test->writer_context.reset(
      new S3AsyncClovisKVSWriterContext(ptr_mock_request, NULL, NULL));
  action_under_test->writer_context->ops_response[0].error_code = -ENOENT;

  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj);
  action_under_test->delete_index_failed();
  EXPECT_EQ(S3ClovisKVSWriterOpState::missing, action_under_test->get_state());
  EXPECT_FALSE(s3cloviskvscallbackobj.success_called);
  EXPECT_TRUE(s3cloviskvscallbackobj.fail_called);

  s3cloviskvscallbackobj.fail_called = false;
  action_under_test->writer_context->ops_response[0].error_code = -EACCES;
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3cloviskvscallbackobj);
  action_under_test->delete_index_failed();
  EXPECT_EQ(S3ClovisKVSWriterOpState::failed, action_under_test->get_state());
  EXPECT_FALSE(s3cloviskvscallbackobj.success_called);
  EXPECT_TRUE(s3cloviskvscallbackobj.fail_called);
}
