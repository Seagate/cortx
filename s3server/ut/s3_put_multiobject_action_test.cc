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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 24-March-2017
 */

#include "mock_s3_clovis_wrapper.h"
#include "s3_put_multiobject_action.h"
#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_clovis_layout.h"
#include "s3_ut_common.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::_;
using ::testing::ReturnRef;
using ::testing::AtLeast;
using ::testing::DefaultValue;
using ::testing::HasSubstr;

//++
// Many of the test cases here taken directly from
// s3_put_chunk_upload_object_action_test.cc
//--

class S3PutMultipartObjectActionTest : public testing::Test {
 protected:
  S3PutMultipartObjectActionTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    mp_indx_oid = {0xffff, 0xffff};
    oid = {0x1ffff, 0x1ffff};
    object_list_indx_oid = {0x11ffff, 0x1ffff};
    upload_id = "upload_id";
    call_count_one = 0;
    bucket_name = "seagatebucket";
    object_name = "objname";

    layout_id =
        S3ClovisLayoutMap::get_instance()->get_best_layout_for_object_size();

    async_buffer_factory =
        std::make_shared<MockS3AsyncBufferOptContainerFactory>(
            S3Option::get_instance()->get_libevent_pool_buffer_size());

    ptr_mock_request = std::make_shared<MockS3RequestObject>(
        req, evhtp_obj_ptr, async_buffer_factory);
    EXPECT_CALL(*ptr_mock_request, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));
    EXPECT_CALL(*ptr_mock_request, get_object_name())
        .WillRepeatedly(ReturnRef(object_name));

    ptr_mock_s3_clovis_api = std::make_shared<MockS3Clovis>();

    EXPECT_CALL(*ptr_mock_s3_clovis_api, m0_h_ufid_next(_))
        .WillRepeatedly(Invoke(dummy_helpers_ufid_next));

    EXPECT_CALL(*ptr_mock_request, get_query_string_value("uploadId"))
        .WillRepeatedly(Return("upload_id"));
    EXPECT_CALL(*ptr_mock_request, get_query_string_value("partNumber"))
        .WillRepeatedly(Return("1"));

    bucket_meta_factory = std::make_shared<MockS3BucketMetadataFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);
    object_mp_meta_factory =
        std::make_shared<MockS3ObjectMultipartMetadataFactory>(
            ptr_mock_request, ptr_mock_s3_clovis_api, upload_id);
    object_mp_meta_factory->set_object_list_index_oid(mp_indx_oid);
    part_meta_factory = std::make_shared<MockS3PartMetadataFactory>(
        ptr_mock_request, oid, upload_id, 0);
    clovis_writer_factory = std::make_shared<MockS3ClovisWriterFactory>(
        ptr_mock_request, oid, ptr_mock_s3_clovis_api);
  }

  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::shared_ptr<MockS3PartMetadataFactory> part_meta_factory;
  std::shared_ptr<MockS3ObjectMultipartMetadataFactory> object_mp_meta_factory;
  std::shared_ptr<MockS3ClovisWriterFactory> clovis_writer_factory;
  std::shared_ptr<MockS3AsyncBufferOptContainerFactory> async_buffer_factory;
  std::shared_ptr<S3PutMultiObjectAction> action_under_test;
  struct m0_uint128 mp_indx_oid;
  struct m0_uint128 object_list_indx_oid;
  struct m0_uint128 oid;
  int layout_id;
  std::string upload_id;
  std::string object_name;
  std::string bucket_name;
  int call_count_one;

 public:
  void func_callback_one() { call_count_one += 1; }
};

class S3PutMultipartObjectActionTestNoMockAuth
    : public S3PutMultipartObjectActionTest {
 protected:
  S3PutMultipartObjectActionTestNoMockAuth()
      : S3PutMultipartObjectActionTest() {
    S3Option::get_instance()->disable_auth();
    EXPECT_CALL(*ptr_mock_s3_clovis_api, m0_h_ufid_next(_))
        .WillRepeatedly(Invoke(dummy_helpers_ufid_next));

    EXPECT_CALL(*ptr_mock_request, is_chunked()).WillRepeatedly(Return(false));
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    action_under_test.reset(
        new S3PutMultiObjectAction(ptr_mock_request, object_mp_meta_factory,
                                   part_meta_factory, clovis_writer_factory));
  }
};

