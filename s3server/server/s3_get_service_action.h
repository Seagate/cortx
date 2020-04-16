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

#ifndef __S3_SERVER_S3_GET_SERVICE_ACTION_H__
#define __S3_SERVER_S3_GET_SERVICE_ACTION_H__

#include <memory>

#include "s3_action_base.h"
#include "s3_clovis_kvs_reader.h"
#include "s3_service_list_response.h"

class S3GetServiceAction : public S3Action {
  std::shared_ptr<S3ClovisKVSReader> clovis_kv_reader;
  std::shared_ptr<ClovisAPI> s3_clovis_api;
  m0_uint128 bucket_list_index_oid;
  std::string last_key;  // last key during each iteration
  std::string key_prefix;  // holds account id
  S3ServiceListResponse bucket_list;
  std::shared_ptr<S3BucketMetadataFactory> bucket_metadata_factory;
  bool fetch_successful;
  std::shared_ptr<S3ClovisKVSReaderFactory> s3_clovis_kvs_reader_factory;

  std::string get_search_bucket_prefix() {
    return request->get_account_id() + "/";
  }

 public:
  S3GetServiceAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kvs_reader_factory =
          nullptr,
      std::shared_ptr<S3BucketMetadataFactory> bucket_metadata_factory =
          nullptr);
  void setup_steps();
  void initialization();
  void get_next_buckets();
  void get_next_buckets_successful();
  void get_next_buckets_failed();

  void send_response_to_s3_client();
  FRIEND_TEST(S3GetServiceActionTest, ConstructorTest);
  FRIEND_TEST(S3GetServiceActionTest,
              GetNextBucketCallsGetBucketListIndexIfMetadataPresent);
  FRIEND_TEST(S3GetServiceActionTest,
              GetNextBucketDoesNotCallsGetBucketListIndexIfMetadataFailed);
  FRIEND_TEST(S3GetServiceActionTest, GetNextBucketSuccessful);
  FRIEND_TEST(S3GetServiceActionTest,
              GetNextBucketFailedClovisReaderStateMissing);
  FRIEND_TEST(S3GetServiceActionTest,
              GetNextBucketFailedClovisReaderStatePresent);
  FRIEND_TEST(S3GetServiceActionTest, SendResponseToClientInternalError);
  FRIEND_TEST(S3GetServiceActionTest, SendResponseToClientServiceUnavailable);
  FRIEND_TEST(S3GetServiceActionTest, SendResponseToClientSuccess);
};

#endif
