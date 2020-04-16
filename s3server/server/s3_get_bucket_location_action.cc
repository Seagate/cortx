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
 * Original author:  Kaustubh Deorukhkar  <kaustubh.deorukhkar@seagate.com>
 * Original author:  Priya Saboo  <priya.chhagan@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#include "s3_get_bucket_location_action.h"
#include "s3_error_codes.h"
#include "s3_log.h"

S3GetBucketlocationAction::S3GetBucketlocationAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory)
    : S3BucketAction(std::move(req), std::move(bucket_meta_factory)) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  s3_log(S3_LOG_INFO, request_id, "S3 API: Get Bucket Location. Bucket[%s]\n",
         request->get_bucket_name().c_str());

  setup_steps();
}

void S3GetBucketlocationAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3GetBucketlocationAction::fetch_bucket_info, this);
  ACTION_TASK_ADD(S3GetBucketlocationAction::send_response_to_s3_client, this);
}

void S3GetBucketlocationAction::fetch_bucket_info_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (bucket_metadata->get_state() == S3BucketMetadataState::missing) {
    s3_log(S3_LOG_WARN, request_id, "Bucket not found\n");
    set_s3_error("NoSuchBucket");
  } else if (bucket_metadata->get_state() ==
             S3BucketMetadataState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Bucket metadata load operation failed due to pre launch failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    s3_log(S3_LOG_WARN, request_id, "Failed to fetch Bucket metadata\n");
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetBucketlocationAction::send_response_to_s3_client() {
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
  } else if (bucket_metadata->get_state() == S3BucketMetadataState::present) {
    std::string response_xml;
    response_xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    response_xml +=
        "<LocationConstraint "
        "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">";
    response_xml += bucket_metadata->get_location_constraint();
    response_xml += "</LocationConstraint>";
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));
    request->send_response(S3HttpSuccess200, response_xml);
  } else {
    S3Error error("InternalError", request->get_request_id(),
                  request->get_object_uri());
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));
    request->send_response(error.get_http_status_code(), response_xml);
  }

  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
