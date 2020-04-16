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

#ifndef __S3_SERVER_S3_ACTION_BASE_H__
#define __S3_SERVER_S3_ACTION_BASE_H__

#include <functional>
#include <memory>
#include <vector>

#include "action_base.h"
#include "s3_bucket_metadata.h"
#include "s3_factory.h"
#include "s3_fi_common.h"
#include "s3_log.h"
#include "s3_object_metadata.h"

// Derived Action Objects will have steps (member functions)
// required to complete the action.
// All member functions should perform an async operation as
// we do not want to block the main thread that manages the
// action. The async callbacks should ensure to call next or
// done/abort etc depending on the result of the operation.
class S3Action : public Action {
 protected:
  std::shared_ptr<S3RequestObject> request;
 private:
  std::shared_ptr<S3ObjectMetadata> object_metadata;
  std::shared_ptr<S3BucketMetadata> bucket_metadata;
  bool skip_authorization;

 public:
  S3Action(std::shared_ptr<S3RequestObject> req, bool check_shutdown = true,
           std::shared_ptr<S3AuthClientFactory> auth_factory = nullptr,
           bool skip_auth = false, bool skip_authorize = false);
  virtual ~S3Action();

  // Register all the member functions required to complete the action.
  // This can register the function as
  // task_list.push_back(std::bind( &S3SomeDerivedAction::step1, this ));
  // Ensure you call this in Derived class constructor.
  virtual void setup_steps();

  // TODO This can be made pure virtual once its implemented for all action
  // class
  virtual void load_metadata();
  virtual void set_authorization_meta();

  // Common steps for all Actions like Authorization.
  void check_authorization();
  void check_authorization_successful();
  void check_authorization_failed();

  void fetch_acl_policies();
  void fetch_acl_bucket_policies_failed();
  void fetch_acl_object_policies_failed();

  FRIEND_TEST(S3ActionTest, Constructor);
  FRIEND_TEST(S3ActionTest, ClientReadTimeoutCallBackRollback);
  FRIEND_TEST(S3ActionTest, ClientReadTimeoutCallBack);
  FRIEND_TEST(S3ActionTest, AddTask);
  FRIEND_TEST(S3ActionTest, AddTaskRollback);
  FRIEND_TEST(S3ActionTest, TasklistRun);
  FRIEND_TEST(S3ActionTest, RollbacklistRun);
  FRIEND_TEST(S3ActionTest, SkipAuthTest);
  FRIEND_TEST(S3ActionTest, EnableAuthTest);
  FRIEND_TEST(S3ActionTest, SetSkipAuthFlagAndSetS3OptionDisableAuthFlag);
  FRIEND_TEST(S3APIHandlerTest, DispatchActionTest);
};

#endif
