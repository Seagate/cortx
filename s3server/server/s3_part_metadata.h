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
 * Original author:  Rajesh Nambiar <rajesh.nambiar@seagate.com>
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 *
 * Original creation date: 21-Jan-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_PART_METADATA_H__
#define __S3_SERVER_S3_PART_METADATA_H__

#include <gtest/gtest_prod.h>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "s3_clovis_kvs_reader.h"
#include "s3_clovis_kvs_writer.h"
#include "s3_request_object.h"

enum class S3PartMetadataState {
  empty = 1,          // Initial state, no lookup done.
  present,            // Part Metadata exists and was read successfully.
  missing,            // Part Metadata not present in store.
  missing_partially,  // Some of the Parts Metadata not present in store.
  store_created,      // Created store for Parts Metadata.
  saved,              // Parts Metadata saved to store.
  deleted,            // Metadata deleted from store.
  index_deleted,      // store deleted.
  failed,
  failed_to_launch  // Pre launch operation failed
};

// Forward declarations.
class S3ClovisKVSReaderFactory;
class S3ClovisKVSWriterFactory;

class S3PartMetadata {
  // Holds system-defined metadata (creation date etc).
  // Holds user-defined metadata (names must begin with "x-amz-meta-").
  // Partially supported on need bases, some of these are placeholders.
 private:
  std::string account_name;
  std::string account_id;
  std::string user_name;
  std::string user_id;
  std::string bucket_name;
  std::string object_name;
  std::string upload_id;
  std::string part_number;
  std::string index_name;
  std::string salt;
  std::string str_part_num;

  std::string request_id;

  std::map<std::string, std::string> system_defined_attribute;
  std::map<std::string, std::string> user_defined_attribute;

  std::shared_ptr<S3RequestObject> request;
  std::shared_ptr<ClovisAPI> s3_clovis_api;
  std::shared_ptr<S3ClovisKVSReader> clovis_kv_reader;
  std::shared_ptr<S3ClovisKVSWriter> clovis_kv_writer;
  bool put_metadata;
  struct m0_uint128 part_index_name_oid;

  // Used to report to caller.
  std::function<void()> handler_on_success;
  std::function<void()> handler_on_failed;

  S3PartMetadataState state;
  size_t collision_attempt_count;

  // `true` in case of json parsing failure.
  bool json_parsing_error;

  std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kv_reader_factory;
  std::shared_ptr<S3ClovisKVSWriterFactory> clovis_kv_writer_factory;

 private:
  // Any validations we want to do on metadata.
  void validate();
  void initialize(std::string uploadid, int part);
  void create_new_oid();
  void handle_collision();
  void regenerate_new_indexname();

 public:
  S3PartMetadata(
      std::shared_ptr<S3RequestObject> req, std::string uploadid, int part_num,
      std::shared_ptr<S3ClovisKVSReaderFactory> kv_reader_factory = nullptr,
      std::shared_ptr<S3ClovisKVSWriterFactory> kv_writer_factory = nullptr);

  S3PartMetadata(
      std::shared_ptr<S3RequestObject> req, struct m0_uint128 oid,
      std::string uploadid, int part_num,
      std::shared_ptr<S3ClovisKVSReaderFactory> kv_reader_factory = nullptr,
      std::shared_ptr<S3ClovisKVSWriterFactory> kv_writer_factory = nullptr);

  std::string get_part_index_name() {
    return "BUCKET/" + bucket_name + "/" + object_name + "/" + upload_id;
  }

  struct m0_uint128 get_part_index_oid() {
    return part_index_name_oid;
  }

  virtual void set_content_length(std::string length);
  virtual size_t get_content_length();
  virtual std::string get_content_length_str();

  virtual void set_md5(std::string md5);
  virtual std::string get_md5();
  virtual std::string get_object_name();
  std::string get_last_modified();
  std::string get_last_modified_gmt();
  virtual std::string get_last_modified_iso();
  virtual void reset_date_time_to_current();
  virtual std::string get_storage_class();
  virtual std::string get_upload_id();
  std::string get_part_number();

  // Load attributes.
  std::string get_system_attribute(std::string key);
  void add_system_attribute(std::string key, std::string val);
  std::string get_user_defined_attribute(std::string key);
  virtual void add_user_defined_attribute(std::string key, std::string val);

