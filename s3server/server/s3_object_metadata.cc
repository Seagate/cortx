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

#include <json/json.h>
#include <stdlib.h>
#include "base64.h"

#include "s3_datetime.h"
#include "s3_factory.h"
#include "s3_iem.h"
#include "s3_log.h"
#include "s3_object_metadata.h"
#include "s3_object_versioning_helper.h"
#include "s3_uri_to_mero_oid.h"
#include "s3_common_utilities.h"
#include "s3_m0_uint128_helper.h"
#include "s3_stats.h"

extern struct m0_uint128 global_instance_id;

void S3ObjectMetadata::initialize(bool ismultipart, std::string uploadid) {
  json_parsing_error = false;
  account_name = request->get_account_name();
  account_id = request->get_account_id();
  user_name = request->get_user_name();
  canonical_id = request->get_canonical_id();
  user_id = request->get_user_id();
  bucket_name = request->get_bucket_name();
  object_name = request->get_object_name();
  state = S3ObjectMetadataState::empty;
  is_multipart = ismultipart;
  upload_id = uploadid;
  oid = M0_CLOVIS_ID_APP;
  old_oid = {0ULL, 0ULL};
  layout_id = 0;
  old_layout_id = 0;
  tried_count = 0;

  object_key_uri = bucket_name + "\\" + object_name;

  // Set the defaults
  system_defined_attribute["Date"] = "";
  system_defined_attribute["Content-Length"] = "";
  system_defined_attribute["Last-Modified"] = "";
  system_defined_attribute["Content-MD5"] = "";
  // Initially set to requestor of API, but on load json can be overriden
  system_defined_attribute["Owner-User"] = user_name;
  system_defined_attribute["Owner-Canonical-id"] = canonical_id;
  system_defined_attribute["Owner-User-id"] = user_id;
  system_defined_attribute["Owner-Account"] = account_name;
  system_defined_attribute["Owner-Account-id"] = account_id;

  system_defined_attribute["x-amz-server-side-encryption"] =
      "None";  // Valid values aws:kms, AES256
  system_defined_attribute["x-amz-version-id"] =
      "";  // Generate if versioning enabled
  system_defined_attribute["x-amz-storage-class"] =
      "STANDARD";  // Valid Values: STANDARD | STANDARD_IA | REDUCED_REDUNDANCY
  system_defined_attribute["x-amz-website-redirect-location"] = "None";
  system_defined_attribute["x-amz-server-side-encryption-aws-kms-key-id"] = "";
  system_defined_attribute["x-amz-server-side-encryption-customer-algorithm"] =
      "";
  system_defined_attribute["x-amz-server-side-encryption-customer-key"] = "";
  system_defined_attribute["x-amz-server-side-encryption-customer-key-MD5"] =
      "";
  if (is_multipart) {
    index_name = get_multipart_index_name();
    system_defined_attribute["Upload-ID"] = upload_id;
    system_defined_attribute["Part-One-Size"] = "";
  } else {
    index_name = get_object_list_index_name();
  }

  object_list_index_oid = {0ULL, 0ULL};
  objects_version_list_index_oid = {0ULL, 0ULL};
}

S3ObjectMetadata::S3ObjectMetadata(
    std::shared_ptr<S3RequestObject> req, bool ismultipart,
    std::string uploadid,
    std::shared_ptr<S3ClovisKVSReaderFactory> kv_reader_factory,
    std::shared_ptr<S3ClovisKVSWriterFactory> kv_writer_factory,
    std::shared_ptr<ClovisAPI> clovis_api)
    : request(std::move(req)) {
  request_id = request->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  initialize(ismultipart, uploadid);

  if (clovis_api) {
    s3_clovis_api = std::move(clovis_api);
  } else {
    s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
  }

  if (kv_reader_factory) {
    clovis_kv_reader_factory = std::move(kv_reader_factory);
  } else {
    clovis_kv_reader_factory = std::make_shared<S3ClovisKVSReaderFactory>();
  }

  if (kv_writer_factory) {
    clovis_kv_writer_factory = std::move(kv_writer_factory);
  } else {
    clovis_kv_writer_factory = std::make_shared<S3ClovisKVSWriterFactory>();
  }
}

