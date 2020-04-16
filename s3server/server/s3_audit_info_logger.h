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

#pragma once

#ifndef __S3_SERVER_AUDIT_INFO_LOGGER__H__
#define __S3_SERVER_AUDIT_INFO_LOGGER__H__

#include "s3_audit_info_logger_base.h"

#include "gtest/gtest_prod.h"

class S3AuditInfoLogger {
 private:
  static S3AuditInfoLoggerBase* audit_info_logger;
  static bool audit_info_logger_enabled;

 public:
  static int init();
  static void finalize();
  static bool is_enabled();
  static int save_msg(std::string const&, std::string const&);
  S3AuditInfoLogger() = delete;

 private:
  FRIEND_TEST(S3AuditInfoLoggerTest, PolicyDisabled);
  FRIEND_TEST(S3AuditInfoLoggerTest, PolicyInvalid);
  FRIEND_TEST(S3AuditInfoLoggerTest, MissedInit);
  FRIEND_TEST(S3AuditInfoLoggerTest, PolicyLog4cxx);
  FRIEND_TEST(S3AuditInfoLoggerTest, PolicySyslog);
  FRIEND_TEST(S3AuditInfoLoggerTest, PolicyRsyslogTcp);
  FRIEND_TEST(S3AuditInfoLoggerTest, PolicyRsyslogTcpBaseNULL);
};

#endif