class S3PutMultipartObjectActionTestWithMockAuth
    : public S3PutMultipartObjectActionTest {
 protected:
  S3PutMultipartObjectActionTestWithMockAuth()
      : S3PutMultipartObjectActionTest() {
    S3Option::get_instance()->enable_auth();
    EXPECT_CALL(*ptr_mock_request, is_chunked()).WillRepeatedly(Return(true));
    mock_auth_factory =
        std::make_shared<MockS3AuthClientFactory>(ptr_mock_request);
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    action_under_test.reset(new S3PutMultiObjectAction(
        ptr_mock_request, object_mp_meta_factory, part_meta_factory,
        clovis_writer_factory, mock_auth_factory));
  }
  std::shared_ptr<MockS3AuthClientFactory> mock_auth_factory;
};

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, ConstructorTest) {
  EXPECT_STREQ("upload_id", action_under_test->upload_id.c_str());
  EXPECT_NE(0, action_under_test->number_of_tasks());
  EXPECT_EQ(0, action_under_test->total_data_to_stream);
  EXPECT_FALSE(action_under_test->auth_failed);
  EXPECT_FALSE(action_under_test->write_failed);
  EXPECT_FALSE(action_under_test->clovis_write_in_progress);
  EXPECT_FALSE(action_under_test->clovis_write_completed);
  EXPECT_FALSE(action_under_test->auth_in_progress);
  EXPECT_TRUE(action_under_test->auth_completed);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       ChunkAuthSucessfulShuttingDown) {
  action_under_test->check_shutdown_signal_for_next_task(true);
  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);
  action_under_test->chunk_auth_successful();
  EXPECT_TRUE(action_under_test->auth_completed == true);
  action_under_test->check_shutdown_signal_for_next_task(false);
  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, ChunkAuthSucessfulNext) {
  action_under_test->clovis_write_completed = true;
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutMultipartObjectActionTest::func_callback_one,
                         this);

  action_under_test->chunk_auth_successful();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       ChunkAuthSucessfulWriteFailed) {
  action_under_test->clovis_write_completed = true;
  action_under_test->write_failed = true;
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), get_content_md5())
      .Times(AtLeast(1))
      .WillOnce(Return("abcd1234abcd"));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->chunk_auth_successful();
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, ChunkAuthSucessful) {
  action_under_test->chunk_auth_successful();
  EXPECT_TRUE(action_under_test->auth_completed);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, ChunkAuthSuccessShuttingDown) {
  action_under_test->check_shutdown_signal_for_next_task(true);
  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  action_under_test->chunk_auth_successful();
  EXPECT_TRUE(action_under_test->auth_completed);
  action_under_test->check_shutdown_signal_for_next_task(false);
  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, ChunkAuthFailedShuttingDown) {
  action_under_test->check_shutdown_signal_for_next_task(true);
  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(403, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);
  action_under_test->chunk_auth_failed();
  action_under_test->check_shutdown_signal_for_next_task(false);
  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, ChunkAuthFailedNext) {
  action_under_test->clovis_write_in_progress = true;
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutMultipartObjectActionTest::func_callback_one,
                         this);

  action_under_test->chunk_auth_failed();
  EXPECT_EQ(0, call_count_one);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       ChunkAuthFailedWriteSuccessful) {
  action_under_test->clovis_write_in_progress = false;
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);
  action_under_test->chunk_auth_failed();

  EXPECT_STREQ("SignatureDoesNotMatch",
               action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->auth_completed);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       FetchBucketInfoFailedMissingTest) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::missing));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(404, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);
  action_under_test->fetch_bucket_info_failed();
  EXPECT_STREQ("NoSuchBucket", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       CheckPartNumberFailedInvalidPartTest) {

  action_under_test->part_number = MINIMUM_PART_NUMBER - 1;
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutMultipartObjectActionTest::func_callback_one,
                         this);
  action_under_test->check_part_number();
  EXPECT_STREQ("InvalidPart", action_under_test->get_s3_error_code().c_str());
  EXPECT_EQ(0, call_count_one);

  action_under_test->part_number = MAXIMUM_PART_NUMBER + 1;
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutMultipartObjectActionTest::func_callback_one,
                         this);
  action_under_test->check_part_number();
  EXPECT_STREQ("InvalidPart", action_under_test->get_s3_error_code().c_str());
  EXPECT_EQ(0, call_count_one);

  action_under_test->part_number = MINIMUM_PART_NUMBER;
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutMultipartObjectActionTest::func_callback_one,
                         this);
  action_under_test->check_part_number();
  EXPECT_EQ(1, call_count_one);

  action_under_test->part_number = MAXIMUM_PART_NUMBER;
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutMultipartObjectActionTest::func_callback_one,
                         this);
  action_under_test->check_part_number();
  EXPECT_EQ(2, call_count_one);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       FetchBucketInfoFailedInternalErrorTest) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::failed));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);
  action_under_test->fetch_bucket_info_failed();
  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, FetchMultipartMetadata) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;

  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), load(_, _))
      .Times(1);
  action_under_test->fetch_multipart_metadata();
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       FetchMultiPartMetadataNoSuchUploadFailed) {
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), get_state())
      .WillOnce(Return(S3ObjectMetadataState::missing));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);

  action_under_test->fetch_multipart_failed();

  EXPECT_STREQ("NoSuchUpload", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       FetchMultiPartMetadataInternalErrorFailed) {
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), get_state())
      .WillOnce(Return(S3ObjectMetadataState::failed));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);

  action_under_test->fetch_multipart_failed();

  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, SaveMultipartMetadata) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  action_under_test->part_number = 1;

  size_t unit_size =
      S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(layout_id);
  EXPECT_CALL(*object_mp_meta_factory->mock_object_mp_metadata,
              get_part_one_size()).WillRepeatedly(Return(0));
  EXPECT_CALL(*ptr_mock_request, get_content_length())
      .WillRepeatedly(Return(unit_size));

  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), save(_, _))
      .Times(AtLeast(1));
  action_under_test->save_multipart_metadata();
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, SaveMultipartMetadataError) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  action_under_test->part_number = 1;

  size_t unit_size =
      S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(layout_id);
  EXPECT_CALL(*object_mp_meta_factory->mock_object_mp_metadata,
              get_part_one_size()).WillRepeatedly(Return(unit_size));
  EXPECT_CALL(*ptr_mock_request, get_data_length())
      .WillRepeatedly(Return(unit_size - 2));
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), save(_, _))
      .Times(AtLeast(0));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(403, _)).Times(1);
  action_under_test->save_multipart_metadata();
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       SaveMultipartMetadataFailedServiceUnavailable) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  action_under_test->part_number = 1;

  EXPECT_CALL(*object_mp_meta_factory->mock_object_mp_metadata, get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::failed_to_launch));
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  action_under_test->save_multipart_metadata_failed();
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       SaveMultipartMetadataFailedInternalError) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  action_under_test->part_number = 1;

  EXPECT_CALL(*object_mp_meta_factory->mock_object_mp_metadata, get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::failed));
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(1);
  action_under_test->save_multipart_metadata_failed();
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, SaveMultipartMetadataAssert) {
  action_under_test->bucket_metadata =
      bucket_meta_factory->mock_bucket_metadata;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  action_under_test->part_number = 2;

  size_t unit_size =
      S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(layout_id);
  EXPECT_CALL(*object_mp_meta_factory->mock_object_mp_metadata,
              get_part_one_size()).WillRepeatedly(Return(0));
  EXPECT_CALL(*ptr_mock_request, get_content_length())
      .WillRepeatedly(Return(unit_size));

  EXPECT_CALL(*(object_mp_meta_factory->mock_object_mp_metadata), save(_, _))
      .Times(0);
  ASSERT_DEATH(action_under_test->save_multipart_metadata(), ".*");
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, FetchFirstPartInfo) {
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), load(_, _, 1)).Times(1);
  action_under_test->fetch_firstpart_info();
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       FetchFirstPartInfoServiceUnavailableFailed) {
  action_under_test->part_metadata = part_meta_factory->mock_part_metadata;
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), get_state())
      .WillOnce(Return(S3PartMetadataState::missing));

  action_under_test->fetch_firstpart_info_failed();

  EXPECT_STREQ("ServiceUnavailable",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       FetchFirstPartInfoInternalErrorFailed) {
  action_under_test->part_metadata = part_meta_factory->mock_part_metadata;
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), get_state())
      .WillRepeatedly(Return(S3PartMetadataState::failed));

  action_under_test->fetch_firstpart_info_failed();

  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, ComputePartOffsetPart1) {
  action_under_test->part_metadata = part_meta_factory->mock_part_metadata;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;

  m0_uint128 oid = {0x1ffff, 0x1ffff};
  size_t unit_size =
      S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(layout_id);
  EXPECT_CALL(*part_meta_factory->mock_part_metadata, get_content_length())
      .WillRepeatedly(Return(unit_size - 2));
  EXPECT_CALL(*object_mp_meta_factory->mock_object_mp_metadata, get_layout_id())
      .WillRepeatedly(Return(layout_id));
  EXPECT_CALL(*object_mp_meta_factory->mock_object_mp_metadata, get_oid())
      .WillRepeatedly(Return(oid));
  EXPECT_CALL(*ptr_mock_request, get_content_length())
      .WillRepeatedly(Return(unit_size - 2));
  EXPECT_CALL(*ptr_mock_request, get_data_length())
      .WillRepeatedly(Return(unit_size - 2));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(400, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->compute_part_offset();
  EXPECT_TRUE(action_under_test->clovis_writer != nullptr);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, ComputePartOffset) {
  m0_uint128 oid = {0x1ffff, 0x1ffff};
  action_under_test->part_metadata = part_meta_factory->mock_part_metadata;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  action_under_test->part_number = 2;
  EXPECT_CALL(*(ptr_mock_request), is_chunked()).WillOnce(Return(true));

  size_t unit_size =
      S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(layout_id);
  EXPECT_CALL(*part_meta_factory->mock_part_metadata, get_content_length())
      .WillRepeatedly(Return(unit_size));
  EXPECT_CALL(*object_mp_meta_factory->mock_object_mp_metadata, get_oid())
      .WillRepeatedly(Return(oid));
  EXPECT_CALL(*ptr_mock_request, get_content_length())
      .WillRepeatedly(Return(unit_size));
  EXPECT_CALL(*object_mp_meta_factory->mock_object_mp_metadata, get_layout_id())
      .WillRepeatedly(Return(layout_id));

  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutMultipartObjectActionTest::func_callback_one,
                         this);
  action_under_test->compute_part_offset();
  EXPECT_TRUE(action_under_test->clovis_writer != nullptr);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, ComputePartOffsetNoChunk) {
  m0_uint128 oid = {0x1ffff, 0x1ffff};
  action_under_test->part_metadata = part_meta_factory->mock_part_metadata;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;
  action_under_test->part_number = 2;
  EXPECT_CALL(*(ptr_mock_request), is_chunked()).WillOnce(Return(false));

  size_t unit_size =
      S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(layout_id);
  EXPECT_CALL(*object_mp_meta_factory->mock_object_mp_metadata,
              get_part_one_size()).WillRepeatedly(Return(unit_size));
  EXPECT_CALL(*ptr_mock_request, get_content_length())
      .WillRepeatedly(Return(unit_size));
  EXPECT_CALL(*object_mp_meta_factory->mock_object_mp_metadata, get_layout_id())
      .WillRepeatedly(Return(layout_id));
  EXPECT_CALL(*object_mp_meta_factory->mock_object_mp_metadata, get_oid())
      .WillRepeatedly(Return(oid));

  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutMultipartObjectActionTest::func_callback_one,
                         this);
  action_under_test->compute_part_offset();
  EXPECT_TRUE(action_under_test->clovis_writer != nullptr);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutMultipartObjectActionTestWithMockAuth,
       InitiateDataStreamingForZeroSizeObject) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*ptr_mock_request, get_data_length()).Times(1).WillOnce(
      Return(0));
  EXPECT_CALL(*(mock_auth_factory->mock_auth_client),
              init_chunk_auth_cycle(_, _)).Times(1);

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutMultipartObjectActionTest::func_callback_one,
                         this);

  action_under_test->initiate_data_streaming();
  EXPECT_FALSE(action_under_test->clovis_write_in_progress);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       InitiateDataStreamingExpectingMoreData) {
  EXPECT_CALL(*ptr_mock_request, get_data_length()).Times(1).WillOnce(
      Return(1024));
  EXPECT_CALL(*ptr_mock_request, has_all_body_content()).Times(1).WillOnce(
      Return(false));
  EXPECT_CALL(*ptr_mock_request, listen_for_incoming_data(_, _)).Times(1);

  action_under_test->initiate_data_streaming();

  EXPECT_FALSE(action_under_test->clovis_write_in_progress);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       InitiateDataStreamingWeHaveAllData) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*ptr_mock_request, get_data_length())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(1024));
  EXPECT_CALL(*ptr_mock_request, has_all_body_content()).Times(1).WillOnce(
      Return(true));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);

  action_under_test->initiate_data_streaming();

  EXPECT_TRUE(action_under_test->clovis_write_in_progress);
}

