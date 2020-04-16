/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Author         :  Rajesh Nambiar        <rajesh.nambiar@seagate.com>
 * Author         :  Abrarahmed Momin   <abrar.habib@seagate.com>
 * Original creation date: 13-Jan-2016
 */

#include <string>

#include "s3_error_codes.h"
#include "s3_get_multipart_bucket_action.h"
#include "s3_iem.h"
#include "s3_log.h"
#include "s3_object_metadata.h"
#include "s3_option.h"

S3GetMultipartBucketAction::S3GetMultipartBucketAction(
    std::shared_ptr<S3RequestObject> req, std::shared_ptr<ClovisAPI> clovis_api,
    std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kvs_reader_factory,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory,
    std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory)
    : S3BucketAction(req, std::move(bucket_meta_factory)),
      multipart_object_list(req->get_query_string_value("encoding-type")),
      last_key(""),
      return_list_size(0),
      fetch_successful(false),
      last_uploadid("") {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  s3_log(S3_LOG_INFO, request_id,
         "S3 API: List Multipart Uploads. Bucket[%s]\n",
         request->get_bucket_name().c_str());
  if (clovis_api) {
    s3_clovis_api = clovis_api;
  } else {
    s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
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
  object_list_setup();
  setup_steps();
  // TODO request param validations
}

void S3GetMultipartBucketAction::object_list_setup() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  request_marker_key = request->get_query_string_value("key-marker");
  if (!request_marker_key.empty()) {
    multipart_object_list.set_request_marker_key(request_marker_key);
  }
  s3_log(S3_LOG_DEBUG, request_id, "request_marker_key = %s\n",
         request_marker_key.c_str());

  last_key = request_marker_key;  // as requested by user

  request_marker_uploadid = request->get_query_string_value("upload-id-marker");
  multipart_object_list.set_request_marker_uploadid(request_marker_uploadid);
  s3_log(S3_LOG_DEBUG, request_id, "request_marker_uploadid = %s\n",
         request_marker_uploadid.c_str());
  last_uploadid = request_marker_uploadid;

  multipart_object_list.set_bucket_name(request->get_bucket_name());
  request_prefix = request->get_query_string_value("prefix");
  multipart_object_list.set_request_prefix(request_prefix);
  s3_log(S3_LOG_DEBUG, request_id, "prefix = %s\n", request_prefix.c_str());

  request_delimiter = request->get_query_string_value("delimiter");
  multipart_object_list.set_request_delimiter(request_delimiter);
  s3_log(S3_LOG_DEBUG, request_id, "delimiter = %s\n",
         request_delimiter.c_str());

  std::string maxuploads = request->get_query_string_value("max-uploads");
  if (maxuploads.empty()) {
    max_uploads = 1000;
    multipart_object_list.set_max_uploads("1000");
  } else {
    max_uploads = std::stoul(maxuploads);
    multipart_object_list.set_max_uploads(maxuploads);
  }
  s3_log(S3_LOG_DEBUG, request_id, "max-uploads = %s\n", maxuploads.c_str());
}

void S3GetMultipartBucketAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3GetMultipartBucketAction::get_next_objects, this);
  ACTION_TASK_ADD(S3GetMultipartBucketAction::send_response_to_s3_client, this);
  // ...
}

void S3GetMultipartBucketAction::fetch_bucket_info_failed() {
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

void S3GetMultipartBucketAction::get_next_objects() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  if (bucket_metadata->get_state() != S3BucketMetadataState::present) {
    set_s3_error("NoSuchBucket");
    send_response_to_s3_client();
    return;
  }

  s3_log(S3_LOG_DEBUG, request_id,
         "Fetching next set of multipart uploads listing\n");
  struct m0_uint128 indx_oid = bucket_metadata->get_multipart_index_oid();
  if (indx_oid.u_hi == 0ULL && indx_oid.u_lo == 0ULL) {
    fetch_successful = true;
    send_response_to_s3_client();
  } else {
    size_t count = S3Option::get_instance()->get_clovis_idx_fetch_count();
    clovis_kv_reader = s3_clovis_kvs_reader_factory->create_clovis_kvs_reader(
        request, s3_clovis_api);
    clovis_kv_reader->next_keyval(
        bucket_metadata->get_multipart_index_oid(), last_key, count,
        std::bind(&S3GetMultipartBucketAction::get_next_objects_successful,
                  this),
        std::bind(&S3GetMultipartBucketAction::get_next_objects_failed, this));
  }
}

