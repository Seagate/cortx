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
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original creation date: 6-Jan-2016
 */

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <unistd.h>

#include "s3_error_codes.h"
#include "s3_iem.h"
#include "s3_log.h"
#include "s3_md5_hash.h"
#include "s3_post_complete_action.h"
#include "s3_uri_to_mero_oid.h"
#include "s3_m0_uint128_helper.h"

extern struct m0_uint128 global_probable_dead_object_list_index_oid;

S3PostCompleteAction::S3PostCompleteAction(
    std::shared_ptr<S3RequestObject> req, std::shared_ptr<ClovisAPI> clovis_api,
    std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kvs_reader_factory,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory,
    std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory,
    std::shared_ptr<S3ObjectMultipartMetadataFactory> object_mp_meta_factory,
    std::shared_ptr<S3PartMetadataFactory> part_meta_factory,
    std::shared_ptr<S3ClovisWriterFactory> clovis_s3_writer_factory,
    std::shared_ptr<S3ClovisKVSWriterFactory> kv_writer_factory)
    : S3ObjectAction(std::move(req), std::move(bucket_meta_factory),
                     std::move(object_meta_factory), false) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  upload_id = request->get_query_string_value("uploadId");
  bucket_name = request->get_bucket_name();
  object_name = request->get_object_name();

  s3_log(S3_LOG_INFO, request_id,
         "S3 API: Complete Multipart Upload. Bucket[%s] Object[%s]\
         for UploadId[%s]\n",
         bucket_name.c_str(), object_name.c_str(), upload_id.c_str());

  if (clovis_api) {
    s3_clovis_api = std::move(clovis_api);
  } else {
    s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
  }
  if (clovis_kvs_reader_factory) {
    s3_clovis_kvs_reader_factory = std::move(clovis_kvs_reader_factory);
  } else {
    s3_clovis_kvs_reader_factory = std::make_shared<S3ClovisKVSReaderFactory>();
  }
  if (object_mp_meta_factory) {
    object_mp_metadata_factory = std::move(object_mp_meta_factory);
  } else {
    object_mp_metadata_factory =
        std::make_shared<S3ObjectMultipartMetadataFactory>();
  }
  if (part_meta_factory) {
    part_metadata_factory = std::move(part_meta_factory);
  } else {
    part_metadata_factory = std::make_shared<S3PartMetadataFactory>();
  }
  if (clovis_s3_writer_factory) {
    clovis_writer_factory = std::move(clovis_s3_writer_factory);
  } else {
    clovis_writer_factory = std::make_shared<S3ClovisWriterFactory>();
  }

  if (kv_writer_factory) {
    clovis_kv_writer_factory = std::move(kv_writer_factory);
  } else {
    clovis_kv_writer_factory = std::make_shared<S3ClovisKVSWriterFactory>();
  }

  object_size = 0;
  current_parts_size = 0;
  prev_fetched_parts_size = 0;
  obj_metadata_updated = false;
  validated_parts_count = 0;
  set_abort_multipart(false);
  count_we_requested = S3Option::get_instance()->get_clovis_idx_fetch_count();
  setup_steps();
}

void S3PostCompleteAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");

  ACTION_TASK_ADD(S3PostCompleteAction::load_and_validate_request, this);
  ACTION_TASK_ADD(S3PostCompleteAction::fetch_multipart_info, this);
  ACTION_TASK_ADD(S3PostCompleteAction::get_next_parts_info, this);
  ACTION_TASK_ADD(
      S3PostCompleteAction::add_object_oid_to_probable_dead_oid_list, this);
  ACTION_TASK_ADD(S3PostCompleteAction::save_metadata, this);
  ACTION_TASK_ADD(S3PostCompleteAction::delete_multipart_metadata, this);
  ACTION_TASK_ADD(S3PostCompleteAction::delete_part_list_index, this);
  ACTION_TASK_ADD(S3PostCompleteAction::send_response_to_s3_client, this);
  // ...
}

