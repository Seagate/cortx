/*
 * COPYRIGHT 2015 SEAGATE LLC
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
 * Original creation date: 09-Nov-2015
 */

#include "s3_uri.h"
#include "mock_s3_request_object.h"

using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;
using ::testing::ReturnRef;

class S3URITEST : public testing::Test {
 protected:
  S3URITEST() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
  }

  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
};

class S3PathStyleURITEST : public S3URITEST {};
class S3VirtualHostStyleURITEST : public S3URITEST {};

TEST_F(S3URITEST, Constructor) {
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters()).Times(1).WillOnce(
      ReturnRef(query_params));

  S3URI s3uri_test_obj(ptr_mock_request);
  EXPECT_STREQ("", s3uri_test_obj.get_bucket_name().c_str());
  EXPECT_STREQ("", s3uri_test_obj.get_object_name().c_str());
  EXPECT_EQ(S3ApiType::unsupported, s3uri_test_obj.get_s3_api_type());
  EXPECT_EQ(S3OperationCode::none, s3uri_test_obj.get_operation_code());
}

TEST_F(S3URITEST, OperationCode) {
  std::map<std::string, std::string, compare> query_params;
  query_params["acl"] = "";

  EXPECT_CALL(*ptr_mock_request, get_query_parameters()).Times(1).WillOnce(
      ReturnRef(query_params));

  S3URI s3uri_test_obj(ptr_mock_request);
  EXPECT_EQ(S3OperationCode::acl, s3uri_test_obj.get_operation_code());
}

TEST_F(S3PathStyleURITEST, ServiceTest) {
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters()).Times(1).WillOnce(
      ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, has_query_param_key(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*ptr_mock_request, c_get_full_path()).WillOnce(Return("/"));
  EXPECT_CALL(*ptr_mock_request, get_header_value("x-seagate-faultinjection"))
      .Times(1)
      .WillRepeatedly(Return(""));
  S3PathStyleURI s3pathstyle_service(ptr_mock_request);
  EXPECT_EQ(S3ApiType::service, s3pathstyle_service.get_s3_api_type());
  EXPECT_STREQ("", s3pathstyle_service.get_bucket_name().c_str());
  EXPECT_STREQ("", s3pathstyle_service.get_object_name().c_str());
}

TEST_F(S3PathStyleURITEST, BucketTest) {
  evhtp_request_t *req = NULL;
  EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
  std::shared_ptr<MockS3RequestObject> ptr_mock_request =
      std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, c_get_full_path())
      .WillOnce(Return("/seagate"))
      .WillOnce(Return("/TrailingSlash/"));

  S3PathStyleURI s3pathstyleone(ptr_mock_request);
  EXPECT_EQ(S3ApiType::bucket, s3pathstyleone.get_s3_api_type());
  EXPECT_STREQ("seagate", s3pathstyleone.get_bucket_name().c_str());
  EXPECT_STREQ("", s3pathstyleone.get_object_name().c_str());

  S3PathStyleURI s3pathstyletwo(ptr_mock_request);
  EXPECT_EQ(S3ApiType::bucket, s3pathstyletwo.get_s3_api_type());
  EXPECT_STREQ("TrailingSlash", s3pathstyletwo.get_bucket_name().c_str());
  EXPECT_STREQ("", s3pathstyletwo.get_object_name().c_str());
}

TEST_F(S3PathStyleURITEST, ObjectTest) {
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, c_get_full_path())
      .WillOnce(Return("/seagate/8kfile"))
      .WillOnce(Return("/seagate/test.txt"))
      .WillOnce(Return("/seagate/somedir/"));

  S3PathStyleURI s3pathstyleone(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3pathstyleone.get_s3_api_type());
  EXPECT_STREQ("seagate", s3pathstyleone.get_bucket_name().c_str());
  EXPECT_STREQ("8kfile", s3pathstyleone.get_object_name().c_str());

  S3PathStyleURI s3pathstyletwo(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3pathstyletwo.get_s3_api_type());
  EXPECT_STREQ("seagate", s3pathstyletwo.get_bucket_name().c_str());
  EXPECT_STREQ("test.txt", s3pathstyletwo.get_object_name().c_str());

  S3PathStyleURI s3pathstylethree(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3pathstylethree.get_s3_api_type());
  EXPECT_STREQ("seagate", s3pathstylethree.get_bucket_name().c_str());
  EXPECT_STREQ("somedir/", s3pathstylethree.get_object_name().c_str());
}

TEST_F(S3PathStyleURITEST, ObjectDirTest) {
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, c_get_full_path())
      .WillOnce(Return("/seagate/mh/pune/8kfile"))
      .WillOnce(Return("/seagate/mh/pune/test.txt"));

  S3PathStyleURI s3pathstyleone(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3pathstyleone.get_s3_api_type());
  EXPECT_STREQ("seagate", s3pathstyleone.get_bucket_name().c_str());
  EXPECT_STREQ("mh/pune/8kfile", s3pathstyleone.get_object_name().c_str());

  S3PathStyleURI s3pathstyletwo(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3pathstyletwo.get_s3_api_type());
  EXPECT_STREQ("seagate", s3pathstyletwo.get_bucket_name().c_str());
  EXPECT_STREQ("mh/pune/test.txt", s3pathstyletwo.get_object_name().c_str());
}

TEST_F(S3PathStyleURITEST, ObjectDirTrailSlashTest) {
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, c_get_full_path())
      .WillOnce(Return("/seagate/mh/pune/8kfile/"))
      .WillOnce(Return("/seagate/mh/pune/test.txt"));

  S3PathStyleURI s3pathstyleone(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3pathstyleone.get_s3_api_type());
  EXPECT_STREQ("seagate", s3pathstyleone.get_bucket_name().c_str());
  EXPECT_STREQ("mh/pune/8kfile/", s3pathstyleone.get_object_name().c_str());

  S3PathStyleURI s3pathstyletwo(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3pathstyletwo.get_s3_api_type());
  EXPECT_STREQ("seagate", s3pathstyletwo.get_bucket_name().c_str());
  EXPECT_STREQ("mh/pune/test.txt", s3pathstyletwo.get_object_name().c_str());
}

