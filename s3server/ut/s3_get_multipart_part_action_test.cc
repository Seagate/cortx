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
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original creation date: 24-March-2017
 */

#include "s3_get_multipart_part_action.h"
#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_abort_multipart_action.h"
#include "s3_test_utils.h"

using ::testing::Return;
using ::testing::_;
using ::testing::ReturnRef;
using ::testing::AtLeast;
using ::testing::Eq;

class S3GetMultipartPartActionTest : public testing::Test {
 protected:
  S3GetMultipartPartActionTest() {
    evhtp_request_t *req = NULL;
    bucket_name_test = "seagatebucket";
    object_name_test = "18MBfile";
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    mp_indx_oid = {0xffff, 0xffff};
    oid = {0x1ffff, 0x1ffff};
    call_count_one = 0;
    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    ptr_mock_s3_clovis_api = std::make_shared<MockS3Clovis>();

    EXPECT_CALL(*ptr_mock_request, get_query_string_value("uploadId"))
        .WillRepeatedly(Return("206440e0-1f5b-4114-9f93-aa96350e4a16"));
    upload_id = "206440e0-1f5b-4114-9f93-aa96350e4a16";
    EXPECT_CALL(*ptr_mock_request, get_query_string_value("part-number-marker"))
        .WillRepeatedly(Return("1"));
    EXPECT_CALL(*ptr_mock_request, get_query_string_value("max-parts"))
        .WillRepeatedly(Return("2"));
    EXPECT_CALL(*ptr_mock_request, get_query_string_value("encoding-type"))
        .WillRepeatedly(Return(""));

    EXPECT_CALL(*ptr_mock_request, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name_test));
    EXPECT_CALL(*ptr_mock_request, get_object_name())
        .WillRepeatedly(ReturnRef(object_name_test));
    bucket_meta_factory =
        std::make_shared<MockS3BucketMetadataFactory>(ptr_mock_request);
    part_meta_factory = std::make_shared<MockS3PartMetadataFactory>(
        ptr_mock_request, oid, upload_id, 0);
    object_mp_meta_factory =
        std::make_shared<MockS3ObjectMultipartMetadataFactory>(
            ptr_mock_request, ptr_mock_s3_clovis_api, upload_id);
    object_mp_meta_factory->set_object_list_index_oid(mp_indx_oid);
    clovis_kvs_reader_factory = std::make_shared<MockS3ClovisKVSReaderFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    part_meta_factory = std::make_shared<MockS3PartMetadataFactory>(
        ptr_mock_request, oid, upload_id, 0);
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));

    action_under_test.reset(new S3GetMultipartPartAction(
        ptr_mock_request, ptr_mock_s3_clovis_api, bucket_meta_factory,
        object_mp_meta_factory, part_meta_factory, clovis_kvs_reader_factory));
  }

  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::shared_ptr<MockS3PartMetadataFactory> part_meta_factory;
  std::shared_ptr<MockS3ObjectMultipartMetadataFactory> object_mp_meta_factory;
  std::shared_ptr<MockS3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<S3GetMultipartPartAction> action_under_test;
  struct m0_uint128 mp_indx_oid;
  struct m0_uint128 oid;
  std::string upload_id;
  std::string object_name_test;
  std::string bucket_name_test;
  int call_count_one;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3GetMultipartPartActionTest, ConstructorTest) {
  struct m0_uint128 zero_oid = {0ULL, 0ULL};
  EXPECT_STREQ("1", action_under_test->last_key.c_str());
  EXPECT_EQ(0, action_under_test->return_list_size);
  EXPECT_FALSE(action_under_test->fetch_successful);
  EXPECT_FALSE(action_under_test->invalid_upload_id);
  EXPECT_TRUE(action_under_test->s3_clovis_api != nullptr);
  EXPECT_STREQ(
      "1", action_under_test->multipart_part_list.request_marker_key.c_str());
  EXPECT_STREQ("206440e0-1f5b-4114-9f93-aa96350e4a16",
               action_under_test->upload_id.c_str());
  EXPECT_OID_EQ(action_under_test->multipart_oid, zero_oid);
  EXPECT_STREQ("206440e0-1f5b-4114-9f93-aa96350e4a16",
               action_under_test->multipart_part_list.upload_id.c_str());
  EXPECT_STREQ("STANDARD",
               action_under_test->multipart_part_list.storage_class.c_str());
  EXPECT_EQ(2, action_under_test->max_parts);
  EXPECT_STREQ("2", action_under_test->multipart_part_list.max_parts.c_str());
  EXPECT_NE(0, action_under_test->number_of_tasks());
}

