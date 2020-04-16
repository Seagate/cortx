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
 * Original author:  Basavaraj Kirunge   <basavaraj.kirunge@seagate.com>
 * Original creation date: 10-Aug-2019
 */

#include "s3_bucket_action_base.h"
#include "s3_clovis_layout.h"
#include "s3_error_codes.h"
#include "s3_option.h"
#include "s3_stats.h"

S3BucketAction::S3BucketAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory,
    bool check_shutdown, std::shared_ptr<S3AuthClientFactory> auth_factory,
    bool skip_auth)
    : S3Action(std::move(req), check_shutdown, std::move(auth_factory),
               skip_auth) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  if (bucket_meta_factory) {
    bucket_metadata_factory = std::move(bucket_meta_factory);
  } else {
    bucket_metadata_factory = std::make_shared<S3BucketMetadataFactory>();
  }
}

S3BucketAction::~S3BucketAction() {
  s3_log(S3_LOG_DEBUG, request_id, "Destructor\n");
}

void S3BucketAction::fetch_bucket_info() {
  s3_log(S3_LOG_INFO, request_id, "Fetching bucket metadata\n");
  if (s3_fi_is_enabled("fail_fetch_bucket_info")) {
    s3_fi_enable_once("clovis_kv_get_fail");
  }
  bucket_metadata =
      bucket_metadata_factory->create_bucket_metadata_obj(request);
  bucket_metadata->load(
      std::bind(&S3BucketAction::next, this),
      std::bind(&S3BucketAction::fetch_bucket_info_failed, this));
  // for shutdown testcases, check FI and set shutdown signal
  S3_CHECK_FI_AND_SET_SHUTDOWN_SIGNAL(
      "put_bucket_acl_action_fetch_bucket_info_shutdown_fail");
  S3_CHECK_FI_AND_SET_SHUTDOWN_SIGNAL(
      "put_bucket_policy_action_get_metadata_shutdown_fail");
  S3_CHECK_FI_AND_SET_SHUTDOWN_SIGNAL(
      "put_object_tagging_action_fetch_bucket_info_shutdown_fail");
}

void S3BucketAction::load_metadata() { fetch_bucket_info(); }

void S3BucketAction::set_authorization_meta() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  auth_client->set_acl_and_policy(bucket_metadata->get_encoded_bucket_acl(),
                                  bucket_metadata->get_policy_as_json());
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
