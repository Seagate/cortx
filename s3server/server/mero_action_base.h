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
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 1-JUNE-2019
 */

#pragma once

#ifndef __S3_SERVER_MERO_ACTION_BASE_H__
#define __S3_SERVER_MERO_ACTION_BASE_H__

#include <functional>
#include <memory>
#include <vector>

#include "action_base.h"
#include "s3_factory.h"
#include "s3_fi_common.h"
#include "s3_log.h"
#include "mero_request_object.h"

#define BACKGROUND_STALE_OBJECT_DELETE_ACCOUNT "s3-background-delete-svc"

// Derived Action Objects will have steps (member functions)
// required to complete the action.
// All member functions should perform an async operation as
// we do not want to block the main thread that manages the
// action. The async callbacks should ensure to call next or
// done/abort etc depending on the result of the operation.
class MeroAction : public Action {
 protected:
  std::shared_ptr<MeroRequestObject> request;

 public:
  MeroAction(std::shared_ptr<MeroRequestObject> req, bool check_shutdown = true,
             std::shared_ptr<S3AuthClientFactory> auth_factory = nullptr,
             bool skip_auth = false);
  virtual ~MeroAction();

  // Register all the member functions required to complete the action.
  // This can register the function as
  // task_list.push_back(std::bind( &S3SomeDerivedAction::step1, this ));
  // Ensure you call this in Derived class constructor.
  virtual void setup_steps();

  // Common steps for all Actions like Authorization.
  void check_authorization();

  FRIEND_TEST(MeroActionTest, Constructor);
  FRIEND_TEST(MeroActionTest, ClientReadTimeoutCallBackRollback);
  FRIEND_TEST(MeroActionTest, ClientReadTimeoutCallBack);
  FRIEND_TEST(MeroActionTest, AddTask);
  FRIEND_TEST(MeroActionTest, AddTaskRollback);
  FRIEND_TEST(MeroActionTest, TasklistRun);
  FRIEND_TEST(MeroActionTest, RollbacklistRun);
  FRIEND_TEST(MeroActionTest, SkipAuthTest);
  FRIEND_TEST(MeroActionTest, EnableAuthTest);
  FRIEND_TEST(MeroActionTest, SetSkipAuthFlagAndSetS3OptionDisableAuthFlag);
  FRIEND_TEST(S3APIHandlerTest, DispatchActionTest);
};

#endif
