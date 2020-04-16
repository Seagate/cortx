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

#ifndef __S3_SERVER_S3_DELETE_OBJECT_ACTION_H__
#define __S3_SERVER_S3_DELETE_OBJECT_ACTION_H__

#include <gtest/gtest_prod.h>
#include <memory>

#include "s3_object_action_base.h"
#include "s3_clovis_writer.h"
#include "s3_factory.h"
#include "s3_log.h"
#include "s3_object_metadata.h"
#include "s3_probable_delete_record.h"

enum class S3DeleteObjectActionState {
  empty,             // Initial state
  validationFailed,  // Any validations failed for request, including missing
                     // metadata
  probableEntryRecordFailed,
  metadataDeleted,
  metadataDeleteFailed,
};

class S3DeleteObjectAction : public S3ObjectAction {
  std::shared_ptr<S3ClovisWriter> clovis_writer;
  std::shared_ptr<ClovisAPI> s3_clovis_api;
  std::shared_ptr<S3ClovisKVSWriter> clovis_kv_writer;

  std::shared_ptr<S3ClovisWriterFactory> clovis_writer_factory;
  std::shared_ptr<S3ClovisKVSWriterFactory> clovis_kv_writer_factory;

  // Probable delete record for object OID to be deleted
  std::string oid_str;  // Key for probable delete rec
  std::unique_ptr<S3ProbableDeleteRecord> probable_delete_rec;
  S3DeleteObjectActionState s3_del_obj_action_state;

 public:
  S3DeleteObjectAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory = nullptr,
      std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory = nullptr,
      std::shared_ptr<S3ClovisWriterFactory> writer_factory = nullptr,
      std::shared_ptr<S3ClovisKVSWriterFactory> kv_writer_factory = nullptr,
      std::shared_ptr<ClovisAPI> clovis_api = nullptr);

  void setup_steps();

  void fetch_bucket_info_failed();
  void fetch_object_info_failed();
  void delete_metadata();
  void delete_metadata_failed();
  void delete_metadata_successful();
  void set_authorization_meta();

  void add_object_oid_to_probable_dead_oid_list();
  void add_object_oid_to_probable_dead_oid_list_failed();

  void startcleanup();
  void mark_oid_for_deletion();
  void delete_object();
  void remove_probable_record();

  void send_response_to_s3_client();

  FRIEND_TEST(S3DeleteObjectActionTest, ConstructorTest);
  FRIEND_TEST(S3DeleteObjectActionTest, FetchBucketInfo);
  FRIEND_TEST(S3DeleteObjectActionTest, FetchObjectInfo);
  FRIEND_TEST(S3DeleteObjectActionTest, FetchObjectInfoWhenBucketNotPresent);
  FRIEND_TEST(S3DeleteObjectActionTest, FetchObjectInfoWhenBucketFetchFailed);
  FRIEND_TEST(S3DeleteObjectActionTest,
              DeleteObjectWhenMetadataDeleteFailedToLaunch);
  FRIEND_TEST(S3DeleteObjectActionTest,
              FetchObjectInfoWhenBucketFetchFailedToLaunch);
  FRIEND_TEST(S3DeleteObjectActionTest,
              FetchObjectInfoWhenBucketAndObjIndexPresent);
  FRIEND_TEST(S3DeleteObjectActionTest,
              FetchObjectInfoWhenBucketPresentAndObjIndexAbsent);
  FRIEND_TEST(S3DeleteObjectActionTest, DeleteMetadataWhenObjectPresent);
  FRIEND_TEST(S3DeleteObjectActionTest, DeleteMetadataWhenObjectAbsent);
  FRIEND_TEST(S3DeleteObjectActionTest,
              DeleteMetadataWhenObjectMetadataFetchFailed);
  FRIEND_TEST(S3DeleteObjectActionTest, DeleteObjectWhenMetadataDeleteFailed);
  FRIEND_TEST(S3DeleteObjectActionTest, DeleteObjectMissingShouldReportDeleted);
  FRIEND_TEST(S3DeleteObjectActionTest, DeleteObjectFailedShouldReportDeleted);
  FRIEND_TEST(S3DeleteObjectActionTest, SendErrorResponse);
  FRIEND_TEST(S3DeleteObjectActionTest, SendAnyFailedResponse);
  FRIEND_TEST(S3DeleteObjectActionTest, SendSuccessResponse);
};

#endif

