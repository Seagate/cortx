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

#pragma once

#ifndef __S3_SERVER_GLOBAL_BUCKET_INDEX_METADATA_H__
#define __S3_SERVER_GLOBAL_BUCKET_INDEX_METADATA_H__

#include <string>

#include "gtest/gtest_prod.h"
#include "s3_clovis_kvs_reader.h"
#include "s3_clovis_kvs_writer.h"
#include "s3_log.h"
#include "s3_request_object.h"

// Forward declarations
class S3ClovisKVSReaderFactory;
class S3ClovisKVSWriterFactory;

enum class S3GlobalBucketIndexMetadataState {
  empty,  // Initial state, no lookup done
  // Ops on root index
  created,  // create root success
  exists,   // create root exists

  // Following are ops on key-val
  present,  // Metadata exists and was read successfully
  missing,  // Metadata not present in store.
  saved,    // Metadata saved to store.
  deleted,  // Metadata deleted from store
  failed,
  failed_to_launch,  // pre launch operation failed
};

class S3GlobalBucketIndexMetadata {
  // Holds bucket, account and region information
  //    Key = "bucket name" | Value = "Account information, region"

 private:
  // below entries will holds bucket owner information
  std::string account_name;
  std::string account_id;
  std::string bucket_name;
  // region
  std::string location_constraint;
  std::string request_id;

  std::shared_ptr<S3RequestObject> request;
  std::shared_ptr<ClovisAPI> s3_clovis_api;
  std::shared_ptr<S3ClovisKVSReader> clovis_kv_reader;
  std::shared_ptr<S3ClovisKVSWriter> clovis_kv_writer;
  std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<S3ClovisKVSWriterFactory> clovis_kvs_writer_factory;

  // Used to report to caller
  std::function<void()> handler_on_success;
  std::function<void()> handler_on_failed;

  S3GlobalBucketIndexMetadataState state;

  // `true` in case of json parsing failure
  bool json_parsing_error;

 public:
  S3GlobalBucketIndexMetadata(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<ClovisAPI> s3_clovis_apii = nullptr,
      std::shared_ptr<S3ClovisKVSReaderFactory> clovis_s3_kvs_reader_factory =
          nullptr,
      std::shared_ptr<S3ClovisKVSWriterFactory> clovis_s3_kvs_writer_factory =
          nullptr);

  std::string get_account_name();
  std::string get_account_id();
  void set_location_constraint(const std::string &location) {
    location_constraint = location;
  }

  std::string get_location_constraint() { return location_constraint; }

  // Load Account user info(bucket list oid)
  virtual void load(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed);
  virtual void load_successful();
  virtual void load_failed();
  virtual ~S3GlobalBucketIndexMetadata() {}

  // Save Account user info(bucket list oid)
  virtual void save(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed);
  void save_successful();
  void save_failed();

  // Remove Account user info(bucket list oid)
  virtual void remove(std::function<void(void)> on_success,
                      std::function<void(void)> on_failed);
  void remove_successful();
  void remove_failed();

  virtual S3GlobalBucketIndexMetadataState get_state() { return state; }

  // Streaming to/from json
  std::string to_json();
  // returns 0 on success, -1 on parsing error
  int from_json(std::string content);

  // For Google mocks
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, Constructor);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, Load);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, LoadSuccessful);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, LoadSuccessfulJsonError);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, LoadFailed);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, LoadFailedMissing);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, Save);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, SaveSuccessful);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, SaveFailed);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, SaveFailedToLaunch);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, Remove);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, RemoveSuccessful);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, RemoveFailed);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, RemoveFailedToLaunch);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, ToJson);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, FromJson);
  FRIEND_TEST(S3GlobalBucketIndexMetadataTest, FromJsonError);
};

#endif
