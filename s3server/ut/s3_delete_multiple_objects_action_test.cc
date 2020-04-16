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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 28-Apr-2017
 */

#include <memory>

#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_factory.h"
#include "s3_delete_multiple_objects_action.h"
#include "s3_error_codes.h"
#include "s3_test_utils.h"
#include "s3_ut_common.h"
#include "s3_m0_uint128_helper.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Invoke;
using ::testing::_;
using ::testing::AtLeast;

#define CREATE_BUCKET_METADATA                                            \
  do {                                                                    \
    EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), load(_, _)) \
        .Times(AtLeast(1));                                               \
    action_under_test->fetch_bucket_info();                               \
  } while (0)

#define SAMPLE_DELETE_REQUEST          \
  "<Delete>"                           \
  "  <Object>"                         \
  "    <Key>SampleDocument1.txt</Key>" \
  "  </Object>"                        \
  "  <Object>"                         \
  "    <Key>SampleDocument2.txt</Key>" \
  "  </Object>"                        \
  "</Delete>"

#define INVALID_DELETE_REQUEST         \
  "<Delete>"                           \
  "  <Object>"                         \
  "    <Key>SampleDocument1.txt</Key>" \
  "  </Object>"                        \
  "  <Object>"                         \
  "    <Key>SampleDocument2.txt</Key>" \
  "  </Object>"
// "</Delete>" tag missing

