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
 * Original creation date: 27-March-2017
 */

#pragma once

#ifndef __S3_UT_MOCK_S3_CLOVIS_KVS_READER_H__
#define __S3_UT_MOCK_S3_CLOVIS_KVS_READER_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "s3_clovis_kvs_reader.h"
#include "s3_request_object.h"

using ::testing::_;
using ::testing::Return;

class MockS3ClovisKVSReader : public S3ClovisKVSReader {
 public:
  MockS3ClovisKVSReader(std::shared_ptr<RequestObject> req,
                        std::shared_ptr<ClovisAPI> s3clovis_api)
      : S3ClovisKVSReader(req, s3clovis_api) {}
  MOCK_METHOD0(get_state, S3ClovisKVSReaderOpState());
  MOCK_METHOD0(get_value, std::string());
  MOCK_METHOD0(get_key_values,
               std::map<std::string, std::pair<int, std::string>> &());
  MOCK_METHOD3(lookup_index,
               void(struct m0_uint128, std::function<void(void)> on_success,
                    std::function<void(void)> on_failed));
  MOCK_METHOD4(get_keyval,
               void(struct m0_uint128 oid, std::vector<std::string> keys,
                    std::function<void(void)> on_success,
                    std::function<void(void)> on_failed));
  MOCK_METHOD4(get_keyval, void(struct m0_uint128 oid, std::string key,
                                std::function<void(void)> on_success,
                                std::function<void(void)> on_failed));
  MOCK_METHOD6(next_keyval,
               void(struct m0_uint128 oid, std::string key, size_t nr_kvp,
                    std::function<void(void)> on_success,
                    std::function<void(void)> on_failed, unsigned int flag));
};

#endif
