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

#include "s3_stats.h"
#include <string.h>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iomanip>
#include <sstream>

S3Stats* g_stats_instance = NULL;
S3Stats* S3Stats::stats_instance = NULL;

S3Stats* S3Stats::get_instance(SocketInterface* socket_obj) {
  if (!stats_instance) {
    stats_instance =
        new S3Stats(g_option_instance->get_statsd_ip_addr(),
                    g_option_instance->get_statsd_port(), socket_obj);
    if (stats_instance->init() != 0) {
      delete stats_instance;
      stats_instance = NULL;
    }
  }
  return stats_instance;
}

void S3Stats::delete_instance() {
  if (stats_instance) {
    stats_instance->finish();
    delete stats_instance;
    stats_instance = NULL;
  }
}

int S3Stats::count(const std::string& key, int64_t value, int retry,
                   float sample_rate) {
  return form_and_send_msg(key, "c", std::to_string(value), retry, sample_rate);
}

int S3Stats::timing(const std::string& key, size_t time_ms, int retry,
                    float sample_rate) {
  return form_and_send_msg(key, "ms", std::to_string(time_ms), retry,
                           sample_rate);
}

int S3Stats::set_gauge(const std::string& key, int value, int retry) {
  if (value < 0) {
    s3_log(S3_LOG_ERROR, "",
           "Invalid gauge value: Initial gauge value can not be negative. Key "
           "[%s], Value[%d]\n",
           key.c_str(), value);
    errno = EINVAL;
    return -1;
  } else {
    return form_and_send_msg(key, "g", std::to_string(value), retry, 1.0);
  }
}

int S3Stats::update_gauge(const std::string& key, int value, int retry) {
  std::string value_str;
  if (value >= 0) {
    value_str = "+" + std::to_string(value);
  } else {
    value_str = std::to_string(value);
  }
  return form_and_send_msg(key, "g", value_str, retry, 1.0);
}

int S3Stats::count_unique(const std::string& key, const std::string& value,
                          int retry) {
  return form_and_send_msg(key, "s", value, retry, 1.0);
}

int S3Stats::load_whitelist() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  std::string whitelist_filename =
      g_option_instance->get_stats_whitelist_filename();
  s3_log(S3_LOG_DEBUG, "", "Loading whitelist file: %s\n",
         whitelist_filename.c_str());
  std::ifstream fstream(whitelist_filename.c_str());
  if (!fstream.good()) {
    s3_log(S3_LOG_ERROR, "", "Stats whitelist file does not exist: %s\n",
           whitelist_filename.c_str());
    return -1;
  }
  try {
    YAML::Node root_node = YAML::LoadFile(whitelist_filename);
    if (root_node.IsNull()) {
      s3_log(S3_LOG_DEBUG, "", "Stats whitelist file is empty: %s\n",
             whitelist_filename.c_str());
      return 0;
    }
    for (auto it = root_node.begin(); it != root_node.end(); ++it) {
      metrics_whitelist.insert(it->as<std::string>());
    }
  } catch (const YAML::RepresentationException& e) {
    s3_log(S3_LOG_ERROR, "", "YAML::RepresentationException caught: %s\n",
           e.what());
    s3_log(S3_LOG_ERROR, "", "Yaml file [%s] is incorrect\n",
           whitelist_filename.c_str());
    return -1;
  } catch (const YAML::ParserException& e) {
    s3_log(S3_LOG_ERROR, "", "YAML::ParserException caught: %s\n", e.what());
    s3_log(S3_LOG_ERROR, "", "Parsing Error in yaml file %s\n",
           whitelist_filename.c_str());
    return -1;
  } catch (const YAML::EmitterException& e) {
    s3_log(S3_LOG_ERROR, "", "YAML::EmitterException caught: %s\n", e.what());
    return -1;
  } catch (YAML::Exception& e) {
    s3_log(S3_LOG_ERROR, "", "YAML::Exception caught: %s\n", e.what());
    return -1;
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return 0;
}

