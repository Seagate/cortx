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
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 16-July-2019
 */

#include <json/json.h>
#include <evhttp.h>
#include "mero_kv_list_response.h"
#include "s3_common_utilities.h"
#include "s3_log.h"

MeroKVListResponse::MeroKVListResponse(std::string encoding_type)
    : encoding_type(encoding_type),
      request_prefix(""),
      request_delimiter(""),
      request_marker_key(""),
      max_keys(""),
      response_is_truncated(false),
      next_marker_key("") {
  s3_log(S3_LOG_DEBUG, "", "Constructor\n");
}

void MeroKVListResponse::set_index_id(std::string name) { index_id = name; }

// Encoding type used by S3 to encode object key names in the XML response.
// If you specify encoding-type request parameter, S3 includes this element in
// the response, and returns encoded key name values in the following response
// elements:
// Delimiter, KeyMarker, Prefix, NextKeyMarker, Key.
std::string MeroKVListResponse::get_response_format_key_value(
    const std::string& key_value) {
  std::string format_key_value;
  if (encoding_type == "url") {
    char* decoded_str = evhttp_uriencode(key_value.c_str(), -1, 1);
    format_key_value = decoded_str;
    free(decoded_str);
  } else {
    format_key_value = key_value;
  }
  return format_key_value;
}

void MeroKVListResponse::set_request_prefix(std::string prefix) {
  request_prefix = get_response_format_key_value(prefix);
}

void MeroKVListResponse::set_request_delimiter(std::string delimiter) {
  request_delimiter = get_response_format_key_value(delimiter);
}

void MeroKVListResponse::set_request_marker_key(std::string marker) {
  request_marker_key = get_response_format_key_value(marker);
}

void MeroKVListResponse::set_max_keys(std::string count) { max_keys = count; }

void MeroKVListResponse::set_response_is_truncated(bool flag) {
  response_is_truncated = flag;
}

void MeroKVListResponse::set_next_marker_key(std::string next) {
  next_marker_key = get_response_format_key_value(next);
}

void MeroKVListResponse::add_kv(const std::string& key,
                                const std::string& value) {
  kv_list[key] = value;
}

unsigned int MeroKVListResponse::size() { return kv_list.size(); }

unsigned int MeroKVListResponse::common_prefixes_size() {
  return common_prefixes.size();
}

void MeroKVListResponse::add_common_prefix(std::string common_prefix) {
  common_prefixes.insert(common_prefix);
}

std::string MeroKVListResponse::as_json() {
  // clang-format off
  Json::Value root;
  root["Index-Id"] = index_id;
  root["Prefix"] = request_prefix;
  root["Delimiter"] = request_delimiter;
  if (encoding_type == "url") {
    root["EncodingType"] = "url";
  }
  root["Marker"] = request_marker_key;
  root["MaxKeys"] = max_keys;
  root["NextMarker"] = next_marker_key;
  root["IsTruncated"] = response_is_truncated ? "true" : "false";

  Json::Value keys_array;
  for (auto&& kv : kv_list) {
    Json::Value key_object;
    key_object["Key"] = kv.first;
    key_object["Value"] = kv.second;
    keys_array.append(key_object);
  }

  for (auto&& prefix : common_prefixes) {
    root["CommonPrefixes"] = prefix;
  }

  root["Keys"] = keys_array;

  Json::FastWriter fastWriter;
  return fastWriter.write(root);
}