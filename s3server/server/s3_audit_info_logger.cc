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
 * Original author:  Dmitrii Surnin <dmitrii.surnin@seagate.com>
 * Original creation date: 10-June-2019
 */

#include "s3_option.h"
#include "s3_log.h"
#include "s3_audit_info_logger.h"
#include "s3_audit_info_logger_rsyslog_tcp.h"
#include "s3_audit_info_logger_log4cxx.h"
#include "s3_audit_info_logger_syslog.h"

#include <stdexcept>

S3AuditInfoLoggerBase* S3AuditInfoLogger::audit_info_logger = nullptr;
bool S3AuditInfoLogger::audit_info_logger_enabled = false;

int S3AuditInfoLogger::init() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  int ret = 0;
  audit_info_logger = nullptr;
  audit_info_logger_enabled = false;
  std::string policy = S3Option::get_instance()->get_audit_logger_policy();
  if (policy == "disabled") {
    s3_log(S3_LOG_INFO, "", "Audit logger disabled by policy settings\n");
  } else if (policy == "rsyslog-tcp") {
    s3_log(S3_LOG_INFO, "", "Audit logger:> tcp %s:%d msgid:> %s\n",
           S3Option::get_instance()->get_audit_logger_host().c_str(),
           S3Option::get_instance()->get_audit_logger_port(),
           S3Option::get_instance()->get_audit_logger_rsyslog_msgid().c_str());
    try {
      audit_info_logger = new S3AuditInfoLoggerRsyslogTcp(
          S3Option::get_instance()->get_eventbase(),
          S3Option::get_instance()->get_audit_logger_host(),
          S3Option::get_instance()->get_audit_logger_port(),
          S3Option::get_instance()->get_max_retry_count(),
          S3Option::get_instance()->get_audit_logger_rsyslog_msgid());
      audit_info_logger_enabled = true;
    }
    catch (std::exception const& ex) {
      s3_log(S3_LOG_ERROR, "", "Cannot create Rsyslog logger %s", ex.what());
      audit_info_logger = nullptr;
      ret = -1;
    }
  } else if (policy == "log4cxx") {
    s3_log(S3_LOG_INFO, "", "Audit logger:> log4cxx cfg_file %s\n",
           S3Option::get_instance()->get_s3_audit_config().c_str());
    try {
      audit_info_logger = new S3AuditInfoLoggerLog4cxx(
          S3Option::get_instance()->get_s3_audit_config(),
          S3Option::get_instance()->get_log_dir() + "/audit/audit.log");
      audit_info_logger_enabled = true;
    }
    catch (...) {
      s3_log(S3_LOG_FATAL, "", "Cannot create Log4cxx logger");
      audit_info_logger = nullptr;
      ret = -1;
    }
  } else if (policy == "syslog") {
    s3_log(S3_LOG_INFO, "", "Audit logger:> syslog\n");
    audit_info_logger = new S3AuditInfoLoggerSyslog(
        S3Option::get_instance()->get_audit_logger_rsyslog_msgid());
    audit_info_logger_enabled = true;
  } else {
    s3_log(S3_LOG_INFO, "", "Audit logger disabled by unknown policy %s\n",
           policy.c_str());
    ret = -1;
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting ret %d\n", ret);
  return ret;
}

int S3AuditInfoLogger::save_msg(std::string const& cur_request_id,
                                std::string const& audit_logging_msg) {
  if (audit_info_logger) {
    return audit_info_logger->save_msg(cur_request_id, audit_logging_msg);
  }
  return 1;
}

bool S3AuditInfoLogger::is_enabled() { return audit_info_logger_enabled; }

void S3AuditInfoLogger::finalize() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  delete audit_info_logger;
  audit_info_logger = nullptr;
  audit_info_logger_enabled = false;
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