std::string S3ObjectMetadata::get_owner_id() {
  return system_defined_attribute["Owner-User-id"];
}

std::string S3ObjectMetadata::get_owner_name() {
  return system_defined_attribute["Owner-User"];
}

std::string S3ObjectMetadata::get_object_name() { return object_name; }

void S3ObjectMetadata::set_object_list_index_oid(struct m0_uint128 id) {
  object_list_index_oid.u_hi = id.u_hi;
  object_list_index_oid.u_lo = id.u_lo;
}

void S3ObjectMetadata::set_objects_version_list_index_oid(
    struct m0_uint128 id) {
  objects_version_list_index_oid.u_hi = id.u_hi;
  objects_version_list_index_oid.u_lo = id.u_lo;
}

struct m0_uint128 S3ObjectMetadata::get_object_list_index_oid() const {
  return object_list_index_oid;
}

struct m0_uint128 S3ObjectMetadata::get_objects_version_list_index_oid() const {
  return objects_version_list_index_oid;
}

void S3ObjectMetadata::regenerate_version_id() {
  // generate new epoch time value for new object
  rev_epoch_version_id_key = S3ObjectVersioingHelper::generate_new_epoch_time();
  // set version id
  object_version_id = S3ObjectVersioingHelper::get_versionid_from_epoch_time(
      rev_epoch_version_id_key);
  system_defined_attribute["x-amz-version-id"] = object_version_id;
}

std::string S3ObjectMetadata::get_version_key_in_index() {
  assert(!object_name.empty());
  assert(!rev_epoch_version_id_key.empty());
  // sample objectname/revversionkey
  return object_name + "/" + rev_epoch_version_id_key;
}

std::string S3ObjectMetadata::get_user_id() { return user_id; }

std::string S3ObjectMetadata::get_upload_id() { return upload_id; }

std::string S3ObjectMetadata::get_user_name() { return user_name; }

std::string S3ObjectMetadata::get_canonical_id() { return canonical_id; }

std::string S3ObjectMetadata::get_account_name() { return account_name; }

std::string S3ObjectMetadata::get_creation_date() {
  return system_defined_attribute["Date"];
}

std::string S3ObjectMetadata::get_last_modified() {
  return system_defined_attribute["Last-Modified"];
}

std::string S3ObjectMetadata::get_last_modified_gmt() {
  S3DateTime temp_time;
  temp_time.init_with_iso(system_defined_attribute["Last-Modified"]);
  return temp_time.get_gmtformat_string();
}

std::string S3ObjectMetadata::get_last_modified_iso() {
  // we store isofmt in json
  return system_defined_attribute["Last-Modified"];
}

void S3ObjectMetadata::reset_date_time_to_current() {
  S3DateTime current_time;
  current_time.init_current_time();
  std::string time_now = current_time.get_isoformat_string();
  system_defined_attribute["Date"] = time_now;
  system_defined_attribute["Last-Modified"] = time_now;
}

std::string S3ObjectMetadata::get_storage_class() {
  return system_defined_attribute["x-amz-storage-class"];
}

void S3ObjectMetadata::set_content_length(std::string length) {
  system_defined_attribute["Content-Length"] = length;
}

size_t S3ObjectMetadata::get_content_length() {
  return atol(system_defined_attribute["Content-Length"].c_str());
}

std::string S3ObjectMetadata::get_content_length_str() {
  return system_defined_attribute["Content-Length"];
}

void S3ObjectMetadata::set_part_one_size(const size_t& part_size) {
  system_defined_attribute["Part-One-Size"] = std::to_string(part_size);
}

size_t S3ObjectMetadata::get_part_one_size() {
  if (system_defined_attribute["Part-One-Size"] == "") {
    return 0;
  } else {
    return std::stoul(system_defined_attribute["Part-One-Size"].c_str());
  }
}

void S3ObjectMetadata::set_md5(std::string md5) {
  system_defined_attribute["Content-MD5"] = md5;
}

std::string S3ObjectMetadata::get_md5() {
  return system_defined_attribute["Content-MD5"];
}

