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

#ifndef __S3_SERVER_S3_DELETE_BUCKET_ACTION_H__
#define __S3_SERVER_S3_DELETE_BUCKET_ACTION_H__

#include <gtest/gtest_prod.h>
#include <memory>
#include "s3_bucket_action_base.h"
#include "s3_bucket_metadata.h"
#include "s3_clovis_kvs_reader.h"
#include "s3_clovis_writer.h"
#include "s3_factory.h"

class S3DeleteBucketAction : public S3BucketAction {
  std::shared_ptr<S3ClovisKVSReader> clovis_kv_reader;
  std::shared_ptr<S3ObjectMetadata> object_multipart_metadata;
  std::shared_ptr<S3ClovisKVSWriter> clovis_kv_writer;
  std::shared_ptr<S3ClovisWriter> clovis_writer;
  std::shared_ptr<ClovisAPI> s3_clovis_api;
  std::map<std::string, std::string>::iterator multipart_kv;
  std::map<std::string, std::string> multipart_objects;
  std::vector<struct m0_uint128> part_oids;
  std::vector<struct m0_uint128> multipart_object_oids;
  std::vector<int> multipart_object_layoutids;
  m0_uint128 object_list_index_oid;
  m0_uint128 objects_version_list_index_oid;
  std::string last_key;  // last key during each iteration

  bool is_bucket_empty;
  bool delete_successful;
  bool multipart_present;

  // Helpers
  std::string get_bucket_index_name() {
    return "BUCKET/" + request->get_bucket_name();
  }

  std::string get_multipart_bucket_index_name() {
    return "BUCKET/" + request->get_bucket_name() + "/Multipart";
  }

  std::shared_ptr<S3ObjectMetadataFactory> object_metadata_factory;
  std::shared_ptr<S3ObjectMultipartMetadataFactory> object_mp_metadata_factory;
  std::shared_ptr<S3ClovisWriterFactory> clovis_writer_factory;
  std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<S3ClovisKVSWriterFactory> clovis_kvs_writer_factory;

