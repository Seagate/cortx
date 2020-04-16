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

#ifndef __S3_SERVER_AUDIT_INFO_LOGGER_LOG4CXX_H__
#define __S3_SERVER_AUDIT_INFO_LOGGER_LOG4CXX_H__

#include "s3_audit_info_logger_base.h"

#include "log4cxx/logger.h"

class S3AuditInfoLoggerLog4cxx : public S3AuditInfoLoggerBase {
 public:
  S3AuditInfoLoggerLog4cxx(std::string const &, std::string const & = "");
  virtual ~S3AuditInfoLoggerLog4cxx();
  virtual int save_msg(std::string const &, std::string const &);

 private:
  log4cxx::LoggerPtr l4c_logger_ptr;
};

#endif
