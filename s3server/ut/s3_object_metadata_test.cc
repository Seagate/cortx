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
#include "s3_object_metadata.h"
#include "s3_test_utils.h"
#include "s3_ut_common.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::_;
using ::testing::ReturnRef;
using ::testing::AtLeast;
using ::testing::DefaultValue;


#define DUMMY_ACL_STR "<Owner>\n<ID>1</ID>\n</Owner>"

class S3ObjectMetadataTest : public testing::Test {
 protected:
  S3ObjectMetadataTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    call_count_one = 0;
    bucket_name = "seagatebucket";
    object_name = "objectname";

    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    ptr_mock_request->set_account_name("s3account");
    ptr_mock_request->set_account_id("s3acc-id");
    ptr_mock_request->set_user_name("s3user");
    ptr_mock_request->set_user_id("s3user-id");
    EXPECT_CALL(*ptr_mock_request, get_object_name())
        .WillRepeatedly(ReturnRef(object_name));
    EXPECT_CALL(*ptr_mock_request, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));

    ptr_mock_s3_clovis_api = std::make_shared<MockS3Clovis>();
    EXPECT_CALL(*ptr_mock_s3_clovis_api, m0_h_ufid_next(_))
        .WillRepeatedly(Invoke(dummy_helpers_ufid_next));

    clovis_kvs_reader_factory = std::make_shared<MockS3ClovisKVSReaderFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    clovis_kvs_writer_factory = std::make_shared<MockS3ClovisKVSWriterFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    bucket_meta_factory = std::make_shared<MockS3BucketMetadataFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    object_list_index_oid = {0xffff, 0xffff};
    objects_version_list_index_oid = {0xffff, 0xfff0};

    metadata_obj_under_test.reset(new S3ObjectMetadata(
        ptr_mock_request, false, "", clovis_kvs_reader_factory,
        clovis_kvs_writer_factory, ptr_mock_s3_clovis_api));
    metadata_obj_under_test_with_oid.reset(new S3ObjectMetadata(
        ptr_mock_request, false, "", clovis_kvs_reader_factory,
        clovis_kvs_writer_factory, ptr_mock_s3_clovis_api));
    metadata_obj_under_test_with_oid->set_object_list_index_oid(
        object_list_index_oid);
    metadata_obj_under_test_with_oid->set_objects_version_list_index_oid(
        objects_version_list_index_oid);
  }

  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::shared_ptr<MockS3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<MockS3ClovisKVSWriterFactory> clovis_kvs_writer_factory;
  S3CallBack s3objectmetadata_callbackobj;
  std::shared_ptr<S3ObjectMetadata> metadata_obj_under_test;
  std::shared_ptr<S3ObjectMetadata> metadata_obj_under_test_with_oid;
  struct m0_uint128 object_list_index_oid;
  struct m0_uint128 objects_version_list_index_oid;
  int call_count_one;
  std::string bucket_name, object_name;

 public:
  void func_callback_one() { call_count_one += 1; }
};

