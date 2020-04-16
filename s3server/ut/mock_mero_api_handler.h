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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 17-JULY-2019
 */

#pragma once

#ifndef __S3_UT_MOCK_MERO_API_HANDLER_H__
#define __S3_UT_MOCK_MERO_API_HANDLER_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mero_api_handler.h"

class MockMeroIndexAPIHandler : public MeroIndexAPIHandler {
 public:
  MockMeroIndexAPIHandler(std::shared_ptr<MeroRequestObject> req,
                          MeroOperationCode op_code)
      : MeroIndexAPIHandler(req, op_code) {}
  MOCK_METHOD0(dispatch, void());
};

class MockMeroKeyValueAPIHandler : public MeroKeyValueAPIHandler {
 public:
  MockMeroKeyValueAPIHandler(std::shared_ptr<MeroRequestObject> req,
                             MeroOperationCode op_code)
      : MeroKeyValueAPIHandler(req, op_code) {}
  MOCK_METHOD0(dispatch, void());
};

class MockMeroObjectAPIHandler : public MeroObjectAPIHandler {
 public:
  MockMeroObjectAPIHandler(std::shared_ptr<MeroRequestObject> req,
                           MeroOperationCode op_code)
      : MeroObjectAPIHandler(req, op_code) {}
  MOCK_METHOD0(dispatch, void());
};

class MockMeroAPIHandlerFactory : public MeroAPIHandlerFactory {
 public:
  MOCK_METHOD3(create_api_handler,
               std::shared_ptr<MeroAPIHandler>(
                   MeroApiType api_type,
                   std::shared_ptr<MeroRequestObject> request,
                   MeroOperationCode op_code));
};

#endif