// Write not in progress and we have all the data
TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       ConsumeIncomingShouldWriteIfWeAllData) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(1024));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);
  action_under_test->consume_incoming_content();

  EXPECT_TRUE(action_under_test->clovis_write_in_progress);
}

// Write not in progress, expecting more, we have exact what we can write
TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       ConsumeIncomingShouldWriteIfWeExactData) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(false));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(
           S3Option::get_instance()->get_clovis_write_payload_size(layout_id)));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);

  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);

  action_under_test->consume_incoming_content();

  EXPECT_TRUE(action_under_test->clovis_write_in_progress);
}

// Write not in progress, expecting more, we have more than we can write
TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       ConsumeIncomingShouldWriteIfWeHaveMoreData) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(false));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(
           S3Option::get_instance()->get_clovis_write_payload_size(layout_id) +
           1024));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);

  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);

  action_under_test->consume_incoming_content();

  EXPECT_TRUE(action_under_test->clovis_write_in_progress);
}

// we are expecting more data
TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       ConsumeIncomingShouldPauseWhenWeHaveTooMuch) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(false));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  // S3_READ_AHEAD_MULTIPLE: 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(
           S3Option::get_instance()->get_clovis_write_payload_size(layout_id) *
           S3Option::get_instance()->get_read_ahead_multiple() * 2));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  action_under_test->consume_incoming_content();

  EXPECT_TRUE(action_under_test->clovis_write_in_progress);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       ConsumeIncomingShouldNotWriteWhenWriteInprogress) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->clovis_write_in_progress = true;
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(0);

  action_under_test->consume_incoming_content();
}

