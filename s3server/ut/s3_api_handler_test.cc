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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 09-May-2017
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

#include "s3_api_handler.h"

#include "mock_s3_async_buffer_opt_container.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;
using ::testing::AtLeast;

class S3APIHandlerTest : public testing::Test {
 protected:
  S3APIHandlerTest() {
    S3Option::get_instance()->disable_auth();

    call_count_one = 0;

    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();

    async_buffer_factory =
        std::make_shared<MockS3AsyncBufferOptContainerFactory>(
            S3Option::get_instance()->get_libevent_pool_buffer_size());

    mock_request = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr,
                                                         async_buffer_factory);
  }

  std::shared_ptr<MockS3RequestObject> mock_request;
  std::shared_ptr<MockS3AsyncBufferOptContainerFactory> async_buffer_factory;

  std::shared_ptr<S3APIHandler> handler_under_test;

  int call_count_one;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3APIHandlerTest, ConstructorTest) {
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::none));

  EXPECT_EQ(S3OperationCode::none, handler_under_test->operation_code);
}

TEST_F(S3APIHandlerTest, ManageSelfAndReset) {
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::none));
  handler_under_test->manage_self(handler_under_test);
  EXPECT_TRUE(handler_under_test == handler_under_test->_get_self_ref());
  handler_under_test->i_am_done();
  EXPECT_TRUE(handler_under_test->_get_self_ref() == nullptr);
}

TEST_F(S3APIHandlerTest, DispatchActionTest) {
  // Creation handler per test as it will be specific
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::none));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), get_api_type())
      .WillRepeatedly(Return(S3ApiType::service));

  handler_under_test->create_action();

  //
  handler_under_test->_get_action()->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(handler_under_test->_get_action(),
                         S3APIHandlerTest::func_callback_one, this);

  handler_under_test->dispatch();

  EXPECT_EQ(1, call_count_one);
  handler_under_test->_get_action()->i_am_done();
}

TEST_F(S3APIHandlerTest, DispatchUnSupportedAction) {
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::none));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);

  handler_under_test->dispatch();
}

// Test if analytics method returns 501 - NotImplemented error when called.
TEST_F(S3APIHandlerTest, DispatchBucketAnalyticsAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::analytics));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchBucketInventoryAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::inventory));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchBucketMetricsAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::metrics));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchBucketTaggingAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::tagging));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchBucketWebsiteAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::website));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchBucketReplicationAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::replication));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchBucketLoggingAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::logging));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchBucketVersioningAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::versioning));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchBucketNotificationAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::notification));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchObjectTorrentAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::torrent));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchObjectRequestPaymentAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::requestPayment));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchBucketEncryptionAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::encryption));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchSelectObjectContentAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::selectcontent));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchSelectObjectRestoreAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::restore));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(501, _)).Times(1);
  handler_under_test->dispatch();
}

TEST_F(S3APIHandlerTest, DispatchBucketCreateEmptyNameAction) {
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::metrics));
  std::string bucket_name = "";
  EXPECT_CALL(*mock_request, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));
  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::PUT));
  EXPECT_CALL(*(mock_request), set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(mock_request), send_response(405, _)).Times(1);
  handler_under_test->dispatch();
}
