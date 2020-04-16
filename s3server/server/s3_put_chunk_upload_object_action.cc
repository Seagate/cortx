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
 * Original creation date: 17-Mar-2016
 */

#include "s3_put_chunk_upload_object_action.h"
#include "s3_clovis_layout.h"
#include "s3_error_codes.h"
#include "s3_iem.h"
#include "s3_log.h"
#include "s3_m0_uint128_helper.h"
#include "s3_option.h"
#include "s3_perf_logger.h"
#include "s3_put_tag_body.h"
#include "s3_stats.h"
#include "s3_uri_to_mero_oid.h"
#include "s3_m0_uint128_helper.h"
#include <evhttp.h>

extern struct m0_uint128 global_probable_dead_object_list_index_oid;

S3PutChunkUploadObjectAction::S3PutChunkUploadObjectAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory,
    std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory,
    std::shared_ptr<S3ClovisWriterFactory> clovis_s3_factory,
    std::shared_ptr<S3AuthClientFactory> auth_factory,
    std::shared_ptr<ClovisAPI> clovis_api,
    std::shared_ptr<S3PutTagsBodyFactory> put_tags_body_factory,
    std::shared_ptr<S3ClovisKVSWriterFactory> kv_writer_factory)
    : S3ObjectAction(std::move(req), std::move(bucket_meta_factory),
                     std::move(object_meta_factory), true, auth_factory),
      auth_failed(false),
      write_failed(false),
      clovis_write_in_progress(false),
      clovis_write_completed(false),
      auth_in_progress(false),
      auth_completed(false) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  s3_log(S3_LOG_INFO, request_id,
         "S3 API: Put Object (Chunk mode). Bucket[%s]\
         Object[%s]\n",
         request->get_bucket_name().c_str(),
         request->get_object_name().c_str());

  if (clovis_api) {
    s3_clovis_api = clovis_api;
  } else {
    s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
  }

  if (S3Option::get_instance()->is_auth_disabled()) {
    auth_completed = true;
  }
  // Note valid value is set during create object
  layout_id = -1;

  s3_put_chunk_action_state = S3PutChunkUploadObjectActionState::empty;

  old_object_oid = {0ULL, 0ULL};
  old_layout_id = -1;
  new_object_oid = {0ULL, 0ULL};
  S3UriToMeroOID(s3_clovis_api, request->get_object_uri().c_str(), request_id,
                 &new_object_oid);
  tried_count = 0;
  salt = "uri_salt_";

  if (clovis_s3_factory) {
    clovis_writer_factory = std::move(clovis_s3_factory);
  } else {
    clovis_writer_factory = std::make_shared<S3ClovisWriterFactory>();
  }

  if (kv_writer_factory) {
    clovis_kv_writer_factory = std::move(kv_writer_factory);
  } else {
    clovis_kv_writer_factory = std::make_shared<S3ClovisKVSWriterFactory>();
  }

  if (put_tags_body_factory) {
    put_object_tag_body_factory = std::move(put_tags_body_factory);
  } else {
    put_object_tag_body_factory = std::make_shared<S3PutTagsBodyFactory>();
  }

  setup_steps();
}

void S3PutChunkUploadObjectAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");

  if (!request->get_header_value("x-amz-tagging").empty()) {
    ACTION_TASK_ADD(
        S3PutChunkUploadObjectAction::validate_x_amz_tagging_if_present, this);
  }
  ACTION_TASK_ADD(S3PutChunkUploadObjectAction::create_object, this);
  ACTION_TASK_ADD(S3PutChunkUploadObjectAction::initiate_data_streaming, this);
  ACTION_TASK_ADD(S3PutChunkUploadObjectAction::save_metadata, this);
  ACTION_TASK_ADD(S3PutChunkUploadObjectAction::send_response_to_s3_client,
                  this);
  // ...
}