void S3ObjectMetadata::set_oid(struct m0_uint128 id) {
  oid = id;
  mero_oid_str = S3M0Uint128Helper::to_string(oid);
}

void S3ObjectMetadata::set_version_id(std::string ver_id) {
  object_version_id = ver_id;
  rev_epoch_version_id_key =
      S3ObjectVersioingHelper::generate_keyid_from_versionid(object_version_id);
}

void S3ObjectMetadata::set_old_oid(struct m0_uint128 id) {
  old_oid = id;
  mero_old_oid_str = S3M0Uint128Helper::to_string(old_oid);
}

void S3ObjectMetadata::set_old_version_id(std::string old_obj_ver_id) {
  mero_old_object_version_id = old_obj_ver_id;
}

void S3ObjectMetadata::set_part_index_oid(struct m0_uint128 id) {
  part_index_oid = id;
  mero_part_oid_str = S3M0Uint128Helper::to_string(part_index_oid);
}

void S3ObjectMetadata::add_system_attribute(std::string key, std::string val) {
  system_defined_attribute[key] = val;
}

void S3ObjectMetadata::add_user_defined_attribute(std::string key,
                                                  std::string val) {
  user_defined_attribute[key] = val;
}

void S3ObjectMetadata::validate() {
  // TODO
}

void S3ObjectMetadata::load(std::function<void(void)> on_success,
                            std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  // object_list_index_oid should be set before using this method
  assert(object_list_index_oid.u_hi || object_list_index_oid.u_lo);

  s3_timer.start();

  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  clovis_kv_reader = clovis_kv_reader_factory->create_clovis_kvs_reader(
      request, s3_clovis_api);
  clovis_kv_reader->get_keyval(
      object_list_index_oid, object_name,
      std::bind(&S3ObjectMetadata::load_successful, this),
      std::bind(&S3ObjectMetadata::load_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ObjectMetadata::load_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Object metadata load successful\n");
  if (this->from_json(clovis_kv_reader->get_value()) != 0) {
    s3_log(S3_LOG_ERROR, request_id,
           "Json Parsing failed. Index oid = "
           "%" SCNx64 " : %" SCNx64 ", Key = %s, Value = %s\n",
           object_list_index_oid.u_hi, object_list_index_oid.u_lo,
           object_name.c_str(), clovis_kv_reader->get_value().c_str());
    s3_iem(LOG_ERR, S3_IEM_METADATA_CORRUPTED, S3_IEM_METADATA_CORRUPTED_STR,
           S3_IEM_METADATA_CORRUPTED_JSON);

    json_parsing_error = true;
    load_failed();
  } else {
    s3_timer.stop();
    const auto mss = s3_timer.elapsed_time_in_millisec();
    LOG_PERF("load_object_metadata_ms", request_id.c_str(), mss);
    s3_stats_timing("load_object_metadata", mss);

    state = S3ObjectMetadataState::present;
    this->handler_on_success();
  }
}

void S3ObjectMetadata::load_failed() {
  if (json_parsing_error) {
    state = S3ObjectMetadataState::failed;
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::missing) {
    s3_log(S3_LOG_DEBUG, request_id, "Object metadata missing for %s\n",
           object_name.c_str());
    state = S3ObjectMetadataState::missing;  // Missing
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::failed_to_launch) {
    s3_log(S3_LOG_WARN, request_id, "Object metadata load failed\n");
    state = S3ObjectMetadataState::failed_to_launch;
  } else {
    s3_log(S3_LOG_WARN, request_id, "Object metadata load failed\n");
    state = S3ObjectMetadataState::failed;
  }
  this->handler_on_failed();
}

void S3ObjectMetadata::save(std::function<void(void)> on_success,
                            std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id, "Saving Object metadata\n");

  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;
  if (is_multipart) {
    // Write only to multpart object list and not real object list in a bucket.
    save_metadata();
  } else {
    // First write metadata to objects version list index for a bucket.
    // Next write metadata to object list index for a bucket.
    save_version_metadata();
  }
}

// Save to objects version list index
void S3ObjectMetadata::save_version_metadata() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  // objects_version_list_index_oid should be set before using this method
  assert(objects_version_list_index_oid.u_hi ||
         objects_version_list_index_oid.u_lo);

  clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
      request, s3_clovis_api);
  clovis_kv_writer->put_keyval(
      objects_version_list_index_oid, get_version_key_in_index(),
      this->version_entry_to_json(),
      std::bind(&S3ObjectMetadata::save_version_metadata_successful, this),
      std::bind(&S3ObjectMetadata::save_version_metadata_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ObjectMetadata::save_version_metadata_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Version metadata saved for Object [%s].\n",
         object_name.c_str());
  save_metadata();
}

