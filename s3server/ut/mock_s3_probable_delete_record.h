/*
 * COPYRIGHT 2020 SEAGATE LLC
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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 19-Mar-2020
 */

#pragma once

#ifndef __S3_UT_MOCK_S3_PROBABLE_DELETE_RECORD_H__
#define __S3_UT_MOCK_S3_PROBABLE_DELETE_RECORD_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "s3_probable_delete_record.h"

using ::testing::_;
using ::testing::Return;

class MockS3ProbableDeleteRecord : public S3ProbableDeleteRecord {
 public:
  MockS3ProbableDeleteRecord(std::string rec_key, struct m0_uint128 old_oid,
                             std::string obj_key_in_index,
                             struct m0_uint128 new_oid, int layout_id,
                             struct m0_uint128 obj_list_idx_oid,
                             struct m0_uint128 objs_version_list_idx_oid,
                             std::string ver_key_in_index,
                             bool force_del = false, bool is_multipart = false,
                             struct m0_uint128 part_list_oid = {0ULL, 0ULL})
      : S3ProbableDeleteRecord(rec_key, old_oid, obj_key_in_index, new_oid,
                               layout_id, obj_list_idx_oid,
                               objs_version_list_idx_oid, ver_key_in_index,
                               force_del, is_multipart, part_list_oid) {}
  MOCK_METHOD0(get_state, S3ObjectMetadataState());
  MOCK_METHOD0(get_key, const std::string&());
  MOCK_METHOD0(get_current_object_oid, struct m0_uint128());
  MOCK_METHOD1(set_force_delete, void(bool));
  MOCK_METHOD0(to_json, std::string());
};

#endif
