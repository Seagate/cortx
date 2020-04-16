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

#include "action_base.h"
#include "s3_clovis_layout.h"
#include "s3_error_codes.h"
#include "s3_option.h"
#include "s3_stats.h"

std::map<std::string, uint64_t> Action::s3_task_name_to_addb_task_id_map;

void Action::s3_task_name_to_addb_task_id_map_init() {
  if (s3_task_name_to_addb_task_id_map.size() ==
      g_s3_to_addb_idx_func_name_map_size) {
    return;
  }

  uint64_t idx = 0;
  for (; idx < g_s3_to_addb_idx_func_name_map_size; ++idx) {
    s3_task_name_to_addb_task_id_map[g_s3_to_addb_idx_func_name_map[idx]] =
        idx + ADDB_TASK_LIST_OFFSET;
  }
}

Action::Action(std::shared_ptr<RequestObject> req, bool check_shutdown,
               std::shared_ptr<S3AuthClientFactory> auth_factory,
               bool skip_auth)
    : base_request(req),
      check_shutdown_signal(check_shutdown),
      is_response_scheduled(false),
      is_fi_hit(false),
      invalid_request(false),
      skip_auth(skip_auth) {

  s3_task_name_to_addb_task_id_map_init();

  addb_action_type_id = S3_ADDB_ACTION_BASE_ID;
  request_id = base_request->get_request_id();
  addb_request_id = base_request->addb_request_id;

  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  task_iteration_index = 0;
  rollback_index = 0;

  state = ACTS_START;
  rollback_state = ACTS_START;
  ADDB(get_addb_action_type_id(), addb_request_id, (uint64_t)state);

  mem_profile.reset(new S3MemoryProfile());

  if (auth_factory) {
    auth_client_factory = std::move(auth_factory);
  } else {
    auth_client_factory = std::make_shared<S3AuthClientFactory>();
  }
  auth_client = auth_client_factory->create_auth_client(std::move(req));
  setup_steps();
}

Action::~Action() { s3_log(S3_LOG_DEBUG, request_id, "Destructor\n"); }

void Action::set_s3_error(std::string code) {
  state = ACTS_ERROR;
  s3_error_code = std::move(code);
}

void Action::set_s3_error_message(std::string message) {
  s3_error_message = std::move(message);
}

void Action::client_read_error() {
  set_s3_error(base_request->get_s3_client_read_error());
  rollback_start();
}

const std::string& Action::get_s3_error_code() const { return s3_error_code; }

const std::string& Action::get_s3_error_message() const {
  return s3_error_message;
}

bool Action::is_error_state() const { return state == ACTS_ERROR; }

void Action::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setup the action\n");

  check_authorization_header();

  s3_log(S3_LOG_DEBUG, request_id,
         "S3Option::is_auth_disabled: (%d), skip_auth: (%d)\n",
         S3Option::get_instance()->is_auth_disabled(), skip_auth);

  if (!S3Option::get_instance()->is_auth_disabled() && !skip_auth &&
      (is_authorizationheader_present)) {

    ACTION_TASK_ADD(Action::check_authentication, this);
  }
}

void Action::check_authorization_header() {
  is_authorizationheader_present = false;
  for (auto it : base_request->get_in_headers_copy()) {
    if (strcmp(it.first.c_str(), "Authorization") == 0) {
      is_authorizationheader_present = true;
    }
  }
  auth_client->set_is_authheader_present(is_authorizationheader_present);
}

void Action::start() {

  if (check_shutdown_signal && check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }

  task_iteration_index = 0;
  if (task_list.size() > 0) {
    state = ACTS_RUNNING;

    ADDB(get_addb_action_type_id(), addb_request_id, (uint64_t)state);
    ADDB(get_addb_action_type_id(), addb_request_id,
         task_addb_id_list[task_iteration_index]);

    task_list[task_iteration_index++]();
  }
}

