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

#pragma once

#ifndef __S3_SERVER_S3_AUTH_CLIENT_H__
#define __S3_SERVER_S3_AUTH_CLIENT_H__

#include <gtest/gtest_prod.h>
#include <functional>
#include <memory>
#include <queue>
#include <string>

#include "s3_asyncop_context_base.h"
#include "s3_auth_context.h"
#include "s3_auth_response_error.h"
#include "s3_auth_response_success.h"
#include "s3_log.h"
#include "s3_option.h"
#include "request_object.h"

extern "C" evhtp_res on_auth_response(evhtp_request_t* req, evbuf_t* buf,
                                      void* arg);

class S3AuthClientOpContext : public S3AsyncOpContextBase {
  struct s3_auth_op_context* auth_op_context;
  bool has_auth_op_context;

  bool is_auth_successful;
  bool is_authorization_successful;
  bool is_aclvalidation_successful;
  bool is_policyvalidation_successful;

  std::unique_ptr<S3AuthResponseSuccess> success_obj;
  std::unique_ptr<S3AuthResponseError> error_obj;

  std::string auth_response_xml;
  std::string authorization_response_xml;

 public:
  S3AuthClientOpContext(std::shared_ptr<RequestObject> req,
                        std::function<void()> success_callback,
                        std::function<void()> failed_callback)
      : S3AsyncOpContextBase(req, success_callback, failed_callback),
        auth_op_context(NULL),
        has_auth_op_context(false),
        is_auth_successful(false),
        is_authorization_successful(false),
        is_aclvalidation_successful(false),
        is_policyvalidation_successful(false),
        auth_response_xml("") {
    s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  }

  ~S3AuthClientOpContext() {
    s3_log(S3_LOG_DEBUG, request_id, "Destructor\n");
    if (has_auth_op_context) {
      free_basic_auth_client_op_ctx(auth_op_context);
      auth_op_context = NULL;
    }
  }

  void set_auth_response_error(std::string error_code,
                               std::string error_message,
                               std::string request_id);

  void set_auth_response_xml(const char* xml, bool success = true) {
    s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
    auth_response_xml = xml;
    is_auth_successful = success;
    if (is_auth_successful) {
      success_obj.reset(new S3AuthResponseSuccess(auth_response_xml));
      if (success_obj->isOK()) {
        get_request()->set_user_name(success_obj->get_user_name());
        get_request()->set_canonical_id(success_obj->get_canonical_id());
        get_request()->set_user_id(success_obj->get_user_id());
        get_request()->set_account_name(success_obj->get_account_name());
        get_request()->set_account_id(success_obj->get_account_id());
        get_request()->set_email(success_obj->get_email());
      } else {
        is_auth_successful = false;  // since auth response details are bad
      }
    } else {
      error_obj.reset(new S3AuthResponseError(auth_response_xml));
    }
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  }

  void set_aclvalidation_response_xml(const char* xml, bool success = true) {
    s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
    is_aclvalidation_successful = success;
    auth_response_xml = xml;
    if (!success) {
      error_obj.reset(new S3AuthResponseError(auth_response_xml));
    }
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  }

  void set_policyvalidation_response_xml(const char* xml, bool success = true) {
    s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
    is_policyvalidation_successful = success;
    auth_response_xml = xml;
    if (!success) {
      error_obj.reset(new S3AuthResponseError(auth_response_xml));
    }
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  }

