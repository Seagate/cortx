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
 * Original author:  Rajesh Nambiar  <rajesh.nambiar@seagate.com>
 * Original creation date: 21-Jan-2016
 */

#include <json/json.h>
#include <stdlib.h>

#include "s3_datetime.h"
#include "s3_factory.h"
#include "s3_iem.h"
#include "s3_log.h"
#include "s3_part_metadata.h"

void S3PartMetadata::initialize(std::string uploadid, int part_num) {
  json_parsing_error = false;
  bucket_name = request->get_bucket_name();
  object_name = request->get_object_name();
  state = S3PartMetadataState::empty;
  upload_id = uploadid;
  part_number = std::to_string(part_num);
  put_metadata = true;
  index_name = get_part_index_name();
  salt = "index_salt_";
  collision_attempt_count = 0;
  s3_clovis_api = std::make_shared<ConcreteClovisAPI>();

  // Set the defaults
  system_defined_attribute["Date"] = "";
  system_defined_attribute["Content-Length"] = "";
  system_defined_attribute["Last-Modified"] = "";
  system_defined_attribute["Content-MD5"] = "";

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
}

S3PartMetadata::S3PartMetadata(
    std::shared_ptr<S3RequestObject> req, std::string uploadid, int part_num,
    std::shared_ptr<S3ClovisKVSReaderFactory> kv_reader_factory,
    std::shared_ptr<S3ClovisKVSWriterFactory> kv_writer_factory)
    : request(req) {
  request_id = request->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  initialize(uploadid, part_num);
  part_index_name_oid = {0ULL, 0ULL};

  if (kv_reader_factory) {
    clovis_kv_reader_factory = kv_reader_factory;
  } else {
    clovis_kv_reader_factory = std::make_shared<S3ClovisKVSReaderFactory>();
  }

  if (kv_writer_factory) {
    clovis_kv_writer_factory = kv_writer_factory;
  } else {
    clovis_kv_writer_factory = std::make_shared<S3ClovisKVSWriterFactory>();
  }
}

S3PartMetadata::S3PartMetadata(
    std::shared_ptr<S3RequestObject> req, struct m0_uint128 oid,
    std::string uploadid, int part_num,
    std::shared_ptr<S3ClovisKVSReaderFactory> kv_reader_factory,
    std::shared_ptr<S3ClovisKVSWriterFactory> kv_writer_factory)
    : request(req) {
  request_id = request->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  initialize(uploadid, part_num);
  part_index_name_oid = oid;

  if (kv_reader_factory) {
    clovis_kv_reader_factory = kv_reader_factory;
  } else {
    clovis_kv_reader_factory = std::make_shared<S3ClovisKVSReaderFactory>();
  }

  if (kv_writer_factory) {
    clovis_kv_writer_factory = kv_writer_factory;
  } else {
    clovis_kv_writer_factory = std::make_shared<S3ClovisKVSWriterFactory>();
  }
}

std::string S3PartMetadata::get_object_name() { return object_name; }

std::string S3PartMetadata::get_upload_id() { return upload_id; }

std::string S3PartMetadata::get_part_number() { return part_number; }

std::string S3PartMetadata::get_last_modified() {
  return system_defined_attribute["Last-Modified"];
}

std::string S3PartMetadata::get_last_modified_gmt() {
  S3DateTime temp_time;
  temp_time.init_with_iso(system_defined_attribute["Last-Modified"]);
  return temp_time.get_gmtformat_string();
}

std::string S3PartMetadata::get_last_modified_iso() {
  // we store isofmt in json
  return system_defined_attribute["Last-Modified"];
}

void S3PartMetadata::reset_date_time_to_current() {
  // we store isofmt in json
  S3DateTime current_time;
  current_time.init_current_time();
  std::string time_now = current_time.get_isoformat_string();
  system_defined_attribute["Date"] = time_now;
  system_defined_attribute["Last-Modified"] = time_now;
}

std::string S3PartMetadata::get_storage_class() {
  return system_defined_attribute["x-amz-storage-class"];
}

void S3PartMetadata::set_content_length(std::string length) {
  system_defined_attribute["Content-Length"] = length;
}

size_t S3PartMetadata::get_content_length() {
  return atol(system_defined_attribute["Content-Length"].c_str());
}

std::string S3PartMetadata::get_content_length_str() {
  return system_defined_attribute["Content-Length"];
}

void S3PartMetadata::set_md5(std::string md5) {
  system_defined_attribute["Content-MD5"] = md5;
}

std::string S3PartMetadata::get_md5() {
  return system_defined_attribute["Content-MD5"];
}

void S3PartMetadata::add_system_attribute(std::string key, std::string val) {
  system_defined_attribute[key] = val;
}

void S3PartMetadata::add_user_defined_attribute(std::string key,
                                                std::string val) {
  user_defined_attribute[key] = val;
}