void S3ObjectMetadata::save_version_metadata_failed() {
  s3_log(S3_LOG_ERROR, request_id,
         "Version metadata save failed for Object [%s].\n",
         object_name.c_str());
  if (clovis_kv_writer->get_state() ==
      S3ClovisKVSWriterOpState::failed_to_launch) {
    state = S3ObjectMetadataState::failed_to_launch;
  } else {
    state = S3ObjectMetadataState::failed;
  }
  this->handler_on_failed();
}

void S3ObjectMetadata::save_metadata() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  // object_list_index_oid should be set before using this method
  assert(object_list_index_oid.u_hi || object_list_index_oid.u_lo);

  clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
      request, s3_clovis_api);
  clovis_kv_writer->put_keyval(
      object_list_index_oid, object_name, this->to_json(),
      std::bind(&S3ObjectMetadata::save_metadata_successful, this),
      std::bind(&S3ObjectMetadata::save_metadata_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

// Save to objects list index
void S3ObjectMetadata::save_metadata(std::function<void(void)> on_success,
                                     std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;
  save_metadata();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ObjectMetadata::save_metadata_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Object metadata saved for Object [%s].\n",
         object_name.c_str());
  state = S3ObjectMetadataState::saved;
  this->handler_on_success();
}

void S3ObjectMetadata::save_metadata_failed() {
  s3_log(S3_LOG_ERROR, request_id,
         "Object metadata save failed for Object [%s].\n", object_name.c_str());
  if (clovis_kv_writer->get_state() ==
      S3ClovisKVSWriterOpState::failed_to_launch) {
    state = S3ObjectMetadataState::failed_to_launch;
  } else {
    state = S3ObjectMetadataState::failed;
  }
  this->handler_on_failed();
}

void S3ObjectMetadata::remove(std::function<void(void)> on_success,
                              std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id,
         "Deleting Object metadata for Object [%s].\n", object_name.c_str());
  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;
  // Remove Object metadata from object list followed by
  // removing version entry from version list.
  remove_object_metadata();
}

void S3ObjectMetadata::remove_object_metadata() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  // object_list_index_oid should be set before using this method
  assert(object_list_index_oid.u_hi || object_list_index_oid.u_lo);

  clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
      request, s3_clovis_api);
  clovis_kv_writer->delete_keyval(
      object_list_index_oid, object_name,
      std::bind(&S3ObjectMetadata::remove_object_metadata_successful, this),
      std::bind(&S3ObjectMetadata::remove_object_metadata_failed, this));
}

void S3ObjectMetadata::remove_object_metadata_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Deleted metadata for Object [%s].\n",
         object_name.c_str());
  if (is_multipart) {
    // In multipart, version entry is not yet created.
    state = S3ObjectMetadataState::deleted;
    this->handler_on_success();
  } else {
    remove_version_metadata();
  }
}

void S3ObjectMetadata::remove_object_metadata_failed() {
  s3_log(S3_LOG_DEBUG, request_id,
         "Delete Object metadata failed for Object [%s].\n",
         object_name.c_str());
  if (clovis_kv_writer->get_state() ==
      S3ClovisKVSWriterOpState::failed_to_launch) {
    state = S3ObjectMetadataState::failed_to_launch;
  } else {
    state = S3ObjectMetadataState::failed;
  }
  this->handler_on_failed();
}

void S3ObjectMetadata::remove_version_metadata(
    std::function<void(void)> on_success, std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id,
         "Deleting Object Version metadata for Object [%s].\n",
         object_name.c_str());
  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;
  remove_version_metadata();
}