class S3DeleteMultipleObjectsActionTest : public testing::Test {
 protected:
  S3DeleteMultipleObjectsActionTest() {
    S3Option::get_instance()->disable_auth();

    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();

    oid = {0x1ffff, 0x1ffff};
    object_list_indx_oid = {0x11ffff, 0x1ffff};
    objects_version_list_indx_oid = {0x11ffff, 0x1fff0};
    bucket_name = "seagatebucket";
    object_name = "obj_abc";
    call_count_one = 0;

    layout_id =
        S3ClovisLayoutMap::get_instance()->get_best_layout_for_object_size();

    async_buffer_factory =
        std::make_shared<MockS3AsyncBufferOptContainerFactory>(
            S3Option::get_instance()->get_libevent_pool_buffer_size());

    mock_request = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr,
                                                         async_buffer_factory);
    EXPECT_CALL(*mock_request, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));
    EXPECT_CALL(*mock_request, get_object_name())
        .WillRepeatedly(ReturnRef(object_name));

    ptr_mock_s3_clovis_api = std::make_shared<MockS3Clovis>();

    EXPECT_CALL(*ptr_mock_s3_clovis_api, m0_h_ufid_next(_))
        .WillRepeatedly(Invoke(dummy_helpers_ufid_next));

    keys = {"SampleDocument1.txt", "SampleDocument2.txt"};

    // Owned and deleted by shared_ptr in S3DeleteMultipleObjectsAction
    bucket_meta_factory = std::make_shared<MockS3BucketMetadataFactory>(
        mock_request, ptr_mock_s3_clovis_api);

    object_meta_factory = std::make_shared<MockS3ObjectMetadataFactory>(
        mock_request, ptr_mock_s3_clovis_api);
    object_meta_factory->set_object_list_index_oid(object_list_indx_oid);

    clovis_writer_factory = std::make_shared<MockS3ClovisWriterFactory>(
        mock_request, oid, ptr_mock_s3_clovis_api);

    clovis_kvs_reader_factory = std::make_shared<MockS3ClovisKVSReaderFactory>(
        mock_request, ptr_mock_s3_clovis_api);

    clovis_kvs_writer_factory = std::make_shared<MockS3ClovisKVSWriterFactory>(
        mock_request, ptr_mock_s3_clovis_api);
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*mock_request, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    action_under_test.reset(new S3DeleteMultipleObjectsAction(
        mock_request, bucket_meta_factory, object_meta_factory,
        clovis_writer_factory, clovis_kvs_reader_factory,
        clovis_kvs_writer_factory));
  }

  std::shared_ptr<MockS3RequestObject> mock_request;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::shared_ptr<MockS3ObjectMetadataFactory> object_meta_factory;
  std::shared_ptr<MockS3ClovisWriterFactory> clovis_writer_factory;
  std::shared_ptr<MockS3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<MockS3ClovisKVSWriterFactory> clovis_kvs_writer_factory;
  std::shared_ptr<MockS3AsyncBufferOptContainerFactory> async_buffer_factory;
  std::vector<std::string> keys;

  std::shared_ptr<S3DeleteMultipleObjectsAction> action_under_test;

  struct m0_uint128 object_list_indx_oid;
  struct m0_uint128 objects_version_list_indx_oid;
  struct m0_uint128 oid;
  std::string bucket_name, object_name;

  int call_count_one;
  int layout_id;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3DeleteMultipleObjectsActionTest, ConstructorTest) {
  EXPECT_NE(0, action_under_test->number_of_tasks());
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       ValidateOnAllDataShouldCallNext4ValidData) {
  std::string request_data = SAMPLE_DELETE_REQUEST;

  EXPECT_CALL(*mock_request, has_all_body_content()).WillOnce(Return(true));
  EXPECT_CALL(*mock_request, get_full_body_content_as_string())
      .WillOnce(ReturnRef(request_data));
  EXPECT_CALL(*mock_request, get_header_value(_))
      .WillOnce(Return("vxQpICn70jvA6+9R0/d5iA=="));

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteMultipleObjectsActionTest::func_callback_one,
                         this);

  action_under_test->validate_request();

  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       ValidateOnAllDataShouldError4InvalidData) {
  std::string request_data = INVALID_DELETE_REQUEST;

  EXPECT_CALL(*mock_request, has_all_body_content()).WillOnce(Return(true));
  EXPECT_CALL(*mock_request, get_full_body_content_as_string())
      .WillOnce(ReturnRef(request_data));
  EXPECT_CALL(*mock_request, get_header_value(_))
      .WillOnce(Return("E07TDc6qXbxVZHpZPziZ4w=="));

  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed400, _)).Times(1);
  EXPECT_CALL(*mock_request, resume(_)).Times(1);

  action_under_test->validate_request();

  EXPECT_STREQ("MalformedXML", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       ValidateOnPartialDataShouldWaitForMore) {
  std::string request_data = INVALID_DELETE_REQUEST;

  EXPECT_CALL(*mock_request, has_all_body_content()).WillOnce(Return(false));
  EXPECT_CALL(*mock_request, get_data_length()).WillOnce(Return(1024));
  EXPECT_CALL(*mock_request, listen_for_incoming_data(_, _)).Times(1);

  action_under_test->validate_request();
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       ConsumeOnAllDataShouldCallNext4ValidData) {
  std::string request_data = SAMPLE_DELETE_REQUEST;

  EXPECT_CALL(*mock_request, has_all_body_content()).WillOnce(Return(true));
  EXPECT_CALL(*mock_request, get_full_body_content_as_string())
      .WillOnce(ReturnRef(request_data));
  EXPECT_CALL(*mock_request, get_header_value(_))
      .WillOnce(Return("vxQpICn70jvA6+9R0/d5iA=="));

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteMultipleObjectsActionTest::func_callback_one,
                         this);

  action_under_test->consume_incoming_content();

  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteMultipleObjectsActionTest, ConsumeOnPartialDataShouldDoNothing) {
  std::string request_data = SAMPLE_DELETE_REQUEST;

  EXPECT_CALL(*mock_request, has_all_body_content()).WillOnce(Return(false));

  action_under_test->consume_incoming_content();
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       FetchBucketInfoFailedBucketNotPresent) {
  CREATE_BUCKET_METADATA;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::missing));
  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed404, _)).Times(1);
  EXPECT_CALL(*mock_request, resume(_)).Times(1);

  action_under_test->fetch_bucket_info_failed();

  EXPECT_STREQ("NoSuchBucket", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3DeleteMultipleObjectsActionTest, FetchBucketInfoFailedWithError) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::failed));
  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed500, _)).Times(1);
  EXPECT_CALL(*mock_request, resume(_)).Times(1);

  action_under_test->fetch_bucket_info_failed();

  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       FetchObjectInfoWhenBucketPresentAndObjIndexAbsent) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpSuccess200, _)).Times(1);
  EXPECT_CALL(*mock_request, resume(_)).Times(1);
  action_under_test->fetch_objects_info();
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       FetchObjectInfoWhenBucketAndObjIndexPresent) {
  CREATE_BUCKET_METADATA;
  bucket_meta_factory->mock_bucket_metadata->set_object_list_index_oid(
      object_list_indx_oid);
  EXPECT_CALL(*mock_request, get_header_value(_))
      .WillOnce(Return("vxQpICn70jvA6+9R0/d5iA=="));
  // Clear tasks so validate_request_body calls mocked next
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteMultipleObjectsActionTest::func_callback_one,
                         this);
  action_under_test->validate_request_body(SAMPLE_DELETE_REQUEST);

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_keyval(_, keys, _, _)).Times(AtLeast(1));
  action_under_test->fetch_objects_info();
  EXPECT_EQ(2, action_under_test->delete_index_in_req);
}

