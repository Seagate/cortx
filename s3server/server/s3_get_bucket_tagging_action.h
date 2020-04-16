/*
 * COPYRIGHT 2019 SEAGATE LLC
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
 * Original author:  Siddhivinayak Shanbhag <siddhivinayak.shanbhag@seagate.com>
 * Original creation date: 09-January-2019
 */

#pragma once

#ifndef __S3_SERVER_S3_GET_BUCKET_TAGGING_ACTION_H__
#define __S3_SERVER_S3_GET_BUCKET_TAGGING_ACTION_H__

#include <gtest/gtest_prod.h>
#include <memory>

#include "s3_bucket_action_base.h"
#include "s3_bucket_metadata.h"
#include "s3_factory.h"

class S3GetBucketTaggingAction : public S3BucketAction {

 public:
  S3GetBucketTaggingAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory = nullptr);

  void setup_steps();
  void check_metadata_missing_status();
  void fetch_bucket_info_failed();
  void send_response_to_s3_client();

  // google unit tests
  FRIEND_TEST(S3GetBucketTaggingActionTest,
              SendResponseToClientServiceUnavailable);
  FRIEND_TEST(S3GetBucketTaggingActionTest, SendResponseToClientNoSuchBucket);
  FRIEND_TEST(S3GetBucketTaggingActionTest, SendResponseToClientSuccess);
  FRIEND_TEST(S3GetBucketTaggingActionTest, SendResponseToClientInternalError);
  FRIEND_TEST(S3GetBucketTaggingActionTest,
              SendResponseToClientNoSuchTagSetError);
};

#endif
