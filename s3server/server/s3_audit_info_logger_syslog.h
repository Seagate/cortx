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

#pragma once

#ifndef __S3_SERVER_AUDIT_INFO_LOGGER_SYSLOG_H__
#define __S3_SERVER_AUDIT_INFO_LOGGER_SYSLOG_H__

#include "s3_audit_info_logger_base.h"

class S3AuditInfoLoggerSyslog : public S3AuditInfoLoggerBase {
 public:
  S3AuditInfoLoggerSyslog(std::string const &msg_filter =
                              "s3server-audit-logging");
  virtual ~S3AuditInfoLoggerSyslog();
  virtual int save_msg(std::string const &, std::string const &);

 private:
  std::string rsyslog_msg_filter;
};

#endif
