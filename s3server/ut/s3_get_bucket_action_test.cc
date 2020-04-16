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
#include "s3_get_bucket_action.h"

using ::testing::Invoke;
using ::testing::AtLeast;
using ::testing::ReturnRef;
using ::testing::Return;
using ::testing::_;

#define CREATE_BUCKET_METADATA_OBJ                      \
  do {                                                  \
    action_under_test_ptr->bucket_metadata =            \
        action_under_test_ptr->bucket_metadata_factory  \
            ->create_bucket_metadata_obj(request_mock); \
  } while (0)

#define CREATE_KVS_READER_OBJ                                             \
  do {                                                                    \
    action_under_test_ptr->clovis_kv_reader =                             \
        action_under_test_ptr->s3_clovis_kvs_reader_factory               \
            ->create_clovis_kvs_reader(request_mock, s3_clovis_api_mock); \
  } while (0)

#define CREATE_ACTION_UNDER_TEST_OBJ                                     \
  do {                                                                   \
    EXPECT_CALL(*request_mock, get_query_string_value("encoding-type"))  \
        .Times(AtLeast(1))                                               \
        .WillRepeatedly(Return(""));                                     \
    std::map<std::string, std::string> input_headers;                    \
    input_headers["Authorization"] = "1";                                \
    EXPECT_CALL(*request_mock, get_in_headers_copy()).Times(1).WillOnce( \
        ReturnRef(input_headers));                                       \
    action_under_test_ptr = std::make_shared<S3GetBucketAction>(         \
        request_mock, s3_clovis_api_mock, clovis_kvs_reader_factory,     \
        bucket_meta_factory, object_meta_factory);                       \
  } while (0)

#define SET_NEXT_OBJ_SUCCESSFUL_EXPECTATIONS                                  \
  do {                                                                        \
    EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),         \
                get_key_values())                                             \
        .WillRepeatedly(ReturnRef(result_keys_values));                       \
    EXPECT_CALL(*(object_meta_factory->mock_object_metadata), from_json(_))   \
        .WillRepeatedly(Return(0));                                           \
    EXPECT_CALL(*(object_meta_factory->mock_object_metadata),                 \
                get_content_length_str())                                     \
        .WillRepeatedly(Return("0"));                                         \
    EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())    \
        .WillRepeatedly(Return(S3BucketMetadataState::present));              \
    EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1)); \
    EXPECT_CALL(*request_mock, send_response(200, _)).Times(AtLeast(1));      \
  } while (0)

#define OBJ_METADATA_EXPECTATIONS                                              \
  do {                                                                         \
    EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_md5())       \
        .WillRepeatedly(Return(""));                                           \
    EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_user_name()) \
        .WillRepeatedly(Return("s3user"));                                     \
    EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_user_id())   \
        .WillRepeatedly(Return("1"));                                          \
    EXPECT_CALL(*(object_meta_factory->mock_object_metadata),                  \
                get_storage_class())                                           \
        .WillRepeatedly(Return("STANDARD"));                                   \
    EXPECT_CALL(*(object_meta_factory->mock_object_metadata),                  \
                get_last_modified_iso())                                       \
        .WillRepeatedly(Return("last_modified"));                              \
  } while (0)

class S3GetBucketActionTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.
  S3GetBucketActionTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    object_list_indx_oid = {0x11ffff, 0x1ffff};
    bucket_name = "seagatebucket";
    object_name = "objname";
    request_mock = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    EXPECT_CALL(*request_mock, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));
    EXPECT_CALL(*request_mock, get_object_name())
        .WillRepeatedly(ReturnRef(object_name));

    s3_clovis_api_mock = std::make_shared<MockS3Clovis>();
    bucket_meta_factory = std::make_shared<MockS3BucketMetadataFactory>(
        request_mock, s3_clovis_api_mock);
    clovis_kvs_reader_factory = std::make_shared<MockS3ClovisKVSReaderFactory>(
        request_mock, s3_clovis_api_mock);
    object_meta_factory =
        std::make_shared<MockS3ObjectMetadataFactory>(request_mock);
    object_meta_factory->set_object_list_index_oid(object_list_indx_oid);
  }

  std::shared_ptr<MockS3RequestObject> request_mock;
  std::shared_ptr<S3GetBucketAction> action_under_test_ptr;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::shared_ptr<MockS3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<MockS3ObjectMetadataFactory> object_meta_factory;
  std::shared_ptr<MockS3Clovis> s3_clovis_api_mock;

  struct m0_uint128 object_list_indx_oid;
  std::map<std::string, std::pair<int, std::string>> result_keys_values;
  std::string bucket_name, object_name;
};