void S3ObjectMetadata::remove_version_metadata() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  // objects_version_list_index_oid should be set before using this method
  assert(objects_version_list_index_oid.u_hi ||
         objects_version_list_index_oid.u_lo);

  clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
      request, s3_clovis_api);
  clovis_kv_writer->delete_keyval(
      objects_version_list_index_oid, get_version_key_in_index(),
      std::bind(&S3ObjectMetadata::remove_version_metadata_successful, this),
      std::bind(&S3ObjectMetadata::remove_version_metadata_failed, this));
}

void S3ObjectMetadata::remove_version_metadata_successful() {
  s3_log(S3_LOG_DEBUG, request_id,
         "Deleted [%s] Version metadata for Object [%s].\n",
         object_version_id.c_str(), object_name.c_str());
  state = S3ObjectMetadataState::deleted;
  this->handler_on_success();
}

void S3ObjectMetadata::remove_version_metadata_failed() {
  s3_log(S3_LOG_DEBUG, request_id,
         "Delete Version [%s] metadata failed for Object [%s].\n",
         object_version_id.c_str(), object_name.c_str());
  // We still treat remove object metadata as successful as from S3 client
  // perspective object is gone. Version can be clean up by backgrounddelete
  state = S3ObjectMetadataState::deleted;
  this->handler_on_success();
}

// Streaming to json
std::string S3ObjectMetadata::to_json() {
  s3_log(S3_LOG_DEBUG, request_id, "Called\n");
  Json::Value root;
  root["Bucket-Name"] = bucket_name;
  root["Object-Name"] = object_name;
  root["Object-URI"] = object_key_uri;
  root["layout_id"] = layout_id;

  if (is_multipart) {
    root["Upload-ID"] = upload_id;
    root["mero_part_oid"] = mero_part_oid_str;
    root["mero_old_oid"] = mero_old_oid_str;
    root["old_layout_id"] = old_layout_id;
    root["mero_old_object_version_id"] = mero_old_object_version_id;
  }

  root["mero_oid"] = mero_oid_str;

  for (auto sit : system_defined_attribute) {
    root["System-Defined"][sit.first] = sit.second;
  }
  for (auto uit : user_defined_attribute) {
    root["User-Defined"][uit.first] = uit.second;
  }
  for (const auto& tag : object_tags) {
    root["User-Defined-Tags"][tag.first] = tag.second;
  }
  if (encoded_acl == "") {

    root["ACL"] = request->get_default_acl();

  } else {
    root["ACL"] = encoded_acl;
  }

  S3DateTime current_time;
  current_time.init_current_time();
  root["create_timestamp"] = current_time.get_isoformat_string();

  Json::FastWriter fastWriter;
  return fastWriter.write(root);
  ;
}

// Streaming to json
std::string S3ObjectMetadata::version_entry_to_json() {
  s3_log(S3_LOG_DEBUG, request_id, "Called\n");
  Json::Value root;
  // Processing version entry currently only needs minimal information
  // In future when real S3 versioning is supported, this method will not be
  // required and we can simply use to_json.

  // root["Object-Name"] = object_name;
  root["mero_oid"] = mero_oid_str;
  root["layout_id"] = layout_id;

  S3DateTime current_time;
  current_time.init_current_time();
  root["create_timestamp"] = current_time.get_isoformat_string();

  Json::FastWriter fastWriter;
  return fastWriter.write(root);
  ;
}

