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

#include "s3_delete_object_action.h"
#include "s3_error_codes.h"
#include "s3_iem.h"
#include "s3_m0_uint128_helper.h"

extern struct m0_uint128 global_probable_dead_object_list_index_oid;

S3DeleteObjectAction::S3DeleteObjectAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory,
    std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory,
    std::shared_ptr<S3ClovisWriterFactory> writer_factory,
    std::shared_ptr<S3ClovisKVSWriterFactory> kv_writer_factory,
    std::shared_ptr<ClovisAPI> clovis_api)
    : S3ObjectAction(std::move(req), std::move(bucket_meta_factory),
                     std::move(object_meta_factory), false) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  s3_log(S3_LOG_INFO, request_id,
         "S3 API: Delete Object API. Bucket[%s] Object[%s]\n",
         request->get_bucket_name().c_str(),
         request->get_object_name().c_str());

  s3_del_obj_action_state = S3DeleteObjectActionState::empty;

  if (clovis_api) {
    s3_clovis_api = clovis_api;
  } else {
    s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
  }

  if (writer_factory) {
    clovis_writer_factory = writer_factory;
  } else {
    clovis_writer_factory = std::make_shared<S3ClovisWriterFactory>();
  }

  if (kv_writer_factory) {
    clovis_kv_writer_factory = kv_writer_factory;
  } else {
    clovis_kv_writer_factory = std::make_shared<S3ClovisKVSWriterFactory>();
  }

  setup_steps();
}

void S3DeleteObjectAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setup the action\n");
  // We delete metadata first followed by object, since if we delete
  // the object first and say for some reason metadata clean up fails,
  // If this "oid" gets allocated to some other object, current object
  // metadata will point to someone else's object leading 2 problems:
  // accessing someone else's object and second retrying delete, deleting
  // someone else's object. Whereas deleting metadata first will worst case
  // lead to object leak in mero which can handle separately.
  // To delete stale objects: ref: MINT-602
  ACTION_TASK_ADD(
      S3DeleteObjectAction::add_object_oid_to_probable_dead_oid_list, this);
  ACTION_TASK_ADD(S3DeleteObjectAction::delete_metadata, this);
  ACTION_TASK_ADD(S3DeleteObjectAction::send_response_to_s3_client, this);
  // ...
}

void S3DeleteObjectAction::fetch_bucket_info_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_del_obj_action_state = S3DeleteObjectActionState::validationFailed;
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

