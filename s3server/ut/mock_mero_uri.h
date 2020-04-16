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
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 17-JULY-2019
 */

#pragma once

#ifndef __S3_UT_MOCK_MERO_URI_H__
#define __S3_UT_MOCK_MERO_URI_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mero_uri.h"

class MockMeroPathStyleURI : public MeroPathStyleURI {
 public:
  MockMeroPathStyleURI(std::shared_ptr<MeroRequestObject> req)
      : MeroPathStyleURI(req) {}
  MOCK_METHOD0(get_key_name, std::string&());
  MOCK_METHOD0(get_object_oid_lo, std::string&());
  MOCK_METHOD0(get_object_oid_ho, std::string&());
  MOCK_METHOD0(get_index_id_lo, std::string&());
  MOCK_METHOD0(get_index_id_hi, std::string&());
  MOCK_METHOD0(get_operation_code, MeroOperationCode());
  MOCK_METHOD0(get_mero_api_type, MeroApiType());
};

class MockMeroUriFactory : public MeroUriFactory {
 public:
  MOCK_METHOD2(create_uri_object,
               MeroURI*(MeroUriType uri_type,
                        std::shared_ptr<MeroRequestObject> request));
};
#endif
