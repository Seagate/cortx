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
 * Original creation date: 1-JUNE-2019
 */

#include <string>

#include "s3_error_codes.h"
#include "mero_kvs_listing_action.h"
#include "s3_common_utilities.h"
#include "s3_iem.h"
#include "s3_log.h"
#include "s3_option.h"
#include "s3_m0_uint128_helper.h"

MeroKVSListingAction::MeroKVSListingAction(
    std::shared_ptr<MeroRequestObject> req,
    std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kvs_reader_factory)
    : MeroAction(req), last_key(""), fetch_successful(false) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  mero_clovis_api = std::make_shared<ConcreteClovisAPI>();

  s3_log(S3_LOG_INFO, request_id, "Mero API: kvs list Service.\n");

  if (clovis_kvs_reader_factory) {
    mero_clovis_kvs_reader_factory = clovis_kvs_reader_factory;
  } else {
    mero_clovis_kvs_reader_factory =
        std::make_shared<S3ClovisKVSReaderFactory>();
  }

  setup_steps();
}

void MeroKVSListingAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(MeroKVSListingAction::validate_request, this);
  ACTION_TASK_ADD(MeroKVSListingAction::get_next_key_value, this);
  ACTION_TASK_ADD(MeroKVSListingAction::send_response_to_s3_client, this);
  // ...
}

void MeroKVSListingAction::validate_request() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  index_id = S3M0Uint128Helper::to_m0_uint128(request->get_index_id_lo(),
                                              request->get_index_id_hi());
  // invalid oid check
  if (index_id.u_hi == 0ULL && index_id.u_lo == 0ULL) {
    set_s3_error("BadRequest");
    send_response_to_s3_client();
    return;
  }

  kvs_response_list.set_index_id(request->get_index_id_hi() + "-" +
                                 request->get_index_id_lo());
  request_prefix = request->get_query_string_value("prefix");
  kvs_response_list.set_request_prefix(request_prefix);
  s3_log(S3_LOG_DEBUG, request_id, "prefix = %s\n", request_prefix.c_str());

  request_delimiter = request->get_query_string_value("delimiter");
  kvs_response_list.set_request_delimiter(request_delimiter);
  s3_log(S3_LOG_DEBUG, request_id, "delimiter = %s\n",
         request_delimiter.c_str());

  request_marker_key = request->get_query_string_value("marker");
  if (!request_marker_key.empty()) {
    kvs_response_list.set_request_marker_key(request_marker_key);
  }
  s3_log(S3_LOG_DEBUG, request_id, "request_marker_key = %s\n",
         request_marker_key.c_str());

  last_key = request_marker_key;  // as requested by user
  std::string max_k = request->get_query_string_value("max-keys");
  if (max_k.empty()) {
    max_keys = 1000;
    kvs_response_list.set_max_keys("1000");
  } else {
    if (!S3CommonUtilities::stoul(max_k, max_keys)) {
      s3_log(S3_LOG_DEBUG, request_id, "invalid max-keys = %s\n",
             max_k.c_str());
      set_s3_error("InvalidArgument");
      send_response_to_s3_client();
      return;
    }
    kvs_response_list.set_max_keys(max_k);
  }
  s3_log(S3_LOG_DEBUG, request_id, "max-keys = %s\n", max_k.c_str());
  next();
}

