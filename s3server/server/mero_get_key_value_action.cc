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

#include "mero_get_key_value_action.h"
#include "s3_error_codes.h"
#include "s3_m0_uint128_helper.h"

MeroGetKeyValueAction::MeroGetKeyValueAction(
    std::shared_ptr<MeroRequestObject> req,
    std::shared_ptr<ClovisAPI> clovis_api,
    std::shared_ptr<S3ClovisKVSReaderFactory> clovis_mero_kvs_reader_factory)
    : MeroAction(req) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor");
  if (clovis_api) {
    mero_clovis_api = clovis_api;
  } else {
    mero_clovis_api = std::make_shared<ConcreteClovisAPI>();
  }

  if (clovis_mero_kvs_reader_factory) {
    clovis_kvs_reader_factory = clovis_mero_kvs_reader_factory;
  } else {
    clovis_kvs_reader_factory = std::make_shared<S3ClovisKVSReaderFactory>();
  }

  setup_steps();
}

void MeroGetKeyValueAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(MeroGetKeyValueAction::fetch_key_value, this);
  ACTION_TASK_ADD(MeroGetKeyValueAction::send_response_to_s3_client, this);
  // ...
}

void MeroGetKeyValueAction::fetch_key_value() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  index_id = S3M0Uint128Helper::to_m0_uint128(request->get_index_id_lo(),
                                              request->get_index_id_hi());
  // invalid oid
  if (index_id.u_hi == 0ULL && index_id.u_lo == 0ULL) {
    set_s3_error("BadRequest");
    send_response_to_s3_client();
  } else {
    clovis_kv_reader = clovis_kvs_reader_factory->create_clovis_kvs_reader(
        request, mero_clovis_api);
    clovis_kv_reader->get_keyval(
        index_id, request->get_key_name(),
        std::bind(&MeroGetKeyValueAction::fetch_key_value_successful, this),
        std::bind(&MeroGetKeyValueAction::fetch_key_value_failed, this));
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void MeroGetKeyValueAction::fetch_key_value_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void MeroGetKeyValueAction::fetch_key_value_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (clovis_kv_reader->get_state() == S3ClovisKVSReaderOpState::missing) {
    set_s3_error("NoSuchKey");
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Failed to retrive the key, due to pre launch failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void MeroGetKeyValueAction::send_response_to_s3_client() {
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
    request->send_response(S3HttpSuccess200, clovis_kv_reader->get_value());
  }
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
