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
 * Original author:  Siddhivinayak Shanbhag <siddhivinayak.shanbhag@seagate.com>
 * Original creation date: 09-January-2019
 */

#pragma once

#ifndef __S3_UT_MOCK_S3_PUT_TAG_BODY_H__
#define __S3_UT_MOCK_S3_PUT_TAG_BODY_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "s3_put_tag_body.h"

using ::testing::_;
using ::testing::Return;

class MockS3PutTagBody : public S3PutTagBody {
 public:
  MockS3PutTagBody(std::string& xml, std::string& request_id)
      : S3PutTagBody(xml, request_id) {}
  MOCK_METHOD0(isOK, bool());
  MOCK_METHOD0(get_resource_tags_as_map, std::map<std::string, std::string>&());
  MOCK_METHOD1(validate_bucket_xml_tags,
               bool(std::map<std::string, std::string>& tags_str));
};

#endif
