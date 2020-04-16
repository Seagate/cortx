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
 * Original author:  Abhilekh Mustapure   <abhilekh.mustapure@seagate.com>
 * Original creation date: 15-Jan-2019
 */

#pragma once

#ifndef __S3_SERVER_S3_DELETE_BUCKET_TAGGING_ACTION_H__
#define __S3_SERVER_S3_DELETE_BUCKET_TAGGING_ACTION_H__

#include <gtest/gtest_prod.h>
#include <memory>

#include "s3_bucket_action_base.h"
#include "s3_bucket_metadata.h"
#include "s3_factory.h"

class S3DeleteBucketTaggingAction : public S3BucketAction {

 public:
  S3DeleteBucketTaggingAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory = nullptr);

  void setup_steps();
  void delete_bucket_tags();
  void delete_bucket_tags_failed();
  void fetch_bucket_info_failed();

  void send_response_to_s3_client();

  // For Testing purpose
  FRIEND_TEST(S3DeleteBucketTaggingActionTest, DeleteBucketTagging);
  FRIEND_TEST(S3DeleteBucketTaggingActionTest, DeleteBucketTaggingSuccessful);
  FRIEND_TEST(S3DeleteBucketTaggingActionTest,
              FetchBucketMetadataFailed_Metadatafailed);
  FRIEND_TEST(S3DeleteBucketTaggingActionTest,
              FetchBucketMetadaFailed_Metadatafailedtolaunch);
  FRIEND_TEST(S3DeleteBucketTaggingActionTest,
              FetchBucketMetadataFailed_Metadatamissing);
  FRIEND_TEST(S3DeleteBucketTaggingActionTest,
              DeleteBucketTaggingFailed_Metadatafailed);
  FRIEND_TEST(S3DeleteBucketTaggingActionTest,
              DeleteBucketTaggingFailed_Metadatafailedtolaunch);
  FRIEND_TEST(S3DeleteBucketTaggingActionTest,
              SendResponseToClientNoSuchBucket);
  FRIEND_TEST(S3DeleteBucketTaggingActionTest, SendResponseToClientSuccess);
  FRIEND_TEST(S3DeleteBucketTaggingActionTest,
              SendResponseToClientInternalError);
};

#endif
