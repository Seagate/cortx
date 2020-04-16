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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 8-Jan-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_PERF_LOGGER_H__
#define __S3_SERVER_S3_PERF_LOGGER_H__

#include <string>
#include <fstream>
#include <chrono>

// Not thread-safe, but we are single threaded.
class S3PerfLogger {

  static S3PerfLogger* instance;
  std::ofstream perf_file;

 public:
  // Opens the Performance log file
  // "/var/log/seagate/s3/perf.log" should be by default
  S3PerfLogger(const std::string& log_file);

  static bool is_enabled() noexcept { return instance; }

  static void write(const char* perf_text, const char* req_id,
                    size_t elapsed_time);
};

namespace s3_perf {

inline const char* to_c_str(const char* c_str) {
  return (c_str && c_str[0]) ? c_str : "-";
}

inline const char* to_c_str(const std::string& str) {
  return str.empty() ? "-" : str.c_str();
}

}  // namespace s3_perf

#define LOG_PERF(perf_text, req_id, elapsed_time)                 \
  if (S3PerfLogger::is_enabled()) {                               \
    S3PerfLogger::write(s3_perf::to_c_str(perf_text),             \
                        s3_perf::to_c_str(req_id), elapsed_time); \
  }

struct TimedCounter {
  std::chrono::time_point<std::chrono::steady_clock> time_point;
  unsigned counter;
};

extern TimedCounter get_timed_counter;
extern TimedCounter put_timed_counter;

void log_timed_counter(TimedCounter& timed_counter,
                       const std::string& metric_name);

#endif  // __S3_SERVER_S3_PERF_LOGGER_H__