void S3PostCompleteAction::fetch_bucket_info_success() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::fetch_bucket_info_failed() {
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

void S3PostCompleteAction::fetch_object_info_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::load_and_validate_request() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (request->get_data_length() > 0) {
    if (request->has_all_body_content()) {
      if (validate_request_body(request->get_full_body_content_as_string())) {
        next();
      } else {
        invalid_request = true;
        if (get_s3_error_code().empty()) {
          set_s3_error("MalformedXML");
        }
        send_response_to_s3_client();
        return;
      }
    } else {
      request->listen_for_incoming_data(
          std::bind(&S3PostCompleteAction::consume_incoming_content, this),
          request->get_data_length() /* we ask for all */
          );
    }
  } else {
    invalid_request = true;
    set_s3_error("MalformedXML");
    send_response_to_s3_client();
    return;
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::consume_incoming_content() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (request->is_s3_client_read_error()) {
    client_read_error();
  } else if (request->has_all_body_content()) {
    if (validate_request_body(request->get_full_body_content_as_string())) {
      next();
    } else {
      invalid_request = true;
      set_s3_error("MalformedXML");
      send_response_to_s3_client();
      return;
    }
  } else {
    // else just wait till entire body arrives. rare.
    request->resume();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::fetch_multipart_info() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  multipart_index_oid = bucket_metadata->get_multipart_index_oid();
  multipart_metadata =
      object_mp_metadata_factory->create_object_mp_metadata_obj(
          request, multipart_index_oid, upload_id);

  // Loads specific object entry from BUCKET/<Bucket Name>/Multipart index which
  // has inprogress uploads list
  multipart_metadata->load(
      std::bind(&S3PostCompleteAction::fetch_multipart_info_success, this),
      std::bind(&S3PostCompleteAction::fetch_multipart_info_failed, this));

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::fetch_multipart_info_success() {
  // Set the appropriate members after loading multipart metadata
  old_object_oid = multipart_metadata->get_old_oid();
  old_layout_id = multipart_metadata->get_old_layout_id();
  new_object_oid = multipart_metadata->get_oid();
  layout_id = multipart_metadata->get_layout_id();

  if (old_object_oid.u_hi != 0ULL || old_object_oid.u_lo != 0ULL) {
    old_oid_str = S3M0Uint128Helper::to_string(old_object_oid);
  }
  new_oid_str = S3M0Uint128Helper::to_string(new_object_oid);

  next();
}

void S3PostCompleteAction::fetch_multipart_info_failed() {
  s3_log(S3_LOG_ERROR, request_id, "Multipart info missing\n");
  if (multipart_metadata->get_state() == S3ObjectMetadataState::missing) {
    set_s3_error("InvalidObjectState");
  } else if (multipart_metadata->get_state() ==
             S3ObjectMetadataState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Object metadata load operation failed due to pre launch failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
}

void S3PostCompleteAction::get_next_parts_info() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_DEBUG, request_id, "Fetching parts list from KV store\n");
  clovis_kv_reader = s3_clovis_kvs_reader_factory->create_clovis_kvs_reader(
      request, s3_clovis_api);
  clovis_kv_reader->next_keyval(
      multipart_metadata->get_part_index_oid(), last_key, count_we_requested,
      std::bind(&S3PostCompleteAction::get_next_parts_info_successful, this),
      std::bind(&S3PostCompleteAction::get_next_parts_info_failed, this));
}

void S3PostCompleteAction::get_next_parts_info_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering with size %d while requested %d\n",
         (int)clovis_kv_reader->get_key_values().size(),
         (int)count_we_requested);
  if (clovis_kv_reader->get_key_values().size() > 0) {
    // Do validation of parts
    if (!validate_parts()) {
      s3_log(S3_LOG_DEBUG, request_id, "validate_parts failed");
      return;
    }
  }

  if (is_abort_multipart()) {
    s3_log(S3_LOG_DEBUG, request_id, "aborting multipart");
    next();
  } else {
    if (clovis_kv_reader->get_key_values().size() < count_we_requested) {
      // Fetched all parts
      validated_parts_count += clovis_kv_reader->get_key_values().size();
      if ((parts.size() != 0) ||
          (validated_parts_count != std::stoul(total_parts))) {
        s3_log(S3_LOG_DEBUG, request_id,
               "invalid: parts.size %d validated %d exp %d", (int)parts.size(),
               (int)validated_parts_count, (int)std::stoul(total_parts));
        if (part_metadata) {
          part_metadata->set_state(S3PartMetadataState::missing_partially);
        }
        set_s3_error("InvalidPart");
        send_response_to_s3_client();
        return;
      }
      // All parts info processed and validated, finalize etag and move ahead.
      s3_log(S3_LOG_DEBUG, request_id, "finalizing");
      etag = awsetag.finalize();
      next();
    } else {
      // Continue fetching
      validated_parts_count += count_we_requested;
      if (!clovis_kv_reader->get_key_values().empty()) {
        last_key = clovis_kv_reader->get_key_values().rbegin()->first;
      }
      s3_log(S3_LOG_DEBUG, request_id, "continue fetching with %s",
             last_key.c_str());
      get_next_parts_info();
    }
  }
}

