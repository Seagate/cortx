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
 * Original author:  Siddhivinayak Shanbhag <siddhivinayak.shanbhag@seagate.com>
 * Original creation date: 09-January-2019
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

#include "mock_s3_bucket_metadata.h"
#include "mock_s3_request_object.h"
#include "mock_s3_factory.h"
#include "s3_put_tag_body.h"

using ::testing::Invoke;
using ::testing::AtLeast;
using ::testing::ReturnRef;

class S3PutTagBodyTest : public testing::Test {
 protected:
  S3PutTagBodyTest() {
    put_bucket_tag_body_factory = std::make_shared<S3PutTagsBodyFactory>();
  }
  bool result = false;
  std::string RequestId;
  std::string BucketTagsStr;
  std::map<std::string, std::string> bucket_tags_map;
  std::shared_ptr<S3PutTagBody> put_bucket_tag_body;
  std::shared_ptr<S3PutTagsBodyFactory> put_bucket_tag_body_factory;
};

TEST_F(S3PutTagBodyTest, ValidateRequestBodyXml) {
  BucketTagsStr.assign(
      "<Tagging "
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/"
      "\"><TagSet><Tag><Key>organization124</Key><Value>marketing123</Value></"
      "Tag><Tag><Key>organization1234</Key><Value>marketing123</Value></Tag></"
      "TagSet></Tagging>");
  RequestId.assign("RequestId");

  put_bucket_tag_body =
      put_bucket_tag_body_factory->create_put_resource_tags_body(BucketTagsStr,
                                                                 RequestId);
  result = put_bucket_tag_body->isOK();
  EXPECT_TRUE(result);
}

TEST_F(S3PutTagBodyTest, ValidateRequestCompareContents) {
  std::map<std::string, std::string> request_tags_map;
  bool map_compare;
  BucketTagsStr.assign(
      "<Tagging "
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/"
      "\"><TagSet><Tag><Key>organization124</Key><Value>marketing123</Value></"
      "Tag><Tag><Key>organization1234</Key><Value>marketing1234</Value></Tag></"
      "TagSet></Tagging>");
  RequestId.assign("RequestId");
  bucket_tags_map["organization124"] = "marketing123";
  bucket_tags_map["organization1234"] = "marketing1234";

  put_bucket_tag_body =
      put_bucket_tag_body_factory->create_put_resource_tags_body(BucketTagsStr,
                                                                 RequestId);
  request_tags_map = put_bucket_tag_body->get_resource_tags_as_map();

  map_compare = (bucket_tags_map.size() == request_tags_map.size()) &&
                (std::equal(bucket_tags_map.begin(), bucket_tags_map.end(),
                            request_tags_map.begin()));
  result = put_bucket_tag_body->isOK();
  EXPECT_TRUE(result);
  EXPECT_TRUE(map_compare);
}

TEST_F(S3PutTagBodyTest, ValidateRepeatedKeysXml) {
  BucketTagsStr.assign(
      "<Tagging "
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/"
      "\"><TagSet><Tag><Key>organization124</Key><Value>seagate</Value></"
      "Tag><Tag><Key>organization124</Key><Value>seagate</Value></Tag></"
      "TagSet></Tagging>");
  RequestId.assign("RequestId");

  put_bucket_tag_body =
      put_bucket_tag_body_factory->create_put_resource_tags_body(BucketTagsStr,
                                                                 RequestId);
  result = put_bucket_tag_body->isOK();
  EXPECT_FALSE(result);
}

TEST_F(S3PutTagBodyTest, ValidateEmptyTagSetXml) {
  BucketTagsStr.assign(
      "<Tagging "
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\"><TagSet></TagSet></"
      "Tagging>");
  RequestId.assign("RequestId");

  put_bucket_tag_body =
      put_bucket_tag_body_factory->create_put_resource_tags_body(BucketTagsStr,
                                                                 RequestId);
  result = put_bucket_tag_body->isOK();
  EXPECT_TRUE(result);
}

TEST_F(S3PutTagBodyTest, ValidateEmptyTagsXml) {
  BucketTagsStr.assign(
      "<Tagging "
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\"><TagSet><Tag></Tag></"
      "TagSet></Tagging>");
  RequestId.assign("RequestId");

  put_bucket_tag_body =
      put_bucket_tag_body_factory->create_put_resource_tags_body(BucketTagsStr,
                                                                 RequestId);
  result = put_bucket_tag_body->isOK();
  EXPECT_FALSE(result);
}

TEST_F(S3PutTagBodyTest, ValidateEmptyKeyXml) {
  BucketTagsStr.assign(
      "<Tagging "
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\"><TagSet><Tag><Key></"
      "Key><Value>marketing123</Value></Tag></TagSet></Tagging>");
  RequestId.assign("RequestId");

  put_bucket_tag_body =
      put_bucket_tag_body_factory->create_put_resource_tags_body(BucketTagsStr,
                                                                 RequestId);
  result = put_bucket_tag_body->isOK();
  EXPECT_FALSE(result);
}

