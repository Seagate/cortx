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
 * Original author:  Rajesh Nambiar <rajesh.nambiar@seagate.com>
 * Original creation date: 19-Nov-2015
 */

#include "s3_auth_client.h"
#include <functional>
#include <iostream>
#include <memory>
#include "gtest/gtest.h"
#include "mock_evhtp_wrapper.h"
#include "mock_s3_asyncop_context_base.h"
#include "mock_s3_auth_client.h"
#include "mock_s3_request_object.h"
#include "s3_option.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::StrEq;
using ::testing::StrNe;
using ::testing::Return;
using ::testing::Mock;
using ::testing::InvokeWithoutArgs;
using ::testing::ReturnRef;

static void dummy_request_cb(evhtp_request_t *req, void *arg) {}

void dummy_function() { return; }

class S3AuthBaseResponse {
 public:
  virtual void auth_response_wrapper(evhtp_request_t *req, evbuf_t *buf,
                                     void *arg) = 0;
};

class S3AuthResponse : public S3AuthBaseResponse {
 public:
  S3AuthResponse() { success_called = fail_called = false; }
  virtual void auth_response_wrapper(evhtp_request_t *req, evbuf_t *buf,
                                     void *arg) {
    on_auth_response(req, buf, arg);
  }

  void success_callback() { success_called = true; }

  void fail_callback() { fail_called = true; }

  bool success_called;
  bool fail_called;
};

class S3AuthClientOpContextTest : public testing::Test {
 protected:
  S3AuthClientOpContextTest() {

    evbase_t *evbase = event_base_new();
    evhtp_request_t *req = evhtp_request_new(NULL, evbase);
    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(req, new EvhtpWrapper());
    S3Option::get_instance()->set_eventbase(evbase);
    success_callback = NULL;
    failed_callback = NULL;
    p_authopctx = new S3AuthClientOpContext(ptr_mock_request, success_callback,
                                            failed_callback);
  }

  ~S3AuthClientOpContextTest() { delete p_authopctx; }

  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::function<void()> success_callback;
  std::function<void()> failed_callback;
  S3AuthClientOpContext *p_authopctx;
};

class S3AuthClientTest : public testing::Test {
 protected:
  S3AuthClientTest() {
    evbase_t *evbase = event_base_new();
    ev_req = evhtp_request_new(NULL, evbase);
    ptr_mock_request =
        std::make_shared<MockS3RequestObject>(ev_req, new EvhtpWrapper());

    p_authclienttest = new S3AuthClient(ptr_mock_request);

    auth_client_op_context = (struct s3_auth_op_context *)calloc(
        1, sizeof(struct s3_auth_op_context));
    auth_client_op_context->evbase = event_base_new();
    auth_client_op_context->authrequest =
        evhtp_request_new(dummy_request_cb, auth_client_op_context->evbase);
  }

  ~S3AuthClientTest() {
    event_base_free(auth_client_op_context->evbase);
    free(auth_client_op_context);
    delete p_authclienttest;
  }

  void fake_in_header(std::string key, std::string val) {
    evhtp_headers_add_header(ev_req->headers_in,
                             evhtp_header_new(key.c_str(), val.c_str(), 0, 0));
  }

  evhtp_request_t *ev_req;
  S3AuthClient *p_authclienttest;
  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  struct s3_auth_op_context *auth_client_op_context;
};

TEST_F(S3AuthClientOpContextTest, Constructor) {
  EXPECT_EQ(NULL, p_authopctx->auth_op_context);
  EXPECT_FALSE(p_authopctx->has_auth_op_context);
}

TEST_F(S3AuthClientOpContextTest, InitAuthCtxNull) {
  evbase_t *evbase = NULL;
  S3Option::get_instance()->set_eventbase(evbase);
  S3AuthClientOpType auth_request_type = S3AuthClientOpType::authentication;
  bool ret = p_authopctx->init_auth_op_ctx(auth_request_type);
  EXPECT_FALSE(ret);
}

TEST_F(S3AuthClientOpContextTest, InitAuthCtxValid) {
  S3AuthClientOpType auth_request_type = S3AuthClientOpType::authentication;
  bool ret = p_authopctx->init_auth_op_ctx(auth_request_type);
  EXPECT_TRUE(ret == true);
}

