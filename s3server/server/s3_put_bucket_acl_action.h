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
 * Original creation date: 19-May-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_PUT_BUCKET_ACL_ACTION_H__
#define __S3_SERVER_S3_PUT_BUCKET_ACL_ACTION_H__

#include <gtest/gtest_prod.h>
#include <memory>
#include <string>

#include "s3_bucket_action_base.h"
#include "s3_factory.h"

class S3PutBucketACLAction : public S3BucketAction {

  std::string user_input_acl;

 public:
  S3PutBucketACLAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory = nullptr);

  void setup_steps();
  void validate_request();
  void consume_incoming_content();
  void fetch_bucket_info_failed();
  void on_aclvalidation_success();
  void on_aclvalidation_failure();
  void validate_acl_with_auth();
  void setvalidateacl();
  void setacl();
  void setacl_failed();
  void send_response_to_s3_client();

  FRIEND_TEST(S3PutBucketAclActionTest, Constructor);
  FRIEND_TEST(S3PutBucketAclActionTest, FetchBucketInfo);
  FRIEND_TEST(S3PutBucketAclActionTest, FetchBucketInfoFailedWithMissing);
  FRIEND_TEST(S3PutBucketAclActionTest, FetchBucketInfoFailed);
  FRIEND_TEST(S3PutBucketAclActionTest, SendResponseWhenShuttingDown);
  FRIEND_TEST(S3PutBucketAclActionTest, SendErrorResponse);
  FRIEND_TEST(S3PutBucketAclActionTest, SendAnyFailedResponse);
  FRIEND_TEST(S3PutBucketAclActionTest, SetAclShouldUpdateMetadata);
  FRIEND_TEST(S3PutBucketAclActionTest, ValidateOnAllDataShouldCallNext);
  FRIEND_TEST(S3PutBucketAclActionTest, ValidateOnPartialDataShouldWaitForMore);
};

#endif