TEST_F(S3GetMultipartPartActionTest,
       GetMultiPartMetadataPresentOidPresentTest) {
  struct m0_uint128 oid = {0xffff, 0xffff};

  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillOnce(Return(S3BucketMetadataState::present));
  action_under_test->bucket_metadata->set_multipart_index_oid(oid);
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), load(_, _))
      .Times(1);
  action_under_test->get_multipart_metadata();
}

TEST_F(S3GetMultipartPartActionTest, GetMultiPartMetadataPresentOIDNullTest) {
  struct m0_uint128 empty_oid = {0ULL, 0ULL};

  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3BucketMetadataState::present));
  action_under_test->bucket_metadata->set_multipart_index_oid(empty_oid);
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), load(_, _))
      .Times(0);

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  /* No such upload error */
  EXPECT_CALL(*ptr_mock_request, send_response(404, _)).Times(1);

  action_under_test->get_multipart_metadata();
  EXPECT_STREQ("NoSuchUpload", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3GetMultipartPartActionTest, GetMultiPartMetadataMissingTest) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3BucketMetadataState::missing));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(404, _)).Times(1);

  action_under_test->fetch_bucket_info_failed();
  EXPECT_STREQ("NoSuchBucket", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3GetMultipartPartActionTest, GetMultiPartMetadataFailedTest) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3BucketMetadataState::failed));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(1);

  action_under_test->fetch_bucket_info_failed();
  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3GetMultipartPartActionTest, GetMultiPartMetadataFailedToLaunchTest) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3BucketMetadataState::failed_to_launch));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);

  action_under_test->fetch_bucket_info_failed();
  EXPECT_STREQ("ServiceUnavailable",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3GetMultipartPartActionTest,
       GetkeyObjectMetadataPresentUploadIDMatchTest) {
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::present));
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata),
              get_upload_id()).WillRepeatedly(Return(upload_id));
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_keyval(_, "1", _, _)).Times(1);
  action_under_test->get_key_object();
}

TEST_F(S3GetMultipartPartActionTest,
       GetkeyObjectMetadataPresentUploadMisMatchTest) {
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::present));
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata),
              get_upload_id()).WillRepeatedly(Return("upload_id_mismatch"));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(404, _)).Times(1);
  action_under_test->get_key_object();
  EXPECT_TRUE(action_under_test->invalid_upload_id == true);
  EXPECT_STREQ("NoSuchUpload", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3GetMultipartPartActionTest, GetkeyObjectMetadataMissing) {
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::missing));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(404, _)).Times(1);
  action_under_test->get_key_object();
  EXPECT_STREQ("NoSuchUpload", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3GetMultipartPartActionTest, GetkeyObjectMetadataFailed) {
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::failed));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(1);
  action_under_test->get_key_object();
  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3GetMultipartPartActionTest, GetKeyObjectSuccessfulShutdownSet) {
  action_under_test->check_shutdown_signal_for_next_task(true);
  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  action_under_test->get_key_object_successful();
  action_under_test->check_shutdown_signal_for_next_task(false);
  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3GetMultipartPartActionTest, GetKeyObjectSuccessfulValueEmpty) {
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_value())
      .WillRepeatedly(Return(""));
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3GetMultipartPartActionTest::func_callback_one, this);
  action_under_test->get_key_object_successful();

  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3GetMultipartPartActionTest,
       GetKeyObjectSuccessfulValueNotEmptyListSizeSameAsMaxAllowed) {
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_value())
      .WillRepeatedly(Return(
           "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"3kfile\"}"));
  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), from_json(_))
      .WillRepeatedly(Return(0));
  action_under_test->return_list_size = action_under_test->max_parts - 1;
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(200, _)).Times(1);
  action_under_test->get_key_object_successful();
  EXPECT_EQ(2, action_under_test->return_list_size);
  EXPECT_TRUE(action_under_test->multipart_part_list.response_is_truncated);
  EXPECT_TRUE(action_under_test->fetch_successful);
}

