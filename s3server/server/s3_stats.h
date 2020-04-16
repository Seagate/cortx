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
 * Original creation date: 26-October-2016
 */

#pragma once

#ifndef __S3_SERVER_STATS_H__
#define __S3_SERVER_STATS_H__

#include <gtest/gtest_prod.h>
#include <math.h>
#include <limits>
#include <cstdint>
#include <string>
#include <unordered_set>
#include "s3_log.h"
#include "s3_option.h"
#include "socket_wrapper.h"

class S3Stats {
 public:
  static S3Stats* get_instance(SocketInterface* socket_obj = NULL);
  static void delete_instance();

  // Increase/decrease count for `key` by `value`
  int count(const std::string& key, int64_t value, int retry = 1,
            float sample_rate = 1.0);

  // Log the time in ms for `key`
  int timing(const std::string& key, size_t time_ms, int retry = 1,
             float sample_rate = 1.0);

  // Set the initial gauge value for `key`
  int set_gauge(const std::string& key, int value, int retry = 1);

  // Increase/decrease gauge for `key` by `value`
  int update_gauge(const std::string& key, int value, int retry = 1);

  // Count unique `value`s for a `key`
  int count_unique(const std::string& key, const std::string& value,
                   int retry = 1);

 private:
  S3Stats(const std::string& host_addr, const unsigned short port_num,
          SocketInterface* socket_obj_ptr = NULL)
      : host(host_addr), port(port_num), sock(-1) {
    s3_log(S3_LOG_DEBUG, "", "Constructor\n");
    metrics_whitelist.clear();
    if (socket_obj_ptr) {
      socket_obj.reset(socket_obj_ptr);
    } else {
      socket_obj.reset(new SocketWrapper());
    }
  }

  int init();
  void finish();

  // Load & parse the whitelist file
  int load_whitelist();

  // Check if metric is present in whitelist
  bool is_allowed_to_publish(const std::string& key) {
    return metrics_whitelist.find(key) != metrics_whitelist.end();
  }

  // Send message to server
  int send(const std::string& msg, int retry = 1);

  // Form a message in StatsD format and send it to server
  int form_and_send_msg(const std::string& key, const std::string& type,
                        const std::string& value, int retry, float sample_rate);

  // Returns true if two float numbers are (alomst) equal
  bool is_fequal(float x, float y) {
    return (fabsf(x - y) < std::numeric_limits<float>::epsilon());
  }

  // Returns true if key name is valid (doesn't have chars: `@`, `|`, or `:`)
  bool is_keyname_valid(const std::string& key) {
    std::size_t found = key.find_first_of("@|:");
    return (found == std::string::npos);
  }

  static S3Stats* stats_instance;

  // StatsD host address & port number
  std::string host;
  unsigned short port;

  int sock;
  struct sockaddr_in server;
  std::unique_ptr<SocketInterface> socket_obj;

  // metrics whitelist
  std::unordered_set<std::string> metrics_whitelist;

  FRIEND_TEST(S3StatsTest, Init);
  FRIEND_TEST(S3StatsTest, Whitelist);
  FRIEND_TEST(S3StatsTest, S3StatsSendMustSucceedIfSocketSendToSucceeds);
  FRIEND_TEST(S3StatsTest, S3StatsSendMustRetryAndFailIfRetriesFail);
};

extern S3Option* g_option_instance;
extern S3Stats* g_stats_instance;

// Utility Wrappers for StatsD
int s3_stats_init();
void s3_stats_fini();

static inline int s3_stats_inc(const std::string& key, int retry = 1,
                               float sample_rate = 1.0) {
  if (!g_option_instance->is_stats_enabled()) {
    return 0;
  }
  return g_stats_instance->count(key, 1, retry, sample_rate);
}

static inline int s3_stats_dec(const std::string& key, int retry = 1,
                               float sample_rate = 1.0) {
  if (!g_option_instance->is_stats_enabled()) {
    return 0;
  }
  return g_stats_instance->count(key, -1, retry, sample_rate);
}

static inline int s3_stats_count(const std::string& key, int64_t value,
                                 int retry = 1, float sample_rate = 1.0) {
  if (!g_option_instance->is_stats_enabled()) {
    return 0;
  }
  return g_stats_instance->count(key, value, retry, sample_rate);
}

int s3_stats_timing(const std::string& key, size_t value, int retry = 1,
                    float sample_rate = 1.0);

static inline int s3_stats_set_gauge(const std::string& key, int value,
                                     int retry = 1) {
  if (!g_option_instance->is_stats_enabled()) {
    return 0;
  }
  return g_stats_instance->set_gauge(key, value, retry);
}

static inline int s3_stats_update_gauge(const std::string& key, int value,
                                        int retry = 1) {
  if (!g_option_instance->is_stats_enabled()) {
    return 0;
  }
  return g_stats_instance->update_gauge(key, value, retry);
}

static inline int s3_stats_count_unique(const std::string& key,
                                        const std::string& value,
                                        int retry = 1) {
  if (!g_option_instance->is_stats_enabled()) {
    return 0;
  }
  return g_stats_instance->count_unique(key, value, retry);
}

#endif  // __S3_SERVER_STATS_H__
