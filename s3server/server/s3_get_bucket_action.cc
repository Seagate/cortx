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

#include "s3_error_codes.h"
#include "s3_get_bucket_action.h"
#include "s3_iem.h"
#include "s3_log.h"
#include "s3_object_metadata.h"
#include "s3_option.h"
#include "s3_common_utilities.h"

S3GetBucketAction::S3GetBucketAction(
    std::shared_ptr<S3RequestObject> req, std::shared_ptr<ClovisAPI> clovis_api,
    std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kvs_reader_factory,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory,
    std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory)
    : S3BucketAction(req, bucket_meta_factory),
      object_list(req->get_query_string_value("encoding-type")),
      last_key(""),
      fetch_successful(false) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  s3_log(S3_LOG_INFO, request_id, "S3 API: Get Bucket(List Objects).\n");

  if (clovis_api) {
    s3_clovis_api = clovis_api;
  } else {
    s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
  }
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
  if (object_meta_factory) {
    object_metadata_factory = object_meta_factory;
  } else {
    object_metadata_factory = std::make_shared<S3ObjectMetadataFactory>();
  }

  setup_steps();
  // TODO request param validations
}

void S3GetBucketAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3GetBucketAction::validate_request, this);
  ACTION_TASK_ADD(S3GetBucketAction::get_next_objects, this);
  ACTION_TASK_ADD(S3GetBucketAction::send_response_to_s3_client, this);
  // ...
}
void S3GetBucketAction::fetch_bucket_info_failed() {
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
}
void S3GetBucketAction::validate_request() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  object_list.set_bucket_name(request->get_bucket_name());
  request_prefix = request->get_query_string_value("prefix");
  object_list.set_request_prefix(request_prefix);
  s3_log(S3_LOG_DEBUG, request_id, "prefix = %s\n", request_prefix.c_str());

  request_delimiter = request->get_query_string_value("delimiter");
  object_list.set_request_delimiter(request_delimiter);
  s3_log(S3_LOG_DEBUG, request_id, "delimiter = %s\n",
         request_delimiter.c_str());

  request_marker_key = request->get_query_string_value("marker");
  if (!request_marker_key.empty()) {
    object_list.set_request_marker_key(request_marker_key);
  }
  s3_log(S3_LOG_DEBUG, request_id, "request_marker_key = %s\n",
         request_marker_key.c_str());

  last_key = request_marker_key;  // as requested by user
  std::string max_k = request->get_query_string_value("max-keys");
  if (max_k.empty()) {
    max_keys = 1000;
    object_list.set_max_keys("1000");
  } else {
    if (!S3CommonUtilities::stoul(max_k, max_keys)) {
      s3_log(S3_LOG_DEBUG, request_id, "invalid max-keys = %s\n",
             max_k.c_str());
      set_s3_error("InvalidArgument");
      send_response_to_s3_client();
      return;
    }
    object_list.set_max_keys(max_k);
  }
  s3_log(S3_LOG_DEBUG, request_id, "max-keys = %s\n", max_k.c_str());
  next();
}

