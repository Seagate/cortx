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
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 31-Jan-2019
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_s3_bucket_metadata.h"
#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_global_bucket_index_metadata.h"
#include "s3_callback_test_helpers.h"
#include "s3_test_utils.h"

using ::testing::Invoke;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

#define CREATE_KVS_WRITER_OBJ                                                \
  do {                                                                       \
    global_bucket_idx_metadata_under_test_ptr->clovis_kv_writer =            \
        global_bucket_idx_metadata_under_test_ptr->clovis_kvs_writer_factory \
            ->create_clovis_kvs_writer(request_mock, s3_clovis_api_mock);    \
  } while (0)

#define CREATE_KVS_READER_OBJ                                                \
  do {                                                                       \
    global_bucket_idx_metadata_under_test_ptr->clovis_kv_reader =            \
        global_bucket_idx_metadata_under_test_ptr->clovis_kvs_reader_factory \
            ->create_clovis_kvs_reader(request_mock, s3_clovis_api_mock);    \
  } while (0)

#define CREATE_ACTION_UNDER_TEST_OBJ                                     \
  do {                                                                   \
    request_mock->set_account_name(account_name);                        \
    request_mock->set_account_id(account_id);                            \
    global_bucket_idx_metadata_under_test_ptr =                          \
        std::make_shared<S3GlobalBucketIndexMetadata>(                   \
            request_mock, s3_clovis_api_mock, clovis_kvs_reader_factory, \
            clovis_kvs_writer_factory);                                  \
  } while (0)

class S3GlobalBucketIndexMetadataTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.
  S3GlobalBucketIndexMetadataTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    bucket_list_indx_oid = {0x11ffff, 0x1ffff};
    bucket_name = "seagate";
    request_mock = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    EXPECT_CALL(*request_mock, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));

    s3_clovis_api_mock = std::make_shared<MockS3Clovis>();

    account_id.assign("12345");
    account_name.assign("s3_test");
    call_count_one = 0;
    clovis_kvs_reader_factory = std::make_shared<MockS3ClovisKVSReaderFactory>(
        request_mock, s3_clovis_api_mock);
    clovis_kvs_writer_factory =
        std::make_shared<MockS3ClovisKVSWriterFactory>(request_mock);
    CREATE_ACTION_UNDER_TEST_OBJ;
  }

  std::shared_ptr<MockS3RequestObject> request_mock;
  std::shared_ptr<S3GlobalBucketIndexMetadata>
      global_bucket_idx_metadata_under_test_ptr;
  std::shared_ptr<MockS3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<MockS3ClovisKVSWriterFactory> clovis_kvs_writer_factory;
  std::shared_ptr<MockS3Clovis> s3_clovis_api_mock;

  S3CallBack s3globalbucketindex_callbackobj;
  std::string account_name;
  std::string account_id;
  std::string bucket_name;
  struct m0_uint128 bucket_list_indx_oid;
  std::string mock_json_string;
  int call_count_one;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3GlobalBucketIndexMetadataTest, Constructor) {
  global_bucket_idx_metadata_under_test_ptr->bucket_name = "seagate";
  EXPECT_TRUE(
      global_bucket_idx_metadata_under_test_ptr->clovis_kvs_reader_factory !=
      nullptr);
  EXPECT_TRUE(
      global_bucket_idx_metadata_under_test_ptr->clovis_kvs_writer_factory !=
      nullptr);
  EXPECT_STREQ(account_name.c_str(),
               global_bucket_idx_metadata_under_test_ptr->account_name.c_str());
  EXPECT_STREQ(account_id.c_str(),
               global_bucket_idx_metadata_under_test_ptr->account_id.c_str());
  EXPECT_STREQ(bucket_name.c_str(),
               global_bucket_idx_metadata_under_test_ptr->bucket_name.c_str());
}