  void set_authorization_response(const char* xml, bool success = true) {
    s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
    authorization_response_xml = xml;
    is_authorization_successful = success;
    if (is_authorization_successful) {
      success_obj.reset(new S3AuthResponseSuccess(authorization_response_xml));
      if (success_obj->isOK()) {
        std::shared_ptr<S3RequestObject> s3_request =
            std::dynamic_pointer_cast<S3RequestObject>(request);
        if (s3_request != nullptr) {
          s3_request->set_user_name(success_obj->get_user_name());
          s3_request->set_canonical_id(success_obj->get_canonical_id());
          s3_request->set_user_id(success_obj->get_user_id());
          s3_request->set_account_name(success_obj->get_account_name());
          s3_request->set_account_id(success_obj->get_account_id());
          s3_request->set_default_acl(success_obj->get_acl());
          s3_request->set_email(success_obj->get_email());
        }
      } else {
        // Invalid authorisation response xml
        is_authorization_successful = false;
      }
    } else {
      error_obj.reset(new S3AuthResponseError(authorization_response_xml));
    }
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  }

  bool auth_successful() { return is_auth_successful; }

  bool authorization_successful() { return is_authorization_successful; }

  bool aclvalidation_successful() { return is_aclvalidation_successful; }

  bool policyvalidation_successful() { return is_policyvalidation_successful; }

  std::string get_user_id() {
    if (is_auth_successful) {
      return success_obj->get_user_id();
    }
    return "";
  }

  std::string get_user_name() {
    if (is_auth_successful) {
      return success_obj->get_user_name();
    }
    return "";
  }

  std::string get_canonical_id() {
    if (is_auth_successful) {
      return success_obj->get_canonical_id();
    }
    return "";
  }

  std::string get_email() {
    if (is_auth_successful) {
      return success_obj->get_email();
    }
    return "";
  }

  std::string get_account_id() {
    if (is_auth_successful) {
      return success_obj->get_account_id();
    }
    return "";
  }

  std::string get_account_name() {
    if (is_auth_successful) {
      return success_obj->get_account_name();
    }
    return "";
  }

  std::string get_signature_sha256() {
    if (is_auth_successful) {
      return success_obj->get_signature_sha256();
    }
    return "";
  }

  const std::string& get_error_code() { return error_obj->get_code(); }

  const std::string& get_error_message() { return error_obj->get_message(); }

  // Call this when you want to do auth op.
  virtual bool init_auth_op_ctx(const S3AuthClientOpType& type) {
    struct event_base* eventbase = S3Option::get_instance()->get_eventbase();
    if (eventbase == NULL) {
      return false;
    }
    if (has_auth_op_context) {
      free_basic_auth_client_op_ctx(auth_op_context);
      auth_op_context = NULL;
      has_auth_op_context = false;
    }
    auth_op_context = create_basic_auth_op_ctx(eventbase, type);
    if (auth_op_context == NULL) {
      return false;
    }
    has_auth_op_context = true;
    return true;
  }

  struct s3_auth_op_context* get_auth_op_ctx() {
    return auth_op_context;
  }

  void set_auth_callbacks(std::function<void(void)> success,
                          std::function<void(void)> failed) {
    reset_callbacks(success, failed);
  }

  FRIEND_TEST(S3AuthClientOpContextTest, Constructor);
  FRIEND_TEST(S3AuthClientOpContextTest, CanParseAuthSuccessResponse);
  FRIEND_TEST(S3AuthClientOpContextTest,
              CanHandleParseErrorInAuthSuccessResponse);
  FRIEND_TEST(S3AuthClientOpContextTest,
              CanHandleParseErrorInAuthErrorResponse);
  FRIEND_TEST(S3AuthClientOpContextTest, CanParseAuthErrorResponse);
  FRIEND_TEST(S3AuthClientOpContextTest, CanParseAuthInvalidTokenErrorResponse);
  FRIEND_TEST(S3AuthClientOpContextTest, CanParseAuthorizationSuccessResponse);
  FRIEND_TEST(S3AuthClientOpContextTest,
              CanHandleParseErrorInAuthorizeSuccessResponse);
  FRIEND_TEST(S3AuthClientOpContextTest, CanParseAuthorizationErrorResponse);
  FRIEND_TEST(S3AuthClientOpContextTest,
              CanHandleParseErrorInAuthorizeErrorResponse);