void S3PostCompleteAction::get_next_parts_info_failed() {
  if (clovis_kv_reader->get_state() == S3ClovisKVSReaderOpState::missing) {
    // There may not be any records left
    if ((parts.size() != 0) ||
        (validated_parts_count != std::stoul(total_parts))) {
      if (part_metadata) {
        part_metadata->set_state(S3PartMetadataState::missing_partially);
      }
      set_s3_error("InvalidPart");
      send_response_to_s3_client();
      return;
    }
    etag = awsetag.finalize();
    next();
  } else {
    if (clovis_kv_reader->get_state() ==
        S3ClovisKVSReaderOpState::failed_to_launch) {
      s3_log(S3_LOG_ERROR, request_id,
             "Parts metadata next keyval operation failed due to pre launch "
             "failure\n");
      set_s3_error("ServiceUnavailable");
    } else {
      set_s3_error("InternalError");
    }
    send_response_to_s3_client();
  }
}

void S3PostCompleteAction::set_abort_multipart(bool abortit) {
  delete_multipart_object = abortit;
}

bool S3PostCompleteAction::is_abort_multipart() {
  // If this returns true it means that request is bad (say parts (apart from
  // last part) are not of same size) so we continue further in cleanup mode.
  // We dont save metadata and delete object/indexes.
  // Its set to true by set_abort_multipart()
  return delete_multipart_object;
}

