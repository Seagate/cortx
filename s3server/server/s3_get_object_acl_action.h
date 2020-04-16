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
 * Original creation date: 20-May-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_GET_OBJECT_ACL_ACTION_H__
#define __S3_SERVER_S3_GET_OBJECT_ACL_ACTION_H__

#include <gtest/gtest_prod.h>
#include <memory>

#include "s3_object_action_base.h"
#include "s3_factory.h"

class S3GetObjectACLAction : public S3ObjectAction {

 public:
  S3GetObjectACLAction(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory = nullptr,
      std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory = nullptr);

  void setup_steps();

  void fetch_bucket_info_failed();

  void fetch_object_info_failed();
  void send_response_to_s3_client();

  FRIEND_TEST(S3GetObjectAclActionTest, Constructor);
  FRIEND_TEST(S3GetObjectAclActionTest, FetchBucketInfo);
  FRIEND_TEST(S3GetObjectAclActionTest, FetchBucketInfoFailedNoSuchBucket);
  FRIEND_TEST(S3GetObjectAclActionTest, FetchBucketInfoFailedInternalError);
  FRIEND_TEST(S3GetObjectAclActionTest, FetchObjectInfoEmpty);
  FRIEND_TEST(S3GetObjectAclActionTest, FetchObjectInfo);
  FRIEND_TEST(S3GetObjectAclActionTest, FetchObjectInfoFailedMissing);
  FRIEND_TEST(S3GetObjectAclActionTest, FetchObjectInfoFailedInternalError);
  FRIEND_TEST(S3GetObjectAclActionTest, SendResponseToClientServiceUnavailable);
  FRIEND_TEST(S3GetObjectAclActionTest, SendResponseToClientInternalError);
  FRIEND_TEST(S3GetObjectAclActionTest, SendResponseToClientSuccess);
};

#endif
