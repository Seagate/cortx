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
 * Original creation date: 25-June-2019
 */

#include "s3_audit_info_logger_syslog.h"
#include "s3_log.h"
#include <syslog.h>

// NOTE: syslog in used by RAS product, so recofiguration should be avoided
// so openlog/closelog should be avoided

S3AuditInfoLoggerSyslog::S3AuditInfoLoggerSyslog(std::string const &msg_filter)
    : rsyslog_msg_filter(msg_filter) {}

S3AuditInfoLoggerSyslog::~S3AuditInfoLoggerSyslog() {}

int S3AuditInfoLoggerSyslog::save_msg(std::string const &cur_request_id,
                                      std::string const &audit_logging_msg) {
  s3_log(S3_LOG_DEBUG, cur_request_id, "Entering\n");
  syslog(LOG_NOTICE, "%s%s", rsyslog_msg_filter.c_str(),
         audit_logging_msg.c_str());
  s3_log(S3_LOG_DEBUG, cur_request_id, "Exiting\n");
  return 0;
}