TEST_F(S3PutMultipartObjectActionTestWithMockAuth,
       SendChunkDetailsToAuthClientWithSingleChunk) {
  S3ChunkDetail detail;
  detail.add_size(10);
  detail.add_signature(std::string("abcd1234abcd"));
  detail.update_hash("Test data", 9);
  detail.fini_hash();

  EXPECT_CALL(*ptr_mock_request, is_chunk_detail_ready())
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*ptr_mock_request, pop_chunk_detail()).Times(1).WillOnce(
      Return(detail));

  EXPECT_CALL(*(mock_auth_factory->mock_auth_client),
              add_checksum_for_chunk(_, _)).Times(1);

  action_under_test->send_chunk_details_if_any();

  EXPECT_TRUE(action_under_test->auth_in_progress);
}

TEST_F(S3PutMultipartObjectActionTestWithMockAuth,
       SendChunkDetailsToAuthClientWithTwoChunks) {
  S3ChunkDetail detail;
  detail.add_size(10);
  detail.add_signature(std::string("abcd1234abcd"));
  detail.update_hash("Test data", 9);
  detail.fini_hash();

  EXPECT_CALL(*ptr_mock_request, is_chunk_detail_ready())
      .Times(3)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*ptr_mock_request, pop_chunk_detail()).Times(2).WillRepeatedly(
      Return(detail));

  EXPECT_CALL(*(mock_auth_factory->mock_auth_client),
              add_checksum_for_chunk(_, _)).Times(2);

  action_under_test->send_chunk_details_if_any();

  EXPECT_TRUE(action_under_test->auth_in_progress);
}

