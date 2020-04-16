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
 * Original author:  Kaustubh Deorukhkar  <kaustubh.deorukhkar@seagate.com>
 * Original author:  Priya Saboo  <priya.chhagan@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#pragma once

#ifndef __S3_SERVER_S3_GET_BUCKET_LOCATION_ACTION_H__
#define __S3_SERVER_S3_GET_BUCKET_LOCATION_ACTION_H__

#include <memory>

#include "s3_bucket_action_base.h"
#include "s3_bucket_metadata.h"

class S3GetBucketlocationAction : public S3BucketAction {

 public:
  S3GetBucketlocationAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory = nullptr);

  void setup_steps();

  virtual void fetch_bucket_info_failed();
  virtual void send_response_to_s3_client();

  FRIEND_TEST(S3GetBucketLocationActionTest, BucketMetadataMustNotBeNull);
  FRIEND_TEST(S3GetBucketLocationActionTest, FetchBucketInfoFailedWithMissing);
  FRIEND_TEST(S3GetBucketLocationActionTest, FetchBucketInfoFailedWithFailed);
  FRIEND_TEST(S3GetBucketLocationActionTest, SendResponseWhenShuttingDown);
  FRIEND_TEST(S3GetBucketLocationActionTest, SendErrorResponse);
  FRIEND_TEST(S3GetBucketLocationActionTest, SendAnyFailedResponse);
  FRIEND_TEST(S3GetBucketLocationActionTest, SendAnySuccessResponse);
};
#endif
