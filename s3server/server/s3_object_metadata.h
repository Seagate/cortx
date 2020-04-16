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

#pragma once

#ifndef __S3_SERVER_S3_OBJECT_METADATA_H__
#define __S3_SERVER_S3_OBJECT_METADATA_H__

#include <gtest/gtest_prod.h>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "s3_bucket_metadata.h"
#include "s3_clovis_kvs_reader.h"
#include "s3_clovis_kvs_writer.h"
#include "s3_request_object.h"
#include "s3_timer.h"

enum class S3ObjectMetadataState {
  empty,    // Initial state, no lookup done.
  present,  // Metadata exists and was read successfully.
  missing,  // Metadata not present in store.
  saved,    // Metadata saved to store.
  deleted,  // Metadata deleted from store.
  failed,
  failed_to_launch,  // pre launch operation failed.
  invalid
};

// Forward declarations.
class S3ClovisKVSReaderFactory;
class S3ClovisKVSWriterFactory;
class S3BucketMetadataFactory;

class S3ObjectMetadata {
  // Holds system-defined metadata (creation date etc).
  // Holds user-defined metadata (names must begin with "x-amz-meta-").
  // Partially supported on need bases, some of these are placeholders.
 private:
  std::string account_name;
  std::string account_id;
  std::string user_name;
  std::string canonical_id;
  std::string user_id;
  std::string bucket_name;
  std::string object_name;

  std::string request_id;

  // Reverse epoch time used as version id key in verion index
  std::string rev_epoch_version_id_key;
  // holds base64 encoding value of rev_epoch_version_id_key, this is used
  // in S3 REST APIs as http header x-amz-version-id or query param "VersionId"
  std::string object_version_id;

  std::string upload_id;
  // Maximum retry count for collision resolution.
  unsigned short tried_count;
  std::string salt;

  // The name for a key is a sequence of Unicode characters whose UTF-8 encoding
  // is at most 1024 bytes long.
  // http://docs.aws.amazon.com/AmazonS3/latest/dev/UsingMetadata.html#object-keys
  std::string object_key_uri;

  int layout_id;
  int old_layout_id;

  struct m0_uint128 oid;
  struct m0_uint128 old_oid;
  // Will be object list index oid when simple object upload.
  // Will be multipart object list index oid when multipart object upload.
  struct m0_uint128 object_list_index_oid;
  struct m0_uint128 objects_version_list_index_oid;
  struct m0_uint128 part_index_oid;

  std::string mero_oid_str;
  std::string mero_old_oid_str;
  std::string mero_old_object_version_id;

  std::string mero_part_oid_str;
  std::string encoded_acl;

  std::map<std::string, std::string> system_defined_attribute;
  std::map<std::string, std::string> user_defined_attribute;

  std::map<std::string, std::string> object_tags;

  bool is_multipart;

  std::shared_ptr<S3RequestObject> request;
  std::shared_ptr<ClovisAPI> s3_clovis_api;
  std::shared_ptr<S3ClovisKVSReader> clovis_kv_reader;
  std::shared_ptr<S3ClovisKVSWriter> clovis_kv_writer;
  std::shared_ptr<S3BucketMetadata> bucket_metadata;

  std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kv_reader_factory;
  std::shared_ptr<S3ClovisKVSWriterFactory> clovis_kv_writer_factory;
  std::shared_ptr<S3BucketMetadataFactory> bucket_metadata_factory;

  // Used to report to caller.
  std::function<void()> handler_on_success;
  std::function<void()> handler_on_failed;

  S3ObjectMetadataState state;
  S3Timer s3_timer;

  // `true` in case of json parsing failure.
  bool json_parsing_error;

  void initialize(bool is_multipart, std::string uploadid);

  // Any validations we want to do on metadata.
  void validate();
  std::string index_name;

  // TODO Eventually move these to s3_common as duplicated in s3_bucket_metadata
  // This index has keys as "object_name"
  std::string get_object_list_index_name() const {
    return "BUCKET/" + bucket_name;
  }
  // This index has keys as "object_name"
  std::string get_multipart_index_name() const {
    return "BUCKET/" + bucket_name + "/" + "Multipart";
  }
  // This index has keys as "object_name/version_id"
  std::string get_version_list_index_name() const {
    return "BUCKET/" + bucket_name + "/objects/versions";
  }

