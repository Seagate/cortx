/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original author:  Rajesh Nambiar <rajesh.nambiar@seagate.com>
 * Original creation date: 30-March-2016
 */

#include "s3_option.h"
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include "gtest/gtest.h"

extern S3Option *g_option_instance;

class S3OptionsTest : public testing::Test {
 protected:
  S3OptionsTest() {
    instance = S3Option::get_instance();
    instance->set_option_file("s3config-test.yaml");
  }

  ~S3OptionsTest() { S3Option::destroy_instance(); }

  static void TearDownTestCase() {
    // Called after the last test fixture in this file.
    // This is to ensure rest all tests cases in other test files have a
    // good S3option instance as most server/* code is using global ptr.
    g_option_instance = S3Option::get_instance();
    g_option_instance->set_stats_whitelist_filename(
        "s3stats-whitelist-test.yaml");
  }

  S3Option *instance;
};

TEST_F(S3OptionsTest, Constructor) {
  EXPECT_STREQ("/var/log/seagate/s3", instance->get_log_dir().c_str());
  EXPECT_STREQ("INFO", instance->get_log_level().c_str());
  EXPECT_STREQ("/etc/ssl/stx-s3/s3/ca.crt", instance->get_iam_cert_file());
  EXPECT_STREQ("10.10.1.1", instance->get_ipv4_bind_addr().c_str());
  EXPECT_STREQ("", instance->get_ipv6_bind_addr().c_str());
  EXPECT_STREQ("localhost@tcp:12345:33:100",
               instance->get_clovis_local_addr().c_str());
  EXPECT_STREQ("<0x7000000000000001:0>", instance->get_clovis_prof().c_str());
  EXPECT_STREQ("ipv4:10.10.1.2", instance->get_auth_ip_addr().c_str());
  EXPECT_EQ(9081, instance->get_s3_bind_port());
  EXPECT_EQ(8095, instance->get_auth_port());
  EXPECT_EQ(1, instance->get_clovis_layout_id());
  EXPECT_TRUE(instance->get_clovis_is_oostore());
  EXPECT_FALSE(instance->get_clovis_is_read_verify());
  EXPECT_EQ(16, instance->get_clovis_tm_recv_queue_min_len());
  EXPECT_EQ(65536, instance->get_clovis_max_rpc_msg_size());
  EXPECT_EQ("<0x7200000000000000:0>", instance->get_clovis_process_fid());
  EXPECT_EQ(1, instance->get_clovis_idx_service_id());
  EXPECT_EQ("10.10.1.3", instance->get_clovis_cass_cluster_ep());
  EXPECT_EQ("clovis_index_keyspace", instance->get_clovis_cass_keyspace());
  EXPECT_EQ(1, instance->get_clovis_cass_max_column_family_num());
  EXPECT_EQ(10, instance->get_log_file_max_size_in_mb());
  EXPECT_FALSE(instance->is_log_buffering_enabled());
  EXPECT_FALSE(instance->is_murmurhash_oid_enabled());
  EXPECT_EQ(3, instance->get_log_flush_frequency_in_sec());
  EXPECT_EQ(4, instance->get_s3_grace_period_sec());
  EXPECT_FALSE(instance->get_is_s3_shutting_down());
  EXPECT_FALSE(instance->is_stats_enabled());
  EXPECT_EQ("127.9.7.5", instance->get_statsd_ip_addr());
  EXPECT_EQ(9125, instance->get_statsd_port());
  EXPECT_EQ(15, instance->get_statsd_max_send_retry());
  EXPECT_EQ(5, instance->get_client_req_read_timeout_secs());
}

TEST_F(S3OptionsTest, SingletonCheck) {
  S3Option *inst1 = S3Option::get_instance();
  S3Option *inst2 = S3Option::get_instance();
  EXPECT_EQ(inst1, inst2);
  ASSERT_TRUE(inst1);
}

