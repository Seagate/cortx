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

#ifndef __S3_SERVER_S3_BUCKET_METADATA_V1_H__
#define __S3_SERVER_S3_BUCKET_METADATA_V1_H__

#include <gtest/gtest_prod.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include "s3_global_bucket_index_metadata.h"
#include "s3_bucket_metadata.h"
#include "s3_log.h"
#include "s3_timer.h"

// Forward declarations
class S3GlobalBucketIndexMetadataFactory;

class S3BucketMetadataV1 : public S3BucketMetadata {
  // Holds mainly system-defined metadata (creation date etc)
  // Partially supported on need bases, some of these are placeholders
 protected:
  std::string bucket_owner_account_id;

  std::shared_ptr<S3GlobalBucketIndexMetadataFactory>
      global_bucket_index_metadata_factory;

  std::shared_ptr<S3GlobalBucketIndexMetadata> global_bucket_index_metadata;

  S3Timer s3_timer;

 private:
  void fetch_global_bucket_account_id_info();
  void fetch_global_bucket_account_id_info_success();
  void fetch_global_bucket_account_id_info_failed();

  void load_bucket_info();
  void load_bucket_info_successful();
  void load_bucket_info_failed();

  void create_object_list_index();
  void create_object_list_index_successful();
  void create_object_list_index_failed();

  void create_multipart_list_index();
  void create_multipart_list_index_successful();
  void create_multipart_list_index_failed();

  void create_objects_version_list_index();
  void create_objects_version_list_index_successful();
  void create_objects_version_list_index_failed();

  void save_bucket_info();
  void save_bucket_info_successful();
  void save_bucket_info_failed();

  void remove_bucket_info();
  void remove_bucket_info_successful();
  void remove_bucket_info_failed();

  void remove_global_bucket_account_id_info();
  void remove_global_bucket_account_id_info_successful();
  void remove_global_bucket_account_id_info_failed();

  void save_global_bucket_account_id_info();
  void save_global_bucket_account_id_info_successful();
  void save_global_bucket_account_id_info_failed();

  std::string get_bucket_metadata_index_key_name() {
    return bucket_owner_account_id + "/" + bucket_name;
  }

  // for UTs only
  struct m0_uint128 get_bucket_metadata_list_index_oid();
  void set_bucket_metadata_list_index_oid(struct m0_uint128 id);

 public:
  S3BucketMetadataV1(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<ClovisAPI> clovis_api = nullptr,
      std::shared_ptr<S3ClovisKVSReaderFactory> clovis_s3_kvs_reader_factory =
          nullptr,
      std::shared_ptr<S3ClovisKVSWriterFactory> clovis_s3_kvs_writer_factory =
          nullptr,
      std::shared_ptr<S3GlobalBucketIndexMetadataFactory>
          s3_global_bucket_index_metadata_factory = nullptr);

  virtual void load(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed);

  virtual void save(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed);

  virtual void remove(std::function<void(void)> on_success,
                      std::function<void(void)> on_failed);

  virtual S3BucketMetadataState get_state() { return state; }

