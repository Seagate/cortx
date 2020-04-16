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
 * Original author:  Rajesh Nambiar <rajesh.nambiar@seagate.com>
 * Original creation date: 17-May-2017
 */

#include <event2/event.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_evhtp_wrapper.h"
#include "mock_s3_asyncop_context_base.h"
#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_request_object.h"
#include "s3_callback_test_helpers.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::_;
using ::testing::AtLeast;

class S3ClovisReadWriteCommonTest : public testing::Test {
 protected:
  S3ClovisReadWriteCommonTest() {
    evbase_t *evbase = event_base_new();
    evhtp_request_t *req = evhtp_request_new(NULL, evbase);
    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(req, new EvhtpWrapper());
    ptr_mock_s3clovis = std::make_shared<MockS3Clovis>();
    EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_rc(_)).WillRepeatedly(Return(0));
    ptr_mock_s3_async_context = std::make_shared<MockS3AsyncOpContextBase>(
        ptr_mock_request,
        std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj),
        std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj),
        ptr_mock_s3clovis);
    s3user_event =
        event_new(evbase, -1, EV_WRITE | EV_READ | EV_TIMEOUT, NULL, NULL);
    user_context = (struct user_event_context *)calloc(
        1, sizeof(struct user_event_context));
    user_context->app_ctx =
        static_cast<void *>(ptr_mock_s3_async_context.get());
    user_context->user_event = (void *)s3user_event;
  }

  std::shared_ptr<MockS3Clovis> ptr_mock_s3clovis;
  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::shared_ptr<MockS3AsyncOpContextBase> ptr_mock_s3_async_context;
  S3CallBack s3objectmetadata_callbackobj;
  struct event *s3user_event;
  struct user_event_context *user_context;
};

TEST_F(S3ClovisReadWriteCommonTest, ClovisOpDoneOnMainThreadOnSuccess) {
  ptr_mock_s3_async_context->at_least_one_success = true;
  clovis_op_done_on_main_thread(1, 1, (void *)user_context);
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
}

TEST_F(S3ClovisReadWriteCommonTest, ClovisOpDoneOnMainThreadOnFail) {
  clovis_op_done_on_main_thread(1, 1, (void *)user_context);
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
}

TEST_F(S3ClovisReadWriteCommonTest,
       S3ClovisOpStableResponseCountSameAsOpCount) {
  struct m0_clovis_op op;
  struct s3_clovis_context_obj *op_ctx = (struct s3_clovis_context_obj *)calloc(
      1, sizeof(struct s3_clovis_context_obj));
  op.op_datum = op_ctx;
  ptr_mock_s3_async_context->at_least_one_success = true;
  op_ctx->application_context =
      static_cast<void *>(ptr_mock_s3_async_context.get());

  EXPECT_CALL(*ptr_mock_s3_async_context, set_op_errno_for(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_s3_async_context,
              set_op_status_for(_, S3AsyncOpStatus::success, "Success."))
      .Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_rc(_)).WillOnce(Return(0));
  ptr_mock_s3_async_context->response_received_count = 0;
  ptr_mock_s3_async_context->ops_count = 1;
  s3_clovis_op_stable(&op);
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
}

TEST_F(S3ClovisReadWriteCommonTest,
       S3ClovisOpStableResponseCountNotSameAsOpCount) {
  struct m0_clovis_op op;
  struct s3_clovis_context_obj *op_ctx = (struct s3_clovis_context_obj *)calloc(
      1, sizeof(struct s3_clovis_context_obj));
  op.op_datum = op_ctx;
  ptr_mock_s3_async_context->at_least_one_success = true;
  op_ctx->application_context =
      static_cast<void *>(ptr_mock_s3_async_context.get());

  EXPECT_CALL(*ptr_mock_s3_async_context, set_op_errno_for(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_s3_async_context,
              set_op_status_for(_, S3AsyncOpStatus::success, "Success."))
      .Times(1);
  EXPECT_CALL(*ptr_mock_s3clovis, clovis_op_rc(_))
      .WillOnce(Return(0))
      .WillRepeatedly(Return(-EPERM));
  ptr_mock_s3_async_context->response_received_count = 1;
  ptr_mock_s3_async_context->ops_count = 3;
  s3_clovis_op_stable(&op);
  EXPECT_FALSE(s3objectmetadata_callbackobj.success_called);
  EXPECT_FALSE(s3objectmetadata_callbackobj.fail_called);
}

TEST_F(S3ClovisReadWriteCommonTest,
       S3ClovisOpFailedResponseCountSameAsOpCount) {
  struct m0_clovis_op op;
  struct s3_clovis_context_obj *op_ctx = (struct s3_clovis_context_obj *)calloc(
      1, sizeof(struct s3_clovis_context_obj));
  op.op_datum = op_ctx;
  ptr_mock_s3_async_context->at_least_one_success = true;
  op_ctx->application_context =
      static_cast<void *>(ptr_mock_s3_async_context.get());

  EXPECT_CALL(*ptr_mock_s3_async_context, set_op_errno_for(_, _)).Times(1);
  EXPECT_CALL(
      *ptr_mock_s3_async_context,
      set_op_status_for(_, S3AsyncOpStatus::failed, "Operation Failed."))
      .Times(1);
  ptr_mock_s3_async_context->response_received_count = 0;
  ptr_mock_s3_async_context->ops_count = 1;
  s3_clovis_op_failed(&op);
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
}

TEST_F(S3ClovisReadWriteCommonTest,
       S3ClovisOpFailedResponseCountNotSameAsOpCount) {
  struct m0_clovis_op op;
  struct s3_clovis_context_obj *op_ctx = (struct s3_clovis_context_obj *)calloc(
      1, sizeof(struct s3_clovis_context_obj));
  op.op_datum = op_ctx;
  ptr_mock_s3_async_context->at_least_one_success = true;
  op_ctx->application_context =
      static_cast<void *>(ptr_mock_s3_async_context.get());

  EXPECT_CALL(*ptr_mock_s3_async_context, set_op_errno_for(_, _)).Times(1);
  EXPECT_CALL(
      *ptr_mock_s3_async_context,
      set_op_status_for(_, S3AsyncOpStatus::failed, "Operation Failed."))
      .Times(1);
  ptr_mock_s3_async_context->response_received_count = 0;
  ptr_mock_s3_async_context->ops_count = 3;
  s3_clovis_op_failed(&op);
  EXPECT_FALSE(s3objectmetadata_callbackobj.success_called);
  EXPECT_FALSE(s3objectmetadata_callbackobj.fail_called);
}