TEST_F(S3OptionsTest, GetOptionsfromFile) {
  EXPECT_TRUE(instance->load_all_sections(false));
  EXPECT_EQ(std::string("s3config-test.yaml"), instance->get_option_file());
  EXPECT_EQ(std::string("/var/log/seagate/s3"), instance->get_log_dir());
  EXPECT_STREQ("/etc/ssl/stx-s3/s3/ca.crt", instance->get_iam_cert_file());
  EXPECT_EQ(std::string("INFO"), instance->get_log_level());
  EXPECT_EQ(std::string("10.10.1.1"), instance->get_ipv4_bind_addr());
  EXPECT_EQ(std::string("localhost@tcp:12345:33:100"),
            instance->get_clovis_local_addr());
  EXPECT_EQ(std::string("<0x7000000000000001:0>"), instance->get_clovis_prof());
  EXPECT_EQ("<0x7200000000000000:0>", instance->get_clovis_process_fid());
  EXPECT_EQ(std::string("ipv4:10.10.1.2"), instance->get_auth_ip_addr());
  EXPECT_EQ(40960, instance->get_libevent_pool_initial_size());
  EXPECT_EQ(20480, instance->get_libevent_pool_expandable_size());
  EXPECT_EQ(104857600, instance->get_libevent_pool_max_threshold());
  EXPECT_EQ(9081, instance->get_s3_bind_port());
  EXPECT_EQ(8095, instance->get_auth_port());
  EXPECT_EQ(1, instance->get_clovis_layout_id());
  EXPECT_EQ(0, instance->s3_performance_enabled());
  EXPECT_EQ("10.10.1.3", instance->get_clovis_cass_cluster_ep());
  EXPECT_EQ(1, instance->get_clovis_idx_service_id());
  EXPECT_TRUE(instance->get_clovis_is_oostore());
  EXPECT_FALSE(instance->get_clovis_is_read_verify());
  EXPECT_EQ(10, instance->get_log_file_max_size_in_mb());
  EXPECT_FALSE(instance->is_log_buffering_enabled());
  EXPECT_FALSE(instance->is_murmurhash_oid_enabled());
  EXPECT_EQ(3, instance->get_log_flush_frequency_in_sec());
  EXPECT_EQ(4, instance->get_s3_grace_period_sec());
  EXPECT_FALSE(instance->is_stats_enabled());
  EXPECT_EQ("127.9.7.5", instance->get_statsd_ip_addr());
  EXPECT_EQ(9125, instance->get_statsd_port());
  EXPECT_EQ(15, instance->get_statsd_max_send_retry());
  EXPECT_EQ(5, instance->get_client_req_read_timeout_secs());
  EXPECT_EQ("s3stats-whitelist-test.yaml",
            instance->get_stats_whitelist_filename());
}