TEST_F(S3GlobalBucketIndexMetadataTest, Load) {
  global_bucket_idx_metadata_under_test_ptr->bucket_name = "seagate";
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_keyval(_, bucket_name, _, _)).Times(AtLeast(1));
  global_bucket_idx_metadata_under_test_ptr->load(
      std::bind(&S3CallBack::on_success, &s3globalbucketindex_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3globalbucketindex_callbackobj));
  EXPECT_EQ(S3GlobalBucketIndexMetadataState::missing,
            global_bucket_idx_metadata_under_test_ptr->state);
}

TEST_F(S3GlobalBucketIndexMetadataTest, FromJson) {
  mock_json_string.assign(
      "{\"account_id\":\"12345\",\"account_name\":\"s3_test\","
      "\"location_constraint\":\"us-west-2\"}");
  EXPECT_EQ(0, global_bucket_idx_metadata_under_test_ptr->from_json(
                   mock_json_string));
}

TEST_F(S3GlobalBucketIndexMetadataTest, FromJsonError) {
  mock_json_string.assign(
      "{\"account_id\":\"12345\",\"account_name\":\"s3_test\","
      "\"location_constraint\"\"us-west-2\"}");

  EXPECT_EQ(-1, global_bucket_idx_metadata_under_test_ptr->from_json(
                    mock_json_string));
}

