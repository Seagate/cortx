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
 * Original creation date: 01-Feb-2019
 */

#include <json/json.h>
#include <memory>

#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_bucket_metadata_v1.h"
#include "s3_callback_test_helpers.h"
#include "s3_common.h"
#include "s3_test_utils.h"
#include "s3_ut_common.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::_;
using ::testing::ReturnRef;
using ::testing::AtLeast;
using ::testing::DefaultValue;

#define DEFAULT_ACL_STR                                                 \
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<AccessControlPolicy "   \
  "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n  <Owner>\n    " \
  "<ID></ID>\n      <DisplayName></DisplayName>\n  </Owner>\n  "        \
  "<AccessControlList>\n    <Grant>\n      <Grantee "                   \
  "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "            \
  "xsi:type=\"CanonicalUser\">\n        <ID></ID>\n        "            \
  "<DisplayName></DisplayName>\n      </Grantee>\n      "               \
  "<Permission>FULL_CONTROL</Permission>\n    </Grant>\n  "             \
  "</AccessControlList>\n</AccessControlPolicy>\n"

#define DUMMY_POLICY_STR                                                \
  "{\n \"Id\": \"Policy1462526893193\",\n \"Statement\": [\n \"Sid\": " \
  "\"Stmt1462526862401\", \n }"

#define DUMMY_ACL_STR "<Owner>\n<ID></ID>\n</Owner>"

class S3BucketMetadataV1Test : public testing::Test {
 protected:
  S3BucketMetadataV1Test() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    call_count_one = 0;
    bucket_name = "seagatebucket";
    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    EXPECT_CALL(*ptr_mock_request, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));

    ptr_mock_s3_clovis_api = std::make_shared<MockS3Clovis>();
    EXPECT_CALL(*ptr_mock_s3_clovis_api, m0_h_ufid_next(_))
        .WillRepeatedly(Invoke(dummy_helpers_ufid_next));

    clovis_kvs_reader_factory = std::make_shared<MockS3ClovisKVSReaderFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    clovis_kvs_writer_factory = std::make_shared<MockS3ClovisKVSWriterFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    s3_global_bucket_index_metadata_factory =
        std::make_shared<MockS3GlobalBucketIndexMetadataFactory>(
            ptr_mock_request);

    ptr_mock_request->set_account_name("s3account");
    ptr_mock_request->set_account_id("s3accountid");
    ptr_mock_request->set_user_name("s3user");
    ptr_mock_request->set_user_id("s3userid");

    action_under_test.reset(new S3BucketMetadataV1(
        ptr_mock_request, ptr_mock_s3_clovis_api, clovis_kvs_reader_factory,
        clovis_kvs_writer_factory, s3_global_bucket_index_metadata_factory));
  }

  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api;
  std::shared_ptr<MockS3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<MockS3ClovisKVSWriterFactory> clovis_kvs_writer_factory;
  std::shared_ptr<MockS3GlobalBucketIndexMetadataFactory>
      s3_global_bucket_index_metadata_factory;
  S3CallBack s3bucketmetadata_callbackobj;
  std::shared_ptr<S3BucketMetadataV1> action_under_test;
  int call_count_one;
  std::string bucket_name;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3BucketMetadataV1Test, ConstructorTest) {
  struct m0_uint128 zero_oid = {0ULL, 0ULL};
  EXPECT_STREQ("s3user", action_under_test->user_name.c_str());
  EXPECT_STREQ("s3account", action_under_test->account_name.c_str());
  EXPECT_STREQ("", action_under_test->bucket_policy.c_str());
  EXPECT_STREQ("index_salt_", action_under_test->collision_salt.c_str());
  EXPECT_EQ(0, action_under_test->collision_attempt_count);
  EXPECT_OID_EQ(zero_oid,
                action_under_test->get_bucket_metadata_list_index_oid());
  EXPECT_OID_EQ(zero_oid, action_under_test->object_list_index_oid);
  EXPECT_OID_EQ(zero_oid, action_under_test->multipart_index_oid);
  EXPECT_STREQ(
      "", action_under_test->system_defined_attribute["Owner-User"].c_str());
  EXPECT_STREQ(
      "", action_under_test->system_defined_attribute["Owner-User-id"].c_str());
  EXPECT_STREQ(
      "", action_under_test->system_defined_attribute["Owner-Account"].c_str());
  EXPECT_STREQ(
      "",
      action_under_test->system_defined_attribute["Owner-Account-id"].c_str());
  std::string date = action_under_test->system_defined_attribute["Date"];
  EXPECT_STREQ("Z", date.substr(date.length() - 1).c_str());
  EXPECT_STREQ("us-west-2",
               action_under_test->system_defined_attribute["LocationConstraint"]
                   .c_str());
}