void S3PutChunkUploadObjectAction::chunk_auth_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  auth_in_progress = false;
  auth_completed = true;
  if (check_shutdown_and_rollback(true)) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  if (clovis_write_completed) {
    if (write_failed) {
      assert(s3_put_chunk_action_state ==
             S3PutChunkUploadObjectActionState::writeFailed);
      // Clean up will be done after response.
      send_response_to_s3_client();
    } else {
      next();
    }
  } else {
    // wait for write to complete. do nothing here.
  }
}

void S3PutChunkUploadObjectAction::chunk_auth_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_put_chunk_action_state =
      S3PutChunkUploadObjectActionState::dataSignatureCheckFailed;
  auth_in_progress = false;
  auth_failed = true;
  auth_completed = true;
  set_s3_error("SignatureDoesNotMatch");
  if (check_shutdown_and_rollback(true)) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  if (clovis_write_in_progress) {
    // Do nothing, handle after write returns
  } else {
    // write_failed check not required as cleanup do necessary
    // Clean up will be done after response.
    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::fetch_bucket_info_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_put_chunk_action_state =
      S3PutChunkUploadObjectActionState::validationFailed;
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
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::validate_x_amz_tagging_if_present() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  std::string new_object_tags = request->get_header_value("x-amz-tagging");
  s3_log(S3_LOG_DEBUG, request_id, "Received tags= %s\n",
         new_object_tags.c_str());
  if (!new_object_tags.empty()) {
    parse_x_amz_tagging_header(new_object_tags);
  } else {
    s3_put_chunk_action_state =
        S3PutChunkUploadObjectActionState::validationFailed;
    set_s3_error("InvalidTagError");
    send_response_to_s3_client();
  }
}