TEST_F(S3OptionsTest, TestOverrideOptions) {
  instance->set_cmdline_option(S3_OPTION_IPV4_BIND_ADDR, "198.1.1.1");
  instance->set_cmdline_option(S3_OPTION_IPV6_BIND_ADDR, "::1");
  instance->set_cmdline_option(S3_OPTION_BIND_PORT, "1");
  instance->set_cmdline_option(S3_OPTION_CLOVIS_LOCAL_ADDR, "localhost@test");
  instance->set_cmdline_option(S3_OPTION_AUTH_IP_ADDR, "192.192.191");
  instance->set_cmdline_option(S3_OPTION_AUTH_PORT, "2");
  instance->set_cmdline_option(S3_CLOVIS_LAYOUT_ID, "123");
  instance->set_cmdline_option(S3_OPTION_LOG_DIR, "/tmp/");
  instance->set_cmdline_option(S3_OPTION_LOG_MODE, "debug");
  instance->set_cmdline_option(S3_OPTION_LOG_FILE_MAX_SIZE, "1");
  instance->set_cmdline_option(S3_OPTION_STATSD_IP_ADDR, "192.168.0.9");
  instance->set_cmdline_option(S3_OPTION_STATSD_PORT, "1234");
  instance->set_cmdline_option(S3_OPTION_REUSEPORT, "true");
  // load from Config file, overriding the command options
  EXPECT_TRUE(instance->load_all_sections(true));
  EXPECT_EQ(std::string("/var/log/seagate/s3"), instance->get_log_dir());
  EXPECT_EQ(std::string("INFO"), instance->get_log_level());
  EXPECT_EQ(std::string("10.10.1.1"), instance->get_ipv4_bind_addr());
  EXPECT_EQ(std::string(""), instance->get_ipv6_bind_addr());
  EXPECT_EQ(std::string("localhost@tcp:12345:33:100"),
            instance->get_clovis_local_addr());
  EXPECT_EQ(std::string("<0x7000000000000001:0>"), instance->get_clovis_prof());
  EXPECT_EQ("<0x7200000000000000:0>", instance->get_clovis_process_fid());
  EXPECT_EQ(std::string("ipv4:10.10.1.2"), instance->get_auth_ip_addr());
  EXPECT_EQ(9081, instance->get_s3_bind_port());
  EXPECT_EQ(8095, instance->get_auth_port());
  EXPECT_EQ(1, instance->get_clovis_layout_id());
  EXPECT_EQ("10.10.1.3", instance->get_clovis_cass_cluster_ep());
  EXPECT_EQ(1, instance->get_clovis_idx_service_id());
  EXPECT_TRUE(instance->get_clovis_is_oostore());
  EXPECT_FALSE(instance->get_clovis_is_read_verify());
  EXPECT_EQ(10, instance->get_log_file_max_size_in_mb());
  EXPECT_FALSE(instance->is_log_buffering_enabled());
  EXPECT_FALSE(instance->is_murmurhash_oid_enabled());
  EXPECT_EQ(3, instance->get_log_flush_frequency_in_sec());
  EXPECT_EQ(4, instance->get_s3_grace_period_sec());
  EXPECT_FALSE(instance->is_stats_enabled());
  EXPECT_EQ("127.9.7.5", instance->get_statsd_ip_addr());
  EXPECT_EQ(9125, instance->get_statsd_port());
  EXPECT_EQ(15, instance->get_statsd_max_send_retry());
  EXPECT_EQ("s3stats-whitelist-test.yaml",
            instance->get_stats_whitelist_filename());
  EXPECT_FALSE(instance->is_s3_reuseport_enabled());
}

TEST_F(S3OptionsTest, TestDontOverrideCmdOptions) {
  instance->set_cmdline_option(S3_OPTION_IPV4_BIND_ADDR, "198.1.1.1");
  instance->set_cmdline_option(S3_OPTION_IPV6_BIND_ADDR, "::1");
  instance->set_cmdline_option(S3_OPTION_BIND_PORT, "1");
  instance->set_cmdline_option(S3_OPTION_CLOVIS_LOCAL_ADDR, "localhost@test");
  instance->set_cmdline_option(S3_OPTION_AUTH_IP_ADDR, "ipv4:192.168.15.131");
  instance->set_cmdline_option(S3_OPTION_AUTH_PORT, "2");
  instance->set_cmdline_option(S3_CLOVIS_LAYOUT_ID, "123");
  instance->set_cmdline_option(S3_OPTION_LOG_DIR, "/tmp/");
  instance->set_cmdline_option(S3_OPTION_LOG_MODE, "debug");
  instance->set_cmdline_option(S3_OPTION_LOG_FILE_MAX_SIZE, "1");
  instance->set_cmdline_option(S3_OPTION_STATSD_IP_ADDR, "192.168.0.9");
  instance->set_cmdline_option(S3_OPTION_STATSD_PORT, "1234");
  instance->set_cmdline_option(S3_OPTION_REUSEPORT, "true");
  instance->set_is_s3_shutting_down(true);
  EXPECT_TRUE(instance->load_all_sections(false));
  EXPECT_EQ(std::string("s3config-test.yaml"), instance->get_option_file());
  EXPECT_EQ(std::string("/tmp/"), instance->get_log_dir());
  EXPECT_EQ(std::string("debug"), instance->get_log_level());
  EXPECT_EQ(std::string("198.1.1.1"), instance->get_ipv4_bind_addr());
  EXPECT_EQ(std::string("::1"), instance->get_ipv6_bind_addr());
  EXPECT_EQ(std::string("localhost@test"), instance->get_clovis_local_addr());
  EXPECT_EQ(std::string("ipv4:192.168.15.131"), instance->get_auth_ip_addr());
  EXPECT_EQ(1, instance->get_s3_bind_port());
  EXPECT_EQ(2, instance->get_auth_port());
  EXPECT_EQ(123, instance->get_clovis_layout_id());
  EXPECT_EQ("10.10.1.3", instance->get_clovis_cass_cluster_ep());
  EXPECT_EQ(1, instance->get_clovis_idx_service_id());
  EXPECT_TRUE(instance->get_clovis_is_oostore());
  EXPECT_FALSE(instance->get_clovis_is_read_verify());
  EXPECT_EQ(1, instance->get_log_file_max_size_in_mb());
  EXPECT_TRUE(instance->get_is_s3_shutting_down());
  EXPECT_FALSE(instance->is_stats_enabled());
  EXPECT_EQ("192.168.0.9", instance->get_statsd_ip_addr());
  EXPECT_EQ(1234, instance->get_statsd_port());
  EXPECT_EQ(15, instance->get_statsd_max_send_retry());
  EXPECT_EQ("s3stats-whitelist-test.yaml",
            instance->get_stats_whitelist_filename());
  EXPECT_FALSE(instance->is_s3_reuseport_enabled());
}

