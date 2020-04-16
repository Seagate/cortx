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
 * Original creation date: 31-Mar-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_GET_BUCKET_ACL_ACTION_H__
#define __S3_SERVER_S3_GET_BUCKET_ACL_ACTION_H__

#include <gtest/gtest_prod.h>
#include <memory>

#include "s3_bucket_action_base.h"
#include "s3_bucket_metadata.h"

class S3GetBucketACLAction : public S3BucketAction {

 public:
  S3GetBucketACLAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory = nullptr);

  void setup_steps();

  void fetch_bucket_info_failed();
  void send_response_to_s3_client();

  FRIEND_TEST(S3GetBucketAclActionTest, Constructor);
  FRIEND_TEST(S3GetBucketAclActionTest, FetchBucketInfoFailedWithMissing);
  FRIEND_TEST(S3GetBucketAclActionTest, FetchBucketInfoFailed);
  FRIEND_TEST(S3GetBucketAclActionTest, SendResponseWhenShuttingDown);
  FRIEND_TEST(S3GetBucketAclActionTest, SendErrorResponse);
  FRIEND_TEST(S3GetBucketAclActionTest, SendAnyFailedResponse);
  FRIEND_TEST(S3GetBucketAclActionTest, SendSuccessResponse);
};

#endif