TEST_F(S3DeleteMultipleObjectsActionTest, FetchObjectInfoFailed) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .Times(AtLeast(1))
      .WillOnce(Return(S3ClovisKVSReaderOpState::failed));

  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed500, _)).Times(1);
  EXPECT_CALL(*mock_request, resume(_)).Times(1);
  action_under_test->fetch_objects_info_failed();

  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       FetchObjectInfoFailedWithMissingAndMoreToProcess) {
  std::vector<std::string> missing_key = {"SampleDocument2.txt"};
  CREATE_BUCKET_METADATA;

  bucket_meta_factory->mock_bucket_metadata->set_object_list_index_oid(
      object_list_indx_oid);
  EXPECT_CALL(*mock_request, get_header_value(_))
      .WillOnce(Return("vxQpICn70jvA6+9R0/d5iA=="));
  // Clear tasks so validate_request_body calls mocked next
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteMultipleObjectsActionTest::func_callback_one,
                         this);
  action_under_test->validate_request_body(SAMPLE_DELETE_REQUEST);
  action_under_test->keys_to_delete.push_back("SampleDocument1.txt");
  action_under_test->delete_index_in_req = 1;

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .Times(AtLeast(1))
      .WillOnce(Return(S3ClovisKVSReaderOpState::missing));
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_keyval(_, missing_key, _, _)).Times(AtLeast(1));
  action_under_test->fetch_objects_info_failed();
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       FetchObjectInfoFailedWithMissingAllDone) {
  CREATE_BUCKET_METADATA;

  bucket_meta_factory->mock_bucket_metadata->set_object_list_index_oid(
      object_list_indx_oid);
  EXPECT_CALL(*mock_request, get_header_value(_))
      .WillOnce(Return("vxQpICn70jvA6+9R0/d5iA=="));
  // Clear tasks so validate_request_body calls mocked next
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteMultipleObjectsActionTest::func_callback_one,
                         this);
  action_under_test->validate_request_body(SAMPLE_DELETE_REQUEST);
  action_under_test->keys_to_delete.push_back("SampleDocument1.txt");
  action_under_test->keys_to_delete.push_back("SampleDocument1.txt");
  action_under_test->delete_index_in_req = 2;

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .Times(AtLeast(1))
      .WillOnce(Return(S3ClovisKVSReaderOpState::missing));

  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpSuccess200, _)).Times(1);
  EXPECT_CALL(*mock_request, resume(_)).Times(1);
  action_under_test->fetch_objects_info_failed();
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       FetchObjectsInfoSuccessful4FewMissingObjs) {
  std::map<std::string, std::pair<int, std::string>> result_keys_values;
  result_keys_values.insert(
      std::make_pair("testkey0", std::make_pair(0, "dummyobjinfoplaceholder")));
  result_keys_values.insert(std::make_pair(
      "testkey1", std::make_pair(-ENOENT, "dummyobjinfoplaceholder")));
  result_keys_values.insert(std::make_pair(
      "testkey2", std::make_pair(-ENOENT, "dummyobjinfoplaceholder")));

  CREATE_BUCKET_METADATA;

  bucket_meta_factory->mock_bucket_metadata->set_object_list_index_oid(
      object_list_indx_oid);
  bucket_meta_factory->mock_bucket_metadata->set_objects_version_list_index_oid(
      objects_version_list_indx_oid);

  // mock kv reader/writer
  action_under_test->clovis_kv_reader =
      action_under_test->clovis_kvs_reader_factory->create_clovis_kvs_reader(
          mock_request);
  action_under_test->clovis_kv_writer =
      action_under_test->clovis_kvs_writer_factory->create_clovis_kvs_writer(
          mock_request);

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values()).WillRepeatedly(ReturnRef(result_keys_values));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), from_json(_))
      .Times(1)
      .WillRepeatedly(Return(0));

  // Few expectations for add_object_oid_to_probable_dead_oid_list
  object_meta_factory->mock_object_metadata->regenerate_version_id();
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::present));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_object_name())
      .WillRepeatedly(Return("objname"));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_oid())
      .WillRepeatedly(Return(oid));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_layout_id())
      .WillRepeatedly(Return(layout_id));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_version_key_in_index()).WillRepeatedly(Return("objname/v1"));
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              put_keyval(_, _, _, _)).Times(1);

  action_under_test->fetch_objects_info_successful();

  EXPECT_EQ(1, action_under_test->objects_metadata.size());
  EXPECT_EQ(2, action_under_test->delete_objects_response.get_success_count());
  EXPECT_EQ(0, action_under_test->delete_objects_response.get_failure_count());
}