class S3MultipartObjectMetadataTest : public testing::Test {
 protected:
  S3MultipartObjectMetadataTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    call_count_one = 0;
    bucket_name = "seagatebucket";
    object_name = "objectname";

    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    ptr_mock_request->set_account_name("s3account");
    ptr_mock_request->set_account_id("s3acc-id");
    ptr_mock_request->set_user_name("s3user");
    ptr_mock_request->set_user_id("s3user-id");
    EXPECT_CALL(*ptr_mock_request, get_object_name())
        .WillRepeatedly(ReturnRef(object_name));
    EXPECT_CALL(*ptr_mock_request, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));

    ptr_mock_s3_clovis_api = std::make_shared<MockS3Clovis>();

    clovis_kvs_reader_factory = std::make_shared<MockS3ClovisKVSReaderFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    clovis_kvs_writer_factory = std::make_shared<MockS3ClovisKVSWriterFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    bucket_meta_factory =
        std::make_shared<MockS3BucketMetadataFactory>(ptr_mock_request);

    object_list_index_oid = {0xffff, 0xffff};
    metadata_obj_under_test.reset(new S3ObjectMetadata(
        ptr_mock_request, true, "1234-1234", clovis_kvs_reader_factory,
        clovis_kvs_writer_factory, ptr_mock_s3_clovis_api));

    metadata_obj_under_test_with_oid.reset(new S3ObjectMetadata(
        ptr_mock_request, true, "1234-1234", clovis_kvs_reader_factory,
        clovis_kvs_writer_factory, ptr_mock_s3_clovis_api));
    metadata_obj_under_test_with_oid->set_object_list_index_oid(
        object_list_index_oid);
  }

  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::shared_ptr<MockS3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<MockS3ClovisKVSWriterFactory> clovis_kvs_writer_factory;
  S3CallBack s3objectmetadata_callbackobj;
  std::shared_ptr<S3ObjectMetadata> metadata_obj_under_test;
  std::shared_ptr<S3ObjectMetadata> metadata_obj_under_test_with_oid;
  struct m0_uint128 object_list_index_oid;
  int call_count_one;
  std::string bucket_name, object_name;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3ObjectMetadataTest, ConstructorTest) {
  struct m0_uint128 zero_oid = {0ULL, 0ULL};
  std::string index_name;
  EXPECT_STREQ("s3user", metadata_obj_under_test->user_name.c_str());
  EXPECT_STREQ("s3account", metadata_obj_under_test->account_name.c_str());
  EXPECT_OID_EQ(zero_oid, metadata_obj_under_test->object_list_index_oid);
  EXPECT_OID_EQ(object_list_index_oid,
                metadata_obj_under_test_with_oid->object_list_index_oid);
  EXPECT_OID_EQ(zero_oid, metadata_obj_under_test->old_oid);
  EXPECT_OID_EQ(M0_CLOVIS_ID_APP, metadata_obj_under_test->oid);
  EXPECT_STREQ(
      "s3user",
      metadata_obj_under_test->system_defined_attribute["Owner-User"].c_str());
  EXPECT_STREQ(
      "s3user-id",
      metadata_obj_under_test->system_defined_attribute["Owner-User-id"]
          .c_str());
  EXPECT_STREQ(
      "s3account",
      metadata_obj_under_test->system_defined_attribute["Owner-Account"]
          .c_str());
  EXPECT_STREQ(
      "s3acc-id",
      metadata_obj_under_test->system_defined_attribute["Owner-Account-id"]
          .c_str());
  std::string date = metadata_obj_under_test->system_defined_attribute["Date"];
  EXPECT_STREQ("", date.c_str());
}

TEST_F(S3MultipartObjectMetadataTest, ConstructorTest) {
  struct m0_uint128 zero_oid = {0ULL, 0ULL};
  std::string index_name;
  EXPECT_STREQ("s3user", metadata_obj_under_test->user_name.c_str());
  EXPECT_STREQ("s3account", metadata_obj_under_test->account_name.c_str());
  EXPECT_OID_EQ(zero_oid, metadata_obj_under_test->object_list_index_oid);
  EXPECT_OID_EQ(object_list_index_oid,
                metadata_obj_under_test_with_oid->object_list_index_oid);
  EXPECT_OID_EQ(zero_oid, metadata_obj_under_test->old_oid);
  EXPECT_OID_EQ(M0_CLOVIS_ID_APP, metadata_obj_under_test->oid);
  EXPECT_STREQ(
      "s3user",
      metadata_obj_under_test->system_defined_attribute["Owner-User"].c_str());
  EXPECT_STREQ(
      "s3user-id",
      metadata_obj_under_test->system_defined_attribute["Owner-User-id"]
          .c_str());
  EXPECT_STREQ(
      "s3account",
      metadata_obj_under_test->system_defined_attribute["Owner-Account"]
          .c_str());
  EXPECT_STREQ(
      "s3acc-id",
      metadata_obj_under_test->system_defined_attribute["Owner-Account-id"]
          .c_str());
  std::string date = metadata_obj_under_test->system_defined_attribute["Date"];
  EXPECT_STREQ("", date.c_str());
  index_name = metadata_obj_under_test->index_name;
  EXPECT_STREQ("Multipart", index_name.substr(index_name.length() - 9).c_str());
  EXPECT_STREQ("1234-1234", metadata_obj_under_test->upload_id.c_str());
  EXPECT_STREQ(
      "1234-1234",
      metadata_obj_under_test->system_defined_attribute["Upload-ID"].c_str());
}

