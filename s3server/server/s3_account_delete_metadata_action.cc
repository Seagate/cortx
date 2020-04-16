/*
 * COPYRIGHT 2018 SEAGATE LLC
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
 * Original creation date: 18-JUNE-2018
 */

#include <functional>

#include "s3_error_codes.h"
#include "s3_log.h"
#include "s3_iem.h"
#include "s3_account_delete_metadata_action.h"

extern struct m0_uint128 bucket_metadata_list_index_oid;

S3AccountDeleteMetadataAction::S3AccountDeleteMetadataAction(
    std::shared_ptr<S3RequestObject> req, std::shared_ptr<ClovisAPI> clovis_api,
    std::shared_ptr<S3ClovisKVSReaderFactory> kvs_reader_factory)
    : S3Action(req, true, nullptr, false, true) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  // get the account_id from uri
  account_id_from_uri = request->c_get_file_name();

  if (clovis_api) {
    s3_clovis_api = clovis_api;
  } else {
    s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
  }

  if (kvs_reader_factory) {
    clovis_kvs_reader_factory = kvs_reader_factory;
  } else {
    clovis_kvs_reader_factory = std::make_shared<S3ClovisKVSReaderFactory>();
  }
  setup_steps();
}

void S3AccountDeleteMetadataAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3AccountDeleteMetadataAction::validate_request, this);
  ACTION_TASK_ADD(S3AccountDeleteMetadataAction::fetch_first_bucket_metadata,
                  this);
  ACTION_TASK_ADD(S3AccountDeleteMetadataAction::send_response_to_s3_client,
                  this);
  // ...
}

void S3AccountDeleteMetadataAction::validate_request() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // check for the account_id got from authentication response and account_id is
  // in uri same or not
  if (account_id_from_uri == request->get_account_id()) {
    s3_log(S3_LOG_DEBUG, request_id,
           "Accound_id validation success. URI account_id: %s matches with the "
           "account_id: %s recevied from auth server.\n",
           account_id_from_uri.c_str(), request->get_account_id().c_str());
    next();  // account_id's are same, so perform next action
  } else {
    s3_log(S3_LOG_DEBUG, request_id,
           "Accound_id validation failed. URI account_id: %s does not match "
           "with the account_id: %s recevied from auth server.\n",
           account_id_from_uri.c_str(), request->get_account_id().c_str());
    set_s3_error("InvalidAccountForMgmtApi");
    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AccountDeleteMetadataAction::fetch_first_bucket_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  clovis_kv_reader = clovis_kvs_reader_factory->create_clovis_kvs_reader(
      request, s3_clovis_api);
  bucket_account_id_key_prefix = account_id_from_uri + "/";
  clovis_kv_reader->next_keyval(
      bucket_metadata_list_index_oid, bucket_account_id_key_prefix, 1,
      std::bind(&S3AccountDeleteMetadataAction::
                     fetch_first_bucket_metadata_successful,
                this),
      std::bind(
          &S3AccountDeleteMetadataAction::fetch_first_bucket_metadata_failed,
          this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AccountDeleteMetadataAction::fetch_first_bucket_metadata_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  auto& kvps = clovis_kv_reader->get_key_values();
  for (auto& kv : kvps) {
    // account_id_from_uri has buckets
    if (kv.first.find(bucket_account_id_key_prefix) != std::string::npos) {
      s3_log(S3_LOG_DEBUG, request_id,
             "Account: %s has at least one Bucket. Account delete should not "
             "be allowed.\n",
             account_id_from_uri.c_str());
      set_s3_error("AccountNotEmpty");
      send_response_to_s3_client();
    } else {  // no buckets found in given account
      next();
    }
    break;
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AccountDeleteMetadataAction::fetch_first_bucket_metadata_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (clovis_kv_reader->get_state() == S3ClovisKVSReaderOpState::missing) {
    s3_log(S3_LOG_DEBUG, request_id,
           "There is no bucket for the acocunt id: %s\n",
           account_id_from_uri.c_str());
    next();
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Bucket metadata next keyval operation failed due to pre launch "
           "failure\n");
    set_s3_error("ServiceUnavailable");
    send_response_to_s3_client();
  } else {
    s3_log(S3_LOG_ERROR, request_id, "Failed to retrieve bucket metadata\n");
    set_s3_error("InternalError");
    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AccountDeleteMetadataAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (reject_if_shutting_down() ||
      (is_error_state() && !get_s3_error_code().empty())) {
    S3Error error(get_s3_error_code(), request->get_request_id(),
                  request->get_account_id());
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));
    if (get_s3_error_code() == "ServiceUnavailable") {
      request->set_out_header_value("Retry-After", "1");
    }

    request->send_response(error.get_http_status_code(), response_xml);
  } else if (get_s3_error_code().empty()) {
    request->send_response(S3HttpSuccess204);
  }

  S3_RESET_SHUTDOWN_SIGNAL;  // for shutdown testcases
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
