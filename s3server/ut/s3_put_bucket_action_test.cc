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
 * Original author:  Abrarahmed Momin   <abrar.habib@seagate.com>
 * Original creation date: April-10-2017
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_s3_bucket_metadata.h"
#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_put_bucket_action.h"

using ::testing::Invoke;
using ::testing::AtLeast;
using ::testing::ReturnRef;

#define CREATE_BUCKET_METADATA_OBJ                      \
  do {                                                  \
    action_under_test_ptr->bucket_metadata =            \
        action_under_test_ptr->bucket_metadata_factory  \
            ->create_bucket_metadata_obj(request_mock); \
  } while (0)

class S3PutBucketActionTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.
  S3PutBucketActionTest() {

    call_count_one = 0;
    bucket_name = "seagatebucket";
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    request_mock = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    EXPECT_CALL(*request_mock, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));

    bucket_meta_factory =
        std::make_shared<MockS3BucketMetadataFactory>(request_mock);
    bucket_body_factory_mock =
        std::make_shared<MockS3PutBucketBodyFactory>(MockBucketBody);
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*request_mock, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    action_under_test_ptr = std::make_shared<S3PutBucketAction>(
        request_mock, bucket_meta_factory, bucket_body_factory_mock);
    MockBucketBody.assign("MockBucketBodyData");
  }

  std::shared_ptr<MockS3RequestObject> request_mock;
  std::shared_ptr<S3PutBucketAction> action_under_test_ptr;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::shared_ptr<MockS3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<MockS3PutBucketBodyFactory> bucket_body_factory_mock;
  std::shared_ptr<ClovisAPI> s3_clovis_api_mock;
  std::string MockBucketBody;
  int call_count_one;
  std::string bucket_name;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3PutBucketActionTest, Constructor) {
  EXPECT_NE(0, action_under_test_ptr->number_of_tasks());
  EXPECT_TRUE(action_under_test_ptr->bucket_metadata_factory != nullptr);
  EXPECT_TRUE(action_under_test_ptr->put_bucketbody_factory != nullptr);
  EXPECT_STREQ("", action_under_test_ptr->location_constraint.c_str());
}

TEST_F(S3PutBucketActionTest, ValidateRequest) {
  std::string MockString = "mockstring";

  action_under_test_ptr->put_bucket_body =
      action_under_test_ptr->put_bucketbody_factory->create_put_bucket_body(
          MockBucketBody);

  EXPECT_CALL(*request_mock, has_all_body_content())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*request_mock, get_full_body_content_as_string())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(MockBucketBody));
  EXPECT_CALL(*(bucket_body_factory_mock->mock_put_bucket_body), isOK())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*(bucket_body_factory_mock->mock_put_bucket_body),
              get_location_constraint())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(MockString));
  action_under_test_ptr->clear_tasks();
  action_under_test_ptr->validate_request();
}

TEST_F(S3PutBucketActionTest, ValidateRequestInvalid) {
  std::string MockString = "mockstring";

  action_under_test_ptr->put_bucket_body =
      action_under_test_ptr->put_bucketbody_factory->create_put_bucket_body(
          MockBucketBody);

  EXPECT_CALL(*request_mock, has_all_body_content())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*request_mock, get_full_body_content_as_string())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(MockBucketBody));
  EXPECT_CALL(*(bucket_body_factory_mock->mock_put_bucket_body), isOK())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_request();
  EXPECT_STREQ("MalformedXML",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3PutBucketActionTest, ValidateRequestMoreContent) {
  EXPECT_CALL(*request_mock, has_all_body_content()).Times(1).WillOnce(
      Return(false));
  EXPECT_CALL(*request_mock, get_data_length()).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*request_mock, listen_for_incoming_data(_, _)).Times(1);

  action_under_test_ptr->validate_request();
}
TEST_F(S3PutBucketActionTest, ValidateBucketNameValidNameTest1) {
  bucket_name = "seagatebucket";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  // Mock out the next calls on action.
  action_under_test_ptr->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test_ptr,
                         S3PutBucketActionTest::func_callback_one, this);

  action_under_test_ptr->validate_bucket_name();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameValidNameTest2) {
  bucket_name = "1234.0.0.1";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  // Mock out the next calls on action.
  action_under_test_ptr->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test_ptr,
                         S3PutBucketActionTest::func_callback_one, this);

  action_under_test_ptr->validate_bucket_name();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameValidNameTest3) {
  bucket_name = "123.0.1";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  // Mock out the next calls on action.
  action_under_test_ptr->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test_ptr,
                         S3PutBucketActionTest::func_callback_one, this);

  action_under_test_ptr->validate_bucket_name();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameValidNameTest4) {
  bucket_name = "123.0.1.0.0";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  // Mock out the next calls on action.
  action_under_test_ptr->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test_ptr,
                         S3PutBucketActionTest::func_callback_one, this);

  action_under_test_ptr->validate_bucket_name();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameValidNameTest5) {
  bucket_name = "sea-gate-pune";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  // Mock out the next calls on action.
  action_under_test_ptr->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test_ptr,
                         S3PutBucketActionTest::func_callback_one, this);

  action_under_test_ptr->validate_bucket_name();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameValidNameTest6) {
  bucket_name = "1sea-gate-pune2";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  // Mock out the next calls on action.
  action_under_test_ptr->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test_ptr,
                         S3PutBucketActionTest::func_callback_one, this);

  action_under_test_ptr->validate_bucket_name();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest1) {
  bucket_name = "Seagatebucket";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest2) {
  bucket_name = "seagatebucket-";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}
TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest3) {
  bucket_name = "seagatebucket.";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest4) {
  bucket_name = "seagatebu.-cket";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest5) {
  bucket_name = "seagatebu-.cket";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}
TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest6) {
  bucket_name = "0.0.0.0";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest7) {
  bucket_name = "192.168.192.145";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest8) {
  bucket_name = "ab";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest9) {
  bucket_name =
      "abcdfghjkloiuytrewasdfghjklmnhgtrfedsedrftgyhujikolmjnhbgvfcdsxzawsedrft"
      "gyhujikmjnhbgvfcdserfvfghdjh";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest10) {
  bucket_name = "seaGate";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest11) {
  bucket_name = "*seagate";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest12) {
  bucket_name = "-seagate";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest13) {
  bucket_name = "sea#gate";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest14) {
  bucket_name = "sea+gate";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}

TEST_F(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest15) {
  bucket_name = "sea....gate";
  EXPECT_CALL(*request_mock, get_bucket_name())
      .WillRepeatedly(ReturnRef(bucket_name));

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_bucket_name();
}

TEST_F(S3PutBucketActionTest, ReadMetaData) {
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), load(_, _))
      .Times(AtLeast(1));
  action_under_test_ptr->read_metadata();
}

TEST_F(S3PutBucketActionTest, CreateBucketAlreadyExist) {
  CREATE_BUCKET_METADATA_OBJ;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::present));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(409, _)).Times(AtLeast(1));
  action_under_test_ptr->create_bucket();
  EXPECT_STREQ("BucketAlreadyExists",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3PutBucketActionTest, CreateBucketSuccess) {
  CREATE_BUCKET_METADATA_OBJ;
  action_under_test_ptr->location_constraint.assign("MockLocation");

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::missing));
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata),
              set_location_constraint(_)).Times(AtLeast(1));
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), save(_, _))
      .Times(AtLeast(1));
  action_under_test_ptr->create_bucket();
}

TEST_F(S3PutBucketActionTest, CreateBucketFailed) {
  CREATE_BUCKET_METADATA_OBJ;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::failed));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(500, _)).Times(AtLeast(1));
  action_under_test_ptr->create_bucket();
  EXPECT_STREQ("InternalError",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3PutBucketActionTest, SendResponseToClientServiceUnavailable) {
  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*request_mock, pause()).Times(1);
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(503, _)).Times(AtLeast(1));
  action_under_test_ptr->check_shutdown_and_rollback();
  EXPECT_STREQ("ServiceUnavailable",
               action_under_test_ptr->get_s3_error_code().c_str());
  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3PutBucketActionTest, SendResponseToClientMalformedXML) {
  action_under_test_ptr->set_s3_error("MalformedXML");
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3PutBucketActionTest, SendResponseToClientNoSuchBucket) {
  action_under_test_ptr->set_s3_error("NoSuchBucket");
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(404, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3PutBucketActionTest, SendResponseToClientSuccess) {
  CREATE_BUCKET_METADATA_OBJ;

  EXPECT_CALL(*request_mock, send_response(200, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3PutBucketActionTest, SendResponseToClientInternalError) {
  CREATE_BUCKET_METADATA_OBJ;
  action_under_test_ptr->set_s3_error("InternalError");
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(500, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