 public:
  S3ObjectMetadata(
      std::shared_ptr<S3RequestObject> req, bool ismultipart = false,
      std::string uploadid = "",
      std::shared_ptr<S3ClovisKVSReaderFactory> kv_reader_factory = nullptr,
      std::shared_ptr<S3ClovisKVSWriterFactory> kv_writer_factory = nullptr,
      std::shared_ptr<ClovisAPI> clovis_api = nullptr);

  // Call these when Object metadata save/remove needs to be called.
  // id can be object list index OID or
  // id can be multipart upload list index OID
  void set_object_list_index_oid(struct m0_uint128 id);
  void set_objects_version_list_index_oid(struct m0_uint128 id);

  virtual std::string get_version_key_in_index();
  virtual struct m0_uint128 get_object_list_index_oid() const;
  virtual struct m0_uint128 get_objects_version_list_index_oid() const;

  virtual void set_content_length(std::string length);
  virtual size_t get_content_length();
  virtual size_t get_part_one_size();
  virtual std::string get_content_length_str();

  virtual void set_md5(std::string md5);
  virtual void set_part_one_size(const size_t& part_size);
  virtual std::string get_md5();

  virtual void set_oid(struct m0_uint128 id);
  void set_old_oid(struct m0_uint128 id);
  void acl_from_json(std::string acl_json_str);
  void set_part_index_oid(struct m0_uint128 id);
  virtual struct m0_uint128 get_oid() { return oid; }
  virtual int get_layout_id() { return layout_id; }
  void set_layout_id(int id) { layout_id = id; }
  virtual int get_old_layout_id() { return old_layout_id; }
  void set_old_layout_id(int id) { old_layout_id = id; }

  virtual struct m0_uint128 get_old_oid() { return old_oid; }

  struct m0_uint128 get_part_index_oid() const { return part_index_oid; }

  void regenerate_version_id();

  std::string const get_obj_version_id() { return object_version_id; }
  std::string const get_obj_version_key() { return rev_epoch_version_id_key; }

  void set_version_id(std::string ver_id);

  std::string get_old_obj_version_id() { return mero_old_object_version_id; }
  void set_old_version_id(std::string old_obj_ver_id);

  std::string get_owner_name();
  std::string get_owner_id();
  virtual std::string get_object_name();
  virtual std::string get_user_id();
  virtual std::string get_user_name();
  virtual std::string get_canonical_id();
  virtual std::string get_account_name();
  std::string get_creation_date();
  std::string get_last_modified();
  virtual std::string get_last_modified_gmt();
  virtual std::string get_last_modified_iso();
  virtual void reset_date_time_to_current();
  virtual std::string get_storage_class();
  virtual std::string get_upload_id();
  std::string& get_encoded_object_acl();
  std::string get_acl_as_xml();

  // Load attributes.
  std::string get_system_attribute(std::string key);
  void add_system_attribute(std::string key, std::string val);
  std::string get_user_defined_attribute(std::string key);
  virtual void add_user_defined_attribute(std::string key, std::string val);
  virtual std::map<std::string, std::string>& get_user_attributes() {
    return user_defined_attribute;
  }

  // Load object metadata from object list index
  virtual void load(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed);

  // Save object metadata to versions list index & object list index
  virtual void save(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed);

  // Save object metadata ONLY object list index
  virtual void save_metadata(std::function<void(void)> on_success,
                             std::function<void(void)> on_failed);

  // Remove object metadata from object list index & versions list index
  virtual void remove(std::function<void(void)> on_success,
                      std::function<void(void)> on_failed);
  void remove_version_metadata(std::function<void(void)> on_success,
                               std::function<void(void)> on_failed);

  virtual S3ObjectMetadataState get_state() { return state; }

  // placeholder state, so as to not perform any operation on this.
  void mark_invalid() { state = S3ObjectMetadataState::invalid; }

  // For object metadata in object listing
  std::string to_json();
  // For storing minimal version entry in version listing
  std::string version_entry_to_json();