TEST_F(S3BucketMetadataV1Test, GetSystemAttributesTest) {
  EXPECT_STRNE("", action_under_test->get_creation_time().c_str());
  action_under_test->system_defined_attribute["Owner-User-id"] = "s3owner";
  EXPECT_STREQ("s3owner", action_under_test->get_owner_id().c_str());
  EXPECT_STREQ("us-west-2",
               action_under_test->get_location_constraint().c_str());
  action_under_test->system_defined_attribute["Owner-User"] = "s3user";
  EXPECT_STREQ("s3user", action_under_test->get_owner_name().c_str());
}

TEST_F(S3BucketMetadataV1Test, GetSetOIDsPolicyAndLocation) {
  struct m0_uint128 oid = {0x1ffff, 0x1fff};
  action_under_test->multipart_index_oid = {0x1fff, 0xff1};
  action_under_test->object_list_index_oid = {0xff, 0xff};

  EXPECT_OID_EQ(action_under_test->get_multipart_index_oid(),
                action_under_test->multipart_index_oid);
  EXPECT_OID_EQ(action_under_test->get_object_list_index_oid(),
                action_under_test->object_list_index_oid);

  action_under_test->set_multipart_index_oid(oid);
  EXPECT_OID_EQ(action_under_test->multipart_index_oid, oid);

  action_under_test->set_object_list_index_oid(oid);
  EXPECT_OID_EQ(action_under_test->object_list_index_oid, oid);

  action_under_test->set_location_constraint("us-east");
  EXPECT_STREQ("us-east",
               action_under_test->system_defined_attribute["LocationConstraint"]
                   .c_str());
}

TEST_F(S3BucketMetadataV1Test, SetBucketPolicy) {
  std::string policy = DUMMY_POLICY_STR;
  action_under_test->setpolicy(policy);
  EXPECT_STREQ(DUMMY_POLICY_STR, action_under_test->bucket_policy.c_str());
}

TEST_F(S3BucketMetadataV1Test, DeletePolicy) {
  action_under_test->deletepolicy();
  EXPECT_STREQ("", action_under_test->bucket_policy.c_str());
}

TEST_F(S3BucketMetadataV1Test, SetAcl) {
  char expected_str[] = "<Owner>\n<ID></ID>\n</Owner>";
  std::string acl = DUMMY_ACL_STR;
  action_under_test->setacl(acl);
  EXPECT_STREQ(expected_str, action_under_test->encoded_acl.c_str());
}

TEST_F(S3BucketMetadataV1Test, GetTagsAsXml) {
  char expected_str[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Tagging "
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/"
      "\"><TagSet><Tag><Key>organization1234</Key><Value>marketing1234</"
      "Value></Tag><Tag><Key>organization124</Key><Value>marketing123</Value></"
      "Tag></TagSet></Tagging>";
  std::map<std::string, std::string> bucket_tags_map;
  bucket_tags_map["organization124"] = "marketing123";
  bucket_tags_map["organization1234"] = "marketing1234";

  action_under_test->set_tags(bucket_tags_map);

  EXPECT_STREQ(expected_str, action_under_test->get_tags_as_xml().c_str());
}

TEST_F(S3BucketMetadataV1Test, GetSpecialCharTagsAsXml) {
  char expected_str[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Tagging "
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/"
      "\"><TagSet><Tag><Key>organiza$tion</Key><Value>marketin*g</Value></"
      "Tag><Tag><Key>organizati#on124</Key><Value>marke@ting123</Value></Tag></"
      "TagSet></Tagging>";
  std::map<std::string, std::string> bucket_tags_map;
  bucket_tags_map["organiza$tion"] = "marketin*g";
  bucket_tags_map["organizati#on124"] = "marke@ting123";

  action_under_test->set_tags(bucket_tags_map);

  EXPECT_STREQ(expected_str, action_under_test->get_tags_as_xml().c_str());
}

TEST_F(S3BucketMetadataV1Test, AddSystemAttribute) {
  action_under_test->add_system_attribute("LocationConstraint", "us-east");
  EXPECT_STREQ("us-east",
               action_under_test->system_defined_attribute["LocationConstraint"]
                   .c_str());
}

TEST_F(S3BucketMetadataV1Test, AddUserDefinedAttribute) {
  action_under_test->add_user_defined_attribute("Etag", "1234");
  EXPECT_STREQ("1234",
               action_under_test->user_defined_attribute["Etag"].c_str());
}

TEST_F(S3BucketMetadataV1Test, Load) {
  EXPECT_CALL(*(s3_global_bucket_index_metadata_factory
                    ->mock_global_bucket_index_metadata),
              load(_, _)).Times(1);
  action_under_test->load(
      std::bind(&S3CallBack::on_success, &s3bucketmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj));
  EXPECT_EQ(action_under_test->S3BucketMetadataCurrentOp::fetching,
            action_under_test->current_op);
}

TEST_F(S3BucketMetadataV1Test, LoadBucketInfo) {
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader),
              get_keyval(_, "12345/seagate", _, _)).Times(1);
  action_under_test->bucket_owner_account_id = "12345";
  action_under_test->bucket_name = "seagate";
  action_under_test->load_bucket_info();
}

