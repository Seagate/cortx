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

#ifndef __S3_SERVER_S3_PUT_OBJECT_ACTION_H__
#define __S3_SERVER_S3_PUT_OBJECT_ACTION_H__

#include <gtest/gtest_prod.h>
#include <memory>
#include <string>
#include "s3_object_action_base.h"
#include "s3_async_buffer.h"
#include "s3_bucket_metadata.h"
#include "s3_clovis_writer.h"
#include "s3_factory.h"
#include "s3_object_metadata.h"
#include "s3_probable_delete_record.h"
#include "s3_timer.h"
#include "evhtp_wrapper.h"

enum class S3PutObjectActionState {
  empty,             // Initial state
  validationFailed,  // Any validations failed for request, including metadata
  probableEntryRecordFailed,
  newObjOidCreated,         // New object created
  newObjOidCreationFailed,  // New object create failed
  writeComplete,            // data write to object completed successfully
  writeFailed,              // data write to object failed
  md5ValidationFailed,      // md5 check failed
  metadataSaved,            // metadata saved for new object
  metadataSaveFailed,       // metadata saved for new object
  completed,                // All stages done completely
};

class S3PutObjectAction : public S3ObjectAction {
  S3PutObjectActionState s3_put_action_state;
  struct m0_uint128 old_object_oid;
  int old_layout_id;
  struct m0_uint128 new_object_oid;
  // Maximum retry count for collision resolution
  unsigned short tried_count;
  int layout_id;
  // string used for salting the uri
  std::string salt;
  std::shared_ptr<S3ClovisWriter> clovis_writer;
  std::shared_ptr<S3ClovisKVSWriter> clovis_kv_writer;

  size_t total_data_to_stream;
  S3Timer s3_timer;
  bool write_in_progress;

  std::shared_ptr<S3ClovisWriterFactory> clovis_writer_factory;
  std::shared_ptr<S3PutTagsBodyFactory> put_object_tag_body_factory;
  std::shared_ptr<S3ClovisKVSWriterFactory> clovis_kv_writer_factory;
  std::shared_ptr<ClovisAPI> s3_clovis_api;
  std::shared_ptr<S3ObjectMetadata> new_object_metadata;
  std::map<std::string, std::string> new_object_tags_map;

  // Probable delete record for old object OID in case of overwrite
  std::string old_oid_str;  // Key for old probable delete rec
  std::unique_ptr<S3ProbableDeleteRecord> old_probable_del_rec;
  // Probable delete record for new object OID in case of current req failure
  std::string new_oid_str;  // Key for new probable delete rec
  std::unique_ptr<S3ProbableDeleteRecord> new_probable_del_rec;

  void create_new_oid(struct m0_uint128 current_oid);
  void collision_detected();

  // Only for use with UT
  void _set_layout_id(int layoutid) { layout_id = layoutid; }

