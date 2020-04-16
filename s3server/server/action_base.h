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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 1-June-2019
 */

#pragma once

#ifndef __S3_SERVER_ACTION_BASE_H__
#define __S3_SERVER_ACTION_BASE_H__

#include <utility>
#include <functional>
#include <memory>
#include <vector>

#include "s3_auth_client.h"
#include "s3_factory.h"
#include "s3_fi_common.h"
#include "s3_log.h"
#include "s3_memory_profile.h"
#include "request_object.h"
#include "s3_timer.h"
#include "s3_addb.h"

#ifdef ENABLE_FAULT_INJECTION

#define S3_CHECK_FI_AND_SET_SHUTDOWN_SIGNAL(tag)                        \
  do {                                                                  \
    if (s3_fi_is_enabled(tag)) {                                        \
      s3_log(S3_LOG_DEBUG, "", "set shutdown signal for testing...\n"); \
      set_is_fi_hit(true);                                              \
      S3Option::get_instance()->set_is_s3_shutting_down(true);          \
    }                                                                   \
  } while (0)

#define S3_RESET_SHUTDOWN_SIGNAL                                \
  do {                                                          \
    if (get_is_fi_hit()) {                                      \
      s3_log(S3_LOG_DEBUG, "", "reset shutdown signal ...\n");  \
      S3Option::get_instance()->set_is_s3_shutting_down(false); \
      set_is_fi_hit(false);                                     \
    }                                                           \
  } while (0)

#else  // ENABLE_FAULT_INJECTION

#define S3_CHECK_FI_AND_SET_SHUTDOWN_SIGNAL(tag)
#define S3_RESET_SHUTDOWN_SIGNAL

#endif  // ENABLE_FAULT_INJECTION

/* All tasks should be added with the following macro to be sure
 * that proper furntion idx is used */
