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
 * Original author: Priya Saboo <priya.chhagan@seagate.com>
 * Original creation date: 10-May-2017
 */
#include "s3_get_bucket_location_action.h"
#include "gtest/gtest.h"
#include "mock_s3_bucket_metadata.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_error_codes.h"
#include "s3_option.h"

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

class S3GetBucketLocationActionTest : public testing::Test {
 protected:
  std::shared_ptr<S3GetBucketlocationAction> action_under_test;
  std::shared_ptr<MockS3RequestObject> mock_request;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::shared_ptr<MockS3AsyncBufferOptContainerFactory> async_buffer_factory;
  std::string bucket_name;

  S3GetBucketLocationActionTest() {
    S3Option::get_instance()->disable_auth();
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    bucket_name = "seagatebucket";
    mock_request = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    EXPECT_CALL(*mock_request, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));

    async_buffer_factory =
        std::make_shared<MockS3AsyncBufferOptContainerFactory>(
            S3Option::get_instance()->get_libevent_pool_buffer_size());
    bucket_meta_factory =
        std::make_shared<MockS3BucketMetadataFactory>(mock_request);
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    action_under_test = std::make_shared<S3GetBucketlocationAction>(
        mock_request, bucket_meta_factory);
  }
};

TEST_F(S3GetBucketLocationActionTest, ConstructorTest) {
  EXPECT_NE(0, action_under_test->number_of_tasks());
}

TEST_F(S3GetBucketLocationActionTest, BucketMetadataMustNotBeNull) {
  // This will initialize bucket_metadata field.
  CREATE_BUCKET_METADATA;
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
}

TEST_F(S3GetBucketLocationActionTest, FetchBucketInfoFailedWithMissing) {
  CREATE_BUCKET_METADATA;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::missing));

  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed404, _)).Times(1);

  action_under_test->fetch_bucket_info_failed();
  EXPECT_STREQ("NoSuchBucket", action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
}

TEST_F(S3GetBucketLocationActionTest, FetchBucketInfoFailedWithFailed) {
  CREATE_BUCKET_METADATA;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::failed));
  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed500, _)).Times(1);
  action_under_test->fetch_bucket_info_failed();
  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
}

TEST_F(S3GetBucketLocationActionTest, SendResponseWhenShuttingDown) {
  S3Option::get_instance()->set_is_s3_shutting_down(true);

  EXPECT_CALL(*mock_request, pause()).Times(1);
  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, set_out_header_value(Eq("Retry-After"), Eq("1")))
      .Times(1);
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed503, _))
      .Times(AtLeast(1));
  action_under_test->check_shutdown_and_rollback();

  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3GetBucketLocationActionTest, SendErrorResponse) {
  action_under_test->set_s3_error("InternalError");
  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed500, _))
      .Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3GetBucketLocationActionTest, SendAnyFailedResponse) {
  action_under_test->set_s3_error("NoSuchBucket");
  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed404, _))
      .Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}
TEST_F(S3GetBucketLocationActionTest, SendAnySuccessResponse) {
  CREATE_BUCKET_METADATA;
  EXPECT_CALL((*bucket_meta_factory->mock_bucket_metadata), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3BucketMetadataState::present));

  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpSuccess200, _))
      .Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

