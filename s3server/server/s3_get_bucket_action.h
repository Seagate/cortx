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

#ifndef __S3_SERVER_S3_GET_BUCKET_ACTION_H__
#define __S3_SERVER_S3_GET_BUCKET_ACTION_H__

#include <memory>

#include "s3_bucket_action_base.h"
#include "s3_bucket_metadata.h"
#include "s3_clovis_kvs_reader.h"
#include "s3_factory.h"
#include "s3_object_list_response.h"

class S3GetBucketAction : public S3BucketAction {
  std::shared_ptr<S3ClovisKVSReaderFactory> s3_clovis_kvs_reader_factory;
  std::shared_ptr<S3ObjectMetadataFactory> object_metadata_factory;
  std::shared_ptr<S3ClovisKVSReader> clovis_kv_reader;
  std::shared_ptr<ClovisAPI> s3_clovis_api;
  S3ObjectListResponse object_list;
  std::string last_key;  // last key during each iteration

  bool fetch_successful;

  // Helpers
  std::string get_bucket_index_name() {
    return "BUCKET/" + request->get_bucket_name();
  }

  std::string get_multipart_bucket_index_name() {
    return "BUCKET/" + request->get_bucket_name() + "/Multipart";
  }

  // Request Input params
  std::string request_prefix;
  std::string request_delimiter;
  std::string request_marker_key;
  size_t max_keys;

 public:
  S3GetBucketAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<ClovisAPI> clovis_api = nullptr,
      std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kvs_reader_factory =
          nullptr,
      std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory = nullptr,
      std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory = nullptr);

  void setup_steps();
  void validate_request();
  void fetch_bucket_info_failed();
  void get_next_objects();
  void get_next_objects_successful();
  void get_next_objects_failed();
  void send_response_to_s3_client();

  // For Testing purpose
  FRIEND_TEST(S3GetBucketActionTest, Constructor);
  FRIEND_TEST(S3GetBucketActionTest, ObjectListSetup);
  FRIEND_TEST(S3GetBucketActionTest, FetchBucketInfo);
  FRIEND_TEST(S3GetBucketActionTest, FetchBucketInfoFailedMissing);
  FRIEND_TEST(S3GetBucketActionTest, FetchBucketInfoFailedInternalError);
  // FRIEND_TEST(S3GetBucketActionTest, GetNextObjects);
  FRIEND_TEST(S3GetBucketActionTest, GetNextObjectsWithZeroObjects);
  FRIEND_TEST(S3GetBucketActionTest, GetNextObjectsSuccessful);
  FRIEND_TEST(S3GetBucketActionTest, GetNextObjectsSuccessfulJsonError);
  FRIEND_TEST(S3GetBucketActionTest, GetNextObjectsSuccessfulPrefix);
  FRIEND_TEST(S3GetBucketActionTest, GetNextObjectsSuccessfulDelimiter);
  FRIEND_TEST(S3GetBucketActionTest, GetNextObjectsSuccessfulPrefixDelimiter);
  FRIEND_TEST(S3GetBucketActionTest, GetNextObjectsFailed);
  FRIEND_TEST(S3GetBucketActionTest, GetNextObjectsFailedNoEntries);
  FRIEND_TEST(S3GetBucketActionTest, SendResponseToClientServiceUnavailable);
  FRIEND_TEST(S3GetBucketActionTest, SendResponseToClientNoSuchBucket);
  FRIEND_TEST(S3GetBucketActionTest, SendResponseToClientSuccess);
  FRIEND_TEST(S3GetBucketActionTest, SendResponseToClientInternalError);
};

#endif
