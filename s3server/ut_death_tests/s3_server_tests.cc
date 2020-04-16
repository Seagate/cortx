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
 * Original creation date: 1-Oct-2015
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "clovis_helpers.h"
#include "s3_clovis_layout.h"
#include "s3_error_messages.h"
#include "s3_log.h"
#include "s3_mem_pool_manager.h"
#include "s3_option.h"
#include "s3_stats.h"
#include "s3_audit_info_logger.h"

// Some declarations from s3server that are required to get compiled.
// TODO - Remove such globals by implementing config file.
// S3 Auth service
const char *auth_ip_addr = "127.0.0.1";
uint16_t auth_port = 8095;
extern int s3log_level;
struct m0_uint128 global_bucket_list_index_oid;
struct m0_uint128 bucket_metadata_list_index_oid;
struct m0_uint128 global_probable_dead_object_list_index_oid;
struct m0_uint128 global_instance_id;
S3Option *g_option_instance = NULL;
evhtp_ssl_ctx_t *g_ssl_auth_ctx = NULL;
extern S3Stats *g_stats_instance;
evbase_t *global_evbase_handle = nullptr;
int global_shutdown_in_progress;

static void _init_log() {
  s3log_level = S3_LOG_FATAL;
  FLAGS_log_dir = "./";
  FLAGS_minloglevel = (s3log_level == S3_LOG_DEBUG) ? S3_LOG_INFO : s3log_level;
  google::InitGoogleLogging("s3ut");
}

static void _fini_log() {
  google::FlushLogFiles(google::GLOG_INFO);
  google::ShutdownGoogleLogging();
}

int main(int argc, char **argv) {
  int rc;

  g_option_instance = S3Option::get_instance();
  g_stats_instance = S3Stats::get_instance();
  _init_log();

  ::testing::InitGoogleTest(&argc, argv);
  ::testing::InitGoogleMock(&argc, argv);

  if (S3AuditInfoLogger::init() != 0) {
    s3_log(S3_LOG_FATAL, "", "Couldn't init audit logger!");
    return -1;
  }

  S3ErrorMessages::init_messages("resources/s3_error_messages.json");

  size_t libevent_pool_buffer_size =
      g_option_instance->get_libevent_pool_buffer_size();
  event_use_mempool(libevent_pool_buffer_size, libevent_pool_buffer_size * 100,
                    libevent_pool_buffer_size * 100,
                    libevent_pool_buffer_size * 500, CREATE_ALIGNED_MEMORY);

  rc = S3MempoolManager::create_pool(
      g_option_instance->get_clovis_read_pool_max_threshold(),
      g_option_instance->get_clovis_unit_sizes_for_mem_pool(),
      g_option_instance->get_clovis_read_pool_initial_buffer_count(),
      g_option_instance->get_clovis_read_pool_expandable_count());

  rc = RUN_ALL_TESTS();

  S3MempoolManager::destroy_instance();

  _fini_log();
  S3AuditInfoLogger::finalize();

  if (g_stats_instance) {
    S3Stats::delete_instance();
  }
  if (g_option_instance) {
    S3Option::destroy_instance();
  }

  return rc;
}