void S3PutChunkUploadObjectAction::parse_x_amz_tagging_header(
    std::string content) {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  struct evkeyvalq key_value;
  memset(&key_value, 0, sizeof(key_value));
  if (0 == evhttp_parse_query_str(content.c_str(), &key_value)) {
    char* decoded_key = NULL;
    for (struct evkeyval* header = key_value.tqh_first; header;
         header = header->next.tqe_next) {

      decoded_key = evhttp_decode_uri(header->key);
      s3_log(S3_LOG_DEBUG, request_id,
             "Successfully parsed the Key Values=%s %s", decoded_key,
             header->value);
      new_object_tags_map[decoded_key] = header->value;
    }
    validate_tags();
  } else {
    s3_put_chunk_action_state =
        S3PutChunkUploadObjectActionState::validationFailed;
    set_s3_error("InvalidTagError");
    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::validate_tags() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  std::string xml;
  std::shared_ptr<S3PutTagBody> put_object_tag_body =
      put_object_tag_body_factory->create_put_resource_tags_body(xml,
                                                                 request_id);

  if (put_object_tag_body->validate_object_xml_tags(new_object_tags_map)) {
    next();
  } else {
    s3_put_chunk_action_state =
        S3PutChunkUploadObjectActionState::validationFailed;
    set_s3_error("InvalidTagError");
    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::fetch_object_info_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  struct m0_uint128 object_list_oid =
      bucket_metadata->get_object_list_index_oid();
  if ((object_list_oid.u_hi == 0ULL && object_list_oid.u_lo == 0ULL) ||
      (objects_version_list_oid.u_hi == 0ULL &&
       objects_version_list_oid.u_lo == 0ULL)) {
    // Rare/unlikely: Mero KVS data corruption:
    // object_list_oid/objects_version_list_oid is null only when bucket
    // metadata is corrupted.
    // user has to delete and recreate the bucket again to make it work.
    s3_log(S3_LOG_ERROR, request_id, "Bucket(%s) metadata is corrupted.\n",
           request->get_bucket_name().c_str());
    s3_iem(LOG_ERR, S3_IEM_METADATA_CORRUPTED, S3_IEM_METADATA_CORRUPTED_STR,
           S3_IEM_METADATA_CORRUPTED_JSON);
    s3_put_chunk_action_state =
        S3PutChunkUploadObjectActionState::validationFailed;
    set_s3_error("MetaDataCorruption");
    send_response_to_s3_client();
  } else {
    next();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::fetch_object_info_success() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (object_metadata->get_state() == S3ObjectMetadataState::present) {
    s3_log(S3_LOG_DEBUG, request_id, "S3ObjectMetadataState::present\n");
    old_object_oid = object_metadata->get_oid();
    old_oid_str = S3M0Uint128Helper::to_string(old_object_oid);
    old_layout_id = object_metadata->get_layout_id();
    create_new_oid(old_object_oid);
    next();
  } else if (object_metadata->get_state() == S3ObjectMetadataState::missing) {
    s3_log(S3_LOG_DEBUG, request_id, "S3ObjectMetadataState::missing\n");
    next();
  } else if (object_metadata->get_state() ==
             S3ObjectMetadataState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Object metadata load operation failed due to pre launch failure\n");
    s3_put_chunk_action_state =
        S3PutChunkUploadObjectActionState::validationFailed;
    set_s3_error("ServiceUnavailable");
    send_response_to_s3_client();
  } else {
    s3_log(S3_LOG_DEBUG, request_id, "Failed to look up metadata.\n");
    s3_put_chunk_action_state =
        S3PutChunkUploadObjectActionState::validationFailed;
    set_s3_error("InternalError");
    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::create_object() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  create_object_timer.start();
  if (tried_count == 0) {
    clovis_writer =
        clovis_writer_factory->create_clovis_writer(request, new_object_oid);
  } else {
    clovis_writer->set_oid(new_object_oid);
  }

  layout_id = S3ClovisLayoutMap::get_instance()->get_layout_for_object_size(
      request->get_data_length());

  clovis_writer->create_object(
      std::bind(&S3PutChunkUploadObjectAction::create_object_successful, this),
      std::bind(&S3PutChunkUploadObjectAction::create_object_failed, this),
      layout_id);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::create_object_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_put_chunk_action_state =
      S3PutChunkUploadObjectActionState::newObjOidCreated;

  // New Object or overwrite, create new metadata and release old.
  new_object_metadata = object_metadata_factory->create_object_metadata_obj(
      request, bucket_metadata->get_object_list_index_oid());
  new_object_metadata->set_objects_version_list_index_oid(
      bucket_metadata->get_objects_version_list_index_oid());

  new_oid_str = S3M0Uint128Helper::to_string(new_object_oid);

  // Generate a version id for the new object.
  new_object_metadata->regenerate_version_id();
  new_object_metadata->set_oid(clovis_writer->get_oid());
  new_object_metadata->set_layout_id(layout_id);

  add_object_oid_to_probable_dead_oid_list();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::create_object_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  if (clovis_writer->get_state() == S3ClovisWriterOpState::exists) {
    collision_detected();
  } else if (clovis_writer->get_state() ==
             S3ClovisWriterOpState::failed_to_launch) {
    s3_put_chunk_action_state =
        S3PutChunkUploadObjectActionState::newObjOidCreationFailed;
    create_object_timer.stop();
    LOG_PERF("create_object_failed_ms", request_id.c_str(),
             create_object_timer.elapsed_time_in_millisec());
    s3_stats_timing("create_object_failed",
                    create_object_timer.elapsed_time_in_millisec());
    s3_log(S3_LOG_WARN, request_id, "Create object failed.\n");
    set_s3_error("ServiceUnavailable");
    send_response_to_s3_client();
  } else {
    s3_put_chunk_action_state =
        S3PutChunkUploadObjectActionState::newObjOidCreationFailed;
    create_object_timer.stop();
    LOG_PERF("create_object_failed_ms", request_id.c_str(),
             create_object_timer.elapsed_time_in_millisec());
    s3_stats_timing("create_object_failed",
                    create_object_timer.elapsed_time_in_millisec());
    s3_log(S3_LOG_WARN, request_id, "Create object failed.\n");

    // Any other error report failure.
    set_s3_error("InternalError");
    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::collision_detected() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  if (tried_count < MAX_COLLISION_RETRY_COUNT) {
    s3_log(S3_LOG_INFO, request_id, "Object ID collision happened for uri %s\n",
           request->get_object_uri().c_str());
    // Handle Collision
    create_new_oid(new_object_oid);
    tried_count++;
    if (tried_count > 5) {
      s3_log(S3_LOG_INFO, request_id,
             "Object ID collision happened %d times for uri %s\n", tried_count,
             request->get_object_uri().c_str());
    }
    create_object();
  } else {
    s3_log(S3_LOG_ERROR, request_id,
           "Exceeded maximum collision retry attempts."
           "Collision occurred %d times for uri %s\n",
           tried_count, request->get_object_uri().c_str());
    s3_iem(LOG_ERR, S3_IEM_COLLISION_RES_FAIL, S3_IEM_COLLISION_RES_FAIL_STR,
           S3_IEM_COLLISION_RES_FAIL_JSON);
    s3_put_chunk_action_state =
        S3PutChunkUploadObjectActionState::newObjOidCreationFailed;
    set_s3_error("InternalError");
    send_response_to_s3_client();
  }
}

void S3PutChunkUploadObjectAction::create_new_oid(
    struct m0_uint128 current_oid) {
  int salt_counter = 0;
  std::string salted_uri;
  do {
    salted_uri = request->get_object_uri() + salt +
                 std::to_string(salt_counter) + std::to_string(tried_count);

    S3UriToMeroOID(s3_clovis_api, salted_uri.c_str(), request_id,
                   &new_object_oid);

    ++salt_counter;
  } while ((new_object_oid.u_hi == current_oid.u_hi) &&
           (new_object_oid.u_lo == current_oid.u_lo));

  return;
}

void S3PutChunkUploadObjectAction::initiate_data_streaming() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  create_object_timer.stop();
  LOG_PERF("create_object_successful_ms", request_id.c_str(),
           create_object_timer.elapsed_time_in_millisec());
  s3_stats_timing("chunkupload_create_object_success",
                  create_object_timer.elapsed_time_in_millisec());

  if (!S3Option::get_instance()->is_auth_disabled()) {
    get_auth_client()->init_chunk_auth_cycle(
        std::bind(&S3PutChunkUploadObjectAction::chunk_auth_successful, this),
        std::bind(&S3PutChunkUploadObjectAction::chunk_auth_failed, this));
  }

  if (request->get_data_length() == 0) {
    next();  // Zero size object.
  } else {
    if (request->has_all_body_content()) {
      s3_log(S3_LOG_DEBUG, request_id,
             "We have all the data, so just write it.\n");
      write_object(request->get_buffered_input());
    } else {
      s3_log(S3_LOG_DEBUG, request_id,
             "We do not have all the data, start listening...\n");
      // Start streaming, logically pausing action till we get data.
      request->listen_for_incoming_data(
          std::bind(&S3PutChunkUploadObjectAction::consume_incoming_content,
                    this),
          S3Option::get_instance()->get_clovis_write_payload_size(layout_id));
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::consume_incoming_content() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // for shutdown testcases, check FI and set shutdown signal
  S3_CHECK_FI_AND_SET_SHUTDOWN_SIGNAL(
      "put_chunk_upload_object_action_consume_incoming_content_shutdown_fail");
  if (request->is_s3_client_read_error()) {
    if (!clovis_write_in_progress) {
      client_read_error();
    }
    return;
  }

  if (!clovis_write_in_progress) {
    if (request->get_buffered_input()->is_freezed() ||
        request->get_buffered_input()->get_content_length() >=
            S3Option::get_instance()->get_clovis_write_payload_size(
                layout_id)) {
      write_object(request->get_buffered_input());
      if (!clovis_write_in_progress && write_failed) {
        s3_log(S3_LOG_DEBUG, "", "Exiting\n");
        return;
      }
    }
  }
  if (!request->get_buffered_input()->is_freezed() &&
      request->get_buffered_input()->get_content_length() >=
          (S3Option::get_instance()->get_clovis_write_payload_size(layout_id) *
           S3Option::get_instance()->get_read_ahead_multiple())) {
    s3_log(S3_LOG_DEBUG, request_id, "Pausing with Buffered length = %zu\n",
           request->get_buffered_input()->get_content_length());
    request->pause();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::send_chunk_details_if_any() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // Also send any ready chunk data for auth
  while (request->is_chunk_detail_ready()) {
    S3ChunkDetail detail = request->pop_chunk_detail();
    s3_log(S3_LOG_DEBUG, request_id, "Using chunk details for auth:\n");
    detail.debug_dump();
    if (!S3Option::get_instance()->is_auth_disabled()) {
      if (detail.get_size() == 0) {
        // Last chunk is size 0
        get_auth_client()->add_last_checksum_for_chunk(
            detail.get_signature(), detail.get_payload_hash());
      } else {
        get_auth_client()->add_checksum_for_chunk(detail.get_signature(),
                                                  detail.get_payload_hash());
      }
      auth_in_progress = true;  // this triggers auth
    }
  }
}

void S3PutChunkUploadObjectAction::write_object(
    std::shared_ptr<S3AsyncBufferOptContainer> buffer) {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  // Also send any ready chunk data for auth
  send_chunk_details_if_any();

  clovis_writer->write_content(
      std::bind(&S3PutChunkUploadObjectAction::write_object_successful, this),
      std::bind(&S3PutChunkUploadObjectAction::write_object_failed, this),
      buffer);
  clovis_write_in_progress = true;

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::write_object_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_DEBUG, request_id, "Write to clovis successful\n");
  clovis_write_in_progress = false;

  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  if (request->is_s3_client_read_error()) {
    client_read_error();
    return;
  }
  if (auth_failed) {
    // Proper error is already set in chunk_auth_failed
    assert(get_s3_error_code().compare("SignatureDoesNotMatch") == 0);
    assert(s3_put_chunk_action_state ==
           S3PutChunkUploadObjectActionState::dataSignatureCheckFailed);
    // Clean up will be done after response.
    send_response_to_s3_client();
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }

  if (/* buffered data len is at least equal max we can write to clovis in one
         write */
      request->get_buffered_input()->get_content_length() >=
          S3Option::get_instance()->get_clovis_write_payload_size(
              layout_id) || /* we have all the data buffered and ready to
                               write */
      (request->get_buffered_input()->is_freezed() &&
       request->get_buffered_input()->get_content_length() > 0)) {
    write_object(request->get_buffered_input());
  } else if (request->get_buffered_input()->is_freezed() &&
             request->get_buffered_input()->get_content_length() == 0) {
    clovis_write_completed = true;
    if (auth_completed) {
      next();
    } else {
      // else wait for auth to complete
      send_chunk_details_if_any();
    }
  }
  if (!request->get_buffered_input()->is_freezed()) {
    // else we wait for more incoming data
    request->resume();
  }
}

void S3PutChunkUploadObjectAction::write_object_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  clovis_write_in_progress = false;
  write_failed = true;
  s3_put_chunk_action_state = S3PutChunkUploadObjectActionState::writeFailed;

  request->pause();  // pause any further reading from client
  get_auth_client()->abort_chunk_auth_op();

  if (request->is_s3_client_read_error()) {
    client_read_error();
    return;
  }
  if (clovis_writer->get_state() == S3ClovisWriterOpState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
    s3_log(S3_LOG_ERROR, request_id,
           "write_object_failed called due to clovis_entity_open failure\n");
  } else {
    set_s3_error("InternalError");
  }

  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  clovis_write_completed = true;
  if (!auth_in_progress) {
    // Clean up will be done after response.
    send_response_to_s3_client();
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::save_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  // to rest Date and Last-Modfied time object metadata
  new_object_metadata->reset_date_time_to_current();
  new_object_metadata->set_content_length(request->get_data_length_str());
  new_object_metadata->set_md5(clovis_writer->get_content_md5());
  new_object_metadata->set_tags(new_object_tags_map);

  for (auto it : request->get_in_headers_copy()) {
    if (it.first.find("x-amz-meta-") != std::string::npos) {
      s3_log(S3_LOG_DEBUG, request_id,
             "Writing user metadata on object: [%s] -> [%s]\n",
             it.first.c_str(), it.second.c_str());
      new_object_metadata->add_user_defined_attribute(it.first, it.second);
    }
  }

  // bypass shutdown signal check for next task
  check_shutdown_signal_for_next_task(false);
  new_object_metadata->save(
      std::bind(&S3PutChunkUploadObjectAction::save_object_metadata_success,
                this),
      std::bind(&S3PutChunkUploadObjectAction::save_object_metadata_failed,
                this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::save_object_metadata_success() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_put_chunk_action_state = S3PutChunkUploadObjectActionState::metadataSaved;
  next();
}

void S3PutChunkUploadObjectAction::save_object_metadata_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_put_chunk_action_state =
      S3PutChunkUploadObjectActionState::metadataSaveFailed;
  if (new_object_metadata->get_state() ==
      S3ObjectMetadataState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id, "Object metadata save failed\n");
    set_s3_error("ServiceUnavailable");
  }
  // Clean up will be done after response.
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::add_object_oid_to_probable_dead_oid_list() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  std::map<std::string, std::string> probable_oid_list;
  assert(!new_oid_str.empty());

  // store old object oid
  if (old_object_oid.u_hi || old_object_oid.u_lo) {
    assert(!old_oid_str.empty());

    // key = oldoid + "-" + newoid
    std::string old_oid_rec_key = old_oid_str + '-' + new_oid_str;
    s3_log(S3_LOG_DEBUG, request_id,
           "Adding old_probable_del_rec with key [%s]\n",
           old_oid_rec_key.c_str());
    old_probable_del_rec.reset(new S3ProbableDeleteRecord(
        old_oid_rec_key, {0ULL, 0ULL}, object_metadata->get_object_name(),
        old_object_oid, old_layout_id,
        bucket_metadata->get_object_list_index_oid(),
        bucket_metadata->get_objects_version_list_index_oid(),
        object_metadata->get_version_key_in_index(), false /* force_delete */));

    probable_oid_list[old_oid_rec_key] = old_probable_del_rec->to_json();
  }

  s3_log(S3_LOG_DEBUG, request_id,
         "Adding new_probable_del_rec with key [%s]\n", new_oid_str.c_str());
  new_probable_del_rec.reset(new S3ProbableDeleteRecord(
      new_oid_str, {0ULL, 0ULL}, new_object_metadata->get_object_name(),
      new_object_oid, layout_id, bucket_metadata->get_object_list_index_oid(),
      bucket_metadata->get_objects_version_list_index_oid(),
      new_object_metadata->get_version_key_in_index(),
      false /* force_delete */));

  // store new oid, key = newoid
  probable_oid_list[new_oid_str] = new_probable_del_rec->to_json();

  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }
  clovis_kv_writer->put_keyval(
      global_probable_dead_object_list_index_oid, probable_oid_list,
      std::bind(&S3PutChunkUploadObjectAction::next, this),
      std::bind(&S3PutChunkUploadObjectAction::
                     add_object_oid_to_probable_dead_oid_list_failed,
                this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::
    add_object_oid_to_probable_dead_oid_list_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_put_chunk_action_state =
      S3PutChunkUploadObjectActionState::probableEntryRecordFailed;
  if (clovis_kv_writer->get_state() ==
      S3ClovisKVSWriterOpState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
  } else {
    set_s3_error("InternalError");
  }
  // Clean up will be done after response.
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if ((auth_in_progress) &&
      (get_auth_client()->get_state() == S3AuthClientOpState::started)) {
    get_auth_client()->abort_chunk_auth_op();
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }

  if (S3Option::get_instance()->is_getoid_enabled()) {
    request->set_out_header_value("x-stx-oid",
                                  S3M0Uint128Helper::to_string(new_object_oid));
    request->set_out_header_value("x-stx-layout-id", std::to_string(layout_id));
  }

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
  } else {
    // Metadata saved implies its success.
    assert(s3_put_chunk_action_state ==
           S3PutChunkUploadObjectActionState::metadataSaved);

    s3_put_chunk_action_state = S3PutChunkUploadObjectActionState::completed;

    // AWS adds explicit quotes "" to etag values.
    std::string e_tag = "\"" + clovis_writer->get_content_md5() + "\"";

    request->set_out_header_value("ETag", e_tag);

    request->send_response(S3HttpSuccess200);
  }

  S3_RESET_SHUTDOWN_SIGNAL;  // for shutdown testcases
  request->resume(false);

  startcleanup();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::startcleanup() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // TODO: Perf - all below tasks can be done in parallel
  // Any of the following steps fail, backgrounddelete will be able to perform
  // cleanups.
  // Clear task list and setup cleanup task list
  clear_tasks();

  // Success conditions
  if (s3_put_chunk_action_state ==
      S3PutChunkUploadObjectActionState::completed) {
    s3_log(S3_LOG_DEBUG, request_id, "Cleanup old Object\n");
    if (old_object_oid.u_hi || old_object_oid.u_lo) {
      // mark old OID for deletion in overwrite case, this optimizes
      // backgrounddelete decisions.
      ACTION_TASK_ADD(S3PutChunkUploadObjectAction::mark_old_oid_for_deletion,
                      this);
    }
    // remove new oid from probable delete list.
    ACTION_TASK_ADD(
        S3PutChunkUploadObjectAction::remove_new_oid_probable_record, this);
    if (old_object_oid.u_hi || old_object_oid.u_lo) {
      // Object overwrite case, old object exists, delete it.
      ACTION_TASK_ADD(S3PutChunkUploadObjectAction::delete_old_object, this);
      // If delete object is successful, attempt to delete old probable record
    }
  } else if (s3_put_chunk_action_state ==
                 S3PutChunkUploadObjectActionState::newObjOidCreated ||
             s3_put_chunk_action_state ==
                 S3PutChunkUploadObjectActionState::writeFailed ||
             s3_put_chunk_action_state ==
                 S3PutChunkUploadObjectActionState::metadataSaveFailed ||
             s3_put_chunk_action_state ==
                 S3PutChunkUploadObjectActionState::dataSignatureCheckFailed) {
    // PUT is assumed to be failed with a need to rollback new object
    s3_log(S3_LOG_DEBUG, request_id,
           "Cleanup new Object: s3_put_chunk_action_state[%d]\n",
           s3_put_chunk_action_state);
    // Mark new OID for deletion, this optimizes backgrounddelete decisionss.
    ACTION_TASK_ADD(S3PutChunkUploadObjectAction::mark_new_oid_for_deletion,
                    this);
    if (old_object_oid.u_hi || old_object_oid.u_lo) {
      // remove old oid from probable delete list.
      ACTION_TASK_ADD(
          S3PutChunkUploadObjectAction::remove_old_oid_probable_record, this);
    }
    ACTION_TASK_ADD(S3PutChunkUploadObjectAction::delete_new_object, this);
    // If delete object is successful, attempt to delete new probable record
  } else {
    s3_log(S3_LOG_DEBUG, request_id,
           "No Cleanup required: s3_put_chunk_action_state[%d]\n",
           s3_put_chunk_action_state);
    assert(s3_put_chunk_action_state ==
               S3PutChunkUploadObjectActionState::empty ||
           s3_put_chunk_action_state ==
               S3PutChunkUploadObjectActionState::validationFailed ||
           s3_put_chunk_action_state ==
               S3PutChunkUploadObjectActionState::probableEntryRecordFailed ||
           s3_put_chunk_action_state ==
               S3PutChunkUploadObjectActionState::newObjOidCreationFailed);
    // Nothing to undo
  }

  // Start running the cleanup task list
  start();
}

void S3PutChunkUploadObjectAction::mark_new_oid_for_deletion() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  assert(!new_oid_str.empty());

  // update new oid, key = newoid, force_del = true
  new_probable_del_rec->set_force_delete(true);

  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }
  clovis_kv_writer->put_keyval(
      global_probable_dead_object_list_index_oid, new_oid_str,
      new_probable_del_rec->to_json(),
      std::bind(&S3PutChunkUploadObjectAction::next, this),
      std::bind(&S3PutChunkUploadObjectAction::next, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::mark_old_oid_for_deletion() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  assert(!old_oid_str.empty());
  assert(!new_oid_str.empty());

  // key = oldoid + "-" + newoid
  std::string old_oid_rec_key = old_oid_str + '-' + new_oid_str;

  // update old oid, force_del = true
  old_probable_del_rec->set_force_delete(true);

  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }
  clovis_kv_writer->put_keyval(
      global_probable_dead_object_list_index_oid, old_oid_str,
      old_probable_del_rec->to_json(),
      std::bind(&S3PutChunkUploadObjectAction::next, this),
      std::bind(&S3PutChunkUploadObjectAction::next, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::remove_old_oid_probable_record() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  assert(!old_oid_str.empty());
  assert(!new_oid_str.empty());

  // key = oldoid + "-" + newoid
  std::string old_oid_rec_key = old_oid_str + '-' + new_oid_str;

  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }
  clovis_kv_writer->delete_keyval(
      global_probable_dead_object_list_index_oid, old_oid_rec_key,
      std::bind(&S3PutChunkUploadObjectAction::next, this),
      std::bind(&S3PutChunkUploadObjectAction::next, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::remove_new_oid_probable_record() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  assert(!new_oid_str.empty());

  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }
  clovis_kv_writer->delete_keyval(
      global_probable_dead_object_list_index_oid, new_oid_str,
      std::bind(&S3PutChunkUploadObjectAction::next, this),
      std::bind(&S3PutChunkUploadObjectAction::next, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::delete_old_object() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // If PUT is success, we delete old object if present
  assert(old_object_oid.u_hi != 0ULL || old_object_oid.u_lo != 0ULL);

  clovis_writer->set_oid(old_object_oid);
  clovis_writer->delete_object(
      std::bind(
          &S3PutChunkUploadObjectAction::remove_old_object_version_metadata,
          this),
      std::bind(&S3PutChunkUploadObjectAction::next, this), old_layout_id);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::remove_old_object_version_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  object_metadata->remove_version_metadata(
      std::bind(&S3PutChunkUploadObjectAction::remove_old_oid_probable_record,
                this),
      std::bind(&S3PutChunkUploadObjectAction::next, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::delete_new_object() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // If PUT failed, then clean new object.
  assert(s3_put_chunk_action_state !=
         S3PutChunkUploadObjectActionState::completed);
  assert(new_object_oid.u_hi != 0ULL || new_object_oid.u_lo != 0ULL);

  clovis_writer->set_oid(new_object_oid);
  clovis_writer->delete_object(
      std::bind(&S3PutChunkUploadObjectAction::remove_new_oid_probable_record,
                this),
      std::bind(&S3PutChunkUploadObjectAction::next, this), layout_id);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutChunkUploadObjectAction::set_authorization_meta() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  auth_client->set_acl_and_policy(bucket_metadata->get_encoded_bucket_acl(),
                                  bucket_metadata->get_policy_as_json());
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