TEST_F(S3OptionsTest, LoadThirdPartySectionFromFile) {
  EXPECT_TRUE(instance->load_section("S3_THIRDPARTY_CONFIG", false));
  EXPECT_EQ(40960, instance->get_libevent_pool_initial_size());
  EXPECT_EQ(20480, instance->get_libevent_pool_expandable_size());
  EXPECT_EQ(104857600, instance->get_libevent_pool_max_threshold());

  EXPECT_TRUE(instance->load_section("S3_SERVER_CONFIG", true));
  EXPECT_EQ(40960, instance->get_libevent_pool_initial_size());
  EXPECT_EQ(20480, instance->get_libevent_pool_expandable_size());
  EXPECT_EQ(104857600, instance->get_libevent_pool_max_threshold());
}

TEST_F(S3OptionsTest, LoadS3SectionFromFile) {
  EXPECT_TRUE(instance->load_section("S3_SERVER_CONFIG", false));

  EXPECT_EQ(std::string("/var/log/seagate/s3"), instance->get_log_dir());
  EXPECT_STREQ("/etc/ssl/stx-s3/s3/ca.crt", instance->get_iam_cert_file());
  EXPECT_EQ(std::string("INFO"), instance->get_log_level());
  EXPECT_EQ(std::string("10.10.1.1"), instance->get_ipv4_bind_addr());
  EXPECT_EQ(9081, instance->get_s3_bind_port());
  EXPECT_EQ(10, instance->get_log_file_max_size_in_mb());
  EXPECT_FALSE(instance->is_log_buffering_enabled());
  EXPECT_FALSE(instance->is_murmurhash_oid_enabled());
  EXPECT_FALSE(instance->is_stats_enabled());
  EXPECT_EQ("127.9.7.5", instance->get_statsd_ip_addr());
  EXPECT_EQ(9125, instance->get_statsd_port());
  EXPECT_EQ(15, instance->get_statsd_max_send_retry());
  EXPECT_EQ("s3stats-whitelist-test.yaml",
            instance->get_stats_whitelist_filename());

  // These will come with default values.
  EXPECT_EQ(std::string("localhost@tcp:12345:33:100"),
            instance->get_clovis_local_addr());
  EXPECT_EQ(std::string("<0x7000000000000001:0>"), instance->get_clovis_prof());
  EXPECT_EQ("<0x7200000000000000:0>", instance->get_clovis_process_fid());
  EXPECT_EQ(9, instance->get_clovis_layout_id());
  EXPECT_EQ(std::string("ipv4:127.0.0.1"), instance->get_auth_ip_addr());
  EXPECT_EQ(8095, instance->get_auth_port());
  EXPECT_EQ("127.0.0.1", instance->get_clovis_cass_cluster_ep());
  EXPECT_EQ(2, instance->get_clovis_idx_service_id());
  EXPECT_FALSE(instance->get_clovis_is_oostore());
  EXPECT_FALSE(instance->get_clovis_is_read_verify());
  EXPECT_FALSE(instance->is_s3_reuseport_enabled());
}