void S3GetBucketAction::get_next_objects() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  size_t count = S3Option::get_instance()->get_clovis_idx_fetch_count();
  m0_uint128 object_list_index_oid =
      bucket_metadata->get_object_list_index_oid();
  clovis_kv_reader = s3_clovis_kvs_reader_factory->create_clovis_kvs_reader(
      request, s3_clovis_api);

  if (max_keys == 0) {
    // as requested max_keys is 0
    // Go ahead and respond.
    fetch_successful = true;
    send_response_to_s3_client();
  } else if (object_list_index_oid.u_hi == 0ULL &&
             object_list_index_oid.u_lo == 0ULL) {
    fetch_successful = true;
    send_response_to_s3_client();
  } else {
    // We pass M0_OIF_EXCLUDE_START_KEY flag to Clovis. This flag skips key that
    // is passed during listing of all keys. If this flag is not passed then
    // input key is returned in result.
    clovis_kv_reader->next_keyval(
        object_list_index_oid, last_key, count,
        std::bind(&S3GetBucketAction::get_next_objects_successful, this),
        std::bind(&S3GetBucketAction::get_next_objects_failed, this));
  }

  // for shutdown testcases, check FI and set shutdown signal
  S3_CHECK_FI_AND_SET_SHUTDOWN_SIGNAL(
      "get_bucket_action_get_next_objects_shutdown_fail");
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetBucketAction::get_next_objects_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  s3_log(S3_LOG_DEBUG, request_id, "Found Object listing\n");
  m0_uint128 object_list_index_oid =
      bucket_metadata->get_object_list_index_oid();
  bool atleast_one_json_error = false;
  auto& kvps = clovis_kv_reader->get_key_values();
  size_t length = kvps.size();
  for (auto& kv : kvps) {
    s3_log(S3_LOG_DEBUG, request_id, "Read Object = %s\n", kv.first.c_str());
    s3_log(S3_LOG_DEBUG, request_id, "Read Object Value = %s\n",
           kv.second.second.c_str());
    auto object = object_metadata_factory->create_object_metadata_obj(request);
    size_t delimiter_pos = std::string::npos;
    if (request_prefix.empty() && request_delimiter.empty()) {
      if (object->from_json(kv.second.second) != 0) {
        atleast_one_json_error = true;
        s3_log(S3_LOG_ERROR, request_id,
               "Json Parsing failed. Index oid = "
               "%" SCNx64 " : %" SCNx64 ", Key = %s, Value = %s\n",
               object_list_index_oid.u_hi, object_list_index_oid.u_lo,
               kv.first.c_str(), kv.second.second.c_str());
      } else {
        object_list.add_object(object);
      }
    } else if (!request_prefix.empty() && request_delimiter.empty()) {
      // Filter out by prefix
      if (kv.first.find(request_prefix) == 0) {
        if (object->from_json(kv.second.second) != 0) {
          atleast_one_json_error = true;
          s3_log(S3_LOG_ERROR, request_id,
                 "Json Parsing failed. Index oid = "
                 "%" SCNx64 " : %" SCNx64 ", Key = %s, Value = %s\n",
                 object_list_index_oid.u_hi, object_list_index_oid.u_lo,
                 kv.first.c_str(), kv.second.second.c_str());
        } else {
          object_list.add_object(object);
        }
      }
    } else if (request_prefix.empty() && !request_delimiter.empty()) {
      delimiter_pos = kv.first.find(request_delimiter);
      if (delimiter_pos == std::string::npos) {
        if (object->from_json(kv.second.second) != 0) {
          atleast_one_json_error = true;
          s3_log(S3_LOG_ERROR, request_id,
                 "Json Parsing failed. Index oid = "
                 "%" SCNx64 " : %" SCNx64 ", Key = %s, Value = %s\n",
                 object_list_index_oid.u_hi, object_list_index_oid.u_lo,
                 kv.first.c_str(), kv.second.second.c_str());
        } else {
          object_list.add_object(object);
        }
      } else {
        // Roll up
        s3_log(S3_LOG_DEBUG, request_id,
               "Delimiter %s found at pos %zu in string %s\n",
               request_delimiter.c_str(), delimiter_pos, kv.first.c_str());
        object_list.add_common_prefix(kv.first.substr(0, delimiter_pos + 1));
      }
    } else {
      // both prefix and delimiter are not empty
      bool prefix_match = (kv.first.find(request_prefix) == 0) ? true : false;
      if (prefix_match) {
        delimiter_pos =
            kv.first.find(request_delimiter, request_prefix.length());
        if (delimiter_pos == std::string::npos) {
          if (object->from_json(kv.second.second) != 0) {
            atleast_one_json_error = true;
            s3_log(S3_LOG_ERROR, request_id.c_str(),
                   "Json Parsing failed. Index oid = "
                   "%" SCNx64 " : %" SCNx64 ", Key = %s, Value = %s\n",
                   object_list_index_oid.u_hi, object_list_index_oid.u_lo,
                   kv.first.c_str(), kv.second.second.c_str());
          } else {
            object_list.add_object(object);
          }
        } else {
          s3_log(S3_LOG_DEBUG, request_id,
                 "Delimiter %s found at pos %zu in string %s\n",
                 request_delimiter.c_str(), delimiter_pos, kv.first.c_str());
          object_list.add_common_prefix(kv.first.substr(0, delimiter_pos + 1));
        }
      }  // else no prefix match, filter it out
    }

    if (--length == 0 || object_list.size() == max_keys) {
      // this is the last element returned or we reached limit requested
      last_key = kv.first;
      break;
    }
  }

  if (atleast_one_json_error) {
    s3_iem(LOG_ERR, S3_IEM_METADATA_CORRUPTED, S3_IEM_METADATA_CORRUPTED_STR,
           S3_IEM_METADATA_CORRUPTED_JSON);
  }

  // We ask for more if there is any.
  size_t count_we_requested =
      S3Option::get_instance()->get_clovis_idx_fetch_count();

  if ((object_list.size() == max_keys) || (kvps.size() < count_we_requested)) {
    // Go ahead and respond.
    if (object_list.size() == max_keys) {
      object_list.set_response_is_truncated(true);
      object_list.set_next_marker_key(last_key);
    }
    fetch_successful = true;
    send_response_to_s3_client();
  } else {
    get_next_objects();
  }
}

void S3GetBucketAction::get_next_objects_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (clovis_kv_reader->get_state() == S3ClovisKVSReaderOpState::missing) {
    s3_log(S3_LOG_DEBUG, request_id, "No Objects found in Object listing\n");
    fetch_successful = true;  // With no entries.
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Bucket metadata next keyval operation failed due to pre launch "
           "failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    s3_log(S3_LOG_DEBUG, request_id, "Failed to find Object listing\n");
    set_s3_error("InternalError");
    fetch_successful = false;
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetBucketAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (reject_if_shutting_down() ||
      (is_error_state() && !get_s3_error_code().empty())) {
    s3_log(S3_LOG_DEBUG, request_id, "Sending %s response...\n",
           get_s3_error_code().c_str());
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
    std::string& response_xml = object_list.get_xml(
        request->get_canonical_id(), bucket_metadata->get_owner_id(),
        request->get_user_id());
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));
    request->set_out_header_value("Content-Type", "application/xml");
    s3_log(S3_LOG_DEBUG, request_id, "Object list response_xml = %s\n",
           response_xml.c_str());

    request->send_response(S3HttpSuccess200, response_xml);
  } else {
    S3Error error("InternalError", request->get_request_id(),
                  request->get_bucket_name());
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
