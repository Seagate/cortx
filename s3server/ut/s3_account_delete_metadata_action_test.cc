/*
 * COPYRIGHT 2018 SEAGATE LLC
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
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: July-2018
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_s3_bucket_metadata.h"
#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_callback_test_helpers.h"
#include "s3_account_delete_metadata_action.h"
#include "s3_test_utils.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::_;
using ::testing::ReturnRef;
using ::testing::AtLeast;
using ::testing::DefaultValue;

class S3AccountDeleteMetadataActionTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.
  S3AccountDeleteMetadataActionTest() {
    call_count_one = 0;
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    ptr_mock_request->set_account_id("account_test");
    clovis_kvs_reader_factory = std::make_shared<MockS3ClovisKVSReaderFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    action_under_test.reset(new S3AccountDeleteMetadataAction(
        ptr_mock_request, ptr_mock_s3_clovis_api, clovis_kvs_reader_factory));
  }

  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api;
  std::shared_ptr<MockS3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<S3AccountDeleteMetadataAction> action_under_test;

  S3CallBack S3AccountDeleteMetadataAction_callbackobj;

  int call_count_one;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3AccountDeleteMetadataActionTest, Constructor) {
  EXPECT_NE(0, action_under_test->number_of_tasks());
  EXPECT_STREQ("", action_under_test->account_id_from_uri.c_str());
  EXPECT_STREQ("", action_under_test->bucket_account_id_key_prefix.c_str());
}

TEST_F(S3AccountDeleteMetadataActionTest, ValidateRequestSuceess) {
  action_under_test->account_id_from_uri = "account_test";
  EXPECT_STREQ(action_under_test->account_id_from_uri.c_str(),
               ptr_mock_request->get_account_id().c_str());
  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3AccountDeleteMetadataActionTest::func_callback_one,
                         this);
  action_under_test->validate_request();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3AccountDeleteMetadataActionTest, ValidateRequestFailed) {
  action_under_test->account_id_from_uri = "account_test123";
  EXPECT_STRNE(action_under_test->account_id_from_uri.c_str(),
               ptr_mock_request->get_account_id().c_str());

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(400, _)).Times(1);

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3AccountDeleteMetadataActionTest::func_callback_one,
                         this);
  action_under_test->validate_request();
  EXPECT_EQ(0, call_count_one);
  EXPECT_STREQ("InvalidAccountForMgmtApi",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3AccountDeleteMetadataActionTest, FetchFirstBucketMetadata) {
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  action_under_test->account_id_from_uri = "account_test123";
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              next_keyval(_, _, _, _, _, _)).Times(1);

  action_under_test->fetch_first_bucket_metadata();
}

TEST_F(S3AccountDeleteMetadataActionTest, FetchFirstBucketMetadataExist) {
  std::map<std::string, std::pair<int, std::string>> mymap;
  action_under_test->bucket_account_id_key_prefix = "12345/";
  mymap.insert(std::make_pair(
      "12345/seagate_bucket",
      std::make_pair(
          0,
          "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"file1\"}")));
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values())
      .Times(1)
      .WillOnce(ReturnRef(mymap));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(409, _)).Times(AtLeast(1));
  action_under_test->fetch_first_bucket_metadata_successful();
  EXPECT_STREQ("AccountNotEmpty",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3AccountDeleteMetadataActionTest, FetchFirstBucketMetadataNotExist) {
  std::map<std::string, std::pair<int, std::string>> mymap;
  action_under_test->bucket_account_id_key_prefix = "4567/";
  mymap.insert(std::make_pair(
      "12345/seagate_bucket",
      std::make_pair(
          0,
          "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"file1\"}")));
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values())
      .Times(1)
      .WillOnce(ReturnRef(mymap));

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3AccountDeleteMetadataActionTest::func_callback_one,
                         this);
  action_under_test->fetch_first_bucket_metadata_successful();
}

TEST_F(S3AccountDeleteMetadataActionTest, FetchFirstBucketMetadataMissing) {
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::missing));
  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3AccountDeleteMetadataActionTest::func_callback_one,
                         this);
  action_under_test->fetch_first_bucket_metadata_failed();
}

TEST_F(S3AccountDeleteMetadataActionTest, FetchFirstBucketMetadataFailed) {
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::failed));
  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3AccountDeleteMetadataActionTest::func_callback_one,
                         this);
  action_under_test->fetch_first_bucket_metadata_failed();
  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3AccountDeleteMetadataActionTest, SendResponseToInternalError) {
  action_under_test->set_s3_error("InternalError");
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(AtLeast(1));
  action_under_test->send_response_to_s3_client();
}

TEST_F(S3AccountDeleteMetadataActionTest, SendSuccessResponse) {
  EXPECT_CALL(*ptr_mock_request, send_response(204, _)).Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}