void S3PartMetadata::load(std::function<void(void)> on_success,
                          std::function<void(void)> on_failed,
                          int part_num = 1) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  str_part_num = "";
  if (part_num > 0) {
    str_part_num = std::to_string(part_num);
  }
  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  clovis_kv_reader = clovis_kv_reader_factory->create_clovis_kvs_reader(
      request, s3_clovis_api);

  clovis_kv_reader->get_keyval(
      part_index_name_oid, str_part_num,
      std::bind(&S3PartMetadata::load_successful, this),
      std::bind(&S3PartMetadata::load_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PartMetadata::load_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Found part metadata\n");
  if (this->from_json(clovis_kv_reader->get_value()) != 0) {
    s3_log(S3_LOG_ERROR, request_id,
           "Json Parsing failed. Index oid = "
           "%" SCNx64 " : %" SCNx64 ", Key = %s, Value = %s\n",
           part_index_name_oid.u_hi, part_index_name_oid.u_lo,
           str_part_num.c_str(), clovis_kv_reader->get_value().c_str());
    s3_iem(LOG_ERR, S3_IEM_METADATA_CORRUPTED, S3_IEM_METADATA_CORRUPTED_STR,
           S3_IEM_METADATA_CORRUPTED_JSON);

    json_parsing_error = true;
    load_failed();
  } else {
    state = S3PartMetadataState::present;
    this->handler_on_success();
  }
}

void S3PartMetadata::load_failed() {
  s3_log(S3_LOG_WARN, request_id, "Missing part metadata\n");
  if (json_parsing_error) {
    state = S3PartMetadataState::failed;
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::missing) {
    state = S3PartMetadataState::missing;  // Missing
  } else {
    state = S3PartMetadataState::failed;
  }
  this->handler_on_failed();
}

void S3PartMetadata::create_index(std::function<void(void)> on_success,
                                  std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;
  put_metadata = false;
  create_part_index();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PartMetadata::save(std::function<void(void)> on_success,
                          std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;
  put_metadata = true;
  save_metadata();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PartMetadata::create_part_index() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  // Mark missing as we initiate write, in case it fails to write.
  state = S3PartMetadataState::missing;

  clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
      request, s3_clovis_api);

  clovis_kv_writer->create_index(
      index_name,
      std::bind(&S3PartMetadata::create_part_index_successful, this),
      std::bind(&S3PartMetadata::create_part_index_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PartMetadata::create_part_index_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Created index for part info\n");
  part_index_name_oid = clovis_kv_writer->get_oid();
  if (put_metadata) {
    save_metadata();
  } else {
    state = S3PartMetadataState::store_created;
    this->handler_on_success();
  }
}

void S3PartMetadata::create_part_index_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Failed to create index for part info\n");
  if (clovis_kv_writer->get_state() == S3ClovisKVSWriterOpState::exists) {
    // Since part index name comprises of bucket name + object name + upload id,
    // upload id is unique
    // hence if clovis returns exists then its due to collision, resolve it
    s3_log(S3_LOG_WARN, request_id, "Collision detected for Index %s\n",
           index_name.c_str());
    handle_collision();
  } else if (clovis_kv_writer->get_state() ==
             S3ClovisKVSWriterOpState::failed_to_launch) {
    state = S3PartMetadataState::failed_to_launch;
    this->handler_on_failed();
  } else {
    state = S3PartMetadataState::failed;  // todo Check error
    this->handler_on_failed();
  }
}

void S3PartMetadata::save_metadata() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  // Set up system attributes
  system_defined_attribute["Upload-ID"] = upload_id;
  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }
  clovis_kv_writer->put_keyval(
      part_index_name_oid, part_number, this->to_json(),
      std::bind(&S3PartMetadata::save_metadata_successful, this),
      std::bind(&S3PartMetadata::save_metadata_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PartMetadata::save_metadata_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Saved part metadata\n");
  state = S3PartMetadataState::saved;
  this->handler_on_success();
}

void S3PartMetadata::save_metadata_failed() {
  // TODO - do anything more for failure?
  s3_log(S3_LOG_ERROR, request_id, "Failed to save part metadata\n");
  if (clovis_kv_writer->get_state() ==
      S3ClovisKVSWriterOpState::failed_to_launch) {
    state = S3PartMetadataState::failed_to_launch;
  } else {
    state = S3PartMetadataState::failed;
  }
  this->handler_on_failed();
}

void S3PartMetadata::remove(std::function<void(void)> on_success,
                            std::function<void(void)> on_failed,
                            int remove_part = 0) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  std::string part_removal = std::to_string(remove_part);

  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  s3_log(S3_LOG_DEBUG, request_id, "Deleting part info for part = %s\n",
         part_removal.c_str());

  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }
  clovis_kv_writer->delete_keyval(
      part_index_name_oid, part_removal,
      std::bind(&S3PartMetadata::remove_successful, this),
      std::bind(&S3PartMetadata::remove_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PartMetadata::remove_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Deleted part info\n");
  state = S3PartMetadataState::deleted;
  this->handler_on_success();
}

void S3PartMetadata::remove_failed() {
  s3_log(S3_LOG_WARN, request_id, "Failed to delete part info\n");
  if (clovis_kv_writer->get_state() ==
      S3ClovisKVSWriterOpState::failed_to_launch) {
    state = S3PartMetadataState::failed_to_launch;
  } else {
    state = S3PartMetadataState::failed;
  }
  this->handler_on_failed();
}

void S3PartMetadata::remove_index(std::function<void(void)> on_success,
                                  std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;
  if (!clovis_kv_writer) {
    clovis_kv_writer = clovis_kv_writer_factory->create_clovis_kvs_writer(
        request, s3_clovis_api);
  }
  clovis_kv_writer->delete_index(
      part_index_name_oid,
      std::bind(&S3PartMetadata::remove_index_successful, this),
      std::bind(&S3PartMetadata::remove_index_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PartMetadata::remove_index_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Deleted index for part info\n");
  state = S3PartMetadataState::index_deleted;
  this->handler_on_success();
}

void S3PartMetadata::remove_index_failed() {
  s3_log(S3_LOG_WARN, request_id, "Failed to remove index for part info\n");
  s3_iem(LOG_ERR, S3_IEM_DELETE_IDX_FAIL, S3_IEM_DELETE_IDX_FAIL_STR,
         S3_IEM_DELETE_IDX_FAIL_JSON);
  if (clovis_kv_writer->get_state() == S3ClovisKVSWriterOpState::failed) {
    state = S3PartMetadataState::failed;
  } else if (clovis_kv_writer->get_state() ==
             S3ClovisKVSWriterOpState::failed_to_launch) {
    state = S3PartMetadataState::failed_to_launch;
  }
  this->handler_on_failed();
}

// Streaming to json
std::string S3PartMetadata::to_json() {
  s3_log(S3_LOG_DEBUG, request_id, "\n");
  Json::Value root;
  root["Bucket-Name"] = bucket_name;
  root["Object-Name"] = object_name;
  root["Upload-ID"] = upload_id;
  root["Part-Num"] = part_number;

  for (auto sit : system_defined_attribute) {
    root["System-Defined"][sit.first] = sit.second;
  }
  for (auto uit : user_defined_attribute) {
    root["User-Defined"][uit.first] = uit.second;
  }
  Json::FastWriter fastWriter;
  return fastWriter.write(root);
}

int S3PartMetadata::from_json(std::string content) {
  s3_log(S3_LOG_DEBUG, request_id, "\n");
  Json::Value newroot;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(content.c_str(), newroot);
  if (!parsingSuccessful || s3_fi_is_enabled("part_metadata_corrupted")) {
    s3_log(S3_LOG_ERROR, request_id, "Json Parsing failed.\n");
    return -1;
  }

  bucket_name = newroot["Bucket-Name"].asString();
  object_name = newroot["Object-Name"].asString();
  upload_id = newroot["Upload-ID"].asString();
  part_number = newroot["Part-Num"].asString();

  Json::Value::Members members = newroot["System-Defined"].getMemberNames();
  for (auto it : members) {
    system_defined_attribute[it.c_str()] =
        newroot["System-Defined"][it].asString().c_str();
  }
  members = newroot["User-Defined"].getMemberNames();
  for (auto it : members) {
    user_defined_attribute[it.c_str()] =
        newroot["User-Defined"][it].asString().c_str();
  }

  return 0;
}

void S3PartMetadata::regenerate_new_indexname() {
  index_name = index_name + salt + std::to_string(collision_attempt_count);
}

void S3PartMetadata::handle_collision() {
  if (collision_attempt_count < MAX_COLLISION_RETRY_COUNT) {
    s3_log(S3_LOG_INFO, request_id,
           "Object ID collision happened for index %s\n", index_name.c_str());
    // Handle Collision
    regenerate_new_indexname();
    collision_attempt_count++;
    if (collision_attempt_count > 5) {
      s3_log(S3_LOG_INFO, request_id,
             "Object ID collision happened %zu times for index %s\n",
             collision_attempt_count, index_name.c_str());
    }
    create_part_index();
  } else {
    if (collision_attempt_count >= MAX_COLLISION_RETRY_COUNT) {
      s3_log(S3_LOG_ERROR, request_id,
             "Failed to resolve object id collision %zu times for index %s\n",
             collision_attempt_count, index_name.c_str());
      s3_iem(LOG_ERR, S3_IEM_COLLISION_RES_FAIL, S3_IEM_COLLISION_RES_FAIL_STR,
             S3_IEM_COLLISION_RES_FAIL_JSON);
    }
    state = S3PartMetadataState::failed;
    this->handler_on_failed();
  }
}
