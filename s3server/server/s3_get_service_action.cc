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

#include <string>

#include "s3_bucket_metadata.h"
#include "s3_error_codes.h"
#include "s3_get_service_action.h"
#include "s3_iem.h"
#include "s3_log.h"
#include "s3_option.h"

extern struct m0_uint128 bucket_metadata_list_index_oid;

S3GetServiceAction::S3GetServiceAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kvs_reader_factory,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory)
    : S3Action(req), last_key(""), key_prefix(""), fetch_successful(false) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  s3_clovis_api = std::make_shared<ConcreteClovisAPI>();

  s3_log(S3_LOG_INFO, request_id, "S3 API: Get Service.\n");

  if (bucket_meta_factory) {
    bucket_metadata_factory = bucket_meta_factory;
  } else {
    bucket_metadata_factory = std::make_shared<S3BucketMetadataFactory>();
  }
  if (clovis_kvs_reader_factory) {
    s3_clovis_kvs_reader_factory = clovis_kvs_reader_factory;
  } else {
    s3_clovis_kvs_reader_factory = std::make_shared<S3ClovisKVSReaderFactory>();
  }

  setup_steps();
  bucket_list_index_oid = {0ULL, 0ULL};
}

void S3GetServiceAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3GetServiceAction::initialization, this);
  ACTION_TASK_ADD(S3GetServiceAction::get_next_buckets, this);
  ACTION_TASK_ADD(S3GetServiceAction::send_response_to_s3_client, this);
  // ...
}

void S3GetServiceAction::initialization() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (!is_authorizationheader_present) {
    set_s3_error("AccessDenied");
    s3_log(S3_LOG_ERROR, request_id, "missing authorization header\n");
    send_response_to_s3_client();
  } else {
    bucket_list.set_owner_name(request->get_user_name());
    bucket_list.set_owner_id(request->get_user_id());
    // to filter keys
    key_prefix = get_search_bucket_prefix();
    // fetch the keys having account id as a prefix
    last_key = key_prefix;
    next();
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  }
}

void S3GetServiceAction::get_next_buckets() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }

  s3_log(S3_LOG_DEBUG, request_id, "Fetching bucket list from KV store\n");
  size_t count = S3Option::get_instance()->get_clovis_idx_fetch_count();

  clovis_kv_reader = s3_clovis_kvs_reader_factory->create_clovis_kvs_reader(
      request, s3_clovis_api);
  clovis_kv_reader->next_keyval(
      bucket_metadata_list_index_oid, last_key, count,
      std::bind(&S3GetServiceAction::get_next_buckets_successful, this),
      std::bind(&S3GetServiceAction::get_next_buckets_failed, this));

  // for shutdown testcases, check FI and set shutdown signal
  S3_CHECK_FI_AND_SET_SHUTDOWN_SIGNAL(
      "get_service_action_get_next_buckets_shutdown_fail");
}

void S3GetServiceAction::get_next_buckets_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  s3_log(S3_LOG_DEBUG, request_id, "Found buckets listing\n");
  auto& kvps = clovis_kv_reader->get_key_values();
  size_t length = kvps.size();
  bool atleast_one_json_error = false;
  bool retrived_all_keys = false;
  for (auto& kv : kvps) {
    // process the only keys which is having requested accountid as prefix
    if (kv.first.find(key_prefix) == std::string::npos) {
      retrived_all_keys = true;
      break;
    }
    auto bucket = bucket_metadata_factory->create_bucket_metadata_obj(request);
    if (bucket->from_json(kv.second.second) != 0) {
      atleast_one_json_error = true;
      s3_log(S3_LOG_ERROR, request_id,
             "Json Parsing failed. Index oid = "
             "%" SCNx64 " : %" SCNx64 ", Key = %s, Value = %s\n",
             bucket_list_index_oid.u_hi, bucket_list_index_oid.u_lo,
             kv.first.c_str(), kv.second.second.c_str());
    } else {
      bucket_list.add_bucket(bucket);
    }
    if (--length == 0) {
      // this is the last element returned.
      last_key = kv.first;
    }
  }
  if (atleast_one_json_error) {
    s3_iem(LOG_ERR, S3_IEM_METADATA_CORRUPTED, S3_IEM_METADATA_CORRUPTED_STR,
           S3_IEM_METADATA_CORRUPTED_JSON);
  }
  // We ask for more if there is any.
  size_t count_we_requested =
      S3Option::get_instance()->get_clovis_idx_fetch_count();
  if ((kvps.size() < count_we_requested) || retrived_all_keys) {
    // Go ahead and respond.
    fetch_successful = true;
    send_response_to_s3_client();
  } else {
    get_next_buckets();
  }
}

void S3GetServiceAction::get_next_buckets_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (clovis_kv_reader->get_state() == S3ClovisKVSReaderOpState::missing) {
    s3_log(S3_LOG_DEBUG, request_id, "Buckets list is empty\n");
    fetch_successful = true;  // With no entries.
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::failed_to_launch) {
    s3_log(
        S3_LOG_ERROR, request_id,
        "Bucket list next keyval operation failed due to pre launch failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    s3_log(S3_LOG_ERROR, request_id, "Failed to fetch bucket list info\n");
    set_s3_error("InternalError");
    fetch_successful = false;
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetServiceAction::send_response_to_s3_client() {
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
  } else if (fetch_successful) {
    std::string& response_xml = bucket_list.get_xml();
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));
    request->set_out_header_value("Content-Type", "application/xml");
    s3_log(S3_LOG_DEBUG, request_id, "Bucket list response_xml = %s\n",
           response_xml.c_str());
    request->send_response(S3HttpSuccess200, response_xml);
  } else {
    S3Error error("InternalError", request->get_request_id(), "");
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