TEST_F(S3GetMultipartPartActionTest,
       GetKeyObjectSuccessfulValueNotEmptyListSizeNotSameAsMaxAllowed) {
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_value())
      .WillRepeatedly(Return(
           "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"3kfile\"}"));
  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), from_json(_))
      .WillRepeatedly(Return(0));
  action_under_test->return_list_size = 0;
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3GetMultipartPartActionTest::func_callback_one, this);
  action_under_test->get_key_object_successful();
  EXPECT_EQ(1, action_under_test->return_list_size);
  EXPECT_FALSE(action_under_test->multipart_part_list.response_is_truncated);
  EXPECT_FALSE(action_under_test->fetch_successful);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3GetMultipartPartActionTest,
       GetKeyObjectSuccessfulValueNotEmptyJsonFailed) {
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_value())
      .WillRepeatedly(Return(
           "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"3kfile\"}"));
  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), from_json(_))
      .WillRepeatedly(Return(1));
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3GetMultipartPartActionTest::func_callback_one, this);
  action_under_test->get_key_object_successful();
  EXPECT_FALSE(action_under_test->multipart_part_list.response_is_truncated);
  EXPECT_FALSE(action_under_test->fetch_successful);
  EXPECT_EQ(0, action_under_test->return_list_size);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3GetMultipartPartActionTest, GetKeyObjectFailedNoMetadata) {
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::missing));
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3GetMultipartPartActionTest::func_callback_one, this);
  action_under_test->get_key_object_failed();
  EXPECT_EQ(1, call_count_one);
  EXPECT_TRUE(action_under_test->fetch_successful);
}

TEST_F(S3GetMultipartPartActionTest, GetKeyObjectFailedMetadataFailed) {
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::failed));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(1);

  action_under_test->get_key_object_failed();
  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
  EXPECT_FALSE(action_under_test->fetch_successful);
}

TEST_F(S3GetMultipartPartActionTest, GetNextObjectsMultipartPresent) {
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::present));
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              next_keyval(_, _, _, _, _, _)).Times(1);
  action_under_test->get_next_objects();
}

TEST_F(S3GetMultipartPartActionTest, GetNextObjectsMultipartMissing) {
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::missing));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(404, _)).Times(1);

  action_under_test->get_next_objects();
  EXPECT_STREQ("NoSuchUpload", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3GetMultipartPartActionTest, GetNextObjectsMultipartStateFailed) {
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::failed));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(1);

  action_under_test->get_next_objects();
  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3GetMultipartPartActionTest, GetNextObjectsSuccessfulRollBackSet) {
  action_under_test->check_shutdown_signal_for_next_task(true);
  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  action_under_test->get_next_objects_successful();
  action_under_test->check_shutdown_signal_for_next_task(false);
  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3GetMultipartPartActionTest,
       GetNextObjectsSuccessfulListSizeisMaxAllowed) {
  std::map<std::string, std::pair<int, std::string>> mymap;
  mymap.insert(std::make_pair(
      "0",
      std::make_pair(
          0, "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"1\"}")));
  mymap.insert(std::make_pair(
      "1",
      std::make_pair(
          1, "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"2\"}")));
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values())
      .Times(1)
      .WillOnce(ReturnRef(mymap));
  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), get_md5())
      .WillRepeatedly(Return(""));
  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), get_last_modified_iso())
      .WillRepeatedly(Return("last_modified"));
  EXPECT_CALL(*(part_meta_factory->mock_part_metadata),
              get_content_length_str()).WillRepeatedly(Return("1024"));

  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), from_json(_))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(200, _)).Times(1);

  action_under_test->get_next_objects_successful();

  EXPECT_EQ(2, action_under_test->return_list_size);
  EXPECT_TRUE(action_under_test->multipart_part_list.part_list.size() != 0);
  EXPECT_TRUE(action_under_test->multipart_part_list.response_is_truncated);
  EXPECT_STREQ("1",
               action_under_test->multipart_part_list.next_marker_key.c_str());
  EXPECT_STREQ("1", action_under_test->last_key.c_str());
  EXPECT_TRUE(action_under_test->fetch_successful);
}