TEST_F(S3OptionsTest, LoadSelectiveS3SectionFromFile) {
  instance->set_cmdline_option(S3_OPTION_IPV4_BIND_ADDR, "198.1.1.1");
  instance->set_cmdline_option(S3_OPTION_IPV6_BIND_ADDR, "::1");
  instance->set_cmdline_option(S3_OPTION_BIND_PORT, "1");
  instance->set_cmdline_option(S3_OPTION_LOG_DIR, "/tmp/");
  instance->set_cmdline_option(S3_OPTION_LOG_MODE, "debug");
  instance->set_cmdline_option(S3_OPTION_LOG_FILE_MAX_SIZE, "1");
  instance->set_cmdline_option(S3_OPTION_STATSD_IP_ADDR, "192.168.0.9");
  instance->set_cmdline_option(S3_OPTION_REUSEPORT, "true");
  EXPECT_TRUE(instance->load_section("S3_SERVER_CONFIG", true));

  EXPECT_EQ(std::string("/var/log/seagate/s3"), instance->get_log_dir());
  EXPECT_EQ(std::string("INFO"), instance->get_log_level());
  EXPECT_EQ(std::string("10.10.1.1"), instance->get_ipv4_bind_addr());
  EXPECT_EQ(std::string(""), instance->get_ipv6_bind_addr());
  EXPECT_EQ(9081, instance->get_s3_bind_port());
  EXPECT_EQ(10, instance->get_log_file_max_size_in_mb());
  EXPECT_FALSE(instance->is_log_buffering_enabled());
  EXPECT_FALSE(instance->is_murmurhash_oid_enabled());
  EXPECT_FALSE(instance->is_stats_enabled());
  EXPECT_EQ("127.9.7.5", instance->get_statsd_ip_addr());
  EXPECT_EQ(9125, instance->get_statsd_port());
  EXPECT_EQ(15, instance->get_statsd_max_send_retry());
  EXPECT_EQ("s3stats-whitelist-test.yaml",
            instance->get_stats_whitelist_filename());

  // These should be default values
  EXPECT_EQ(std::string("localhost@tcp:12345:33:100"),
            instance->get_clovis_local_addr());
  EXPECT_EQ(std::string("<0x7000000000000001:0>"), instance->get_clovis_prof());
  EXPECT_EQ("<0x7200000000000000:0>", instance->get_clovis_process_fid());
  EXPECT_EQ(9, instance->get_clovis_layout_id());
  EXPECT_EQ(std::string("ipv4:127.0.0.1"), instance->get_auth_ip_addr());
  EXPECT_EQ(8095, instance->get_auth_port());
  EXPECT_EQ("127.0.0.1", instance->get_clovis_cass_cluster_ep());
  EXPECT_EQ(2, instance->get_clovis_idx_service_id());
  EXPECT_FALSE(instance->get_clovis_is_oostore());
  EXPECT_FALSE(instance->get_clovis_is_read_verify());
  EXPECT_FALSE(instance->is_s3_reuseport_enabled());
}