TEST_F(S3BucketMetadataV1Test, LoadBucketInfoFailedJsonParsingFailed) {
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  action_under_test->json_parsing_error = true;
  action_under_test->load_bucket_info_failed();
  EXPECT_TRUE(s3bucketmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3BucketMetadataState::failed, action_under_test->state);
}

TEST_F(S3BucketMetadataV1Test, LoadBucketInfoFailedMetadataMissing) {
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::missing));
  action_under_test->load_bucket_info_failed();
  EXPECT_TRUE(s3bucketmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3BucketMetadataState::missing, action_under_test->state);
}

TEST_F(S3BucketMetadataV1Test, LoadBucketInfoFailedMetadataFailed) {
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::failed));
  action_under_test->load_bucket_info_failed();
  EXPECT_TRUE(s3bucketmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3BucketMetadataState::failed, action_under_test->state);
}

TEST_F(S3BucketMetadataV1Test, LoadBucketInfoFailedMetadataFailedToLaunch) {
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  action_under_test->clovis_kv_reader =
      clovis_kvs_reader_factory->mock_clovis_kvs_reader;
  EXPECT_CALL(*(clovis_kvs_reader_factory->mock_clovis_kvs_reader), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3ClovisKVSReaderOpState::failed_to_launch));
  action_under_test->load_bucket_info_failed();
  EXPECT_TRUE(s3bucketmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3BucketMetadataState::failed_to_launch, action_under_test->state);
}

TEST_F(S3BucketMetadataV1Test, SaveMeatdataMissingIndexOID) {
  struct m0_uint128 oid = {0ULL, 0ULL};
  action_under_test->set_bucket_metadata_list_index_oid(oid);
  EXPECT_CALL(*(s3_global_bucket_index_metadata_factory
                    ->mock_global_bucket_index_metadata),
              load(_, _)).Times(1);
  action_under_test->save(
      std::bind(&S3CallBack::on_success, &s3bucketmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj));
  EXPECT_EQ(action_under_test->S3BucketMetadataCurrentOp::saving,
            action_under_test->current_op);
}

TEST_F(S3BucketMetadataV1Test, SaveMeatdataIndexOIDPresent) {
  struct m0_uint128 oid = {0x111f, 0xffff};
  action_under_test->set_bucket_metadata_list_index_oid(oid);
  action_under_test->object_list_index_oid = {0x11ff, 0x1fff};
  action_under_test->bucket_owner_account_id = "12345";
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              put_keyval(_, _, _, _, _)).Times(1);
  action_under_test->save(
      std::bind(&S3CallBack::on_success, &s3bucketmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj));
  EXPECT_EQ(action_under_test->S3BucketMetadataCurrentOp::saving,
            action_under_test->current_op);
}

TEST_F(S3BucketMetadataV1Test, CreateObjectIndexOIDNotPresent) {
  struct m0_uint128 oid = {0x111f, 0xffff};
  ;
  action_under_test->set_bucket_metadata_list_index_oid(oid);
  action_under_test->object_list_index_oid = {0x0000, 0x0000};
  action_under_test->bucket_owner_account_id = "12345";
  EXPECT_CALL(*ptr_mock_s3_clovis_api, m0_h_ufid_next(_))
      .WillRepeatedly(Invoke(dummy_helpers_ufid_next));

  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              create_index_with_oid(_, _, _)).Times(1);
  action_under_test->save(
      std::bind(&S3CallBack::on_success, &s3bucketmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj));
  EXPECT_EQ(action_under_test->S3BucketMetadataCurrentOp::saving,
            action_under_test->current_op);
}

