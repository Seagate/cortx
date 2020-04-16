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
 * Original creation date: 17-Mar-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_PUT_CHUNK_UPLOAD_OBJECT_ACTION_H__
#define __S3_SERVER_S3_PUT_CHUNK_UPLOAD_OBJECT_ACTION_H__

#include <gtest/gtest_prod.h>
#include <memory>

#include "s3_object_action_base.h"
#include "s3_async_buffer.h"
#include "s3_bucket_metadata.h"
#include "s3_clovis_writer.h"
#include "s3_factory.h"
#include "s3_object_metadata.h"
#include "s3_probable_delete_record.h"
#include "s3_timer.h"

enum class S3PutChunkUploadObjectActionState {
  empty,             // Initial state
  validationFailed,  // Any validations failed for request, including metadata
  newObjOidCreated,  // New object created
  newObjOidCreationFailed,  // New object create failed
  probableEntryRecordFailed,
  dataSignatureCheckFailed,
  writeComplete,       // data write to object completed successfully
  writeFailed,         // data write to object failed
  metadataSaved,       // metadata saved for new object
  metadataSaveFailed,  // metadata saved for new object
  completed,           // All stages done completely
};

class S3PutChunkUploadObjectAction : public S3ObjectAction {
  S3PutChunkUploadObjectActionState s3_put_chunk_action_state;
  std::shared_ptr<S3ClovisWriter> clovis_writer;
  std::shared_ptr<S3ClovisKVSWriter> clovis_kv_writer;
  int layout_id;
  struct m0_uint128 old_object_oid;
  int old_layout_id;
  struct m0_uint128 new_object_oid;
  // Maximum retry count for collision resolution
  unsigned short tried_count;
  // string used for salting the uri
  std::string salt;
  S3Timer create_object_timer;
  S3Timer write_content_timer;

  bool auth_failed;
  bool write_failed;
  // These 2 flags help respond to client gracefully when either auth or write
  // fails.
  // Both write and chunk auth happen in parallel.
  bool clovis_write_in_progress;
  bool clovis_write_completed;  // full object write
  bool auth_in_progress;
  bool auth_completed;  // all chunk auth

  // Probable delete record for old object OID in case of overwrite
  std::string old_oid_str;  // Key for old probable delete rec
  std::unique_ptr<S3ProbableDeleteRecord> old_probable_del_rec;
  // Probable delete record for new object OID in case of current req failure
  std::string new_oid_str;  // Key for new probable delete rec
  std::unique_ptr<S3ProbableDeleteRecord> new_probable_del_rec;

  std::shared_ptr<S3ClovisWriterFactory> clovis_writer_factory;
  std::shared_ptr<S3ClovisKVSWriterFactory> clovis_kv_writer_factory;
  std::shared_ptr<ClovisAPI> s3_clovis_api;
  std::shared_ptr<S3ObjectMetadata> new_object_metadata;
  std::shared_ptr<S3PutTagsBodyFactory> put_object_tag_body_factory;
  std::map<std::string, std::string> new_object_tags_map;

  void create_new_oid(struct m0_uint128 current_oid);
  void collision_detected();
  void send_chunk_details_if_any();

  // Used in UT Only
  void _set_layout_id(int layoutid) { layout_id = layoutid; }

