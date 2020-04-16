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
* Original author:  Priya Saboo   <priya.chhagan@seagate.com>
* Original creation date: 15-May-2017
*/

#include "s3_object_list_response.h"
#include "gtest/gtest.h"
#include "mock_s3_async_buffer_opt_container.h"
#include "mock_s3_factory.h"
#include "mock_s3_object_metadata.h"
#include "mock_s3_part_metadata.h"
#include "mock_s3_request_object.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::HasSubstr;

#define CHECK_XML_RESPONSE                        \
  do {                                            \
    EXPECT_THAT(response, HasSubstr("obj1"));     \
    EXPECT_THAT(response, HasSubstr("abcd"));     \
    EXPECT_THAT(response, HasSubstr("1024"));     \
    EXPECT_THAT(response, HasSubstr("STANDARD")); \
    EXPECT_THAT(response, HasSubstr("1"));        \
  } while (0)

#define CHECK_MULTIUPLOAD_XML_RESPONSE               \
  do {                                               \
    EXPECT_THAT(response, HasSubstr("object_name")); \
    EXPECT_THAT(response, HasSubstr("upload_id"));   \
    EXPECT_THAT(response, HasSubstr("1"));           \
  } while (0)

#define CHECK_MULTIPART_XML_RESPONSE              \
  do {                                            \
    EXPECT_THAT(response, HasSubstr("abcd"));     \
    EXPECT_THAT(response, HasSubstr("1024"));     \
    EXPECT_THAT(response, HasSubstr("STANDARD")); \
    EXPECT_THAT(response, HasSubstr("1"));        \
  } while (0)

class S3ObjectListResponseTest : public testing::Test {
 protected:
  S3ObjectListResponseTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    bucket_name = "seagatebucket";
    object_name = "objname";

    object_list_indx_oid = {0x11ffff, 0x1ffff};

    async_buffer_factory =
        std::make_shared<MockS3AsyncBufferOptContainerFactory>(
            S3Option::get_instance()->get_libevent_pool_buffer_size());

    mock_request = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr,
                                                         async_buffer_factory);
    EXPECT_CALL(*mock_request, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));
    EXPECT_CALL(*mock_request, get_object_name())
        .WillRepeatedly(ReturnRef(object_name));
    mock_request->set_user_id("1");
    mock_request->set_user_name("s3user");
    mock_request->set_account_id("1");
    mock_request->set_account_name("s3user");
    mock_request->set_canonical_id("qWwZGnGYTga8gbpcuY79SA");

    response_under_test = std::make_shared<S3ObjectListResponse>();
  }

  virtual void SetUp() {
    response_under_test->set_bucket_name("test.bucket.name");
    response_under_test->set_request_marker_key("test_request_marker_key");
    response_under_test->set_next_marker_key("test_next_marker_key");
    response_under_test->set_response_is_truncated(true);
  }

  std::shared_ptr<S3ObjectListResponse> response_under_test;
  std::shared_ptr<MockS3RequestObject> mock_request;
  std::shared_ptr<MockS3AsyncBufferOptContainerFactory> async_buffer_factory;
  struct m0_uint128 object_list_indx_oid;
  std::string bucket_name, object_name;
};

// Test fields are initialized to empty.
TEST_F(S3ObjectListResponseTest, ObjectListResponseConstructorTest) {
  EXPECT_EQ("", response_under_test->request_prefix);
  EXPECT_EQ("", response_under_test->request_delimiter);
  EXPECT_EQ("", response_under_test->request_marker_uploadid);
  EXPECT_EQ("", response_under_test->max_keys);
  EXPECT_EQ("", response_under_test->response_xml);
  EXPECT_EQ("", response_under_test->max_uploads);
  EXPECT_EQ("", response_under_test->next_marker_uploadid);
  response_under_test->set_response_is_truncated(false);
  EXPECT_FALSE(response_under_test->response_is_truncated);
}