TEST_F(S3VirtualHostStyleURITEST, BucketTest) {
  std::string bucket;
  std::string object;
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, get_host_header())
      .WillOnce(Return("bucket_name.s3.seagate.com"));
  EXPECT_CALL(*ptr_mock_request, c_get_full_path()).WillOnce(Return("/"));

  S3VirtualHostStyleURI s3virtualhost(ptr_mock_request);
  EXPECT_EQ(S3ApiType::bucket, s3virtualhost.get_s3_api_type());
  EXPECT_STREQ("bucket_name", s3virtualhost.get_bucket_name().c_str());
  EXPECT_STREQ("", s3virtualhost.get_object_name().c_str());
}

TEST_F(S3VirtualHostStyleURITEST, ObjectTest) {
  std::string bucket;
  std::string object;
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, get_host_header())
      .WillRepeatedly(Return("bucket_name.s3.seagate.com"));
  EXPECT_CALL(*ptr_mock_request, c_get_full_path())
      .WillOnce(Return("/8kfile"))
      .WillOnce(Return("/test.txt"))
      .WillOnce(Return("/somedir/"));

  S3VirtualHostStyleURI s3virtualhostone(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3virtualhostone.get_s3_api_type());
  EXPECT_STREQ("bucket_name", s3virtualhostone.get_bucket_name().c_str());
  EXPECT_STREQ("8kfile", s3virtualhostone.get_object_name().c_str());

  S3VirtualHostStyleURI s3virtualhosttwo(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3virtualhosttwo.get_s3_api_type());
  EXPECT_STREQ("bucket_name", s3virtualhosttwo.get_bucket_name().c_str());
  EXPECT_STREQ("test.txt", s3virtualhosttwo.get_object_name().c_str());

  S3VirtualHostStyleURI s3virtualhostthree(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3virtualhostthree.get_s3_api_type());
  EXPECT_STREQ("bucket_name", s3virtualhostthree.get_bucket_name().c_str());
  EXPECT_STREQ("somedir/", s3virtualhostthree.get_object_name().c_str());
}

TEST_F(S3VirtualHostStyleURITEST, ObjectTrailSlashTest) {
  std::string bucket;
  std::string object;
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, get_host_header())
      .WillRepeatedly(Return("bucket_name.s3.seagate.com"));
  EXPECT_CALL(*ptr_mock_request, c_get_full_path())
      .WillOnce(Return("/8kfile/"))
      .WillOnce(Return("/somedir/"));

  S3VirtualHostStyleURI s3virtualhostone(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3virtualhostone.get_s3_api_type());
  EXPECT_STREQ("bucket_name", s3virtualhostone.get_bucket_name().c_str());
  EXPECT_STREQ("8kfile/", s3virtualhostone.get_object_name().c_str());

  S3VirtualHostStyleURI s3virtualhosttwo(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3virtualhosttwo.get_s3_api_type());
  EXPECT_STREQ("bucket_name", s3virtualhosttwo.get_bucket_name().c_str());
  EXPECT_STREQ("somedir/", s3virtualhosttwo.get_object_name().c_str());
}

TEST_F(S3VirtualHostStyleURITEST, ObjectDirTest) {
  std::string bucket;
  std::string object;
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, get_host_header())
      .WillRepeatedly(Return("bucket_name.s3.seagate.com"));
  EXPECT_CALL(*ptr_mock_request, c_get_full_path())
      .WillOnce(Return("/mh/pune/8kfile"))
      .WillOnce(Return("/mh/pune/test.txt"));

  S3VirtualHostStyleURI s3virtualhostone(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3virtualhostone.get_s3_api_type());
  EXPECT_STREQ("bucket_name", s3virtualhostone.get_bucket_name().c_str());
  EXPECT_STREQ("mh/pune/8kfile", s3virtualhostone.get_object_name().c_str());

  S3VirtualHostStyleURI s3virtualhosttwo(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3virtualhosttwo.get_s3_api_type());
  EXPECT_STREQ("bucket_name", s3virtualhosttwo.get_bucket_name().c_str());
  EXPECT_STREQ("mh/pune/test.txt", s3virtualhosttwo.get_object_name().c_str());
}

TEST_F(S3VirtualHostStyleURITEST, ObjectDirTrailSlashTest) {
  std::string bucket;
  std::string object;
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, get_host_header())
      .WillRepeatedly(Return("bucket_name.s3.seagate.com"));
  EXPECT_CALL(*ptr_mock_request, c_get_full_path())
      .WillOnce(Return("/mh/pune/8kfile/"))
      .WillOnce(Return("/mh/pune/test.txt"));

  S3VirtualHostStyleURI s3virtualhostone(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3virtualhostone.get_s3_api_type());
  EXPECT_STREQ("bucket_name", s3virtualhostone.get_bucket_name().c_str());
  EXPECT_STREQ("mh/pune/8kfile/", s3virtualhostone.get_object_name().c_str());

  S3VirtualHostStyleURI s3virtualhosttwo(ptr_mock_request);
  EXPECT_EQ(S3ApiType::object, s3virtualhosttwo.get_s3_api_type());
  EXPECT_STREQ("bucket_name", s3virtualhosttwo.get_bucket_name().c_str());
  EXPECT_STREQ("mh/pune/test.txt", s3virtualhosttwo.get_object_name().c_str());
}
