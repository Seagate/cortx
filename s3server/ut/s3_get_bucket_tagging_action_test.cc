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
#include "s3_get_bucket_tagging_action.h"

using ::testing::Invoke;
using ::testing::AtLeast;
using ::testing::ReturnRef;

class S3GetBucketTaggingActionTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.
  S3GetBucketTaggingActionTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    bucket_name = "seagatebucket";
    request_mock = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    EXPECT_CALL(*request_mock, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));
    bucket_meta_factory =
        std::make_shared<MockS3BucketMetadataFactory>(request_mock);
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*request_mock, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    action_under_test_ptr = std::make_shared<S3GetBucketTaggingAction>(
        request_mock, bucket_meta_factory);
  }

  std::shared_ptr<MockS3RequestObject> request_mock;
  std::shared_ptr<S3GetBucketTaggingAction> action_under_test_ptr;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::string bucket_name;
};

TEST_F(S3GetBucketTaggingActionTest, Constructor) {
  EXPECT_NE(0, action_under_test_ptr->number_of_tasks());
}

TEST_F(S3GetBucketTaggingActionTest, SendResponseToClientNoSuchTagSetError) {
  action_under_test_ptr->bucket_metadata =
      action_under_test_ptr->bucket_metadata_factory
          ->create_bucket_metadata_obj(request_mock);
  action_under_test_ptr->set_s3_error("NoSuchTagSetError");
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(404, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3GetBucketTaggingActionTest, SendResponseToClientServiceUnavailable) {
  action_under_test_ptr->bucket_metadata =
      action_under_test_ptr->bucket_metadata_factory
          ->create_bucket_metadata_obj(request_mock);

  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*request_mock, pause()).Times(1);
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(503, _)).Times(AtLeast(1));
  action_under_test_ptr->check_shutdown_and_rollback();
  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3GetBucketTaggingActionTest, SendResponseToClientNoSuchBucket) {
  action_under_test_ptr->bucket_metadata =
      action_under_test_ptr->bucket_metadata_factory
          ->create_bucket_metadata_obj(request_mock);
  action_under_test_ptr->set_s3_error("NoSuchBucket");
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(404, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3GetBucketTaggingActionTest, SendResponseToClientSuccess) {
  action_under_test_ptr->bucket_metadata =
      action_under_test_ptr->bucket_metadata_factory
          ->create_bucket_metadata_obj(request_mock);

  std::string MockJsonResponse("Mockresponse");
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::present));
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata),
              get_policy_as_json()).WillRepeatedly(ReturnRef(MockJsonResponse));
  EXPECT_CALL(*request_mock, send_response(200, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3GetBucketTaggingActionTest, SendResponseToClientInternalError) {
  action_under_test_ptr->bucket_metadata =
      action_under_test_ptr->bucket_metadata_factory
          ->create_bucket_metadata_obj(request_mock);
  action_under_test_ptr->set_s3_error("InternalError");

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(500, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