void MeroKVSListingAction::get_next_key_value() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  size_t count = S3Option::get_instance()->get_clovis_idx_fetch_count();

  clovis_kv_reader = mero_clovis_kvs_reader_factory->create_clovis_kvs_reader(
      request, mero_clovis_api);

  if (max_keys == 0) {
    // as requested max_keys is 0
    // Go ahead and respond.
    fetch_successful = true;
    send_response_to_s3_client();
  } else {
    // We pass M0_OIF_EXCLUDE_START_KEY flag to Clovis. This flag skips key that
    // is passed during listing of all keys. If this flag is not passed then
    // input key is returned in result.
    clovis_kv_reader->next_keyval(
        index_id, last_key, count,
        std::bind(&MeroKVSListingAction::get_next_key_value_successful, this),
        std::bind(&MeroKVSListingAction::get_next_key_value_failed, this));
  }

  // for shutdown testcases, check FI and set shutdown signal
  S3_CHECK_FI_AND_SET_SHUTDOWN_SIGNAL(
      "get_kvs_listing_action_get_next_key_value_shutdown_fail");
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void MeroKVSListingAction::get_next_key_value_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  s3_log(S3_LOG_DEBUG, request_id, "Found kv listing\n");
  auto& kvps = clovis_kv_reader->get_key_values();
  size_t length = kvps.size();
  for (auto& kv : kvps) {
    s3_log(S3_LOG_DEBUG, request_id, "Read key = %s\n", kv.first.c_str());
    s3_log(S3_LOG_DEBUG, request_id, "Read Value = %s\n",
           kv.second.second.c_str());
    size_t delimiter_pos = std::string::npos;
    if (request_prefix.empty() && request_delimiter.empty()) {
      kvs_response_list.add_kv(kv.first, kv.second.second);
    } else if (!request_prefix.empty() && request_delimiter.empty()) {
      // Filter out by prefix
      if (kv.first.find(request_prefix) == 0) {
        kvs_response_list.add_kv(kv.first, kv.second.second);
      }
    } else if (request_prefix.empty() && !request_delimiter.empty()) {
      delimiter_pos = kv.first.find(request_delimiter);
      if (delimiter_pos == std::string::npos) {
        kvs_response_list.add_kv(kv.first, kv.second.second);
      } else {
        // Roll up
        s3_log(S3_LOG_DEBUG, request_id,
               "Delimiter %s found at pos %zu in string %s\n",
               request_delimiter.c_str(), delimiter_pos, kv.first.c_str());
        kvs_response_list.add_common_prefix(
            kv.first.substr(0, delimiter_pos + 1));
      }
    } else {
      // both prefix and delimiter are not empty
      bool prefix_match = (kv.first.find(request_prefix) == 0) ? true : false;
      if (prefix_match) {
        delimiter_pos =
            kv.first.find(request_delimiter, request_prefix.length());
        if (delimiter_pos == std::string::npos) {
          kvs_response_list.add_kv(kv.first, kv.second.second);
        } else {
          s3_log(S3_LOG_DEBUG, request_id,
                 "Delimiter %s found at pos %zu in string %s\n",
                 request_delimiter.c_str(), delimiter_pos, kv.first.c_str());
          kvs_response_list.add_common_prefix(
              kv.first.substr(0, delimiter_pos + 1));
        }
      }  // else no prefix match, filter it out
    }

    if (--length == 0 || kvs_response_list.size() == max_keys) {
      // this is the last element returned or we reached limit requested
      last_key = kv.first;
      break;
    }
  }

  // We ask for more if there is any.
  size_t count_we_requested =
      S3Option::get_instance()->get_clovis_idx_fetch_count();

  if ((kvs_response_list.size() == max_keys) ||
      (kvps.size() < count_we_requested)) {
    // Go ahead and respond.
    if (kvs_response_list.size() == max_keys) {
      kvs_response_list.set_response_is_truncated(true);
      kvs_response_list.set_next_marker_key(last_key);
    }
    fetch_successful = true;
    send_response_to_s3_client();
  } else {
    get_next_key_value();
  }
}

void MeroKVSListingAction::get_next_key_value_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (clovis_kv_reader->get_state() == S3ClovisKVSReaderOpState::missing) {
    s3_log(S3_LOG_DEBUG, request_id, "No keys found in kv listing\n");
    fetch_successful = true;  // With no entries.
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Next keyval operation failed due to pre launch failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    s3_log(S3_LOG_DEBUG, request_id, "Failed to find kv listing\n");
    set_s3_error("InternalError");
    fetch_successful = false;
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void MeroKVSListingAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (reject_if_shutting_down() ||
      (is_error_state() && !get_s3_error_code().empty())) {
    s3_log(S3_LOG_DEBUG, request_id, "Sending %s response...\n",
           get_s3_error_code().c_str());
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
  } else if (fetch_successful) {
    std::string response_json_str = kvs_response_list.as_json();

    request->set_out_header_value("Content-Length",
                                  std::to_string(response_json_str.length()));
    request->set_out_header_value("Content-Type", "application/json");
    s3_log(S3_LOG_DEBUG, request_id, "kv list response_json_str = %s\n",
           response_json_str.c_str());

    request->send_response(S3HttpSuccess200, response_json_str);
  } else {
    S3Error error("InternalError", request->get_request_id(),
                  request->c_get_full_path());
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));

    request->send_response(error.get_http_status_code(), response_xml);
  }
  S3_RESET_SHUTDOWN_SIGNAL;  // for shutdown testcases
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
