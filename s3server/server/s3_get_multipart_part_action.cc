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
 * Original creation date: 13-Jan-2016
 */

#include "s3_get_multipart_part_action.h"
#include <string>
#include "s3_error_codes.h"
#include "s3_factory.h"
#include "s3_iem.h"
#include "s3_log.h"
#include "s3_option.h"
#include "s3_part_metadata.h"

S3GetMultipartPartAction::S3GetMultipartPartAction(
    std::shared_ptr<S3RequestObject> req, std::shared_ptr<ClovisAPI> clovis_api,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory,
    std::shared_ptr<S3ObjectMultipartMetadataFactory> object_mp_meta_factory,
    std::shared_ptr<S3PartMetadataFactory> part_meta_factory,
    std::shared_ptr<S3ClovisKVSReaderFactory> clovis_s3_kvs_reader_factory)
    : S3BucketAction(req, std::move(bucket_meta_factory)),
      multipart_part_list(req->get_query_string_value("encoding-type")),
      last_key(""),
      return_list_size(0),
      fetch_successful(false),
      invalid_upload_id(false) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  bucket_name = request->get_bucket_name();
  object_name = request->get_object_name();
  upload_id = request->get_query_string_value("uploadId");
  std::string maxparts = request->get_query_string_value("max-parts");
  request_marker_key = request->get_query_string_value("part-number-marker");
  if (request_marker_key.empty()) {
    request_marker_key = "0";
  }
  last_key = request_marker_key;  // as requested by user

  s3_log(S3_LOG_INFO, request_id,
         "S3 API: List Parts. Bucket[%s] Object[%s] for UploadId[%s]\
         from part-number-marker[%s] with max-parts[%s]\n",
         bucket_name.c_str(), object_name.c_str(), upload_id.c_str(),
         last_key.c_str(), maxparts.c_str());

  if (object_mp_meta_factory) {
    object_mp_metadata_factory = object_mp_meta_factory;
  } else {
    object_mp_metadata_factory =
        std::make_shared<S3ObjectMultipartMetadataFactory>();
  }

  if (clovis_s3_kvs_reader_factory) {
    clovis_kvs_reader_factory = clovis_s3_kvs_reader_factory;
  } else {
    clovis_kvs_reader_factory = std::make_shared<S3ClovisKVSReaderFactory>();
  }

  if (part_meta_factory) {
    part_metadata_factory = part_meta_factory;
  } else {
    part_metadata_factory = std::make_shared<S3PartMetadataFactory>();
  }

  if (clovis_api) {
    s3_clovis_api = clovis_api;
  } else {
    s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
  }

  multipart_part_list.set_request_marker_key(request_marker_key);
  s3_log(S3_LOG_DEBUG, request_id, "part-number-marker = %s\n",
         request_marker_key.c_str());

  multipart_oid = {0ULL, 0ULL};
  multipart_part_list.set_bucket_name(bucket_name);
  multipart_part_list.set_object_name(object_name);
  multipart_part_list.set_upload_id(upload_id);
  multipart_part_list.set_storage_class("STANDARD");

  if (maxparts.empty()) {
    max_parts = 1000;
    multipart_part_list.set_max_parts("1000");
  } else {
    max_parts = std::stoul(maxparts);
    multipart_part_list.set_max_parts(maxparts);
  }
  setup_steps();
  // TODO request param validations
}

void S3GetMultipartPartAction::setup_steps() {
  ACTION_TASK_ADD(S3GetMultipartPartAction::get_multipart_metadata, this);
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  if (!request_marker_key.empty()) {
    ACTION_TASK_ADD(S3GetMultipartPartAction::get_key_object, this);
  }
  ACTION_TASK_ADD(S3GetMultipartPartAction::get_next_objects, this);
  ACTION_TASK_ADD(S3GetMultipartPartAction::send_response_to_s3_client, this);
  // ...
}