TEST_F(S3BucketMetadataV1Test, CreateObjectListIndexCollisionCount0) {
  action_under_test->collision_attempt_count = 0;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              create_index_with_oid(_, _, _)).Times(1);
  action_under_test->create_object_list_index();
}

TEST_F(S3BucketMetadataV1Test, CreateMultipartListIndexCollisionCount0) {
  action_under_test->collision_attempt_count = 0;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              create_index_with_oid(_, _, _)).Times(1);
  action_under_test->create_multipart_list_index();
}

TEST_F(S3BucketMetadataV1Test, CreateObjectListIndexSuccessful) {
  action_under_test->collision_attempt_count = 0;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              create_index_with_oid(_, _, _)).Times(1);
  action_under_test->create_object_list_index_successful();
}

TEST_F(S3BucketMetadataV1Test, CreateObjectListIndexFailedCollisionHappened) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  action_under_test->collision_attempt_count = 1;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::exists));

  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              create_index_with_oid(_, _, _)).Times(1);

  action_under_test->create_object_list_index_failed();
  EXPECT_EQ(2, action_under_test->collision_attempt_count);
}

TEST_F(S3BucketMetadataV1Test,
       CreateMultipartListIndexFailedCollisionHappened) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  action_under_test->collision_attempt_count = 1;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::exists));

  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              create_index_with_oid(_, _, _)).Times(1);

  action_under_test->create_multipart_list_index_failed();
  EXPECT_EQ(2, action_under_test->collision_attempt_count);
}

TEST_F(S3BucketMetadataV1Test, CreateObjectListIndexFailed) {
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(2)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  action_under_test->create_object_list_index_failed();
  EXPECT_EQ(action_under_test->state, S3BucketMetadataState::failed);
}

TEST_F(S3BucketMetadataV1Test, CreateObjectListIndexFailedToLaunch) {
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(2)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  action_under_test->create_object_list_index_failed();
  EXPECT_EQ(action_under_test->state, S3BucketMetadataState::failed_to_launch);
}

TEST_F(S3BucketMetadataV1Test, CreateMultipartListIndexFailed) {
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(2)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  action_under_test->create_multipart_list_index_failed();
  EXPECT_TRUE(s3bucketmetadata_callbackobj.fail_called);
  EXPECT_EQ(action_under_test->state, S3BucketMetadataState::failed);
}

TEST_F(S3BucketMetadataV1Test, CreateMultipartListIndexFailedToLaunch) {
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(2)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  action_under_test->create_multipart_list_index_failed();
  EXPECT_TRUE(s3bucketmetadata_callbackobj.fail_called);
  EXPECT_EQ(action_under_test->state, S3BucketMetadataState::failed_to_launch);
}

TEST_F(S3BucketMetadataV1Test, HandleCollision) {
  action_under_test->collision_attempt_count = 1;
  std::string base_index_name = "BUCKET/seagate";
  action_under_test->salted_object_list_index_name = "BUCKET/seagate_salt_0";
  action_under_test->handle_collision(
      base_index_name, action_under_test->salted_object_list_index_name,
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj));
  EXPECT_STREQ("BUCKET/seagateindex_salt_1",
               action_under_test->salted_object_list_index_name.c_str());
  EXPECT_EQ(2, action_under_test->collision_attempt_count);
  EXPECT_TRUE(s3bucketmetadata_callbackobj.fail_called);
}

TEST_F(S3BucketMetadataV1Test, HandleCollisionMaxAttemptExceeded) {
  std::string base_index_name = "BUCKET/seagate";
  action_under_test->salted_object_list_index_name = "BUCKET/seagate_salt_0";
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  action_under_test->collision_attempt_count = MAX_COLLISION_RETRY_COUNT + 1;
  action_under_test->handle_collision(
      base_index_name, action_under_test->salted_object_list_index_name,
      std::bind(&S3CallBack::on_success, &s3bucketmetadata_callbackobj));
  EXPECT_EQ(S3BucketMetadataState::failed, action_under_test->state);
  EXPECT_EQ(MAX_COLLISION_RETRY_COUNT + 1,
            action_under_test->collision_attempt_count);
}