int S3Stats::init() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  if (load_whitelist() == -1) {
    s3_log(S3_LOG_ERROR, "", "load_whitelist failed parsing the file: %s\n",
           g_option_instance->get_stats_whitelist_filename().c_str());
    return -1;
  }
  sock = socket_obj->socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == -1) {
    s3_log(S3_LOG_ERROR, "", "socket call failed: %s\n", strerror(errno));
    return -1;
  }
  int flags = socket_obj->fcntl(sock, F_GETFL, 0);
  if (flags == -1) {
    s3_log(S3_LOG_ERROR, "", "fcntl [cmd = F_GETFL] call failed: %s\n",
           strerror(errno));
    return -1;
  }
  if (socket_obj->fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
    s3_log(S3_LOG_ERROR, "", "fcntl [cmd = F_SETFL] call failed: %s\n",
           strerror(errno));
    return -1;
  }
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  if (socket_obj->inet_aton(host.c_str(), &server.sin_addr) == 0) {
    s3_log(S3_LOG_ERROR, "", "inet_aton call failed for host [%s]\n",
           host.c_str());
    errno = EINVAL;
    return -1;
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return 0;
}

void S3Stats::finish() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  if (sock != -1) {
    socket_obj->close(sock);
    sock = -1;
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

int S3Stats::send(const std::string& msg, int retry) {
  s3_log(S3_LOG_DEBUG, "", "msg: %s\n", msg.c_str());
  if (retry > g_option_instance->get_statsd_max_send_retry()) {
    retry = g_option_instance->get_statsd_max_send_retry();
  } else if (retry < 0) {
    retry = 0;
  }
  while (1) {
    if (socket_obj->sendto(sock, msg.c_str(), msg.length(), 0,
                           (struct sockaddr*)&server, sizeof(server)) == -1) {
      if ((errno == EAGAIN || errno == EWOULDBLOCK) && retry > 0) {
        retry--;
        continue;
      } else {
        s3_log(S3_LOG_ERROR, "", "sendto call failed: msg [%s], error [%s]\n",
               msg.c_str(), strerror(errno));
        return -1;
      }
    } else {
      break;
    }
  }
  return 0;
}

int S3Stats::form_and_send_msg(const std::string& key, const std::string& type,
                               const std::string& value, int retry,
                               float sample_rate) {
  // validate key/value name
  assert(is_keyname_valid(key));
  assert(is_keyname_valid(value));

  // check if metric is present in the whitelist
  if (!is_allowed_to_publish(key)) {
    s3_log(S3_LOG_DEBUG, "", "Metric not found in whitelist: [%s]\n",
           key.c_str());
    return 0;
  }

  // Form message in StatsD format:
  // <metricname>:<value>|<type>[|@<sampling rate>]
  std::string buf;
  if ((type == "s") || (type == "g")) {
    buf = key + ":" + value + "|" + type;
  } else if ((type == "c") || (type == "ms")) {
    if (is_fequal(sample_rate, 1.0)) {
      buf = key + ":" + value + "|" + type;
    } else {
      std::stringstream str_stream;
      str_stream << std::fixed << std::setprecision(2) << sample_rate;
      buf = key + ":" + value + "|" + type + "|@" + str_stream.str();
    }
  } else {
    s3_log(S3_LOG_ERROR, "", "Invalid type: key [%s], type [%s]\n", key.c_str(),
           type.c_str());
    errno = EINVAL;
    return -1;
  }

  return send(buf, retry);
}

int s3_stats_init() {
  if (!g_option_instance->is_stats_enabled()) {
    return 0;
  }
  if (!g_stats_instance) {
    g_stats_instance = S3Stats::get_instance();
  }
  if (g_stats_instance) {
    return 0;
  } else {
    return -1;
  }
}

void s3_stats_fini() {
  if (!g_option_instance->is_stats_enabled()) {
    return;
  }
  if (g_stats_instance) {
    S3Stats::delete_instance();
    g_stats_instance = NULL;
  }
}

int s3_stats_timing(const std::string& key, size_t value, int retry,
                    float sample_rate) {

  if (!g_option_instance->is_stats_enabled()) {
    return 0;
  }
  if (value == (size_t)(-1)) {
    s3_log(S3_LOG_ERROR, "", "Invalid time value for key %s\n", key.c_str());
    errno = EINVAL;
    return -1;
  }
  return g_stats_instance->timing(key, value, retry, sample_rate);
}