TEST_F(S3GetBucketActionTest, Constructor) {
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*request_mock, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  action_under_test_ptr = std::make_shared<S3GetBucketAction>(
      request_mock, s3_clovis_api_mock, clovis_kvs_reader_factory,
      bucket_meta_factory, object_meta_factory);
  EXPECT_NE(0, action_under_test_ptr->number_of_tasks());
  EXPECT_TRUE(action_under_test_ptr->bucket_metadata_factory != nullptr);
  EXPECT_TRUE(action_under_test_ptr->s3_clovis_kvs_reader_factory != nullptr);
  EXPECT_TRUE(action_under_test_ptr->object_metadata_factory != nullptr);
  // EXPECT_STREQ("marker", action_under_test_ptr->last_key.c_str());
  EXPECT_FALSE(action_under_test_ptr->fetch_successful);
}

TEST_F(S3GetBucketActionTest, FetchBucketInfo) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), load(_, _))
      .Times(AtLeast(1));
  action_under_test_ptr->fetch_bucket_info();
}

TEST_F(S3GetBucketActionTest, FetchBucketInfoFailedMissing) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3BucketMetadataState::missing));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(404, _)).Times(AtLeast(1));
  action_under_test_ptr->fetch_bucket_info_failed();
  EXPECT_STREQ("NoSuchBucket",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3GetBucketActionTest, FetchBucketInfoFailedInternalError) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3BucketMetadataState::failed));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(500, _)).Times(AtLeast(1));
  action_under_test_ptr->fetch_bucket_info_failed();
  EXPECT_STREQ("InternalError",
               action_under_test_ptr->get_s3_error_code().c_str());
}

/*
 TODO: Fix this. Clean build fails with this UT on jenkins.

TEST_F(S3GetBucketActionTest, GetNextObjects) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;
  action_under_test_ptr->bucket_metadata->set_object_list_index_oid(
      object_list_indx_oid);

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              next_keyval(_, _, _, _, _, _))
      .Times(1);
  action_under_test_ptr->get_next_objects();
}
*/

TEST_F(S3GetBucketActionTest, GetNextObjectsWithZeroObjects) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::present));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(200, _)).Times(AtLeast(1));
  action_under_test_ptr->get_next_objects();
}

TEST_F(S3GetBucketActionTest, GetNextObjectsFailedNoEntries) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;
  CREATE_KVS_READER_OBJ;

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::missing));
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::present));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(200, _)).Times(AtLeast(1));

  action_under_test_ptr->get_next_objects_failed();
}

TEST_F(S3GetBucketActionTest, GetNextObjectsFailed) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;
  CREATE_KVS_READER_OBJ;

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::failed));
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::present));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(500, _)).Times(AtLeast(1));

  action_under_test_ptr->get_next_objects_failed();
  EXPECT_STREQ("InternalError",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3GetBucketActionTest, GetNextObjectsSuccessful) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;
  CREATE_KVS_READER_OBJ;

  result_keys_values.insert(
      std::make_pair("testkey0", std::make_pair(10, "keyval")));
  result_keys_values.insert(
      std::make_pair("testkey1", std::make_pair(10, "keyval")));
  result_keys_values.insert(
      std::make_pair("testkey2", std::make_pair(10, "keyval")));

  action_under_test_ptr->request_prefix.assign("");
  action_under_test_ptr->request_delimiter.assign("");

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values())
      .WillRepeatedly(ReturnRef(result_keys_values));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), from_json(_))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_content_length_str())
      .WillRepeatedly(Return("0"));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_object_name())
      .WillOnce(Return("testkey0"))
      .WillOnce(Return("testkey1"))
      .WillOnce(Return("testkey2"));

  OBJ_METADATA_EXPECTATIONS;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::present));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(200, _)).Times(AtLeast(1));

  action_under_test_ptr->get_next_objects_successful();
  EXPECT_EQ(3, action_under_test_ptr->object_list.size());
}

