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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 17-Nov-2015
 */

#pragma once

#ifndef __S3_UT_MOCK_S3_API_HANDLER_H__
#define __S3_UT_MOCK_S3_API_HANDLER_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "s3_api_handler.h"

class MockS3ServiceAPIHandler : public S3ServiceAPIHandler {
 public:
  MockS3ServiceAPIHandler(std::shared_ptr<S3RequestObject> req,
                          S3OperationCode op_code)
      : S3ServiceAPIHandler(req, op_code) {}
  MOCK_METHOD0(dispatch, void());
};

class MockS3BucketAPIHandler : public S3BucketAPIHandler {
 public:
  MockS3BucketAPIHandler(std::shared_ptr<S3RequestObject> req,
                         S3OperationCode op_code)
      : S3BucketAPIHandler(req, op_code) {}
  MOCK_METHOD0(dispatch, void());
};

class MockS3ObjectAPIHandler : public S3ObjectAPIHandler {
 public:
  MockS3ObjectAPIHandler(std::shared_ptr<S3RequestObject> req,
                         S3OperationCode op_code)
      : S3ObjectAPIHandler(req, op_code) {}
  MOCK_METHOD0(dispatch, void());
};

class MockS3APIHandlerFactory : public S3APIHandlerFactory {
 public:
  MOCK_METHOD3(create_api_handler,
               std::shared_ptr<S3APIHandler>(
                   S3ApiType api_type, std::shared_ptr<S3RequestObject> request,
                   S3OperationCode op_code));
};

#endif