TEST_F(S3AuthClientOpContextTest, GetAuthCtx) {

  S3AuthClientOpType auth_request_type = S3AuthClientOpType::authentication;
  p_authopctx->init_auth_op_ctx(auth_request_type);
  struct s3_auth_op_context *p_ctx = p_authopctx->get_auth_op_ctx();
  EXPECT_TRUE(p_ctx != NULL);
}

TEST_F(S3AuthClientOpContextTest, CanParseAuthSuccessResponse) {
  std::string sample_response =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" "
      "standalone=\"no\"?><AuthenticateUserResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/"
      "\"><AuthenticateUserResult><UserId>123</UserId><UserName>tester</"
      "UserName><AccountId>12345</AccountId><AccountName>s3_test</"
      "AccountName><SignatureSHA256>BSewvoSw/0og+hWR4I77NcWea24=</"
      "SignatureSHA256></"
      "AuthenticateUserResult><ResponseMetadata><RequestId>0000</RequestId></"
      "ResponseMetadata></AuthenticateUserResponse>";

  p_authopctx->set_auth_response_xml(sample_response.c_str(), true);

  EXPECT_TRUE(p_authopctx->is_auth_successful);
  EXPECT_STREQ("tester", p_authopctx->get_request()->get_user_name().c_str());
  EXPECT_STREQ("123", p_authopctx->get_request()->get_user_id().c_str());
  EXPECT_STREQ("s3_test",
               p_authopctx->get_request()->get_account_name().c_str());
  EXPECT_STREQ("12345", p_authopctx->get_request()->get_account_id().c_str());
}

TEST_F(S3AuthClientOpContextTest, CanParseAuthorizationSuccessResponse) {
  std::string sample_response =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" "
      "standalone=\"no\"?><AuthorizeUserResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/"
      "\"><AuthorizeUserResult><UserId>123</UserId><UserName>tester</"
      "UserName><AccountId>12345</AccountId><AccountName>s3_test</"
      "AccountName><CanonicalId>507e9f946afa4a18b0ac54e869c5fc6b6eb518c22e3a90<"
      "/CanonicalId></AuthorizeUserResult><ResponseMetadata><RequestId>0000</"
      "RequestId></ResponseMetadata></AuthorizeUserResponse>";

  p_authopctx->set_auth_response_xml(sample_response.c_str(), true);

  EXPECT_TRUE(p_authopctx->is_auth_successful);
  EXPECT_STREQ("tester", p_authopctx->get_request()->get_user_name().c_str());
  EXPECT_STREQ("507e9f946afa4a18b0ac54e869c5fc6b6eb518c22e3a90",
               p_authopctx->get_request()->get_canonical_id().c_str());
  EXPECT_STREQ("123", p_authopctx->get_request()->get_user_id().c_str());
  EXPECT_STREQ("s3_test",
               p_authopctx->get_request()->get_account_name().c_str());
  EXPECT_STREQ("12345", p_authopctx->get_request()->get_account_id().c_str());
}

TEST_F(S3AuthClientOpContextTest,
       CanHandleParseErrorInAuthorizeSuccessResponse) {
  // Missing AccountId
  std::string sample_response =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" "
      "standalone=\"no\"?><AuthenticateUserResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/"
      "\"><AuthorizeUserResult><UserId>123</UserId><UserName>tester</"
      "UserName><AccountName>s3_test</AccountName><SignatureSHA256>BSewvoSw/"
      "0og+hWR4I77NcWea24=</SignatureSHA256></"
      "AuthenticateUserResult><ResponseMetadata><RequestId>0000</RequestId></"
      "ResponseMetadata></AuthorizeUserResponse>";

  p_authopctx->set_auth_response_xml(sample_response.c_str(), true);

  EXPECT_FALSE(p_authopctx->is_auth_successful);
}

TEST_F(S3AuthClientOpContextTest, CanHandleParseErrorInAuthSuccessResponse) {
  // Missing AccountId
  std::string sample_response =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" "
      "standalone=\"no\"?><AuthenticateUserResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/"
      "\"><AuthenticateUserResult><UserId>123</UserId><UserName>tester</"
      "UserName><AccountName>s3_test</AccountName><SignatureSHA256>BSewvoSw/"
      "0og+hWR4I77NcWea24=</SignatureSHA256></"
      "AuthenticateUserResult><ResponseMetadata><RequestId>0000</RequestId></"
      "ResponseMetadata></AuthenticateUserResponse>";

  p_authopctx->set_auth_response_xml(sample_response.c_str(), true);

  EXPECT_FALSE(p_authopctx->is_auth_successful);
}

