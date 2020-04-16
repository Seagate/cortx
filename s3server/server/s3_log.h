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
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original author:  Vinayak Kale <vinayak.kale@seagate.com>
 * Original creation date: 2-March-2016
 */

#pragma once

#ifndef __S3_SERVER_LOG_H__
#define __S3_SERVER_LOG_H__
#include <iostream>
#include <string>
#include <glog/logging.h>
#include <syslog.h>
#include <time.h>
#include <memory>
#include <inttypes.h>

#define S3_LOG_FATAL 4
#define S3_LOG_ERROR 3
#define S3_LOG_WARN 2
#define S3_LOG_INFO 1
#define S3_LOG_DEBUG 0

const char S3_DEFAULT_REQID[] = "-";

extern int s3log_level;

inline const char* s3_log_get_req_id(const char* requestid) {
  return (requestid && requestid[0]) ? requestid : S3_DEFAULT_REQID;
}

inline const char* s3_log_get_req_id(const std::string& requestid) {
  return requestid.empty() ? S3_DEFAULT_REQID : requestid.c_str();
}

#define s3_log_msg_S3_LOG_DEBUG(p) (LOG(INFO) << (p))
#define s3_log_msg_S3_LOG_INFO(p) (LOG(INFO) << (p))
#define s3_log_msg_S3_LOG_WARN(p) (LOG(WARNING) << (p))
#define s3_log_msg_S3_LOG_ERROR(p) (LOG(ERROR) << (p))
#define s3_log_msg_S3_LOG_FATAL(p) (LOG(FATAL) << (p))

// Note:
// 1. Google glog doesn't have a separate severity level for DEBUG logs.
//    So we map our DEBUG logs to INFO level. This level promotion happens
//    only if S3 log level is set to DEBUG.
// 2. Logging a FATAL message terminates the program (after the message is
//    logged).
#define s3_log(loglevel, requestid, fmt, ...)                             \
  do {                                                                    \
    if (loglevel >= s3log_level) {                                        \
      char* s3_log_msg__ = nullptr;                                       \
      int s3_log_len__ =                                                  \
          asprintf(&s3_log_msg__, "[%s] [ReqID: %s] " fmt "\n", __func__, \
                   s3_log_get_req_id(requestid), ##__VA_ARGS__);          \
      if (s3_log_len__ > 0) {                                             \
        if (s3_log_msg__[s3_log_len__ - 2] == '\n')                       \
          s3_log_msg__[s3_log_len__ - 1] = '\0';                          \
        s3_log_msg_##loglevel(s3_log_msg__);                              \
        free(s3_log_msg__);                                               \
      }                                                                   \
    }                                                                     \
  } while (0)

// Note:
// 1. Use syslog defined severity levels as loglevel.
// 2. syslog messages will always be sent irrespective of s3loglevel.
#define s3_syslog(loglevel, fmt, ...)     \
  do {                                    \
    syslog(loglevel, fmt, ##__VA_ARGS__); \
  } while (0)

// returns timestamp in format: "yyyy:mm:dd hh:mm:ss.uuuuuu"
static inline std::string s3_get_timestamp() {
  struct timespec ts;
  struct tm result;
  char date_time[20];
  char timestamp[30];

  clock_gettime(CLOCK_REALTIME, &ts);
  tzset();
  if (localtime_r(&ts.tv_sec, &result) == NULL) {
    return std::string();
  }
  strftime(date_time, sizeof(date_time), "%Y:%m:%d %H:%M:%S", &result);
  snprintf(timestamp, sizeof(timestamp), "%s.%06li", date_time,
           ts.tv_nsec / 1000);
  return std::string(timestamp);
}

int init_log(char *process_name);
void fini_log();
void flushall_log();

#endif  // __S3_SERVER_LOG_H__