TEST_F(S3OptionsTest, LoadAuthSectionFromFile) {
  EXPECT_TRUE(instance->load_section("S3_AUTH_CONFIG", false));

  EXPECT_EQ(std::string("ipv4:10.10.1.2"), instance->get_auth_ip_addr());
  EXPECT_EQ(8095, instance->get_auth_port());

  // Others should not be loaded
  EXPECT_EQ(std::string("/var/log/seagate/s3"), instance->get_log_dir());
  EXPECT_EQ(std::string("INFO"), instance->get_log_level());
  EXPECT_EQ(std::string(""), instance->get_ipv4_bind_addr());
  EXPECT_EQ(std::string(""), instance->get_ipv6_bind_addr());
  EXPECT_EQ(8081, instance->get_s3_bind_port());
  EXPECT_EQ(std::string("localhost@tcp:12345:33:100"),
            instance->get_clovis_local_addr());
  EXPECT_EQ(std::string("<0x7000000000000001:0>"), instance->get_clovis_prof());
  EXPECT_EQ("<0x7200000000000000:0>", instance->get_clovis_process_fid());
  EXPECT_EQ(9, instance->get_clovis_layout_id());
  EXPECT_EQ("127.0.0.1", instance->get_clovis_cass_cluster_ep());
  EXPECT_EQ(2, instance->get_clovis_idx_service_id());
  EXPECT_FALSE(instance->get_clovis_is_oostore());
  EXPECT_FALSE(instance->get_clovis_is_read_verify());
  EXPECT_EQ(100, instance->get_log_file_max_size_in_mb());
  EXPECT_TRUE(instance->is_log_buffering_enabled());
  EXPECT_FALSE(instance->is_stats_enabled());
  EXPECT_EQ("127.0.0.1", instance->get_statsd_ip_addr());
  EXPECT_EQ(8125, instance->get_statsd_port());
  EXPECT_EQ(3, instance->get_statsd_max_send_retry());
}

TEST_F(S3OptionsTest, LoadSelectiveAuthSectionFromFile) {
  instance->set_cmdline_option(S3_OPTION_AUTH_IP_ADDR, "192.192.191.1");
  instance->set_cmdline_option(S3_OPTION_AUTH_PORT, "2");
  EXPECT_TRUE(instance->load_section("S3_AUTH_CONFIG", true));

  EXPECT_EQ(std::string("ipv4:10.10.1.2"), instance->get_auth_ip_addr());
  EXPECT_EQ(8095, instance->get_auth_port());

  // Others should not be loaded
  EXPECT_EQ(std::string("/var/log/seagate/s3"), instance->get_log_dir());
  EXPECT_EQ(std::string("INFO"), instance->get_log_level());
  EXPECT_EQ(std::string(""), instance->get_ipv4_bind_addr());
  EXPECT_EQ(std::string(""), instance->get_ipv6_bind_addr());
  EXPECT_EQ(8081, instance->get_s3_bind_port());
  EXPECT_EQ(std::string("localhost@tcp:12345:33:100"),
            instance->get_clovis_local_addr());
  EXPECT_EQ(std::string("<0x7000000000000001:0>"), instance->get_clovis_prof());
  EXPECT_EQ("<0x7200000000000000:0>", instance->get_clovis_process_fid());
  EXPECT_EQ(9, instance->get_clovis_layout_id());
  EXPECT_EQ("127.0.0.1", instance->get_clovis_cass_cluster_ep());
  EXPECT_EQ(2, instance->get_clovis_idx_service_id());
  EXPECT_FALSE(instance->get_clovis_is_oostore());
  EXPECT_FALSE(instance->get_clovis_is_read_verify());
  EXPECT_EQ(100, instance->get_log_file_max_size_in_mb());
  EXPECT_TRUE(instance->is_log_buffering_enabled());
  EXPECT_FALSE(instance->is_stats_enabled());
  EXPECT_EQ("127.0.0.1", instance->get_statsd_ip_addr());
  EXPECT_EQ(8125, instance->get_statsd_port());
  EXPECT_EQ(3, instance->get_statsd_max_send_retry());
}

