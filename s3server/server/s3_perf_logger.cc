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

#include <cassert>
#include <sstream>
#include "s3_perf_logger.h"
#include "s3_log.h"
#include "s3_stats.h"
#include "s3_cli_options.h"

S3PerfLogger* S3PerfLogger::instance;

S3PerfLogger::S3PerfLogger(const std::string& log_file) {

  if (!instance) {
    perf_file.open(log_file.c_str(), std::ios_base::ate);

    if (perf_file.is_open()) {
      instance = this;
    }
  }
}

/*
 Ensure the pointers really point to zero-terminated char arrays
*/
void S3PerfLogger::write(const char* perf_text, const char* req_id,
                         size_t elapsed_time) {
  assert(perf_text && perf_text[0]);
  assert(req_id && req_id[0]);

  if (instance && elapsed_time != (size_t)(-1)) {
    instance->perf_file << req_id << ' ' << perf_text << ':' << elapsed_time
                        << '\n';
  }
}

TimedCounter get_timed_counter;
TimedCounter put_timed_counter;

/*
 The function is called on every incoming/outgoing block,
 accumulates the count of calls, and then approx every 1 second dumps
 this information to PERF LOG, to s3server.log and to STATSD.
 TimedCounter structure should be zero-initialized before 1st invocation.
*/
void log_timed_counter(TimedCounter& timed_counter,
                       const std::string& metric_name) {
  if (!FLAGS_loading_indicators) {
    return;
  }
  using namespace std::chrono;

  auto current_time = steady_clock::now();
  auto counter = ++timed_counter.counter;

  if (timed_counter.time_point.time_since_epoch().count() == 0) {
    // The first invocation of the function.
    // "timed_counter.time_point" should contain zero.
    timed_counter.time_point = current_time;
    return;
  }
  auto elapsed_ms = duration_cast<milliseconds>(
      current_time - timed_counter.time_point).count();

  if (elapsed_ms < 1000) {
    // Less then 1 second elapsed
    return;
  }
  std::ostringstream oss;
  oss << metric_name << "_count = " << counter
      << ", elapsed_ms = " << elapsed_ms << ", rate = "
      << static_cast<decltype(counter)>(counter * 1000.0 / elapsed_ms)
      << " events/sec\n";
  std::string str = oss.str();
  oss.clear();
  s3_log(S3_LOG_INFO, "", "%s", str.c_str());

  s3_stats_count(metric_name + "_count", static_cast<int>(counter));

  // counter is always > 0 here
  LOG_PERF(metric_name + "_avg_ms", nullptr, elapsed_ms / counter);

  timed_counter.time_point = current_time;
  timed_counter.counter = 0;
}
