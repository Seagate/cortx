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

#include "s3_delete_bucket_action.h"
#include "s3_error_codes.h"
#include "s3_iem.h"
#include "s3_log.h"
#include "s3_uri_to_mero_oid.h"

S3DeleteBucketAction::S3DeleteBucketAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<ClovisAPI> s3_clovis_apis,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory,
    std::shared_ptr<S3ObjectMultipartMetadataFactory> object_mp_meta_factory,
    std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory,
    std::shared_ptr<S3ClovisWriterFactory> clovis_s3_writer_factory,
    std::shared_ptr<S3ClovisKVSWriterFactory> clovis_s3_kvs_writer_factory,
    std::shared_ptr<S3ClovisKVSReaderFactory> clovis_s3_kvs_reader_factory)
    : S3BucketAction(std::move(req), std::move(bucket_meta_factory), false),
      last_key(""),
      is_bucket_empty(false),
      delete_successful(false) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  s3_log(S3_LOG_INFO, request_id, "S3 API: Delete Bucket API. Bucket[%s]\n",
         request->get_bucket_name().c_str());

  if (object_meta_factory) {
    object_metadata_factory = object_meta_factory;
  } else {
    object_metadata_factory = std::make_shared<S3ObjectMetadataFactory>();
  }

  if (object_mp_meta_factory) {
    object_mp_metadata_factory = object_mp_meta_factory;
  } else {
    object_mp_metadata_factory =
        std::make_shared<S3ObjectMultipartMetadataFactory>();
  }

  if (clovis_s3_writer_factory) {
    clovis_writer_factory = clovis_s3_writer_factory;
  } else {
    clovis_writer_factory = std::make_shared<S3ClovisWriterFactory>();
  }

  if (clovis_s3_kvs_reader_factory) {
    clovis_kvs_reader_factory = clovis_s3_kvs_reader_factory;
  } else {
    clovis_kvs_reader_factory = std::make_shared<S3ClovisKVSReaderFactory>();
  }

  if (clovis_s3_kvs_writer_factory) {
    clovis_kvs_writer_factory = clovis_s3_kvs_writer_factory;
  } else {
    clovis_kvs_writer_factory = std::make_shared<S3ClovisKVSWriterFactory>();
  }

  if (s3_clovis_apis) {
    s3_clovis_api = s3_clovis_apis;
  } else {
    s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
  }

  multipart_present = false;
  setup_steps();
}

void S3DeleteBucketAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3DeleteBucketAction::fetch_first_object_metadata, this);
  ACTION_TASK_ADD(S3DeleteBucketAction::fetch_multipart_objects, this);
  ACTION_TASK_ADD(S3DeleteBucketAction::delete_multipart_objects, this);
  ACTION_TASK_ADD(S3DeleteBucketAction::remove_part_indexes, this);
  ACTION_TASK_ADD(S3DeleteBucketAction::remove_multipart_index, this);
  ACTION_TASK_ADD(S3DeleteBucketAction::remove_object_list_index, this);
  ACTION_TASK_ADD(S3DeleteBucketAction::remove_objects_version_list_index,
                  this);
  ACTION_TASK_ADD(S3DeleteBucketAction::delete_bucket, this);
  ACTION_TASK_ADD(S3DeleteBucketAction::send_response_to_s3_client, this);
  // ...
}

