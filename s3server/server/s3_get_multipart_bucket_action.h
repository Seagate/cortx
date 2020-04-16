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
 * Author         :  Rajesh Nambiar        <rajesh.nambiar@seagate.com>
 * Author         :  Abrarahmed Momin   <abrar.habib@seagate.com>
 * Original creation date: 13-Jan-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_GET_MULTIPART_BUCKET_ACTION_H__
#define __S3_SERVER_S3_GET_MULTIPART_BUCKET_ACTION_H__

#include <memory>

#include "s3_bucket_action_base.h"
#include "s3_clovis_kvs_reader.h"
#include "s3_factory.h"
#include "s3_object_list_response.h"

class S3GetMultipartBucketAction : public S3BucketAction {
  std::shared_ptr<S3ClovisKVSReader> clovis_kv_reader;
  std::shared_ptr<ClovisAPI> s3_clovis_api;
  std::shared_ptr<S3ClovisKVSReaderFactory> s3_clovis_kvs_reader_factory;
  std::shared_ptr<S3ObjectMetadataFactory> object_metadata_factory;
  S3ObjectListResponse multipart_object_list;
  std::string last_key;  // last key during each iteration
  size_t return_list_size;

  bool fetch_successful;

  std::string get_multipart_bucket_index_name() {
    return "BUCKET/" + request->get_bucket_name() + "/Multipart";
  }

  // Request Input params
  std::string request_prefix;
  std::string request_delimiter;
  std::string request_marker_key;
  std::string last_uploadid;
  std::string request_marker_uploadid;
  size_t max_uploads;

 public:
  S3GetMultipartBucketAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<ClovisAPI> clovis_api = nullptr,
      std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kvs_reader_factory =
          nullptr,
      std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory = nullptr,
      std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory = nullptr);

  void object_list_setup();
  void setup_steps();
  void fetch_bucket_info_failed();
  void get_next_objects();
  void get_next_objects_successful();
  void get_next_objects_failed();
  void get_key_object();
  void get_key_object_successful();
  void get_key_object_failed();
  void send_response_to_s3_client();

  // For testing purpose
  FRIEND_TEST(S3GetMultipartBucketActionTest, Constructor);
  FRIEND_TEST(S3GetMultipartBucketActionTest, ObjectListSetup);
  FRIEND_TEST(S3GetMultipartBucketActionTest, FetchBucketInfoFailedMissing);
  FRIEND_TEST(S3GetMultipartBucketActionTest,
              FetchBucketInfoFailedInternalError);
  FRIEND_TEST(S3GetMultipartBucketActionTest, GetNextObjects);
  FRIEND_TEST(S3GetMultipartBucketActionTest, GetNextObjectsWithZeroObjects);
  FRIEND_TEST(S3GetMultipartBucketActionTest, GetNextObjectsSuccessful);
  FRIEND_TEST(S3GetMultipartBucketActionTest,
              GetNextObjectsSuccessfulJsonError);
  FRIEND_TEST(S3GetMultipartBucketActionTest, GetNextObjectsSuccessfulPrefix);
  FRIEND_TEST(S3GetMultipartBucketActionTest,
              GetNextObjectsSuccessfulDelimiter);
  FRIEND_TEST(S3GetMultipartBucketActionTest,
              GetNextObjectsSuccessfulPrefixDelimiter);
  FRIEND_TEST(S3GetMultipartBucketActionTest, GetNextObjectsFailed);
  FRIEND_TEST(S3GetMultipartBucketActionTest, GetNextObjectsFailedNoEntries);
  FRIEND_TEST(S3GetMultipartBucketActionTest,
              SendResponseToClientServiceUnavailable);
  FRIEND_TEST(S3GetMultipartBucketActionTest, SendResponseToClientNoSuchBucket);
  FRIEND_TEST(S3GetMultipartBucketActionTest, SendResponseToClientSuccess);
  FRIEND_TEST(S3GetMultipartBucketActionTest,
              SendResponseToClientInternalError);
};

#endif
