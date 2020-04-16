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
#include "s3_perf_metrics.h"
#include "s3_option.h"

extern S3Option* g_option_instance;

class S3PerfMetricsTest : public testing::Test {
 protected:
  evbase_t *evbase;

 public:
  virtual void SetUp() {
    evbase = event_base_new();
    g_option_instance = S3Option::get_instance();
    g_option_instance->set_stats_enable(true);
  }
  void TearDown() {
    g_option_instance->set_stats_enable(false);
    if (evbase) {
      event_base_free(evbase);
      evbase = nullptr;
    }
  }
};

TEST_F(S3PerfMetricsTest, TestInitSuccess) {
  int rc;
  int count_orig;
  // Make sure it is adding event on success.
  count_orig = event_base_get_num_events(evbase, EVENT_BASE_COUNT_ADDED);
  EXPECT_EQ(0, count_orig);

  rc = s3_perf_metrics_init(evbase);
  EXPECT_EQ(0, rc);
  EXPECT_EQ(count_orig + 1,
            event_base_get_num_events(evbase, EVENT_BASE_COUNT_ADDED));

  // Make sure it is deleted on _fini.
  s3_perf_metrics_fini();
  EXPECT_EQ(count_orig,
            event_base_get_num_events(evbase, EVENT_BASE_COUNT_ADDED));
}

TEST_F(S3PerfMetricsTest, TestInitFail) {
  int rc;
  int count_orig;

  // Make sure no event is added on fail.
  count_orig = event_base_get_num_events(evbase, EVENT_BASE_COUNT_ADDED);
  EXPECT_EQ(0, count_orig);

  rc = s3_perf_metrics_init(nullptr);
  EXPECT_EQ(-EINVAL, rc);
  EXPECT_EQ(count_orig,
            event_base_get_num_events(evbase, EVENT_BASE_COUNT_ADDED));

  // Make sure no event is removed on _fini if _init failed.
  s3_perf_metrics_fini();
  EXPECT_EQ(count_orig,
            event_base_get_num_events(evbase, EVENT_BASE_COUNT_ADDED));
}