void S3GetMultipartBucketAction::get_next_objects_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  s3_log(S3_LOG_DEBUG, request_id, "Found multipart uploads listing\n");
  struct m0_uint128 indx_oid = bucket_metadata->get_multipart_index_oid();
  bool atleast_one_json_error = false;
  bool skip_marker_key = true;
  auto& kvps = clovis_kv_reader->get_key_values();
  size_t length = kvps.size();
  for (auto& kv : kvps) {
    s3_log(S3_LOG_DEBUG, request_id, "Read Object = %s\n", kv.first.c_str());
    auto object = object_metadata_factory->create_object_metadata_obj(request);
    size_t delimiter_pos = std::string::npos;

    if (object->from_json(kv.second.second) != 0) {
      atleast_one_json_error = true;
      s3_log(S3_LOG_ERROR, request_id,
             "Json Parsing failed. Index oid = "
             "%" SCNx64 " : %" SCNx64 ", Key = %s, Value = %s\n",
             indx_oid.u_hi, indx_oid.u_lo, kv.first.c_str(),
             kv.second.second.c_str());
      --length;
      continue;
    }

    if (skip_marker_key && !request_marker_uploadid.empty() &&
        !request_marker_key.empty()) {
      skip_marker_key = false;
      std::string upload_id = object->get_upload_id();
      if (!request_marker_key.compare(kv.first) &&
          !upload_id.compare(request_marker_uploadid)) {
        --length;
        continue;
      }
    }

    if (request_prefix.empty() && request_delimiter.empty()) {
      return_list_size++;
      multipart_object_list.add_object(object);
    } else if (!request_prefix.empty() && request_delimiter.empty()) {
      // Filter out by prefix
      if (kv.first.find(request_prefix) == 0) {
        return_list_size++;
        multipart_object_list.add_object(object);
      }
    } else if (request_prefix.empty() && !request_delimiter.empty()) {
      delimiter_pos = kv.first.find(request_delimiter);
      if (delimiter_pos == std::string::npos) {
        return_list_size++;
        multipart_object_list.add_object(object);
      } else {
        // Roll up
        s3_log(S3_LOG_DEBUG, request_id,
               "Delimiter %s found at pos %zu in string %s\n",
               request_delimiter.c_str(), delimiter_pos, kv.first.c_str());
        multipart_object_list.add_common_prefix(
            kv.first.substr(0, delimiter_pos + 1));
      }
    } else {
      // both prefix and delimiter are not empty
      bool prefix_match = (kv.first.find(request_prefix) == 0) ? true : false;
      if (prefix_match) {
        delimiter_pos =
            kv.first.find(request_delimiter, request_prefix.length());
        if (delimiter_pos == std::string::npos) {
          return_list_size++;
          multipart_object_list.add_object(object);
        } else {
          s3_log(S3_LOG_DEBUG, request_id,
                 "Delimiter %s found at pos %zu in string %s\n",
                 request_delimiter.c_str(), delimiter_pos, kv.first.c_str());
          multipart_object_list.add_common_prefix(
              kv.first.substr(0, delimiter_pos + 1));
        }
      }  // else no prefix match, filter it out
    }

    if (--length == 0 || return_list_size == max_uploads) {
      // this is the last element returned or we reached limit requested
      last_key = kv.first;
      last_uploadid = object->get_upload_id();
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

  if ((return_list_size == max_uploads) || (kvps.size() < count_we_requested)) {
    // Go ahead and respond.
    if (return_list_size == max_uploads) {
      multipart_object_list.set_response_is_truncated(true);
    }
    multipart_object_list.set_next_marker_key(last_key);
    multipart_object_list.set_next_marker_uploadid(last_uploadid);
    fetch_successful = true;
    send_response_to_s3_client();
  } else {
    get_next_objects();
  }
}

void S3GetMultipartBucketAction::get_next_objects_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (clovis_kv_reader->get_state() == S3ClovisKVSReaderOpState::missing) {
    s3_log(S3_LOG_DEBUG, request_id, "No more multipart uploads listing\n");
    fetch_successful = true;  // With no entries.
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Multipart metadata next keyval operation failed due to pre launch "
           "failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    s3_log(S3_LOG_DEBUG, request_id, "Failed to fetch multipart listing\n");
    set_s3_error("InternalError");
    fetch_successful = false;
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetMultipartBucketAction::send_response_to_s3_client() {
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
    std::string& response_xml = multipart_object_list.get_multiupload_xml();

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
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
