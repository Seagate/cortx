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
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#include <event2/event.h>
#include <evhttp.h>
#include <unistd.h>
#include <string>

#include "s3_auth_client.h"
#include "s3_auth_fake.h"
#include "s3_common.h"
#include "s3_error_codes.h"
#include "s3_fi_common.h"
#include "s3_iem.h"
#include "s3_option.h"
#include "s3_common_utilities.h"
#include "atexit.h"

void s3_auth_op_done_on_main_thread(evutil_socket_t, short events,
                                    void *user_data) {
  if (user_data == NULL) {
    s3_log(S3_LOG_DEBUG, "", "Entering\n");
    s3_log(S3_LOG_ERROR, "", "Input argument user_data is NULL\n");
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  struct user_event_context *user_context =
      (struct user_event_context *)user_data;
  S3AsyncOpContextBase *context = (S3AsyncOpContextBase *)user_context->app_ctx;
  if (context == NULL) {
    s3_log(S3_LOG_ERROR, "", "context pointer is NULL\n");
  }
  const auto request_id = context->get_request()->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  struct event *s3user_event = (struct event *)user_context->user_event;
  if (s3user_event == NULL) {
    s3_log(S3_LOG_ERROR, request_id, "User event is NULL\n");
  }

  if (context->is_at_least_one_op_successful()) {
    context->on_success_handler()();  // Invoke the handler.
  } else {
    context->on_failed_handler()();  // Invoke the handler.
  }
  free(user_data);
  // Free user event
  if (s3user_event) event_free(s3user_event);
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

// Post success handler on main thread so that current auth callback stack can
// unwind and let the current request-response of evhtp to complete.
// This is primarily required as evhtp request object should not be deleted
// during the response callbacks like auth_response.
void s3_auth_op_success(void *application_context, int rc) {
  S3AsyncOpContextBase *app_ctx = (S3AsyncOpContextBase *)application_context;
  const auto request_id = app_ctx->get_request()->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  s3_log(S3_LOG_DEBUG, request_id, "Error code = %d\n", rc);
  app_ctx->set_op_errno_for(0, rc);
  struct user_event_context *user_ctx =
      (struct user_event_context *)calloc(1, sizeof(struct user_event_context));
  user_ctx->app_ctx = app_ctx;
#ifdef S3_GOOGLE_TEST
  evutil_socket_t test_sock = 0;
  short events = 0;
  s3_auth_op_done_on_main_thread(test_sock, events, (void *)user_ctx);
#else
  S3PostToMainLoop((void *)user_ctx)(s3_auth_op_done_on_main_thread,
                                     request_id);
#endif  // S3_GOOGLE_TEST
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

/* S3 Auth client callbacks */

extern "C" evhtp_res on_auth_conn_err_callback(evhtp_connection_t *connection,
                                               evhtp_error_flags errtype,
                                               void *arg) {
  S3AuthClientOpContext *context = (S3AuthClientOpContext *)arg;
  if (context == NULL) {
    s3_log(S3_LOG_ERROR, "", "input arg is NULL\n");
    return EVHTP_RES_OK;
  }
  const auto request_id = context->get_request()->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  if (connection == NULL) {
    s3_log(S3_LOG_ERROR, request_id, "input connection is NULL\n");
    return EVHTP_RES_OK;
  }
  if (connection != context->get_auth_op_ctx()->conn) {
    s3_log(S3_LOG_ERROR, request_id,
           "Mismatch between input connection and auth context connection\n");
  }
  std::string errtype_str =
      S3CommonUtilities::evhtp_error_flags_description(errtype);
  if (connection->request) {
    s3_log(S3_LOG_INFO, request_id, "Connection status = %d\n",
           connection->request->status);
  }
  s3_log(
      S3_LOG_WARN, request_id, "Socket error: %s, errno: %d, set errtype: %s\n",
      evutil_socket_error_to_string(evutil_socket_geterror(connection->sock)),
      evutil_socket_geterror(connection->sock), errtype_str.c_str());
  // Note: Do not remove this, else you will have s3 crashes as the
  // callbacks are invoked after request/connection is freed.
  evhtp_unset_all_hooks(&context->get_auth_op_ctx()->conn->hooks);
  if (context->get_auth_op_ctx()->authrequest != NULL) {
    evhtp_unset_all_hooks(&context->get_auth_op_ctx()->authrequest->hooks);
  }
  if (context->get_auth_op_ctx()->authorization_request != NULL) {
    evhtp_unset_all_hooks(
        &context->get_auth_op_ctx()->authorization_request->hooks);
  }

  if (!context->get_request()->client_connected()) {
    // S3 client has already disconnected, ignore
    s3_log(S3_LOG_DEBUG, request_id, "S3 Client has already disconnected.\n");
    // Calling failed handler to do proper cleanup to avoid leaks
    // i.e cleanup of S3Request and respective action chain.
    context->on_failed_handler()();  // Invoke the handler.
    return EVHTP_RES_OK;
  }
  context->set_op_status_for(0, S3AsyncOpStatus::connection_failed,
                             "Cannot connect to Auth server.");
  context->set_auth_response_xml("", false);
  context->on_failed_handler()();  // Invoke the handler.
  return EVHTP_RES_OK;
}

extern "C" evhtp_res on_conn_err_callback(evhtp_connection_t *connection,
                                          evhtp_error_flags errtype,
                                          void *arg) {
  S3AuthClientOpContext *context = static_cast<S3AuthClientOpContext *>(arg);
  const auto request_id = context->get_request()->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  // Note: Do not remove this, else you will have s3 crashes as the
  // callbacks are invoked after request/connection is freed.
  evhtp_unset_all_hooks(&context->get_auth_op_ctx()->conn->hooks);
  if (context->get_auth_op_ctx()->authrequest) {
    evhtp_unset_all_hooks(
        &context->get_auth_op_ctx()->aclvalidation_request->hooks);
  }
  if (context->get_auth_op_ctx()->aclvalidation_request) {
    evhtp_unset_all_hooks(
        &context->get_auth_op_ctx()->aclvalidation_request->hooks);
  }
  if (context->get_auth_op_ctx()->policyvalidation_request) {
    evhtp_unset_all_hooks(
        &context->get_auth_op_ctx()->policyvalidation_request->hooks);
  }

  if (!context->get_request()->client_connected()) {
    // S3 client has already disconnected, ignore
    s3_log(S3_LOG_DEBUG, request_id, "S3 Client has already disconnected.\n");
    // Calling failed handler to do proper cleanup to avoid leaks
    // i.e cleanup of S3Request and respective action chain.
    context->on_failed_handler()();  // Invoke the handler.
    return EVHTP_RES_OK;
  }
  context->set_op_status_for(0, S3AsyncOpStatus::connection_failed,
                             "Cannot connect to Auth server.");
  context->set_auth_response_xml("", false);
  context->on_failed_handler()();  // Invoke the handler.
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
  return EVHTP_RES_OK;
}

extern "C" evhtp_res on_authorization_response(evhtp_request_t *req,
                                               evbuf_t *buf, void *arg) {
  unsigned int auth_resp_status = evhtp_request_status(req);
  S3AuthClientOpContext *context = (S3AuthClientOpContext *)arg;
  const auto request_id = context->get_request()->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  s3_log(S3_LOG_DEBUG, request_id, "auth_resp_status = %d\n", auth_resp_status);

  // Note: Do not remove this, else you will have s3 crashes as the
  // callbacks are invoked after request/connection is freed.
  evhtp_unset_all_hooks(&context->get_auth_op_ctx()->conn->hooks);
  evhtp_unset_all_hooks(
      &context->get_auth_op_ctx()->authorization_request->hooks);

  if (!context->get_request()->client_connected()) {
    // S3 client has already disconnected, ignore
    s3_log(S3_LOG_DEBUG, context->get_request()->get_request_id(),
           "S3 Client has already disconnected.\n");
    // Calling failed handler to do proper cleanup to avoid leaks
    // i.e cleanup of S3Request and respective action chain.
    context->on_failed_handler()();  // Invoke the handler.
    s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
    return EVHTP_RES_OK;
  }
  size_t buffer_len = evbuffer_get_length(buf);
  char *auth_response_body = (char *)malloc(buffer_len + 1);
  if (auth_response_body == NULL) {
    s3_log(S3_LOG_FATAL, request_id, "malloc() returned NULL\n");
  }
  const auto nread = evbuffer_copyout(buf, auth_response_body, buffer_len);
  if (nread > 0) {
    auth_response_body[nread] = '\0';
  } else {
    auth_response_body[0] = '\0';
  }
  s3_log(S3_LOG_DEBUG, request_id,
         "Response data received from Auth service = %s\n", auth_response_body);
  if (auth_resp_status == S3HttpSuccess200) {
    s3_log(S3_LOG_DEBUG, request_id, "Authorization successful\n");
    context->set_op_status_for(0, S3AsyncOpStatus::success,
                               "Authorization successful");
    context->set_authorization_response(auth_response_body, true);
  } else if (auth_resp_status == S3HttpFailed400) {
    s3_log(S3_LOG_ERROR, request_id, "Authorization failed\n");
    context->set_op_status_for(0, S3AsyncOpStatus::failed,
                               "Authorization failed: BadRequest");
    context->set_authorization_response(auth_response_body, false);
  } else if (auth_resp_status == S3HttpFailed401) {
    s3_log(S3_LOG_ERROR, request_id, "Authorization failed\n");
    context->set_op_status_for(0, S3AsyncOpStatus::failed,
                               "Authorization failed");
    context->set_authorization_response(auth_response_body, false);
  } else if (auth_resp_status == S3HttpFailed405) {
    s3_log(S3_LOG_ERROR, request_id,
           "Authorization failed: Method Not Allowed \n");
    context->set_op_status_for(0, S3AsyncOpStatus::failed,
                               "Authorization failed: Method Not Allowed");
    context->set_authorization_response(auth_response_body, false);
  } else if (auth_resp_status == S3HttpFailed403) {
    s3_log(S3_LOG_ERROR, request_id, "Authorization failed: Access Denied \n");
    context->set_op_status_for(0, S3AsyncOpStatus::failed,
                               "Authorization failed: AccessDenied");
    context->set_authorization_response(auth_response_body, false);
  } else {
    s3_log(S3_LOG_ERROR, request_id, "Something is wrong with Auth server\n");
    context->set_op_status_for(0, S3AsyncOpStatus::failed,
                               "Something is wrong with Auth server");
    context->set_authorization_response("", false);
  }
  free(auth_response_body);
  if (context->get_op_status_for(0) == S3AsyncOpStatus::success) {
    s3_auth_op_success(context, 0);  // Invoke the handler.
  } else {
    context->on_failed_handler()();  // Invoke the handler.
  }
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
  return EVHTP_RES_OK;
}

extern "C" evhtp_res on_aclvalidation_response(evhtp_request_t *req,
                                               evbuf_t *buf, void *arg) {
  unsigned int auth_resp_status = evhtp_request_status(req);

  // S3AuthClientOpContext *context = (S3AuthClientOpContext *)arg;
  S3AuthClientOpContext *context = static_cast<S3AuthClientOpContext *>(arg);

  const auto request_id = context->get_request()->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  if (context->get_request()) {
    s3_log(S3_LOG_DEBUG, request_id, "auth_resp_status = %d\n",
           auth_resp_status);
  }

  // Note: Do not remove this, else you will have s3 crashes as the
  // callbacks are invoked after request/connection is freed.
  evhtp_unset_all_hooks(&context->get_auth_op_ctx()->conn->hooks);
  evhtp_unset_all_hooks(
      &context->get_auth_op_ctx()->aclvalidation_request->hooks);

  if (!context->get_request()->client_connected()) {
    // S3 client has already disconnected, ignore
    s3_log(S3_LOG_DEBUG, request_id, "S3 Client has already disconnected.\n");
    // Calling failed handler to do proper cleanup to avoid leaks
    // i.e cleanup of S3Request and respective action chain.
    context->on_failed_handler()();  // Invoke the handler.
    return EVHTP_RES_OK;
  }
  size_t buffer_len = evbuffer_get_length(buf);
  char *auth_response_body = (char *)malloc(buffer_len + 1);
  if (auth_response_body == NULL) {
    s3_log(S3_LOG_FATAL, request_id, "malloc() returned NULL\n");
  }
  const auto nread = evbuffer_copyout(buf, auth_response_body, buffer_len);
  if (nread > 0) {
    auth_response_body[nread] = '\0';
  } else {
    auth_response_body[0] = '\0';
  }
  s3_log(S3_LOG_DEBUG, request_id,
         "Response data received from Auth service = %s\n", auth_response_body);

  if (auth_resp_status == S3HttpSuccess200) {
    s3_log(S3_LOG_DEBUG, request_id, "AclValidation successful\n");
    context->set_op_status_for(0, S3AsyncOpStatus::success,
                               "Aclvalidation successful");

  } else if (auth_resp_status == S3HttpFailed401) {
    s3_log(S3_LOG_ERROR, request_id, "Aclvalidation failed\n");
    context->set_op_status_for(0, S3AsyncOpStatus::failed,
                               "Aclvalidation failed");
    context->set_aclvalidation_response_xml(auth_response_body, false);

  } else if (auth_resp_status == S3HttpFailed405) {
    s3_log(S3_LOG_ERROR, request_id,
           "Aclvalidation failed: Method Not Allowed \n");
    context->set_op_status_for(0, S3AsyncOpStatus::failed,
                               "AclValidation failed: Method Not Allowed");
    context->set_aclvalidation_response_xml(auth_response_body, false);

  } else {
    s3_log(S3_LOG_ERROR, request_id, "Something is wrong with Auth server\n");
    context->set_op_status_for(0, S3AsyncOpStatus::failed,
                               "Something is wrong with Auth server");
    context->set_aclvalidation_response_xml(auth_response_body, false);
  }

  free(auth_response_body);
  if (context->get_op_status_for(0) == S3AsyncOpStatus::success) {
    s3_auth_op_success(context, 0);  // Invoke the handler.
  } else {
    context->on_failed_handler()();  // Invoke the handler.
  }
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
  return EVHTP_RES_OK;
}

extern "C" evhtp_res on_policy_validation_response(evhtp_request_t *req,
                                                   evbuf_t *buf, void *arg) {
  unsigned int auth_resp_status = evhtp_request_status(req);

  // S3AuthClientOpContext *context = (S3AuthClientOpContext *)arg;
  S3AuthClientOpContext *context = static_cast<S3AuthClientOpContext *>(arg);
  const auto request_id = context->get_request()->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  if (context->get_request()) {
    s3_log(S3_LOG_DEBUG, request_id, "auth_resp_status = %d\n",
           auth_resp_status);
  }

  // Note: Do not remove this, else you will have s3 crashes as the
  // callbacks are invoked after request/connection is freed.
  evhtp_unset_all_hooks(&context->get_auth_op_ctx()->conn->hooks);
  evhtp_unset_all_hooks(
      &context->get_auth_op_ctx()->policyvalidation_request->hooks);

  if (!context->get_request()->client_connected()) {
    // S3 client has already disconnected, ignore
    s3_log(S3_LOG_DEBUG, request_id, "S3 Client has already disconnected.\n");
    // Calling failed handler to do proper cleanup to avoid leaks
    // i.e cleanup of S3Request and respective action chain.
    context->on_failed_handler()();  // Invoke the handler.
    return EVHTP_RES_OK;
  }
  size_t buffer_len = evbuffer_get_length(buf);
  char *auth_response_body = (char *)malloc(buffer_len + 1);
  if (auth_response_body == NULL) {
    s3_log(S3_LOG_FATAL, request_id, "malloc() returned NULL\n");
  }
  const auto nread = evbuffer_copyout(buf, auth_response_body, buffer_len);
  if (nread > 0) {
    auth_response_body[nread] = '\0';
  } else {
    auth_response_body[0] = '\0';
  }
  s3_log(S3_LOG_DEBUG, request_id,
         "Response data received from Auth service = %s\n", auth_response_body);

  if (auth_resp_status == S3HttpSuccess200) {
    s3_log(S3_LOG_DEBUG, request_id, "Policy validation successful\n");
    context->set_op_status_for(0, S3AsyncOpStatus::success,
                               "Policy validation successful");

  } else if (auth_resp_status == S3HttpFailed401) {
    s3_log(S3_LOG_ERROR, request_id, "Policy validation failed\n");
    context->set_op_status_for(0, S3AsyncOpStatus::failed,
                               "Policy validation failed");
    context->set_policyvalidation_response_xml(auth_response_body, false);

  } else if (auth_resp_status == S3HttpFailed405) {
    s3_log(S3_LOG_ERROR, request_id,
           "Policy validation failed: Method Not Allowed \n");
    context->set_op_status_for(0, S3AsyncOpStatus::failed,
                               "Policy validation failed: Method Not Allowed");
    context->set_policyvalidation_response_xml(auth_response_body, false);

  } else {
    s3_log(S3_LOG_ERROR, request_id, "Something is wrong with Auth server\n");
    context->set_op_status_for(0, S3AsyncOpStatus::failed,
                               "Something is wrong with Auth server");
    context->set_policyvalidation_response_xml(auth_response_body, false);
  }
  free(auth_response_body);
  if (context->get_op_status_for(0) == S3AsyncOpStatus::success) {
    s3_auth_op_success(context, 0);  // Invoke the handler.
  } else {
    context->on_failed_handler()();  // Invoke the handler.
  }
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
  return EVHTP_RES_OK;
}

extern "C" evhtp_res on_auth_response(evhtp_request_t *req, evbuf_t *buf,
                                      void *arg) {
  unsigned int auth_resp_status;
  S3AuthClientOpContext *context = (S3AuthClientOpContext *)arg;
  const auto request_id = context->get_request()->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  if (s3_fi_is_enabled("fake_authentication_fail")) {
    auth_resp_status = S3HttpFailed401;
  } else {
    auth_resp_status = evhtp_request_status(req);
  }

  s3_log(S3_LOG_DEBUG, request_id, "auth_resp_status = %d\n", auth_resp_status);

  // Note: Do not remove this, else you will have s3 crashes as the
  // callbacks are invoked after request/connection is freed.
  auto *p_auth_op_ctx = context->get_auth_op_ctx();

  if (p_auth_op_ctx) {
    if (p_auth_op_ctx->conn) {
      evhtp_unset_all_hooks(&p_auth_op_ctx->conn->hooks);
    }
    if (p_auth_op_ctx->authrequest) {
      evhtp_unset_all_hooks(&p_auth_op_ctx->authrequest->hooks);
    }
  } else {
    s3_log(S3_LOG_WARN, request_id,
           "S3AuthClientOpContext::get_auth_op_ctx() returns NULL");
  }
  if (!context->get_request()->client_connected()) {
    // S3 client has already disconnected, ignore
    s3_log(S3_LOG_DEBUG, request_id, "S3 Client has already disconnected.\n");
    // Calling failed handler to do proper cleanup to avoid leaks
    // i.e cleanup of S3Request and respective action chain.
    context->on_failed_handler()();  // Invoke the handler.
    return EVHTP_RES_OK;
  }
  const auto buffer_len = evbuffer_get_length(buf);
  char *auth_response_body = (char *)malloc(buffer_len + 1);

  if (auth_response_body) {
    const auto nread = evbuffer_copyout(buf, auth_response_body, buffer_len);

    if (nread > 0) {
      auth_response_body[nread] = '\0';
    } else {
      auth_response_body[0] = '\0';
    }
    s3_log(S3_LOG_DEBUG, request_id,
           "Response data received from Auth service = [[%s]]\n\n\n",
           auth_response_body);
    if (auth_resp_status == S3HttpSuccess200) {
      s3_log(S3_LOG_DEBUG, request_id, "Authentication successful\n");
      context->set_op_status_for(0, S3AsyncOpStatus::success,
                                 "Authentication successful");
      context->set_auth_response_xml(auth_response_body, true);
    } else if (auth_resp_status == S3HttpFailed401) {
      s3_log(S3_LOG_ERROR, request_id, "Authentication failed\n");
      context->set_op_status_for(0, S3AsyncOpStatus::failed,
                                 "Authentication failed");
      context->set_auth_response_xml(auth_response_body, false);
    } else if (auth_resp_status == S3HttpFailed400) {
      s3_log(S3_LOG_ERROR, request_id, "Authentication failed\n");
      context->set_op_status_for(0, S3AsyncOpStatus::failed,
                                 "Authentication failed:Bad Request");
      context->set_auth_response_xml(auth_response_body, false);
    } else if (auth_resp_status == S3HttpFailed403) {
      s3_log(S3_LOG_ERROR, request_id, "Authentication failed\n");
      context->set_op_status_for(0, S3AsyncOpStatus::failed,
                                 "Authentication failed:Access denied");
      context->set_auth_response_xml(auth_response_body, false);
    } else {
      s3_log(S3_LOG_ERROR, request_id, "Something is wrong with Auth server\n");
      context->set_op_status_for(0, S3AsyncOpStatus::failed,
                                 "Something is wrong with Auth server");
      context->set_auth_response_xml("", false);
    }
    free(auth_response_body);
  } else {
    s3_log(S3_LOG_FATAL, request_id, "malloc() returned NULL\n");
  }
  if (context->get_op_status_for(0) == S3AsyncOpStatus::success) {
    s3_auth_op_success(context, 0);  // Invoke the handler.
  } else {
    context->on_failed_handler()();  // Invoke the handler.
  }
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
  return EVHTP_RES_OK;
}

extern "C" void timeout_cb_auth_retry(evutil_socket_t fd, short event,
                                      void *arg) {
  struct event_auth_timeout_arg *timeout_arg =
      (struct event_auth_timeout_arg *)arg;
  S3AuthClient *auth_client = (S3AuthClient *)(timeout_arg->auth_client);
  const auto request_id = auth_client->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  s3_log(S3_LOG_DEBUG, request_id, "Retring to connect to Auth server\n");
  struct event *timeout_event = (struct event *)(timeout_arg->event);
  S3AuthClientOpType auth_request_type = auth_client->get_op_type();
  if (auth_request_type == S3AuthClientOpType::authorization) {
    auth_client->check_authorization();
  } else {
    auth_client->check_authentication();
  }
  event_free(timeout_event);
  free(timeout_arg);
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

/******/

void S3AuthClientOpContext::set_auth_response_error(std::string error_code,
                                                    std::string error_message,
                                                    std::string request_id) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  error_obj.reset(new S3AuthResponseError(
      std::move(error_code), std::move(error_message), std::move(request_id)));
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

S3AuthClient::S3AuthClient(std::shared_ptr<RequestObject> req,
                           bool skip_authorization)
    : request(req),
      state(S3AuthClientOpState::init),
      req_body_buffer(NULL),
      is_chunked_auth(false),
      last_chunk_added(false),
      chunk_auth_aborted(false),
      skip_authorization(skip_authorization) {
  request_id = request->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  retry_count = 0;
  op_type = S3AuthClientOpType::authentication;
}

void S3AuthClient::add_key_val_to_body(std::string key, std::string val) {
  data_key_val[key] = val;
}

void S3AuthClient::set_event_with_retry_interval() {
  S3Option *option_instance = S3Option::get_instance();
  short interval = option_instance->get_retry_interval_in_millisec();
  interval = interval * retry_count;
  struct timeval s_tv;
  s_tv.tv_sec = interval / 1000;
  s_tv.tv_usec = (interval % 1000) * 1000;
  struct event *ev;
  // Will be freed in callback
  struct event_auth_timeout_arg *arg = (struct event_auth_timeout_arg *)calloc(
      1, sizeof(struct event_auth_timeout_arg));
  arg->auth_client = this;
  ev = event_new(option_instance->get_eventbase(), -1, 0, timeout_cb_auth_retry,
                 (void *)arg);

  arg->event = ev;  // Will be freed in callback
  event_add(ev, &s_tv);
}

std::string S3AuthClient::get_signature_from_response() {
  if (auth_context->auth_successful()) {
    return auth_context->get_signature_sha256();
  }
  return "";
}

void S3AuthClient::remember_auth_details_in_request() {
  if (auth_context->auth_successful()) {
    request->set_user_id(auth_context->get_user_id());
    request->set_user_name(auth_context->get_user_name());
    request->set_canonical_id(auth_context->get_canonical_id());
    request->set_account_id(auth_context->get_account_id());
    request->set_account_name(auth_context->get_account_name());
    request->set_email(auth_context->get_email());
  }
}

std::string S3AuthClient::get_error_message() {
  if ((((!auth_context->auth_successful()) &&
        get_op_type() == S3AuthClientOpType::authentication)) ||
      ((!auth_context->authorization_successful()) &&
       get_op_type() == S3AuthClientOpType::authorization) ||
      ((!auth_context->aclvalidation_successful() &&
        get_op_type() == S3AuthClientOpType::aclvalidation)) ||
      ((!auth_context->policyvalidation_successful() &&
        get_op_type() == S3AuthClientOpType::policyvalidation))) {

    std::string message = auth_context->get_error_message();
    return message;
  }
  return "";
}

// Returns AccessDenied | InvalidAccessKeyId | SignatureDoesNotMatch
// auth InactiveAccessKey maps to InvalidAccessKeyId in S3
std::string S3AuthClient::get_error_code() {

  if ((((!auth_context->auth_successful()) &&
        get_op_type() == S3AuthClientOpType::authentication)) ||
      ((!auth_context->authorization_successful()) &&
       get_op_type() == S3AuthClientOpType::authorization) ||
      ((!auth_context->aclvalidation_successful() &&
        get_op_type() == S3AuthClientOpType::aclvalidation)) ||
      ((!auth_context->policyvalidation_successful() &&
        get_op_type() == S3AuthClientOpType::policyvalidation))) {
    std::string code = auth_context->get_error_code();

    if (code == "InactiveAccessKey") {
      return "InvalidAccessKeyId";
    } else if (code == "ExpiredCredential") {
      return "ExpiredToken";
    } else if (code == "InvalidClientTokenId") {
      return "InvalidToken";
    } else if (code == "TokenRefreshRequired") {
      return "AccessDenied";
    } else if (code == "AccessDenied") {
      return "AccessDenied";
    } else if (code == "MethodNotAllowed") {
      return "MethodNotAllowed";
    } else if (!code.empty()) {
      return code;  // InvalidAccessKeyId | SignatureDoesNotMatch
    }
  }
  return "ServiceUnavailable";  // auth server may be down
}

bool S3AuthClient::setup_auth_request_body() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  S3AuthClientOpType auth_request_type = get_op_type();
  std::string auth_request_body;
  std::string method = "";
  if (request->http_verb() == S3HttpVerb::GET) {
    method = "GET";
  } else if (request->http_verb() == S3HttpVerb::HEAD) {
    method = "HEAD";
  } else if (request->http_verb() == S3HttpVerb::PUT) {
    method = "PUT";
  } else if (request->http_verb() == S3HttpVerb::DELETE) {
    method = "DELETE";
  } else if (request->http_verb() == S3HttpVerb::POST) {
    method = "POST";
  }
  add_key_val_to_body("Method", method);
  add_key_val_to_body("RequestorAccountId", request->get_account_id());
  add_key_val_to_body("RequestorAccountName", request->get_account_name());
  add_key_val_to_body("RequestorUserId", request->get_user_id());
  add_key_val_to_body("RequestorUserName", request->get_user_name());
  add_key_val_to_body("RequestorEmail", request->get_email());
  add_key_val_to_body("RequestorCanonicalId", request->get_canonical_id());

  const char *full_path = request->c_get_full_encoded_path();
  if (full_path != NULL) {
    std::string uri_full_path = full_path;
    // in encoded uri path space is encoding as '+'
    // but cannonical request should have '%20' for verification.
    // decode plus into space, this special handling
    // is not required for other charcters.
    S3CommonUtilities::find_and_replaceall(uri_full_path, "+", "%20");
    add_key_val_to_body("ClientAbsoluteUri", uri_full_path);
  } else {
    add_key_val_to_body("ClientAbsoluteUri", "");
  }

  // get the query paramters in a map
  // eg: query_map = { {prefix, abc&def}, {delimiter, /}};
  const std::map<std::string, std::string, compare> query_map =
      request->get_query_parameters();
  std::string query;
  // iterate through the each query parameter
  // and url-encode the values (abc%26def)
  // then form the query (prefix=abc%26def&delimiter=%2F)
  bool f_evhttp_encode_uri_succ = true;

  for (auto it : query_map) {
    if (it.second == "") {
      query += query.empty() ? it.first : "&" + it.first;
    } else {

      char *encoded_value = evhttp_encode_uri(it.second.c_str());
      if (!encoded_value) {
        s3_log(S3_LOG_ERROR, request_id, "Failed to URI encode value = %s",
               it.second.c_str());
        f_evhttp_encode_uri_succ = false;
        break;
      }

      std::string encoded_value_str = encoded_value;
      query += query.empty() ? it.first + "=" + encoded_value_str
                             : "&" + it.first + "=" + encoded_value_str;
      free(encoded_value);
    }
  }

  if (!f_evhttp_encode_uri_succ) {
    s3_log(S3_LOG_ERROR, request_id,
           "evhttp_encode_uri() returned NULL for client query params");
    auth_request_body.clear();
    return false;
  }

  add_key_val_to_body("ClientQueryParams", query);

  // May need to take it from config
  add_key_val_to_body("Version", "2010-05-08");
  add_key_val_to_body("Request_id", request_id);
  if (auth_request_type == S3AuthClientOpType::authorization) {

    std::shared_ptr<S3RequestObject> s3_request =
        std::dynamic_pointer_cast<S3RequestObject>(request);
    if (s3_request != nullptr) {

      // Set flag to request default bucket acl from authserver.
      if ((s3_request->http_verb() == S3HttpVerb::PUT &&
           s3_request->get_operation_code() == S3OperationCode::none &&
           s3_request->get_api_type() == S3ApiType::bucket)) {

        add_key_val_to_body("Request-ACL", "true");
      }

      // Set flag to request default object acl for api put-object, put object
      // in complete multipart upload and put object in chunked mode
      // from authserver.
      else if (((s3_request->http_verb() == S3HttpVerb::PUT &&
                 s3_request->get_operation_code() == S3OperationCode::none) ||
                (s3_request->http_verb() == S3HttpVerb::POST &&
                 s3_request->get_operation_code() ==
                     S3OperationCode::multipart)) &&
               s3_request->get_api_type() == S3ApiType::object) {
        add_key_val_to_body("Request-ACL", "true");
      } else if ((s3_request->http_verb() == S3HttpVerb::PUT &&
                  s3_request->get_operation_code() == S3OperationCode::acl) &&
                 ((s3_request->get_api_type() == S3ApiType::object) ||
                  (s3_request->get_api_type() == S3ApiType::bucket))) {
        add_key_val_to_body("Request-ACL", "true");
      }
      // PUT Object ACL case
      else if (s3_request->http_verb() == S3HttpVerb::PUT &&
               s3_request->get_operation_code() == S3OperationCode::acl) {
        add_key_val_to_body("Request-ACL", "true");
      }
    }
    if (policy_str != "") {
      add_key_val_to_body("Policy", policy_str);
    }
    if (acl_str != "") {

      add_key_val_to_body("Auth-ACL", acl_str);
    }
    if (bucket_acl != "") {
      add_key_val_to_body("Bucket-ACL", bucket_acl);
    }
    if (s3_request->get_action_str() != "") {
      add_key_val_to_body("S3Action", s3_request->get_action_str());
    }

    auth_request_body = "Action=AuthorizeUser";
  } else if (auth_request_type ==
             S3AuthClientOpType::authentication) {  // Auth request

    if (is_chunked_auth &&
        !(prev_chunk_signature_from_auth.empty() ||
          current_chunk_signature_from_auth.empty())) {
      add_key_val_to_body("previous-signature-sha256",
                          prev_chunk_signature_from_auth);
      add_key_val_to_body("current-signature-sha256",
                          current_chunk_signature_from_auth);
      add_key_val_to_body("x-amz-content-sha256", hash_sha256_current_chunk);
    }

    auth_request_body = "Action=AuthenticateUser";
  } else if (auth_request_type == S3AuthClientOpType::aclvalidation) {

    add_key_val_to_body("ACL", user_acl);
    add_key_val_to_body("Auth-ACL", acl_str);
    auth_request_body = "Action=ValidateACL";
  } else if (auth_request_type == S3AuthClientOpType::policyvalidation) {
    add_key_val_to_body("Policy", policy_str);
    auth_request_body = "Action=ValidatePolicy";
  }

  for (auto &it : data_key_val) {

    char *encoded_key = evhttp_encode_uri(it.first.c_str());
    if (!encoded_key) {
      f_evhttp_encode_uri_succ = false;
      break;
    }
    auth_request_body += '&';
    auth_request_body += encoded_key;
    free(encoded_key);

    char *encoded_value = evhttp_encode_uri(it.second.c_str());
    if (!encoded_value) {
      f_evhttp_encode_uri_succ = false;
      break;
    }
    auth_request_body += '=';
    auth_request_body += encoded_value;
    free(encoded_value);
  }
  if (!f_evhttp_encode_uri_succ) {
    s3_log(S3_LOG_ERROR, request_id, "evhttp_encode_uri() returned NULL");
    auth_request_body.clear();
  }
  // evbuffer_add_printf(op_ctx->authrequest->buffer_out,
  // auth_request_body.c_str());
  req_body_buffer = evbuffer_new();
  evbuffer_add(req_body_buffer, auth_request_body.c_str(),
               auth_request_body.length());

  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");

  return !auth_request_body.empty();
}

void S3AuthClient::set_acl_and_policy(const std::string &acl,
                                      const std::string &policy) {
  policy_str = policy;
  acl_str = acl;
}

void S3AuthClient::set_bucket_acl(const std::string &acl) { bucket_acl = acl; }

void S3AuthClient::set_validate_acl(const std::string &validateacl) {
  user_acl = validateacl;
}

void S3AuthClient::setup_auth_request_headers() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  evhtp_request_t *auth_request = NULL;
  S3AuthClientOpType auth_request_type = get_op_type();
  struct s3_auth_op_context *op_ctx = auth_context->get_auth_op_ctx();
  char sz_size[100] = {0};

  size_t out_len = evbuffer_get_length(req_body_buffer);

  if (auth_request_type == S3AuthClientOpType::authorization) {
    auth_request = op_ctx->authorization_request;
  } else if (auth_request_type == S3AuthClientOpType::authentication) {
    auth_request = op_ctx->authrequest;
  } else if (auth_request_type == S3AuthClientOpType::aclvalidation) {
    auth_request = op_ctx->aclvalidation_request;
  } else if (auth_request_type == S3AuthClientOpType::policyvalidation) {
    auth_request = op_ctx->policyvalidation_request;
  }

  sprintf(sz_size, "%zu", out_len);
  s3_log(S3_LOG_DEBUG, request_id, "Header - Length = %zu\n", out_len);
  s3_log(S3_LOG_DEBUG, request_id, "Header - Length-str = %s\n", sz_size);
  std::string host = request->get_host_header();

  if (!host.empty()) {
    evhtp_headers_add_header(auth_request->headers_out,
                             evhtp_header_new("Host", host.c_str(), 1, 1));
  }
  evhtp_headers_add_header(auth_request->headers_out,
                           evhtp_header_new("Content-Length", sz_size, 1, 1));
  evhtp_headers_add_header(
      auth_request->headers_out,
      evhtp_header_new("Content-Type", "application/x-www-form-urlencoded", 0,
                       0));

  evhtp_headers_add_header(auth_request->headers_out,
                           evhtp_header_new("User-Agent", "s3server", 1, 1));
  /* Using a new connection per request, all types of requests like
  *   authenticarion, authorization, validateacl, validatepolicy open
  *   a connection and it will be closed by Authserver.
  *   TODO : Evaluate performance and change this approach to reuse
  *   connections
  */

  evhtp_headers_add_header(auth_request->headers_out,
                           evhtp_header_new("Connection", "close", 1, 1));

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AuthClient::set_is_authheader_present(
    bool is_authorizationheader_present) {
  is_authheader_present = is_authorizationheader_present;
}

void S3AuthClient::execute_authconnect_request(
    struct s3_auth_op_context *auth_ctx) {
  S3AuthClientOpType auth_request_type = get_op_type();

  if (auth_request_type == S3AuthClientOpType::authorization) {

    if (s3_fi_is_enabled("fake_authorization_fail")) {
      s3_auth_fake_evhtp_request(S3AuthClientOpType::authorization,
                                 auth_context.get());
    } else {
      evhtp_make_request(auth_ctx->conn, auth_ctx->authorization_request,
                         htp_method_POST, "/");
      evhtp_send_reply_body(auth_ctx->authorization_request, req_body_buffer);
    }
  } else if (auth_request_type == S3AuthClientOpType::authentication) {
    if (s3_fi_is_enabled("fake_authentication_fail")) {
      s3_auth_fake_evhtp_request(S3AuthClientOpType::authentication,
                                 auth_context.get());
    } else {

      evhtp_make_request(auth_ctx->conn, auth_ctx->authrequest, htp_method_POST,
                         "/");
      evhtp_send_reply_body(auth_ctx->authrequest, req_body_buffer);
    }

  } else if (auth_request_type == S3AuthClientOpType::aclvalidation) {
    if (auth_ctx->authorization_request != NULL) {
      evhtp_request_free(auth_ctx->authorization_request);
      auth_ctx->authorization_request = NULL;
    }
    evhtp_make_request(auth_ctx->conn, auth_ctx->aclvalidation_request,
                       htp_method_POST, "/");
    evhtp_send_reply_body(auth_ctx->aclvalidation_request, req_body_buffer);
  } else if (auth_request_type == S3AuthClientOpType::policyvalidation) {
    if (auth_ctx->authorization_request != NULL) {
      evhtp_request_free(auth_ctx->authorization_request);
      auth_ctx->authorization_request = NULL;
    }
    evhtp_make_request(auth_ctx->conn, auth_ctx->policyvalidation_request,
                       htp_method_POST, "/");
    evhtp_send_reply_body(auth_ctx->policyvalidation_request, req_body_buffer);
  }
  evbuffer_free(req_body_buffer);
  req_body_buffer = NULL;
}

void S3AuthClient::trigger_authentication() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  AtExit at_exit_on_error([this]() {
    if (auth_context) {
      auth_context->set_auth_response_error(
          "InternalError", "Error while preparing request to Auth server",
          request_id);
    }
    if (handler_on_failed) {
      handler_on_failed();
    }
  });

  state = S3AuthClientOpState::started;
  S3AuthClientOpType auth_request_type = get_op_type();
  auth_context->init_auth_op_ctx(auth_request_type);
  struct s3_auth_op_context *auth_ctx = auth_context->get_auth_op_ctx();
  // Setup the auth callbacks
  evhtp_set_hook(&auth_ctx->authrequest->hooks, evhtp_hook_on_read,
                 (evhtp_hook)on_auth_response, auth_context.get());
  evhtp_set_hook(&auth_ctx->conn->hooks, evhtp_hook_on_conn_error,
                 (evhtp_hook)on_auth_conn_err_callback, auth_context.get());

  // Setup the headers to forward to auth service
  s3_log(S3_LOG_DEBUG, request_id, "Headers from S3 client:\n");
  for (auto it : request->get_in_headers_copy()) {
    s3_log(S3_LOG_DEBUG, request_id, "Header = %s, Value = %s\n",
           it.first.c_str(), it.second.c_str());
    add_key_val_to_body(it.first.c_str(), it.second.c_str());
  }

  // Setup the body to be sent to auth service
  if (!setup_auth_request_body()) {
    if (req_body_buffer != NULL) {
      evbuffer_free(req_body_buffer);
      req_body_buffer = NULL;
    }
    return;
  }
  setup_auth_request_headers();
  S3Option *option_instance = S3Option::get_instance();
  if (option_instance->get_log_level() == "DEBUG") {
    size_t buffer_len = evbuffer_get_length(req_body_buffer);
    char *auth_response_body = (char *)malloc(buffer_len + 1);
    if (auth_response_body == NULL) {
      s3_log(S3_LOG_FATAL, request_id, "malloc() returned NULL\n");
    }
    const auto nread =
        evbuffer_copyout(req_body_buffer, auth_response_body, buffer_len);
    if (nread > 0) {
      auth_response_body[nread] = '\0';
    } else {
      auth_response_body[0] = '\0';
    }
    s3_log(S3_LOG_DEBUG, request_id, "Data being send to Auth server: = %s\n",
           auth_response_body);
    free(auth_response_body);
  }

  execute_authconnect_request(auth_ctx);

  at_exit_on_error.cancel();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

// Note: S3AuthClientOpContext should be created before calling this.
// This is called by check_authentication and start_chunk_auth
void S3AuthClient::trigger_authentication(std::function<void(void)> on_success,
                                          std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");

  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  trigger_authentication();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AuthClient::validate_acl(std::function<void(void)> on_success,
                                std::function<void(void)> on_failed) {

  data_key_val.clear();
  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  auth_context.reset(new S3AuthClientOpContext(
      request, std::bind(&S3AuthClient::check_aclvalidation_successful, this),
      std::bind(&S3AuthClient::check_aclvalidation_failed, this)));

  S3AuthClientOpType auth_request_type = S3AuthClientOpType::aclvalidation;
  set_op_type(auth_request_type);
  auth_context->init_auth_op_ctx(auth_request_type);

  struct s3_auth_op_context *auth_ctx = auth_context->get_auth_op_ctx();
  evhtp_set_hook(&auth_ctx->aclvalidation_request->hooks, evhtp_hook_on_read,
                 (evhtp_hook)on_aclvalidation_response, auth_context.get());
  evhtp_set_hook(&auth_ctx->conn->hooks, evhtp_hook_on_conn_error,
                 (evhtp_hook)on_conn_err_callback, auth_context.get());

  // Setup the Aclvalidation body to be sent to auth service

  for (auto it : request->get_in_headers_copy()) {
    s3_log(S3_LOG_DEBUG, request_id, "Header = %s, Value = %s\n",
           it.first.c_str(), it.second.c_str());
    add_key_val_to_body(it.first.c_str(), it.second.c_str());
  }

  setup_auth_request_body();

  setup_auth_request_headers();
  S3Option *option_instance = S3Option::get_instance();
  if (option_instance->get_log_level() == "DEBUG") {
    size_t buffer_len = evbuffer_get_length(req_body_buffer);
    char *auth_body = (char *)malloc(buffer_len + 1);
    if (auth_body == NULL) {
      s3_log(S3_LOG_FATAL, request_id, "malloc() returned NULL\n");
    }
    const auto nread = evbuffer_copyout(req_body_buffer, auth_body, buffer_len);
    if (nread > 0) {
      auth_body[nread] = '\0';
    } else {
      auth_body[0] = '\0';
    }
    s3_log(S3_LOG_DEBUG, request_id,
           "Aclvalidation Data being send to Auth server: %s\n", auth_body);
    free(auth_body);
  }
  execute_authconnect_request(auth_ctx);

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AuthClient::validate_policy(std::function<void(void)> on_success,
                                   std::function<void(void)> on_failed) {
  data_key_val.clear();
  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  auth_context.reset(new S3AuthClientOpContext(
      request, std::bind(&S3AuthClient::policy_validation_successful, this),
      std::bind(&S3AuthClient::policy_validation_failed, this)));

  S3AuthClientOpType auth_request_type = S3AuthClientOpType::policyvalidation;
  set_op_type(auth_request_type);
  auth_context->init_auth_op_ctx(auth_request_type);

  struct s3_auth_op_context *auth_ctx = auth_context->get_auth_op_ctx();
  evhtp_set_hook(&auth_ctx->policyvalidation_request->hooks, evhtp_hook_on_read,
                 (evhtp_hook)on_policy_validation_response, auth_context.get());
  evhtp_set_hook(&auth_ctx->conn->hooks, evhtp_hook_on_conn_error,
                 (evhtp_hook)on_conn_err_callback, auth_context.get());

  // Setup the Policy validation body to be sent to auth service

  setup_auth_request_body();

  setup_auth_request_headers();
  S3Option *option_instance = S3Option::get_instance();
  if (option_instance->get_log_level() == "DEBUG") {
    size_t buffer_len = evbuffer_get_length(req_body_buffer);
    char *auth_body = (char *)malloc(buffer_len + 1);
    if (auth_body == NULL) {
      s3_log(S3_LOG_FATAL, request_id, "malloc() returned NULL\n");
    }
    const auto nread = evbuffer_copyout(req_body_buffer, auth_body, buffer_len);
    if (nread > 0) {
      auth_body[nread] = '\0';
    } else {
      auth_body[0] = '\0';
    }
    s3_log(S3_LOG_DEBUG, request_id,
           "Policy validation Data being send to Auth server: %s\n", auth_body);
    free(auth_body);
  }
  execute_authconnect_request(auth_ctx);

  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

void S3AuthClient::policy_validation_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  state = S3AuthClientOpState::authenticated;
  remember_auth_details_in_request();
  // prev_chunk_signature_from_auth = get_signature_from_response();
  this->handler_on_success();
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

void S3AuthClient::policy_validation_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  S3AsyncOpStatus op_state = auth_context.get()->get_op_status_for(0);
  if (op_state == S3AsyncOpStatus::connection_failed) {
    S3Option *option_instance = S3Option::get_instance();
    s3_log(S3_LOG_WARN, request_id,
           "Policy validation failure, not able to connect to Auth server\n");
    if (retry_count < option_instance->get_max_retry_count()) {
      retry_count++;
      set_event_with_retry_interval();
    } else {
      s3_log(S3_LOG_ERROR, request_id,
             "Cannot connect to Auth server (Retry count = %d).\n",
             retry_count);
      s3_iem(LOG_ALERT, S3_IEM_AUTH_CONN_FAIL, S3_IEM_AUTH_CONN_FAIL_STR,
             S3_IEM_AUTH_CONN_FAIL_JSON);
      this->handler_on_failed();
    }
  } else {
    s3_log(S3_LOG_ERROR, request_id, "Policy validation failure\n");

    this->handler_on_failed();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AuthClient::check_authorization() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  set_op_type(S3AuthClientOpType::authorization);
  state = S3AuthClientOpState::started;

  // New connection in case of retry
  S3AuthClientOpType auth_request_type = get_op_type();
  auth_context->init_auth_op_ctx(auth_request_type);
  struct s3_auth_op_context *auth_ctx = auth_context->get_auth_op_ctx();

  // Setup the auth callbacks
  evhtp_set_hook(&auth_ctx->authorization_request->hooks, evhtp_hook_on_read,
                 (evhtp_hook)on_authorization_response, auth_context.get());
  evhtp_set_hook(&auth_ctx->conn->hooks, evhtp_hook_on_conn_error,
                 (evhtp_hook)on_auth_conn_err_callback, auth_context.get());

  // Setup the Authorization body to be sent to auth service
  setup_auth_request_body();
  setup_auth_request_headers();
  S3Option *option_instance = S3Option::get_instance();
  if (option_instance->get_log_level() == "DEBUG") {
    size_t buffer_len = evbuffer_get_length(req_body_buffer);
    char *auth_body = (char *)malloc(buffer_len + 1);
    if (auth_body == NULL) {
      s3_log(S3_LOG_FATAL, request_id, "malloc() returned NULL\n");
    }
    const auto nread = evbuffer_copyout(req_body_buffer, auth_body, buffer_len);
    if (nread > 0) {
      auth_body[nread] = '\0';
    } else {
      auth_body[0] = '\0';
    }
    s3_log(S3_LOG_DEBUG, request_id,
           "Authorization Data being send to Auth server: %s\n", auth_body);
    free(auth_body);
  }

  execute_authconnect_request(auth_ctx);

  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

void S3AuthClient::check_authorization(std::function<void(void)> on_success,
                                       std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  set_op_type(S3AuthClientOpType::authorization);

  if (!is_authheader_present) {
    auth_context.reset(new S3AuthClientOpContext(
        request, std::bind(&S3AuthClient::check_authorization_successful, this),
        std::bind(&S3AuthClient::check_authorization_failed, this)));

  for (auto it : request->get_in_headers_copy()) {
    s3_log(S3_LOG_DEBUG, request_id, "Header = %s, Value = %s\n",
           it.first.c_str(), it.second.c_str());
    add_key_val_to_body(it.first.c_str(), it.second.c_str());
  }
  } else {
    auth_context->reset_callbacks(
        std::bind(&S3AuthClient::check_authorization_successful, this),
        std::bind(&S3AuthClient::check_authorization_failed, this));
  }

  state = S3AuthClientOpState::started;

  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;
  S3AuthClientOpType auth_request_type = get_op_type();

  auth_context->init_auth_op_ctx(auth_request_type);

  struct s3_auth_op_context *auth_ctx = auth_context->get_auth_op_ctx();
  // Setup the auth callbacks
  evhtp_set_hook(&auth_ctx->authorization_request->hooks, evhtp_hook_on_read,
                 (evhtp_hook)on_authorization_response, auth_context.get());
  evhtp_set_hook(&auth_ctx->conn->hooks, evhtp_hook_on_conn_error,
                 (evhtp_hook)on_auth_conn_err_callback, auth_context.get());

  // Setup the Authorization body to be sent to auth service
  setup_auth_request_body();
  setup_auth_request_headers();
  S3Option *option_instance = S3Option::get_instance();
  if (option_instance->get_log_level() == "DEBUG") {
    size_t buffer_len = evbuffer_get_length(req_body_buffer);
    char *auth_body = (char *)malloc(buffer_len + 1);
    if (auth_body == NULL) {
      s3_log(S3_LOG_FATAL, request_id, "malloc failed to allocate memory\n");
    }
    const auto nread = evbuffer_copyout(req_body_buffer, auth_body, buffer_len);
    if (nread > 0) {
      auth_body[nread] = '\0';
    } else {
      auth_body[0] = '\0';
    }
    s3_log(S3_LOG_DEBUG, request_id,
           "Authorization Data being send to Auth server: %s\n", auth_body);
    free(auth_body);
  }
  execute_authconnect_request(auth_ctx);

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AuthClient::check_authorization_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  state = S3AuthClientOpState::authorized;
  retry_count = 0;
  this->handler_on_success();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

/*
 *  <IEM_INLINE_DOCUMENTATION>
 *    <event_code>047001001</event_code>
 *    <application>S3 Server</application>
 *    <submodule>Auth</submodule>
 *    <description>Auth server connection failed</description>
 *    <audience>Service</audience>
 *    <details>
 *      Not able to connect to the Auth server.
 *      The data section of the event has following keys:
 *        time - timestamp.
 *        node - node name.
 *        pid  - process-id of s3server instance, useful to identify logfile.
 *        file - source code filename.
 *        line - line number within file where error occurred.
 *    </details>
 *    <service_actions>
 *      Save the S3 server log files. Restart S3 Auth server and check the
 *      auth server log files for any startup issues. If problem persists,
 *      contact development team for further investigation.
 *    </service_actions>
 *  </IEM_INLINE_DOCUMENTATION>
 */

void S3AuthClient::check_authorization_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  s3_log(S3_LOG_ERROR, request_id, "Authorization failure\n");
  S3AsyncOpStatus op_state = auth_context.get()->get_op_status_for(0);
  if (op_state == S3AsyncOpStatus::connection_failed) {
    S3Option *option_instance = S3Option::get_instance();
    s3_log(S3_LOG_WARN, request_id,
           "Authorization failure, not able to connect to Auth server\n");
    if (retry_count < option_instance->get_max_retry_count()) {
      retry_count++;
      set_event_with_retry_interval();
    } else {
      s3_log(S3_LOG_ERROR, request_id,
             "Cannot connect to Auth server (Retry count = %d).\n",
             retry_count);
      s3_iem(LOG_ALERT, S3_IEM_AUTH_CONN_FAIL, S3_IEM_AUTH_CONN_FAIL_STR,
             S3_IEM_AUTH_CONN_FAIL_JSON);
      this->handler_on_failed();
    }
  } else {
    s3_log(S3_LOG_ERROR, request_id, "Authorization failure\n");
    state = S3AuthClientOpState::unauthorized;
    this->handler_on_failed();
  }

  // this->handler_on_failed();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AuthClient::check_authentication() {
  auth_context.reset(new S3AuthClientOpContext(
      request, std::bind(&S3AuthClient::check_authentication_successful, this),
      std::bind(&S3AuthClient::check_authentication_failed, this)));
  is_chunked_auth = false;
  set_op_type(S3AuthClientOpType::authentication);
  trigger_authentication();
}

void S3AuthClient::check_authentication(std::function<void(void)> on_success,
                                        std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  auth_context.reset(new S3AuthClientOpContext(
      request, std::bind(&S3AuthClient::check_authentication_successful, this),
      std::bind(&S3AuthClient::check_authentication_failed, this)));

  is_chunked_auth = false;
  set_op_type(S3AuthClientOpType::authentication);

  trigger_authentication(on_success, on_failed);
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

void S3AuthClient::check_authentication_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  state = S3AuthClientOpState::authenticated;
  retry_count = 0;
  remember_auth_details_in_request();
  prev_chunk_signature_from_auth = get_signature_from_response();
  this->handler_on_success();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AuthClient::check_aclvalidation_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  state = S3AuthClientOpState::authenticated;
  retry_count = 0;
  remember_auth_details_in_request();
  prev_chunk_signature_from_auth = get_signature_from_response();
  this->handler_on_success();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AuthClient::check_authentication_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  S3AsyncOpStatus op_state = auth_context.get()->get_op_status_for(0);
  if (op_state == S3AsyncOpStatus::connection_failed) {
    S3Option *option_instance = S3Option::get_instance();
    s3_log(S3_LOG_WARN, request_id,
           "Authentication failure, not able to connect to Auth server\n");
    if (retry_count < option_instance->get_max_retry_count()) {
      retry_count++;
      set_event_with_retry_interval();
    } else {
      s3_log(S3_LOG_ERROR, request_id,
             "Cannot connect to Auth server (Retry count = %d).\n",
             retry_count);
      s3_iem(LOG_ALERT, S3_IEM_AUTH_CONN_FAIL, S3_IEM_AUTH_CONN_FAIL_STR,
             S3_IEM_AUTH_CONN_FAIL_JSON);
      this->handler_on_failed();
    }
  } else {
    s3_log(S3_LOG_ERROR, request_id, "Authentication failure\n");
    state = S3AuthClientOpState::unauthenticated;
    this->handler_on_failed();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AuthClient::check_aclvalidation_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  S3AsyncOpStatus op_state = auth_context.get()->get_op_status_for(0);
  if (op_state == S3AsyncOpStatus::connection_failed) {
    S3Option *option_instance = S3Option::get_instance();
    s3_log(S3_LOG_WARN, request_id,
           "Aclvalidation failure, not able to connect to Auth server\n");
    if (retry_count < option_instance->get_max_retry_count()) {
      retry_count++;
      set_event_with_retry_interval();
    } else {
      s3_log(S3_LOG_ERROR, request_id,
             "Cannot connect to Auth server (Retry count = %d).\n",
             retry_count);
      s3_iem(LOG_ALERT, S3_IEM_AUTH_CONN_FAIL, S3_IEM_AUTH_CONN_FAIL_STR,
             S3_IEM_AUTH_CONN_FAIL_JSON);
      this->handler_on_failed();
    }
  } else {
    s3_log(S3_LOG_ERROR, request_id, "Aclvalidation failure\n");

    this->handler_on_failed();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

// This is same as above but will cycle through with the
// add_checksum_for_chunk()
// to validate each chunk we receive.
void S3AuthClient::init_chunk_auth_cycle(std::function<void(void)> on_success,
                                         std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  auth_context.reset(new S3AuthClientOpContext(
      request, std::bind(&S3AuthClient::chunk_auth_successful, this),
      std::bind(&S3AuthClient::chunk_auth_failed, this)));

  is_chunked_auth = true;
  set_op_type(S3AuthClientOpType::authentication);
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
  // trigger is done when sign and hash are available for any chunk
}

// Insert the signature sent in each chunk (chunk-signature=value)
// and sha256(chunk-data) to be used in auth of chunk
void S3AuthClient::add_checksum_for_chunk(std::string current_sign,
                                          std::string sha256_of_payload) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  chunk_validation_data.push(std::make_tuple(current_sign, sha256_of_payload));
  if (state != S3AuthClientOpState::started) {
    current_chunk_signature_from_auth =
        std::get<0>(chunk_validation_data.front());
    hash_sha256_current_chunk = std::get<1>(chunk_validation_data.front());
    chunk_validation_data.pop();
    trigger_authentication(this->handler_on_success, this->handler_on_failed);
  }
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

void S3AuthClient::add_last_checksum_for_chunk(std::string current_sign,
                                               std::string sha256_of_payload) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  last_chunk_added = true;
  add_checksum_for_chunk(current_sign, sha256_of_payload);
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

void S3AuthClient::chunk_auth_successful() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  state = S3AuthClientOpState::authenticated;
  retry_count = 0;
  remember_auth_details_in_request();
  prev_chunk_signature_from_auth = get_signature_from_response();

  if (chunk_auth_aborted ||
      (last_chunk_added && chunk_validation_data.empty())) {
    // we are done with all validations
    this->handler_on_success();
  } else {
    if (chunk_validation_data.empty()) {
      // we have to wait for more chunk signatures, which will be
      // added with add_checksum_for_chunk/add_last_checksum_for_chunk
    } else {
      // Validate next chunk signature
      current_chunk_signature_from_auth =
          std::get<0>(chunk_validation_data.front());
      hash_sha256_current_chunk = std::get<1>(chunk_validation_data.front());
      chunk_validation_data.pop();
      trigger_authentication(this->handler_on_success, this->handler_on_failed);
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AuthClient::chunk_auth_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  s3_log(S3_LOG_ERROR, request_id, "Authentication failure\n");
  state = S3AuthClientOpState::unauthenticated;
  this->handler_on_failed();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