TEST_F(S3PutMultipartObjectActionTestWithMockAuth,
       SendChunkDetailsToAuthClientWithTwoChunksAndOneZeroChunk) {
  S3ChunkDetail detail, detail_zero;
  detail.add_size(10);
  detail.add_signature(std::string("abcd1234abcd"));
  detail.update_hash("Test data", 9);
  detail.fini_hash();
  detail_zero.add_size(0);
  detail_zero.add_signature(std::string("abcd1234abcd"));
  detail_zero.update_hash("", 0);
  detail_zero.fini_hash();

  EXPECT_CALL(*ptr_mock_request, is_chunk_detail_ready())
      .Times(3)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*ptr_mock_request, pop_chunk_detail())
      .Times(2)
      .WillOnce(Return(detail))
      .WillOnce(Return(detail_zero));

  EXPECT_CALL(*(mock_auth_factory->mock_auth_client),
              add_checksum_for_chunk(_, _)).Times(1);
  EXPECT_CALL(*(mock_auth_factory->mock_auth_client),
              add_last_checksum_for_chunk(_, _)).Times(1);

  action_under_test->send_chunk_details_if_any();

  EXPECT_TRUE(action_under_test->auth_in_progress);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, WriteObject) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);

  action_under_test->write_object(async_buffer_factory->get_mock_buffer());
  EXPECT_TRUE(action_under_test->clovis_write_in_progress);
}