  // For UT.
 private:
  bool error_resp_is_OK() {
    if (!is_auth_successful) {
      return error_obj->isOK();
    }
    return false;
  }

  bool success_resp_is_OK() {
    if (is_auth_successful) {
      return success_obj->isOK();
    }
    return false;
  }
};

enum class S3AuthClientOpState {
  init,
  started,
  authenticated,
  unauthenticated,
  authorized,
  unauthorized,
  connection_error
};

class S3AuthClient {
 private:
  std::shared_ptr<RequestObject> request;
  std::unique_ptr<S3AuthClientOpContext> auth_context;
  std::string request_id;
  std::string bucket_acl;

  // Used to report to caller
  std::function<void()> handler_on_success;
  std::function<void()> handler_on_failed;

  S3AuthClientOpState state;
  S3AuthClientOpType op_type;

  short retry_count;

  // Key=val pairs to be sent in auth request body
  std::map<std::string, std::string> data_key_val;

  struct evbuffer* req_body_buffer;

  // Holds tuple (signature-received-in-current-chunk, sha256-chunk-data)
  // indexed in chunk order
  std::queue<std::tuple<std::string, std::string>> chunk_validation_data;
  bool is_chunked_auth;
  std::string prev_chunk_signature_from_auth;     // remember on each hop
  std::string current_chunk_signature_from_auth;  // remember on each hop
  std::string hash_sha256_current_chunk;
  std::string policy_str;
  std::string acl_str;
  std::string user_acl;
  bool last_chunk_added;
  bool is_authheader_present;
  bool chunk_auth_aborted;
  bool skip_authorization;
  void trigger_authentication(std::function<void(void)> on_success,
                              std::function<void(void)> on_failed);
  void trigger_authentication();
  void remember_auth_details_in_request();

 public:
  S3AuthClient(std::shared_ptr<RequestObject> req,
               bool skip_authorization = false);
  virtual ~S3AuthClient() {
    s3_log(S3_LOG_DEBUG, request_id, "Destructor\n");
    if (state == S3AuthClientOpState::started) {
      unset_hooks();
    }
  }

  void unset_hooks() {
    s3_log(S3_LOG_DEBUG, request_id, "Unsetting various auth related hooks\n");
    if (auth_context != NULL) {
      struct s3_auth_op_context* auth_op_ctx = auth_context->get_auth_op_ctx();
      if (auth_op_ctx != NULL) {
        if ((auth_op_ctx->conn != NULL)) {
          s3_log(S3_LOG_DEBUG, request_id, "Unsetting auth connection hooks\n");
          evhtp_unset_all_hooks(&auth_op_ctx->conn->hooks);
        }
        if (auth_op_ctx->authrequest != NULL) {
          s3_log(S3_LOG_DEBUG, "", "Unsetting auth hooks\n");
          evhtp_unset_all_hooks(&auth_op_ctx->authrequest->hooks);
        }
        if (auth_op_ctx->authorization_request != NULL) {
          s3_log(S3_LOG_DEBUG, "", "Unsetting authorization hooks\n");
          evhtp_unset_all_hooks(&auth_op_ctx->authorization_request->hooks);
        }
        if (auth_op_ctx->aclvalidation_request != NULL) {
          s3_log(S3_LOG_DEBUG, "", "Unsetting acl validation hooks\n");
          evhtp_unset_all_hooks(&auth_op_ctx->aclvalidation_request->hooks);
        }
        if (auth_op_ctx->policyvalidation_request != NULL) {
          s3_log(S3_LOG_DEBUG, "", "Unsetting policy validation hooks\n");
          evhtp_unset_all_hooks(&auth_op_ctx->policyvalidation_request->hooks);
        }
      }
    }
    s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
  }

  std::string get_request_id() { return request_id; }

  S3AuthClientOpState get_state() { return state; }

  S3AuthClientOpType get_op_type() { return op_type; }

