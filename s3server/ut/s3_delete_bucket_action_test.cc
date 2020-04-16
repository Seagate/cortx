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
 * Original author:  Rajesh Nambiar   <rajesh.nambiarr@seagate.com>
 * Original creation date: 07-April-2017
 */

#include <memory>

#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_common.h"
#include "s3_delete_bucket_action.h"
#include "s3_ut_common.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::_;
using ::testing::ReturnRef;
using ::testing::AtLeast;
using ::testing::DefaultValue;

class S3DeleteBucketActionTest : public testing::Test {
 protected:
  S3DeleteBucketActionTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    oid = {0x1ffff, 0x1ffff};
    zero_oid = {0ULL, 0ULL};
    object_list_indx_oid = {0x11ffff, 0x1ffff};
    upload_id = "upload_id";
    call_count_one = 0;
    bucket_name = "seagatebucket";
    object_name = "objname";
    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    EXPECT_CALL(*ptr_mock_request, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));
    EXPECT_CALL(*ptr_mock_request, get_object_name())
        .WillRepeatedly(ReturnRef(object_name));

    ptr_mock_s3_clovis_api = std::make_shared<MockS3Clovis>();

    EXPECT_CALL(*ptr_mock_s3_clovis_api, m0_h_ufid_next(_))
        .WillRepeatedly(Invoke(dummy_helpers_ufid_next));

    // Owned and deleted by shared_ptr in S3PostMultipartObjectAction
    bucket_meta_factory = std::make_shared<MockS3BucketMetadataFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);
    clovis_writer_factory = std::make_shared<MockS3ClovisWriterFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    clovis_kvs_reader_factory = std::make_shared<MockS3ClovisKVSReaderFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    clovis_kvs_writer_factory = std::make_shared<MockS3ClovisKVSWriterFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    object_mp_meta_factory =
        std::make_shared<MockS3ObjectMultipartMetadataFactory>(
            ptr_mock_request, ptr_mock_s3_clovis_api, upload_id);
    object_mp_meta_factory->set_object_list_index_oid(mp_indx_oid);

    object_meta_factory = std::make_shared<MockS3ObjectMetadataFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);
    object_meta_factory->set_object_list_index_oid(object_list_indx_oid);

    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    action_under_test.reset(new S3DeleteBucketAction(
        ptr_mock_request, ptr_mock_s3_clovis_api, bucket_meta_factory,
        object_mp_meta_factory, object_meta_factory, clovis_writer_factory,
        clovis_kvs_writer_factory, clovis_kvs_reader_factory));
  }

  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::shared_ptr<MockS3ObjectMetadataFactory> object_meta_factory;
  std::shared_ptr<MockS3ObjectMultipartMetadataFactory> object_mp_meta_factory;
  std::shared_ptr<MockS3ClovisWriterFactory> clovis_writer_factory;
  std::shared_ptr<MockS3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<MockS3ClovisKVSWriterFactory> clovis_kvs_writer_factory;
  std::shared_ptr<S3DeleteBucketAction> action_under_test;
  struct m0_uint128 mp_indx_oid;
  struct m0_uint128 object_list_indx_oid;
  struct m0_uint128 oid;
  struct m0_uint128 zero_oid;
  std::string upload_id;
  int call_count_one;
  std::string bucket_name, object_name;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3DeleteBucketActionTest, ConstructorTest) {
  EXPECT_STREQ("", action_under_test->last_key.c_str());
  EXPECT_TRUE(action_under_test->is_bucket_empty == false);
  EXPECT_TRUE(action_under_test->delete_successful == false);
  EXPECT_TRUE(action_under_test->multipart_present == false);
  EXPECT_NE(0, action_under_test->number_of_tasks());
}

TEST_F(S3DeleteBucketActionTest, FetchFirstObjectMetadataPresent) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillOnce(Return(S3BucketMetadataState::present));

  // set the OID
  action_under_test->bucket_metadata->set_object_list_index_oid(oid);

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              next_keyval(_, _, _, _, _, _)).Times(1);
  action_under_test->fetch_first_object_metadata();
  EXPECT_TRUE(action_under_test->clovis_kv_reader != nullptr);
}