// Step to next async step.
void Action::next() {
  if (check_shutdown_signal && check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  if (base_request->is_s3_client_read_error()) {
    client_read_error();
    return;
  }
  if (task_iteration_index < task_list.size()) {
    if (base_request->client_connected()) {

      ADDB(get_addb_action_type_id(), addb_request_id,
           task_addb_id_list[task_iteration_index]);

      task_list[task_iteration_index++]();
    } else {
      rollback_start();
    }
  } else {
    done();
  }
}

void Action::done() {
  task_iteration_index = 0;
  state = ACTS_COMPLETE;
  ADDB(get_addb_action_type_id(), addb_request_id, (uint64_t)state);
  i_am_done();
}

void Action::pause() {
  // Set state as Paused.
  state = ACTS_PAUSED;
  ADDB(get_addb_action_type_id(), addb_request_id, (uint64_t)state);
}

void Action::resume() {
  // Resume only if paused.
  state = ACTS_RUNNING;
  ADDB(get_addb_action_type_id(), addb_request_id, (uint64_t)state);
}

void Action::abort() {
  // Mark state as Aborted.
  task_iteration_index = 0;
  state = ACTS_STOPPED;
  ADDB(get_addb_action_type_id(), addb_request_id, (uint64_t)state);
}

// rollback async steps
void Action::rollback_start() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  base_request->stop_processing_incoming_data();
  if (rollback_state > ACTS_START) {
    s3_log(S3_LOG_WARN, request_id,
           "rollback_start() has been already invoked\n");
    return;
  }
  rollback_index = 0;
  rollback_state = ACTS_RUNNING;
  if (rollback_list.size())
    rollback_list[rollback_index++]();
  else {
    s3_log(S3_LOG_DEBUG, request_id, "Rollback triggered on empty list\n");
    rollback_done();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void Action::rollback_next() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  if (base_request->client_connected() &&
      rollback_index < rollback_list.size()) {
    // Call step and move index to next
    rollback_list[rollback_index++]();
  } else {
    rollback_done();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void Action::rollback_done() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  rollback_index = 0;
  rollback_state = ACTS_COMPLETE;
  rollback_exit();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void Action::rollback_exit() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void Action::check_authentication() {
  auth_timer.start();
  auth_client->check_authentication(
      std::bind(&Action::check_authentication_successful, this),
      std::bind(&Action::check_authentication_failed, this));
}

void Action::check_authentication_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  auth_timer.stop();
  const auto mss = auth_timer.elapsed_time_in_millisec();
  LOG_PERF("check_authentication_ms", request_id.c_str(), mss);
  s3_stats_timing("check_authentication", mss);

  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void Action::check_authentication_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  if (base_request->client_connected()) {
    std::string error_code = auth_client->get_error_code();
    std::string error_message = auth_client->get_error_message();
    if (error_code == "InvalidAccessKeyId") {
      s3_stats_inc("authentication_failed_invalid_accesskey_count");
    } else if (error_code == "SignatureDoesNotMatch") {
      s3_stats_inc("authentication_failed_signature_mismatch_count");
    }
    s3_log(S3_LOG_ERROR, request_id, "Authentication failure: %s\n",
           error_code.c_str());
    base_request->respond_error(error_code, {}, error_message);
  }
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

bool Action::check_shutdown_and_rollback(bool check_auth_op_aborted) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  bool is_s3_shutting_down =
      S3Option::get_instance()->get_is_s3_shutting_down();
  if (is_s3_shutting_down) {
    base_request->stop_processing_incoming_data();
  }
  if (is_s3_shutting_down && check_auth_op_aborted &&
      get_auth_client()->is_chunk_auth_op_aborted()) {
    // Cleanup/rollback will be done after response.
    send_response_to_s3_client();
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return is_s3_shutting_down;
  }
  if (!is_response_scheduled && is_s3_shutting_down) {
    s3_log(S3_LOG_DEBUG, request_id, "S3 server is about to shutdown\n");
    base_request->pause();
    is_response_scheduled = true;
    if (s3_error_code.empty()) {
      set_s3_error("ServiceUnavailable");
    }
    // Cleanup/rollback will be done after response.
    send_response_to_s3_client();
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return is_s3_shutting_down;
}

void Action::send_retry_error_to_s3_client(int retry_after_in_secs) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  base_request->respond_retry_after(1);
  done();
}