TEST_F(S3BucketMetadataV1Test, RegeneratedNewIndexName) {
  std::string base_index_name = "BUCKET/seagate";
  action_under_test->salted_object_list_index_name = "BUCKET/seagate";
  action_under_test->collision_attempt_count = 2;
  action_under_test->regenerate_new_index_name(
      base_index_name, action_under_test->salted_object_list_index_name);
  EXPECT_STREQ("BUCKET/seagateindex_salt_2",
               action_under_test->salted_object_list_index_name.c_str());
}

TEST_F(S3BucketMetadataV1Test, SaveBucketInfo) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              put_keyval(_, _, _, _, _)).Times(1);

  action_under_test->save_bucket_info();
  EXPECT_STREQ(
      "s3user",
      action_under_test->system_defined_attribute["Owner-User"].c_str());
  EXPECT_STREQ(
      "s3userid",
      action_under_test->system_defined_attribute["Owner-User-id"].c_str());
  EXPECT_STREQ(
      "s3account",
      action_under_test->system_defined_attribute["Owner-Account"].c_str());
  EXPECT_STREQ(
      "s3accountid",
      action_under_test->system_defined_attribute["Owner-Account-id"].c_str());
}

TEST_F(S3BucketMetadataV1Test, SaveBucketInfoSuccess) {
  action_under_test->handler_on_success =
      std::bind(&S3CallBack::on_success, &s3bucketmetadata_callbackobj);
  action_under_test->save_bucket_info_successful();
  EXPECT_TRUE(s3bucketmetadata_callbackobj.success_called);
}

TEST_F(S3BucketMetadataV1Test, SaveBucketInfoFailed) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  action_under_test->save_bucket_info_failed();
  EXPECT_TRUE(s3bucketmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3BucketMetadataState::failed, action_under_test->state);
}

TEST_F(S3BucketMetadataV1Test, SaveBucketInfoFailedToLaunch) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  action_under_test->save_bucket_info_failed();
  EXPECT_TRUE(s3bucketmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3BucketMetadataState::failed_to_launch, action_under_test->state);
}

TEST_F(S3BucketMetadataV1Test, RemovePresentMetadata) {
  action_under_test->state = S3BucketMetadataState::present;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_keyval(_, _, _, _)).Times(1);

  action_under_test->remove(
      std::bind(&S3CallBack::on_success, &s3bucketmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj));
  EXPECT_TRUE(action_under_test->handler_on_success != NULL);
  EXPECT_TRUE(action_under_test->handler_on_failed != NULL);
}

TEST_F(S3BucketMetadataV1Test, RemoveAfterFetchingBucketListIndexOID) {
  action_under_test->state = S3BucketMetadataState::missing;
  action_under_test->global_bucket_index_metadata =
      s3_global_bucket_index_metadata_factory
          ->mock_global_bucket_index_metadata;

  EXPECT_CALL(*(s3_global_bucket_index_metadata_factory
                    ->mock_global_bucket_index_metadata),
              load(_, _)).Times(1);
  action_under_test->remove(
      std::bind(&S3CallBack::on_success, &s3bucketmetadata_callbackobj),
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj));

  EXPECT_EQ(action_under_test->S3BucketMetadataCurrentOp::deleting,
            action_under_test->current_op);
}

TEST_F(S3BucketMetadataV1Test, RemoveBucketInfo) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_keyval(_, _, _, _)).Times(1);
  action_under_test->remove_bucket_info();
}

TEST_F(S3BucketMetadataV1Test, RemoveBucketInfoSuccessful) {
  action_under_test->global_bucket_index_metadata =
      s3_global_bucket_index_metadata_factory
          ->mock_global_bucket_index_metadata;

  EXPECT_CALL(*(s3_global_bucket_index_metadata_factory
                    ->mock_global_bucket_index_metadata),
              remove(_, _)).Times(1);

  action_under_test->remove_bucket_info_successful();
}

TEST_F(S3BucketMetadataV1Test, RemoveBucketAccountidInfoSuccessful) {
  action_under_test->handler_on_success =
      std::bind(&S3CallBack::on_success, &s3bucketmetadata_callbackobj);

  action_under_test->remove_global_bucket_account_id_info_successful();
  EXPECT_TRUE(s3bucketmetadata_callbackobj.success_called);
}