TEST_F(S3OptionsTest, LoadClovisSectionFromFile) {
  EXPECT_TRUE(instance->load_section("S3_CLOVIS_CONFIG", false));

  EXPECT_EQ(std::string("localhost@tcp:12345:33:100"),
            instance->get_clovis_local_addr());
  EXPECT_EQ(std::string("<0x7000000000000001:0>"), instance->get_clovis_prof());
  EXPECT_EQ("<0x7200000000000000:0>", instance->get_clovis_process_fid());
  EXPECT_EQ("10.10.1.3", instance->get_clovis_cass_cluster_ep());
  EXPECT_EQ(1, instance->get_clovis_idx_service_id());
  EXPECT_TRUE(instance->get_clovis_is_oostore());
  EXPECT_FALSE(instance->get_clovis_is_read_verify());

  // Others should not be loaded
  EXPECT_EQ(std::string("/var/log/seagate/s3"), instance->get_log_dir());
  EXPECT_EQ(std::string("INFO"), instance->get_log_level());
  EXPECT_EQ(std::string(""), instance->get_ipv4_bind_addr());
  EXPECT_EQ(std::string(""), instance->get_ipv6_bind_addr());
  EXPECT_EQ(8081, instance->get_s3_bind_port());
  EXPECT_EQ(100, instance->get_log_file_max_size_in_mb());
  EXPECT_TRUE(instance->is_log_buffering_enabled());
  EXPECT_FALSE(instance->is_stats_enabled());
  EXPECT_EQ("127.0.0.1", instance->get_statsd_ip_addr());
  EXPECT_EQ(8125, instance->get_statsd_port());
  EXPECT_EQ(3, instance->get_statsd_max_send_retry());
}

TEST_F(S3OptionsTest, LoadSelectiveClovisSectionFromFile) {
  instance->set_cmdline_option(S3_OPTION_CLOVIS_LOCAL_ADDR, "localhost@test");
  EXPECT_TRUE(instance->load_section("S3_CLOVIS_CONFIG", true));

  EXPECT_EQ(std::string("localhost@tcp:12345:33:100"),
            instance->get_clovis_local_addr());
  EXPECT_EQ(std::string("<0x7000000000000001:0>"), instance->get_clovis_prof());
  EXPECT_EQ("<0x7200000000000000:0>", instance->get_clovis_process_fid());
  EXPECT_EQ("10.10.1.3", instance->get_clovis_cass_cluster_ep());
  EXPECT_EQ(1, instance->get_clovis_idx_service_id());
  EXPECT_TRUE(instance->get_clovis_is_oostore());
  EXPECT_FALSE(instance->get_clovis_is_read_verify());

  // Others should not be loaded
  EXPECT_EQ(std::string("/var/log/seagate/s3"), instance->get_log_dir());
  EXPECT_EQ(std::string("INFO"), instance->get_log_level());
  EXPECT_EQ(std::string(""), instance->get_ipv4_bind_addr());
  EXPECT_EQ(std::string(""), instance->get_ipv6_bind_addr());
  EXPECT_EQ(8081, instance->get_s3_bind_port());
  EXPECT_EQ(100, instance->get_log_file_max_size_in_mb());
  EXPECT_TRUE(instance->is_log_buffering_enabled());
  EXPECT_FALSE(instance->is_stats_enabled());
  EXPECT_EQ("127.0.0.1", instance->get_statsd_ip_addr());
  EXPECT_EQ(8125, instance->get_statsd_port());
  EXPECT_EQ(3, instance->get_statsd_max_send_retry());
}

