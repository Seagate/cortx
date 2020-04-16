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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 3-Feb-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_DELETE_MULTIPLE_OBJECTS_ACTION_H__
#define __S3_SERVER_S3_DELETE_MULTIPLE_OBJECTS_ACTION_H__

#include <gtest/gtest_prod.h>
#include <map>
#include <memory>

#include "s3_bucket_action_base.h"
#include "s3_clovis_kvs_reader.h"
#include "s3_clovis_kvs_writer.h"
#include "s3_clovis_writer.h"
#include "s3_delete_multiple_objects_body.h"
#include "s3_delete_multiple_objects_response_body.h"
#include "s3_factory.h"
#include "s3_log.h"
#include "s3_object_metadata.h"
#include "s3_probable_delete_record.h"

class S3DeleteMultipleObjectsAction : public S3BucketAction {
  std::vector<std::shared_ptr<S3ObjectMetadata>> objects_metadata;
  std::shared_ptr<S3ClovisWriter> clovis_writer;
  std::shared_ptr<S3ClovisKVSReader> clovis_kv_reader;
  std::shared_ptr<S3ClovisKVSWriter> clovis_kv_writer;

  std::shared_ptr<S3ObjectMetadataFactory> object_metadata_factory;
  std::shared_ptr<S3ClovisWriterFactory> clovis_writer_factory;
  std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kvs_reader_factory;
  std::shared_ptr<S3ClovisKVSWriterFactory> clovis_kvs_writer_factory;

  // index within delete object list
  struct m0_uint128 object_list_index_oid;
  S3DeleteMultipleObjectsBody delete_request;
  int delete_index_in_req;
  std::vector<struct m0_uint128> oids_to_delete;
  std::vector<int> layout_id_for_objs_to_delete;
  std::vector<std::string> keys_to_delete;
  bool at_least_one_delete_successful;

  S3DeleteMultipleObjectsResponseBody delete_objects_response;

  std::map<std::string, std::unique_ptr<S3ProbableDeleteRecord>>
      probable_oid_list;

  std::string get_bucket_index_name() {
    return "BUCKET/" + request->get_bucket_name();
  }

 public:
  S3DeleteMultipleObjectsAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<S3BucketMetadataFactory> bucket_md_factory = nullptr,
      std::shared_ptr<S3ObjectMetadataFactory> object_md_factory = nullptr,
      std::shared_ptr<S3ClovisWriterFactory> writer_factory = nullptr,
      std::shared_ptr<S3ClovisKVSReaderFactory> kvs_reader_factory = nullptr,
      std::shared_ptr<S3ClovisKVSWriterFactory> kvs_writer_factory = nullptr);

  // Helpers
  void setup_steps();

  void validate_request();
  void validate_request_body(std::string content);
  void consume_incoming_content();
  void validate_request_body();

  void fetch_bucket_info_failed();
  void fetch_objects_info();
  void fetch_objects_info_successful();
  void fetch_objects_info_failed();

  void delete_objects_metadata();
  void delete_objects_metadata_successful();
  void delete_objects_metadata_failed();

  void add_object_oid_to_probable_dead_oid_list();
  void add_object_oid_to_probable_dead_oid_list_failed();

  void cleanup();
  void cleanup_oid_from_probable_dead_oid_list();
  void cleanup_successful();
  void cleanup_failed();

  void send_response_to_s3_client();

  FRIEND_TEST(S3DeleteMultipleObjectsActionTest, ConstructorTest);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              ValidateOnAllDataShouldCallNext4ValidData);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              ValidateOnAllDataShouldError4InvalidData);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              ValidateOnPartialDataShouldWaitForMore);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              ConsumeOnAllDataShouldCallNext4ValidData);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              ConsumeOnPartialDataShouldDoNothing);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              FetchBucketInfoFailedBucketNotPresent);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              FetchBucketInfoFailedWithError);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              FetchObjectInfoWhenBucketPresentAndObjIndexAbsent);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              FetchObjectInfoWhenBucketAndObjIndexPresent);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest, FetchObjectInfoFailed);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              FetchObjectInfoFailedWithMissingAndMoreToProcess);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              FetchObjectInfoFailedWithMissingAllDone);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              FetchObjectsInfoSuccessful4FewMissingObjs);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest, FetchObjectsInfoSuccessful);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              FetchObjectsInfoSuccessfulJsonErrors);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest, DeleteObjectMetadata);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              DeleteObjectMetadataFailedToLaunch);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              DeleteObjectMetadataFailedWithMissing);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              DeleteObjectMetadataFailedWithErrors);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              DeleteObjectMetadataFailedMoreToProcess);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest, DeleteObjectMetadataSucceeded);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              DeleteObjectsWithNoObjsToDeleteAndMoreToProcess);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              DeleteObjectsWithNoObjsToDeleteAndNoMoreToProcess);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest, DeleteObjectsWithObjsToDelete);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              DeleteObjectsSuccessfulNoMoreToProcess);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              DeleteObjectsSuccessfulMoreToProcess);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              DeleteObjectsFailedNoMoreToProcess);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              DeleteObjectsInitFailedNoMoreToProcess);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              DeleteObjectsFailedMoreToProcess);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest, SendErrorResponse);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest, SendAnyFailedResponse);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest, SendSuccessResponse);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              CleanupOnMetadataFailedToSaveTest1);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest,
              CleanupOnMetadataFailedToSaveTest2);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest, CleanupOnMetadataSavedTest1);
  FRIEND_TEST(S3DeleteMultipleObjectsActionTest, CleanupOnMetadataSavedTest2);
};

#endif
