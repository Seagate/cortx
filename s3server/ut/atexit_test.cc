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
 * Original author:  Ivan Tishchenko     <ivan.tishchenko@seagate.com>
 * Original creation date: 26-Dec-2019
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "atexit.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class AtExitTest : public testing::Test {
 public:
  virtual void SetUp() {}
  void TearDown() {}
};

TEST_F(AtExitTest, BasicTest) {
  int call_count;

  call_count = 0;
  {  // Standard case.
    AtExit tst([&]() { call_count++; });
  }
  EXPECT_EQ(1, call_count);

  call_count = 0;
  {  // Cancel the call.
    AtExit tst([&]() { call_count++; });
    tst.cancel();
  }
  EXPECT_EQ(0, call_count);

  call_count = 0;
  {  // Cancel the call, then re-enable.
    AtExit tst([&]() { call_count++; });
    tst.cancel();
    tst.reenable();
  }
  EXPECT_EQ(1, call_count);

  call_count = 0;
  {  // Force deinit.
    AtExit tst([&]() { call_count++; });
    tst.call_now();
    EXPECT_EQ(1, call_count);
  }
  // make sure it's only called once
  EXPECT_EQ(1, call_count);

  call_count = 0;
  {  // Force deinit does not work when cancelled.
    AtExit tst([&]() { call_count++; });
    tst.cancel();
    tst.call_now();
    EXPECT_EQ(0, call_count);
  }
  // make sure it's not called in destructor
  EXPECT_EQ(0, call_count);
}