TEST_F(S3DeleteBucketActionTest, FetchFirstObjectMetadataEmptyBucket) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillOnce(Return(S3BucketMetadataState::present));

  // set the OID
  action_under_test->bucket_metadata->set_object_list_index_oid(zero_oid);

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              next_keyval(_, _, _, _, _, _)).Times(0);
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);
  action_under_test->fetch_first_object_metadata();

  EXPECT_TRUE(action_under_test->is_bucket_empty == true);
  EXPECT_TRUE(action_under_test->clovis_kv_reader != nullptr);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteBucketActionTest, FetchFirstObjectMetadataBucketMissing) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillOnce(Return(S3BucketMetadataState::missing));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);

  action_under_test->fetch_bucket_info_failed();

  EXPECT_STREQ("NoSuchBucket", action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->clovis_kv_reader == nullptr);
}

TEST_F(S3DeleteBucketActionTest, FetchFirstObjectMetadataBucketFailedToLaunch) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillOnce(Return(S3BucketMetadataState::failed_to_launch));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);

  action_under_test->fetch_bucket_info_failed();

  EXPECT_STREQ("ServiceUnavailable",
               action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->clovis_kv_reader == nullptr);
}

TEST_F(S3DeleteBucketActionTest, FetchFirstObjectMetadataBucketfailure) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillOnce(Return(S3BucketMetadataState::failed));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);

  action_under_test->fetch_bucket_info_failed();

  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->clovis_kv_reader == nullptr);
}

TEST_F(S3DeleteBucketActionTest, FetchFirstObjectMetadataSuccess) {
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);

  action_under_test->fetch_first_object_metadata_successful();
  EXPECT_STREQ("BucketNotEmpty",
               action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->is_bucket_empty == false);
}

TEST_F(S3DeleteBucketActionTest,
       FetchFirstObjectMetadataFailedObjectMetaMissing) {
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::missing));
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);

  action_under_test->fetch_first_object_metadata_failed();
  EXPECT_TRUE(action_under_test->is_bucket_empty == true);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteBucketActionTest,
       FetchFirstObjectMetadataFailedObjectRetrievalFailed) {
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::failed));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);

  action_under_test->fetch_first_object_metadata_failed();
  EXPECT_TRUE(action_under_test->is_bucket_empty == false);
  EXPECT_STREQ("BucketNotEmpty",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3DeleteBucketActionTest, FetchMultipartObjectsMultipartPresent) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->bucket_metadata->set_multipart_index_oid(oid);

  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              next_keyval(_, _, _, _, _, _)).Times(1);
  action_under_test->fetch_multipart_objects();
}

TEST_F(S3DeleteBucketActionTest, FetchMultipartObjectsMultipartNotPresent) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->bucket_metadata->set_multipart_index_oid(zero_oid);

  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);

  action_under_test->fetch_multipart_objects();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteBucketActionTest, FetchMultipartObjectSuccess) {
  std::map<std::string, std::pair<int, std::string>> mymap;
  mymap.insert(std::make_pair(
      "file1",
      std::make_pair(0,
                     "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":"
                     "\"file1\",\"mero_oid\":\"AAAAAAAANmU=-AAAAAAAANmU=\","
                     "\"mero_part_oid\":\"AAAAAAAANmU=-AAAAAAAANmU=\"}")));
  mymap.insert(std::make_pair(
      "file2",
      std::make_pair(1,
                     "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":"
                     "\"file2\",\"mero_oid\":\"AAAAAAAANmU=-AAAAAAAANmU=\","
                     "\"mero_part_oid\":\"AAAAAAAANmU=-AAAAAAAANmU=\"}")));
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->bucket_metadata->set_multipart_index_oid(oid);

  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values())
      .Times(1)
      .WillOnce(ReturnRef(mymap));
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);
  object_mp_meta_factory->mock_object_mp_metadata->set_oid(oid);
  action_under_test->fetch_multipart_objects_successful();
  EXPECT_STREQ("file2", action_under_test->last_key.c_str());
  EXPECT_EQ(2, action_under_test->part_oids.size());
  EXPECT_EQ(2, action_under_test->multipart_object_oids.size());
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteBucketActionTest, FetchMultipartObjectSuccessIllegalJson) {
  std::map<std::string, std::pair<int, std::string>> mymap;
  mymap.insert(
      std::make_pair("file1", std::make_pair(0, "Illegal json format")));
  mymap.insert(
      std::make_pair("file2", std::make_pair(1, "Ilegal json format")));
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->bucket_metadata->set_multipart_index_oid(oid);

  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values())
      .Times(1)
      .WillOnce(ReturnRef(mymap));
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);
  action_under_test->fetch_multipart_objects_successful();
  EXPECT_EQ(0, action_under_test->part_oids.size());
  EXPECT_EQ(0, action_under_test->multipart_object_oids.size());
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteBucketActionTest,
       FetchMultipartObjectSuccessMaxFetchCountLessThanMapSize) {
  std::map<std::string, std::pair<int, std::string>> mymap;
  int old_idx_fetch_count =
      S3Option::get_instance()->get_clovis_idx_fetch_count();
  S3Option::get_instance()->set_clovis_idx_fetch_count(1);
  mymap.insert(std::make_pair(
      "file1",
      std::make_pair(0,
                     "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":"
                     "\"file1\",\"mero_oid\":\"AAAAAAAANmU=-AAAAAAAANmU=\","
                     "\"mero_part_oid\":\"AAAAAAAANmU=-AAAAAAAANmU=\"}")));
  mymap.insert(std::make_pair(
      "file2",
      std::make_pair(1,
                     "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":"
                     "\"file2\",\"mero_oid\":\"AAAAAAAANmU=-AAAAAAAANmU=\","
                     "\"mero_part_oid\":\"AAAAAAAANmU=-AAAAAAAANmU=\"}")));
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;

  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values())
      .Times(1)
      .WillOnce(ReturnRef(mymap));
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);
  object_mp_meta_factory->mock_object_mp_metadata->set_oid(oid);
  action_under_test->fetch_multipart_objects_successful();
  EXPECT_STREQ("file1", action_under_test->last_key.c_str());
  EXPECT_EQ(1, action_under_test->part_oids.size());
  EXPECT_EQ(1, action_under_test->multipart_object_oids.size());
  EXPECT_EQ(1, call_count_one);
  S3Option::get_instance()->set_clovis_idx_fetch_count(old_idx_fetch_count);
}