TEST_F(S3GlobalBucketIndexMetadataTest, LoadSuccessful) {
  CREATE_KVS_READER_OBJ;
  mock_json_string.assign(
      "{\"account_id\":\"12345\",\"account_name\":\"s3_test\","
      "\"location_constraint\":\"us-west-2\"}");

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_value())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(mock_json_string));

  global_bucket_idx_metadata_under_test_ptr->handler_on_success =
      std::bind(&S3GlobalBucketIndexMetadataTest::func_callback_one, this);
  global_bucket_idx_metadata_under_test_ptr->load_successful();
  EXPECT_EQ(S3GlobalBucketIndexMetadataState::present,
            global_bucket_idx_metadata_under_test_ptr->state);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3GlobalBucketIndexMetadataTest, LoadSuccessfulJsonError) {
  CREATE_KVS_READER_OBJ;
  mock_json_string.assign(
      "{\"account_id\":\"12345\",\"account_name\":\"s3_test\",\"location_"
      "constraint\"\"us-west-2\"}");

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_value())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(mock_json_string));

  global_bucket_idx_metadata_under_test_ptr->handler_on_failed =
      std::bind(&S3GlobalBucketIndexMetadataTest::func_callback_one, this);
  global_bucket_idx_metadata_under_test_ptr->load_successful();
  EXPECT_EQ(S3GlobalBucketIndexMetadataState::failed,
            global_bucket_idx_metadata_under_test_ptr->state);
  EXPECT_TRUE(global_bucket_idx_metadata_under_test_ptr->json_parsing_error);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3GlobalBucketIndexMetadataTest, LoadFailed) {
  CREATE_KVS_READER_OBJ;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::failed));

  global_bucket_idx_metadata_under_test_ptr->handler_on_failed =
      std::bind(&S3GlobalBucketIndexMetadataTest::func_callback_one, this);
  global_bucket_idx_metadata_under_test_ptr->load_failed();
  EXPECT_EQ(S3GlobalBucketIndexMetadataState::failed,
            global_bucket_idx_metadata_under_test_ptr->state);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3GlobalBucketIndexMetadataTest, LoadFailedMissing) {
  CREATE_KVS_READER_OBJ;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::missing));

  global_bucket_idx_metadata_under_test_ptr->handler_on_failed =
      std::bind(&S3GlobalBucketIndexMetadataTest::func_callback_one, this);
  global_bucket_idx_metadata_under_test_ptr->load_failed();
  EXPECT_EQ(S3GlobalBucketIndexMetadataState::missing,
            global_bucket_idx_metadata_under_test_ptr->state);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3GlobalBucketIndexMetadataTest, SaveSuccessful) {
  global_bucket_idx_metadata_under_test_ptr->handler_on_success =
      std::bind(&S3GlobalBucketIndexMetadataTest::func_callback_one, this);
  global_bucket_idx_metadata_under_test_ptr->save_successful();
  EXPECT_EQ(S3GlobalBucketIndexMetadataState::saved,
            global_bucket_idx_metadata_under_test_ptr->state);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3GlobalBucketIndexMetadataTest, SaveFailed) {
  CREATE_KVS_WRITER_OBJ;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  global_bucket_idx_metadata_under_test_ptr->handler_on_failed =
      std::bind(&S3GlobalBucketIndexMetadataTest::func_callback_one, this);
  global_bucket_idx_metadata_under_test_ptr->save_failed();
  EXPECT_EQ(S3GlobalBucketIndexMetadataState::failed,
            global_bucket_idx_metadata_under_test_ptr->state);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3GlobalBucketIndexMetadataTest, SaveFailedToLaunch) {
  CREATE_KVS_WRITER_OBJ;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  global_bucket_idx_metadata_under_test_ptr->handler_on_failed =
      std::bind(&S3GlobalBucketIndexMetadataTest::func_callback_one, this);
  global_bucket_idx_metadata_under_test_ptr->save_failed();
  EXPECT_EQ(S3GlobalBucketIndexMetadataState::failed_to_launch,
            global_bucket_idx_metadata_under_test_ptr->state);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3GlobalBucketIndexMetadataTest, RemoveSuccessful) {
  global_bucket_idx_metadata_under_test_ptr->handler_on_success =
      std::bind(&S3GlobalBucketIndexMetadataTest::func_callback_one, this);
  global_bucket_idx_metadata_under_test_ptr->remove_successful();
  EXPECT_EQ(S3GlobalBucketIndexMetadataState::deleted,
            global_bucket_idx_metadata_under_test_ptr->state);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3GlobalBucketIndexMetadataTest, RemoveFailed) {
  CREATE_KVS_WRITER_OBJ;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  global_bucket_idx_metadata_under_test_ptr->handler_on_failed =
      std::bind(&S3GlobalBucketIndexMetadataTest::func_callback_one, this);
  global_bucket_idx_metadata_under_test_ptr->remove_failed();
  EXPECT_EQ(S3GlobalBucketIndexMetadataState::failed,
            global_bucket_idx_metadata_under_test_ptr->state);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3GlobalBucketIndexMetadataTest, RemoveFailedToLaunch) {
  CREATE_KVS_WRITER_OBJ;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  global_bucket_idx_metadata_under_test_ptr->handler_on_failed =
      std::bind(&S3GlobalBucketIndexMetadataTest::func_callback_one, this);
  global_bucket_idx_metadata_under_test_ptr->remove_failed();
  EXPECT_EQ(S3GlobalBucketIndexMetadataState::failed_to_launch,
            global_bucket_idx_metadata_under_test_ptr->state);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3GlobalBucketIndexMetadataTest, ToJson) {
  mock_json_string.assign(
      "{\"account_id\":\"12345\",\"account_name\":\"s3_test\","
      "\"location_constraint\":\"us-west-2\"}\n");

  EXPECT_STREQ(mock_json_string.c_str(),
               global_bucket_idx_metadata_under_test_ptr->to_json().c_str());
}

TEST_F(S3GlobalBucketIndexMetadataTest, Save) {
  CREATE_KVS_WRITER_OBJ;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              put_keyval(_, _, _, _, _)).Times(1);

  global_bucket_idx_metadata_under_test_ptr->save(
      std::bind(&S3CallBack::on_success, &s3globalbucketindex_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3globalbucketindex_callbackobj));
  EXPECT_EQ(S3GlobalBucketIndexMetadataState::missing,
            global_bucket_idx_metadata_under_test_ptr->state);
}

TEST_F(S3GlobalBucketIndexMetadataTest, Remove) {
  CREATE_KVS_WRITER_OBJ;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_keyval(_, _, _, _)).Times(1);

  global_bucket_idx_metadata_under_test_ptr->remove(
      std::bind(&S3CallBack::on_success, &s3globalbucketindex_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3globalbucketindex_callbackobj));
}

