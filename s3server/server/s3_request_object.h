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

#ifndef __S3_SERVER_S3_REQUEST_OBJECT_H__
#define __S3_SERVER_S3_REQUEST_OBJECT_H__

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

/* libevhtp */
#include <gtest/gtest_prod.h>
#include "evhtp_wrapper.h"
#include "event_wrapper.h"

#include "request_object.h"
#include "s3_async_buffer_opt.h"
#include "s3_chunk_payload_parser.h"
#include "s3_log.h"
#include "s3_option.h"
#include "s3_perf_logger.h"
#include "s3_timer.h"
#include "s3_uuid.h"
#include "s3_audit_info.h"

class S3AsyncBufferOptContainerFactory;

class S3RequestObject : public RequestObject {
  std::string bucket_name;
  std::string object_name;
  std::string default_acl;
  std::string s3_action;

  S3AuditInfo audit_log_obj;
  size_t object_size;

  S3ApiType s3_api_type;
  S3OperationCode s3_operation_code;

 public:
  S3RequestObject(
      evhtp_request_t* req, EvhtpInterface* evhtp_obj_ptr,
      std::shared_ptr<S3AsyncBufferOptContainerFactory> async_buf_factory =
          nullptr,
      EventInterface* event_obj_ptr = nullptr);
  virtual ~S3RequestObject();
  void set_api_type(S3ApiType apitype);
  virtual S3ApiType get_api_type();
  void set_operation_code(S3OperationCode operation_code);
  virtual S3OperationCode get_operation_code();
  virtual void populate_and_log_audit_info();

 public:
  virtual void set_object_size(size_t obj_size);
  // Operation params.
  std::string get_object_uri();
  std::string get_action_str();
  S3AuditInfo& get_audit_info();

  virtual void set_bucket_name(const std::string& name);
  virtual const std::string& get_bucket_name();
  virtual void set_object_name(const std::string& name);
  virtual const std::string& get_object_name();
  virtual void set_action_str(const std::string& action);
  virtual void set_default_acl(const std::string& name);
  virtual const std::string& get_default_acl();

 public:

  FRIEND_TEST(S3MockAuthClientCheckTest, CheckAuth);
  FRIEND_TEST(S3RequestObjectTest, ReturnsValidUriPaths);
  FRIEND_TEST(S3RequestObjectTest, ReturnsValidRawQuery);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameValidNameTest1);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameValidNameTest2);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameValidNameTest3);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameValidNameTest4);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameValidNameTest5);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameValidNameTest6);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest1);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest2);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest3);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest4);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest5);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest6);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest7);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest8);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest9);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest10);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest11);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest12);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest13);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest14);
  FRIEND_TEST(S3PutBucketActionTest, ValidateBucketNameInvalidNameTest15);
  FRIEND_TEST(S3RequestObjectTest, SetStartClientRequestReadTimeout);
  FRIEND_TEST(S3RequestObjectTest, StopClientReadTimerNull);
  FRIEND_TEST(S3RequestObjectTest, StopClientReadTimer);
  FRIEND_TEST(S3RequestObjectTest, FreeReadTimer);
  FRIEND_TEST(S3RequestObjectTest, FreeReadTimerNull);
  FRIEND_TEST(S3RequestObjectTest, TriggerClientReadTimeoutNoCallback);
  FRIEND_TEST(S3RequestObjectTest, TriggerClientReadTimeout);
};

#endif
