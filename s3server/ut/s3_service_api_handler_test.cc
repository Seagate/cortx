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
#include "s3_get_service_action.h"

#include "mock_s3_async_buffer_opt_container.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

class S3ServiceAPIHandlerTest : public testing::Test {
 protected:
  S3ServiceAPIHandlerTest() {
    S3Option::get_instance()->disable_auth();

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

  std::shared_ptr<S3ServiceAPIHandler> handler_under_test;
};

TEST_F(S3ServiceAPIHandlerTest, ConstructorTest) {
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::none));

  EXPECT_EQ(S3OperationCode::none, handler_under_test->operation_code);
}

TEST_F(S3ServiceAPIHandlerTest, ManageSelfAndReset) {
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::none));
  handler_under_test->manage_self(handler_under_test);
  EXPECT_TRUE(handler_under_test == handler_under_test->_get_self_ref());
  handler_under_test->i_am_done();
  EXPECT_TRUE(handler_under_test->_get_self_ref() == nullptr);
}

TEST_F(S3ServiceAPIHandlerTest, ShouldCreateS3GetServiceAction) {
  // Creation handler per test as it will be specific
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::none));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));

  handler_under_test->create_action();

  EXPECT_FALSE((dynamic_cast<S3GetServiceAction *>(
                   handler_under_test->_get_action().get())) == nullptr);
  handler_under_test->_get_action()->i_am_done();
}

TEST_F(S3ServiceAPIHandlerTest, OperationNoneDefaultNoAction) {
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ServiceAPIHandler(mock_request, S3OperationCode::none));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::POST));

  handler_under_test->create_action();

  EXPECT_TRUE(handler_under_test->_get_action() == nullptr);
}
