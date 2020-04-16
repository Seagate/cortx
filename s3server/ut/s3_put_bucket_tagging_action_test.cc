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
 * Original creation date: 09-January-2019
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_s3_bucket_metadata.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_put_bucket_tagging_action.h"

using ::testing::Invoke;
using ::testing::AtLeast;
using ::testing::ReturnRef;

class S3PutBucketTaggingActionTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.
  S3PutBucketTaggingActionTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    bucket_name = "seagatebucket";

    request_mock = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    EXPECT_CALL(*request_mock, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));

    bucket_meta_factory =
        std::make_shared<MockS3BucketMetadataFactory>(request_mock);
    bucket_tag_body_factory_mock = std::make_shared<MockS3PutTagBodyFactory>(
        MockBucketTagsStr, MockRequestId);
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*request_mock, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    action_under_test_ptr = std::make_shared<S3PutBucketTaggingAction>(
        request_mock, bucket_meta_factory, bucket_tag_body_factory_mock);
    MockRequestId.assign("MockRequestId");
    MockBucketTagsStr.assign("MockBucketTags");
  }

  std::shared_ptr<MockS3RequestObject> request_mock;
  std::shared_ptr<S3PutBucketTaggingAction> action_under_test_ptr;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::shared_ptr<MockS3PutTagBodyFactory> bucket_tag_body_factory_mock;
  std::map<std::string, std::string> MockBucketTags;
  std::string MockBucketTagsStr;
  std::string MockRequestId;
  int call_count_one;
  std::string bucket_name;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3PutBucketTaggingActionTest, Constructor) {
  EXPECT_NE(0, action_under_test_ptr->number_of_tasks());
  EXPECT_TRUE(action_under_test_ptr->put_bucket_tag_body_factory != nullptr);
}

TEST_F(S3PutBucketTaggingActionTest, ValidateRequest) {

  MockBucketTagsStr =
      "<Tagging xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
      "<TagSet><Tag><Key>organization124</Key>"
      "<Value>marketing123</Value>"
      "</Tag><Tag><Key>organization1234</Key>"
      "<Value>marketing123</Value></Tag></TagSet></Tagging>";
  call_count_one = 0;

  EXPECT_CALL(*request_mock, has_all_body_content())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*request_mock, get_full_body_content_as_string())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(MockBucketTagsStr));
  EXPECT_CALL(*(bucket_tag_body_factory_mock->mock_put_bucket_tag_body), isOK())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*(bucket_tag_body_factory_mock->mock_put_bucket_tag_body),
              get_resource_tags_as_map())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(MockBucketTags));

  action_under_test_ptr->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test_ptr,
                         S3PutBucketTaggingActionTest::func_callback_one, this);
  action_under_test_ptr->validate_request();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutBucketTaggingActionTest, ValidateRequestMoreContent) {
  EXPECT_CALL(*request_mock, has_all_body_content()).Times(1).WillOnce(
      Return(false));
  EXPECT_CALL(*request_mock, get_data_length()).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*request_mock, listen_for_incoming_data(_, _)).Times(1);

  action_under_test_ptr->validate_request();
}