TEST_F(S3BucketMetadataV1Test, RemoveBucketAccountidInfoFailedToLaunch) {
  action_under_test->global_bucket_index_metadata =
      s3_global_bucket_index_metadata_factory
          ->mock_global_bucket_index_metadata;

  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  EXPECT_CALL(*(s3_global_bucket_index_metadata_factory
                    ->mock_global_bucket_index_metadata),
              get_state())
      .Times(1)
      .WillRepeatedly(
           Return(S3GlobalBucketIndexMetadataState::failed_to_launch));

  action_under_test->remove_global_bucket_account_id_info_failed();
  EXPECT_EQ(S3BucketMetadataState::failed_to_launch, action_under_test->state);
  EXPECT_TRUE(s3bucketmetadata_callbackobj.fail_called);
}

TEST_F(S3BucketMetadataV1Test, RemoveBucketAccountidInfoFailed) {
  action_under_test->global_bucket_index_metadata =
      s3_global_bucket_index_metadata_factory
          ->mock_global_bucket_index_metadata;

  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  EXPECT_CALL(*(s3_global_bucket_index_metadata_factory
                    ->mock_global_bucket_index_metadata),
              get_state())
      .Times(1)
      .WillRepeatedly(Return(S3GlobalBucketIndexMetadataState::failed));

  action_under_test->remove_global_bucket_account_id_info_failed();
  EXPECT_EQ(S3BucketMetadataState::failed, action_under_test->state);
  EXPECT_TRUE(s3bucketmetadata_callbackobj.fail_called);
}

TEST_F(S3BucketMetadataV1Test, RemoveBucketInfoFailed) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));
  action_under_test->remove_bucket_info_failed();
  EXPECT_EQ(S3BucketMetadataState::failed, action_under_test->state);
  EXPECT_TRUE(s3bucketmetadata_callbackobj.fail_called);
}

TEST_F(S3BucketMetadataV1Test, RemoveBucketInfoFailedToLaunch) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  action_under_test->handler_on_failed =
      std::bind(&S3CallBack::on_failed, &s3bucketmetadata_callbackobj);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .Times(1)
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));
  action_under_test->remove_bucket_info_failed();
  EXPECT_TRUE(s3bucketmetadata_callbackobj.fail_called);
  EXPECT_EQ(S3BucketMetadataState::failed_to_launch, action_under_test->state);
}

TEST_F(S3BucketMetadataV1Test, ToJson) {
  std::string json_str = action_under_test->to_json();
  Json::Value newglobal;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(json_str, newglobal);
  EXPECT_TRUE(parsingSuccessful);
}

TEST_F(S3BucketMetadataV1Test, FromJson) {
  struct m0_uint128 zero_oid = {0ULL, 0ULL};
  EXPECT_OID_EQ(zero_oid, action_under_test->multipart_index_oid);
  std::string json_str =
      "{\"ACL\":\"PD94+Cg==\",\"Bucket-Name\":\"seagatebucket\",\"System-"
      "Defined\":{\"Date\":\"2016-10-18T16:01:00.000Z\",\"Owner-Account\":\"s3_"
      "test\",\"Owner-Account-id\":\"s3accountid\",\"Owner-User\":\"tester\",\""
      "Owner-User-id\":\"s3userid\"},\"mero_multipart_index_oid\":\""
      "g1qTetGfvWk=-lvH6Q65xFAI=\","
      "\"mero_object_list_index_oid\":\"AAAAAAAAAAA=-AAAAAAAAAAA=\"}";

  action_under_test->from_json(json_str);
  EXPECT_STREQ("seagatebucket", action_under_test->bucket_name.c_str());
  EXPECT_STREQ("tester", action_under_test->user_name.c_str());
  EXPECT_STREQ("s3_test", action_under_test->account_name.c_str());
  EXPECT_STREQ("s3accountid", action_under_test->account_id.c_str());
  EXPECT_OID_NE(zero_oid, action_under_test->multipart_index_oid);
}

TEST_F(S3BucketMetadataV1Test, GetEncodedBucketAcl) {
  std::string json_str = "{\"ACL\":\"PD94bg==\"}";

  action_under_test->from_json(json_str);
  EXPECT_STREQ("PD94bg==", action_under_test->get_encoded_bucket_acl().c_str());
}