TEST_F(S3GetBucketActionTest, GetNextObjectsSuccessfulJsonError) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;
  CREATE_KVS_READER_OBJ;

  result_keys_values.insert(
      std::make_pair("testkey0", std::make_pair(10, "keyval")));
  result_keys_values.insert(
      std::make_pair("testkey1", std::make_pair(10, "keyval")));
  result_keys_values.insert(
      std::make_pair("testkey2", std::make_pair(10, "keyval")));

  action_under_test_ptr->request_prefix.assign("");
  action_under_test_ptr->request_delimiter.assign("");

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values())
      .WillRepeatedly(ReturnRef(result_keys_values));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), from_json(_))
      .WillRepeatedly(Return(-1));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_content_length_str())
      .WillRepeatedly(Return("0"));
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::present));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(200, _)).Times(AtLeast(1));

  action_under_test_ptr->get_next_objects_successful();
  EXPECT_EQ(0, action_under_test_ptr->object_list.size());
}

TEST_F(S3GetBucketActionTest, GetNextObjectsSuccessfulPrefix) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;
  CREATE_KVS_READER_OBJ;

  action_under_test_ptr->request_prefix.assign("test");
  action_under_test_ptr->request_delimiter.assign("");

  result_keys_values.insert(
      std::make_pair("testkey0", std::make_pair(10, "keyval")));
  result_keys_values.insert(
      std::make_pair("testkey1", std::make_pair(10, "keyval")));
  result_keys_values.insert(
      std::make_pair("key2", std::make_pair(10, "keyval")));

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_object_name())
      .WillOnce(Return("testkey0"))
      .WillOnce(Return("testkey1"));
  OBJ_METADATA_EXPECTATIONS;
  SET_NEXT_OBJ_SUCCESSFUL_EXPECTATIONS;

  action_under_test_ptr->get_next_objects_successful();
  EXPECT_EQ(2, action_under_test_ptr->object_list.size());
}

TEST_F(S3GetBucketActionTest, GetNextObjectsSuccessfulDelimiter) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;
  CREATE_KVS_READER_OBJ;

  action_under_test_ptr->request_delimiter.assign("key2");
  action_under_test_ptr->request_prefix.assign("");

  result_keys_values.insert(
      std::make_pair("testkey0", std::make_pair(10, "keyval")));
  result_keys_values.insert(
      std::make_pair("testkey1", std::make_pair(10, "keyval")));
  result_keys_values.insert(
      std::make_pair("testkey2", std::make_pair(10, "keyval")));

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_object_name())
      .WillOnce(Return("testkey0"))
      .WillOnce(Return("testkey1"));
  OBJ_METADATA_EXPECTATIONS;
  SET_NEXT_OBJ_SUCCESSFUL_EXPECTATIONS;

  action_under_test_ptr->get_next_objects_successful();
  EXPECT_EQ(2, action_under_test_ptr->object_list.size());
  EXPECT_EQ(1, action_under_test_ptr->object_list.common_prefixes_size());
}

TEST_F(S3GetBucketActionTest, GetNextObjectsSuccessfulPrefixDelimiter) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;
  CREATE_KVS_READER_OBJ;

  action_under_test_ptr->request_prefix.assign("test");
  action_under_test_ptr->request_delimiter.assign("key");

  result_keys_values.insert(
      std::make_pair("test/some/key", std::make_pair(10, "keyval")));
  result_keys_values.insert(
      std::make_pair("test/some1/key", std::make_pair(10, "keyval")));
  result_keys_values.insert(
      std::make_pair("test/some2/kval", std::make_pair(10, "keyval")));

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_object_name())
      .WillOnce(Return("test/some/key"));
  OBJ_METADATA_EXPECTATIONS;
  SET_NEXT_OBJ_SUCCESSFUL_EXPECTATIONS;

  action_under_test_ptr->get_next_objects_successful();
  EXPECT_EQ(1, action_under_test_ptr->object_list.size());
  EXPECT_EQ(2, action_under_test_ptr->object_list.common_prefixes_size());
}

TEST_F(S3GetBucketActionTest, SendResponseToClientServiceUnavailable) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;

  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*request_mock, pause()).Times(1);
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(503, _)).Times(AtLeast(1));

  action_under_test_ptr->check_shutdown_and_rollback();

  EXPECT_STREQ("ServiceUnavailable",
               action_under_test_ptr->get_s3_error_code().c_str());
  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3GetBucketActionTest, SendResponseToClientNoSuchBucket) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;
  action_under_test_ptr->set_s3_error("NoSuchBucket");

  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(404, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3GetBucketActionTest, SendResponseToClientSuccess) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;

  action_under_test_ptr->fetch_successful = true;
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(200, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3GetBucketActionTest, SendResponseToClientInternalError) {
  CREATE_ACTION_UNDER_TEST_OBJ;
  CREATE_BUCKET_METADATA_OBJ;

  action_under_test_ptr->fetch_successful = false;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::present));
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(500, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