/*
 *  <IEM_INLINE_DOCUMENTATION>
 *    <event_code>047006003</event_code>
 *    <application>S3 Server</application>
 *    <submodule>S3 Actions</submodule>
 *    <description>Metadata corrupted causing Json parsing failure</description>
 *    <audience>Development</audience>
 *    <details>
 *      Json parsing failed due to metadata corruption.
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

int S3ObjectMetadata::from_json(std::string content) {
  s3_log(S3_LOG_DEBUG, request_id, "Called with content [%s]\n",
         content.c_str());
  Json::Value newroot;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(content.c_str(), newroot);
  if (!parsingSuccessful || s3_fi_is_enabled("object_metadata_corrupted")) {
    s3_log(S3_LOG_ERROR, request_id, "Json Parsing failed\n");
    return -1;
  }

  bucket_name = newroot["Bucket-Name"].asString();
  object_name = newroot["Object-Name"].asString();
  object_key_uri = newroot["Object-URI"].asString();
  upload_id = newroot["Upload-ID"].asString();
  mero_part_oid_str = newroot["mero_part_oid"].asString();

  mero_oid_str = newroot["mero_oid"].asString();
  layout_id = newroot["layout_id"].asInt();

  oid = S3M0Uint128Helper::to_m0_uint128(mero_oid_str);

  //
  // Old oid is needed to remove the OID when the object already exists
  // during upload, this is needed in case of multipart upload only
  // As upload happens in multiple sessions, so this old oid
  // will be used in post complete action.
  //
  if (is_multipart) {
    mero_old_oid_str = newroot["mero_old_oid"].asString();
    old_oid = S3M0Uint128Helper::to_m0_uint128(mero_old_oid_str);
    old_layout_id = newroot["old_layout_id"].asInt();
    mero_old_object_version_id =
        newroot["mero_old_object_version_id"].asString();
  }

  part_index_oid = S3M0Uint128Helper::to_m0_uint128(mero_part_oid_str);

  Json::Value::Members members = newroot["System-Defined"].getMemberNames();
  for (auto it : members) {
    system_defined_attribute[it.c_str()] =
        newroot["System-Defined"][it].asString().c_str();
  }
  user_name = system_defined_attribute["Owner-User"];
  canonical_id = system_defined_attribute["Owner-Canonical-id"];
  user_id = system_defined_attribute["Owner-User-id"];
  account_name = system_defined_attribute["Owner-Account"];
  account_id = system_defined_attribute["Owner-Account-id"];
  object_version_id = system_defined_attribute["x-amz-version-id"];
  rev_epoch_version_id_key =
      S3ObjectVersioingHelper::generate_keyid_from_versionid(object_version_id);

  members = newroot["User-Defined"].getMemberNames();
  for (auto it : members) {
    user_defined_attribute[it.c_str()] =
        newroot["User-Defined"][it].asString().c_str();
  }
  members = newroot["User-Defined-Tags"].getMemberNames();
  for (const auto& tag : members) {
    object_tags[tag] = newroot["User-Defined-Tags"][tag].asString();
  }
  acl_from_json(newroot["ACL"].asString());

  return 0;
}

void S3ObjectMetadata::acl_from_json(std::string acl_json_str) {
  s3_log(S3_LOG_DEBUG, "", "Called\n");
  encoded_acl = acl_json_str;
}

std::string& S3ObjectMetadata::get_encoded_object_acl() {
  // base64 encoded acl
  return encoded_acl;
}

void S3ObjectMetadata::setacl(const std::string& input_acl) {
  encoded_acl = input_acl;
}

std::string S3ObjectMetadata::get_acl_as_xml() {
  return base64_decode(encoded_acl);
}

void S3ObjectMetadata::set_tags(
    const std::map<std::string, std::string>& tags_as_map) {
  object_tags = tags_as_map;
}

const std::map<std::string, std::string>& S3ObjectMetadata::get_tags() {
  return object_tags;
}

void S3ObjectMetadata::delete_object_tags() { object_tags.clear(); }

std::string S3ObjectMetadata::get_tags_as_xml() {

  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  std::string user_defined_tags;
  std::string tags_as_xml_str;

  for (const auto& tag : object_tags) {
    user_defined_tags +=
        "<Tag>" + S3CommonUtilities::format_xml_string("Key", tag.first) +
        S3CommonUtilities::format_xml_string("Value", tag.second) + "</Tag>";
  }

  tags_as_xml_str =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<Tagging xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
      "<TagSet>" +
      user_defined_tags +
      "</TagSet>"
      "</Tagging>";
  s3_log(S3_LOG_DEBUG, request_id, "Tags xml: %s\n", tags_as_xml_str.c_str());
  s3_log(S3_LOG_INFO, request_id, "Exiting\n");
  return tags_as_xml_str;
}

bool S3ObjectMetadata::check_object_tags_exists() {
  return !object_tags.empty();
}

int S3ObjectMetadata::object_tags_count() { return object_tags.size(); }