void S3DeleteObjectAction::fetch_object_info_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_del_obj_action_state = S3DeleteObjectActionState::validationFailed;
  if ((object_list_oid.u_hi == 0ULL && object_list_oid.u_lo == 0ULL) ||
      (object_metadata->get_state() == S3ObjectMetadataState::missing)) {
    s3_log(S3_LOG_WARN, request_id, "Object not found\n");
  } else if (object_metadata->get_state() ==
             S3ObjectMetadataState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Object metadata load operation failed due to pre launch failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    s3_log(S3_LOG_WARN, request_id, "Failed to look up Object metadata\n");
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteObjectAction::add_object_oid_to_probable_dead_oid_list() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  oid_str = S3M0Uint128Helper::to_string(object_metadata->get_oid());
  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }

  probable_delete_rec.reset(new S3ProbableDeleteRecord(
      oid_str, {0ULL, 0ULL}, object_metadata->get_object_name(),
      object_metadata->get_oid(), object_metadata->get_layout_id(),
      bucket_metadata->get_object_list_index_oid(),
      bucket_metadata->get_objects_version_list_index_oid(),
      object_metadata->get_version_key_in_index(), false /* force_delete */));

  clovis_kv_writer->put_keyval(
      global_probable_dead_object_list_index_oid, oid_str,
      probable_delete_rec->to_json(),
      std::bind(&S3DeleteObjectAction::next, this),
      std::bind(&S3DeleteObjectAction::
                     add_object_oid_to_probable_dead_oid_list_failed,
                this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteObjectAction::add_object_oid_to_probable_dead_oid_list_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_del_obj_action_state =
      S3DeleteObjectActionState::probableEntryRecordFailed;
  set_s3_error("InternalError");
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteObjectAction::delete_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  object_metadata->remove(
      std::bind(&S3DeleteObjectAction::delete_metadata_successful, this),
      std::bind(&S3DeleteObjectAction::delete_metadata_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteObjectAction::delete_metadata_successful() {
  s3_log(S3_LOG_WARN, request_id, "Deleted Object metadata\n");
  s3_del_obj_action_state = S3DeleteObjectActionState::metadataDeleted;
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteObjectAction::delete_metadata_failed() {
  s3_log(S3_LOG_WARN, request_id, "Failed to delete Object metadata\n");
  s3_del_obj_action_state = S3DeleteObjectActionState::metadataDeleteFailed;
  set_s3_error("InternalError");
  send_response_to_s3_client();
}

void S3DeleteObjectAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (is_error_state() && !get_s3_error_code().empty()) {
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
  } else {
    assert((object_list_oid.u_hi == 0ULL && object_list_oid.u_lo == 0ULL) ||
           object_metadata->get_state() == S3ObjectMetadataState::missing ||
           object_metadata->get_state() == S3ObjectMetadataState::deleted);
    request->send_response(S3HttpSuccess204);
  }
  startcleanup();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteObjectAction::startcleanup() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // Clear task list and setup cleanup task list
  clear_tasks();

  if (s3_del_obj_action_state == S3DeleteObjectActionState::validationFailed ||
      s3_del_obj_action_state ==
          S3DeleteObjectActionState::probableEntryRecordFailed) {
    // Nothing to clean up
    done();
  } else {
    if (s3_del_obj_action_state == S3DeleteObjectActionState::metadataDeleted) {
      // Metadata deleted, so object is gone for S3 client, clean storage.
      ACTION_TASK_ADD(S3DeleteObjectAction::mark_oid_for_deletion, this);
      ACTION_TASK_ADD(S3DeleteObjectAction::delete_object, this);
      // If delete object is successful, attempt to delete probable record
      // Start running the cleanup task list
      start();
    } else if (s3_del_obj_action_state ==
               S3DeleteObjectActionState::metadataDeleteFailed) {
      // Failed to delete metadata, so object is still live, remove probable rec
      ACTION_TASK_ADD(S3DeleteObjectAction::remove_probable_record, this);
      // Start running the cleanup task list
      start();
    } else {
      s3_log(S3_LOG_INFO, request_id, "Possible bug\n");
      assert(false);
      done();
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteObjectAction::mark_oid_for_deletion() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  assert(!oid_str.empty());

  probable_delete_rec->set_force_delete(true);

  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }
  clovis_kv_writer->put_keyval(global_probable_dead_object_list_index_oid,
                               oid_str, probable_delete_rec->to_json(),
                               std::bind(&S3DeleteObjectAction::next, this),
                               std::bind(&S3DeleteObjectAction::next, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteObjectAction::delete_object() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // process to delete object
  if (!clovis_writer) {
    clovis_writer = clovis_writer_factory->create_clovis_writer(
        request, object_metadata->get_oid());
  }
  clovis_writer->delete_object(
      std::bind(&S3DeleteObjectAction::remove_probable_record, this),
      std::bind(&S3DeleteObjectAction::next, this),
      object_metadata->get_layout_id());
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3DeleteObjectAction::remove_probable_record() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  assert(!oid_str.empty());

  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }
  clovis_kv_writer->delete_keyval(global_probable_dead_object_list_index_oid,
                                  oid_str,
                                  std::bind(&S3DeleteObjectAction::next, this),
                                  std::bind(&S3DeleteObjectAction::next, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

// To delete a object, we need to check ACL of bucket
void S3DeleteObjectAction::set_authorization_meta() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  auth_client->set_acl_and_policy(bucket_metadata->get_encoded_bucket_acl(),
                                  bucket_metadata->get_policy_as_json());
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