TEST_F(S3DeleteMultipleObjectsActionTest, FetchObjectsInfoSuccessful) {
  std::map<std::string, std::pair<int, std::string>> result_keys_values;
  result_keys_values.insert(
      std::make_pair("testkey0", std::make_pair(0, "dummyobjinfoplaceholder")));
  result_keys_values.insert(
      std::make_pair("testkey1", std::make_pair(0, "dummyobjinfoplaceholder")));
  result_keys_values.insert(
      std::make_pair("testkey2", std::make_pair(0, "dummyobjinfoplaceholder")));

  CREATE_BUCKET_METADATA;

  bucket_meta_factory->mock_bucket_metadata->set_object_list_index_oid(
      object_list_indx_oid);
  bucket_meta_factory->mock_bucket_metadata->set_objects_version_list_index_oid(
      objects_version_list_indx_oid);

  // mock kv reader/writer
  action_under_test->clovis_kv_reader =
      action_under_test->clovis_kvs_reader_factory->create_clovis_kvs_reader(
          mock_request);
  action_under_test->clovis_kv_writer =
      action_under_test->clovis_kvs_writer_factory->create_clovis_kvs_writer(
          mock_request);

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values()).WillRepeatedly(ReturnRef(result_keys_values));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), from_json(_))
      .Times(3)
      .WillRepeatedly(Return(0));

  // Few expectations for add_object_oid_to_probable_dead_oid_list
  object_meta_factory->mock_object_metadata->regenerate_version_id();
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::present));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_object_name())
      .WillRepeatedly(Return("objname"));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_oid())
      .WillRepeatedly(Return(oid));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_layout_id())
      .WillRepeatedly(Return(layout_id));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_version_key_in_index()).WillRepeatedly(Return("objname/v1"));
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              put_keyval(_, _, _, _)).Times(1);

  action_under_test->fetch_objects_info_successful();

  EXPECT_EQ(3, action_under_test->objects_metadata.size());
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       FetchObjectsInfoSuccessfulJsonErrors) {
  std::map<std::string, std::pair<int, std::string>> result_keys_values;
  result_keys_values.insert(
      std::make_pair("testkey0", std::make_pair(0, "dummyobjinfoplaceholder")));
  result_keys_values.insert(
      std::make_pair("testkey1", std::make_pair(0, "dummyobjinfoplaceholder")));
  result_keys_values.insert(
      std::make_pair("testkey2", std::make_pair(0, "dummyobjinfoplaceholder")));

  CREATE_BUCKET_METADATA;

  bucket_meta_factory->mock_bucket_metadata->set_object_list_index_oid(
      object_list_indx_oid);
  bucket_meta_factory->mock_bucket_metadata->set_objects_version_list_index_oid(
      objects_version_list_indx_oid);

  // mock kv reader/writer
  action_under_test->clovis_kv_reader =
      action_under_test->clovis_kvs_reader_factory->create_clovis_kvs_reader(
          mock_request);
  action_under_test->clovis_kv_writer =
      action_under_test->clovis_kvs_writer_factory->create_clovis_kvs_writer(
          mock_request);

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values()).WillRepeatedly(ReturnRef(result_keys_values));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), from_json(_))
      .Times(3)
      .WillRepeatedly(Return(-1));

  action_under_test->at_least_one_delete_successful = true;

  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpSuccess200, _))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_request, resume(_)).Times(1);

  action_under_test->fetch_objects_info_successful();

  EXPECT_EQ(0, action_under_test->oids_to_delete.size());
  EXPECT_EQ(0, action_under_test->objects_metadata.size());
  EXPECT_EQ(0, action_under_test->delete_objects_response.get_success_count());
  EXPECT_EQ(3, action_under_test->delete_objects_response.get_failure_count());
}