 public:
  S3DeleteBucketAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<ClovisAPI> s3_clovis_apis = nullptr,
      std::shared_ptr<S3BucketMetadataFactory> bucket_metadata_factory =
          nullptr,
      std::shared_ptr<S3ObjectMultipartMetadataFactory> object_mp_meta_factory =
          nullptr,
      std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory = nullptr,
      std::shared_ptr<S3ClovisWriterFactory> clovis_s3_writer_factory = nullptr,
      std::shared_ptr<S3ClovisKVSWriterFactory> clovis_s3_kvs_writer_factory =
          nullptr,
      std::shared_ptr<S3ClovisKVSReaderFactory> clovis_s3_kvs_reader_factory =
          nullptr);

  void setup_steps();

  void fetch_bucket_info_failed();
  void fetch_first_object_metadata();
  void fetch_first_object_metadata_successful();
  void fetch_first_object_metadata_failed();

  void delete_bucket();
  void delete_bucket_successful();
  void delete_bucket_failed();
  void fetch_multipart_objects();
  void fetch_multipart_objects_successful();
  void delete_multipart_objects();
  void delete_multipart_objects_successful();
  void delete_multipart_objects_failed();
  void remove_part_indexes();
  void remove_part_indexes_successful();
  void remove_part_indexes_failed();
  void remove_multipart_index();
  void remove_multipart_index_failed();
  void remove_object_list_index();
  void remove_object_list_index_failed();
  void remove_objects_version_list_index();
  void remove_objects_version_list_index_failed();
  void send_response_to_s3_client();

  // Google tests
  FRIEND_TEST(S3DeleteBucketActionTest, ConstructorTest);
  FRIEND_TEST(S3DeleteBucketActionTest, FetchBucketMetadata);
  FRIEND_TEST(S3DeleteBucketActionTest, FetchFirstObjectMetadataPresent);
  FRIEND_TEST(S3DeleteBucketActionTest, FetchFirstObjectMetadataEmptyBucket);
  FRIEND_TEST(S3DeleteBucketActionTest, FetchFirstObjectMetadataBucketMissing);
  FRIEND_TEST(S3DeleteBucketActionTest,
              FetchFirstObjectMetadataBucketFailedToLaunch);
  FRIEND_TEST(S3DeleteBucketActionTest, FetchFirstObjectMetadataBucketfailure);
  FRIEND_TEST(S3DeleteBucketActionTest, FetchFirstObjectMetadataSuccess);
  FRIEND_TEST(S3DeleteBucketActionTest,
              FetchFirstObjectMetadataFailedObjectMetaMissing);
  FRIEND_TEST(S3DeleteBucketActionTest,
              FetchFirstObjectMetadataFailedObjectRetrievalFailed);
  FRIEND_TEST(S3DeleteBucketActionTest, FetchMultipartObjectsMultipartPresent);
  FRIEND_TEST(S3DeleteBucketActionTest,
              FetchMultipartObjectsMultipartNotPresent);
  FRIEND_TEST(S3DeleteBucketActionTest, FetchMultipartObjectSuccess);
  FRIEND_TEST(S3DeleteBucketActionTest, FetchMultipartObjectSuccessIllegalJson);
  FRIEND_TEST(S3DeleteBucketActionTest,
              FetchMultipartObjectSuccessMaxFetchCountLessThanMapSize);
  FRIEND_TEST(S3DeleteBucketActionTest, FetchMultipartObjectSuccessNoMultipart);
  FRIEND_TEST(S3DeleteBucketActionTest,
              DeleteMultipartObjectsMultipartObjectsPresent);
  FRIEND_TEST(S3DeleteBucketActionTest,
              DeleteMultipartObjectsMultipartObjectsNotPresent);
  FRIEND_TEST(S3DeleteBucketActionTest, DeleteMultipartObjectsSuccess);
  FRIEND_TEST(S3DeleteBucketActionTest, DeleteMultipartObjectsFailed);
  FRIEND_TEST(S3DeleteBucketActionTest, RemovePartIndexes);
  FRIEND_TEST(S3DeleteBucketActionTest, RemovePartIndexesSuccess);
  FRIEND_TEST(S3DeleteBucketActionTest, RemovePartIndexesFailed);
  FRIEND_TEST(S3DeleteBucketActionTest, RemovePartIndexesFailedToLaunch);
  FRIEND_TEST(S3DeleteBucketActionTest, RemoveMultipartIndexMultipartPresent);
  FRIEND_TEST(S3DeleteBucketActionTest,
              RemoveMultipartIndexMultipartNotPresent);
  FRIEND_TEST(S3DeleteBucketActionTest, RemoveMultipartIndexFailed);
  FRIEND_TEST(S3DeleteBucketActionTest, RemoveMultipartIndexFailedToLaunch);
  FRIEND_TEST(S3DeleteBucketActionTest, RemoveObjectListIndex);
  FRIEND_TEST(S3DeleteBucketActionTest, RemoveObjectListIndexFailed);
  FRIEND_TEST(S3DeleteBucketActionTest, RemoveObjectListIndexFailedToLaunch);
  FRIEND_TEST(S3DeleteBucketActionTest, DeleteBucket);
  FRIEND_TEST(S3DeleteBucketActionTest, DeleteBucketSuccess);
  FRIEND_TEST(S3DeleteBucketActionTest, DeleteBucketFailedBucketMissing);
  FRIEND_TEST(S3DeleteBucketActionTest,
              DeleteBucketFailedBucketMetaRetrievalFailed);
  FRIEND_TEST(S3DeleteBucketActionTest, SendInternalErrorResponse);
  FRIEND_TEST(S3DeleteBucketActionTest, SendNoSuchBucketErrorResponse);
  FRIEND_TEST(S3DeleteBucketActionTest, SendBucketNotEmptyErrorResponse);
  FRIEND_TEST(S3DeleteBucketActionTest, SendSuccessResponse);
  FRIEND_TEST(S3DeleteBucketActionTest, SendInternalErrorRetry);
};

#endif