 public:
  S3PutObjectAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<ClovisAPI> clovis_api = nullptr,
      std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory = nullptr,
      std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory = nullptr,
      std::shared_ptr<S3ClovisWriterFactory> clovis_s3_factory = nullptr,
      std::shared_ptr<S3PutTagsBodyFactory> put_tags_body_factory = nullptr,
      std::shared_ptr<S3ClovisKVSWriterFactory> kv_writer_factory = nullptr);

  void setup_steps();
  // void start();

  void validate_x_amz_tagging_if_present();
  void parse_x_amz_tagging_header(std::string content);
  void validate_tags();
  void validate_object_key_len();
  void set_authorization_meta();
  void fetch_bucket_info_failed();
  void fetch_object_info_success();
  void fetch_object_info_failed();
  void create_object();
  void create_object_successful();
  void create_object_failed();

  void add_object_oid_to_probable_dead_oid_list();
  void add_object_oid_to_probable_dead_oid_list_failed();

  void initiate_data_streaming();
  void consume_incoming_content();
  void write_object(std::shared_ptr<S3AsyncBufferOptContainer> buffer);

  void write_object_successful();
  void write_object_failed();
  void save_metadata();
  void save_object_metadata_success();
  void save_object_metadata_failed();
  void send_response_to_s3_client();

  // Rollback tasks
  void startcleanup();
  void mark_new_oid_for_deletion();
  void mark_old_oid_for_deletion();
  void remove_old_oid_probable_record();
  void remove_new_oid_probable_record();
  void delete_old_object();
  void remove_old_object_version_metadata();
  void delete_new_object();

  FRIEND_TEST(S3PutObjectActionTest, ConstructorTest);
  FRIEND_TEST(S3PutObjectActionTest, ValidateRequestTags);
  FRIEND_TEST(S3PutObjectActionTest, VaidateEmptyTags);
  FRIEND_TEST(S3PutObjectActionTest, VaidateInvalidTagsCase1);
  FRIEND_TEST(S3PutObjectActionTest, VaidateInvalidTagsCase2);
  FRIEND_TEST(S3PutObjectActionTest, VaidateInvalidTagsCase3);
  FRIEND_TEST(S3PutObjectActionTest, VaidateInvalidTagsCase4);
  FRIEND_TEST(S3PutObjectActionTest, VaidateSpecialCharTagsCase1);
  FRIEND_TEST(S3PutObjectActionTest, VaidateSpecialCharTagsCase2);
  FRIEND_TEST(S3PutObjectActionTest, FetchBucketInfo);
  FRIEND_TEST(S3PutObjectActionTest, FetchObjectInfoWhenBucketNotPresent);
  FRIEND_TEST(S3PutObjectActionTest, FetchObjectInfoWhenBucketFailedTolaunch);
  FRIEND_TEST(S3PutObjectActionTest, FetchObjectInfoWhenBucketFailed);
  FRIEND_TEST(S3PutObjectActionTest,
              FetchObjectInfoWhenBucketAndObjIndexPresent);
  FRIEND_TEST(S3PutObjectActionTest,
              FetchObjectInfoWhenBucketPresentAndObjIndexAbsent);
  FRIEND_TEST(S3PutObjectActionTest,
              FetchObjectInfoReturnedFoundShouldHaveNewOID);
  FRIEND_TEST(S3PutObjectActionTest,
              FetchObjectInfoReturnedNotFoundShouldUseURL2OID);
  FRIEND_TEST(S3PutObjectActionTest,
              FetchObjectInfoReturnedInvalidStateReportsError);
  FRIEND_TEST(S3PutObjectActionTest, CreateObjectFirstAttempt);
  FRIEND_TEST(S3PutObjectActionTest, CreateObjectSecondAttempt);
  FRIEND_TEST(S3PutObjectActionTest, CreateObjectFailedTestWhileShutdown);
  FRIEND_TEST(S3PutObjectActionTest,
              CreateObjectFailedWithCollisionExceededRetry);
  FRIEND_TEST(S3PutObjectActionTest, CreateObjectFailedWithCollisionRetry);
  FRIEND_TEST(S3PutObjectActionTest, CreateObjectFailedTest);
  FRIEND_TEST(S3PutObjectActionTest, CreateObjectFailedToLaunchTest);
  FRIEND_TEST(S3PutObjectActionTest, CreateNewOidTest);
  FRIEND_TEST(S3PutObjectActionTest, InitiateDataStreamingForZeroSizeObject);
  FRIEND_TEST(S3PutObjectActionTest, InitiateDataStreamingExpectingMoreData);
  FRIEND_TEST(S3PutObjectActionTest, InitiateDataStreamingWeHaveAllData);
  FRIEND_TEST(S3PutObjectActionTest, ConsumeIncomingShouldWriteIfWeAllData);
  FRIEND_TEST(S3PutObjectActionTest, ConsumeIncomingShouldWriteIfWeExactData);
  FRIEND_TEST(S3PutObjectActionTest,
              ConsumeIncomingShouldWriteIfWeHaveMoreData);
  FRIEND_TEST(S3PutObjectActionTest,
              ConsumeIncomingShouldPauseWhenWeHaveTooMuch);
  FRIEND_TEST(S3PutObjectActionTest,
              ConsumeIncomingShouldNotWriteWhenWriteInprogress);
  FRIEND_TEST(S3PutObjectActionTest,
              WriteObjectShouldWriteContentAndMarkProgress);
  FRIEND_TEST(S3PutObjectActionTest, WriteObjectFailedShouldUndoMarkProgress);
  FRIEND_TEST(S3PutObjectActionTest, WriteObjectFailedDuetoEntityOpenFailure);
  FRIEND_TEST(S3PutObjectActionTest, WriteObjectSuccessfulWhileShuttingDown);
  FRIEND_TEST(S3PutObjectActionTest,
              WriteObjectSuccessfulWhileShuttingDownAndRollback);
  FRIEND_TEST(S3PutObjectActionTest,
              WriteObjectSuccessfulShouldWriteStateAllData);
  FRIEND_TEST(S3PutObjectActionTest,
              WriteObjectSuccessfulShouldWriteWhenExactWritableSize);
  FRIEND_TEST(S3PutObjectActionTest,
              WriteObjectSuccessfulShouldWriteSomeDataWhenMoreData);
  FRIEND_TEST(S3PutObjectActionTest,
              WriteObjectSuccessfulDoNextStepWhenAllIsWritten);
  FRIEND_TEST(S3PutObjectActionTest,
              WriteObjectSuccessfulShouldRestartReadingData);
  FRIEND_TEST(S3PutObjectActionTest, SaveMetadata);
  FRIEND_TEST(S3PutObjectActionTest, SendResponseWhenShuttingDown);
  FRIEND_TEST(S3PutObjectActionTest, SendErrorResponse);
  FRIEND_TEST(S3PutObjectActionTest, SendSuccessResponse);
  FRIEND_TEST(S3PutObjectActionTest, SendFailedResponse);
};

#endif

