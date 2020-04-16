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
 * Original author:  Rajesh Nambiar  <rajesh.nambiar@seagate.com>
 * Original creation date: 23-May-2016
 */

#include "s3_delete_bucket_policy_action.h"
#include "s3_error_codes.h"
#include "s3_log.h"

S3DeleteBucketPolicyAction::S3DeleteBucketPolicyAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory)
    : S3BucketAction(std::move(req), std::move(bucket_meta_factory), false) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  s3_log(S3_LOG_INFO, request_id,
         "S3 API: Delete Bucket Policy API. Bucket[%s]\n",
         request->get_bucket_name().c_str());

  setup_steps();
}

void S3DeleteBucketPolicyAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3DeleteBucketPolicyAction::delete_bucket_policy, this);
  ACTION_TASK_ADD(S3DeleteBucketPolicyAction::send_response_to_s3_client, this);
  // ...
}

void S3DeleteBucketPolicyAction::fetch_bucket_info_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  S3BucketMetadataState bucket_metadata_state = bucket_metadata->get_state();
  if (bucket_metadata_state == S3BucketMetadataState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Bucket metadata load operation failed due to pre launch failure\n");
    set_s3_error("ServiceUnavailable");
  } else if (bucket_metadata_state == S3BucketMetadataState::missing) {
    set_s3_error("NoSuchBucket");
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketPolicyAction::delete_bucket_policy() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (bucket_metadata->get_state() == S3BucketMetadataState::present) {
    std::string response_json = bucket_metadata->get_policy_as_json();
    if (response_json.empty()) {
      set_s3_error("NoSuchBucketPolicy");
      send_response_to_s3_client();
    } else {
      bucket_metadata->deletepolicy();
      bucket_metadata->save(
          std::bind(
              &S3DeleteBucketPolicyAction::delete_bucket_policy_successful,
              this),
          std::bind(&S3DeleteBucketPolicyAction::delete_bucket_policy_failed,
                    this));
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketPolicyAction::delete_bucket_policy_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketPolicyAction::delete_bucket_policy_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_ERROR, request_id, "Bucket policy deletion failed\n");
  if (bucket_metadata->get_state() == S3BucketMetadataState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketPolicyAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // Trigger metadata read async operation with callback
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
    request->send_response(S3HttpSuccess204);
  }
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
