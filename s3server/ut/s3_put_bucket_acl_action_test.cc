/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original author:  Rajesh Nambiar   <rajesh.nambiarr@seagate.com>
 * Original creation date: 03-June-2016
 */

#include "s3_put_bucket_acl_action.h"
#include "gtest/gtest.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_error_codes.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;
using ::testing::AtLeast;

#define CREATE_BUCKET_METADATA                                            \
  do {                                                                    \
    EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), load(_, _)) \
        .Times(AtLeast(1));                                               \
    action_under_test->fetch_bucket_info();                               \
  } while (0)

class S3PutBucketAclActionTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.
  S3PutBucketAclActionTest() {
    call_count_one = 0;
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    bucket_name = "seagatebucket";
    mock_request = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    EXPECT_CALL(*mock_request, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));
    bucket_meta_factory =
        std::make_shared<MockS3BucketMetadataFactory>(mock_request);
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    action_under_test.reset(
        new S3PutBucketACLAction(mock_request, bucket_meta_factory));
  }

  std::shared_ptr<S3PutBucketACLAction> action_under_test;
  std::shared_ptr<MockS3RequestObject> mock_request;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  int call_count_one;
  std::string bucket_name;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3PutBucketAclActionTest, Constructor) {
  EXPECT_NE(0, action_under_test->number_of_tasks());
}

TEST_F(S3PutBucketAclActionTest, ValidateOnAllDataShouldCallNext) {
  std::string acl = "<AccessControlPolicy>...</AccessControlPolicy>";

  EXPECT_CALL(*mock_request, has_all_body_content()).WillOnce(Return(true));
  EXPECT_CALL(*mock_request, get_full_body_content_as_string())
      .WillOnce(ReturnRef(acl));

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutBucketAclActionTest::func_callback_one, this);

  action_under_test->validate_request();

  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutBucketAclActionTest, ValidateOnPartialDataShouldWaitForMore) {
  std::string acl = "<AccessControlPolicy>...</AccessControlPolicy>";

  EXPECT_CALL(*mock_request, has_all_body_content()).WillOnce(Return(false));
  EXPECT_CALL(*mock_request, listen_for_incoming_data(_, _)).Times(1);
  EXPECT_CALL(*mock_request, get_data_length()).Times(1).WillOnce(Return(0));

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutBucketAclActionTest::func_callback_one, this);

  action_under_test->validate_request();
}

TEST_F(S3PutBucketAclActionTest, FetchBucketInfo) {
  CREATE_BUCKET_METADATA;
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
}

TEST_F(S3PutBucketAclActionTest, FetchBucketInfoFailedWithMissing) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::missing));

  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed404, _)).Times(1);

  action_under_test->fetch_bucket_info_failed();

  EXPECT_STREQ("NoSuchBucket", action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
}

TEST_F(S3PutBucketAclActionTest, FetchBucketInfoFailed) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::failed));

  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed500, _)).Times(1);

  action_under_test->fetch_bucket_info_failed();

  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
}

TEST_F(S3PutBucketAclActionTest, SetAclShouldUpdateMetadata) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), setacl(_)).Times(1);
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), save(_, _))
      .Times(1);

  action_under_test->setacl();
}

TEST_F(S3PutBucketAclActionTest, SendResponseWhenShuttingDown) {
  S3Option::get_instance()->set_is_s3_shutting_down(true);

  EXPECT_CALL(*mock_request, pause()).Times(1);
  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, set_out_header_value(Eq("Retry-After"), Eq("1")))
      .Times(1);
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed503, _))
      .Times(AtLeast(1));

  // send_response_to_s3_client is called in check_shutdown_and_rollback
  action_under_test->check_shutdown_and_rollback();

  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3PutBucketAclActionTest, SendErrorResponse) {
  action_under_test->set_s3_error("InternalError");

  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed500, _))
      .Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3PutBucketAclActionTest, SendAnyFailedResponse) {
  action_under_test->set_s3_error("NoSuchBucket");

  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed404, _))
      .Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