TEST_F(S3DeleteBucketActionTest, FetchMultipartObjectSuccessNoMultipart) {
  std::map<std::string, std::pair<int, std::string>> mymap;
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_key_values())
      .Times(1)
      .WillOnce(ReturnRef(mymap));
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);
  object_mp_meta_factory->mock_object_mp_metadata->set_oid(oid);
  action_under_test->fetch_multipart_objects_successful();
  EXPECT_STREQ("", action_under_test->last_key.c_str());
  EXPECT_EQ(0, action_under_test->part_oids.size());
  EXPECT_EQ(0, action_under_test->multipart_object_oids.size());
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteBucketActionTest,
       DeleteMultipartObjectsMultipartObjectsPresent) {
  action_under_test->multipart_object_oids.push_back(oid);
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              delete_objects(_, _, _, _)).Times(1);
  action_under_test->delete_multipart_objects();
}

TEST_F(S3DeleteBucketActionTest,
       DeleteMultipartObjectsMultipartObjectsNotPresent) {
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);

  action_under_test->delete_multipart_objects();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteBucketActionTest, DeleteMultipartObjectsSuccess) {
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);

  action_under_test->delete_multipart_objects_successful();
  EXPECT_EQ(1, call_count_one);

  action_under_test->multipart_object_oids.push_back(oid);
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              get_op_ret_code_for_delete_op(_))
      .Times(2)
      .WillOnce(Return(0))
      .WillOnce(Return(-ENOENT));

  action_under_test->delete_multipart_objects_successful();
  EXPECT_EQ(2, call_count_one);

  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);
  action_under_test->delete_multipart_objects_successful();
  EXPECT_EQ(3, call_count_one);
}

TEST_F(S3DeleteBucketActionTest, DeleteMultipartObjectsFailed) {
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), get_state())
      .WillOnce(Return(S3ClovisWriterOpState::failed));
  action_under_test->delete_multipart_objects_failed();
  EXPECT_EQ(1, call_count_one);

  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), get_state())
      .WillOnce(Return(S3ClovisWriterOpState::failed_to_launch));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  action_under_test->delete_multipart_objects_failed();
  EXPECT_EQ(1, call_count_one);

  action_under_test->multipart_object_oids.push_back(oid);
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              get_op_ret_code_for_delete_op(_))
      .Times(2)
      .WillOnce(Return(1))
      .WillOnce(Return(-ENOENT));

  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);

  action_under_test->delete_multipart_objects_successful();
  EXPECT_EQ(2, call_count_one);

  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);
  action_under_test->delete_multipart_objects_successful();
  EXPECT_EQ(3, call_count_one);
}

