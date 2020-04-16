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

#include "s3_bucket_metadata.h"
#include <json/json.h>
#include <string>
#include "base64.h"
#include "s3_datetime.h"
#include "s3_factory.h"
#include "s3_iem.h"
#include "s3_uri_to_mero_oid.h"
#include "s3_common_utilities.h"
#include "s3_m0_uint128_helper.h"

S3BucketMetadata::S3BucketMetadata(
    std::shared_ptr<S3RequestObject> req, std::shared_ptr<ClovisAPI> clovis_api,
    std::shared_ptr<S3ClovisKVSReaderFactory> clovis_s3_kvs_reader_factory,
    std::shared_ptr<S3ClovisKVSWriterFactory> clovis_s3_kvs_writer_factory)
    : request(req), json_parsing_error(false) {
  request_id = request->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Constructor");
  account_name = request->get_account_name();
  account_id = request->get_account_id();
  user_name = request->get_user_name();
  user_id = request->get_user_id();
  bucket_name = request->get_bucket_name();
  state = S3BucketMetadataState::empty;
  current_op = S3BucketMetadataCurrentOp::none;
  if (clovis_api) {
    s3_clovis_api = clovis_api;
  } else {
    s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
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
  bucket_policy = "";

  collision_salt = "index_salt_";
  collision_attempt_count = 0;

  // name of the index which holds all objects key values within a bucket
  salted_object_list_index_name = get_object_list_index_name();
  salted_multipart_list_index_name = get_multipart_index_name();
  salted_objects_version_list_index_name = get_version_list_index_name();

  object_list_index_oid = {0ULL, 0ULL};  // Object List index default id
  multipart_index_oid = {0ULL, 0ULL};    // Multipart index default id
  objects_version_list_index_oid = {0ULL, 0ULL};  // objects versions list id

  // Set the defaults
  S3DateTime current_time;
  current_time.init_current_time();
  system_defined_attribute["Date"] = current_time.get_isoformat_string();
  system_defined_attribute["LocationConstraint"] = "us-west-2";
  system_defined_attribute["Owner-User"] = "";
  system_defined_attribute["Owner-User-id"] = "";
  system_defined_attribute["Owner-Account"] = "";
  system_defined_attribute["Owner-Account-id"] = "";
}

std::string S3BucketMetadata::get_bucket_name() { return bucket_name; }

std::string S3BucketMetadata::get_creation_time() {
  return system_defined_attribute["Date"];
}

std::string S3BucketMetadata::get_location_constraint() {
  return system_defined_attribute["LocationConstraint"];
}

std::string S3BucketMetadata::get_owner_id() {
  return system_defined_attribute["Owner-User-id"];
}

std::string S3BucketMetadata::get_owner_name() {
  return system_defined_attribute["Owner-User"];
}

struct m0_uint128 const S3BucketMetadata::get_multipart_index_oid() {
  return multipart_index_oid;
}

struct m0_uint128 const S3BucketMetadata::get_object_list_index_oid() {
  return object_list_index_oid;
}

struct m0_uint128 const S3BucketMetadata::get_objects_version_list_index_oid() {
  return objects_version_list_index_oid;
}

void S3BucketMetadata::set_multipart_index_oid(struct m0_uint128 oid) {
  multipart_index_oid = oid;
}

void S3BucketMetadata::set_object_list_index_oid(struct m0_uint128 oid) {
  object_list_index_oid = oid;
}

void S3BucketMetadata::set_objects_version_list_index_oid(
    struct m0_uint128 oid) {
  objects_version_list_index_oid = oid;
}

void S3BucketMetadata::set_location_constraint(std::string location) {
  system_defined_attribute["LocationConstraint"] = location;
}

void S3BucketMetadata::setpolicy(std::string& policy_str) {
  bucket_policy = policy_str;
}

void S3BucketMetadata::set_tags(
    const std::map<std::string, std::string>& tags_as_map) {
  bucket_tags = tags_as_map;
}

void S3BucketMetadata::deletepolicy() { bucket_policy = ""; }

void S3BucketMetadata::delete_bucket_tags() { bucket_tags.clear(); }

void S3BucketMetadata::setacl(const std::string& acl_str) {
  encoded_acl = acl_str;
}

void S3BucketMetadata::add_system_attribute(std::string key, std::string val) {
  system_defined_attribute[key] = val;
}

void S3BucketMetadata::add_user_defined_attribute(std::string key,
                                                  std::string val) {
  user_defined_attribute[key] = val;
}



void S3BucketMetadata::handle_collision(std::string base_index_name,
                                        std::string& salted_index_name,
                                        std::function<void()> callback) {
  if (collision_attempt_count < MAX_COLLISION_RETRY_COUNT) {
    s3_log(S3_LOG_INFO, request_id,
           "Index ID collision happened for index %s\n",
           salted_index_name.c_str());
    // Handle Collision
    regenerate_new_index_name(base_index_name, salted_index_name);
    collision_attempt_count++;
    if (collision_attempt_count > 5) {
      s3_log(S3_LOG_INFO, request_id,
             "Index ID collision happened %d times for index %s\n",
             collision_attempt_count, salted_index_name.c_str());
    }
    callback();
  } else {
    if (collision_attempt_count >= MAX_COLLISION_RETRY_COUNT) {
      s3_log(S3_LOG_ERROR, request_id,
             "Failed to resolve index id collision %d times for index %s\n",
             collision_attempt_count, salted_index_name.c_str());
      s3_iem(LOG_ERR, S3_IEM_COLLISION_RES_FAIL, S3_IEM_COLLISION_RES_FAIL_STR,
             S3_IEM_COLLISION_RES_FAIL_JSON);
    }
    state = S3BucketMetadataState::failed;
    this->handler_on_failed();
  }
}

void S3BucketMetadata::regenerate_new_index_name(
    std::string base_index_name, std::string& salted_index_name) {
  salted_index_name = base_index_name + collision_salt +
                      std::to_string(collision_attempt_count);
}

// Streaming to json
std::string S3BucketMetadata::to_json() {
  s3_log(S3_LOG_DEBUG, request_id, "Called\n");
  Json::Value root;
  root["Bucket-Name"] = bucket_name;

  for (auto sit : system_defined_attribute) {
    root["System-Defined"][sit.first] = sit.second;
  }
  for (auto uit : user_defined_attribute) {
    root["User-Defined"][uit.first] = uit.second;
  }
  if (encoded_acl == "") {
    root["ACL"] = request->get_default_acl();
  } else {
    root["ACL"] = encoded_acl;
  }
  root["Policy"] = base64_encode((const unsigned char*)bucket_policy.c_str(),
                                 bucket_policy.size());

  for (const auto& tag : bucket_tags) {
    root["User-Defined-Tags"][tag.first] = tag.second;
  }

  root["mero_object_list_index_oid"] =
      S3M0Uint128Helper::to_string(object_list_index_oid);

  root["mero_multipart_index_oid"] =
      S3M0Uint128Helper::to_string(multipart_index_oid);

  root["mero_objects_version_list_index_oid"] =
      S3M0Uint128Helper::to_string(objects_version_list_index_oid);

  S3DateTime current_time;
  current_time.init_current_time();
  root["create_timestamp"] = current_time.get_isoformat_string();

  Json::FastWriter fastWriter;
  return fastWriter.write(root);
}

int S3BucketMetadata::from_json(std::string content) {
  s3_log(S3_LOG_DEBUG, request_id, "Called\n");
  Json::Value newroot;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(content.c_str(), newroot);
  if (!parsingSuccessful || s3_fi_is_enabled("bucket_metadata_corrupted")) {
    s3_log(S3_LOG_ERROR, request_id, "Json Parsing failed.\n");
    return -1;
  }

  bucket_name = newroot["Bucket-Name"].asString();

  Json::Value::Members members = newroot["System-Defined"].getMemberNames();
  for (auto it : members) {
    system_defined_attribute[it.c_str()] =
        newroot["System-Defined"][it].asString();
  }
  members = newroot["User-Defined"].getMemberNames();
  for (auto it : members) {
    user_defined_attribute[it.c_str()] = newroot["User-Defined"][it].asString();
  }
  user_name = system_defined_attribute["Owner-User"];
  user_id = system_defined_attribute["Owner-User-id"];
  account_name = system_defined_attribute["Owner-Account"];
  account_id = system_defined_attribute["Owner-Account-id"];

  object_list_index_oid = S3M0Uint128Helper::to_m0_uint128(
      newroot["mero_object_list_index_oid"].asString());

  multipart_index_oid = S3M0Uint128Helper::to_m0_uint128(
      newroot["mero_multipart_index_oid"].asString());

  objects_version_list_index_oid = S3M0Uint128Helper::to_m0_uint128(
      newroot["mero_objects_version_list_index_oid"].asString());

  acl_from_json((newroot["ACL"]).asString());
  bucket_policy = base64_decode(newroot["Policy"].asString());

  members = newroot["User-Defined-Tags"].getMemberNames();
  for (const auto& tag : members) {
    bucket_tags[tag] = newroot["User-Defined-Tags"][tag].asString();
  }

  return 0;
}

void S3BucketMetadata::acl_from_json(std::string acl_json_str) {
  s3_log(S3_LOG_DEBUG, "", "Called\n");
  encoded_acl = acl_json_str;
}

std::string& S3BucketMetadata::get_encoded_bucket_acl() {
  // base64 acl encoded
  return encoded_acl;
}

std::string S3BucketMetadata::get_acl_as_xml() {
  return base64_decode(encoded_acl);
}

std::string& S3BucketMetadata::get_policy_as_json() { return bucket_policy; }

std::string S3BucketMetadata::get_tags_as_xml() {

  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  std::string user_defined_tags;
  std::string tags_as_xml_str;

  if (bucket_tags.empty()) {
    return tags_as_xml_str;
  } else {
    for (const auto& tag : bucket_tags) {
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
  }
  s3_log(S3_LOG_DEBUG, request_id, "Tags xml: %s\n", tags_as_xml_str.c_str());
  s3_log(S3_LOG_INFO, request_id, "Exiting\n");
  return tags_as_xml_str;
}

bool S3BucketMetadata::check_bucket_tags_exists() {
  return !bucket_tags.empty();
}
