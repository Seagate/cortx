/*
 * COPYRIGHT 2015 SEAGATE LLC
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
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#include <evhttp.h>
#include <string>
#include <algorithm>

#include "s3_error_codes.h"
#include "s3_factory.h"
#include "s3_option.h"
#include "s3_common_utilities.h"
#include "s3_request_object.h"
#include "s3_stats.h"
#include "s3_audit_info_logger.h"

extern S3Option* g_option_instance;

S3RequestObject::S3RequestObject(
    evhtp_request_t* req, EvhtpInterface* evhtp_obj_ptr,
    std::shared_ptr<S3AsyncBufferOptContainerFactory> async_buf_factory,
    EventInterface* event_obj_ptr)
    : RequestObject(req, evhtp_obj_ptr, async_buf_factory, event_obj_ptr),
      s3_api_type(S3ApiType::unsupported) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  bucket_name = object_name = "";
  object_size = 0;
  // For auth disabled, use some dummy user.
  if (g_option_instance->is_auth_disabled()) {
    audit_log_obj.set_authentication_type("QueryString");
  } else {
    audit_log_obj.set_authentication_type("AuthHeader");
  }

  audit_log_obj.set_time_of_request_arrival();
}

S3RequestObject::~S3RequestObject() {
  s3_log(S3_LOG_DEBUG, request_id, "Destructor\n");
  populate_and_log_audit_info();
}

S3AuditInfo& S3RequestObject::get_audit_info() { return audit_log_obj; }

// Operation params.
std::string S3RequestObject::get_object_uri() {
  return bucket_name + "/" + object_name;
}

std::string S3RequestObject::get_action_str() { return s3_action; }

void S3RequestObject::set_bucket_name(const std::string& name) {
  bucket_name = name;
}

void S3RequestObject::set_action_str(const std::string& action) {
  s3_log(S3_LOG_INFO, request_id, "S3Action =  %s\n", action.c_str());
  s3_action = action;
}

void S3RequestObject::set_object_size(size_t obj_size) {
  object_size = obj_size;
}
const std::string& S3RequestObject::get_bucket_name() { return bucket_name; }

void S3RequestObject::set_object_name(const std::string& name) {
  object_name = name;
}

const std::string& S3RequestObject::get_object_name() { return object_name; }

void S3RequestObject::set_api_type(S3ApiType api_type) {
  s3_api_type = api_type;
}

S3ApiType S3RequestObject::get_api_type() { return s3_api_type; }

void S3RequestObject::set_operation_code(S3OperationCode operation_code) {
  s3_operation_code = operation_code;
}

S3OperationCode S3RequestObject::get_operation_code() {
  return s3_operation_code;
}

void S3RequestObject::set_default_acl(const std::string& acl) {
  default_acl = acl;
}

const std::string& S3RequestObject::get_default_acl() { return default_acl; }

void S3RequestObject::populate_and_log_audit_info() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering");
  if (S3Option::get_instance()->get_audit_logger_policy() == "disabled") {
    s3_log(S3_LOG_DEBUG, request_id,
           "Audit logger disabled by policy settings\n");
    return;
  }

  std::string s3_operation_str =
      operation_code_to_audit_str(get_operation_code());

  if (!(s3_operation_str.compare("NONE") &&
        s3_operation_str.compare("UNKNOWN"))) {
    s3_operation_str.clear();
  }

  const char* http_entry = get_http_verb_str(http_verb());
  if (!http_entry) {
    http_entry = "UNKNOWN";
  }

  std::string audit_operation_str =
      std::string("REST.") + http_entry + std::string(".") +
      api_type_to_str(get_api_type()) + s3_operation_str;

  std::string request_uri =
      http_entry + std::string(" ") + full_path_decoded_uri;

  if (!query_raw_decoded_uri.empty()) {
    request_uri = request_uri + std::string("?") + query_raw_decoded_uri;
  }

  request_uri = request_uri + std::string(" ") + http_version;

  audit_log_obj.set_turn_around_time(
      turn_around_time.elapsed_time_in_millisec());
  audit_log_obj.set_total_time(request_timer.elapsed_time_in_millisec());
  audit_log_obj.set_bytes_sent(bytes_sent);

  if (!error_code_str.empty()) {
    audit_log_obj.set_error_code(error_code_str);
  }

  // TODO
  // audit_log_obj.set_object_size(length);

  audit_log_obj.set_bucket_owner_canonical_id(get_account_id());
  audit_log_obj.set_bucket_name(bucket_name);
  audit_log_obj.set_remote_ip(get_header_value("X-Forwarded-For"));
  audit_log_obj.set_bytes_received(get_content_length());
  audit_log_obj.set_requester(get_account_id());
  audit_log_obj.set_request_id(request_id);
  audit_log_obj.set_operation(audit_operation_str);
  audit_log_obj.set_object_key(get_object_uri());
  audit_log_obj.set_request_uri(request_uri);
  audit_log_obj.set_http_status(http_status);
  audit_log_obj.set_signature_version(get_header_value("Authorization"));
  audit_log_obj.set_user_agent(get_header_value("User-Agent"));
  audit_log_obj.set_version_id(get_query_string_value("versionId"));
  // Setting object size for PUT object request
  if (object_size != 0) {
    audit_log_obj.set_object_size(object_size);
  }
  audit_log_obj.set_host_header(get_header_value("Host"));

  // Skip audit logs for health checks.
  if (audit_log_obj.get_publish_flag()) {
  if (S3AuditInfoLogger::save_msg(request_id, audit_log_obj.to_string()) < 0) {
    s3_log(S3_LOG_FATAL, request_id, "Audit Logger Error. STOP Server");
    exit(1);
  }
  }
  s3_log(S3_LOG_DEBUG, request_id, "Exiting");
}
