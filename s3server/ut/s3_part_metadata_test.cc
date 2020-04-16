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
 * Original creation date: 10-May-2017
 */

#include <json/json.h>
#include <memory>

#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_callback_test_helpers.h"
#include "s3_common.h"
#include "s3_part_metadata.h"
#include "s3_test_utils.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::_;
using ::testing::ReturnRef;
using ::testing::AtLeast;
using ::testing::DefaultValue;

class S3PartMetadataTest : public testing::Test {
 protected:
  S3PartMetadataTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    bucket_name = "seagatebucket";
    object_name = "objname";
    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    EXPECT_CALL(*ptr_mock_request, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));
    EXPECT_CALL(*ptr_mock_request, get_object_name())
        .WillRepeatedly(ReturnRef(object_name));
    ptr_mock_s3_clovis_api = std::make_shared<MockS3Clovis>();

    clovis_kvs_reader_factory = std::make_shared<MockS3ClovisKVSReaderFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    clovis_kvs_writer_factory = std::make_shared<MockS3ClovisKVSWriterFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    part_indx_oid = {0xffff, 0xffff};

    metadata_under_test.reset(new S3PartMetadata(ptr_mock_request, "uploadid",
                                                 1, clovis_kvs_reader_factory,
                                                 clovis_kvs_writer_factory));
    metadata_under_test_with_oid.reset(new S3PartMetadata(
        ptr_mock_request, part_indx_oid, "uploadid", 1,
        clovis_kvs_reader_factory, clovis_kvs_writer_factory));
  }

  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api;
  std::shared_ptr<MockS3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<MockS3ClovisKVSWriterFactory> clovis_kvs_writer_factory;
  S3CallBack s3objectmetadata_callbackobj;
  std::shared_ptr<S3PartMetadata> metadata_under_test;
  std::shared_ptr<S3PartMetadata> metadata_under_test_with_oid;
  struct m0_uint128 part_indx_oid;
  std::string bucket_name, object_name;
};

TEST_F(S3PartMetadataTest, ConstructorTest) {
  struct m0_uint128 zero_oid = {0ULL, 0ULL};
  std::string index_name;
  EXPECT_OID_EQ(zero_oid, metadata_under_test->part_index_name_oid);
  EXPECT_OID_EQ(part_indx_oid,
                metadata_under_test_with_oid->part_index_name_oid);
  EXPECT_STREQ(
      "", metadata_under_test->system_defined_attribute["Owner-User"].c_str());
  EXPECT_STREQ(
      "",
      metadata_under_test->system_defined_attribute["Owner-User-id"].c_str());
  EXPECT_STREQ(
      "",
      metadata_under_test->system_defined_attribute["Owner-Account"].c_str());
  EXPECT_STREQ("",
               metadata_under_test->system_defined_attribute["Owner-Account-id"]
                   .c_str());
  std::string date = metadata_under_test->system_defined_attribute["Date"];
  EXPECT_STREQ("", date.c_str());
  EXPECT_STREQ("uploadid", metadata_under_test->upload_id.c_str());
  EXPECT_TRUE(metadata_under_test->put_metadata);
  EXPECT_STREQ("index_salt_", metadata_under_test->salt.c_str());
  EXPECT_EQ(0, metadata_under_test->collision_attempt_count);
}

TEST_F(S3PartMetadataTest, GetSet) {
  metadata_under_test->system_defined_attribute["Last-Modified"] =
      "2016-10-18T16:01:01.000Z";
  EXPECT_STREQ("2016-10-18T16:01:01.000Z",
               metadata_under_test->get_last_modified().c_str());
  std::string gmt_time = metadata_under_test->get_last_modified_gmt();
  EXPECT_STREQ("GMT", gmt_time.substr(gmt_time.length() - 3).c_str());
  std::string iso_time =
      metadata_under_test->system_defined_attribute["Last-Modified"];
  EXPECT_STREQ("00Z", iso_time.substr(iso_time.length() - 3).c_str());
  EXPECT_STREQ("STANDARD", metadata_under_test->get_storage_class().c_str());
  metadata_under_test->set_content_length("100");
  EXPECT_STREQ("100", metadata_under_test->get_content_length_str().c_str());
  EXPECT_EQ(100, metadata_under_test->get_content_length());
  metadata_under_test->set_md5("avbxy");
  EXPECT_STREQ("avbxy", metadata_under_test->get_md5().c_str());

  metadata_under_test->part_number = "5";
  EXPECT_STREQ("5", metadata_under_test->get_part_number().c_str());

  metadata_under_test->bucket_name = "seagatebucket";
  metadata_under_test->object_name = "18MBfile";
  EXPECT_STREQ("BUCKET/seagatebucket/18MBfile/uploadid",
               metadata_under_test->get_part_index_name().c_str());
}