TEST_F(S3AuthClientOpContextTest, CanParseAuthErrorResponse) {
  std::string sample_response =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" "
      "standalone=\"no\"?><ErrorResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/"
      "\"><Error><Code>SignatureDoesNotMatch</Code><Message>The request "
      "signature we "
      "calculated does not match the signature you provided. Check your AWS "
      "secret access key and signing method. For more information, see REST "
      "Authentication andSOAP Authentication for "
      "details.</Message></Error><RequestId>0000</RequestId></ErrorResponse>";

  p_authopctx->set_auth_response_xml(sample_response.c_str(), false);

  EXPECT_FALSE(p_authopctx->is_auth_successful);
  EXPECT_STREQ("SignatureDoesNotMatch", p_authopctx->get_error_code().c_str());
  EXPECT_STREQ(
      "The request signature we calculated does not match the signature you "
      "provided. Check your AWS secret access key and signing method. For more "
      "information, see REST Authentication andSOAP Authentication for "
      "details.",
      p_authopctx->get_error_message().c_str());
}

TEST_F(S3AuthClientOpContextTest, CanParseAuthInvalidTokenErrorResponse) {
  std::string sample_response =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" "
      "standalone=\"no\"?><ErrorResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/"
      "\"><Error><Code>InvalidToken</Code><Message>The provided "
      "token is malformed or otherwise "
      "invalid.</Message></Error><RequestId>0000</RequestId></ErrorResponse>";

  p_authopctx->set_auth_response_xml(sample_response.c_str(), false);

  EXPECT_FALSE(p_authopctx->is_auth_successful);
  EXPECT_STREQ("InvalidToken", p_authopctx->get_error_code().c_str());
  EXPECT_STREQ("The provided token is malformed or otherwise invalid.",
               p_authopctx->get_error_message().c_str());
}

TEST_F(S3AuthClientOpContextTest, CanParseAuthorizationErrorResponse) {
  std::string sample_response =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" "
      "standalone=\"no\"?><ErrorResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/"
      "\"><Error><Code>UnauthorizedOperation</Code><Message>You are not "
      "authorized to "
      "perform this operation. Check your IAM policies, and ensure that you "
      "are using the correct access "
      "keys.</Message></Error><RequestId>0000</RequestId></ErrorResponse>";

  p_authopctx->set_auth_response_xml(sample_response.c_str(), false);

  EXPECT_FALSE(p_authopctx->is_auth_successful);
  EXPECT_STREQ("UnauthorizedOperation", p_authopctx->get_error_code().c_str());
  EXPECT_STREQ(
      "You are not authorized to perform this operation. Check your IAM "
      "policies, and ensure that you are using the correct access keys.",
      p_authopctx->get_error_message().c_str());
}

TEST_F(S3AuthClientOpContextTest, CanHandleParseErrorInAuthErrorResponse) {
  // Missing code
  std::string sample_response =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?><Error "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\"><Message>The request "
      "signature we calculated does not match the signature you provided. "
      "Check your AWS secret access key and signing method. For more "
      "information, see REST Authentication andSOAP Authentication for "
      "details.</Message><RequestId>0000</RequestId></Error>";

  p_authopctx->set_auth_response_xml(sample_response.c_str(), false);

  EXPECT_FALSE(p_authopctx->error_resp_is_OK());
}

TEST_F(S3AuthClientOpContextTest, CanHandleParseErrorInAuthorizeErrorResponse) {
  // Missing code
  std::string sample_response =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?><Error "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\"><Message>You are not "
      "authorized to perform this operation. Check your IAM policies, and "
      "ensure that you are using the correct access "
      "keys.</Message><RequestId>0000</RequestId></Error>";

  p_authopctx->set_auth_response_xml(sample_response.c_str(), false);

  EXPECT_FALSE(p_authopctx->error_resp_is_OK());
}

TEST_F(S3AuthClientTest, Constructor) {
  EXPECT_TRUE(p_authclienttest->get_state() == S3AuthClientOpState::init);
  EXPECT_FALSE(p_authclienttest->is_chunked_auth);
  EXPECT_FALSE(p_authclienttest->last_chunk_added);
  EXPECT_FALSE(p_authclienttest->chunk_auth_aborted);
}

