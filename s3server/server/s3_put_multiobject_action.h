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
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 20-Jan-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_PUT_MULTIOBJECT_ACTION_H__
#define __S3_SERVER_S3_PUT_MULTIOBJECT_ACTION_H__

#include <memory>

#include "s3_object_action_base.h"
#include "s3_async_buffer.h"
#include "s3_bucket_metadata.h"
#include "s3_clovis_writer.h"
#include "s3_object_metadata.h"
#include "s3_part_metadata.h"
#include "s3_timer.h"

class S3PutMultiObjectAction : public S3ObjectAction {
  std::shared_ptr<S3PartMetadata> part_metadata = NULL;
  std::shared_ptr<S3ObjectMetadata> object_multipart_metadata = NULL;
  std::shared_ptr<S3ClovisWriter> clovis_writer = NULL;

  size_t total_data_to_stream;
  S3Timer create_object_timer;
  S3Timer write_content_timer;
  int part_number;
  std::string upload_id;
  int layout_id;

  int get_part_number() {
    return atoi((request->get_query_string_value("partNumber")).c_str());
  }

  bool auth_failed;
  bool write_failed;
  // These 2 flags help respond to client gracefully when either auth or write
  // fails.
  // Both write and chunk auth happen in parallel.
  bool clovis_write_in_progress;
  bool clovis_write_completed;  // full object write
  bool auth_in_progress;
  bool auth_completed;  // all chunk auth

  void chunk_auth_successful();
  void chunk_auth_failed();
  void send_chunk_details_if_any();
  void check_part_number();

  std::shared_ptr<S3ObjectMultipartMetadataFactory> object_mp_metadata_factory;
  std::shared_ptr<S3PartMetadataFactory> part_metadata_factory;
  std::shared_ptr<S3ClovisWriterFactory> clovis_writer_factory;
  std::shared_ptr<S3AuthClientFactory> auth_factory;

  // Used only for UT
  void _set_layout_id(int layoutid) { layout_id = layoutid; }

 public:
  S3PutMultiObjectAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<S3ObjectMultipartMetadataFactory> object_mp_meta_factory =
          nullptr,
      std::shared_ptr<S3PartMetadataFactory> part_meta_factory = nullptr,
      std::shared_ptr<S3ClovisWriterFactory> clovis_s3_factory = nullptr,
      std::shared_ptr<S3AuthClientFactory> auth_factory = nullptr);

  void setup_steps();
  // void start();
  void fetch_bucket_info_success();
  void fetch_bucket_info_failed();
  void fetch_object_info_failed();
  void fetch_multipart_metadata();
  void fetch_multipart_failed();
  void fetch_firstpart_info();
  void fetch_firstpart_info_failed();
  void save_multipart_metadata();
  void save_multipart_metadata_failed();
  void compute_part_offset();

  void initiate_data_streaming();
  void consume_incoming_content();
  void write_object(std::shared_ptr<S3AsyncBufferOptContainer> buffer);

  void write_object_successful();
  void write_object_failed();

  void save_metadata();
  void save_metadata_failed();
  void send_response_to_s3_client();
  void set_authorization_meta();

  // Google tests
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, ConstructorTest);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              ChunkAuthSucessfulShuttingDown);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, ChunkAuthSucessfulNext);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              ChunkAuthFailedWriteSuccessful);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              ChunkAuthSucessfulWriteFailed);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, ChunkAuthSucessful);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              ChunkAuthSuccessShuttingDown);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              ChunkAuthFailedShuttingDown);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, ChunkAuthFailedNext);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, FetchBucketInfoTest);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              FetchBucketInfoFailedMissingTest);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              CheckPartNumberFailedInvalidPartTest);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              FetchBucketInfoFailedInternalErrorTest);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, FetchMultipartMetadata);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              FetchMultiPartMetadataNoSuchUploadFailed);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              FetchMultiPartMetadataInternalErrorFailed);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, FetchFirstPartInfo);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, SaveMultipartMetadata);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              SaveMultipartMetadataError);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              SaveMultipartMetadataAssert);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              SaveMultipartMetadataFailedInternalError);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              SaveMultipartMetadataFailedServiceUnavailable);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              FetchFirstPartInfoServiceUnavailableFailed);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              FetchFirstPartInfoInternalErrorFailed);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, ComputePartOffsetPart1);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              ComputePartOffsetNoChunk);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, ComputePartOffset);
  FRIEND_TEST(S3PutMultipartObjectActionTestWithMockAuth,
              InitiateDataStreamingForZeroSizeObject);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              InitiateDataStreamingExpectingMoreData);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              InitiateDataStreamingWeHaveAllData);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              ConsumeIncomingShouldWriteIfWeAllData);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              ConsumeIncomingShouldWriteIfWeExactData);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              ConsumeIncomingShouldWriteIfWeHaveMoreData);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              ConsumeIncomingShouldPauseWhenWeHaveTooMuch);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              ConsumeIncomingShouldNotWriteWhenWriteInprogress);
  FRIEND_TEST(S3PutMultipartObjectActionTestWithMockAuth,
              SendChunkDetailsToAuthClientWithSingleChunk);
  FRIEND_TEST(S3PutMultipartObjectActionTestWithMockAuth,
              SendChunkDetailsToAuthClientWithTwoChunks);
  FRIEND_TEST(S3PutMultipartObjectActionTestWithMockAuth,
              SendChunkDetailsToAuthClientWithTwoChunksAndOneZeroChunk);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, WriteObject);
  FRIEND_TEST(S3PutMultipartObjectActionTestWithMockAuth,
              WriteObjectShouldSendChunkDetailsForAuth);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              WriteObjectSuccessfulWhileShuttingDown);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              WriteObjectSuccessfulWhileShuttingDownAndRollback);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              WriteObjectSuccessfulShouldWriteStateAllData);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              WriteObjectSuccessfulShouldWriteWhenExactWritableSize);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              WriteObjectSuccessfulDoNextStepWhenAllIsWritten);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth,
              WriteObjectSuccessfulShouldRestartReadingData);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, SaveMetadata);
  FRIEND_TEST(S3PutMultipartObjectActionTestWithMockAuth,
              WriteObjectSuccessfulShouldSendChunkDetailsForAuth);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, SendErrorResponse);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, SendSuccessResponse);
  FRIEND_TEST(S3PutMultipartObjectActionTestNoMockAuth, SendFailedResponse);
};

#endif
