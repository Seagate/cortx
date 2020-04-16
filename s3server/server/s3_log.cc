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
 * Original author:  Vinayak Kale <vinayak.kale@seagate.com>
 * Original creation date: 11-August-2016
 */

#include "s3_log.h"
#include "s3_option.h"

int s3log_level = S3_LOG_INFO;

int init_log(char *process_name) {
  S3Option *option_instance = S3Option::get_instance();

  if (option_instance->get_log_level() != "") {
    if (option_instance->get_log_level() == "INFO") {
      s3log_level = S3_LOG_INFO;
    } else if (option_instance->get_log_level() == "DEBUG") {
      s3log_level = S3_LOG_DEBUG;
    } else if (option_instance->get_log_level() == "ERROR") {
      s3log_level = S3_LOG_ERROR;
    } else if (option_instance->get_log_level() == "FATAL") {
      s3log_level = S3_LOG_FATAL;
    } else if (option_instance->get_log_level() == "WARN") {
      s3log_level = S3_LOG_WARN;
    }
  } else {
    s3log_level = S3_LOG_INFO;
  }

  if (option_instance->get_log_dir() != "") {
    FLAGS_log_dir = option_instance->get_log_dir();
  } else {
    FLAGS_logtostderr = true;
  }
  switch (s3log_level) {
    case S3_LOG_WARN:
      FLAGS_minloglevel = google::GLOG_WARNING;
      break;
    case S3_LOG_ERROR:
      FLAGS_minloglevel = google::GLOG_ERROR;
      break;
    case S3_LOG_FATAL:
      FLAGS_minloglevel = google::GLOG_FATAL;
      break;
    default:
      FLAGS_minloglevel = google::GLOG_INFO;
  }
  FLAGS_max_log_size = option_instance->get_log_file_max_size_in_mb();

  if (option_instance->is_log_buffering_enabled()) {
    // DEBUG, INFO & WARN logs are buffered.
    // ERROR & FATAL logs are always flushed immediately.
    FLAGS_logbuflevel = google::GLOG_WARNING;
    FLAGS_logbufsecs = option_instance->get_log_flush_frequency_in_sec();
  } else {
    // All logs are flushed immediately.
    FLAGS_logbuflevel = -1;
  }

  google::InitGoogleLogging(process_name);

  // init the syslog
  openlog(process_name, LOG_CONS | LOG_NDELAY | LOG_PID, LOG_USER);
  return 0;
}

void fini_log() {
  google::FlushLogFiles(google::GLOG_INFO);
  google::ShutdownGoogleLogging();

  // close syslog
  closelog();
}

void flushall_log() { google::FlushLogFiles(google::GLOG_INFO); }
