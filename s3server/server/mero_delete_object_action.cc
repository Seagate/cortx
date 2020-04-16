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
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 30-May-2019
 */

#include "mero_delete_object_action.h"
#include "s3_error_codes.h"
#include "s3_common_utilities.h"
#include "s3_m0_uint128_helper.h"

MeroDeleteObjectAction::MeroDeleteObjectAction(
    std::shared_ptr<MeroRequestObject> req,
    std::shared_ptr<S3ClovisWriterFactory> writer_factory)
    : MeroAction(std::move(req)) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor");

  if (writer_factory) {
    clovis_writer_factory = std::move(writer_factory);
  } else {
    clovis_writer_factory = std::make_shared<S3ClovisWriterFactory>();
  }

  setup_steps();
}

void MeroDeleteObjectAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(MeroDeleteObjectAction::validate_request, this);
  ACTION_TASK_ADD(MeroDeleteObjectAction::delete_object, this);
  ACTION_TASK_ADD(MeroDeleteObjectAction::send_response_to_s3_client, this);
  // ...
}

void MeroDeleteObjectAction::validate_request() {
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
    if (!S3CommonUtilities::stoi(object_layout_id, layout_id)) {
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

void MeroDeleteObjectAction::delete_object() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  clovis_writer = clovis_writer_factory->create_clovis_writer(request, oid);
  clovis_writer->delete_object(
      std::bind(&MeroDeleteObjectAction::delete_object_successful, this),
      std::bind(&MeroDeleteObjectAction::delete_object_failed, this),
      layout_id);

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void MeroDeleteObjectAction::delete_object_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void MeroDeleteObjectAction::delete_object_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // Object missing is treated as object deleted similar to S3 object delete.
  if (clovis_writer->get_state() == S3ClovisWriterOpState::missing) {
    s3_log(S3_LOG_DEBUG, request_id,
           "Object with oid %" SCNx64 " : %" SCNx64 " is missing\n", oid.u_hi,
           oid.u_lo);
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void MeroDeleteObjectAction::send_response_to_s3_client() {
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
    request->send_response(S3HttpSuccess204);
  }
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