TEST_F(S3ObjectMetadataTest, GetSet) {
  struct m0_uint128 myoid = {0xfff, 0xfff};
  metadata_obj_under_test->system_defined_attribute["Owner-User-id"] =
      "s3owner";
  EXPECT_STREQ("s3owner", metadata_obj_under_test->get_owner_id().c_str());
  metadata_obj_under_test->system_defined_attribute["Owner-User"] = "s3user";
  EXPECT_STREQ("s3user", metadata_obj_under_test->get_owner_name().c_str());
  metadata_obj_under_test->system_defined_attribute["Last-Modified"] =
      "2016-10-18T16:01:01.000Z";
  EXPECT_STREQ("2016-10-18T16:01:01.000Z",
               metadata_obj_under_test->get_last_modified().c_str());
  std::string gmt_time = metadata_obj_under_test->get_last_modified_gmt();
  EXPECT_STREQ("GMT", gmt_time.substr(gmt_time.length() - 3).c_str());
  std::string iso_time =
      metadata_obj_under_test->system_defined_attribute["Last-Modified"];
  EXPECT_STREQ("00Z", iso_time.substr(iso_time.length() - 3).c_str());
  EXPECT_STREQ("STANDARD",
               metadata_obj_under_test->get_storage_class().c_str());
  metadata_obj_under_test->set_content_length("100");
  EXPECT_STREQ("100",
               metadata_obj_under_test->get_content_length_str().c_str());
  EXPECT_EQ(100, metadata_obj_under_test->get_content_length());
  metadata_obj_under_test->set_md5("avbxy");
  EXPECT_STREQ("avbxy", metadata_obj_under_test->get_md5().c_str());

  metadata_obj_under_test->set_oid(myoid);
  EXPECT_OID_EQ(myoid, metadata_obj_under_test->oid);
  EXPECT_TRUE(metadata_obj_under_test->mero_oid_str != "");

  metadata_obj_under_test->object_list_index_oid = myoid;
  EXPECT_OID_EQ(myoid, metadata_obj_under_test->get_object_list_index_oid());

  metadata_obj_under_test->set_old_oid(myoid);
  EXPECT_OID_EQ(myoid, metadata_obj_under_test->old_oid);
  EXPECT_TRUE(metadata_obj_under_test->mero_old_oid_str != "");

  metadata_obj_under_test->set_part_index_oid(myoid);
  EXPECT_OID_EQ(myoid, metadata_obj_under_test->part_index_oid);
  EXPECT_TRUE(metadata_obj_under_test->mero_part_oid_str != "");

  metadata_obj_under_test->bucket_name = "seagatebucket";
  EXPECT_STREQ("BUCKET/seagatebucket",
               metadata_obj_under_test->get_object_list_index_name().c_str());

  EXPECT_STREQ("BUCKET/seagatebucket/Multipart",
               metadata_obj_under_test->get_multipart_index_name().c_str());
}

TEST_F(S3MultipartObjectMetadataTest, GetUserIdUplodIdName) {
  EXPECT_STREQ("s3user-id", metadata_obj_under_test->user_id.c_str());
  EXPECT_STREQ("1234-1234", metadata_obj_under_test->upload_id.c_str());
  EXPECT_STREQ("s3user", metadata_obj_under_test->user_name.c_str());
}

TEST_F(S3ObjectMetadataTest, SetAcl) {
  metadata_obj_under_test->system_defined_attribute["Owner-User"] = "s3user";
  char expected_str[] = "<Owner>\n<ID>1</ID>\n</Owner>";
  std::string acl = DUMMY_ACL_STR;
  metadata_obj_under_test->setacl(acl);
  EXPECT_STREQ(expected_str, metadata_obj_under_test->encoded_acl.c_str());
}

TEST_F(S3ObjectMetadataTest, AddSystemAttribute) {
  metadata_obj_under_test->add_system_attribute("LocationConstraint",
                                                "us-east");
  EXPECT_STREQ(
      "us-east",
      metadata_obj_under_test->system_defined_attribute["LocationConstraint"]
          .c_str());
}

TEST_F(S3ObjectMetadataTest, AddUserDefinedAttribute) {
  metadata_obj_under_test->add_user_defined_attribute("Etag", "1234");
  EXPECT_STREQ("1234",
               metadata_obj_under_test->user_defined_attribute["Etag"].c_str());
}

TEST_F(S3ObjectMetadataTest, Load) {
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_keyval(_, "objectname", _, _)).Times(1);
  metadata_obj_under_test->set_object_list_index_oid(object_list_index_oid);

  metadata_obj_under_test->load(
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj));

  EXPECT_TRUE(metadata_obj_under_test->handler_on_success != nullptr);
  EXPECT_TRUE(metadata_obj_under_test->handler_on_failed != nullptr);
}

TEST_F(S3ObjectMetadataTest, LoadSuccessful) {
  metadata_obj_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;

  metadata_obj_under_test->handler_on_success =
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj);

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_value())
      .WillRepeatedly(Return(
          "{\"Bucket-Name\":\"seagate_bucket\",\"Object-Name\":\"3kfile\"}"));
  metadata_obj_under_test->load_successful();
  EXPECT_EQ(metadata_obj_under_test->state, S3ObjectMetadataState::present);
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
}