TEST_F(S3GetMultipartPartActionTest, GetNextObjectsSuccessfulListNotTruncated) {
  std::map<std::string, std::pair<int, std::string>> mymap;
  mymap.insert(std::make_pair(
      "1",
      std::make_pair(
          0, "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"1\"}")));
  mymap.insert(std::make_pair(
      "file2",
      std::make_pair(
          1, "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"2\"}")));

  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values())
      .Times(1)
      .WillOnce(ReturnRef(mymap));

  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), get_md5())
      .WillRepeatedly(Return(""));

  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), from_json(_))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), get_last_modified_iso())
      .WillRepeatedly(Return("last_modified"));
  EXPECT_CALL(*(part_meta_factory->mock_part_metadata),
              get_content_length_str()).WillRepeatedly(Return("1024"));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(200, _)).Times(1);

  action_under_test->max_parts = 7;
  int old_idx_fetch_count =
      S3Option::get_instance()->get_clovis_idx_fetch_count();
  S3Option::get_instance()->set_clovis_idx_fetch_count(3);
  action_under_test->get_next_objects_successful();

  EXPECT_EQ(2, action_under_test->return_list_size);
  EXPECT_TRUE(action_under_test->multipart_part_list.part_list.size() != 0);
  EXPECT_FALSE(action_under_test->multipart_part_list.response_is_truncated);
  EXPECT_TRUE(action_under_test->fetch_successful);
  S3Option::get_instance()->set_clovis_idx_fetch_count(old_idx_fetch_count);
}

TEST_F(S3GetMultipartPartActionTest, GetNextObjectsSuccessfulGetMoreObjects) {
  std::map<std::string, std::pair<int, std::string>> mymap;
  mymap.insert(std::make_pair(
      "1",
      std::make_pair(
          0, "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"1\"}")));
  mymap.insert(std::make_pair(
      "2",
      std::make_pair(
          1, "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"2\"}")));
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values())
      .Times(1)
      .WillOnce(ReturnRef(mymap));
  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), from_json(_))
      .WillRepeatedly(Return(0));
  action_under_test->max_parts = 7;
  int old_idx_fetch_count =
      S3Option::get_instance()->get_clovis_idx_fetch_count();
  S3Option::get_instance()->set_clovis_idx_fetch_count(1);
  // Expectation in get_next_objects
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::present));

  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              next_keyval(_, _, _, _, _, _)).Times(1);

  action_under_test->get_next_objects_successful();

  EXPECT_TRUE(action_under_test->multipart_part_list.part_list.size() != 0);
  EXPECT_EQ(2, action_under_test->return_list_size);
  EXPECT_STREQ("2", action_under_test->last_key.c_str());
  EXPECT_FALSE(action_under_test->multipart_part_list.response_is_truncated);
  EXPECT_FALSE(action_under_test->fetch_successful);
  S3Option::get_instance()->set_clovis_idx_fetch_count(old_idx_fetch_count);
}

TEST_F(S3GetMultipartPartActionTest, SendInternalErrorResponse) {
  action_under_test->set_s3_error("InternalError");

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3GetMultipartPartActionTest, SendNoSuchBucketErrorResponse) {
  action_under_test->set_s3_error("NoSuchBucket");

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(404, _)).Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3GetMultipartPartActionTest, SendNoSuchUploadErrorResponse) {
  action_under_test->set_s3_error("NoSuchUpload");

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(404, _)).Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3GetMultipartPartActionTest, SendSuccessResponse) {
  action_under_test->fetch_successful = true;
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(200, _)).Times(1);

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3GetMultipartPartActionTest, SendInternalErrorRetry) {
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

