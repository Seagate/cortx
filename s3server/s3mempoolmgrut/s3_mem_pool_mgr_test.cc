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
 * Original creation date: 14-June-2017
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"

extern "C" {
#include "mero/init.h"
#include "module/instance.h"
}

#include <evhtp.h>

#include "s3_mem_pool_manager.h"
#include "s3_option.h"
#include "s3_stats.h"
#include "s3_audit_info_logger.h"

#define FOUR_KB 4096
#define EIGHT_KB (2 * FOUR_KB)
#define TWELVE_KB (3 * FOUR_KB)
#define SIXTEEN_KB (4 * FOUR_KB)
#define TWENTYFOUR_KB (6 * FOUR_KB)

// Just to silent compiler
struct m0_uint128 global_bucket_list_index_oid;
struct m0_uint128 bucket_metadata_list_index_oid;
struct m0_uint128 global_probable_dead_object_list_index_oid;
struct m0_uint128 global_instance_id;
S3Option *g_option_instance = NULL;
evhtp_ssl_ctx_t *g_ssl_auth_ctx = NULL;
extern S3Stats *g_stats_instance;
evbase_t *global_evbase_handle;
extern int s3log_level;
int global_shutdown_in_progress;

static void _init_log() {
  s3log_level = S3_LOG_DEBUG;
  FLAGS_log_dir = "./";
  FLAGS_minloglevel = (s3log_level == S3_LOG_DEBUG) ? S3_LOG_INFO : s3log_level;
  google::InitGoogleLogging("s3mempoolmgrut");
}

static void _fini_log() {
  google::FlushLogFiles(google::GLOG_INFO);
  google::ShutdownGoogleLogging();
}

class S3MempoolManagerTestSuite : public testing::Test {
 protected:
  void SetUp() {
    // TODO
  }

  void Teardown() {
    // TODO
  }
};

TEST_F(S3MempoolManagerTestSuite, CreateAndDestroyPoolTest) {
  std::vector<int> unit_sizes{FOUR_KB, EIGHT_KB};

  EXPECT_EQ(0, S3MempoolManager::free_space);
  EXPECT_TRUE(S3MempoolManager::instance == NULL);

  int rc = S3MempoolManager::create_pool(SIXTEEN_KB,  // max threshold
                                         unit_sizes,  // supported unit_sizes
                                         1,   // initial_buffer_count_per_pool
                                         1);  // expandable_count
  EXPECT_EQ(0, rc);
  EXPECT_EQ(FOUR_KB, S3MempoolManager::free_space);
  EXPECT_EQ(EIGHT_KB,
            S3MempoolManager::get_instance()->get_free_space_for(FOUR_KB));
  EXPECT_EQ(TWELVE_KB,
            S3MempoolManager::get_instance()->get_free_space_for(EIGHT_KB));
  EXPECT_EQ(SIXTEEN_KB,
            S3MempoolManager::get_instance()->total_memory_threshold);
  EXPECT_EQ(2, S3MempoolManager::get_instance()->pool_of_mem_pool.size());

  S3MempoolManager::destroy_instance();
  EXPECT_EQ(0, S3MempoolManager::free_space);
  EXPECT_TRUE(S3MempoolManager::instance == NULL);
}

TEST_F(S3MempoolManagerTestSuite, CreateFailedTest) {
  std::vector<int> unit_sizes{FOUR_KB, EIGHT_KB};

  EXPECT_EQ(0, S3MempoolManager::free_space);
  EXPECT_TRUE(S3MempoolManager::instance == NULL);

  int rc = S3MempoolManager::create_pool(FOUR_KB,     // max threshold
                                         unit_sizes,  // supported unit_sizes
                                         1,   // initial_buffer_count_per_pool
                                         1);  // expandable_count

  EXPECT_NE(0, rc);
  EXPECT_EQ(0, S3MempoolManager::free_space);
  EXPECT_TRUE(S3MempoolManager::instance == NULL);
}

TEST_F(S3MempoolManagerTestSuite, CreateEXISTSTest) {
  std::vector<int> unit_sizes{FOUR_KB, EIGHT_KB};

  EXPECT_EQ(0, S3MempoolManager::free_space);
  EXPECT_TRUE(S3MempoolManager::instance == NULL);

  int rc = S3MempoolManager::create_pool(SIXTEEN_KB,  // max threshold
                                         unit_sizes,  // supported unit_sizes
                                         1,   // initial_buffer_count_per_pool
                                         1);  // expandable_count
  rc = S3MempoolManager::create_pool(SIXTEEN_KB,  // max threshold
                                     unit_sizes,  // supported unit_sizes
                                     1,   // initial_buffer_count_per_pool
                                     1);  // expandable_count

  EXPECT_EQ(EEXIST, rc);
  EXPECT_EQ(FOUR_KB, S3MempoolManager::free_space);
  EXPECT_EQ(EIGHT_KB,
            S3MempoolManager::get_instance()->get_free_space_for(FOUR_KB));
  EXPECT_EQ(TWELVE_KB,
            S3MempoolManager::get_instance()->get_free_space_for(EIGHT_KB));
  EXPECT_EQ(SIXTEEN_KB,
            S3MempoolManager::get_instance()->total_memory_threshold);
  EXPECT_EQ(2, S3MempoolManager::get_instance()->pool_of_mem_pool.size());

  S3MempoolManager::destroy_instance();
  EXPECT_EQ(0, S3MempoolManager::free_space);
  EXPECT_TRUE(S3MempoolManager::instance == NULL);
}