TEST_F(S3DeleteBucketActionTest, RemovePartIndexes) {
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);

  action_under_test->remove_part_indexes();
  EXPECT_EQ(1, call_count_one);

  action_under_test->part_oids.push_back(oid);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_indexes(_, _, _)).Times(1);
  action_under_test->remove_part_indexes();
}

TEST_F(S3DeleteBucketActionTest, RemovePartIndexesSuccess) {
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);

  action_under_test->remove_part_indexes_successful();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteBucketActionTest, RemovePartIndexesFailed) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillOnce(Return(S3ClovisKVSWriterOpState::failed));
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);

  action_under_test->remove_part_indexes_failed();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteBucketActionTest, RemovePartIndexesFailedToLaunch) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillOnce(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  action_under_test->remove_part_indexes_failed();
  EXPECT_STREQ("ServiceUnavailable",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3DeleteBucketActionTest, RemoveMultipartIndexMultipartPresent) {
  action_under_test->multipart_present = true;
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->bucket_metadata->set_multipart_index_oid(oid);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_index(_, _, _)).Times(1);
  action_under_test->remove_multipart_index();
}

TEST_F(S3DeleteBucketActionTest, RemoveMultipartIndexMultipartNotPresent) {
  action_under_test->multipart_present = false;
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);

  action_under_test->remove_multipart_index();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3DeleteBucketActionTest, RemoveMultipartIndexFailed) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->bucket_metadata->set_multipart_index_oid(oid);
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(1);

  action_under_test->remove_multipart_index_failed();
  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3DeleteBucketActionTest, RemoveMultipartIndexFailedToLaunch) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->bucket_metadata->set_multipart_index_oid(oid);

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  action_under_test->remove_multipart_index_failed();
  EXPECT_STREQ("ServiceUnavailable",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3DeleteBucketActionTest, RemoveObjectListIndex) {
  action_under_test->object_list_index_oid = {0ULL, 0ULL};
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3DeleteBucketActionTest::func_callback_one, this);

  action_under_test->remove_object_list_index();
  EXPECT_EQ(1, call_count_one);

  action_under_test->object_list_index_oid = oid;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_index(_, _, _)).Times(1);
  action_under_test->remove_object_list_index();
}

TEST_F(S3DeleteBucketActionTest, RemoveObjectListIndexFailed) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(1);

  action_under_test->remove_object_list_index_failed();
  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3DeleteBucketActionTest, RemoveObjectListIndexFailedToLaunch) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  action_under_test->remove_object_list_index_failed();
  EXPECT_STREQ("ServiceUnavailable",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3DeleteBucketActionTest, DeleteBucket) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), remove(_, _))
      .Times(1);
  action_under_test->delete_bucket();
}

TEST_F(S3DeleteBucketActionTest, DeleteBucketSuccess) {
  EXPECT_CALL(*ptr_mock_request, send_response(204, _)).Times(1);
  action_under_test->delete_bucket_successful();
  EXPECT_TRUE(action_under_test->delete_successful == true);
}

TEST_F(S3DeleteBucketActionTest, DeleteBucketFailedBucketMissing) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillOnce(Return(S3BucketMetadataState::missing));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);

  action_under_test->delete_bucket_failed();
  EXPECT_STREQ("NoSuchBucket", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3DeleteBucketActionTest, DeleteBucketFailedBucketMetaRetrievalFailed) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3BucketMetadataState::failed));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(1);

  action_under_test->delete_bucket_failed();
  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3DeleteBucketActionTest, SendInternalErrorResponse) {
  action_under_test->set_s3_error("InternalError");

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3DeleteBucketActionTest, SendNoSuchBucketErrorResponse) {
  action_under_test->set_s3_error("NoSuchBucket");

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(404, _)).Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3DeleteBucketActionTest, SendBucketNotEmptyErrorResponse) {
  action_under_test->set_s3_error("BucketNotEmpty");

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(409, _)).Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3DeleteBucketActionTest, SendSuccessResponse) {
  action_under_test->delete_successful = true;
  EXPECT_CALL(*ptr_mock_request, send_response(204, _)).Times(1);

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3DeleteBucketActionTest, SendInternalErrorRetry) {
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request,
              set_out_header_value(Eq("Retry-After"), Eq("1"))).Times(1);
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