TEST_F(S3PutTagBodyTest, ValidateEmptyValueXml) {
  BucketTagsStr.assign(
      "<Tagging "
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/"
      "\"><TagSet><Tag><Key>organization124</Key><Value></Value></Tag></"
      "TagSet></Tagging>");
  RequestId.assign("RequestId");

  put_bucket_tag_body =
      put_bucket_tag_body_factory->create_put_resource_tags_body(BucketTagsStr,
                                                                 RequestId);
  result = put_bucket_tag_body->isOK();
  EXPECT_FALSE(result);
}

TEST_F(S3PutTagBodyTest, ValidateValidRequestTags) {
  BucketTagsStr.assign(
      "<Tagging "
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/"
      "\"><TagSet><Tag><Key>organization</Key><Value>seagate</Value></"
      "Tag><Tag><Key>employee</Key><Value>test</Value></Tag></TagSet></"
      "Tagging>");
  RequestId.assign("RequestId");
  bucket_tags_map["organization"] = "seagate";
  bucket_tags_map["employee"] = "test";

  put_bucket_tag_body =
      put_bucket_tag_body_factory->create_put_resource_tags_body(BucketTagsStr,
                                                                 RequestId);
  result = put_bucket_tag_body->validate_bucket_xml_tags(bucket_tags_map);
  EXPECT_TRUE(result);
}

TEST_F(S3PutTagBodyTest, ValidateRequestInvalidTagSize) {
  BucketTagsStr.assign(
      "<Tagging "
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/"
      "\"><TagSet><Tag><Key>"
      "dsxdgxponzcmadtlkwvztnxkczrzelewjvtvzpvitroleasdtlaphxyzkyzdjvpccukwbvgn"
      "lrdmwbiczaodlojquslufbkhafnuymridszpfvrbvrkaeexsxxozptlnbb</"
      "Key><Value>seagate</Value></Tag><Tag><Key>employee</Key><Value>test</"
      "Value></Tag></TagSet></Tagging>");
  RequestId.assign("RequestId");
  bucket_tags_map
      ["dsxdgxponzcmadtlkwvztnxkczrzelewjvtvzpvitroleasdtlaphxyzkyzdjvpccukwbvg"
       "nlrdmwbiczaodlojquslufbkhafnuymridszpfvrbvrkaeexsxxozptlnbb"] =
          "seagate";
  bucket_tags_map["employee"] = "test";

  put_bucket_tag_body =
      put_bucket_tag_body_factory->create_put_resource_tags_body(BucketTagsStr,
                                                                 RequestId);
  result = put_bucket_tag_body->validate_bucket_xml_tags(bucket_tags_map);
  EXPECT_FALSE(result);
}

TEST_F(S3PutTagBodyTest, ValidateRequestInvalidTagCount) {
  BucketTagsStr.assign(
      "<Tagging "
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/"
      "\"><TagSet><Tag><Key>organization1</Key><Value>seagate</Value></Tag></"
      "TagSet></Tagging>");
  RequestId.assign("RequestId");
  for (int max_tag_size = 0; max_tag_size < (BUCKET_MAX_TAGS + 1);
       max_tag_size++)
    bucket_tags_map["organization" + std::to_string(max_tag_size)] = "seagate";

  put_bucket_tag_body =
      put_bucket_tag_body_factory->create_put_resource_tags_body(BucketTagsStr,
                                                                 RequestId);
  result = put_bucket_tag_body->validate_bucket_xml_tags(bucket_tags_map);
  EXPECT_FALSE(result);
}

TEST_F(S3PutTagBodyTest, ValidateRequestBodyXmlReversedNodesOrder) {
  BucketTagsStr.assign(
      "<Tagging "
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/"
      "\"><TagSet><Tag><Value>vlv1</Value><Key>key1</Key></"
      "Tag><Tag><Key>key2</Key><Value>vlv2</Value></Tag></"
      "TagSet></Tagging>");
  RequestId.assign("RequestId");

  std::map<std::string, std::string> templ_to_cmp = {{"key1", "vlv1"},
                                                     {"key2", "vlv2"}};

  put_bucket_tag_body =
      put_bucket_tag_body_factory->create_put_resource_tags_body(BucketTagsStr,
                                                                 RequestId);
  result = put_bucket_tag_body->isOK();
  EXPECT_TRUE(result);

  auto parsed_tags_map = put_bucket_tag_body->get_resource_tags_as_map();
  EXPECT_EQ(templ_to_cmp.size(), parsed_tags_map.size());
  EXPECT_TRUE(std::equal(std::begin(templ_to_cmp), std::end(templ_to_cmp),
                         std::begin(parsed_tags_map)));
}