  virtual void load(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed, int part_number);
  void load_successful();
  void load_failed();

  virtual void save(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed);
  virtual void create_index(std::function<void(void)> on_success,
                            std::function<void(void)> on_failed);
  void save_part_index(std::function<void(void)> on_success,
                       std::function<void(void)> on_failed);
  void create_part_index();
  void create_part_index_successful();
  void create_part_index_failed();
  void save_metadata();
  void save_metadata_successful();
  void save_metadata_failed();

  void remove(std::function<void(void)> on_success,
              std::function<void(void)> on_failed, int remove_part);
  void remove_successful();
  void remove_failed();
  virtual void remove_index(std::function<void(void)> on_success,
                            std::function<void(void)> on_failed);
  void remove_index_successful();
  void remove_index_failed();

  virtual S3PartMetadataState get_state() { return state; }

  void set_state(S3PartMetadataState part_state) { state = part_state; }

  std::string to_json();

  // returns 0 on success, -1 on parsing error.
  virtual int from_json(std::string content);

  // virtual destructor.
  virtual ~S3PartMetadata(){};

  // Google tests.
  FRIEND_TEST(S3PartMetadataTest, ConstructorTest);
  FRIEND_TEST(S3PartMetadataTest, GetSet);
  FRIEND_TEST(S3PartMetadataTest, SetAcl);
  FRIEND_TEST(S3PartMetadataTest, AddSystemAttribute);
  FRIEND_TEST(S3PartMetadataTest, AddUserDefinedAttribute);
  FRIEND_TEST(S3PartMetadataTest, Load);
  FRIEND_TEST(S3PartMetadataTest, LoadSuccessful);
  FRIEND_TEST(S3PartMetadataTest, LoadSuccessInvalidJson);
  FRIEND_TEST(S3PartMetadataTest, LoadPartInfoFailedJsonParsingFailed);
  FRIEND_TEST(S3PartMetadataTest, LoadPartInfoFailedMetadataMissing);
  FRIEND_TEST(S3PartMetadataTest, LoadPartInfoFailedMetadataFailed);
  FRIEND_TEST(S3PartMetadataTest, SaveMetadata);
  FRIEND_TEST(S3PartMetadataTest, SaveMetadataSuccessful);
  FRIEND_TEST(S3PartMetadataTest, SaveMetadataFailed);
  FRIEND_TEST(S3PartMetadataTest, SaveMetadataFailedToLaunch);
  FRIEND_TEST(S3PartMetadataTest, CreateIndex);
  FRIEND_TEST(S3PartMetadataTest, CreatePartIndex);
  FRIEND_TEST(S3PartMetadataTest, CreatePartIndexSuccessful);
  FRIEND_TEST(S3PartMetadataTest, CreatePartIndexSuccessfulSaveMetadata);
  FRIEND_TEST(S3PartMetadataTest, CreatePartIndexSuccessfulOnlyCreateIndex);
  FRIEND_TEST(S3PartMetadataTest, CreatePartIndexFailedCollisionHappened);
  FRIEND_TEST(S3PartMetadataTest, CreateBucketListIndexFailed);
  FRIEND_TEST(S3PartMetadataTest, CreateBucketListIndexFailedToLaunch);
  FRIEND_TEST(S3PartMetadataTest, CollisionDetected);
  FRIEND_TEST(S3PartMetadataTest, CollisionDetectedMaxAttemptExceeded);
  FRIEND_TEST(S3PartMetadataTest, Remove);
  FRIEND_TEST(S3PartMetadataTest, RemoveSuccessful);
  FRIEND_TEST(S3PartMetadataTest, RemoveFailed);
  FRIEND_TEST(S3PartMetadataTest, RemoveFailedToLaunch);
  FRIEND_TEST(S3PartMetadataTest, RemoveIndex);
  FRIEND_TEST(S3PartMetadataTest, RemoveIndexSucessful);
  FRIEND_TEST(S3PartMetadataTest, RemoveIndexFailed);
  FRIEND_TEST(S3PartMetadataTest, ToJson);
  FRIEND_TEST(S3PartMetadataTest, FromJsonSuccess);
  FRIEND_TEST(S3PartMetadataTest, FromJsonFailure);
};

#endif
