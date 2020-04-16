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

#ifndef __S3_UT_MOCK_S3_OBJECT_METADATA_H__
#define __S3_UT_MOCK_S3_OBJECT_METADATA_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mock_s3_clovis_wrapper.h"
#include "s3_object_metadata.h"
#include "s3_request_object.h"

using ::testing::_;
using ::testing::Return;

class MockS3ObjectMetadata : public S3ObjectMetadata {
 public:
  MockS3ObjectMetadata(std::shared_ptr<S3RequestObject> req,
                       std::shared_ptr<MockS3Clovis> clovis_api = nullptr)
      : S3ObjectMetadata(req, false, "", nullptr, nullptr, clovis_api) {}
  MOCK_METHOD0(get_state, S3ObjectMetadataState());
  MOCK_METHOD0(get_version_key_in_index, std::string());
  MOCK_METHOD0(get_oid, struct m0_uint128());
  MOCK_METHOD0(get_layout_id, int());
  MOCK_METHOD1(set_oid, void(struct m0_uint128));
  MOCK_METHOD1(set_md5, void(std::string));
  MOCK_METHOD0(reset_date_time_to_current, void());
  MOCK_METHOD0(get_md5, std::string());
  MOCK_METHOD0(get_object_name, std::string());
  MOCK_METHOD0(get_content_length, size_t());
  MOCK_METHOD1(set_content_length, void(std::string length));
  MOCK_METHOD0(get_content_length_str, std::string());
  MOCK_METHOD0(get_last_modified_gmt, std::string());
  MOCK_METHOD0(get_user_id, std::string());
  MOCK_METHOD0(get_user_name, std::string());
  MOCK_METHOD0(get_account_name, std::string());
  MOCK_METHOD0(get_canonical_id, std::string());
  MOCK_METHOD0(get_last_modified_iso, std::string());
  MOCK_METHOD0(get_storage_class, std::string());
  MOCK_METHOD0(get_upload_id, std::string());
  MOCK_METHOD0(check_object_tags_exists, bool());
  MOCK_METHOD0(delete_object_tags, void());
  MOCK_METHOD2(add_user_defined_attribute,
               void(std::string key, std::string val));
  MOCK_METHOD2(load, void(std::function<void(void)> on_success,
                          std::function<void(void)> on_failed));
  MOCK_METHOD2(save, void(std::function<void(void)> on_success,
                          std::function<void(void)> on_failed));
  MOCK_METHOD2(remove, void(std::function<void(void)> on_success,
                            std::function<void(void)> on_failed));
  MOCK_METHOD1(setacl, void(const std::string&));
  MOCK_METHOD1(set_tags,
               void(const std::map<std::string, std::string>& tags_as_map));
  MOCK_METHOD2(save_metadata, void(std::function<void(void)> on_success,
                                   std::function<void(void)> on_failed));
  MOCK_METHOD1(from_json, int(std::string content));
};

#endif

