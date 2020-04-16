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

#ifndef __S3_UT_MOCK_S3_CLOVIS_WRITER_H__
#define __S3_UT_MOCK_S3_CLOVIS_WRITER_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_clovis_wrapper.h"
#include "s3_clovis_writer.h"
#include "s3_request_object.h"

using ::testing::_;
using ::testing::Return;

class MockS3ClovisWriter : public S3ClovisWriter {
 public:
  MockS3ClovisWriter(std::shared_ptr<RequestObject> req, struct m0_uint128 oid,
                     std::shared_ptr<MockS3Clovis> s3_clovis_mock_ptr)
      : S3ClovisWriter(req, oid, 0, s3_clovis_mock_ptr) {}
  MockS3ClovisWriter(std::shared_ptr<RequestObject> req,
                     std::shared_ptr<MockS3Clovis> s3_clovis_mock_ptr)
      : S3ClovisWriter(req, 0, s3_clovis_mock_ptr) {}

  MOCK_METHOD0(get_state, S3ClovisWriterOpState());
  MOCK_METHOD0(get_oid, struct m0_uint128());
  MOCK_METHOD0(get_content_md5, std::string());
  MOCK_METHOD1(get_op_ret_code_for, int(int));
  MOCK_METHOD1(get_op_ret_code_for_delete_op, int(int));
  MOCK_METHOD3(create_object,
               void(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed, int layout_id));
  MOCK_METHOD3(delete_object,
               void(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed, int layout_id));
  MOCK_METHOD3(delete_index,
               void(struct m0_uint128 oid, std::function<void(void)> on_success,
                    std::function<void(void)> on_failed));
  MOCK_METHOD4(delete_objects, void(std::vector<struct m0_uint128> oids,
                                    std::vector<int> layoutids,
                                    std::function<void(void)> on_success,
                                    std::function<void(void)> on_failed));
  MOCK_METHOD1(set_oid, void(struct m0_uint128 oid));
  MOCK_METHOD3(write_content,
               void(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed,
                    std::shared_ptr<S3AsyncBufferOptContainer> buffer));
};

#endif
