/*
 * COPYRIGHT 2019 SEAGATE LLC
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
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 30-Jan-2019
 */

#include "s3_global_bucket_index_metadata.h"
#include <json/json.h>
#include <string>
#include "s3_factory.h"
#include "s3_iem.h"

extern struct m0_uint128 global_bucket_list_index_oid;

S3GlobalBucketIndexMetadata::S3GlobalBucketIndexMetadata(
    std::shared_ptr<S3RequestObject> req, std::shared_ptr<ClovisAPI> clovis_api,
    std::shared_ptr<S3ClovisKVSReaderFactory> clovis_s3_kvs_reader_factory,
    std::shared_ptr<S3ClovisKVSWriterFactory> clovis_s3_kvs_writer_factory)
    : request(req), json_parsing_error(false) {
  request_id = request->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Constructor");

  account_name = request->get_account_name();
  account_id = request->get_account_id();
  bucket_name = request->get_bucket_name();
  state = S3GlobalBucketIndexMetadataState::empty;
  location_constraint = "us-west-2";
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
}

std::string S3GlobalBucketIndexMetadata::get_account_name() {
  return account_name;
}

std::string S3GlobalBucketIndexMetadata::get_account_id() { return account_id; }

void S3GlobalBucketIndexMetadata::load(std::function<void(void)> on_success,
                                       std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  // Mark missing as we initiate fetch, in case it fails to load due to missing.
  state = S3GlobalBucketIndexMetadataState::missing;

  clovis_kv_reader = clovis_kvs_reader_factory->create_clovis_kvs_reader(
      request, s3_clovis_api);
  clovis_kv_reader->get_keyval(
      global_bucket_list_index_oid, bucket_name,
      std::bind(&S3GlobalBucketIndexMetadata::load_successful, this),
      std::bind(&S3GlobalBucketIndexMetadata::load_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GlobalBucketIndexMetadata::load_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  if (this->from_json(clovis_kv_reader->get_value()) != 0) {
    s3_log(S3_LOG_ERROR, request_id,
           "Json Parsing failed. Index oid = "
           "%" SCNx64 " : %" SCNx64 ", Key = %s, Value = %s\n",
           global_bucket_list_index_oid.u_hi, global_bucket_list_index_oid.u_lo,
           bucket_name.c_str(), clovis_kv_reader->get_value().c_str());
    s3_iem(LOG_ERR, S3_IEM_METADATA_CORRUPTED, S3_IEM_METADATA_CORRUPTED_STR,
           S3_IEM_METADATA_CORRUPTED_JSON);

    json_parsing_error = true;
    load_failed();
  } else {
    state = S3GlobalBucketIndexMetadataState::present;
    this->handler_on_success();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GlobalBucketIndexMetadata::load_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  if (json_parsing_error) {
    state = S3GlobalBucketIndexMetadataState::failed;
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::missing) {
    s3_log(S3_LOG_DEBUG, request_id, "bucket information is missing\n");
    state = S3GlobalBucketIndexMetadataState::missing;
  } else if (clovis_kv_reader->get_state() ==
             S3ClovisKVSReaderOpState::failed_to_launch) {
    state = S3GlobalBucketIndexMetadataState::failed_to_launch;
  } else {
    s3_log(S3_LOG_ERROR, request_id,
           "Loading of root bucket list index failed\n");
    state = S3GlobalBucketIndexMetadataState::failed;
  }

  this->handler_on_failed();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GlobalBucketIndexMetadata::save(std::function<void(void)> on_success,
                                       std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  // Mark missing as we initiate write, in case it fails to write.
  state = S3GlobalBucketIndexMetadataState::missing;
  clovis_kv_writer = clovis_kvs_writer_factory->create_clovis_kvs_writer(
      request, s3_clovis_api);
  clovis_kv_writer->put_keyval(
      global_bucket_list_index_oid, bucket_name, this->to_json(),
      std::bind(&S3GlobalBucketIndexMetadata::save_successful, this),
      std::bind(&S3GlobalBucketIndexMetadata::save_failed, this));

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GlobalBucketIndexMetadata::save_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  state = S3GlobalBucketIndexMetadataState::saved;
  this->handler_on_success();

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GlobalBucketIndexMetadata::save_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  s3_log(S3_LOG_ERROR, request_id, "Saving of root bucket list index failed\n");
  if (clovis_kv_writer->get_state() ==
      S3ClovisKVSWriterOpState::failed_to_launch) {
    state = S3GlobalBucketIndexMetadataState::failed_to_launch;
  } else {
    state = S3GlobalBucketIndexMetadataState::failed;
  }
  this->handler_on_failed();

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GlobalBucketIndexMetadata::remove(std::function<void(void)> on_success,
                                         std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  clovis_kv_writer = clovis_kvs_writer_factory->create_clovis_kvs_writer(
      request, s3_clovis_api);
  clovis_kv_writer->delete_keyval(
      global_bucket_list_index_oid, bucket_name,
      std::bind(&S3GlobalBucketIndexMetadata::remove_successful, this),
      std::bind(&S3GlobalBucketIndexMetadata::remove_failed, this));

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GlobalBucketIndexMetadata::remove_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  state = S3GlobalBucketIndexMetadataState::deleted;
  this->handler_on_success();

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GlobalBucketIndexMetadata::remove_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  s3_log(S3_LOG_ERROR, request_id, "Removal of bucket information failed\n");

  if (clovis_kv_writer->get_state() ==
      S3ClovisKVSWriterOpState::failed_to_launch) {
    state = S3GlobalBucketIndexMetadataState::failed_to_launch;
  } else {
    state = S3GlobalBucketIndexMetadataState::failed;
  }
  this->handler_on_failed();

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

// Streaming to json
std::string S3GlobalBucketIndexMetadata::to_json() {
  s3_log(S3_LOG_DEBUG, request_id, "Called\n");
  Json::Value root;

  root["account_name"] = account_name;
  root["account_id"] = account_id;
  root["location_constraint"] = location_constraint;

  Json::FastWriter fastWriter;
  return fastWriter.write(root);
}

int S3GlobalBucketIndexMetadata::from_json(std::string content) {
  s3_log(S3_LOG_DEBUG, request_id, "Called\n");
  Json::Value root;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(content.c_str(), root);

  if (!parsingSuccessful ||
      s3_fi_is_enabled("root_bucket_list_index_metadata_corrupted")) {
    s3_log(S3_LOG_ERROR, request_id, "Json Parsing failed.\n");
    return -1;
  }

  account_name = root["account_name"].asString();
  account_id = root["account_id"].asString();
  location_constraint = root["location_constraint"].asString();

  return 0;
}
