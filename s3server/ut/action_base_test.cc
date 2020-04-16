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
 * Original author:  Abrarahmed Momin  <abrar.habib@seagate.com>
 * Original creation date: 9th-June-2016
 */

#include <functional>
#include <iostream>
#include "gtest/gtest.h"
#include "mock_evhtp_wrapper.h"
#include "mock_s3_memory_profile.h"
#include "mock_s3_request_object.h"
#include "action_base.h"
#include "s3_error_codes.h"

using ::testing::Mock;
using ::testing::Return;
using ::testing::Eq;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::ReturnRef;

// Before you create an object of this class, ensure
// auth is disabled for unit tests.
class ActionTestbase : public Action {
 public:
  // Instantiate S3Action with authentication disabled
  ActionTestbase(std::shared_ptr<S3RequestObject> req) : Action(req) {
    response_called = 0;
    profile = new MockS3MemoryProfile();
    mem_profile.reset(profile);
  };

  void set_defaults() {
    EXPECT_CALL(*profile, we_have_enough_memory_for_put_obj(_))
        .WillRepeatedly(Return(true));
  }

  void send_response_to_s3_client() {
    response_called += 1;
  };

  // Declare viariables
  int response_called;
  MockS3MemoryProfile *profile;
};

class ActionTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.

  ActionTest() {
    evbase_t *evbase = event_base_new();
    evhtp_request_t *req = evhtp_request_new(NULL, evbase);
    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(req, new EvhtpWrapper());
    call_count_one = call_count_two = 0;
    S3Option::get_instance()->disable_auth();
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    ptr_Actionobject = new ActionTestbase(ptr_mock_request);
    EXPECT_CALL(*ptr_mock_request, get_api_type())
        .WillRepeatedly(Return(S3ApiType::object));
    EXPECT_CALL(*ptr_mock_request, http_verb())
        .WillRepeatedly(Return(S3HttpVerb::GET));
  };

  ~ActionTest() { delete ptr_Actionobject; }

  // Declares the variables your tests want to use.
  std::function<void()> fptr_callback;
  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  ActionTestbase *ptr_Actionobject;

  int call_count_one;
  int call_count_two;

 public:
  void func_callback_one() { call_count_one += 1; }
  void func_callback_two() { call_count_two += 1; }
};

TEST_F(ActionTest, Constructor) {
  ptr_Actionobject->set_defaults();
  EXPECT_EQ(NULL, ptr_Actionobject->task_iteration_index);
  EXPECT_TRUE(ACTS_START == ptr_Actionobject->state);
  EXPECT_EQ(0, ptr_Actionobject->task_list.size());
  EXPECT_EQ(0, ptr_Actionobject->rollback_list.size());
  EXPECT_EQ(NULL, call_count_one);
  EXPECT_EQ(NULL, call_count_two);
}

TEST_F(ActionTest, ClientReadTimeoutCallBackRollback) {
  ptr_Actionobject->rollback_state = ACTS_START;
  ptr_Actionobject->add_task_rollback(
      std::bind(&ActionTest::func_callback_one, this));
  ptr_Actionobject->add_task_rollback(
      std::bind(&ActionTest::func_callback_two, this));
  ptr_Actionobject->client_read_error();
  EXPECT_TRUE(call_count_two == 1);
}

TEST_F(ActionTest, ClientReadTimeoutCallBack) {
  ptr_Actionobject->rollback_state = ACTS_COMPLETE;
  ptr_Actionobject->add_task_rollback(
      std::bind(&ActionTest::func_callback_one, this));
  EXPECT_TRUE(call_count_one == 0);
  ptr_Actionobject->client_read_error();
  EXPECT_TRUE(call_count_one == 0);
  EXPECT_TRUE(ptr_Actionobject->response_called == 0);
}

TEST_F(ActionTest, AddTask) {
  ptr_Actionobject->set_defaults();
  ACTION_TASK_ADD_OBJPTR(ptr_Actionobject, ActionTest::func_callback_one, this);
  ACTION_TASK_ADD_OBJPTR(ptr_Actionobject, ActionTest::func_callback_two, this);
  // test size of list
  EXPECT_EQ(2, ptr_Actionobject->task_list.size());
}

TEST_F(ActionTest, AddTaskRollback) {
  ptr_Actionobject->set_defaults();
  ptr_Actionobject->add_task_rollback(
      std::bind(&ActionTest::func_callback_one, this));
  ptr_Actionobject->add_task_rollback(
      std::bind(&ActionTest::func_callback_two, this));
  // test size of list
  EXPECT_EQ(2, ptr_Actionobject->rollback_list.size());
}

TEST_F(ActionTest, TasklistRun) {
  ptr_Actionobject->set_defaults();
  ACTION_TASK_ADD_OBJPTR(ptr_Actionobject, ActionTest::func_callback_one, this);
  ACTION_TASK_ADD_OBJPTR(ptr_Actionobject, ActionTest::func_callback_two, this);

  ptr_Actionobject->start();
  EXPECT_TRUE(call_count_one == 1);
  ptr_Actionobject->next();
  EXPECT_TRUE(call_count_two == 1);
}

TEST_F(ActionTest, RollbacklistRun) {
  ptr_Actionobject->set_defaults();
  ptr_Actionobject->add_task_rollback(
      std::bind(&ActionTest::func_callback_one, this));
  ptr_Actionobject->add_task_rollback(
      std::bind(&ActionTest::func_callback_two, this));

  EXPECT_EQ(2, ptr_Actionobject->rollback_list.size());

  ptr_Actionobject->rollback_start();
  EXPECT_TRUE(call_count_two == 1);
  ptr_Actionobject->rollback_next();
  EXPECT_TRUE(call_count_one == 1);
  ptr_Actionobject->rollback_next();
}

TEST_F(ActionTest, SkipAuthTest) {
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  ptr_Actionobject->skip_auth = true;
  ptr_Actionobject->setup_steps();
  // No tasks
  EXPECT_EQ(0, ptr_Actionobject->number_of_tasks());
}

TEST_F(ActionTest, EnableAuthTest) {
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  S3Option::get_instance()->enable_auth();
  ptr_Actionobject->setup_steps();
  S3Option::get_instance()->disable_auth();
  // Number of tasks in task list should be only one.
  EXPECT_EQ(1, ptr_Actionobject->number_of_tasks());
}

TEST_F(ActionTest, SetSkipAuthFlagAndSetS3OptionDisableAuthFlag) {
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  S3Option::get_instance()->disable_auth();
  ptr_Actionobject->skip_auth = true;
  ptr_Actionobject->setup_steps();
  // no tasks
  EXPECT_EQ(0, ptr_Actionobject->number_of_tasks());
}

TEST_F(ActionTest, DisableSkipAuthFlagAndSetS3OptionDisableAuthFlag) {
  std::map<std::string, std::string> input_headers;
  input_headers["Authorization"] = "1";
  EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));
  S3Option::get_instance()->disable_auth();
  ptr_Actionobject->setup_steps();
  // no tasks
  EXPECT_EQ(0, ptr_Actionobject->number_of_tasks());
}
