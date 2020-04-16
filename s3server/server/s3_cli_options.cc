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
 * Original author:  Kaustubh Deorukhkar <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 16-Jun-2016
 */

#include "s3_cli_options.h"
#include "s3_option.h"

DEFINE_string(s3config, "/opt/seagate/s3/conf/s3config.yaml",
              "S3 server config file");

DEFINE_string(s3layoutmap, "/opt/seagate/s3/conf/s3_obj_layout_mapping.yaml",
              "S3 Clovis layout mapping file for different object sizes");

DEFINE_string(s3hostv4, "", "S3 server ipv4 bind address");
DEFINE_string(s3hostv6, "", "S3 server ipv6 bind address");
DEFINE_string(merohttpapihost, "", "S3 server mero http bind address");
DEFINE_int32(s3port, 8081, "S3 server bind port");
DEFINE_int32(merohttpapiport, 7081, "mero http server bind port");
DEFINE_string(s3pidfile, "/var/run/s3server.pid", "S3 server pid file");

DEFINE_string(audit_config,
              "/opt/seagate/s3/conf/s3server_audit_log.properties",
              "S3 Audit log Config Path");
DEFINE_string(s3loglevel, "INFO",
              "options: DEBUG | INFO | WARN | ERROR | FATAL");

DEFINE_bool(perfenable, false, "Enable performance log");
DEFINE_bool(reuseport, false, "Enable reusing s3 server port");
DEFINE_string(perflogfile, "/var/log/seagate/s3/perf.log",
              "Performance log path");

DEFINE_string(clovislocal, "localhost@tcp:12345:33:100",
              "Clovis local address");
DEFINE_string(clovisha, "CLOVIS_DEFAULT_HA_ADDR", "Clovis ha address");
DEFINE_int32(clovislayoutid, 9, "For options please see the readme");
DEFINE_string(clovisprofilefid, "<0x7000000000000001:0>", "Clovis profile FID");
DEFINE_string(clovisprocessfid, "<0x7200000000000000:0>", "Clovis process FID");

DEFINE_string(authhost, "ipv4:127.0.0.1", "Auth server host");
DEFINE_int32(authport, 8095, "Auth server port");
DEFINE_bool(disable_auth, false, "Disable authentication");
DEFINE_bool(getoid, false, "Enable getoid in S3 request for testing");

DEFINE_bool(fake_authenticate, false, "Fake out authenticate");
DEFINE_bool(fake_authorization, false, "Fake out authorization");

DEFINE_bool(fake_clovis_createobj, false, "Fake out clovis create object");
DEFINE_bool(fake_clovis_writeobj, false, "Fake out clovis write object data");
DEFINE_bool(fake_clovis_readobj, false, "Fake out clovis read object data");
DEFINE_bool(fake_clovis_deleteobj, false, "Fake out clovis delete object");
DEFINE_bool(fake_clovis_createidx, false, "Fake out clovis create index");
DEFINE_bool(fake_clovis_deleteidx, false, "Fake out clovis delete index");
DEFINE_bool(fake_clovis_getkv, false, "Fake out clovis get key-val");
DEFINE_bool(fake_clovis_putkv, false, "Fake out clovis put key-val");
DEFINE_bool(fake_clovis_deletekv, false, "Fake out clovis delete key-val");
DEFINE_bool(fake_clovis_redis_kvs, false,
            "Fake out clovis kvs with redis in-memory storage");
DEFINE_bool(fault_injection, false, "Enable fault Injection flag for testing");
DEFINE_bool(loading_indicators, false, "Enable logging load indicators");
DEFINE_bool(addb, false, "Enable logging via ADDB mero subsystem");

DEFINE_string(statsd_host, "127.0.0.1", "StatsD daemon host");
DEFINE_int32(statsd_port, 8125, "StatsD daemon port");

int parse_and_load_config_options(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);

  // Create the initial options object with default values.
  S3Option *option_instance = S3Option::get_instance();

  // load the configurations from config file.
  option_instance->set_option_file(FLAGS_s3config);
  bool force_override_from_config = true;
  if (!option_instance->load_all_sections(force_override_from_config)) {
    return -1;
  }

  // Override with options set on command line
  gflags::CommandLineFlagInfo flag_info;

  gflags::GetCommandLineFlagInfo("s3hostv4", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_IPV4_BIND_ADDR,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("s3hostv6", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_IPV6_BIND_ADDR,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("merohttpapihost", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_MERO_BIND_ADDR,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("s3port", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_BIND_PORT,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("merohttpapiport", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_MERO_BIND_PORT,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("s3pidfile", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_PIDFILE,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("authhost", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_AUTH_IP_ADDR,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("authport", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_AUTH_PORT,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("perflogfile", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_PERF_LOG_FILE,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("log_dir", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_LOG_DIR,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("s3loglevel", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_LOG_MODE,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("audit_config", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_AUDIT_CONFIG,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("reuseport", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_REUSEPORT,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("max_log_size", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_LOG_FILE_MAX_SIZE,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("clovislocal", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_CLOVIS_LOCAL_ADDR,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("clovisha", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_CLOVIS_HA_ADDR,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("clovislayoutid", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_CLOVIS_LAYOUT_ID,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("statsd_host", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_STATSD_IP_ADDR,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("statsd_port", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_STATSD_PORT,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("clovisprofilefid", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_CLOVIS_PROF,
                                        flag_info.current_value.c_str());
  }

  gflags::GetCommandLineFlagInfo("clovisprocessfid", &flag_info);
  if (!flag_info.is_default) {
    option_instance->set_cmdline_option(S3_OPTION_CLOVIS_PROCESS_FID,
                                        flag_info.current_value.c_str());
  }
  return 0;
}

void finalize_cli_options() { gflags::ShutDownCommandLineFlags(); }
