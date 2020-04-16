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

#ifndef __S3_SERVER_S3_PUT_BUCKET_TAG_ACTION_H__
#define __S3_SERVER_S3_PUT_BUCKET_TAG_ACTION_H__

#include <memory>
#include <string>

#include "s3_bucket_action_base.h"
#include "s3_bucket_metadata.h"
#include "s3_factory.h"

class S3PutBucketTaggingAction : public S3BucketAction {
  std::shared_ptr<S3BucketMetadataFactory> bucket_metadata_factory;
  std::shared_ptr<S3PutTagsBodyFactory> put_bucket_tag_body_factory;
  std::shared_ptr<S3PutTagBody> put_bucket_tag_body;

  std::string new_bucket_tags;
  std::map<std::string, std::string> bucket_tags_map;

 public:
  S3PutBucketTaggingAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory = nullptr,
      std::shared_ptr<S3PutTagsBodyFactory> bucket_body_factory = nullptr);

  void setup_steps();
  void validate_request();
  void consume_incoming_content();
  void validate_request_body(std::string content);
  void validate_request_xml_tags();
  void save_tags_to_bucket_metadata();
  void save_tags_to_bucket_metadata_failed();
  void fetch_bucket_info_failed();
  void send_response_to_s3_client();

  // For Testing purpose
  FRIEND_TEST(S3PutBucketTaggingActionTest, Constructor);
  FRIEND_TEST(S3PutBucketTaggingActionTest, ValidateRequest);
  FRIEND_TEST(S3PutBucketTaggingActionTest, ValidateInvalidRequest);
  FRIEND_TEST(S3PutBucketTaggingActionTest, ValidateRequestXmlTags);
  FRIEND_TEST(S3PutBucketTaggingActionTest, ValidateInvalidRequestXmlTags);
  FRIEND_TEST(S3PutBucketTaggingActionTest, ValidateRequestMoreContent);
  FRIEND_TEST(S3PutBucketTaggingActionTest, SetTags);
  FRIEND_TEST(S3PutBucketTaggingActionTest, SetTagsWhenBucketMissing);
  FRIEND_TEST(S3PutBucketTaggingActionTest, SetTagsWhenBucketFailed);
  FRIEND_TEST(S3PutBucketTaggingActionTest, SetTagsWhenBucketFailedToLaunch);
  FRIEND_TEST(S3PutBucketTaggingActionTest,
              SendResponseToClientServiceUnavailable);
  FRIEND_TEST(S3PutBucketTaggingActionTest, SendResponseToClientMalformedXML);
  FRIEND_TEST(S3PutBucketTaggingActionTest, SendResponseToClientNoSuchBucket);
  FRIEND_TEST(S3PutBucketTaggingActionTest, SendResponseToClientSuccess);
  FRIEND_TEST(S3PutBucketTaggingActionTest, SendResponseToClientInternalError);
};

#endif