TEST_F(S3PartMetadataTest, AddSystemAttribute) {
  metadata_under_test->add_system_attribute("LocationConstraint", "us-east");
  EXPECT_STREQ("us-east", metadata_under_test
                              ->system_defined_attribute["LocationConstraint"]
                              .c_str());
}

TEST_F(S3PartMetadataTest, AddUserDefinedAttribute) {
  metadata_under_test->add_user_defined_attribute("Etag", "1234");
  EXPECT_STREQ("1234",
               metadata_under_test->user_defined_attribute["Etag"].c_str());
}

TEST_F(S3PartMetadataTest, Load) {
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_keyval(_, "1", _, _))
      .Times(1);
  metadata_under_test->load(
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj), 1);

  EXPECT_TRUE(metadata_under_test->handler_on_success != nullptr);
  EXPECT_TRUE(metadata_under_test->handler_on_failed != nullptr);
}

TEST_F(S3PartMetadataTest, LoadSuccessful) {
  metadata_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;

  metadata_under_test->handler_on_success =
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj);

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_value())
      .WillRepeatedly(Return(
          "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"3kfile\"}"));
  metadata_under_test->load_successful();
  EXPECT_EQ(metadata_under_test->state, S3PartMetadataState::present);
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
}

TEST_F(S3PartMetadataTest, LoadSuccessInvalidJson) {
  metadata_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;

  metadata_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_value())
      .WillRepeatedly(Return(""));
  metadata_under_test->load_successful();
  EXPECT_TRUE(metadata_under_test->json_parsing_error);
  EXPECT_EQ(metadata_under_test->state, S3PartMetadataState::failed);
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
}

TEST_F(S3PartMetadataTest, LoadPartInfoFailedJsonParsingFailed) {
  metadata_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  metadata_under_test->json_parsing_error = true;
  metadata_under_test->load_failed();
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3PartMetadataState::failed, metadata_under_test->state);
}

TEST_F(S3PartMetadataTest, LoadPartInfoFailedMetadataMissing) {
  metadata_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  metadata_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::missing));
  metadata_under_test->load_failed();
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3PartMetadataState::missing, metadata_under_test->state);
}

TEST_F(S3PartMetadataTest, LoadPartInfoFailedMetadataFailed) {
  metadata_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  metadata_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::failed));
  metadata_under_test->load_failed();
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3PartMetadataState::failed, metadata_under_test->state);
}

TEST_F(S3PartMetadataTest, CreateIndex) {
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              create_index(_, _, _))
      .Times(1);
  metadata_under_test->create_index(
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj));
  EXPECT_EQ(S3PartMetadataState::missing, metadata_under_test->state);
  EXPECT_TRUE(metadata_under_test->handler_on_success != NULL);
  EXPECT_TRUE(metadata_under_test->handler_on_failed != NULL);
  EXPECT_FALSE(metadata_under_test->put_metadata);
}

TEST_F(S3PartMetadataTest, CreatePartIndex) {
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              create_index(_, _, _))
      .Times(1);
  metadata_under_test->create_part_index();
  EXPECT_EQ(S3PartMetadataState::missing, metadata_under_test->state);
}

TEST_F(S3PartMetadataTest, CreatePartIndexSuccessful) {
  metadata_under_test->put_metadata = true;
  struct m0_uint128 myoid = {0xfff, 0xffff};
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_under_test->clovis_kv_writer->oid_list.push_back(myoid);

  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              put_keyval(_, _, _, _, _))
      .Times(1);
  metadata_under_test->create_part_index_successful();
  EXPECT_OID_EQ(myoid, metadata_under_test->part_index_name_oid);
}

TEST_F(S3PartMetadataTest, CreatePartIndexSuccessfulSaveMetadata) {
  metadata_under_test->put_metadata = true;
  struct m0_uint128 myoid = {0xfff, 0xffff};
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_under_test->clovis_kv_writer->oid_list.push_back(myoid);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              put_keyval(_, _, _, _, _))
      .Times(1);
  metadata_under_test->create_part_index_successful();
  EXPECT_OID_EQ(myoid, metadata_under_test->part_index_name_oid);
}

TEST_F(S3PartMetadataTest, CreatePartIndexSuccessfulOnlyCreateIndex) {
  struct m0_uint128 myoid = {0xfff, 0xffff};
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_under_test->put_metadata = false;
  metadata_under_test->clovis_kv_writer->oid_list.push_back(myoid);
  metadata_under_test->handler_on_success =
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj);

  metadata_under_test->create_part_index_successful();
  EXPECT_EQ(S3PartMetadataState::store_created, metadata_under_test->state);
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
}

TEST_F(S3PartMetadataTest, CreatePartIndexFailedCollisionHappened) {
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_under_test->collision_attempt_count = 1;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::exists));

  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              create_index(_, _, _))
      .Times(1);

  metadata_under_test->create_part_index_failed();
  EXPECT_EQ(2, metadata_under_test->collision_attempt_count);
}

