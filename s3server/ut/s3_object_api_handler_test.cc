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

#include "s3_abort_multipart_action.h"
#include "s3_api_handler.h"
#include "s3_delete_object_action.h"
#include "s3_get_multipart_part_action.h"
#include "s3_get_object_acl_action.h"
#include "s3_get_object_action.h"
#include "s3_head_object_action.h"
#include "s3_log.h"
#include "s3_post_complete_action.h"
#include "s3_post_multipartobject_action.h"
#include "s3_put_chunk_upload_object_action.h"
#include "s3_put_multiobject_action.h"
#include "s3_put_object_acl_action.h"
#include "s3_put_object_action.h"

#include "mock_s3_async_buffer_opt_container.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"

using ::testing::Eq;
using ::testing::StrEq;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

class S3ObjectAPIHandlerTest : public testing::Test {
 protected:
  S3ObjectAPIHandlerTest() {
    S3Option::get_instance()->disable_auth();

    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    bucket_name = "seagatebucket";
    object_name = "objname";

    async_buffer_factory =
        std::make_shared<MockS3AsyncBufferOptContainerFactory>(
            S3Option::get_instance()->get_libevent_pool_buffer_size());

    mock_request = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr,
                                                         async_buffer_factory);
    EXPECT_CALL(*mock_request, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));
    EXPECT_CALL(*mock_request, get_object_name())
        .WillRepeatedly(ReturnRef(object_name));
  }

  std::shared_ptr<MockS3RequestObject> mock_request;
  std::shared_ptr<MockS3AsyncBufferOptContainerFactory> async_buffer_factory;

  std::shared_ptr<S3ObjectAPIHandler> handler_under_test;
  S3Option *instance = NULL;
  std::string bucket_name, object_name;
};

TEST_F(S3ObjectAPIHandlerTest, ConstructorTest) {
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::acl));

  EXPECT_EQ(S3OperationCode::acl, handler_under_test->operation_code);
}

TEST_F(S3ObjectAPIHandlerTest, ManageSelfAndReset) {
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::acl));
  handler_under_test->manage_self(handler_under_test);
  EXPECT_TRUE(handler_under_test == handler_under_test->_get_self_ref());
  handler_under_test->i_am_done();
  EXPECT_TRUE(handler_under_test->_get_self_ref() == nullptr);
}

TEST_F(S3ObjectAPIHandlerTest, ShouldCreateS3GetObjectACLAction) {
  // Creation handler per test as it will be specific
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::acl));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));

  handler_under_test->create_action();

  EXPECT_FALSE((dynamic_cast<S3GetObjectACLAction *>(
                   handler_under_test->_get_action().get())) == nullptr);
  handler_under_test->_get_action()->i_am_done();
}

TEST_F(S3ObjectAPIHandlerTest, ShouldCreateS3PutObjectACLAction) {
  // Creation handler per test as it will be specific
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::acl));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::PUT));

  handler_under_test->create_action();

  EXPECT_FALSE((dynamic_cast<S3PutObjectACLAction *>(
                   handler_under_test->_get_action().get())) == nullptr);
  handler_under_test->_get_action()->i_am_done();
}

TEST_F(S3ObjectAPIHandlerTest, ShouldNotHaveAction4OtherHttpOps) {
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::acl));

  EXPECT_CALL(*(mock_request), http_verb())
      .WillOnce(Return(S3HttpVerb::DELETE));

  handler_under_test->create_action();

  EXPECT_TRUE(handler_under_test->_get_action() == nullptr);
}

TEST_F(S3ObjectAPIHandlerTest, ShouldCreateS3PostCompleteAction) {
  // Creation handler per test as it will be specific
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::multipart));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::POST));
  EXPECT_CALL(*(mock_request), has_query_param_key(StrEq("uploadid")))
      .WillOnce(Return(true));
  EXPECT_CALL(*(mock_request), get_query_string_value(_))
      .WillRepeatedly(Return("123"));

  handler_under_test->create_action();

  EXPECT_FALSE((dynamic_cast<S3PostCompleteAction *>(
                   handler_under_test->_get_action().get())) == nullptr);
  handler_under_test->_get_action()->i_am_done();
}

TEST_F(S3ObjectAPIHandlerTest, ShouldCreateS3PostMultipartObjectAction) {
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  S3Option::get_instance()->enable_murmurhash_oid();
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::multipart));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::POST));
  EXPECT_CALL(*(mock_request), has_query_param_key(StrEq("uploadid")))
      .WillOnce(Return(false));

  handler_under_test->create_action();

  EXPECT_FALSE((dynamic_cast<S3PostMultipartObjectAction *>(
                   handler_under_test->_get_action().get())) == nullptr);
  handler_under_test->_get_action()->i_am_done();
  S3Option::get_instance()->disable_murmurhash_oid();
}

TEST_F(S3ObjectAPIHandlerTest, DoesNotSupportCopyPart) {
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::multipart));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::PUT));
  EXPECT_CALL(*(mock_request), get_header_value(StrEq("x-amz-copy-source")))
      .WillOnce(Return("someobj"));

  handler_under_test->create_action();

  EXPECT_TRUE(handler_under_test->_get_action() == nullptr);
}

TEST_F(S3ObjectAPIHandlerTest, ShouldCreateS3PutMultiObjectAction) {
  // Creation handler per test as it will be specific
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::multipart));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::PUT));
  EXPECT_CALL(*(mock_request), get_header_value(StrEq("x-amz-copy-source")))
      .WillOnce(Return(""));
  EXPECT_CALL(*(mock_request), get_query_string_value(_))
      .WillRepeatedly(Return("123"));
  EXPECT_CALL(*(mock_request), is_chunked()).WillRepeatedly(Return(true));

  handler_under_test->create_action();

  EXPECT_FALSE((dynamic_cast<S3PutMultiObjectAction *>(
                   handler_under_test->_get_action().get())) == nullptr);
  handler_under_test->_get_action()->i_am_done();
}