TEST_F(S3AuthClientTest, SetUpAuthRequestBodyGet) {
  char expectedbody[] =
      "Action=AuthenticateUser&ClientAbsoluteUri=%2F&ClientQueryParams=&Method="
      "GET&Request_id=123&RequestorAccountId=12345&RequestorAccountName=s3_"
      "test&RequestorCanonicalId=123456789dummyCANONICALID&RequestorEmail=abc%"
      "40dummy.com&RequestorUserId=123&RequestorUserName=tester&Version=2010-"
      "05-08";
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, http_verb())
      .WillRepeatedly(Return(S3HttpVerb::GET));
  EXPECT_CALL(*ptr_mock_request, c_get_full_encoded_path())
      .WillRepeatedly(Return("/"));
  p_authclienttest->request_id = "123";
  p_authclienttest->set_op_type(S3AuthClientOpType::authentication);
  p_authclienttest->setup_auth_request_body();
  int len = evbuffer_get_length(p_authclienttest->req_body_buffer);
  char *mybuff = (char *)calloc(1, len + 1);
  evbuffer_copyout(p_authclienttest->req_body_buffer, mybuff, len);

  EXPECT_STREQ(expectedbody, mybuff);

  free(mybuff);
}

TEST_F(S3AuthClientTest, SetUpAuthRequestBodyPut) {
  char expectedbody[] =
      "Action=AuthenticateUser&ClientAbsoluteUri=%2F&ClientQueryParams=&Method="
      "PUT&Request_id=123&RequestorAccountId=12345&RequestorAccountName=s3_"
      "test&RequestorCanonicalId=123456789dummyCANONICALID&RequestorEmail=abc%"
      "40dummy.com&RequestorUserId=123&RequestorUserName=tester&Version=2010-"
      "05-08";
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, http_verb())
      .WillRepeatedly(Return(S3HttpVerb::PUT));
  EXPECT_CALL(*ptr_mock_request, c_get_full_encoded_path())
      .WillRepeatedly(Return("/"));
  p_authclienttest->request_id = "123";
  p_authclienttest->set_op_type(S3AuthClientOpType::authentication);
  p_authclienttest->setup_auth_request_body();
  int len = evbuffer_get_length(p_authclienttest->req_body_buffer);
  char *mybuff = (char *)calloc(1, len + 1);
  evbuffer_copyout(p_authclienttest->req_body_buffer, mybuff, len);

  EXPECT_STREQ(expectedbody, mybuff);

  free(mybuff);
}

TEST_F(S3AuthClientTest, SetUpAuthRequestBodyHead) {
  char expectedbody[] =
      "Action=AuthenticateUser&ClientAbsoluteUri=%2F&ClientQueryParams=&Method="
      "HEAD&Request_id=123&RequestorAccountId=12345&RequestorAccountName=s3_"
      "test&RequestorCanonicalId=123456789dummyCANONICALID&RequestorEmail=abc%"
      "40dummy.com&RequestorUserId=123&RequestorUserName=tester&Version=2010-"
      "05-08";
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, http_verb())
      .WillRepeatedly(Return(S3HttpVerb::HEAD));
  EXPECT_CALL(*ptr_mock_request, c_get_full_encoded_path())
      .WillRepeatedly(Return("/"));
  p_authclienttest->request_id = "123";
  p_authclienttest->set_op_type(S3AuthClientOpType::authentication);
  p_authclienttest->setup_auth_request_body();
  int len = evbuffer_get_length(p_authclienttest->req_body_buffer);
  char *mybuff = (char *)calloc(1, len + 1);
  evbuffer_copyout(p_authclienttest->req_body_buffer, mybuff, len);

  EXPECT_STREQ(expectedbody, mybuff);

  free(mybuff);
}

TEST_F(S3AuthClientTest, SetUpAuthRequestBodyDelete) {
  char expectedbody[] =
      "Action=AuthenticateUser&ClientAbsoluteUri=%2F&ClientQueryParams=&Method="
      "DELETE&Request_id=123&RequestorAccountId=12345&RequestorAccountName=s3_"
      "test&RequestorCanonicalId=123456789dummyCANONICALID&RequestorEmail=abc%"
      "40dummy.com&RequestorUserId=123&RequestorUserName=tester&Version=2010-"
      "05-08";
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, http_verb())
      .WillRepeatedly(Return(S3HttpVerb::DELETE));
  EXPECT_CALL(*ptr_mock_request, c_get_full_encoded_path())
      .WillRepeatedly(Return("/"));
  p_authclienttest->request_id = "123";
  p_authclienttest->set_op_type(S3AuthClientOpType::authentication);
  p_authclienttest->setup_auth_request_body();
  int len = evbuffer_get_length(p_authclienttest->req_body_buffer);
  char *mybuff = (char *)calloc(1, len + 1);
  evbuffer_copyout(p_authclienttest->req_body_buffer, mybuff, len);

  EXPECT_STREQ(expectedbody, mybuff);

  free(mybuff);
}