  // returns 0 on success, -1 on parsing error.
  virtual int from_json(std::string content);
  virtual void setacl(const std::string& input_acl);
  virtual void set_tags(const std::map<std::string, std::string>& tags_as_map);
  virtual const std::map<std::string, std::string>& get_tags();
  virtual void delete_object_tags();
  virtual std::string get_tags_as_xml();
  virtual bool check_object_tags_exists();
  virtual int object_tags_count();

  // Virtual Destructor
  virtual ~S3ObjectMetadata(){};

 private:
  // Methods used internally within

  // Load object metadata from object list index
  void load_successful();
  void load_failed();

  // save_metadata saves to object list index
  void save_metadata();
  void save_metadata_successful();
  void save_metadata_failed();

  // save_version_metadata saves to objects "version" list index
  void save_version_metadata();
  void save_version_metadata_successful();
  void save_version_metadata_failed();

  // Remove entry from object list index
  void remove_object_metadata();
  void remove_object_metadata_successful();
  void remove_object_metadata_failed();

  // Remove entry from objects version list index
  void remove_version_metadata();
  void remove_version_metadata_successful();
  void remove_version_metadata_failed();

 public:
  // Google tests.
  FRIEND_TEST(S3ObjectMetadataTest, ConstructorTest);
  FRIEND_TEST(S3MultipartObjectMetadataTest, ConstructorTest);
  FRIEND_TEST(S3ObjectMetadataTest, GetSet);
  FRIEND_TEST(S3MultipartObjectMetadataTest, GetUserIdUplodIdName);
  FRIEND_TEST(S3ObjectMetadataTest, GetSetOIDsPolicyAndLocation);
  FRIEND_TEST(S3ObjectMetadataTest, SetAcl);
  FRIEND_TEST(S3ObjectMetadataTest, AddSystemAttribute);
  FRIEND_TEST(S3ObjectMetadataTest, AddUserDefinedAttribute);
  FRIEND_TEST(S3ObjectMetadataTest, Load);
  FRIEND_TEST(S3ObjectMetadataTest, LoadSuccessful);
  FRIEND_TEST(S3ObjectMetadataTest, LoadSuccessInvalidJson);
  FRIEND_TEST(S3ObjectMetadataTest, LoadSuccessfulInvalidJson);
  FRIEND_TEST(S3ObjectMetadataTest, LoadObjectInfoFailedJsonParsingFailed);
  FRIEND_TEST(S3ObjectMetadataTest, LoadObjectInfoFailedMetadataMissing);
  FRIEND_TEST(S3ObjectMetadataTest, LoadObjectInfoFailedMetadataFailed);
  FRIEND_TEST(S3ObjectMetadataTest, LoadObjectInfoFailedMetadataFailedToLaunch);
  FRIEND_TEST(S3ObjectMetadataTest, SaveMeatdataMissingIndexOID);
  FRIEND_TEST(S3ObjectMetadataTest, SaveMeatdataIndexOIDPresent);
  FRIEND_TEST(S3ObjectMetadataTest, SaveMetadataWithoutParam);
  FRIEND_TEST(S3ObjectMetadataTest, SaveMetadataWithParam);
  FRIEND_TEST(S3ObjectMetadataTest, SaveMetadataSuccess);
  FRIEND_TEST(S3ObjectMetadataTest, SaveMetadataFailed);
  FRIEND_TEST(S3ObjectMetadataTest, SaveMetadataFailedToLaunch);
  FRIEND_TEST(S3ObjectMetadataTest, Remove);
  FRIEND_TEST(S3ObjectMetadataTest, RemoveObjectMetadataSuccessful);
  FRIEND_TEST(S3ObjectMetadataTest, RemoveVersionMetadataSuccessful);
  FRIEND_TEST(S3ObjectMetadataTest, RemoveObjectMetadataFailed);
  FRIEND_TEST(S3ObjectMetadataTest, RemoveObjectMetadataFailedToLaunch);
  FRIEND_TEST(S3ObjectMetadataTest, RemoveVersionMetadataFailed);
  FRIEND_TEST(S3ObjectMetadataTest, ToJson);
  FRIEND_TEST(S3ObjectMetadataTest, FromJson);
  FRIEND_TEST(S3MultipartObjectMetadataTest, FromJson);
  FRIEND_TEST(S3ObjectMetadataTest, GetEncodedBucketAcl);
};

#endif