TEST_F(S3OptionsTest, SetCmdOptionFlag) {
  int flag;
  instance->set_cmdline_option(S3_OPTION_IPV4_BIND_ADDR, "198.1.1.1");
  instance->set_cmdline_option(S3_OPTION_IPV6_BIND_ADDR, "::1");
  instance->set_cmdline_option(S3_OPTION_BIND_PORT, "1");
  instance->set_cmdline_option(S3_OPTION_CLOVIS_LOCAL_ADDR, "localhost@test");
  instance->set_cmdline_option(S3_OPTION_AUTH_IP_ADDR, "192.192.191");
  instance->set_cmdline_option(S3_OPTION_AUTH_PORT, "2");
  instance->set_cmdline_option(S3_CLOVIS_LAYOUT_ID, "123");
  instance->set_cmdline_option(S3_OPTION_LOG_DIR, "/tmp/");
  instance->set_cmdline_option(S3_OPTION_LOG_MODE, "debug");
  instance->set_cmdline_option(S3_OPTION_LOG_FILE_MAX_SIZE, "1");
  instance->set_cmdline_option(S3_OPTION_STATSD_IP_ADDR, "192.168.0.9");
  instance->set_cmdline_option(S3_OPTION_STATSD_PORT, "1234");
  instance->set_cmdline_option(S3_OPTION_CLOVIS_PROF, "<0x7000000000000001:0>");
  instance->set_cmdline_option(S3_OPTION_CLOVIS_PROCESS_FID,
                               "<0x7200000000000000:0>");

  flag = S3_OPTION_IPV4_BIND_ADDR | S3_OPTION_IPV6_BIND_ADDR |
         S3_OPTION_BIND_PORT | S3_OPTION_CLOVIS_LOCAL_ADDR |
         S3_OPTION_AUTH_IP_ADDR | S3_OPTION_AUTH_PORT | S3_CLOVIS_LAYOUT_ID |
         S3_OPTION_LOG_DIR | S3_OPTION_LOG_MODE | S3_OPTION_LOG_FILE_MAX_SIZE |
         S3_OPTION_STATSD_IP_ADDR | S3_OPTION_STATSD_PORT |
         S3_OPTION_CLOVIS_PROF | S3_OPTION_CLOVIS_PROCESS_FID;

  EXPECT_EQ(flag, instance->get_cmd_opt_flag());
}

TEST_F(S3OptionsTest, GetDefaultEndPoint) {
  EXPECT_TRUE(instance->load_all_sections(false));
  EXPECT_EQ(std::string("s3.seagate-test.com"),
            instance->get_default_endpoint());
}

TEST_F(S3OptionsTest, GetRegionEndPoints) {
  EXPECT_TRUE(instance->load_all_sections(false));
  std::set<std::string> region_eps = instance->get_region_endpoints();
  EXPECT_TRUE(region_eps.find("s3-asia.seagate-test.com") != region_eps.end());
  EXPECT_TRUE(region_eps.find("s3-us.seagate-test.com") != region_eps.end());
  EXPECT_TRUE(region_eps.find("s3-europe.seagate-test.com") !=
              region_eps.end());
  EXPECT_FALSE(region_eps.find("invalid-region.seagate.com") !=
               region_eps.end());
}

TEST_F(S3OptionsTest, MissingOptions) {
  // create a temporary yaml file
  std::ofstream cfg_file;
  std::string config_file("s3config-temp-test.yaml");
  cfg_file.open(config_file);
  cfg_file << "S3Config_Sections: [S3_SERVER_CONFIG, S3_AUTH_CONFIG, "
              "S3_CLOVIS_CONFIG]\n";
  cfg_file << "S3_SERVER_CONFIG:\n";
  cfg_file << "   S3_LOG_MODE: INFO\n";
  cfg_file << "S3_AUTH_CONFIG:\n";
  cfg_file << "   S3_AUTH_PORT: 8095\n";
  cfg_file << "S3_CLOVIS_CONFIG:\n";
  cfg_file << "   S3_CLOVIS_MAX_BLOCKS_PER_REQUEST: 1\n";
  cfg_file.close();

  instance->set_option_file(config_file);
  EXPECT_FALSE(instance->load_section("S3_CLOVIS_CONFIG", false));
  EXPECT_FALSE(instance->load_section("S3_AUTH_CONFIG", false));
  EXPECT_FALSE(instance->load_section("S3_CLOVIS_CONFIG", false));

  EXPECT_FALSE(instance->load_section("S3_CLOVIS_CONFIG", true));
  EXPECT_FALSE(instance->load_section("S3_AUTH_CONFIG", true));
  EXPECT_FALSE(instance->load_section("S3_CLOVIS_CONFIG", true));

  EXPECT_FALSE(instance->load_all_sections(false));
  EXPECT_FALSE(instance->load_all_sections(true));

  // delete the file
  unlink(config_file.c_str());
}