TEST_F(S3ObjectAPIHandlerTest, ShouldCreateS3GetMultipartPartAction) {
  // Creation handler per test as it will be specific
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::multipart));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));
  EXPECT_CALL(*(mock_request), get_query_string_value(_))
      .WillRepeatedly(Return("123"));

  handler_under_test->create_action();

  EXPECT_FALSE((dynamic_cast<S3GetMultipartPartAction *>(
                   handler_under_test->_get_action().get())) == nullptr);
  handler_under_test->_get_action()->i_am_done();
}

TEST_F(S3ObjectAPIHandlerTest, ShouldCreateS3AbortMultipartAction) {
  // Creation handler per test as it will be specific
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::multipart));

  EXPECT_CALL(*(mock_request), http_verb())
      .WillOnce(Return(S3HttpVerb::DELETE));
  EXPECT_CALL(*(mock_request), get_query_string_value(_))
      .WillRepeatedly(Return("123"));

  handler_under_test->create_action();

  EXPECT_FALSE((dynamic_cast<S3AbortMultipartAction *>(
                   handler_under_test->_get_action().get())) == nullptr);
  handler_under_test->_get_action()->i_am_done();
}

TEST_F(S3ObjectAPIHandlerTest, ShouldCreateS3HeadObjectAction) {
  // Creation handler per test as it will be specific
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::none));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::HEAD));

  handler_under_test->create_action();

  EXPECT_FALSE((dynamic_cast<S3HeadObjectAction *>(
                   handler_under_test->_get_action().get())) == nullptr);
  handler_under_test->_get_action()->i_am_done();
}

TEST_F(S3ObjectAPIHandlerTest, ShouldCreateS3PutChunkUploadObjectAction) {
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  S3Option::get_instance()->enable_murmurhash_oid();
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::none));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::PUT));
  EXPECT_CALL(*(mock_request), get_header_value(StrEq("x-amz-content-sha256")))
      .WillOnce(Return("STREAMING-AWS4-HMAC-SHA256-PAYLOAD"));
  EXPECT_CALL(*(mock_request), get_header_value(StrEq("x-amz-tagging")))
      .WillOnce(Return(""));

  handler_under_test->create_action();

  EXPECT_FALSE((dynamic_cast<S3PutChunkUploadObjectAction *>(
                   handler_under_test->_get_action().get())) == nullptr);
  handler_under_test->_get_action()->i_am_done();
  S3Option::get_instance()->disable_murmurhash_oid();
}

TEST_F(S3ObjectAPIHandlerTest, DoesNotSupportCopyObject) {
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::none));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::PUT));
  EXPECT_CALL(*(mock_request), get_header_value(StrEq("x-amz-content-sha256")))
      .WillOnce(Return(""));
  EXPECT_CALL(*(mock_request), get_header_value(StrEq("x-amz-copy-source")))
      .WillOnce(Return("someobj"));

  handler_under_test->create_action();

  EXPECT_TRUE(handler_under_test->_get_action() == nullptr);
}

TEST_F(S3ObjectAPIHandlerTest, ShouldCreateS3PutObjectAction) {
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  S3Option::get_instance()->enable_murmurhash_oid();
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::none));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::PUT));
  EXPECT_CALL(*(mock_request), get_header_value(StrEq("x-amz-content-sha256")))
      .WillOnce(Return(""));
  EXPECT_CALL(*(mock_request), get_header_value(StrEq("x-amz-copy-source")))
      .WillOnce(Return(""));
  EXPECT_CALL(*(mock_request), get_header_value(StrEq("x-amz-tagging")))
      .WillOnce(Return(""));

  handler_under_test->create_action();

  EXPECT_FALSE((dynamic_cast<S3PutObjectAction *>(
                   handler_under_test->_get_action().get())) == nullptr);
  handler_under_test->_get_action()->i_am_done();
  S3Option::get_instance()->disable_murmurhash_oid();
}

TEST_F(S3ObjectAPIHandlerTest, ShouldCreateS3GetObjectAction) {
  // Creation handler per test as it will be specific
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::none));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::GET));

  handler_under_test->create_action();

  EXPECT_FALSE((dynamic_cast<S3GetObjectAction *>(
                   handler_under_test->_get_action().get())) == nullptr);
  handler_under_test->_get_action()->i_am_done();
}

TEST_F(S3ObjectAPIHandlerTest, ShouldCreateS3DeleteObjectAction) {
  // Creation handler per test as it will be specific
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::none));

  EXPECT_CALL(*(mock_request), http_verb())
      .WillOnce(Return(S3HttpVerb::DELETE));

  handler_under_test->create_action();

  EXPECT_FALSE((dynamic_cast<S3DeleteObjectAction *>(
                   handler_under_test->_get_action().get())) == nullptr);
  handler_under_test->_get_action()->i_am_done();
}

TEST_F(S3ObjectAPIHandlerTest, NoAction) {
  // Creation handler per test as it will be specific
  handler_under_test.reset(
      new S3ObjectAPIHandler(mock_request, S3OperationCode::none));

  EXPECT_CALL(*(mock_request), http_verb()).WillOnce(Return(S3HttpVerb::POST));

  handler_under_test->create_action();

  EXPECT_TRUE(handler_under_test->_get_action() == nullptr);
}