TEST_F(S3DeleteMultipleObjectsActionTest, DeleteObjectMetadata) {
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_keyval(_, _, _, _)).Times(1);

  action_under_test->delete_objects_metadata();
}

TEST_F(S3DeleteMultipleObjectsActionTest, DeleteObjectMetadataSucceeded) {
  action_under_test->objects_metadata.push_back(
      object_meta_factory->create_object_metadata_obj(mock_request));

  action_under_test->oids_to_delete.push_back(oid);
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_oid())
      .WillRepeatedly(Return(oid));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_object_name())
      .WillOnce(Return("objname"));

  action_under_test->delete_objects_metadata_successful();

  EXPECT_TRUE(action_under_test->at_least_one_delete_successful);
  EXPECT_EQ(1, action_under_test->delete_objects_response.get_success_count());
  EXPECT_EQ(0, action_under_test->delete_objects_response.get_failure_count());
}

TEST_F(S3DeleteMultipleObjectsActionTest, DeleteObjectMetadataFailedToLaunch) {
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(500, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, resume(_)).Times(1);

  action_under_test->delete_objects_metadata_failed();

  EXPECT_FALSE(action_under_test->at_least_one_delete_successful);
  EXPECT_EQ(0, action_under_test->delete_objects_response.get_success_count());
  EXPECT_EQ(0, action_under_test->delete_objects_response.get_failure_count());
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       DeleteObjectMetadataFailedWithMissing) {
  action_under_test->objects_metadata.push_back(
      object_meta_factory->create_object_metadata_obj(mock_request));
  action_under_test->objects_metadata.push_back(
      object_meta_factory->create_object_metadata_obj(mock_request));

  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              get_op_ret_code_for_del_kv(_))
      .Times(2)
      .WillRepeatedly(Return(-ENOENT));
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpSuccess200, _))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_request, resume(_)).Times(1);
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_object_name())
      .WillRepeatedly(Return("objname"));

  action_under_test->delete_objects_metadata_failed();

  EXPECT_TRUE(action_under_test->at_least_one_delete_successful);
  EXPECT_EQ(2, action_under_test->delete_objects_response.get_success_count());
  EXPECT_EQ(0, action_under_test->delete_objects_response.get_failure_count());
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       DeleteObjectMetadataFailedWithErrors) {
  action_under_test->objects_metadata.push_back(
      object_meta_factory->create_object_metadata_obj(mock_request));
  action_under_test->objects_metadata.push_back(
      object_meta_factory->create_object_metadata_obj(mock_request));

  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              get_op_ret_code_for_del_kv(_))
      .Times(2)
      .WillRepeatedly(Return(-ENETUNREACH));
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed500, _))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_request, resume(_)).Times(1);
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_object_name())
      .WillRepeatedly(Return("objname"));

  action_under_test->delete_objects_metadata_failed();

  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
  EXPECT_FALSE(action_under_test->at_least_one_delete_successful);
  EXPECT_EQ(0, action_under_test->delete_objects_response.get_success_count());
  EXPECT_EQ(2, action_under_test->delete_objects_response.get_failure_count());
}