  void set_state(S3AuthClientOpState auth_state) { state = auth_state; }

  void set_op_type(S3AuthClientOpType type) { op_type = type; }

  // Returns AccessDenied | InvalidAccessKeyId | SignatureDoesNotMatch
  // auth InactiveAccessKey maps to InvalidAccessKeyId in S3
  std::string get_error_code();
  std::string get_error_message();

  std::string get_signature_from_response();

  void do_skip_authorization() { skip_authorization = true; }
  void setup_auth_request_headers();
  bool setup_auth_request_body();
  virtual void execute_authconnect_request(struct s3_auth_op_context* auth_ctx);
  void add_key_val_to_body(std::string key, std::string val);
  void set_is_authheader_present(bool is_authorizationheader_present);

  // Non-aws-chunk auth
  void check_authentication();
  void check_authentication(std::function<void(void)> on_success,
                            std::function<void(void)> on_failed);
  void check_authentication_successful();
  void check_authentication_failed();

  void check_aclvalidation_successful();
  void check_aclvalidation_failed();

  void policy_validation_successful();
  void policy_validation_failed();

  void check_authorization();
  void validate_acl(std::function<void(void)> on_success,
                    std::function<void(void)> on_failed);
  void validate_policy(std::function<void(void)> on_success,
                       std::function<void(void)> on_failed);
  void set_validate_acl(const std::string& validateacl);
  void check_authorization(std::function<void(void)> on_success,
                           std::function<void(void)> on_failed);
  void check_authorization_successful();
  void check_authorization_failed();

  // This is same as above but will cycle through with the
  // add_checksum_for_chunk() to validate each chunk we receive. Init =
  // headers auth, start = each chunk auth
  void check_chunk_auth(std::function<void(void)> on_success,
                        std::function<void(void)> on_failed);
  virtual void init_chunk_auth_cycle(std::function<void(void)> on_success,
                                     std::function<void(void)> on_failed);
  // Insert the signature sent in each chunk (chunk-signature=value)
  // and sha256(chunk-data) to be used in auth of chunk
  virtual void add_checksum_for_chunk(std::string current_sign,
                                      std::string sha256_of_payload);
  virtual void add_last_checksum_for_chunk(std::string current_sign,
                                           std::string sha256_of_payload);
  void chunk_auth_successful();
  void chunk_auth_failed();
  void abort_chunk_auth_op() { chunk_auth_aborted = true; }
  bool is_chunk_auth_op_aborted() { return chunk_auth_aborted; }
  void set_acl_and_policy(const std::string& acl, const std::string& policy);
  void set_bucket_acl(const std::string& bucket_acl);
  void set_event_with_retry_interval();

  FRIEND_TEST(S3AuthClientTest, Constructor);
  FRIEND_TEST(S3AuthClientTest, SetUpAuthRequestBodyGet);
  FRIEND_TEST(S3AuthClientTest, SetUpAuthRequestBodyHead);
  FRIEND_TEST(S3AuthClientTest, SetUpAuthRequestBodyPut);
  FRIEND_TEST(S3AuthClientTest, SetUpAuthRequestBodyPost);
  FRIEND_TEST(S3AuthClientTest, SetUpAuthRequestBodyDelete);
  FRIEND_TEST(S3AuthClientTest, SetUpAuthRequestBodyWithQueryParams);
  FRIEND_TEST(S3AuthClientTest, SetUpAuthRequestBodyForChunkedAuth);
  FRIEND_TEST(S3AuthClientTest, SetUpAuthRequestBodyForChunkedAuth1);
  FRIEND_TEST(S3AuthClientTest, SetUpAuthRequestBodyForChunkedAuth2);
  FRIEND_TEST(S3BucketActionTest, SetAuthorizationMeta);
};

struct event_auth_timeout_arg {
  S3AuthClient* auth_client;
  struct event* event;
};

#endif