void S3DeleteBucketAction::fetch_bucket_info_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  S3BucketMetadataState bucket_metadata_state = bucket_metadata->get_state();
  if (bucket_metadata_state == S3BucketMetadataState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Bucket metadata load operation failed due to pre launch failure\n");
    set_s3_error("ServiceUnavailable");
  } else if (bucket_metadata_state == S3BucketMetadataState::missing) {
    set_s3_error("NoSuchBucket");
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::fetch_first_object_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  S3BucketMetadataState bucket_metadata_state = bucket_metadata->get_state();
  if (bucket_metadata_state == S3BucketMetadataState::present) {
    clovis_kv_reader = clovis_kvs_reader_factory->create_clovis_kvs_reader(
        request, s3_clovis_api);
    // Try to fetch one object at least
    object_list_index_oid = bucket_metadata->get_object_list_index_oid();
    objects_version_list_index_oid =
        bucket_metadata->get_objects_version_list_index_oid();
    // If no object list index oid then it means bucket is empty
    if (object_list_index_oid.u_lo == 0ULL &&
        object_list_index_oid.u_hi == 0ULL) {
      is_bucket_empty = true;
      next();
    } else {
      clovis_kv_reader->next_keyval(
          object_list_index_oid, "", 1,
          std::bind(
              &S3DeleteBucketAction::fetch_first_object_metadata_successful,
              this),
          std::bind(&S3DeleteBucketAction::fetch_first_object_metadata_failed,
                    this));
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::fetch_first_object_metadata_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  is_bucket_empty = false;
  set_s3_error("BucketNotEmpty");
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::fetch_first_object_metadata_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (clovis_kv_reader->get_state() == S3ClovisKVSReaderOpState::missing) {
    s3_log(S3_LOG_DEBUG, request_id, "There is no object in bucket\n");
    is_bucket_empty = true;
    next();
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::failed) {
    is_bucket_empty = false;
    s3_log(S3_LOG_ERROR, request_id, "Failed to retrieve object metadata\n");
    set_s3_error("BucketNotEmpty");
    send_response_to_s3_client();
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Bucket metadata next keyval operation failed due to pre launch "
           "failure\n");
    set_s3_error("ServiceUnavailable");
    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::fetch_multipart_objects() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  struct m0_uint128 empty_indx_oid = {0ULL, 0ULL};
  struct m0_uint128 indx_oid = bucket_metadata->get_multipart_index_oid();
  // If the index oid is 0 then it implies there is no multipart metadata
  if (m0_uint128_cmp(&indx_oid, &empty_indx_oid) != 0) {
    multipart_present = true;
    // There is an oid for index present, so read objects from it
    size_t count = S3Option::get_instance()->get_clovis_idx_fetch_count();
    clovis_kv_reader->next_keyval(
        indx_oid, last_key, count,
        std::bind(&S3DeleteBucketAction::fetch_multipart_objects_successful,
                  this),
        std::bind(&S3DeleteBucketAction::next, this));
  } else {
    s3_log(S3_LOG_DEBUG, request_id, "Multipart index not present\n");
    next();
  }
}

void S3DeleteBucketAction::fetch_multipart_objects_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  std::string part_oids_str = "";
  struct m0_uint128 multipart_obj_oid;
  s3_log(S3_LOG_DEBUG, request_id, "Found multipart uploads listing\n");
  size_t return_list_size = 0;
  auto& kvps = clovis_kv_reader->get_key_values();
  size_t count_we_requested =
      S3Option::get_instance()->get_clovis_idx_fetch_count();
  size_t length = kvps.size();
  bool atleast_one_json_error = false;
  struct m0_uint128 multipart_index_oid =
      bucket_metadata->get_multipart_index_oid();
  for (auto& kv : kvps) {
    s3_log(S3_LOG_DEBUG, request_id, "Parsing Multipart object metadata = %s\n",
           kv.first.c_str());
    auto object = std::make_shared<S3ObjectMetadata>(request, true);
    if (object->from_json(kv.second.second) != 0) {
      atleast_one_json_error = true;
      s3_log(S3_LOG_ERROR, request_id,
             "Json Parsing failed. Index oid = "
             "%" SCNx64 " : %" SCNx64 ", Key = %s, Value = %s\n",
             multipart_index_oid.u_hi, multipart_index_oid.u_lo,
             kv.first.c_str(), kv.second.second.c_str());
    } else {
      struct m0_uint128 part_oid;
      multipart_objects[kv.first] = object->get_upload_id();
      multipart_obj_oid = object->get_oid();
      part_oid = object->get_part_index_oid();
      part_oids.push_back(part_oid);
      part_oids_str += " " + std::to_string(part_oid.u_hi) + " " +
                       std::to_string(part_oid.u_lo);

      if (multipart_obj_oid.u_hi != 0ULL || multipart_obj_oid.u_lo != 0ULL) {
        multipart_object_oids.push_back(multipart_obj_oid);
        multipart_object_layoutids.push_back(object->get_layout_id());
      }
    }
    return_list_size++;
    if (--length == 0 || return_list_size == count_we_requested) {
      // this is the last element returned or we reached limit requested
      last_key = kv.first;
      break;
    }
  }
  if (part_oids_str.size() != 0) {
    s3_log(S3_LOG_DEBUG, request_id, "Part indexes oids: %s\n",
           part_oids_str.c_str());
  }

  if (atleast_one_json_error) {
    s3_iem(LOG_ERR, S3_IEM_METADATA_CORRUPTED, S3_IEM_METADATA_CORRUPTED_STR,
           S3_IEM_METADATA_CORRUPTED_JSON);
  }
  if (kvps.size() < count_we_requested) {
    next();
  } else {
    fetch_multipart_objects();
  }
}

void S3DeleteBucketAction::delete_multipart_objects() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (multipart_object_oids.size() != 0) {
    clovis_writer = clovis_writer_factory->create_clovis_writer(request);
    clovis_writer->delete_objects(
        multipart_object_oids, multipart_object_layoutids,
        std::bind(&S3DeleteBucketAction::delete_multipart_objects_successful,
                  this),
        std::bind(&S3DeleteBucketAction::delete_multipart_objects_failed,
                  this));
  } else {
    next();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::delete_multipart_objects_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  int count = 0;
  int op_ret_code;
  bool atleast_one_error = false;
  for (auto& multipart_obj_oid : multipart_object_oids) {
    op_ret_code = clovis_writer->get_op_ret_code_for_delete_op(count);
    if (op_ret_code == 0 || op_ret_code == -ENOENT) {
      s3_log(S3_LOG_DEBUG, request_id,
             "Deleted multipart object, oid is "
             "%" SCNx64 " : %" SCNx64 "\n",
             multipart_obj_oid.u_hi, multipart_obj_oid.u_lo);
    } else {
      s3_log(
          S3_LOG_ERROR, request_id,
          "Failed to delete multipart object, this oid will be stale in Mero: "
          "%" SCNx64 " : %" SCNx64 "\n",
          multipart_obj_oid.u_hi, multipart_obj_oid.u_lo);
      atleast_one_error = true;
    }
    count += 1;
  }
  if (atleast_one_error) {
    s3_iem(LOG_ERR, S3_IEM_DELETE_OBJ_FAIL, S3_IEM_DELETE_OBJ_FAIL_STR,
           S3_IEM_DELETE_OBJ_FAIL_JSON);
  }
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::delete_multipart_objects_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  uint count = 0;
  int op_ret_code;
  bool atleast_one_error = false;
  if (clovis_writer->get_state() == S3ClovisWriterOpState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
    s3_log(S3_LOG_ERROR, "", "delete_multipart_objects_failed failed\n");
    send_response_to_s3_client();
    return;
  }
  for (auto& multipart_obj_oid : multipart_object_oids) {
    op_ret_code = clovis_writer->get_op_ret_code_for_delete_op(count);
    if (op_ret_code != -ENOENT && op_ret_code != 0) {
      s3_log(
          S3_LOG_ERROR, request_id,
          "Failed to delete multipart object, this oid will be stale in Mero: "
          "%" SCNx64 " : %" SCNx64 "\n",
          multipart_obj_oid.u_hi, multipart_obj_oid.u_lo);
      atleast_one_error = true;
    }
    count++;
  }
  if (atleast_one_error) {
    s3_iem(LOG_ERR, S3_IEM_DELETE_OBJ_FAIL, S3_IEM_DELETE_OBJ_FAIL_STR,
           S3_IEM_DELETE_OBJ_FAIL_JSON);
  }
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::remove_part_indexes() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (part_oids.size() != 0) {
    clovis_kv_writer = clovis_kvs_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
    clovis_kv_writer->delete_indexes(
        part_oids,
        std::bind(&S3DeleteBucketAction::remove_part_indexes_successful, this),
        std::bind(&S3DeleteBucketAction::remove_part_indexes_failed, this));
  } else {
    next();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::remove_part_indexes_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  int i;
  int op_ret_code;
  bool partial_failure = false;
  for (multipart_kv = multipart_objects.begin(), i = 0;
       multipart_kv != multipart_objects.end(); multipart_kv++, i++) {
    op_ret_code = clovis_kv_writer->get_op_ret_code_for(i);
    if (op_ret_code != 0 && op_ret_code != -ENOENT) {
      partial_failure = true;
    }
  }
  if (partial_failure) {
    s3_log(S3_LOG_WARN, request_id,
           "Failed to delete some of the multipart part metadata\n");
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  next();
}

void S3DeleteBucketAction::remove_part_indexes_failed() {
  s3_log(S3_LOG_WARN, request_id,
         "Failed to delete multipart part metadata index\n");
  if (clovis_kv_writer->get_state() ==
      S3ClovisKVSWriterOpState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
    send_response_to_s3_client();
  } else {
    next();
  }
}

void S3DeleteBucketAction::remove_multipart_index() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (multipart_present) {
    if (clovis_kv_writer == nullptr) {
      clovis_kv_writer = clovis_kvs_writer_factory->create_clovis_kvs_writer(
          request, s3_clovis_api);
    }
    clovis_kv_writer->delete_index(
        bucket_metadata->get_multipart_index_oid(),
        std::bind(&S3DeleteBucketAction::next, this),
        std::bind(&S3DeleteBucketAction::remove_multipart_index_failed, this));
  } else {
    next();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::remove_multipart_index_failed() {
  struct m0_uint128 multipart_index =
      bucket_metadata->get_multipart_index_oid();
  s3_log(S3_LOG_WARN, request_id,
         "Failed to delete multipart index oid "
         "%" SCNx64 " : %" SCNx64 "\n",
         multipart_index.u_hi, multipart_index.u_lo);
  if (clovis_kv_writer->get_state() ==
      S3ClovisKVSWriterOpState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
    send_response_to_s3_client();
  } else if (clovis_kv_writer->get_state() ==
             S3ClovisKVSWriterOpState::failed) {
    set_s3_error("InternalError");
    send_response_to_s3_client();
  } else {
    next();
  }
}

void S3DeleteBucketAction::remove_object_list_index() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (object_list_index_oid.u_hi == 0ULL &&
      object_list_index_oid.u_lo == 0ULL) {
    next();
  } else {
    // Can happen when only index is present, no objects in it
    if (clovis_kv_writer == nullptr) {
      clovis_kv_writer = clovis_kvs_writer_factory->create_clovis_kvs_writer(
          request, s3_clovis_api);
    }
    clovis_kv_writer->delete_index(
        object_list_index_oid, std::bind(&S3DeleteBucketAction::next, this),
        std::bind(&S3DeleteBucketAction::remove_object_list_index_failed,
                  this));
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

/*
 *  <IEM_INLINE_DOCUMENTATION>
 *    <event_code>047006002</event_code>
 *    <application>S3 Server</application>
 *    <submodule>S3 Actions</submodule>
 *    <description>Delete index failed causing stale data in Mero</description>
 *    <audience>Development</audience>
 *    <details>
 *      Delete index op failed. It may cause stale data in Mero.
 *      The data section of the event has following keys:
 *        time - timestamp.
 *        node - node name.
 *        pid  - process-id of s3server instance, useful to identify logfile.
 *        file - source code filename.
 *        line - line number within file where error occurred.
 *    </details>
 *    <service_actions>
 *      Save the S3 server log files.
 *      Contact development team for further investigation.
 *    </service_actions>
 *  </IEM_INLINE_DOCUMENTATION>
 */

void S3DeleteBucketAction::remove_object_list_index_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_ERROR, request_id,
         "Failed to delete index, this will be stale in Mero: %" SCNx64
         " : %" SCNx64 "\n",
         object_list_index_oid.u_hi, object_list_index_oid.u_lo);
  s3_iem(LOG_ERR, S3_IEM_DELETE_IDX_FAIL, S3_IEM_DELETE_IDX_FAIL_STR,
         S3_IEM_DELETE_IDX_FAIL_JSON);
  if (clovis_kv_writer->get_state() ==
      S3ClovisKVSWriterOpState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
    send_response_to_s3_client();
  } else if (clovis_kv_writer->get_state() ==
             S3ClovisKVSWriterOpState::failed) {
    set_s3_error("InternalError");
    send_response_to_s3_client();
  } else {
    next();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::remove_objects_version_list_index() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (objects_version_list_index_oid.u_hi == 0ULL &&
      objects_version_list_index_oid.u_lo == 0ULL) {
    next();
  } else {
    // Can happen when only index is present, no objects in it
    if (clovis_kv_writer == nullptr) {
      clovis_kv_writer = clovis_kvs_writer_factory->create_clovis_kvs_writer(
          request, s3_clovis_api);
    }
    clovis_kv_writer->delete_index(
        objects_version_list_index_oid,
        std::bind(&S3DeleteBucketAction::next, this),
        std::bind(
            &S3DeleteBucketAction::remove_objects_version_list_index_failed,
            this));
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::remove_objects_version_list_index_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_ERROR, request_id,
         "Failed to delete index, this will be stale in Mero: %" SCNx64
         " : %" SCNx64 "\n",
         objects_version_list_index_oid.u_hi,
         objects_version_list_index_oid.u_lo);
  s3_iem(LOG_ERR, S3_IEM_DELETE_IDX_FAIL, S3_IEM_DELETE_IDX_FAIL_STR,
         S3_IEM_DELETE_IDX_FAIL_JSON);
  if (clovis_kv_writer->get_state() ==
      S3ClovisKVSWriterOpState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
    send_response_to_s3_client();
  } else if (clovis_kv_writer->get_state() ==
             S3ClovisKVSWriterOpState::failed) {
    set_s3_error("InternalError");
    send_response_to_s3_client();
  } else {
    next();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::delete_bucket() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  bucket_metadata->remove(
      std::bind(&S3DeleteBucketAction::delete_bucket_successful, this),
      std::bind(&S3DeleteBucketAction::delete_bucket_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::delete_bucket_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  delete_successful = true;
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::delete_bucket_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_ERROR, request_id, "Bucket deletion failed\n");
  delete_successful = false;
  if (bucket_metadata->get_state() == S3BucketMetadataState::missing) {
    set_s3_error("NoSuchBucket");
  } else if (bucket_metadata->get_state() ==
             S3BucketMetadataState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteBucketAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // Trigger metadata read async operation with callback
  if (is_error_state() && !get_s3_error_code().empty()) {
    S3Error error(get_s3_error_code(), request->get_request_id(),
                  request->get_object_uri());
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));

    request->send_response(error.get_http_status_code(), response_xml);
  } else if (delete_successful) {
    request->send_response(S3HttpSuccess204);
  } else {
    S3Error error("InternalError", request->get_request_id(),
                  request->get_object_uri());
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));
    request->set_out_header_value("Retry-After", "1");
    request->send_response(error.get_http_status_code(), response_xml);
  }
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