// Test all setter methods for S3ObjectListResponse.
TEST_F(S3ObjectListResponseTest, TestS3ObjectListResponseSetters) {
  EXPECT_EQ("test.bucket.name", response_under_test->bucket_name);
  response_under_test->set_object_name("test_object_name");
  EXPECT_EQ("test_object_name", response_under_test->object_name);
  response_under_test->set_request_prefix("test_prefix");
  EXPECT_EQ("test_prefix", response_under_test->request_prefix);
  response_under_test->set_request_delimiter("test_delimiter");
  EXPECT_EQ("test_delimiter", response_under_test->request_delimiter);
  EXPECT_EQ("test_request_marker_key", response_under_test->request_marker_key);
  response_under_test->set_request_marker_uploadid(
      "test_request_marker_uploadid");
  EXPECT_EQ("test_request_marker_uploadid",
            response_under_test->request_marker_uploadid);
  response_under_test->set_max_keys("test_max_key_count");
  EXPECT_EQ("test_max_key_count", response_under_test->max_keys);
  response_under_test->set_max_uploads("test_max_upload_count");
  EXPECT_EQ("test_max_upload_count", response_under_test->max_uploads);
  response_under_test->set_max_parts("test_max_part_count");
  EXPECT_EQ("test_max_part_count", response_under_test->max_parts);
  response_under_test->set_response_is_truncated(true);
  EXPECT_TRUE(response_under_test->response_is_truncated);
  EXPECT_EQ("test_next_marker_key", response_under_test->next_marker_key);
  response_under_test->set_next_marker_uploadid("test_next_marker_uploadid");
  EXPECT_EQ("test_next_marker_uploadid",
            response_under_test->next_marker_uploadid);
  response_under_test->set_user_id("test_user_id");
  EXPECT_EQ("test_user_id", response_under_test->user_id);
  response_under_test->set_user_name("test_user_name");
  EXPECT_EQ("test_user_name", response_under_test->user_name);
  response_under_test->set_account_id("test_account_id");
  EXPECT_EQ("test_account_id", response_under_test->account_id);
  response_under_test->set_account_name("test_account_name");
  EXPECT_EQ("test_account_name", response_under_test->account_name);
  response_under_test->set_storage_class("test_storage_class");
  EXPECT_EQ("test_storage_class", response_under_test->storage_class);
  response_under_test->set_upload_id("test_upload_id");
  EXPECT_EQ("test_upload_id", response_under_test->upload_id);
}

// Test all getter methods for S3ObjectListResponse.
TEST_F(S3ObjectListResponseTest, TestS3ObjectListResponseGetters) {
  response_under_test->set_user_id("user_id");
  EXPECT_STREQ(response_under_test->get_user_id().c_str(), "user_id");
  response_under_test->set_user_name("user_name");
  EXPECT_STREQ(response_under_test->get_user_name().c_str(), "user_name");
  response_under_test->set_account_id("account_id");
  EXPECT_STREQ(response_under_test->get_account_id().c_str(), "account_id");
  response_under_test->set_account_name("account_name");
  EXPECT_STREQ(response_under_test->get_account_name().c_str(), "account_name");
  response_under_test->set_storage_class("storage_class");
  EXPECT_STREQ(response_under_test->get_storage_class().c_str(),
               "storage_class");
  response_under_test->set_upload_id("upload_id");
  EXPECT_STREQ(response_under_test->get_upload_id().c_str(), "upload_id");
}