TEST_F(S3PutBucketTaggingActionTest, ValidateInvalidRequest) {

  MockBucketTagsStr =
      "<Tagging xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
      "<TagSet><Tag><Key>organization1234</Key>"
      "<Value>marketing123</Value>"
      "</Tag><Tag><Key>organization1234</Key>"
      "<Value>marketing123</Value></Tag></TagSet></Tagging>";

  EXPECT_CALL(*request_mock, has_all_body_content())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*request_mock, get_full_body_content_as_string())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRef(MockBucketTagsStr));
  EXPECT_CALL(*(bucket_tag_body_factory_mock->mock_put_bucket_tag_body), isOK())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->validate_request();
  EXPECT_STREQ("MalformedXML",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3PutBucketTaggingActionTest, ValidateRequestXmlTags) {

  MockBucketTagsStr =
      "<Tagging xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
      "<TagSet><Tag><Key>organization</Key>"
      "<Value>marketing123</Value>"
      "</Tag><Tag><Key>organization1234</Key>"
      "<Value>marketing123</Value></Tag></TagSet></Tagging>";
  call_count_one = 0;

  action_under_test_ptr->put_bucket_tag_body =
      bucket_tag_body_factory_mock->create_put_resource_tags_body(
          MockBucketTagsStr, MockRequestId);

  EXPECT_CALL(*(bucket_tag_body_factory_mock->mock_put_bucket_tag_body),
              validate_bucket_xml_tags(_))
      .Times(1)
      .WillOnce(Return(true));

  action_under_test_ptr->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test_ptr,
                         S3PutBucketTaggingActionTest::func_callback_one, this);
  action_under_test_ptr->validate_request_xml_tags();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutBucketTaggingActionTest, ValidateInvalidRequestXmlTags) {

  // Invalid Key Length
  MockBucketTagsStr =
      "<Tagging xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
      "<TagSet><Tag><Key>ouwjxjnkbnaprgfkkemojuzgoprwnbrtaojoadn"
      "zzqwdyvwakwjslmcrycuehktbrecoy"
      "welcsfvccyyyimherwarvgdozbfk"
      "anqdfujnjzzmhbyvcvnewngiinxbysuzmxyck</Key>"
      "<Value>marketing123</Value>"
      "</Tag><Tag><Key>organization1234</Key>"
      "<Value>marketing123</Value></Tag></TagSet></Tagging>";

  action_under_test_ptr->put_bucket_tag_body =
      bucket_tag_body_factory_mock->create_put_resource_tags_body(
          MockBucketTagsStr, MockRequestId);

  EXPECT_CALL(*(bucket_tag_body_factory_mock->mock_put_bucket_tag_body),
              validate_bucket_xml_tags(_))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));

  action_under_test_ptr->clear_tasks();
  action_under_test_ptr->validate_request_xml_tags();
  EXPECT_STREQ("InvalidTagError",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3PutBucketTaggingActionTest, SetTags) {
  action_under_test_ptr->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillOnce(Return(S3BucketMetadataState::present));
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), set_tags(_))
      .Times(AtLeast(1));
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), save(_, _))
      .Times(AtLeast(1));
  action_under_test_ptr->save_tags_to_bucket_metadata();
}

TEST_F(S3PutBucketTaggingActionTest, SetTagsWhenBucketMissing) {
  action_under_test_ptr->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3BucketMetadataState::missing));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(404, _)).Times(AtLeast(1));
  action_under_test_ptr->fetch_bucket_info_failed();
  EXPECT_STREQ("NoSuchBucket",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3PutBucketTaggingActionTest, SetTagsWhenBucketFailed) {
  action_under_test_ptr->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3BucketMetadataState::failed));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(500, _)).Times(AtLeast(1));
  action_under_test_ptr->fetch_bucket_info_failed();
  EXPECT_STREQ("InternalError",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3PutBucketTaggingActionTest, SetTagsWhenBucketFailedToLaunch) {
  action_under_test_ptr->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3BucketMetadataState::failed_to_launch));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(503, _)).Times(AtLeast(1));
  action_under_test_ptr->fetch_bucket_info_failed();
  EXPECT_STREQ("ServiceUnavailable",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3PutBucketTaggingActionTest, SendResponseToClientServiceUnavailable) {
  action_under_test_ptr->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;

  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*request_mock, pause()).Times(1);
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(503, _)).Times(AtLeast(1));
  action_under_test_ptr->check_shutdown_and_rollback();
  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3PutBucketTaggingActionTest, SendResponseToClientMalformedXML) {
  action_under_test_ptr->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;

  action_under_test_ptr->set_s3_error("MalformedXML");
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3PutBucketTaggingActionTest, SendResponseToClientNoSuchBucket) {
  action_under_test_ptr->set_s3_error("NoSuchBucket");
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(404, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3PutBucketTaggingActionTest, SendResponseToClientSuccess) {
  EXPECT_CALL(*request_mock, send_response(204, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3PutBucketTaggingActionTest, SendResponseToClientInternalError) {
  action_under_test_ptr->set_s3_error("InternalError");
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(500, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

