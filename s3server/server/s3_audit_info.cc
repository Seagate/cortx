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
 * Original author:  Siddhivinayak Shanbhag <siddhivinayak.shanbhag@seagate.com>
 * Original creation date: 27-February-2019
 */

#include "s3_audit_info.h"
#include "s3_audit_info_logger.h"
#include "s3_log.h"
#include "s3_option.h"

#include <json/json.h>
#include <time.h>
#include <regex>
#include <string>
#include <algorithm>
#include <stdio.h>

std::string audit_format_type_to_string(enum AuditFormatType type) {
  switch (type) {
    case AuditFormatType::JSON:
      return "JSON";
    case AuditFormatType::S3_FORMAT:
      return "S3_FORMAT";
    default:
      return "NONE";
  }
}

S3AuditInfo::S3AuditInfo()
    : bucket_owner_canonical_id("-"),
      bucket("-"),
      time_of_request_arrival("-"),
      remote_ip("-"),
      requester("-"),
      request_id("-"),
      operation("-"),
      object_key("-"),
      request_uri("-"),
      error_code("-"),
      bytes_sent(0),
      object_size(0),
      total_time(0),
      turn_around_time(0),
      referrer("-"),
      user_agent("-"),
      version_id("-"),
      host_id("-"),
      signature_version("-"),
      cipher_suite("-"),
      authentication_type("-"),
      host_header("-") {}

UInt64 S3AuditInfo::convert_to_unsigned(size_t audit_member) {
  if (audit_member > SIZE_MAX) {
    // Error case. Returning default values.
    return 0;
  }
  return static_cast<UInt64>(audit_member);
}

const std::string S3AuditInfo::to_string() {
  if (!S3AuditInfoLogger::is_enabled()) {
    return "";
  }

  AuditFormatType audit_format_type =
      S3Option::get_instance()->get_s3_audit_format_type();
  if (audit_format_type == AuditFormatType::S3_FORMAT) {
    // Logs audit information in S3 format.
    const std::string formatted_log =
        bucket_owner_canonical_id + " " + bucket + " " +
        time_of_request_arrival + " " + remote_ip + " " + requester + " " +
        request_id + " " + operation + " " + object_key + " \"" + request_uri +
        "\" " + std::to_string(http_status) + " " + error_code + " " +
        std::to_string(bytes_sent) + " " + std::to_string(object_size) + " " +
        std::to_string(bytes_received) + " " + std::to_string(total_time) +
        " " + std::to_string(turn_around_time) + " \"" + referrer + "\" \"" +
        user_agent + "\" " + version_id + " " + host_id + " " +
        signature_version + " " + cipher_suite + " " + authentication_type +
        " " + host_header;
    return formatted_log;
  } else if (audit_format_type == AuditFormatType::JSON) {
    // Logs audit information in Json format.
    Json::Value audit;

    audit["bucket_owner"] = bucket_owner_canonical_id;
    audit["bucket"] = bucket;
    audit["time"] = time_of_request_arrival;
    audit["remote_ip"] = remote_ip;
    audit["requester"] = requester;
    audit["request_id"] = request_id;
    audit["operation"] = operation;
    audit["key"] = object_key;
    audit["request_uri"] = request_uri;
    audit["http_status"] = http_status;
    audit["error_code"] = error_code;
    audit["bytes_sent"] = convert_to_unsigned(bytes_sent);
    audit["object_size"] = convert_to_unsigned(object_size);
    audit["bytes_received"] = convert_to_unsigned(bytes_received);
    audit["total_time"] = convert_to_unsigned(total_time);
    audit["turn_around_time"] = convert_to_unsigned(turn_around_time);
    audit["referrer"] = referrer;
    audit["user_agent"] = user_agent;
    audit["version_id"] = version_id;
    audit["host_id"] = host_id;
    audit["signature_version"] = signature_version;
    audit["cipher_suite"] = cipher_suite;
    audit["authentication_type"] = authentication_type;
    audit["host_header"] = host_header;

    Json::FastWriter fastWriter;
    return fastWriter.write(audit);
  }

  return "";
}

