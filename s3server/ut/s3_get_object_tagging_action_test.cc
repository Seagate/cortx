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
 * Original author:  Siddhivinayak Shanbhag <siddhivinayak.shanbhag@seagate.com>
 * Original creation date: 24-January-2019
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_get_object_tagging_action.h"

using ::testing::Invoke;
using ::testing::AtLeast;
using ::testing::ReturnRef;

#define CREATE_BUCKET_METADATA                                            \
  do {                                                                    \
    EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), load(_, _)) \
        .Times(AtLeast(1));                                               \
    action_under_test_ptr->fetch_bucket_info();                           \
  } while (0)

#define CREATE_OBJECT_METADATA                                                 \
  do {                                                                         \
    action_under_test_ptr->object_metadata =                                   \
        object_meta_factory->create_object_metadata_obj(request_mock,          \
                                                        object_list_indx_oid); \
  } while (0)

class S3GetObjectTaggingActionTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.
  S3GetObjectTaggingActionTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    bucket_name = "seagatebucket";
    object_name = "objname";
    request_mock = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    EXPECT_CALL(*request_mock, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));
    EXPECT_CALL(*request_mock, get_object_name())
        .WillRepeatedly(ReturnRef(object_name));

    object_list_indx_oid = {0x11ffff, 0x1ffff};
    object_meta_factory =
        std::make_shared<MockS3ObjectMetadataFactory>(request_mock);
    object_meta_factory->set_object_list_index_oid(object_list_indx_oid);

    bucket_meta_factory =
        std::make_shared<MockS3BucketMetadataFactory>(request_mock);
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*request_mock, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    action_under_test_ptr = std::make_shared<S3GetObjectTaggingAction>(
        request_mock, bucket_meta_factory, object_meta_factory);
  }

  struct m0_uint128 object_list_indx_oid;
  std::shared_ptr<MockS3RequestObject> request_mock;
  std::shared_ptr<S3GetObjectTaggingAction> action_under_test_ptr;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::shared_ptr<MockS3ObjectMetadataFactory> object_meta_factory;
  std::string bucket_name, object_name;

 public:
  void func_callback_one() {
    action_under_test_ptr->send_response_to_s3_client();
  }
};

TEST_F(S3GetObjectTaggingActionTest, Constructor) {
  EXPECT_NE(0, action_under_test_ptr->number_of_tasks());
}

TEST_F(S3GetObjectTaggingActionTest, FetchBucketInfoFailedNoSuchBucket) {
  CREATE_BUCKET_METADATA;
  CREATE_OBJECT_METADATA;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::missing));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(404, _)).Times(AtLeast(1));
  action_under_test_ptr->fetch_bucket_info_failed();

  EXPECT_TRUE(action_under_test_ptr->bucket_metadata != NULL);
  EXPECT_STREQ("NoSuchBucket",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3GetObjectTaggingActionTest, FetchBucketInfoFailedInternalError) {
  CREATE_BUCKET_METADATA;
  CREATE_OBJECT_METADATA;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::failed));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(500, _)).Times(AtLeast(1));
  action_under_test_ptr->fetch_bucket_info_failed();

  EXPECT_TRUE(action_under_test_ptr->bucket_metadata != NULL);
  EXPECT_STREQ("InternalError",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3GetObjectTaggingActionTest, GetObjectMetadataFailedMissing) {
  CREATE_BUCKET_METADATA;
  CREATE_OBJECT_METADATA;
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::missing));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(404, _)).Times(AtLeast(1));
  action_under_test_ptr->object_list_oid = {0xff1f, 0xff1f};
  action_under_test_ptr->fetch_object_info_failed();

  EXPECT_TRUE(action_under_test_ptr->object_metadata != NULL);
  EXPECT_STREQ("NoSuchKey", action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3GetObjectTaggingActionTest, SendResponseToClientEmptyTagSet) {
  std::string user_defined_tags;
  std::string tags_as_xml_str =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<Tagging xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
      "<TagSet>" +
      user_defined_tags +
      "</TagSet>"
      "</Tagging>";

  CREATE_OBJECT_METADATA;

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::present));
  EXPECT_CALL(*request_mock, send_response(200, tags_as_xml_str))
      .Times(AtLeast(1));
  action_under_test_ptr->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test_ptr,
                         S3GetObjectTaggingActionTest::func_callback_one, this);

  action_under_test_ptr->fetch_object_info_success();

  EXPECT_TRUE(action_under_test_ptr->object_metadata != NULL);
}

TEST_F(S3GetObjectTaggingActionTest, GetObjectMetadataFailedInternalError) {
  CREATE_OBJECT_METADATA;
  CREATE_OBJECT_METADATA;
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::failed));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(500, _)).Times(AtLeast(1));
  action_under_test_ptr->object_list_oid = {0xff1f, 0xff1f};
  action_under_test_ptr->fetch_object_info_failed();

  EXPECT_TRUE(action_under_test_ptr->object_metadata != NULL);
  EXPECT_STREQ("InternalError",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3GetObjectTaggingActionTest, SendResponseToClientServiceUnavailable) {
  CREATE_BUCKET_METADATA;
  CREATE_OBJECT_METADATA;
  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*request_mock, pause()).Times(1);
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(503, _)).Times(AtLeast(1));
  action_under_test_ptr->check_shutdown_and_rollback();
  EXPECT_STREQ("ServiceUnavailable",
               action_under_test_ptr->get_s3_error_code().c_str());
  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3GetObjectTaggingActionTest, SendResponseToClientInternalError) {
  CREATE_OBJECT_METADATA;
  CREATE_OBJECT_METADATA;
  action_under_test_ptr->set_s3_error("InternalError");
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::failed));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(500, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3GetObjectTaggingActionTest, SendResponseToClientSuccess) {
  CREATE_OBJECT_METADATA;
  CREATE_OBJECT_METADATA;
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::present));
  EXPECT_CALL(*request_mock, send_response(200, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

