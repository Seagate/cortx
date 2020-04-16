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
 * Original author:  Dattaprasad Govekar   <dattaprasad.govekar@seagate.com>
 * Original creation date: 06-Aug-2019
 */

#include "mero_head_object_action.h"
#include "s3_error_codes.h"
#include "s3_common_utilities.h"
#include "s3_m0_uint128_helper.h"

MeroHeadObjectAction::MeroHeadObjectAction(
    std::shared_ptr<MeroRequestObject> req,
    std::shared_ptr<S3ClovisReaderFactory> reader_factory)
    : MeroAction(std::move(req)), layout_id(0) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor");
  oid = {0ULL, 0ULL};
  if (reader_factory) {
    clovis_reader_factory = std::move(reader_factory);
  } else {
    clovis_reader_factory = std::make_shared<S3ClovisReaderFactory>();
  }

  setup_steps();
}

void MeroHeadObjectAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(MeroHeadObjectAction::validate_request, this);
  ACTION_TASK_ADD(MeroHeadObjectAction::check_object_exist, this);
  ACTION_TASK_ADD(MeroHeadObjectAction::send_response_to_s3_client, this);
  // ...
}

void MeroHeadObjectAction::validate_request() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  oid = S3M0Uint128Helper::to_m0_uint128(request->get_object_oid_lo(),
                                         request->get_object_oid_hi());
  // invalid oid
  if (!oid.u_hi && !oid.u_lo) {
    s3_log(S3_LOG_ERROR, request_id, "Invalid object oid\n");
    set_s3_error("BadRequest");
    send_response_to_s3_client();
  } else {
    std::string object_layout_id = request->get_query_string_value("layout-id");
    if (!S3CommonUtilities::stoi(object_layout_id, layout_id) ||
        (layout_id <= 0)) {
      s3_log(S3_LOG_ERROR, request_id, "Invalid object layout-id: %s\n",
             object_layout_id.c_str());
      set_s3_error("BadRequest");
      send_response_to_s3_client();
    } else {
      next();
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void MeroHeadObjectAction::check_object_exist() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  clovis_reader =
      clovis_reader_factory->create_clovis_reader(request, oid, layout_id);

  clovis_reader->check_object_exist(
      std::bind(&MeroHeadObjectAction::check_object_exist_success, this),
      std::bind(&MeroHeadObjectAction::check_object_exist_failure, this));

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void MeroHeadObjectAction::check_object_exist_success() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void MeroHeadObjectAction::check_object_exist_failure() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (clovis_reader->get_state() == S3ClovisReaderOpState::missing) {
    s3_log(S3_LOG_DEBUG, request_id, "Object not found\n");
    set_s3_error("NoSuchKey");
  } else if (clovis_reader->get_state() ==
             S3ClovisReaderOpState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id, "Failed to lookup object.\n");
    set_s3_error("ServiceUnavailable");
  } else {
    s3_log(S3_LOG_DEBUG, request_id, "Failed to lookup object\n");
    set_s3_error("InternalError");
  }
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void MeroHeadObjectAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (is_error_state() && !get_s3_error_code().empty()) {
    S3Error error(get_s3_error_code(), request->get_request_id(),
                  request->c_get_full_path());
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
    // Object found
    request->send_response(S3HttpSuccess200);
  }
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
