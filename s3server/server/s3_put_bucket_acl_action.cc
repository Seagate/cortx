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
 * Original creation date: 19-May-2016
 */

#include "s3_put_bucket_acl_action.h"
#include "s3_error_codes.h"
#include "s3_log.h"
#include "base64.h"

S3PutBucketACLAction::S3PutBucketACLAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory)
    : S3BucketAction(std::move(req), std::move(bucket_meta_factory)) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  s3_log(S3_LOG_INFO, request_id, "S3 API: Put Bucket Acl. Bucket[%s]\n",
         request->get_bucket_name().c_str());
  setup_steps();
}

void S3PutBucketACLAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3PutBucketACLAction::validate_request, this);
  ACTION_TASK_ADD(S3PutBucketACLAction::validate_acl_with_auth, this);
  ACTION_TASK_ADD(S3PutBucketACLAction::setacl, this);
  ACTION_TASK_ADD(S3PutBucketACLAction::send_response_to_s3_client, this);
}

void S3PutBucketACLAction::validate_request() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (request->has_all_body_content()) {
    user_input_acl = request->get_full_body_content_as_string();
    next();
  } else {
    // Start streaming, logically pausing action till we get data.
    request->listen_for_incoming_data(
        std::bind(&S3PutBucketACLAction::consume_incoming_content, this),
        request->get_data_length() /* we ask for all */
        );
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketACLAction::consume_incoming_content() {
  s3_log(S3_LOG_DEBUG, request_id, "Consume data\n");
  if (request->is_s3_client_read_error()) {
    client_read_error();
  } else if (request->has_all_body_content()) {
    user_input_acl = request->get_full_body_content_as_string();
    next();
  } else {
    // else just wait till entire body arrives. rare.
    request->resume();
  }
}

void S3PutBucketACLAction::validate_acl_with_auth() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (user_input_acl.empty()) {
    next();
  } else {
    auth_client->set_validate_acl(user_input_acl);

    auth_client->validate_acl(
        std::bind(&S3PutBucketACLAction::on_aclvalidation_success, this),
        std::bind(&S3PutBucketACLAction::on_aclvalidation_failure, this));
  }
}

void S3PutBucketACLAction::on_aclvalidation_success() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketACLAction::on_aclvalidation_failure() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  std::string error_code = auth_client->get_error_code();
  if (error_code == "InvalidID") {
    set_s3_error("InvalidArgument");
  } else {
    set_s3_error(error_code);
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketACLAction::fetch_bucket_info_failed() {
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

void S3PutBucketACLAction::setacl() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  check_shutdown_signal_for_next_task(false);

  if (!user_input_acl.empty()) {
    s3_log(S3_LOG_DEBUG, "", "Saving client uploaded ACL\n");
    bucket_metadata->setacl(base64_encode(
        (const unsigned char*)user_input_acl.c_str(), user_input_acl.size()));

  } else {
    std::string auth_generated_acl = request->get_default_acl();
    bucket_metadata->setacl(auth_generated_acl);
  }

  bucket_metadata->save(std::bind(&S3PutBucketACLAction::next, this),
                        std::bind(&S3PutBucketACLAction::setacl_failed, this));

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketACLAction::setacl_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_ERROR, request_id, "setting acl failed\n");
  if (bucket_metadata->get_state() == S3BucketMetadataState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
  } else if (bucket_metadata->get_state() == S3BucketMetadataState::missing) {
    set_s3_error("NoSuchBucket");
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketACLAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (reject_if_shutting_down() ||
      (is_error_state() && !get_s3_error_code().empty())) {
    S3Error error(get_s3_error_code(), request->get_request_id(),
                  request->get_object_uri());
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));
    if (get_s3_error_code() == "ServiceUnavailable") {
      request->set_out_header_value("Retry-After", "1");
    }

    request->send_response(error.get_http_status_code(), response_xml);
  } else {
    request->send_response(S3HttpSuccess200);
  }
  S3_RESET_SHUTDOWN_SIGNAL;  // for shutdown testcases
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
