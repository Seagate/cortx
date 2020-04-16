/*
 * COPYRIGHT 2018 SEAGATE LLC
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
 * Original creation date: 25-July-2018
 */

#include <string>
#include "s3_error_codes.h"
#include "s3_head_service_action.h"
#include "s3_iem.h"
#include "s3_log.h"

S3HeadServiceAction::S3HeadServiceAction(std::shared_ptr<S3RequestObject> req)
    : S3Action(req, true, nullptr, true, true) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  setup_steps();
}

void S3HeadServiceAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3HeadServiceAction::send_response_to_s3_client, this);
  // ...
}

void S3HeadServiceAction::send_response_to_s3_client() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  // Disable Audit logs for Haproxy healthchecks
  const char* full_path_uri = request->c_get_full_path();

  if (full_path_uri) {
    if (std::strcmp(full_path_uri, "/") == 0) {
      request->get_audit_info().set_publish_flag(false);
    }
  }
  if (reject_if_shutting_down()) {
    int shutdown_grace_period =
        S3Option::get_instance()->get_s3_grace_period_sec();
    request->set_out_header_value("Retry-After",
                                   std::to_string(shutdown_grace_period));
    request->set_out_header_value("Connection", "close");
    request->send_response(S3HttpFailed503);
  } else {
    request->send_response(S3HttpSuccess200);
  }
  S3_RESET_SHUTDOWN_SIGNAL;  // for shutdown testcases
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