// Test get_xml response with valid object and all of the results were returned.
TEST_F(S3ObjectListResponseTest, ObjectListResponseWithValidObjectsTruncated) {
  response_under_test->set_request_prefix("test_prefix");
  response_under_test->set_request_delimiter("test_delimiter");
  response_under_test->set_max_keys("test_max_key_count");
  response_under_test->set_response_is_truncated(true);
  response_under_test->add_common_prefix("prefix1");

  std::shared_ptr<MockS3ObjectMetadata> mock_obj =
      std::make_shared<MockS3ObjectMetadata>(mock_request);
  mock_obj->set_object_list_index_oid(object_list_indx_oid);

  response_under_test->add_object(mock_obj);

  EXPECT_CALL(*mock_obj, get_object_name()).WillOnce(Return("obj1"));
  EXPECT_CALL(*mock_obj, get_last_modified_iso())
      .WillOnce(Return("last_modified"));
  EXPECT_CALL(*mock_obj, get_md5()).WillOnce(Return("abcd"));
  EXPECT_CALL(*mock_obj, get_content_length_str()).WillOnce(Return("1024"));
  EXPECT_CALL(*mock_obj, get_storage_class()).WillOnce(Return("STANDARD"));
  EXPECT_CALL(*mock_obj, get_canonical_id()).Times(2).WillOnce(
      Return("qWwZGnGYTga8gbpcuY79SA"));
  EXPECT_CALL(*mock_obj, get_account_name()).WillOnce(Return("s3user"));

  std::string response =
      response_under_test->get_xml("qWwZGnGYTga8gbpcuY79SA", "1", "1");
  CHECK_XML_RESPONSE;
}

// Test get_xml response with valid object and results is truncated.
TEST_F(S3ObjectListResponseTest,
       ObjectListResponseWithValidObjectsNotTruncated) {
  response_under_test->add_common_prefix("prefix1");
  response_under_test->set_response_is_truncated(false);
  std::shared_ptr<MockS3ObjectMetadata> mock_obj =
      std::make_shared<MockS3ObjectMetadata>(mock_request);
  mock_obj->set_object_list_index_oid(object_list_indx_oid);

  response_under_test->add_object(mock_obj);

  EXPECT_CALL(*mock_obj, get_object_name()).WillOnce(Return("obj1"));
  EXPECT_CALL(*mock_obj, get_last_modified_iso())
      .WillOnce(Return("last_modified"));
  EXPECT_CALL(*mock_obj, get_md5()).WillOnce(Return("abcd"));
  EXPECT_CALL(*mock_obj, get_content_length_str()).WillOnce(Return("1024"));
  EXPECT_CALL(*mock_obj, get_storage_class()).WillOnce(Return("STANDARD"));
  EXPECT_CALL(*mock_obj, get_canonical_id()).Times(2).WillOnce(
      Return("qWwZGnGYTga8gbpcuY79SA"));
  EXPECT_CALL(*mock_obj, get_account_name()).WillOnce(Return("s3user"));

  std::string response =
      response_under_test->get_xml("qWwZGnGYTga8gbpcuY79SA", "1", "1");
  CHECK_XML_RESPONSE;
}

// Test get_multiupload_xml with valid object and result is truncated.
TEST_F(S3ObjectListResponseTest,
       ObjectListMultiuploadResponseWithValidObjectTruncated) {
  response_under_test->set_request_marker_uploadid(
      "test_request_marker_upload_id");
  response_under_test->set_max_keys("test_max_keys");
  response_under_test->set_next_marker_uploadid("test_next_marker_upload_id");
  response_under_test->set_max_uploads("test_max_uploads");
  response_under_test->set_response_is_truncated(true);
  response_under_test->add_common_prefix("prefix1");

  std::shared_ptr<MockS3ObjectMetadata> mock_obj =
      std::make_shared<MockS3ObjectMetadata>(mock_request);
  mock_obj->set_object_list_index_oid(object_list_indx_oid);

  response_under_test->add_object(mock_obj);

  EXPECT_CALL(*mock_obj, get_object_name()).WillOnce(Return("object_name"));
  EXPECT_CALL(*mock_obj, get_upload_id()).WillOnce(Return("upload_id"));
  EXPECT_CALL(*mock_obj, get_last_modified_iso())
      .WillRepeatedly(Return("last_modified"));
  EXPECT_CALL(*mock_obj, get_user_id()).WillRepeatedly(Return("1"));
  EXPECT_CALL(*mock_obj, get_user_name()).WillRepeatedly(Return("s3user"));
  EXPECT_CALL(*mock_obj, get_storage_class())
      .WillRepeatedly(Return("STANDARD"));
  std::string response = response_under_test->get_multiupload_xml();

  CHECK_MULTIUPLOAD_XML_RESPONSE;
  EXPECT_THAT(response, HasSubstr("<IsTruncated>true</IsTruncated>"));
}