#define ACTION_TASK_ADD(task_name, obj)                 \
  do {                                                  \
    add_task(std::bind(&task_name, (obj)), #task_name); \
  } while (0)

/* All tasks should be added with the following macro to be sure
 * that proper furntion idx is used.
 * This macro is used in case add_task func needs to be called outside
 * action class via pointer to corresponding obj.
 * Used in tests*/
#define ACTION_TASK_ADD_OBJPTR(ptr, task_name, obj)            \
  do {                                                         \
    (ptr)->add_task(std::bind(&task_name, (obj)), #task_name); \
  } while (0)

// Derived Action Objects will have steps (member functions)
// required to complete the action.
// All member functions should perform an async operation as
// we do not want to block the main thread that manages the
// action. The async callbacks should ensure to call next or
// done/abort etc depending on the result of the operation.
class Action {
 private:
  // Holds mapping from action's task name to addb idx
  static std::map<std::string, uint64_t> s3_task_name_to_addb_task_id_map;
  static void s3_task_name_to_addb_task_id_map_init();

 private:
  std::shared_ptr<RequestObject> base_request;

  // Holds the member functions that will process the request.
  // member function signature should be void fn();
  std::vector<std::function<void()>> task_list;
  // Holds task's addb index
  std::vector<uint64_t> task_addb_id_list;
  size_t task_iteration_index;

  // Hold member functions that will rollback
  // changes in event of error
  std::deque<std::function<void()>> rollback_list;
  size_t rollback_index;

  std::shared_ptr<Action> self_ref;

  std::string s3_error_code;
  std::string s3_error_message;
  ActionState state;
  ActionState rollback_state;

  // If this flag is set then check_shutdown_and_rollback() is called
  // from start() & next() methods.
  bool check_shutdown_signal;
  // In case of shutdown, this flag indicates that a error response
  // is already scheduled.
  bool is_response_scheduled;

  // For shutdown related system tests, if FI gets hit then set this flag
  bool is_fi_hit;

  S3Timer auth_timer;

  bool is_date_header_present_in_request() const;

 protected:
  std::string request_id;
  // ADDB action type id.  See s3_addb.h for details on ADDB.  Should not be
  // used directly, use get_addb_action_type_id() instead.
  enum S3AddbActionTypeId addb_action_type_id;
  // For ADDB, we need request ID. In S3 API, request ID is UUID, which can be
  // represented either as a string, or 128-bit integer.  ADDB does not support
  // neither strings, nor 128bit integers.  So, this addb_request_id below
  // is added to support ADDB logs, and it will be used to create ADDB logs.
  // It is generated in RequestObject (see RequestObject::addb_request_id).
  uint64_t addb_request_id;
  bool invalid_request;
  // Allow class object instiantiation without support for authentication
  bool skip_auth;
  bool is_authorizationheader_present;
  std::shared_ptr<S3AuthClient> auth_client;

  std::shared_ptr<S3MemoryProfile> mem_profile;
  std::shared_ptr<S3AuthClientFactory> auth_client_factory;

 public:
  Action(std::shared_ptr<RequestObject> req, bool check_shutdown = true,
         std::shared_ptr<S3AuthClientFactory> auth_factory = nullptr,
         bool skip_auth = false);
  virtual ~Action();

  void set_s3_error(std::string code);
  void set_s3_error_message(std::string message);
  const std::string& get_s3_error_code() const;
  const std::string& get_s3_error_message() const;
  bool is_error_state() const;
  void client_read_error();

 protected:
  void add_task(std::function<void()> task, const char* func_name) {
    s3_task_name_to_addb_task_id_map_init();

    if (s3_task_name_to_addb_task_id_map.count(func_name) == 0) {
      s3_log(S3_LOG_FATAL, "",
             "Function %s was not found in addb index map. "
             "Make sure you use ACTION_TASK_ADD macro to call add_task. "
             "Regenerate code with addb-codegen.py",
             func_name);
    }
    task_list.push_back(std::move(task));
    task_addb_id_list.push_back(s3_task_name_to_addb_task_id_map[func_name]);
  }

  void clear_tasks() {
    task_list.clear();
    task_addb_id_list.clear();
    task_iteration_index = 0;
  }

  std::shared_ptr<S3AuthClient>& get_auth_client() { return auth_client; }

  virtual void check_authorization_header();

  // Add tasks to list after each successful operation that needs rollback.
  // This list can be used to rollback changes in the event of error
  // during any stage of operation.
  void add_task_rollback(std::function<void()> task) {
    // Insert task at start of list for rollback
    rollback_list.push_front(std::move(task));
  }

  void clear_tasks_rollback() { rollback_list.clear(); }

  size_t number_of_rollback_tasks() const { return rollback_list.size(); }

  ActionState get_rollback_state() const { return rollback_state; }

  // Checks whether S3 is shutting down. If yes then triggers rollback and
  // schedules a response.
  // Return value: true, in case of shutdown.
  virtual bool check_shutdown_and_rollback(bool check_auth_op_aborted = false);

  bool reject_if_shutting_down() const { return is_response_scheduled; }

  // If param 'check_signal' is true then check_shutdown_and_rollback() method
  // will be invoked for next tasks in the queue.
  void check_shutdown_signal_for_next_task(bool check_signal) {
    check_shutdown_signal = check_signal;
  }

  void set_is_fi_hit(bool hit) { is_fi_hit = hit; }

  bool get_is_fi_hit() { return is_fi_hit; }

  // Every Action class gets its own unique unchanging ID.  This method uses
  // RTTI (typeid) to determine what is the actual class of a given instance,
  // and then returns ADDB action type ID for that class.
  static enum S3AddbActionTypeId lookup_addb_action_type_id(
      const Action& instance);
  enum S3AddbActionTypeId get_addb_action_type_id() {
    // Note that typeid() cannot identify the actual class of an instance when
    // used inside the constructor of a base class.  In partially constructed
    // instances it returns the base class which has been constructed so far,
    // not the "final" derived class which is actually being constructed.  And
    // we're using ADDB in Action::Action constructor.  So, to work around
    // that, we'll use S3_ADDB_ACTION_BASE_ID until the object is fully
    // constructed, and will use proper action type id after that.  The lookup
    // method call below will return S3_ADDB_ACTION_BASE_ID unless the class is
    // recognized (i.e. instance is fully constructed), so we'll be attempting
    // the lookup until class is identified.
    if (addb_action_type_id == S3_ADDB_ACTION_BASE_ID) {
      addb_action_type_id = lookup_addb_action_type_id(*this);
    }
    return addb_action_type_id;
  }

 public:
  // Self destructing object.
  void manage_self(std::shared_ptr<Action> ref) { self_ref = std::move(ref); }

  size_t number_of_tasks() const { return task_list.size(); }

  // This *MUST* be the last thing on object. Called @ end of dispatch.
  void i_am_done() { self_ref.reset(); }

  // Register all the member functions required to complete the action.
  // This can register the function as
  // task_list.push_back(std::bind( &S3SomeDerivedAction::step1, this ));
  // Ensure you call this in Derived class constructor.
  virtual void setup_steps();

  // Common Actions.
  void start();
  // Step to next async step.
  void next();
  void done();
  void pause();
  void resume();
  void abort();

  // rollback async steps.
  void rollback_start();
  void rollback_next();
  void rollback_done();
  // Default will call the last task in task_list after exhausting all tasks in
  // rollback_list. It expects last task in task_list to be
  // send_response_to_s3_client;
  virtual void rollback_exit();

  // Common steps for all Actions like Authenticate.
  void check_authentication();
  void check_authentication_successful();
  void check_authentication_failed();

  virtual void send_response_to_s3_client() = 0;
  virtual void send_retry_error_to_s3_client(int retry_after_in_secs = 1);

  FRIEND_TEST(ActionTest, Constructor);
  FRIEND_TEST(ActionTest, ClientReadTimeoutCallBackRollback);
  FRIEND_TEST(ActionTest, ClientReadTimeoutCallBack);
  FRIEND_TEST(ActionTest, AddTask);
  FRIEND_TEST(ActionTest, AddTaskRollback);
  FRIEND_TEST(ActionTest, TasklistRun);
  FRIEND_TEST(ActionTest, RollbacklistRun);
  FRIEND_TEST(ActionTest, SkipAuthTest);
  FRIEND_TEST(ActionTest, EnableAuthTest);
  FRIEND_TEST(ActionTest, SetSkipAuthFlagAndSetS3OptionDisableAuthFlag);
  FRIEND_TEST(S3APIHandlerTest, DispatchActionTest);
  FRIEND_TEST(MeroAPIHandlerTest, DispatchActionTest);
};

#endif