TEST_F(S3PutMultipartObjectActionTestWithMockAuth,
       WriteObjectShouldSendChunkDetailsForAuth) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;

  S3ChunkDetail detail;
  detail.add_size(10);
  detail.add_signature(std::string("abcd1234abcd"));
  detail.update_hash("Test data", 9);
  detail.fini_hash();

  EXPECT_CALL(*ptr_mock_request, is_chunk_detail_ready())
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*ptr_mock_request, pop_chunk_detail()).Times(1).WillOnce(
      Return(detail));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);

  EXPECT_CALL(*(mock_auth_factory->mock_auth_client),
              add_checksum_for_chunk(_, _)).Times(1);

  action_under_test->write_object(async_buffer_factory->get_mock_buffer());

  EXPECT_TRUE(action_under_test->auth_in_progress);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       WriteObjectSuccessfulWhileShuttingDown) {
  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->write_object_successful();

  S3Option::get_instance()->set_is_s3_shutting_down(false);

  EXPECT_FALSE(action_under_test->clovis_write_in_progress);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       WriteObjectSuccessfulWhileShuttingDownAndRollback) {
  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->write_object_successful();

  S3Option::get_instance()->set_is_s3_shutting_down(false);

  EXPECT_FALSE(action_under_test->clovis_write_in_progress);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       WriteObjectSuccessfulShouldWriteStateAllData) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->_set_layout_id(layout_id);

  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(true));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  // S3_READ_AHEAD_MULTIPLE: 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(1024));
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);

  action_under_test->write_object_successful();

  EXPECT_TRUE(action_under_test->clovis_write_in_progress);
}

// We have some data but not all and exact to write
TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       WriteObjectSuccessfulShouldWriteWhenExactWritableSize) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->_set_layout_id(layout_id);

  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(false));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  // S3_READ_AHEAD_MULTIPLE: 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(
           S3Option::get_instance()->get_clovis_write_payload_size(layout_id)));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);

  action_under_test->write_object_successful();

  EXPECT_TRUE(action_under_test->clovis_write_in_progress);
}