// Test get_multiupload_xml with valid object and result is not truncated.
TEST_F(S3ObjectListResponseTest,
       ObjectListMultiuploadResponseWithValidObjectNotTruncated) {
  response_under_test->set_request_marker_uploadid(
      "test_request_marker_upload_id");
  response_under_test->set_max_keys("test_max_keys");
  response_under_test->set_next_marker_uploadid("test_next_marker_upload_id");
  response_under_test->set_max_uploads("test_max_uploads");
  response_under_test->set_response_is_truncated(false);
  response_under_test->add_common_prefix("prefix1");

  std::shared_ptr<MockS3ObjectMetadata> mock_obj =
      std::make_shared<MockS3ObjectMetadata>(mock_request);
  mock_obj->set_object_list_index_oid(object_list_indx_oid);

  response_under_test->add_object(mock_obj);

  EXPECT_CALL(*mock_obj, get_object_name()).WillOnce(Return("object_name"));
  EXPECT_CALL(*mock_obj, get_upload_id()).WillOnce(Return("upload_id"));
  EXPECT_CALL(*mock_obj, get_last_modified_iso())
      .WillRepeatedly(Return("last_modified"));
  EXPECT_CALL(*mock_obj, get_user_id()).WillRepeatedly(Return("1"));
  EXPECT_CALL(*mock_obj, get_user_name()).WillRepeatedly(Return("s3user"));
  EXPECT_CALL(*mock_obj, get_storage_class())
      .WillRepeatedly(Return("STANDARD"));

  std::string response = response_under_test->get_multiupload_xml();

  CHECK_MULTIUPLOAD_XML_RESPONSE;
  EXPECT_THAT(response, HasSubstr("<IsTruncated>false</IsTruncated>"));
}

// Test multipart_xml with valid object and result is truncated.
TEST_F(S3ObjectListResponseTest,
       ObjectListMultipartResponseWithValidObjectNotTruncated) {
  response_under_test->set_max_parts("test_max_part_count");
  response_under_test->set_response_is_truncated(true);
  response_under_test->set_upload_id("test_upload_id");
  response_under_test->set_storage_class("STANDARD");

  std::shared_ptr<MockS3PartMetadata> mock_part =
      std::make_shared<MockS3PartMetadata>(mock_request, object_list_indx_oid,
                                           "test_upload_id", 1);
  response_under_test->add_part(mock_part);

  EXPECT_CALL(*mock_part, get_md5()).WillOnce(Return("abcd"));
  EXPECT_CALL(*mock_part, get_content_length_str()).WillOnce(Return("1024"));
  EXPECT_CALL(*mock_part, get_last_modified_iso())
      .WillRepeatedly(Return("last_modified"));

  std::string response = response_under_test->get_multipart_xml();

  CHECK_MULTIPART_XML_RESPONSE;
}

// Test multipart_xml with valid object and result is not truncated.
TEST_F(S3ObjectListResponseTest,
       ObjectListMultipartResponseWithValidObjectTruncated) {
  response_under_test->set_max_parts("test_max_part_count");
  response_under_test->set_response_is_truncated(false);
  response_under_test->set_upload_id("test_upload_id");
  response_under_test->set_storage_class("STANDARD");

  std::shared_ptr<MockS3PartMetadata> mock_part =
      std::make_shared<MockS3PartMetadata>(mock_request, object_list_indx_oid,
                                           "test_upload_id", 1);
  response_under_test->add_part(mock_part);

  EXPECT_CALL(*mock_part, get_md5()).WillOnce(Return("abcd"));
  EXPECT_CALL(*mock_part, get_content_length_str()).WillOnce(Return("1024"));
  EXPECT_CALL(*mock_part, get_last_modified_iso())
      .WillRepeatedly(Return("last_modified"));

  std::string response = response_under_test->get_multipart_xml();

  CHECK_MULTIPART_XML_RESPONSE;
}