TEST_F(S3PartMetadataTest, CreateBucketListIndexFailed) {
  metadata_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(2)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  metadata_under_test->create_part_index_failed();
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
  EXPECT_EQ(metadata_under_test->state, S3PartMetadataState::failed);
}

TEST_F(S3PartMetadataTest, CreateBucketListIndexFailedToLaunch) {
  metadata_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(2)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  metadata_under_test->create_part_index_failed();
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
  EXPECT_EQ(metadata_under_test->state, S3PartMetadataState::failed_to_launch);
}

TEST_F(S3PartMetadataTest, CollisionDetected) {
  metadata_under_test->index_name = "/BUCKET/seagatebucket/w121";
  metadata_under_test->collision_attempt_count = 1;
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              create_index(_, _, _))
      .Times(1);

  metadata_under_test->handle_collision();
  EXPECT_STRNE("/BUCKET/seagatebucket/w121",
               metadata_under_test->index_name.c_str());
  EXPECT_EQ(2, metadata_under_test->collision_attempt_count);
}

TEST_F(S3PartMetadataTest, CollisionDetectedMaxAttemptExceeded) {
  metadata_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  metadata_under_test->collision_attempt_count = MAX_COLLISION_RETRY_COUNT + 1;
  metadata_under_test->handle_collision();
  EXPECT_EQ(S3PartMetadataState::failed, metadata_under_test->state);
  EXPECT_EQ(MAX_COLLISION_RETRY_COUNT + 1,
            metadata_under_test->collision_attempt_count);
}

TEST_F(S3PartMetadataTest, SaveMetadata) {
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              put_keyval(_, _, _, _, _))
      .Times(1);

  metadata_under_test->save_metadata();
  EXPECT_STREQ(
      "uploadid",
      metadata_under_test->system_defined_attribute["Upload-ID"].c_str());
}

TEST_F(S3PartMetadataTest, SaveMetadataSuccessful) {
  metadata_under_test->handler_on_success =
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj);
  metadata_under_test->save_metadata_successful();
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
  EXPECT_EQ(S3PartMetadataState::saved, metadata_under_test->state);
}

TEST_F(S3PartMetadataTest, SaveMetadataFailed) {
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  metadata_under_test->save_metadata_failed();
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3PartMetadataState::failed, metadata_under_test->state);
}

TEST_F(S3PartMetadataTest, SaveMetadataFailedToLaunch) {
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  metadata_under_test->save_metadata_failed();
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3PartMetadataState::failed_to_launch, metadata_under_test->state);
}

TEST_F(S3PartMetadataTest, Remove) {
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_keyval(_, _, _, _))
      .Times(1);

  metadata_under_test->remove(
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj), 0);
  EXPECT_TRUE(metadata_under_test->handler_on_success != NULL);
  EXPECT_TRUE(metadata_under_test->handler_on_failed != NULL);
}

TEST_F(S3PartMetadataTest, RemoveSuccessful) {
  metadata_under_test->handler_on_success =
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj);

  metadata_under_test->remove_successful();
  EXPECT_EQ(S3PartMetadataState::deleted, metadata_under_test->state);
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
}

TEST_F(S3PartMetadataTest, RemoveFailed) {
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  metadata_under_test->remove_failed();
  EXPECT_EQ(S3PartMetadataState::failed, metadata_under_test->state);
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
}

TEST_F(S3PartMetadataTest, RemoveFailedToLaunch) {
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  metadata_under_test->remove_failed();
  EXPECT_EQ(S3PartMetadataState::failed_to_launch, metadata_under_test->state);
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
}

TEST_F(S3PartMetadataTest, RemoveIndex) {
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;

  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_index(_, _, _))
      .Times(1);
  metadata_under_test->remove_index(
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj));
}

TEST_F(S3PartMetadataTest, RemoveIndexSucessful) {
  metadata_under_test->handler_on_success =
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj);

  metadata_under_test->remove_index_successful();
  EXPECT_EQ(S3PartMetadataState::index_deleted, metadata_under_test->state);
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
}

TEST_F(S3PartMetadataTest, RemoveIndexFailed) {
  metadata_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));

  metadata_under_test->remove_index_failed();
  EXPECT_EQ(S3PartMetadataState::failed, metadata_under_test->state);
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
}

TEST_F(S3PartMetadataTest, ToJson) {
  std::string json_str = metadata_under_test->to_json();
  Json::Value newroot;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(json_str, newroot);
  EXPECT_TRUE(parsingSuccessful);
}

TEST_F(S3PartMetadataTest, FromJsonSuccess) {
  std::string json_str =
      "{\"ACL\":\"PD94+Cg==\",\"Bucket-Name\":\"seagatebucket\"}";
  EXPECT_EQ(0, metadata_under_test->from_json(json_str));
}

TEST_F(S3PartMetadataTest, FromJsonFailure) {
  std::string json_str = "This is invalid Json String";
  EXPECT_EQ(-1, metadata_under_test->from_json(json_str));
}

