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
 * Original creation date: 15-Feb-2017
 */

#pragma once

#ifndef __S3_UT_MOCK_S3_MEMORY_PROFILE_H__
#define __S3_UT_MOCK_S3_MEMORY_PROFILE_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "s3_memory_profile.h"

class MockS3MemoryProfile : public S3MemoryProfile {
 public:
  MockS3MemoryProfile() : S3MemoryProfile() {}
  MOCK_METHOD1(we_have_enough_memory_for_put_obj, bool(int layout_id));
  MOCK_METHOD0(free_memory_in_pool_above_threshold_limits, bool());
};

#endif