bool S3PostCompleteAction::validate_parts() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  size_t part_one_size_in_multipart_metadata =
      multipart_metadata->get_part_one_size();
  if (part_metadata == NULL) {
    part_metadata = part_metadata_factory->create_part_metadata_obj(
        request, multipart_metadata->get_part_index_oid(), upload_id, 0);
  }
  struct m0_uint128 part_index_oid = multipart_metadata->get_part_index_oid();
  std::map<std::string, std::pair<int, std::string>>& parts_batch_from_kvs =
      clovis_kv_reader->get_key_values();
  for (auto part_kv = parts.begin(); part_kv != parts.end();) {
    auto store_kv = parts_batch_from_kvs.find(part_kv->first);
    if (store_kv == parts_batch_from_kvs.end()) {
      // The part from complete request not in current kvs part list
      part_kv++;
      continue;
    } else {
      s3_log(S3_LOG_DEBUG, request_id, "Metadata for key [%s] -> [%s]\n",
             store_kv->first.c_str(), store_kv->second.second.c_str());
      if (part_metadata->from_json(store_kv->second.second) != 0) {
        s3_log(S3_LOG_ERROR, request_id,
               "Json Parsing failed. Index oid = "
               "%" SCNx64 " : %" SCNx64 ", Key = %s, Value = %s\n",
               part_index_oid.u_hi, part_index_oid.u_lo,
               store_kv->first.c_str(), store_kv->second.second.c_str());
        s3_iem(LOG_ERR, S3_IEM_METADATA_CORRUPTED,
               S3_IEM_METADATA_CORRUPTED_STR, S3_IEM_METADATA_CORRUPTED_JSON);

        // part metadata is corrupted, send response and return from here
        part_metadata->set_state(S3PartMetadataState::missing_partially);
        set_s3_error("InvalidPart");
        send_response_to_s3_client();
        return false;
      }
      s3_log(S3_LOG_DEBUG, request_id, "Processing Part [%s]\n",
             part_metadata->get_part_number().c_str());

      current_parts_size = part_metadata->get_content_length();
      if (current_parts_size > MAXIMUM_ALLOWED_PART_SIZE) {
        s3_log(S3_LOG_ERROR, request_id,
               "The part %s size(%zu) is larger than max "
               "part size allowed:5GB\n",
               store_kv->first.c_str(), current_parts_size);
        set_s3_error("EntityTooLarge");
        set_abort_multipart(true);
        break;
      }
      if (current_parts_size < MINIMUM_ALLOWED_PART_SIZE &&
          store_kv->first != total_parts) {
        s3_log(S3_LOG_ERROR, request_id,
               "The part %s size(%zu) is smaller than minimum "
               "part size allowed:%u\n",
               store_kv->first.c_str(), current_parts_size,
               MINIMUM_ALLOWED_PART_SIZE);
        set_s3_error("EntityTooSmall");
        set_abort_multipart(true);
        break;
      }

      if (part_one_size_in_multipart_metadata != 0) {
        // In non chunked mode if current part size is not same as
        // that in multipart metadata and its not the last part,
        // then bail out
        if (current_parts_size != part_one_size_in_multipart_metadata &&
            store_kv->first != total_parts) {
          s3_log(S3_LOG_ERROR, request_id,
                 "The part %s size (%zu) is not "
                 "matching with part one size (%zu) "
                 "in multipart metadata\n",
                 store_kv->first.c_str(), current_parts_size,
                 part_one_size_in_multipart_metadata);
          set_s3_error("InvalidObjectState");
          set_abort_multipart(true);
          break;
        }
      }
      if ((prev_fetched_parts_size != 0) &&
          (prev_fetched_parts_size != current_parts_size)) {
        if (store_kv->first == total_parts) {
          // This is the last part, ignore it after size calculation
          object_size += part_metadata->get_content_length();
          awsetag.add_part_etag(part_metadata->get_md5());
          part_kv = parts.erase(part_kv);
          continue;
        }
        s3_log(S3_LOG_ERROR, request_id,
               "The part %s size(%zu) is different "
               "from previous part size(%zu), Will be "
               "destroying the parts\n",
               store_kv->first.c_str(), current_parts_size,
               prev_fetched_parts_size);
        // Will be deleting complete object along with the part index and
        // multipart kv
        set_s3_error("InvalidObjectState");
        set_abort_multipart(true);
        break;
      } else {
        prev_fetched_parts_size = current_parts_size;
      }
      object_size += part_metadata->get_content_length();
      awsetag.add_part_etag(part_metadata->get_md5());
      // Remove the entry from parts map, so that in next
      // validate_parts() we dont have to scan it again
      part_kv = parts.erase(part_kv);
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return true;
}

void S3PostCompleteAction::add_object_oid_to_probable_dead_oid_list() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  new_object_metadata = object_metadata_factory->create_object_metadata_obj(
      request, bucket_metadata->get_object_list_index_oid());
  new_object_metadata->set_objects_version_list_index_oid(
      bucket_metadata->get_objects_version_list_index_oid());

  new_object_metadata->set_oid(new_object_oid);
  new_object_metadata->set_layout_id(layout_id);
  // Generate version id for the new obj as it will become live to s3 clients.
  new_object_metadata->regenerate_version_id();

  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }

  if (is_abort_multipart()) {
    // Mark new object for probable deletion, we delete obj only after multipart
    // metadata is deleted.
    assert(!new_oid_str.empty());

    new_probable_del_rec.reset(new S3ProbableDeleteRecord(
        new_oid_str, old_object_oid, multipart_metadata->get_object_name(),
        new_object_oid, layout_id, bucket_metadata->get_multipart_index_oid(),
        bucket_metadata->get_objects_version_list_index_oid(),
        new_object_metadata->get_version_key_in_index(),
        false /* force_delete */, true /* is_multipart */,
        multipart_metadata->get_part_index_oid()));
    // backgrounddelete will delete this entry if multipart metadata has
    // been deleted
    clovis_kv_writer->put_keyval(
        global_probable_dead_object_list_index_oid, new_oid_str,
        new_probable_del_rec->to_json(),
        std::bind(&S3PostCompleteAction::next, this),
        std::bind(&S3PostCompleteAction::
                       add_object_oid_to_probable_dead_oid_list_failed,
                  this));
  } else {
    // TODO add probable part list index delete record.

    // Mark old object if any for probable deletion.
    // store old object oid
    if (old_object_oid.u_hi || old_object_oid.u_lo) {
      assert(!old_oid_str.empty());
      assert(!new_oid_str.empty());

      // key = oldoid + "-" + newoid
      std::string old_oid_rec_key = old_oid_str + '-' + new_oid_str;
      old_probable_del_rec.reset(new S3ProbableDeleteRecord(
          old_oid_rec_key, {0ULL, 0ULL}, multipart_metadata->get_object_name(),
          old_object_oid, old_layout_id,
          bucket_metadata->get_object_list_index_oid(),
          bucket_metadata->get_objects_version_list_index_oid(),
          multipart_metadata->get_version_key_in_index(),
          false /* force_delete */
          ));
      // backgrounddelete will delete this entry if current object metadata has
      // moved on
      clovis_kv_writer->put_keyval(
          global_probable_dead_object_list_index_oid, old_oid_rec_key,
          old_probable_del_rec->to_json(),
          std::bind(&S3PostCompleteAction::next, this),
          std::bind(&S3PostCompleteAction::
                         add_object_oid_to_probable_dead_oid_list_failed,
                    this));
    } else {
      // Not an overwrite case, complete multipart upload for brand new object
      next();
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::add_object_oid_to_probable_dead_oid_list_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (clovis_kv_writer->get_state() ==
      S3ClovisKVSWriterOpState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::save_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (is_abort_multipart()) {
    next();
  } else {
    // Mark it as non-multipart, create final object metadata.
    // object_metadata->mark_as_non_multipart();
    for (auto it : multipart_metadata->get_user_attributes()) {
      new_object_metadata->add_user_defined_attribute(it.first, it.second);
    }

    // to rest Date and Last-Modfied time object metadata
    new_object_metadata->reset_date_time_to_current();
    new_object_metadata->set_tags(multipart_metadata->get_tags());
    new_object_metadata->set_content_length(std::to_string(object_size));
    new_object_metadata->set_md5(etag);

    new_object_metadata->save(
        std::bind(&S3PostCompleteAction::save_object_metadata_succesful, this),
        std::bind(&S3PostCompleteAction::save_object_metadata_failed, this));
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::save_object_metadata_succesful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  obj_metadata_updated = true;
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::save_object_metadata_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (new_object_metadata->get_state() ==
      S3ObjectMetadataState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::delete_multipart_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  multipart_metadata->remove(
      std::bind(&S3PostCompleteAction::next, this),
      std::bind(&S3PostCompleteAction::delete_multipart_metadata_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

// TODO - mark this for cleanup by backgrounddelete on failure??
void S3PostCompleteAction::delete_multipart_metadata_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_ERROR, request_id,
         "Deletion of %s key failed from multipart index\n",
         object_name.c_str());
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

// TODO - mark this for cleanup by backgrounddelete on failure??
void S3PostCompleteAction::delete_part_list_index() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  part_metadata->remove_index(
      std::bind(&S3PostCompleteAction::next, this),
      std::bind(&S3PostCompleteAction::delete_part_list_index_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::delete_part_list_index_failed() {
  m0_uint128 part_index_oid;
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  part_index_oid = part_metadata->get_part_index_oid();
  s3_log(S3_LOG_ERROR, request_id,
         "Deletion of part index failed, this oid will be stale in Mero"
         "%" SCNx64 " : %" SCNx64 "\n",
         part_index_oid.u_hi, part_index_oid.u_lo);

  if (part_metadata->get_state() == S3PartMetadataState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
    send_response_to_s3_client();
  } else {
    next();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

bool S3PostCompleteAction::validate_request_body(std::string& xml_str) {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  xmlNode* child_node;
  xmlChar* xml_part_number;
  xmlChar* xml_etag;
  std::string partnumber;
  std::string prev_partnumber = "";
  int previous_part;
  std::string input_etag;

  s3_log(S3_LOG_DEBUG, request_id, "xml string = %s", xml_str.c_str());
  xmlDocPtr document = xmlParseDoc((const xmlChar*)xml_str.c_str());
  if (document == NULL) {
    xmlFreeDoc(document);
    s3_log(S3_LOG_ERROR, request_id, "The xml string %s is invalid\n",
           xml_str.c_str());
    return false;
  }

  /*Get the root element node */
  xmlNodePtr root_node = xmlDocGetRootElement(document);

  // xmlNodePtr child = root_node->xmlChildrenNode;
  xmlNodePtr child = root_node->xmlChildrenNode;
  while (child != NULL) {
    s3_log(S3_LOG_DEBUG, request_id, "Xml Tag = %s\n", (char*)child->name);
    if (!xmlStrcmp(child->name, (const xmlChar*)"Part")) {
      partnumber = "";
      input_etag = "";
      for (child_node = child->children; child_node != NULL;
           child_node = child_node->next) {
        if ((!xmlStrcmp(child_node->name, (const xmlChar*)"PartNumber"))) {
          xml_part_number = xmlNodeGetContent(child_node);
          if (xml_part_number != NULL) {
            partnumber = (char*)xml_part_number;
            xmlFree(xml_part_number);
            xml_part_number = NULL;
          }
        }
        if ((!xmlStrcmp(child_node->name, (const xmlChar*)"ETag"))) {
          xml_etag = xmlNodeGetContent(child_node);
          if (xml_etag != NULL) {
            input_etag = (char*)xml_etag;
            xmlFree(xml_etag);
            xml_etag = NULL;
          }
        }
      }
      if (!partnumber.empty() && !input_etag.empty()) {
        parts[partnumber] = input_etag;
        if (prev_partnumber.empty()) {
          previous_part = 0;
        } else {
          previous_part = std::stoi(prev_partnumber);
        }
        if (previous_part > std::stoi(partnumber)) {
          // The request doesn't contain part numbers in ascending order
          set_s3_error("InvalidPartOrder");
          xmlFreeDoc(document);
          s3_log(S3_LOG_DEBUG, request_id,
                 "The XML string doesn't contain parts in ascending order\n");
          return false;
        }
        prev_partnumber = partnumber;
      } else {
        s3_log(S3_LOG_DEBUG, request_id,
               "Error: Part number/Etag missing for a part\n");
        xmlFreeDoc(document);
        return false;
      }
    }
    child = child->next;
  }
  total_parts = partnumber;
  xmlFreeDoc(document);
  return true;
}

void S3PostCompleteAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (is_error_state() && !get_s3_error_code().empty()) {
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
  } else if (is_abort_multipart()) {
    S3Error error("InvalidObjectState", request->get_request_id(),
                  request->get_object_uri());
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->send_response(error.get_http_status_code(), response_xml);
  } else if (obj_metadata_updated == true) {
    std::string response;
    std::string object_name = request->get_object_name();
    std::string bucket_name = request->get_bucket_name();
    std::string object_uri = request->get_object_uri();
    response = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    response += "<CompleteMultipartUploadResult xmlns=\"http://s3\">\n";
    response += "<Location>" + object_uri + "</Location>";
    response += "<Bucket>" + bucket_name + "</Bucket>\n";
    response += "<Key>" + object_name + "</Key>\n";
    response += "<ETag>\"" + etag + "\"</ETag>";
    response += "</CompleteMultipartUploadResult>";
    request->send_response(S3HttpSuccess200, response);
  } else {
    S3Error error("InternalError", request->get_request_id(),
                  request->get_object_uri());
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));
    request->send_response(error.get_http_status_code(), response_xml);
  }
  request->resume(false);
  startcleanup();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::startcleanup() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // Clear task list and setup cleanup task list
  clear_tasks();

  if (multipart_metadata) {
    if (obj_metadata_updated) {
      // New object has taken life, old object should be deleted if any.
      if (old_object_oid.u_hi || old_object_oid.u_lo) {
        // mark old OID for deletion in overwrite case, this optimizes
        // backgrounddelete decisions.
        ACTION_TASK_ADD(S3PostCompleteAction::mark_old_oid_for_deletion, this);
        ACTION_TASK_ADD(S3PostCompleteAction::delete_old_object, this);
      }
    } else if (is_abort_multipart() && multipart_metadata->get_state() ==
                                           S3ObjectMetadataState::deleted) {
      // Abort is due to validation failures in part sizes 1..n
      ACTION_TASK_ADD(S3PostCompleteAction::mark_new_oid_for_deletion, this);
      ACTION_TASK_ADD(S3PostCompleteAction::delete_new_object, this);
    } else {
      // Any failure we dont clean up objects as next S3 client action will
      // decide
      ACTION_TASK_ADD(S3PostCompleteAction::remove_new_oid_probable_record,
                      this);
    }
  }
  // Start running the cleanup task list
  start();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::mark_old_oid_for_deletion() {
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
  clovis_kv_writer->put_keyval(global_probable_dead_object_list_index_oid,
                               old_oid_str, old_probable_del_rec->to_json(),
                               std::bind(&S3PostCompleteAction::next, this),
                               std::bind(&S3PostCompleteAction::next, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::delete_old_object() {
    if (!clovis_writer) {
      clovis_writer =
          clovis_writer_factory->create_clovis_writer(request, old_object_oid);
    }
    // process to delete old object
    assert(old_object_oid.u_hi || old_object_oid.u_lo);

    clovis_writer->set_oid(old_object_oid);
    clovis_writer->delete_object(
        std::bind(&S3PostCompleteAction::remove_old_object_version_metadata,
                  this),
        std::bind(&S3PostCompleteAction::next, this), old_layout_id);
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::remove_old_object_version_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  object_metadata = object_metadata_factory->create_object_metadata_obj(
      request, bucket_metadata->get_object_list_index_oid());
  object_metadata->set_objects_version_list_index_oid(
      bucket_metadata->get_objects_version_list_index_oid());

  assert(multipart_metadata->get_object_name() == request->get_object_name());
  object_metadata->set_oid(old_object_oid);
  object_metadata->set_layout_id(old_layout_id);
  object_metadata->set_version_id(multipart_metadata->get_old_obj_version_id());

  object_metadata->remove_version_metadata(
      std::bind(&S3PostCompleteAction::remove_old_oid_probable_record, this),
      std::bind(&S3PostCompleteAction::next, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::remove_old_oid_probable_record() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  assert(!old_oid_str.empty());
  assert(!new_oid_str.empty());

  // key = oldoid + "-" + newoid
  std::string old_oid_rec_key = old_oid_str + '-' + new_oid_str;

  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }
  clovis_kv_writer->delete_keyval(global_probable_dead_object_list_index_oid,
                                  old_oid_rec_key,
                                  std::bind(&S3PostCompleteAction::next, this),
                                  std::bind(&S3PostCompleteAction::next, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::mark_new_oid_for_deletion() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  assert(!new_oid_str.empty());
  assert(is_abort_multipart());

  // update new oid, key = newoid, force_del = true
  new_probable_del_rec->set_force_delete(true);

  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }
  clovis_kv_writer->put_keyval(global_probable_dead_object_list_index_oid,
                               new_oid_str, new_probable_del_rec->to_json(),
                               std::bind(&S3PostCompleteAction::next, this),
                               std::bind(&S3PostCompleteAction::next, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::delete_new_object() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  assert(new_object_oid.u_hi || new_object_oid.u_lo);
  assert(is_abort_multipart());

  clovis_writer->set_oid(new_object_oid);
  clovis_writer->delete_object(
      std::bind(&S3PostCompleteAction::remove_new_oid_probable_record, this),
      std::bind(&S3PostCompleteAction::next, this), layout_id);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::remove_new_oid_probable_record() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  assert(!new_oid_str.empty());

  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }
  clovis_kv_writer->delete_keyval(global_probable_dead_object_list_index_oid,
                                  new_oid_str,
                                  std::bind(&S3PostCompleteAction::next, this),
                                  std::bind(&S3PostCompleteAction::next, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PostCompleteAction::set_authorization_meta() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  auth_client->set_acl_and_policy(bucket_metadata->get_encoded_bucket_acl(),
                                  bucket_metadata->get_policy_as_json());
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