TEST_F(S3MempoolManagerTestSuite, PoolShouldGrowAndErrorOnMax) {
  std::vector<int> unit_sizes{FOUR_KB, EIGHT_KB};

  EXPECT_EQ(0, S3MempoolManager::free_space);
  EXPECT_TRUE(S3MempoolManager::instance == NULL);

  int rc = S3MempoolManager::create_pool(SIXTEEN_KB,  // max threshold
                                         unit_sizes,  // supported unit_sizes
                                         1,   // initial_buffer_count_per_pool
                                         1);  // expandable_count
  EXPECT_EQ(0, rc);
  EXPECT_EQ(FOUR_KB, S3MempoolManager::free_space);

  // Get buffer leading to expansion of pool
  void *buffer_1 =
      S3MempoolManager::get_instance()->get_buffer_for_unit_size(FOUR_KB);
  EXPECT_TRUE(buffer_1 != NULL);
  void *buffer_2 =
      S3MempoolManager::get_instance()->get_buffer_for_unit_size(EIGHT_KB);
  EXPECT_TRUE(buffer_2 != NULL);

  // Grow 4k pool
  void *buffer_3 =
      S3MempoolManager::get_instance()->get_buffer_for_unit_size(FOUR_KB);
  EXPECT_TRUE(buffer_3 != NULL);

  // Max out mem
  void *buffer_4 =
      S3MempoolManager::get_instance()->get_buffer_for_unit_size(FOUR_KB);
  EXPECT_TRUE(buffer_4 == NULL);

  // Release back to pool
  EXPECT_EQ(0, S3MempoolManager::get_instance()->release_buffer_for_unit_size(
                   buffer_1, FOUR_KB));
  EXPECT_EQ(0, S3MempoolManager::get_instance()->release_buffer_for_unit_size(
                   buffer_2, EIGHT_KB));
  EXPECT_EQ(0, S3MempoolManager::get_instance()->release_buffer_for_unit_size(
                   buffer_3, FOUR_KB));

  S3MempoolManager::destroy_instance();
  EXPECT_EQ(0, S3MempoolManager::free_space);
  EXPECT_TRUE(S3MempoolManager::instance == NULL);
}

TEST_F(S3MempoolManagerTestSuite, PoolShouldGrowByDownsizingUnusedPool) {
  std::vector<int> unit_sizes{FOUR_KB, EIGHT_KB};

  EXPECT_EQ(0, S3MempoolManager::free_space);
  EXPECT_TRUE(S3MempoolManager::instance == NULL);

  int rc = S3MempoolManager::create_pool(SIXTEEN_KB,  // max threshold
                                         unit_sizes,  // supported unit_sizes
                                         1,   // initial_buffer_count_per_pool
                                         1);  // expandable_count
  EXPECT_EQ(0, rc);
  EXPECT_EQ(FOUR_KB, S3MempoolManager::free_space);

  // Get buffer leading to expansion of pool
  void *buffer_1 =
      S3MempoolManager::get_instance()->get_buffer_for_unit_size(FOUR_KB);
  EXPECT_TRUE(buffer_1 != NULL);
  void *buffer_2 =
      S3MempoolManager::get_instance()->get_buffer_for_unit_size(FOUR_KB);
  EXPECT_TRUE(buffer_2 != NULL);

  // Grow 4k pool, this should free EIGHT_KB pool
  void *buffer_3 =
      S3MempoolManager::get_instance()->get_buffer_for_unit_size(FOUR_KB);
  EXPECT_TRUE(buffer_3 != NULL);

  // EIGHT_KB cannot allocate
  void *buffer_4 =
      S3MempoolManager::get_instance()->get_buffer_for_unit_size(EIGHT_KB);
  EXPECT_TRUE(buffer_4 == NULL);

  // Release back to pool
  EXPECT_EQ(0, S3MempoolManager::get_instance()->release_buffer_for_unit_size(
                   buffer_1, FOUR_KB));
  EXPECT_EQ(0, S3MempoolManager::get_instance()->release_buffer_for_unit_size(
                   buffer_2, FOUR_KB));
  EXPECT_EQ(0, S3MempoolManager::get_instance()->release_buffer_for_unit_size(
                   buffer_3, FOUR_KB));

  S3MempoolManager::destroy_instance();
  EXPECT_EQ(0, S3MempoolManager::free_space);
  EXPECT_TRUE(S3MempoolManager::instance == NULL);
}

int main(int argc, char **argv) {
  int rc = 0;

  _init_log();

  ::testing::InitGoogleTest(&argc, argv);
  ::testing::InitGoogleMock(&argc, argv);

  if (S3AuditInfoLogger::init() != 0) {
    s3_log(S3_LOG_FATAL, "", "Couldn't init audit logger!");
    return -1;
  }

  rc = RUN_ALL_TESTS();

  _fini_log();
  S3AuditInfoLogger::finalize();

  return rc;
}