TEST_F(S3AuthClientTest, SetUpAuthRequestBodyPost) {
  char expectedbody[] =
      "Action=AuthenticateUser&ClientAbsoluteUri=%2F&ClientQueryParams=&Method="
      "POST&Request_id=123&RequestorAccountId=12345&RequestorAccountName=s3_"
      "test&RequestorCanonicalId=123456789dummyCANONICALID&RequestorEmail=abc%"
      "40dummy.com&RequestorUserId=123&RequestorUserName=tester&Version=2010-"
      "05-08";
  std::map<std::string, std::string, compare> query_params;

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, http_verb())
      .WillRepeatedly(Return(S3HttpVerb::POST));
  EXPECT_CALL(*ptr_mock_request, c_get_full_encoded_path())
      .WillRepeatedly(Return("/"));
  p_authclienttest->request_id = "123";
  p_authclienttest->set_op_type(S3AuthClientOpType::authentication);
  p_authclienttest->setup_auth_request_body();
  int len = evbuffer_get_length(p_authclienttest->req_body_buffer);
  char *mybuff = (char *)calloc(1, len + 1);
  evbuffer_copyout(p_authclienttest->req_body_buffer, mybuff, len);

  EXPECT_STREQ(expectedbody, mybuff);

  free(mybuff);
}

TEST_F(S3AuthClientTest, SetUpAuthRequestBodyWithQueryParams) {
  char expectedbody[] =
      "Action=AuthenticateUser&ClientAbsoluteUri=%2F&ClientQueryParams="
      "delimiter%3D%252F%26prefix%3Dtest&Method=GET&Request_id=123&"
      "RequestorAccountId=12345&RequestorAccountName=s3_test&"
      "RequestorCanonicalId=123456789dummyCANONICALID&RequestorEmail=abc%"
      "40dummy.com&RequestorUserId=123&RequestorUserName=tester&Version=2010-"
      "05-08";
  std::map<std::string, std::string, compare> query_params;
  query_params["delimiter"] = "/";
  query_params["prefix"] = "test";

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, http_verb())
      .WillRepeatedly(Return(S3HttpVerb::GET));
  EXPECT_CALL(*ptr_mock_request, c_get_full_encoded_path())
      .WillRepeatedly(Return("/"));
  p_authclienttest->request_id = "123";
  p_authclienttest->set_op_type(S3AuthClientOpType::authentication);
  p_authclienttest->setup_auth_request_body();
  int len = evbuffer_get_length(p_authclienttest->req_body_buffer);
  char *mybuff = (char *)calloc(1, len + 1);
  evbuffer_copyout(p_authclienttest->req_body_buffer, mybuff, len);

  EXPECT_STREQ(expectedbody, mybuff);

  free(mybuff);
}

TEST_F(S3AuthClientTest, SetUpAuthRequestBodyForChunkedAuth) {
  char expectedbody[] =
      "Action=AuthenticateUser&ClientAbsoluteUri=%2F&ClientQueryParams="
      "delimiter%3D%252F%26prefix%3Dtest&Method=GET&Request_id=123&"
      "RequestorAccountId=12345&RequestorAccountName=s3_test&"
      "RequestorCanonicalId=123456789dummyCANONICALID&RequestorEmail=abc%"
      "40dummy.com&RequestorUserId=123&RequestorUserName=tester&Version=2010-"
      "05-08";
  std::map<std::string, std::string, compare> query_params;
  query_params["delimiter"] = "/";
  query_params["prefix"] = "test";

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, http_verb())
      .WillRepeatedly(Return(S3HttpVerb::GET));
  EXPECT_CALL(*ptr_mock_request, c_get_full_encoded_path())
      .WillRepeatedly(Return("/"));

  p_authclienttest->is_chunked_auth = true;
  p_authclienttest->request_id = "123";
  p_authclienttest->prev_chunk_signature_from_auth = "";
  p_authclienttest->current_chunk_signature_from_auth = "ABCD";
  p_authclienttest->set_op_type(S3AuthClientOpType::authentication);
  p_authclienttest->setup_auth_request_body();
  int len = evbuffer_get_length(p_authclienttest->req_body_buffer);
  char *mybuff = (char *)calloc(1, len + 1);
  evbuffer_copyout(p_authclienttest->req_body_buffer, mybuff, len);

  EXPECT_STREQ(expectedbody, mybuff);

  free(mybuff);
}

