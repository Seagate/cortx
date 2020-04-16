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
 * Original author:  Abhilekh Mustapure   <abhilekh.mustapure@seagate.com>
 * Original creation date: 15-Jan-2019
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_s3_bucket_metadata.h"
#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_delete_bucket_tagging_action.h"

using ::testing::Invoke;
using ::testing::AtLeast;
using ::testing::ReturnRef;

class S3DeleteBucketTaggingActionTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.
  S3DeleteBucketTaggingActionTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    bucket_name = "seagatebucket";
    request_mock = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    EXPECT_CALL(*request_mock, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));
    ptr_mock_s3_clovis_api = std::make_shared<MockS3Clovis>();
    bucket_meta_factory = std::make_shared<MockS3BucketMetadataFactory>(
        request_mock, ptr_mock_s3_clovis_api);
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*request_mock, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    action_under_test_ptr = std::make_shared<S3DeleteBucketTaggingAction>(
        request_mock, bucket_meta_factory);
  }

  std::shared_ptr<MockS3RequestObject> request_mock;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api;
  std::shared_ptr<S3DeleteBucketTaggingAction> action_under_test_ptr;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  int call_count_one;
  std::string bucket_name;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3DeleteBucketTaggingActionTest, Constructor) {
  EXPECT_NE(0, action_under_test_ptr->number_of_tasks());
}

TEST_F(S3DeleteBucketTaggingActionTest, DeleteBucketTaggingSuccessful) {

  action_under_test_ptr->bucket_metadata =
      action_under_test_ptr->bucket_metadata_factory
          ->create_bucket_metadata_obj(request_mock);

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::present));
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata),
              delete_bucket_tags()).Times(1);
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), save(_, _))
      .Times(1);

  action_under_test_ptr->delete_bucket_tags();
}

TEST_F(S3DeleteBucketTaggingActionTest,
       FetchBucketMetadataFailed_Metadatafailed) {
  action_under_test_ptr->bucket_metadata =
      action_under_test_ptr->bucket_metadata_factory
          ->create_bucket_metadata_obj(request_mock);

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::failed));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(500, _)).Times(1);
  action_under_test_ptr->fetch_bucket_info_failed();
}

TEST_F(S3DeleteBucketTaggingActionTest,
       FetchBucketMetadaFailed_Metadatafailedtolaunch) {
  action_under_test_ptr->bucket_metadata =
      action_under_test_ptr->bucket_metadata_factory
          ->create_bucket_metadata_obj(request_mock);

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::failed_to_launch));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(503, _)).Times(1);
  action_under_test_ptr->fetch_bucket_info_failed();
}

TEST_F(S3DeleteBucketTaggingActionTest,
       FetchBucketMetadataFailed_Metadatamissing) {
  action_under_test_ptr->bucket_metadata =
      action_under_test_ptr->bucket_metadata_factory
          ->create_bucket_metadata_obj(request_mock);

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::missing));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(404, _)).Times(1);
  action_under_test_ptr->fetch_bucket_info_failed();
}

TEST_F(S3DeleteBucketTaggingActionTest,
       DeleteBucketTaggingFailed_Metadatafailed) {
  call_count_one = 0;
  action_under_test_ptr->bucket_metadata =
      action_under_test_ptr->bucket_metadata_factory
          ->create_bucket_metadata_obj(request_mock);

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::failed));

  action_under_test_ptr->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test_ptr,
                         S3DeleteBucketTaggingActionTest::func_callback_one,
                         this);
  action_under_test_ptr->delete_bucket_tags_failed();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteBucketTaggingActionTest,
       DeleteBucketTaggingFailed_Metadatafailedtolaunch) {
  call_count_one = 0;
  action_under_test_ptr->bucket_metadata =
      action_under_test_ptr->bucket_metadata_factory
          ->create_bucket_metadata_obj(request_mock);

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::failed_to_launch));

  action_under_test_ptr->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test_ptr,
                         S3DeleteBucketTaggingActionTest::func_callback_one,
                         this);
  action_under_test_ptr->delete_bucket_tags_failed();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteBucketTaggingActionTest, SendResponseToClientNoSuchBucket) {
  action_under_test_ptr->set_s3_error("NoSuchBucket");
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(404, _)).Times(1);
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3DeleteBucketTaggingActionTest, SendResponseToClientSuccess) {
  action_under_test_ptr->bucket_metadata =
      action_under_test_ptr->bucket_metadata_factory
          ->create_bucket_metadata_obj(request_mock);
  EXPECT_CALL(*request_mock, send_response(204, _)).Times(1);
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3DeleteBucketTaggingActionTest, SendResponseToClientInternalError) {
  action_under_test_ptr->set_s3_error("InternalError");
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(500, _)).Times(1);
  action_under_test_ptr->send_response_to_s3_client();
}

