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
 * Original creation date: 16-May-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_PUT_BUCKET_POLICY_ACTION_H__
#define __S3_SERVER_S3_PUT_BUCKET_POLICY_ACTION_H__

#include <memory>
#include <string>

#include "s3_bucket_action_base.h"
#include "s3_bucket_metadata.h"
#include "s3_factory.h"

class S3PutBucketPolicyAction : public S3BucketAction {
  std::string new_bucket_policy;

 public:
  S3PutBucketPolicyAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory = nullptr);

  void setup_steps();
  void validate_request();
  void consume_incoming_content();
  void validate_request_body(std::string& content);
  void validate_policy();
  void on_policy_validation_success();
  void on_policy_validation_failure();
  void set_policy();
  void set_policy_failed();
  void fetch_bucket_info_failed();
  void send_response_to_s3_client();

  // For Testing purpose
  FRIEND_TEST(S3PutBucketPolicyActionTest, ValidateRequest);
  FRIEND_TEST(S3PutBucketPolicyActionTest, ValidateRequestMoreContent);
  FRIEND_TEST(S3PutBucketPolicyActionTest, SetPolicy);
  FRIEND_TEST(S3PutBucketPolicyActionTest, SetPolicyWhenBucketMissing);
  FRIEND_TEST(S3PutBucketPolicyActionTest, SetPolicyWhenBucketFailed);
  FRIEND_TEST(S3PutBucketPolicyActionTest, SetPolicyWhenBucketFailedToLaunch);
  FRIEND_TEST(S3PutBucketPolicyActionTest,
              SendResponseToClientServiceUnavailable);
  FRIEND_TEST(S3PutBucketPolicyActionTest, SendResponseToClientMalformedXML);
  FRIEND_TEST(S3PutBucketPolicyActionTest, SendResponseToClientNoSuchBucket);
  FRIEND_TEST(S3PutBucketPolicyActionTest, SendResponseToClientSuccess);
  FRIEND_TEST(S3PutBucketPolicyActionTest, SendResponseToClientInternalError);
};

#endif
