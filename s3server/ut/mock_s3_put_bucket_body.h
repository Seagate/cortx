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
 * Original author:  Abrarahmed Momin   <abrar.habib@seagate.com>
 * Original creation date: April-13-2017
 */

#pragma once

#ifndef __S3_UT_MOCK_S3_PUT_BUCKET_BODY_H__
#define __S3_UT_MOCK_S3_PUT_BUCKET_BODY_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "s3_put_bucket_body.h"

using ::testing::_;
using ::testing::Return;

class MockS3PutBucketBody : public S3PutBucketBody {
 public:
  MockS3PutBucketBody(std::string& xml) : S3PutBucketBody(xml) {}
  MOCK_METHOD0(isOK, bool());
  MOCK_METHOD0(get_location_constraint, std::string());
};

#endif
