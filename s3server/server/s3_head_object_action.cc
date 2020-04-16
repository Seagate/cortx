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
 * Original creation date: 1-Oct-2015
 */

#include "s3_head_object_action.h"
#include "s3_error_codes.h"
#include "s3_log.h"

S3HeadObjectAction::S3HeadObjectAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory,
    std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory)
    : S3ObjectAction(std::move(req), std::move(bucket_meta_factory),
                     std::move(object_meta_factory)) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  s3_log(
      S3_LOG_INFO, request_id, "S3 API: Head Object. Bucket[%s] Object[%s]\n",
      request->get_bucket_name().c_str(), request->get_object_name().c_str());

  setup_steps();
}

void S3HeadObjectAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3HeadObjectAction::send_response_to_s3_client, this);
  // ...
}

void S3HeadObjectAction::fetch_bucket_info_failed() {
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

void S3HeadObjectAction::fetch_object_info_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (object_metadata->get_state() == S3ObjectMetadataState::present) {
    struct m0_uint128 object_list_index_oid =
        bucket_metadata->get_object_list_index_oid();
    // bypass shutdown signal check for next task
    check_shutdown_signal_for_next_task(false);

    if (object_list_index_oid.u_lo == 0ULL &&
        object_list_index_oid.u_hi == 0ULL) {
      // There is no object list index, hence object doesn't exist
      s3_log(S3_LOG_DEBUG, request_id, "Object not found\n");
      set_s3_error("NoSuchKey");
    }
  } else if (object_metadata->get_state() == S3ObjectMetadataState::missing) {
    s3_log(S3_LOG_WARN, request_id, "Object not found\n");
    set_s3_error("NoSuchKey");
  } else if (object_metadata->get_state() ==
             S3ObjectMetadataState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Object metadata load operation failed due to pre launch failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    s3_log(S3_LOG_WARN, request_id, "Failed to look up Object metadata\n");
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3HeadObjectAction::send_response_to_s3_client() {
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
  } else if (object_metadata->get_state() == S3ObjectMetadataState::present) {

    // AWS add explicit quotes "" to etag values.
    // https://docs.aws.amazon.com/AmazonS3/latest/API/API_HeadObject.html
    std::string e_tag = "\"" + object_metadata->get_md5() + "\"";

    request->set_out_header_value("Last-Modified",
                                  object_metadata->get_last_modified_gmt());
    request->set_out_header_value("ETag", e_tag);
    request->set_out_header_value("Accept-Ranges", "bytes");
    request->set_out_header_value("Content-Length",
                                  object_metadata->get_content_length_str());

    for (auto it : object_metadata->get_user_attributes()) {
      request->set_out_header_value(it.first, it.second);
    }

    request->send_response(S3HttpSuccess200);
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