TEST_F(S3ObjectMetadataTest, LoadSuccessInvalidJson) {
  metadata_obj_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;

  metadata_obj_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);

  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_value())
      .WillRepeatedly(Return(""));
  metadata_obj_under_test->load_successful();
  EXPECT_TRUE(metadata_obj_under_test->json_parsing_error);
  EXPECT_EQ(metadata_obj_under_test->state, S3ObjectMetadataState::failed);
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
}

TEST_F(S3ObjectMetadataTest, LoadObjectInfoFailedJsonParsingFailed) {
  metadata_obj_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  metadata_obj_under_test->json_parsing_error = true;
  metadata_obj_under_test->load_failed();
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3ObjectMetadataState::failed, metadata_obj_under_test->state);
}

TEST_F(S3ObjectMetadataTest, LoadObjectInfoFailedMetadataMissing) {
  metadata_obj_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  metadata_obj_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::missing));
  metadata_obj_under_test->load_failed();
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3ObjectMetadataState::missing, metadata_obj_under_test->state);
}

TEST_F(S3ObjectMetadataTest, LoadObjectInfoFailedMetadataFailed) {
  metadata_obj_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  metadata_obj_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::failed));
  metadata_obj_under_test->load_failed();
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3ObjectMetadataState::failed, metadata_obj_under_test->state);
}

TEST_F(S3ObjectMetadataTest, LoadObjectInfoFailedMetadataFailedToLaunch) {
  metadata_obj_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  metadata_obj_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::failed_to_launch));
  metadata_obj_under_test->load_failed();
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3ObjectMetadataState::failed_to_launch,
            metadata_obj_under_test->state);
}

TEST_F(S3ObjectMetadataTest, SaveMeatdataIndexOIDPresent) {
  metadata_obj_under_test->set_object_list_index_oid(object_list_index_oid);
  metadata_obj_under_test->set_objects_version_list_index_oid(
      objects_version_list_index_oid);

  // Generate version id for tests
  metadata_obj_under_test->regenerate_version_id();

  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              put_keyval(_, _, _, _, _))
      .Times(1);
  metadata_obj_under_test->save(
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj));
}

TEST_F(S3ObjectMetadataTest, SaveMetadataWithoutParam) {
  metadata_obj_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              put_keyval(_, _, _, _, _))
      .Times(1);
  metadata_obj_under_test->set_object_list_index_oid(object_list_index_oid);

  metadata_obj_under_test->save_metadata();
  EXPECT_STREQ(
      "s3user",
      metadata_obj_under_test->system_defined_attribute["Owner-User"].c_str());
  EXPECT_STREQ(
      "s3user-id",
      metadata_obj_under_test->system_defined_attribute["Owner-User-id"]
          .c_str());
  EXPECT_STREQ(
      "s3account",
      metadata_obj_under_test->system_defined_attribute["Owner-Account"]
          .c_str());
  EXPECT_STREQ(
      "s3acc-id",
      metadata_obj_under_test->system_defined_attribute["Owner-Account-id"]
          .c_str());
}

TEST_F(S3ObjectMetadataTest, SaveMetadataWithParam) {
  metadata_obj_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              put_keyval(_, _, _, _, _))
      .Times(1);
  metadata_obj_under_test->set_object_list_index_oid(object_list_index_oid);

  metadata_obj_under_test->save_metadata(
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj));
  EXPECT_STREQ(
      "s3user",
      metadata_obj_under_test->system_defined_attribute["Owner-User"].c_str());
  EXPECT_STREQ(
      "s3user-id",
      metadata_obj_under_test->system_defined_attribute["Owner-User-id"]
          .c_str());
  EXPECT_STREQ(
      "s3account",
      metadata_obj_under_test->system_defined_attribute["Owner-Account"]
          .c_str());
  EXPECT_STREQ(
      "s3acc-id",
      metadata_obj_under_test->system_defined_attribute["Owner-Account-id"]
          .c_str());
  EXPECT_TRUE(metadata_obj_under_test->handler_on_success != nullptr);
  EXPECT_TRUE(metadata_obj_under_test->handler_on_failed != nullptr);
}

TEST_F(S3ObjectMetadataTest, SaveMetadataSuccess) {
  metadata_obj_under_test->handler_on_success =
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj);
  metadata_obj_under_test->save_metadata_successful();
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
  EXPECT_EQ(S3ObjectMetadataState::saved, metadata_obj_under_test->state);
}

