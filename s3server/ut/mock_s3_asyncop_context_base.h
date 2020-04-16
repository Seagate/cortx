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
 * Original author:  Rajesh Nambiar <rajesh.nambiar@seagate.com>
 * Original creation date: 19-Nov-2015
 */

#pragma once

#ifndef __S3_UT_MOCK_S3_ASYNCOP_CONTEXT_BASE_H__
#define __S3_UT_MOCK_S3_ASYNCOP_CONTEXT_BASE_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mock_s3_clovis_wrapper.h"
#include "s3_asyncop_context_base.h"
#include "s3_common.h"

class MockS3AsyncOpContextBase : public S3AsyncOpContextBase {
 public:
  MockS3AsyncOpContextBase(std::shared_ptr<S3RequestObject> req,
                           std::function<void(void)> success_callback,
                           std::function<void(void)> failed_callback,
                           std::shared_ptr<ClovisAPI> clovis_api = nullptr)
      // Pass default opcount value of 1 explicitly.
      : S3AsyncOpContextBase(req, success_callback, failed_callback, 1,
                             clovis_api) {}
  MOCK_METHOD2(set_op_errno_for, void(int op_idx, int err));
  MOCK_METHOD3(set_op_status_for,
               void(int op_idx, S3AsyncOpStatus opstatus, std::string message));
};

#endif
