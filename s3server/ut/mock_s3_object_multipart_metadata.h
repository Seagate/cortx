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

#ifndef __S3_UT_MOCK_S3_OBJECT_MULTIPART_METADATA_H__
#define __S3_UT_MOCK_S3_OBJECT_MULTIPART_METADATA_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_request_object.h"
#include "s3_object_metadata.h"

using ::testing::_;
using ::testing::Return;

class MockS3ObjectMultipartMetadata : public S3ObjectMetadata {
 public:
  MockS3ObjectMultipartMetadata(std::shared_ptr<S3RequestObject> req,
                                std::shared_ptr<MockS3Clovis> clovis_api,
                                std::string upload_id)
      : S3ObjectMetadata(req, true, upload_id, nullptr, nullptr, clovis_api) {}
  MOCK_METHOD0(get_state, S3ObjectMetadataState());
  MOCK_METHOD0(get_old_oid, struct m0_uint128());
  MOCK_METHOD0(get_oid, struct m0_uint128());
  MOCK_METHOD0(get_old_layout_id, int());
  MOCK_METHOD0(get_upload_id, std::string());
  MOCK_METHOD0(get_part_one_size, size_t());
  MOCK_METHOD1(set_part_one_size, void(size_t part_size));
  MOCK_METHOD0(get_layout_id, int());
  MOCK_METHOD2(load, void(std::function<void(void)> on_success,
                          std::function<void(void)> on_failed));
  MOCK_METHOD2(save, void(std::function<void(void)> on_success,
                          std::function<void(void)> on_failed));
  MOCK_METHOD2(remove, void(std::function<void(void)> on_success,
                            std::function<void(void)> on_failed));
  MOCK_METHOD0(get_user_attributes, std::map<std::string, std::string>&());
};

#endif
