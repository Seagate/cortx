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
 * Original author:  Ivan Tishchenko     <ivan.tishchenko@seagate.com>
 * Original creation date: 24-Dec-2019
 */

#include <memory>
#include <string>
#include <cstdint>

#include "s3_stats.h"
#include "event_utils.h"
#include "s3_perf_metrics.h"
#include "atexit.h"

// Helper class, will keep value of a single throughput metric.
class S3ThroughputMetric {
 private:
  int64_t bytes_cnt = 0;
  std::string name;

 public:
  explicit S3ThroughputMetric(std::string const &name_) : name(name_) {}

  void submit() {
    s3_stats_count(name, bytes_cnt);
    bytes_cnt = 0;
  }

  void more_bytes(int64_t cnt) {
    bytes_cnt += cnt;
    if (bytes_cnt > (((int64_t)1) << 51)) {  // some docs say that statsd can
                                             // eat numbers up to 2^52^, so
                                             // making sure we're not above that
                                             // limit
      submit();
    }
  }
};

// Helper class, defines recurring event for metrics submission to statsd.
class S3ThroughputMetricsEvent : public RecurringEventBase {
 private:
  S3ThroughputMetric in{"incoming_object_bytes_count"};
  S3ThroughputMetric out{"outcoming_object_bytes_count"};

  void submit_metrics() {
    in.submit();
    out.submit();
  }

 public:
  S3ThroughputMetricsEvent(std::shared_ptr<EventInterface> event_obj_ptr,
                           evbase_t *evbase_ = nullptr)
      : RecurringEventBase(std::move(event_obj_ptr), evbase_) {}

  virtual void action_callback(void) noexcept { submit_metrics(); }

  void more_bytes_in(int cnt) { in.more_bytes(cnt); }

  void more_bytes_out(int cnt) { out.more_bytes(cnt); }
};

static std::shared_ptr<EventWrapper> gs_event_obj_ptr;
static std::shared_ptr<S3ThroughputMetricsEvent> gs_throughput_event;

int s3_perf_metrics_init(evbase_t *evbase) {
  int rc;
  struct timeval tv;
  if (!g_option_instance->is_stats_enabled()) {
    return 0;
  }
  s3_log(S3_LOG_DEBUG, "", "Entering");

  AtExit call_fini([]() { s3_perf_metrics_fini(); });

  if (!evbase) {
    return -EINVAL;
  }
  gs_event_obj_ptr.reset(new EventWrapper());
  if (!gs_event_obj_ptr) {
    return -ENOMEM;
  }
  gs_throughput_event.reset(
      new S3ThroughputMetricsEvent(gs_event_obj_ptr, evbase));
  if (!gs_throughput_event) {
    return -ENOMEM;
  }
  tv.tv_sec =
      S3Option::get_instance()->get_perf_stats_inout_bytes_interval_msec() /
      1000;
  tv.tv_usec =
      1000 *
      (S3Option::get_instance()->get_perf_stats_inout_bytes_interval_msec() %
       1000);
  rc = gs_throughput_event->add_evtimer(tv);
  if (rc != 0) {
    return rc;
  }

  call_fini.cancel();

  return 0;
}

void s3_perf_metrics_fini() {
  if (!g_option_instance->is_stats_enabled()) {
    return;
  }
  s3_log(S3_LOG_DEBUG, "", "Entering");
  if (gs_throughput_event) {
    gs_throughput_event->del_evtimer();
    gs_throughput_event.reset();
  }
  if (gs_event_obj_ptr) {
    gs_event_obj_ptr.reset();
  }
}

void s3_perf_count_incoming_bytes(int byte_count) {
  if (!g_option_instance->is_stats_enabled()) {
    return;
  }
  s3_log(S3_LOG_DEBUG, "", "Entering with byte_count = %d", byte_count);
  if (gs_throughput_event) {
    gs_throughput_event->more_bytes_in(byte_count);
  }
}

void s3_perf_count_outcoming_bytes(int byte_count) {
  if (!g_option_instance->is_stats_enabled()) {
    return;
  }
  s3_log(S3_LOG_DEBUG, "", "Entering with byte_count = %d", byte_count);
  if (gs_throughput_event) {
    gs_throughput_event->more_bytes_out(byte_count);
  }
}