void S3GetMultipartPartAction::fetch_bucket_info_failed() {
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
void S3GetMultipartPartAction::get_multipart_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  S3BucketMetadataState bucket_state = bucket_metadata->get_state();
  if (bucket_state == S3BucketMetadataState::present) {
    multipart_oid = bucket_metadata->get_multipart_index_oid();
    if (multipart_oid.u_hi == 0ULL && multipart_oid.u_lo == 0ULL) {
      s3_log(S3_LOG_DEBUG, request_id,
             "No such upload in progress within the bucket\n");
      set_s3_error("NoSuchUpload");
      send_response_to_s3_client();
    } else {
      object_multipart_metadata =
          object_mp_metadata_factory->create_object_mp_metadata_obj(
              request, multipart_oid, upload_id);

      object_multipart_metadata->load(
          std::bind(&S3GetMultipartPartAction::next, this),
          std::bind(&S3GetMultipartPartAction::next, this));
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetMultipartPartAction::get_key_object() {
  s3_log(S3_LOG_INFO, request_id, "Fetching part listing\n");
  S3ObjectMetadataState multipart_object_state =
      object_multipart_metadata->get_state();
  if (multipart_object_state == S3ObjectMetadataState::present) {
    if (object_multipart_metadata->get_upload_id() == upload_id) {
      clovis_kv_reader = clovis_kvs_reader_factory->create_clovis_kvs_reader(
          request, s3_clovis_api);
      clovis_kv_reader->get_keyval(
          object_multipart_metadata->get_part_index_oid(), last_key,
          std::bind(&S3GetMultipartPartAction::get_key_object_successful, this),
          std::bind(&S3GetMultipartPartAction::get_key_object_failed, this));
    } else {
      invalid_upload_id = true;
      set_s3_error("NoSuchUpload");
      send_response_to_s3_client();
    }
  } else {
    if (multipart_object_state == S3ObjectMetadataState::missing) {
      set_s3_error("NoSuchUpload");
    } else {
      set_s3_error("InternalError");
    }
    send_response_to_s3_client();
  }
}

void S3GetMultipartPartAction::get_key_object_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  std::string value;
  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  s3_log(S3_LOG_DEBUG, request_id, "Found part listing\n");
  std::string key_name = last_key;
  value = clovis_kv_reader->get_value();
  if (!(value.empty())) {
    struct m0_uint128 part_index_oid =
        object_multipart_metadata->get_part_index_oid();
    s3_log(S3_LOG_DEBUG, request_id, "Read Part = %s\n", key_name.c_str());
    std::shared_ptr<S3PartMetadata> part =
        part_metadata_factory->create_part_metadata_obj(
            request, part_index_oid, upload_id, atoi(key_name.c_str()));

    if (part->from_json(value) != 0) {
      s3_log(S3_LOG_ERROR, request_id,
             "Json Parsing failed. Index oid = "
             "%" SCNx64 " : %" SCNx64 ", Key = %s, Value = %s\n",
             part_index_oid.u_hi, part_index_oid.u_lo, key_name.c_str(),
             value.c_str());
      s3_iem(LOG_ERR, S3_IEM_METADATA_CORRUPTED, S3_IEM_METADATA_CORRUPTED_STR,
             S3_IEM_METADATA_CORRUPTED_JSON);
    } else {
      return_list_size++;
    }
  }

  if (return_list_size == max_parts) {
    // Go ahead and respond.
    multipart_part_list.set_response_is_truncated(true);
    multipart_part_list.set_next_marker_key(last_key);
    fetch_successful = true;
    send_response_to_s3_client();
  } else {
    next();
  }
}

void S3GetMultipartPartAction::get_key_object_failed() {
  s3_log(S3_LOG_INFO, request_id, "Failed to find part listing\n");
  if (clovis_kv_reader->get_state() == S3ClovisKVSReaderOpState::missing) {
    fetch_successful = true;  // With no entries.
    next();
  } else {
    fetch_successful = false;
    set_s3_error("InternalError");
    send_response_to_s3_client();
  }
}

void S3GetMultipartPartAction::get_next_objects() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  s3_log(S3_LOG_DEBUG, request_id, "Fetching next part listing\n");
  S3ObjectMetadataState multipart_object_state =
      object_multipart_metadata->get_state();
  if (multipart_object_state == S3ObjectMetadataState::present) {
    size_t count = S3Option::get_instance()->get_clovis_idx_fetch_count();

    clovis_kv_reader = clovis_kvs_reader_factory->create_clovis_kvs_reader(
        request, s3_clovis_api);
    clovis_kv_reader->next_keyval(
        object_multipart_metadata->get_part_index_oid(), last_key, count,
        std::bind(&S3GetMultipartPartAction::get_next_objects_successful, this),
        std::bind(&S3GetMultipartPartAction::get_next_objects_failed, this));
  } else {
    if (multipart_object_state == S3ObjectMetadataState::missing) {
      set_s3_error("NoSuchUpload");
    } else {
      set_s3_error("InternalError");
    }
    send_response_to_s3_client();
  }
}

void S3GetMultipartPartAction::get_next_objects_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  s3_log(S3_LOG_DEBUG, request_id, "Found part listing\n");
  struct m0_uint128 part_index_oid =
      object_multipart_metadata->get_part_index_oid();
  bool atleast_one_json_error = false;
  auto& kvps = clovis_kv_reader->get_key_values();
  size_t length = kvps.size();
  for (auto& kv : kvps) {
    s3_log(S3_LOG_DEBUG, request_id, "Read Object = %s\n", kv.first.c_str());
    auto part = part_metadata_factory->create_part_metadata_obj(
        request, part_index_oid, upload_id, atoi(kv.first.c_str()));

    if (part->from_json(kv.second.second) != 0) {
      atleast_one_json_error = true;
      s3_log(S3_LOG_ERROR, request_id,
             "Json Parsing failed. Index oid = "
             "%" SCNx64 " : %" SCNx64 ", Key = %s, Value = %s\n",
             part_index_oid.u_hi, part_index_oid.u_lo, kv.first.c_str(),
             kv.second.second.c_str());
    } else {
      multipart_part_list.add_part(part);
      return_list_size++;
    }

    if (--length == 0 || return_list_size == max_parts) {
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
  if ((return_list_size == max_parts) || (kvps.size() < count_we_requested)) {
    // Go ahead and respond.
    if (return_list_size == max_parts) {
      multipart_part_list.set_response_is_truncated(true);
    }
    multipart_part_list.set_next_marker_key(last_key);
    fetch_successful = true;
    send_response_to_s3_client();
  } else {
    get_next_objects();
  }
}

void S3GetMultipartPartAction::get_next_objects_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (clovis_kv_reader->get_state() == S3ClovisKVSReaderOpState::missing) {
    s3_log(S3_LOG_DEBUG, request_id, "Missing part listing\n");
    fetch_successful = true;  // With no entries.
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Part metadata next keyval operation failed due to pre launch "
           "failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    s3_log(S3_LOG_DEBUG, request_id, "Failed to find part listing\n");
    fetch_successful = false;
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetMultipartPartAction::send_response_to_s3_client() {
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
  } else if (fetch_successful) {
    multipart_part_list.set_user_id(request->get_user_id());
    multipart_part_list.set_user_name(request->get_user_name());
    multipart_part_list.set_account_id(request->get_account_id());
    multipart_part_list.set_account_name(request->get_account_name());

    std::string& response_xml = multipart_part_list.get_multipart_xml();

    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));
    request->set_out_header_value("Content-Type", "application/xml");
    s3_log(S3_LOG_DEBUG, request_id, "Object list response_xml = %s\n",
           response_xml.c_str());

    request->send_response(S3HttpSuccess200, response_xml);
  } else {
    S3Error error("InternalError", request->get_request_id(), bucket_name);
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));

    request->send_response(error.get_http_status_code(), response_xml);
  }
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
