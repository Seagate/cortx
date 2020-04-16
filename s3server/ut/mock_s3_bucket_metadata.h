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
 * Original creation date: 08-Feb-2017
 */

#pragma once

#ifndef __S3_UT_MOCK_S3_BUCKET_METADATA_H__
#define __S3_UT_MOCK_S3_BUCKET_METADATA_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mock_s3_clovis_wrapper.h"
#include "s3_bucket_metadata_v1.h"
#include "s3_request_object.h"

using ::testing::_;
using ::testing::Return;

class MockS3BucketMetadata : public S3BucketMetadataV1 {
 public:
  MockS3BucketMetadata(std::shared_ptr<S3RequestObject> req,
                       std::shared_ptr<MockS3Clovis> s3_mock_clovis_api)
      : S3BucketMetadataV1(req, s3_mock_clovis_api) {}
  MOCK_METHOD2(load, void(std::function<void(void)> on_success,
                          std::function<void(void)> on_failed));
  MOCK_METHOD0(get_state, S3BucketMetadataState());
  MOCK_METHOD0(get_policy_as_json, std::string &());
  MOCK_METHOD0(get_acl_as_xml, std::string());
  MOCK_METHOD1(setpolicy, void(std::string& policy_str));
  MOCK_METHOD1(set_tags,
               void(const std::map<std::string, std::string>& tags_str));
  MOCK_METHOD1(setacl, void(const std::string& acl_str));
  MOCK_METHOD2(save, void(std::function<void(void)> on_success,
                          std::function<void(void)> on_failed));
  MOCK_METHOD2(remove, void(std::function<void(void)> on_success,
                            std::function<void(void)> on_failed));
  MOCK_METHOD0(deletepolicy, void());
  MOCK_METHOD0(delete_bucket_tags, void());
  MOCK_METHOD1(set_location_constraint, void(std::string location));
  MOCK_METHOD1(from_json, int(std::string content));
};

#endif