// We have some data but not all and but have more to write
TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       WriteObjectSuccessfulDoNextStepWhenAllIsWritten) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->_set_layout_id(layout_id);

  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(0));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(0);

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutMultipartObjectActionTest::func_callback_one,
                         this);

  action_under_test->write_object_successful();

  EXPECT_EQ(1, call_count_one);
  EXPECT_FALSE(action_under_test->clovis_write_in_progress);
}

// We expecting more and not enough to write
TEST_F(S3PutMultipartObjectActionTestNoMockAuth,
       WriteObjectSuccessfulShouldRestartReadingData) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->_set_layout_id(layout_id);

  // mock mark progress
  action_under_test->clovis_write_in_progress = true;

  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(
           S3Option::get_instance()->get_clovis_write_payload_size(layout_id) -
           1024));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(0);

  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->write_object_successful();

  EXPECT_FALSE(action_under_test->clovis_write_in_progress);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, SaveMetadata) {
  action_under_test->part_metadata = part_meta_factory->mock_part_metadata;
  action_under_test->object_multipart_metadata =
      object_mp_meta_factory->mock_object_mp_metadata;

  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;

  EXPECT_CALL(*ptr_mock_request, get_data_length_str()).Times(1).WillOnce(
      Return("1024"));

  EXPECT_CALL(*(part_meta_factory->mock_part_metadata),
              set_content_length(Eq("1024"))).Times(AtLeast(1));
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), get_content_md5())
      .Times(AtLeast(1))
      .WillOnce(Return("abcd1234abcd"));
  EXPECT_CALL(*(part_meta_factory->mock_part_metadata),
              set_md5(Eq("abcd1234abcd"))).Times(AtLeast(1));

  std::map<std::string, std::string> input_headers;
  input_headers["x-amz-meta-item-1"] = "1024";
  input_headers["x-amz-meta-item-2"] = "s3.seagate.com";

  EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));

  EXPECT_CALL(*(part_meta_factory->mock_part_metadata),
              add_user_defined_attribute(Eq("x-amz-meta-item-1"), Eq("1024")))
      .Times(AtLeast(1));
  EXPECT_CALL(
      *(part_meta_factory->mock_part_metadata),
      add_user_defined_attribute(Eq("x-amz-meta-item-2"), Eq("s3.seagate.com")))
      .Times(AtLeast(1));

  EXPECT_CALL(*(part_meta_factory->mock_part_metadata), save(_, _))
      .Times(AtLeast(1));

  EXPECT_CALL(*(part_meta_factory->mock_part_metadata),
              reset_date_time_to_current()).Times(AtLeast(1));

  action_under_test->save_metadata();
}

TEST_F(S3PutMultipartObjectActionTestWithMockAuth,
       WriteObjectSuccessfulShouldSendChunkDetailsForAuth) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->_set_layout_id(layout_id);

  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(true));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  // S3_READ_AHEAD_MULTIPLE: 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(0));

  S3ChunkDetail detail;
  detail.add_size(10);
  detail.add_signature(std::string("abcd1234abcd"));
  detail.update_hash("Test data", 9);
  detail.fini_hash();

  EXPECT_CALL(*ptr_mock_request, is_chunk_detail_ready())
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*ptr_mock_request, pop_chunk_detail()).Times(1).WillOnce(
      Return(detail));

  EXPECT_CALL(*(mock_auth_factory->mock_auth_client),
              add_checksum_for_chunk(_, _)).Times(1);

  action_under_test->write_object_successful();

  EXPECT_TRUE(action_under_test->auth_in_progress);
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, SendErrorResponse) {
  action_under_test->set_s3_error("InternalError");

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, resume(false)).Times(1);

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, SendSuccessResponse) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), get_content_md5())
      .Times(AtLeast(1))
      .WillOnce(Return("abcd1234abcd"));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(200, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3PutMultipartObjectActionTestNoMockAuth, SendFailedResponse) {
  action_under_test->set_s3_error("InternalError");
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->send_response_to_s3_client();
}