void S3AuditInfo::set_bucket_owner_canonical_id(
    const std::string& bucket_owner_canonical_id_str) {
  bucket_owner_canonical_id = bucket_owner_canonical_id_str;
}

void S3AuditInfo::set_bucket_name(const std::string& bucket_str) {
  bucket = bucket_str;
}

void S3AuditInfo::set_time_of_request_arrival() {
  time_t request_time;
  struct tm* request_time_struct;
  char time_of_request[REQUEST_TIME_SIZE];
  time(&request_time);
  request_time_struct = localtime(&request_time);

  // Format specifiers required for strftime() as per
  // https://docs.aws.amazon.com/AmazonS3/latest/dev/LogFormat.html :
  //   %d = Day of the month (01-31)
  //   %b = Abbreviated month name
  //   %Y = The year as a decimal number including the century.
  //   %H = Hour in 24h format (00-23)
  //   %M = Minutes in decimal ranging from 00 to 59.
  //   %S = The second as a decimal number (range 00 to 60).
  //   %z = The +hhmm or -hhmm numeric timezone

  strftime(time_of_request, sizeof(time_of_request), "[%d/%b/%Y:%H:%M:%S %z]",
           request_time_struct);
  time_of_request_arrival = time_of_request;
}

void S3AuditInfo::set_remote_ip(const std::string& remote_ip_str) {
  remote_ip = remote_ip_str;
}

void S3AuditInfo::set_requester(const std::string& requester_str) {
  requester = requester_str;
}

void S3AuditInfo::set_request_id(const std::string& request_id_str) {
  request_id = request_id_str;
}

void S3AuditInfo::set_operation(const std::string& operation_str) {
  operation = operation_str;
}

void S3AuditInfo::set_object_key(const std::string& key_str) {
  object_key = key_str;
}

void S3AuditInfo::set_request_uri(const std::string& request_uri_str) {
  request_uri = request_uri_str;
}

void S3AuditInfo::set_http_status(int httpstatus) { http_status = httpstatus; }

void S3AuditInfo::set_error_code(const std::string& error_code_str) {
  error_code = error_code_str;
}

void S3AuditInfo::set_bytes_sent(size_t total_bytes_sent) {
  bytes_sent = total_bytes_sent;
}

void S3AuditInfo::set_bytes_received(size_t total_bytes_received) {
  bytes_received = total_bytes_received;
}

void S3AuditInfo::set_object_size(size_t obj_size) { object_size = obj_size; }

void S3AuditInfo::set_total_time(size_t total_request_time) {
  total_time = total_request_time;
}

void S3AuditInfo::set_turn_around_time(size_t request_turn_around_time) {
  turn_around_time = request_turn_around_time;
}

void S3AuditInfo::set_referrer(const std::string& referrer_str) {
  referrer = referrer_str;
}

void S3AuditInfo::set_user_agent(const std::string& user_agent_str) {
  user_agent = user_agent_str;
}

void S3AuditInfo::set_version_id(const std::string& version_id_str) {
  version_id = version_id_str;
}

void S3AuditInfo::set_host_id(const std::string& host_id_str) {
  host_id = host_id_str;
}

void S3AuditInfo::set_publish_flag(bool publish_flag) {
  publish_log = publish_flag;
}

void S3AuditInfo::set_signature_version(const std::string& authorization) {
  if (!authorization.empty()) {
    std::regex auth_v2("(AWS )(.*)");
    std::regex auth_v4("(AWS4-HMAC-SHA256)(.*)");
    if (regex_match(authorization, auth_v2)) {
      signature_version = "SigV2";
    } else if (regex_match(authorization, auth_v4)) {
      signature_version = "SigV4";
    } else {
      signature_version = "Unknown_Signature";
    }
  }
}

void S3AuditInfo::set_cipher_suite(const std::string& cipher_suite_str) {
  cipher_suite = cipher_suite_str;
}

void S3AuditInfo::set_authentication_type(
    const std::string& authentication_type_str) {
  authentication_type = authentication_type_str;
}

void S3AuditInfo::set_host_header(const std::string& host_header_str) {
  host_header = host_header_str;
}

bool S3AuditInfo::get_publish_flag() { return publish_log; }