 public:
  S3PutChunkUploadObjectAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory = nullptr,
      std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory = nullptr,
      std::shared_ptr<S3ClovisWriterFactory> clovis_s3_factory = nullptr,
      std::shared_ptr<S3AuthClientFactory> auth_factory = nullptr,
      std::shared_ptr<ClovisAPI> clovis_api = nullptr,
      std::shared_ptr<S3PutTagsBodyFactory> put_tags_body_factory = nullptr,
      std::shared_ptr<S3ClovisKVSWriterFactory> kv_writer_factory = nullptr);

  void setup_steps();

  void chunk_auth_successful();
  void chunk_auth_failed();
  void validate_tags();

  void fetch_bucket_info_failed();
  void fetch_object_info_success();
  void fetch_object_info_failed();
  void create_object();
  void create_object_failed();
  void create_object_successful();
  void parse_x_amz_tagging_header(std::string content);

  void initiate_data_streaming();
  void consume_incoming_content();
  void validate_x_amz_tagging_if_present();
  void write_object(std::shared_ptr<S3AsyncBufferOptContainer> buffer);

  void write_object_successful();
  void write_object_failed();
  void save_metadata();
  void save_object_metadata_success();
  void save_object_metadata_failed();
  void send_response_to_s3_client();

  void add_object_oid_to_probable_dead_oid_list();
  void add_object_oid_to_probable_dead_oid_list_failed();

  // Rollback tasks
  void startcleanup();
  void mark_new_oid_for_deletion();
  void mark_old_oid_for_deletion();
  void remove_old_oid_probable_record();
  void remove_new_oid_probable_record();
  void delete_old_object();
  void remove_old_object_version_metadata();
  void delete_new_object();

  void set_authorization_meta();

  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, ConstructorTest);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, FetchBucketInfo);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              FetchObjectInfoWhenBucketNotPresent);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              FetchObjectInfoWhenBucketFailed);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              FetchObjectInfoWhenBucketFailedToLaunch);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              FetchObjectInfoWhenBucketAndObjIndexPresent);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              FetchObjectInfoWhenBucketPresentAndObjIndexAbsent);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              FetchObjectInfoReturnedFoundShouldHaveNewOID);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              FetchObjectInfoReturnedNotFoundShouldUseURL2OID);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              FetchObjectInfoReturnedInvalidStateReportsError);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, CreateObjectFirstAttempt);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              CreateObjectSecondAttempt);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              CreateObjectFailedTestWhileShutdown);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              CreateObjectFailedWithCollisionExceededRetry);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              CreateObjectFailedWithCollisionRetry);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, CreateObjectFailedTest);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              CreateObjectFailedToLaunchTest);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, CreateNewOidTest);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              InitiateDataStreamingForZeroSizeObject);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              InitiateDataStreamingExpectingMoreData);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              InitiateDataStreamingWeHaveAllData);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              ConsumeIncomingShouldWriteIfWeAllData);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              ConsumeIncomingShouldWriteIfWeExactData);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              ConsumeIncomingShouldWriteIfWeHaveMoreData);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              ConsumeIncomingShouldPauseWhenWeHaveTooMuch);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              ConsumeIncomingShouldNotWriteWhenWriteInprogress);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              WriteObjectShouldWriteContentAndMarkProgress);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              WriteObjectFailedShouldUndoMarkProgress);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              WriteObjectEntityFailedShouldUndoMarkProgress);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              WriteObjectEntityOpenFailedShouldUndoMarkProgress);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              WriteObjectSuccessfulWhileShuttingDown);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              WriteObjectSuccessfulWhileShuttingDownAndRollback);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              WriteObjectSuccessfulShouldWriteStateAllData);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              WriteObjectSuccessfulShouldWriteWhenExactWritableSize);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              WriteObjectSuccessfulShouldWriteSomeDataWhenMoreData);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              WriteObjectSuccessfulDoNextStepWhenAllIsWritten);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              WriteObjectSuccessfulShouldRestartReadingData);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, SaveMetadata);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth,
              SendResponseWhenShuttingDown);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, SendErrorResponse);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, SendSuccessResponse);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, SendFailedResponse);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth, ConstructorTest);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth,
              InitiateDataStreamingShouldInitChunkAuth);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth,
              SendChunkDetailsToAuthClientWithSingleChunk);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth,
              SendChunkDetailsToAuthClientWithTwoChunks);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth,
              SendChunkDetailsToAuthClientWithTwoChunksAndOneZeroChunk);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth,
              WriteObjectShouldSendChunkDetailsForAuth);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth,
              WriteObjectSuccessfulShouldSendChunkDetailsForAuth);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth,
              ChunkAuthSuccessfulWhileShuttingDown);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth,
              ChunkAuthFailedWhileShuttingDown);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth,
              ChunkAuthSuccessfulWriteInProgress);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth,
              ChunkAuthSuccessfulWriteFailed);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth,
              ChunkAuthSuccessfulWriteSuccessful);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth,
              ChunkAuthFailedWriteFailed);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestWithAuth,
              ChunkAuthFailedWriteSuccessful);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, ValidateRequestTags);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, VaidateEmptyTags);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, VaidateInvalidTagsCase1);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, VaidateInvalidTagsCase2);
  FRIEND_TEST(S3PutChunkUploadObjectActionTestNoAuth, VaidateInvalidTagsCase3);
};

#endif