  // Google tests
  FRIEND_TEST(S3BucketMetadataV1Test, ConstructorTest);
  FRIEND_TEST(S3BucketMetadataV1Test, GetSystemAttributesTest);
  FRIEND_TEST(S3BucketMetadataV1Test, GetSetOIDsPolicyAndLocation);
  FRIEND_TEST(S3BucketMetadataV1Test, DeletePolicy);
  FRIEND_TEST(S3BucketMetadataV1Test, GetTagsAsXml);
  FRIEND_TEST(S3BucketMetadataV1Test, GetSpecialCharTagsAsXml);
  FRIEND_TEST(S3BucketMetadataV1Test, AddSystemAttribute);
  FRIEND_TEST(S3BucketMetadataV1Test, AddUserDefinedAttribute);
  FRIEND_TEST(S3BucketMetadataV1Test, Load);
  FRIEND_TEST(S3BucketMetadataV1Test, FetchBucketListIndexOid);
  FRIEND_TEST(S3BucketMetadataV1Test,
              FetchBucketListIndexOIDSuccessStateIsSaving);
  FRIEND_TEST(S3BucketMetadataV1Test,
              FetchBucketListIndexOIDSuccessStateIsFetching);
  FRIEND_TEST(S3BucketMetadataV1Test,
              FetchBucketListIndexOIDSuccessStateIsDeleting);
  FRIEND_TEST(S3BucketMetadataV1Test, FetchBucketListIndexOIDFailedState);
  FRIEND_TEST(S3BucketMetadataV1Test,
              FetchBucketListIndexOIDFailedStateIsSaving);
  FRIEND_TEST(S3BucketMetadataV1Test,
              FetchBucketListIndexOIDFailedStateIsFetching);
  FRIEND_TEST(S3BucketMetadataV1Test,
              FetchBucketListIndexOIDFailedIndexMissingStateSaved);
  FRIEND_TEST(S3BucketMetadataV1Test,
              FetchBucketListIndexOIDFailedIndexFailedStateIsFetching);
  FRIEND_TEST(S3BucketMetadataV1Test, FetchBucketListIndexOIDFailedIndexFailed);
  FRIEND_TEST(S3BucketMetadataV1Test, LoadBucketInfo);
  FRIEND_TEST(S3BucketMetadataV1Test, SetBucketPolicy);
  FRIEND_TEST(S3BucketMetadataV1Test, SetAcl);
  FRIEND_TEST(S3BucketMetadataV1Test, LoadBucketInfoFailedJsonParsingFailed);
  FRIEND_TEST(S3BucketMetadataV1Test, LoadBucketInfoFailedMetadataMissing);
  FRIEND_TEST(S3BucketMetadataV1Test, LoadBucketInfoFailedMetadataFailed);
  FRIEND_TEST(S3BucketMetadataV1Test,
              LoadBucketInfoFailedMetadataFailedToLaunch);
  FRIEND_TEST(S3BucketMetadataV1Test, SaveMeatdataMissingIndexOID);
  FRIEND_TEST(S3BucketMetadataV1Test, SaveMeatdataIndexOIDPresent);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateObjectIndexOIDNotPresent);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateBucketListIndexCollisionCount0);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateBucketListIndexCollisionCount1);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateBucketListIndexSuccessful);
  FRIEND_TEST(S3BucketMetadataV1Test,
              CreateBucketListIndexFailedCollisionHappened);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateBucketListIndexFailed);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateBucketListIndexFailedToLaunch);
  FRIEND_TEST(S3BucketMetadataV1Test, HandleCollision);
  FRIEND_TEST(S3BucketMetadataV1Test, HandleCollisionMaxAttemptExceeded);
  FRIEND_TEST(S3BucketMetadataV1Test, RegeneratedNewIndexName);
  FRIEND_TEST(S3BucketMetadataV1Test, SaveBucketListIndexOid);
  FRIEND_TEST(S3BucketMetadataV1Test,
              SaveBucketListIndexOidSucessfulStateSaving);
  FRIEND_TEST(S3BucketMetadataV1Test, SaveBucketListIndexOidSucessful);
  FRIEND_TEST(S3BucketMetadataV1Test,
              SaveBucketListIndexOidSucessfulBcktMetaMissing);
  FRIEND_TEST(S3BucketMetadataV1Test, SaveBucketListIndexOIDFailed);
  FRIEND_TEST(S3BucketMetadataV1Test, SaveBucketListIndexOIDFailedToLaunch);
  FRIEND_TEST(S3BucketMetadataV1Test, SaveBucketInfo);
  FRIEND_TEST(S3BucketMetadataV1Test, SaveBucketInfoSuccess);
  FRIEND_TEST(S3BucketMetadataV1Test, SaveBucketInfoFailed);
  FRIEND_TEST(S3BucketMetadataV1Test, SaveBucketInfoFailedToLaunch);
  FRIEND_TEST(S3BucketMetadataV1Test, RemovePresentMetadata);
  FRIEND_TEST(S3BucketMetadataV1Test, RemoveAfterFetchingBucketListIndexOID);
  FRIEND_TEST(S3BucketMetadataV1Test, RemoveBucketInfo);
  FRIEND_TEST(S3BucketMetadataV1Test, RemoveBucketInfoSuccessful);
  FRIEND_TEST(S3BucketMetadataV1Test, RemoveBucketAccountidInfoSuccessful);
  FRIEND_TEST(S3BucketMetadataV1Test, RemoveBucketAccountidInfoFailedToLaunch);
  FRIEND_TEST(S3BucketMetadataV1Test, RemoveBucketAccountidInfoFailed);
  FRIEND_TEST(S3BucketMetadataV1Test, RemoveBucketInfoFailed);
  FRIEND_TEST(S3BucketMetadataV1Test, RemoveBucketInfoFailedToLaunch);
  FRIEND_TEST(S3BucketMetadataV1Test, ToJson);
  FRIEND_TEST(S3BucketMetadataV1Test, FromJson);
  FRIEND_TEST(S3BucketMetadataV1Test, GetEncodedBucketAcl);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateMultipartListIndexCollisionCount0);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateObjectListIndexCollisionCount0);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateObjectListIndexSuccessful);
  FRIEND_TEST(S3BucketMetadataV1Test,
              CreateObjectListIndexFailedCollisionHappened);
  FRIEND_TEST(S3BucketMetadataV1Test,
              CreateMultipartListIndexFailedCollisionHappened);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateObjectListIndexFailed);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateObjectListIndexFailedToLaunch);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateMultipartListIndexFailed);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateMultipartListIndexFailedToLaunch);
};

#endif
