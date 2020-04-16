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
 * Original author:  Abhilekh Mustapure   <abhilekh.mustapure@seagate.com>
 * Original creation date: 15-Jan-2019
*/

#include "s3_delete_bucket_tagging_action.h"
#include "s3_error_codes.h"
#include "s3_log.h"

S3DeleteBucketTaggingAction::S3DeleteBucketTaggingAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory)
    : S3BucketAction(std::move(req), std::move(bucket_meta_factory)) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  s3_log(S3_LOG_INFO, request_id, "S3 API: Delete Bucket Tagging. Bucket[%s]\n",
         request->get_bucket_name().c_str());

  setup_steps();
}

void S3DeleteBucketTaggingAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3DeleteBucketTaggingAction::delete_bucket_tags, this);
  ACTION_TASK_ADD(S3DeleteBucketTaggingAction::send_response_to_s3_client,
                  this);
  // ...
}

void S3DeleteBucketTaggingAction::fetch_bucket_info_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (bucket_metadata->get_state() == S3BucketMetadataState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
  } else if (bucket_metadata->get_state() == S3BucketMetadataState::missing) {
    set_s3_error("NoSuchBucket");
  } else {
    set_s3_error("InternalError");
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  send_response_to_s3_client();
}

void S3DeleteBucketTaggingAction::delete_bucket_tags() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  bucket_metadata->delete_bucket_tags();
  bucket_metadata->save(
      std::bind(&S3DeleteBucketTaggingAction::next, this),
      std::bind(&S3DeleteBucketTaggingAction::delete_bucket_tags_failed, this));

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketTaggingAction::delete_bucket_tags_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (bucket_metadata->get_state() == S3BucketMetadataState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
  } else {
    set_s3_error("InternalError");
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  next();
}

void S3DeleteBucketTaggingAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (reject_if_shutting_down() ||
      (is_error_state() && !get_s3_error_code().empty())) {
    S3Error error(get_s3_error_code(), request->get_request_id(),
                  request->get_object_uri());
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
    request->send_response(S3HttpSuccess204);
  }

  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