TEST_F(S3ObjectMetadataTest, SaveMetadataFailed) {
  metadata_obj_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_obj_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  metadata_obj_under_test->save_metadata_failed();
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3ObjectMetadataState::failed, metadata_obj_under_test->state);
}

TEST_F(S3ObjectMetadataTest, SaveMetadataFailedToLaunch) {
  metadata_obj_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_obj_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  metadata_obj_under_test->save_metadata_failed();
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3ObjectMetadataState::failed_to_launch,
            metadata_obj_under_test->state);
}

TEST_F(S3ObjectMetadataTest, Remove) {
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_keyval(_, _, _, _))
      .Times(1);
  metadata_obj_under_test->set_object_list_index_oid(object_list_index_oid);

  metadata_obj_under_test->remove(
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj));
  EXPECT_TRUE(metadata_obj_under_test->handler_on_success != NULL);
  EXPECT_TRUE(metadata_obj_under_test->handler_on_failed != NULL);
}

TEST_F(S3ObjectMetadataTest, RemoveObjectMetadataSuccessful) {
  // Expect to call Version remove/delete
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_keyval(_, _, _, _)).Times(1);
  metadata_obj_under_test->set_objects_version_list_index_oid(
      objects_version_list_index_oid);
  // Generate version id for tests
  metadata_obj_under_test->regenerate_version_id();

  metadata_obj_under_test->remove_object_metadata_successful();
}

TEST_F(S3ObjectMetadataTest, RemoveVersionMetadataSuccessful) {
  metadata_obj_under_test->handler_on_success =
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj);

  metadata_obj_under_test->remove_version_metadata_successful();
  EXPECT_EQ(S3ObjectMetadataState::deleted, metadata_obj_under_test->state);
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
}

TEST_F(S3ObjectMetadataTest, RemoveObjectMetadataFailed) {
  metadata_obj_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_obj_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  metadata_obj_under_test->remove_object_metadata_failed();
  EXPECT_EQ(S3ObjectMetadataState::failed, metadata_obj_under_test->state);
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
}

TEST_F(S3ObjectMetadataTest, RemoveObjectMetadataFailedToLaunch) {
  metadata_obj_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_obj_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3objectmetadata_callbackobj);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  metadata_obj_under_test->remove_object_metadata_failed();
  EXPECT_EQ(S3ObjectMetadataState::failed_to_launch,
            metadata_obj_under_test->state);
  EXPECT_TRUE(s3objectmetadata_callbackobj.fail_called);
}

TEST_F(S3ObjectMetadataTest, RemoveVersionMetadataFailed) {
  metadata_obj_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  metadata_obj_under_test->handler_on_success =
      std::bind(&S3CallBack::on_success, &s3objectmetadata_callbackobj);
  metadata_obj_under_test->remove_version_metadata_failed();
  EXPECT_EQ(S3ObjectMetadataState::deleted, metadata_obj_under_test->state);
  EXPECT_TRUE(s3objectmetadata_callbackobj.success_called);
}

TEST_F(S3ObjectMetadataTest, ToJson) {
  std::string json_str = metadata_obj_under_test->to_json();
  Json::Value newroot;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(json_str, newroot);
  EXPECT_TRUE(parsingSuccessful);
}

TEST_F(S3ObjectMetadataTest, FromJson) {
  int ret_status;
  std::string json_str =
      "{\"ACL\":\"PD94+Cg==\",\"Bucket-Name\":\"seagatebucket\"}";

  ret_status = metadata_obj_under_test->from_json(json_str);
  EXPECT_TRUE(ret_status == 0);
}

TEST_F(S3MultipartObjectMetadataTest, FromJson) {
  int ret_status;
  std::string json_str =
      "{\"ACL\":\"PD94+Cg==\",\"Bucket-Name\":\"seagatebucket\",\"mero_old_"
      "oid\":\"123-456\"}";

  ret_status = metadata_obj_under_test->from_json(json_str);
  EXPECT_TRUE(ret_status == 0);
  EXPECT_STREQ("seagatebucket", metadata_obj_under_test->bucket_name.c_str());
  EXPECT_STREQ("123-456", metadata_obj_under_test->mero_old_oid_str.c_str());
}

TEST_F(S3ObjectMetadataTest, GetEncodedBucketAcl) {
  std::string json_str = "{\"ACL\":\"PD94bg==\"}";

  metadata_obj_under_test->from_json(json_str);
  EXPECT_STREQ("PD94bg==",
               metadata_obj_under_test->get_encoded_object_acl().c_str());
}