TEST_F(S3DeleteMultipleObjectsActionTest,
       DeleteObjectMetadataFailedMoreToProcess) {
  std::vector<std::string> my_keys;
  my_keys.clear();
  CREATE_BUCKET_METADATA;

  bucket_meta_factory->mock_bucket_metadata->set_object_list_index_oid(
      object_list_indx_oid);
  EXPECT_CALL(*mock_request, get_header_value(_))
      .WillOnce(Return("vxQpICn70jvA6+9R0/d5iA=="));
  // Clear tasks so validate_request_body calls mocked next
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteMultipleObjectsActionTest::func_callback_one,
                         this);
  action_under_test->validate_request_body(SAMPLE_DELETE_REQUEST);

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_keyval(_, my_keys, _, _)).Times(AtLeast(1));
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));

  action_under_test->objects_metadata.push_back(
      object_meta_factory->create_object_metadata_obj(mock_request));
  action_under_test->objects_metadata.push_back(
      object_meta_factory->create_object_metadata_obj(mock_request));

  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              get_op_ret_code_for_del_kv(_))
      .Times(2)
      .WillRepeatedly(Return(-ENOENT));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_object_name())
      .WillRepeatedly(Return("objname"));

  action_under_test->delete_index_in_req = -1;  // simulate
  action_under_test->delete_objects_metadata_failed();

  EXPECT_TRUE(action_under_test->at_least_one_delete_successful);
  EXPECT_EQ(2, action_under_test->delete_objects_response.get_success_count());
  EXPECT_EQ(0, action_under_test->delete_objects_response.get_failure_count());
}

TEST_F(S3DeleteMultipleObjectsActionTest, SendErrorResponse) {
  action_under_test->set_s3_error("InternalError");

  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed500, _))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_request, resume(_)).Times(1);

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3DeleteMultipleObjectsActionTest, SendAnyFailedResponse) {
  action_under_test->set_s3_error("NoSuchKey");

  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpFailed404, _))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_request, resume(_)).Times(1);

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3DeleteMultipleObjectsActionTest, SendSuccessResponse) {
  EXPECT_CALL(*mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mock_request, send_response(S3HttpSuccess200, _))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_request, resume(_)).Times(1);

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3DeleteMultipleObjectsActionTest, CleanupOnMetadataFailedToSaveTest1) {
  std::string object_name = "abcd";
  std::string version_key_in_index = "abcd/v1";
  int layout_id = 9;
  struct m0_uint128 object_list_indx_oid = {0x11ffff, 0x1ffff};
  struct m0_uint128 objects_version_list_index_oid = {0x1ff1ff, 0x1ffff};
  struct m0_uint128 oid = {0x1ffff, 0x1ffff};
  std::string oid_str = S3M0Uint128Helper::to_string(oid);

  action_under_test->probable_oid_list[oid_str] =
      std::unique_ptr<S3ProbableDeleteRecord>(new S3ProbableDeleteRecord(
          oid_str, {0ULL, 0ULL}, "abcd", oid, layout_id, object_list_indx_oid,
          objects_version_list_index_oid, version_key_in_index,
          false /* force_delete */));

  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;

  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_keyval(_, _, _, _)).Times(1);

  action_under_test->cleanup();
}

TEST_F(S3DeleteMultipleObjectsActionTest, CleanupOnMetadataFailedToSaveTest2) {
  action_under_test->probable_oid_list.clear();
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;

  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_keyval(_, _, _, _)).Times(0);

  action_under_test->cleanup();
}

TEST_F(S3DeleteMultipleObjectsActionTest, CleanupOnMetadataSavedTest1) {
  m0_uint128 object_oid = {0x1ffff, 0x1ffff};
  action_under_test->oids_to_delete.push_back(object_oid);

  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              delete_objects(_, _, _, _)).Times(AtLeast(1));

  action_under_test->cleanup();
}

TEST_F(S3DeleteMultipleObjectsActionTest, CleanupOnMetadataSavedTest2) {

  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              delete_object(_, _, _)).Times(0);

  action_under_test->cleanup();
}

