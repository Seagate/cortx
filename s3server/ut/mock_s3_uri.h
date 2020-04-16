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

#ifndef __S3_UT_MOCK_S3_URI_H__
#define __S3_UT_MOCK_S3_URI_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "s3_uri.h"

class MockS3PathStyleURI : public S3PathStyleURI {
 public:
  MockS3PathStyleURI(std::shared_ptr<S3RequestObject> req)
      : S3PathStyleURI(req) {}
  MOCK_METHOD0(get_bucket_name, std::string&());
  MOCK_METHOD0(get_object_name, std::string&());
  MOCK_METHOD0(get_operation_code, S3OperationCode());
  MOCK_METHOD0(get_s3_api_type, S3ApiType());
};

class MockS3VirtualHostStyleURI : public S3VirtualHostStyleURI {
 public:
  MockS3VirtualHostStyleURI(std::shared_ptr<S3RequestObject> req)
      : S3VirtualHostStyleURI(req) {}
  MOCK_METHOD0(get_bucket_name, std::string&());
  MOCK_METHOD0(get_object_name, std::string&());
  MOCK_METHOD0(get_operation_code, S3OperationCode());
  MOCK_METHOD0(get_s3_api_type, S3ApiType());
};

class MockS3UriFactory : public S3UriFactory {
 public:
  MOCK_METHOD2(create_uri_object,
               S3URI*(S3UriType uri_type,
                      std::shared_ptr<S3RequestObject> request));
};
#endif
