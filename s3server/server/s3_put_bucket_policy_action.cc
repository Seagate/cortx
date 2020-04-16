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
 * Original creation date: 16-May-2016
 */

#include <json/json.h>
#include "s3_put_bucket_policy_action.h"
#include "s3_error_codes.h"
#include "s3_log.h"
#include "base64.h"

S3PutBucketPolicyAction::S3PutBucketPolicyAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory)
    : S3BucketAction(std::move(req), std::move(bucket_meta_factory)) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  s3_log(S3_LOG_INFO, request_id, "S3 API: Put Bucket Policy. Bucket[%s]\n",
         request->get_bucket_name().c_str());

  setup_steps();
}

void S3PutBucketPolicyAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3PutBucketPolicyAction::validate_request, this);
  ACTION_TASK_ADD(S3PutBucketPolicyAction::validate_policy, this);
  ACTION_TASK_ADD(S3PutBucketPolicyAction::set_policy, this);
  ACTION_TASK_ADD(S3PutBucketPolicyAction::send_response_to_s3_client, this);
  // ...
}

void S3PutBucketPolicyAction::validate_request() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (request->has_all_body_content()) {
    new_bucket_policy = request->get_full_body_content_as_string();
    next();
    //    validate_request_body(new_bucket_policy);
  } else {
    // Start streaming, logically pausing action till we get data.
    request->listen_for_incoming_data(
        std::bind(&S3PutBucketPolicyAction::consume_incoming_content, this),
        request->get_data_length() /* we ask for all */
        );
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketPolicyAction::validate_policy() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
    auth_client->set_acl_and_policy(
        "", base64_encode((const unsigned char*)new_bucket_policy.c_str(),
                          new_bucket_policy.size()));

    auth_client->validate_policy(
        std::bind(&S3PutBucketPolicyAction::on_policy_validation_success, this),
        std::bind(&S3PutBucketPolicyAction::on_policy_validation_failure,
                  this));
}

void S3PutBucketPolicyAction::on_policy_validation_success() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketPolicyAction::on_policy_validation_failure() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  std::string error_code = auth_client->get_error_code();
  std::string error_messag = auth_client->get_error_message();
  s3_log(S3_LOG_INFO, request_id, "Auth server response = [%s]\n",
         error_code.c_str());

  set_s3_error(error_code);
  // Override error message
  set_s3_error_message(error_messag);
  send_response_to_s3_client();

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketPolicyAction::set_policy() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (bucket_metadata->get_state() == S3BucketMetadataState::present) {
    bucket_metadata->setpolicy(new_bucket_policy);
    // bypass shutdown signal check for next task
    check_shutdown_signal_for_next_task(false);
    bucket_metadata->save(
        std::bind(&S3PutBucketPolicyAction::next, this),
        std::bind(&S3PutBucketPolicyAction::set_policy_failed, this));
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketPolicyAction::set_policy_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (bucket_metadata->get_state() == S3BucketMetadataState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Save Bucket metadata operation failed due to prelaunch failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    s3_log(S3_LOG_ERROR, request_id, "Save Bucket metadata operation failed\n");
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketPolicyAction::consume_incoming_content() {
  s3_log(S3_LOG_DEBUG, request_id, "Consume data\n");
  if (request->is_s3_client_read_error()) {
    client_read_error();
  } else if (request->has_all_body_content()) {
    new_bucket_policy = request->get_full_body_content_as_string();
    next();
    // validate_request_body(new_bucket_policy);
  } else {
    // else just wait till entire body arrives. rare.
    request->resume();
  }
}

void S3PutBucketPolicyAction::validate_request_body(std::string& content) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  bool isValid = false;
  if (!new_bucket_policy.empty()) {
    Json::Value policyRoot;
    Json::Reader reader;
    isValid = reader.parse(new_bucket_policy.c_str(), policyRoot);
    if (!isValid) {
      s3_log(S3_LOG_ERROR, request_id,
             "Json Parsing failed for Bucket policy:%s\n",
             reader.getFormattedErrorMessages().c_str());
    } else {
      isValid = true;
      next();
    }
  }

  if (!isValid) {
    s3_log(S3_LOG_ERROR, request_id,
           "Bucket policy data is either empty or invalid\n");
    set_s3_error("MalformedXML");  // TODO: Check the behaviour in AWS S3
    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketPolicyAction::fetch_bucket_info_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (bucket_metadata->get_state() == S3BucketMetadataState::missing) {
    set_s3_error("NoSuchBucket");
  } else if (bucket_metadata->get_state() ==
             S3BucketMetadataState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Bucket metadata load operation failed due to pre launch failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketPolicyAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (reject_if_shutting_down()) {
    // Send response with 'Service Unavailable' code.
    s3_log(S3_LOG_DEBUG, request_id,
           "sending 'Service Unavailable' response...\n");
    S3Error error("ServiceUnavailable", request->get_request_id(),
                  request->get_object_uri());
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));
    if (get_s3_error_code() == "ServiceUnavailable" ||
        get_s3_error_code() == "InternalError") {
      request->set_out_header_value("Connection", "close");
    }

    request->set_out_header_value("Retry-After", "1");

    request->send_response(error.get_http_status_code(), response_xml);
  } else if (is_error_state() && !get_s3_error_code().empty()) {
    S3Error error(get_s3_error_code(), request->get_request_id(),
                  request->get_object_uri(), get_s3_error_message());
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));
    if (get_s3_error_code() == "ServiceUnavailable" ||
        get_s3_error_code() == "InternalError") {
      request->set_out_header_value("Connection", "close");
    }
    if (get_s3_error_code() == "ServiceUnavailable") {
      request->set_out_header_value("Retry-After", "1");
    }
    request->send_response(error.get_http_status_code(), response_xml);
  } else {
    // request->set_header_value(...)
    request->send_response(S3HttpSuccess204);
  }

  S3_RESET_SHUTDOWN_SIGNAL;  // for shutdown testcases
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

