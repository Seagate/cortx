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
 * Original creation date: 09-January-2019
 */

#include "s3_put_bucket_tagging_action.h"
#include "s3_error_codes.h"
#include "s3_log.h"

S3PutBucketTaggingAction::S3PutBucketTaggingAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory,
    std::shared_ptr<S3PutTagsBodyFactory> bucket_body_factory)
    : S3BucketAction(std::move(req), std::move(bucket_meta_factory)) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  s3_log(S3_LOG_INFO, request_id, "S3 API: Put Bucket Tagging. Bucket[%s]\n",
         request->get_bucket_name().c_str());

  if (bucket_body_factory) {
    put_bucket_tag_body_factory = bucket_body_factory;
  } else {
    put_bucket_tag_body_factory = std::make_shared<S3PutTagsBodyFactory>();
  }
  setup_steps();
}

void S3PutBucketTaggingAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3PutBucketTaggingAction::validate_request, this);
  ACTION_TASK_ADD(S3PutBucketTaggingAction::validate_request_xml_tags, this);
  ACTION_TASK_ADD(S3PutBucketTaggingAction::save_tags_to_bucket_metadata, this);
  ACTION_TASK_ADD(S3PutBucketTaggingAction::send_response_to_s3_client, this);
  // ...
}

void S3PutBucketTaggingAction::validate_request() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (request->has_all_body_content()) {
    new_bucket_tags = request->get_full_body_content_as_string();
    validate_request_body(new_bucket_tags);
  } else {
    // Start streaming, logically pausing action till we get data.
    request->listen_for_incoming_data(
        std::bind(&S3PutBucketTaggingAction::consume_incoming_content, this),
        request->get_data_length() /* we ask for all */
        );
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketTaggingAction::consume_incoming_content() {
  s3_log(S3_LOG_INFO, request_id, "Consume data\n");
  if (request->is_s3_client_read_error()) {
    client_read_error();
  } else if (request->has_all_body_content()) {
    new_bucket_tags = request->get_full_body_content_as_string();
    validate_request_body(new_bucket_tags);
  } else {
    // else just wait till entire body arrives. rare.
    request->resume();
  }
}

void S3PutBucketTaggingAction::validate_request_body(std::string content) {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  put_bucket_tag_body =
      put_bucket_tag_body_factory->create_put_resource_tags_body(content,
                                                                 request_id);
  if (put_bucket_tag_body->isOK()) {
    bucket_tags_map = put_bucket_tag_body->get_resource_tags_as_map();
    next();
  } else {
    set_s3_error("MalformedXML");
    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketTaggingAction::validate_request_xml_tags() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  if (put_bucket_tag_body->validate_bucket_xml_tags(bucket_tags_map)) {
    next();
  } else {
    set_s3_error("InvalidTagError");
    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketTaggingAction::fetch_bucket_info_failed() {
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

void S3PutBucketTaggingAction::save_tags_to_bucket_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (bucket_metadata->get_state() == S3BucketMetadataState::present) {
    s3_log(S3_LOG_DEBUG, request_id, "Setting bucket tags =%s\n",
           new_bucket_tags.c_str());
    bucket_metadata->set_tags(bucket_tags_map);
    // bypass shutdown signal check for next task
    check_shutdown_signal_for_next_task(false);
    bucket_metadata->save(
        std::bind(&S3PutBucketTaggingAction::next, this),
        std::bind(
            &S3PutBucketTaggingAction::save_tags_to_bucket_metadata_failed,
            this));
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutBucketTaggingAction::save_tags_to_bucket_metadata_failed() {
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

void S3PutBucketTaggingAction::send_response_to_s3_client() {
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
    request->send_response(S3HttpSuccess204);
  }

  S3_RESET_SHUTDOWN_SIGNAL;  // for shutdown testcases
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
