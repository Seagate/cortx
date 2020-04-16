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

#include <glog/logging.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

extern "C" {
#include "clovis/clovis.h"
#include "lib/uuid.h"  // for m0_node_uuid_string_set();
#include "mero/init.h"
#include "module/instance.h"
}

#include "clovis_helpers.h"
#include "s3_clovis_layout.h"
#include "s3_error_messages.h"
#include "s3_log.h"
#include "s3_mem_pool_manager.h"
#include "s3_option.h"
#include "s3_stats.h"

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
evhtp_ssl_ctx_t *g_ssl_auth_ctx;
extern S3Stats *g_stats_instance;

int global_shutdown_in_progress;

struct m0 instance;

static void _init_log() {
  s3log_level = S3_LOG_FATAL;
  FLAGS_log_dir = "./";

  switch (s3log_level) {
    case S3_LOG_WARN:
      FLAGS_minloglevel = google::GLOG_WARNING;
      break;
    case S3_LOG_ERROR:
      FLAGS_minloglevel = google::GLOG_ERROR;
      break;
    case S3_LOG_FATAL:
      FLAGS_minloglevel = google::GLOG_FATAL;
      break;
    default:
      FLAGS_minloglevel = google::GLOG_INFO;
  }
  google::InitGoogleLogging("s3ut");
}

static void _fini_log() {
  google::FlushLogFiles(google::GLOG_INFO);
  google::ShutdownGoogleLogging();
}

static int _init_option_and_instance() {
  g_option_instance = S3Option::get_instance();
  g_option_instance->set_option_file("s3config-test.yaml");
  bool force_override_from_config = true;
  if (!g_option_instance->load_all_sections(force_override_from_config)) {
    return -1;
  }

  g_option_instance->set_stats_whitelist_filename(
      "s3stats-whitelist-test.yaml");
  g_stats_instance = S3Stats::get_instance();
  g_option_instance->dump_options();
  S3ClovisLayoutMap::get_instance()->load_layout_recommendations(
      g_option_instance->get_layout_recommendation_file());

  return 0;
}

static void _cleanup_option_and_instance() {
  if (g_stats_instance) {
    S3Stats::delete_instance();
  }
  if (g_option_instance) {
    S3Option::destroy_instance();
  }
  S3ClovisLayoutMap::destroy_instance();
}

static int clovis_ut_init() {
  int rc;
  // Mero lib initialization routines
  m0_node_uuid_string_set(NULL);
  rc = m0_init(&instance);
  if (rc != 0) {
    EXPECT_EQ(rc, 0) << "Mero lib initialization failed";
  }
  return rc;
}

static void clovis_ut_fini() {
  // Mero lib cleanup
  m0_fini();
}

static int mempool_init() {
  int rc;

  size_t libevent_pool_buffer_size =
      g_option_instance->get_libevent_pool_buffer_size();
  rc = event_use_mempool(
      libevent_pool_buffer_size, libevent_pool_buffer_size * 100,
      libevent_pool_buffer_size * 100, libevent_pool_buffer_size * 1000,
      CREATE_ALIGNED_MEMORY);
  if (rc != 0) return rc;

  rc = S3MempoolManager::create_pool(
      g_option_instance->get_clovis_read_pool_max_threshold(),
      g_option_instance->get_clovis_unit_sizes_for_mem_pool(),
      g_option_instance->get_clovis_read_pool_initial_buffer_count(),
      g_option_instance->get_clovis_read_pool_expandable_count());

  return rc;
}

static void mempool_fini() {
  S3MempoolManager::destroy_instance();
  event_destroy_mempool();
}

static bool init_auth_ssl() {
  const char *cert_file = g_option_instance->get_iam_cert_file();
  SSL_library_init();
  ERR_load_crypto_strings();
  SSL_load_error_strings();
  g_ssl_auth_ctx = SSL_CTX_new(SSLv23_method());
  if (!SSL_CTX_load_verify_locations(g_ssl_auth_ctx, cert_file, NULL)) {
    s3_log(S3_LOG_ERROR, "", "SSL_CTX_load_verify_locations\n");
    return false;
  }
  SSL_CTX_set_verify(g_ssl_auth_ctx, SSL_VERIFY_PEER, NULL);
  SSL_CTX_set_verify_depth(g_ssl_auth_ctx, 1);
  return true;
}

int main(int argc, char **argv) {
  int rc;

  _init_log();
  rc = _init_option_and_instance();
  if (rc != 0) {
    _cleanup_option_and_instance();
    _fini_log();
    return rc;
  }

  ::testing::InitGoogleTest(&argc, argv);
  ::testing::InitGoogleMock(&argc, argv);

  S3ErrorMessages::init_messages("resources/s3_error_messages.json");

  // Clovis Initialization
  rc = clovis_ut_init();
  if (rc != 0) {
    _cleanup_option_and_instance();
    _fini_log();
    return rc;
  }

  // Mempool Initialization
  rc = mempool_init();
  if (rc != 0) {
    clovis_ut_fini();
    _cleanup_option_and_instance();
    _fini_log();
    return rc;
  }

  // SSL initialization
  if (init_auth_ssl() != true) {
    mempool_fini();
    clovis_ut_fini();
    _cleanup_option_and_instance();
    _fini_log();
    return rc;
  }

  rc = RUN_ALL_TESTS();

  mempool_fini();
  clovis_ut_fini();
  _cleanup_option_and_instance();
  _fini_log();

  return rc;
}
