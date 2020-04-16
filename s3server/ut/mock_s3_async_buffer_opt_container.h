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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 30-Mar-2017
 */

#pragma once

#ifndef __S3_UT_MOCK_S3_ASYNC_BUFFER_OPT_H__
#define __S3_UT_MOCK_S3_ASYNC_BUFFER_OPT_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "s3_async_buffer_opt.h"

using ::testing::_;
using ::testing::Return;

class MockS3AsyncBufferOptContainer : public S3AsyncBufferOptContainer {
 public:
  MockS3AsyncBufferOptContainer(size_t size_of_each_buf)
      : S3AsyncBufferOptContainer(size_of_each_buf) {}
  MOCK_METHOD0(is_freezed, bool());
  MOCK_METHOD0(get_content_length, size_t());
};

#endif