TEST_F(S3AuthClientTest, SetUpAuthRequestBodyForChunkedAuth1) {
  char expectedbody[] =
      "Action=AuthenticateUser&ClientAbsoluteUri=%2F&ClientQueryParams="
      "delimiter%3D%252F%26prefix%3Dtest&Method=GET&Request_id=123&"
      "RequestorAccountId=12345&RequestorAccountName=s3_test&"
      "RequestorCanonicalId=123456789dummyCANONICALID&RequestorEmail=abc%"
      "40dummy.com&RequestorUserId=123&RequestorUserName=tester&Version=2010-"
      "05-08";
  std::map<std::string, std::string, compare> query_params;
  query_params["delimiter"] = "/";
  query_params["prefix"] = "test";

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, http_verb())
      .WillRepeatedly(Return(S3HttpVerb::GET));
  EXPECT_CALL(*ptr_mock_request, c_get_full_encoded_path())
      .WillRepeatedly(Return("/"));

  p_authclienttest->is_chunked_auth = true;
  p_authclienttest->request_id = "123";
  p_authclienttest->prev_chunk_signature_from_auth = "ABCD";
  p_authclienttest->current_chunk_signature_from_auth = "";
  p_authclienttest->set_op_type(S3AuthClientOpType::authentication);
  p_authclienttest->setup_auth_request_body();
  int len = evbuffer_get_length(p_authclienttest->req_body_buffer);
  char *mybuff = (char *)calloc(1, len + 1);
  evbuffer_copyout(p_authclienttest->req_body_buffer, mybuff, len);

  EXPECT_STREQ(expectedbody, mybuff);

  free(mybuff);
}

TEST_F(S3AuthClientTest, SetUpAuthRequestBodyForChunkedAuth2) {
  char expectedbody[] =
      "Action=AuthenticateUser&ClientAbsoluteUri=%2F&ClientQueryParams="
      "delimiter%3D%252F%26prefix%3Dtest&Method=GET&Request_id=123&"
      "RequestorAccountId=12345&RequestorAccountName=s3_test&"
      "RequestorCanonicalId=123456789dummyCANONICALID&RequestorEmail=abc%"
      "40dummy.com&RequestorUserId=123&RequestorUserName=tester&Version=2010-"
      "05-08&current-signature-sha256=cur-XYZ&previous-signature-sha256=prev-"
      "ABCD&x-amz-content-sha256=sha256-abcd";
  std::map<std::string, std::string, compare> query_params;
  query_params["prefix"] = "test";
  query_params["delimiter"] = "/";

  EXPECT_CALL(*ptr_mock_request, get_query_parameters())
      .WillRepeatedly(ReturnRef(query_params));
  EXPECT_CALL(*ptr_mock_request, http_verb())
      .WillRepeatedly(Return(S3HttpVerb::GET));
  EXPECT_CALL(*ptr_mock_request, c_get_full_encoded_path())
      .WillRepeatedly(Return("/"));

  p_authclienttest->is_chunked_auth = true;
  p_authclienttest->request_id = "123";
  p_authclienttest->prev_chunk_signature_from_auth = "prev-ABCD";
  p_authclienttest->current_chunk_signature_from_auth = "cur-XYZ";
  p_authclienttest->hash_sha256_current_chunk = "sha256-abcd";
  p_authclienttest->set_op_type(S3AuthClientOpType::authentication);
  p_authclienttest->setup_auth_request_body();
  int len = evbuffer_get_length(p_authclienttest->req_body_buffer);
  char *mybuff = (char *)calloc(1, len + 1);
  evbuffer_copyout(p_authclienttest->req_body_buffer, mybuff, len);

  EXPECT_STREQ(expectedbody, mybuff);

  free(mybuff);
}
