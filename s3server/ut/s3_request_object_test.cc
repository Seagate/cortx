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
 * Original creation date: 9-Nov-2015
 */
#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "s3_error_codes.h"
#include "s3_request_object.h"
#include "mock_evhtp_wrapper.h"
#include "mock_event_wrapper.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrEq;

static void dummy_request_cb(evhtp_request_t *req, void *arg) {}

// To use a test fixture, derive a class from testing::Test.
class S3RequestObjectTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.

  S3RequestObjectTest() {
    // placeholder evbase
    evbase = event_base_new();
    ev_request = evhtp_request_new(dummy_request_cb, evbase);
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    EventInterface *s3_event_obj_ptr = new EventWrapper();
    request =
        new S3RequestObject(ev_request, evhtp_obj_ptr, NULL, s3_event_obj_ptr);

    mock_evhtp_obj_ptr = new MockEvhtpWrapper();
    mock_event_obj_ptr = new MockEventWrapper();
    request_with_mock_http_event = new S3RequestObject(
        ev_request, mock_evhtp_obj_ptr, nullptr, mock_event_obj_ptr);
    callback_called = false;
  }

  ~S3RequestObjectTest() {
    printf("Calling delete request\n");
    delete request;

    mock_evhtp_obj_ptr = NULL;
    mock_event_obj_ptr = NULL;
    delete request_with_mock_http_event;

    event_base_free(evbase);
  }

  // A helper functions
  void init_uri_() {
    ev_request->uri = (evhtp_uri_t *)calloc(sizeof(evhtp_uri_t), 1);
    ev_request->uri->query = evhtp_query_new();
    ev_request->uri->path = (evhtp_path_t *)calloc(sizeof(evhtp_path_t), 1);
  }

  char *malloc_cp_c_str_(std::string str) {
    char *c_str = (char *)malloc(str.length() + 1);
    memcpy(c_str, str.c_str(), str.length());
    c_str[str.length()] = '\0';
    return c_str;
  }

  // Test setup helpers
  void fake_in_headers(std::map<std::string, std::string> hdrs_in) {
    for (auto itr : hdrs_in) {
      evhtp_headers_add_header(
          ev_request->headers_in,
          evhtp_header_new(itr.first.c_str(), itr.second.c_str(), 0, 0));
    }
    request->initialise();  // reinitialize so proper state can be set
  }

  // For simplicity of test we take separate args and not repeat
  // _evhtp_path_new()
  void fake_uri_path(std::string full, std::string path, std::string file) {
    init_uri_();
    ev_request->uri->path->full = malloc_cp_c_str_(full);
    ASSERT_TRUE(ev_request->uri->path->full != NULL);
    ev_request->uri->path->path = malloc_cp_c_str_(path);
    ASSERT_TRUE(ev_request->uri->path->path != NULL);
    ev_request->uri->path->file = malloc_cp_c_str_(file);
    ASSERT_TRUE(ev_request->uri->path->full != NULL);
  }

  void fake_query_params(std::string raw_query) {
    init_uri_();
    ev_request->uri->query_raw = (unsigned char *)malloc_cp_c_str_(raw_query);
    ASSERT_TRUE(ev_request->uri->query_raw != NULL);

    ev_request->uri->query =
        evhtp_parse_query_wflags(raw_query.c_str(), raw_query.length(),
                                 EVHTP_PARSE_QUERY_FLAG_ALLOW_NULL_VALS);
    if (!ev_request->uri->query) {
      FAIL() << "Test case setup failed due to invalid query.";
    }
  }

  void fake_buffer_in(std::string content) {
    evbuffer_add(ev_request->buffer_in, content.c_str(), content.length());
    // Since we are faking input data, we need to trigger notify on request
    // object.
    request->notify_incoming_data(ev_request->buffer_in);
  }

  // Declares the variables your tests want to use.
  S3RequestObject *request;
  evhtp_request_t *ev_request;  // To fill test data
  evbase_t *evbase;
  bool callback_called;
  MockEvhtpWrapper *mock_evhtp_obj_ptr;
  MockEventWrapper *mock_event_obj_ptr;
  S3RequestObject *request_with_mock_http_event;

 public:
  void callback_func() { callback_called = true; }
};

TEST_F(S3RequestObjectTest, ReturnsValidRawQuery) {
  request->set_query_params("location=US&policy=test");
  EXPECT_STREQ("location=US&policy=test", (char *)request->c_get_uri_query());
}

TEST_F(S3RequestObjectTest, ReturnsValidUriPaths) {
  request->set_full_path("/bucket_name/object_name");
  EXPECT_STREQ("/bucket_name/object_name", request->c_get_full_path());
  request->set_file_name("object_name");
  EXPECT_STREQ("object_name", request->c_get_file_name());
}

TEST_F(S3RequestObjectTest, ReturnsValidHeadersCopy) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Length"] = "512";
  input_headers["Host"] = "kaustubh.s3.seagate.com";

  fake_in_headers(input_headers);

  EXPECT_TRUE(input_headers == request->get_in_headers_copy());
}

TEST_F(S3RequestObjectTest, IsValidIpAddress) {
  std::string ipaddr;
  ipaddr = "127.0.0.1";
  EXPECT_TRUE(request->is_valid_ipaddress(ipaddr));
  ipaddr = "1234";
  EXPECT_FALSE(request->is_valid_ipaddress(ipaddr));
  ipaddr = "::1";
  EXPECT_TRUE(request->is_valid_ipaddress(ipaddr));
}

TEST_F(S3RequestObjectTest, ReturnsValidHeaderValue) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Type"] = "application/xml";
  input_headers["Host"] = "kaustubh.s3.seagate.com";

  fake_in_headers(input_headers);

  EXPECT_EQ(std::string("application/xml"),
            request->get_header_value("Content-Type"));
}

TEST_F(S3RequestObjectTest, ReturnsValidHostHeaderValue) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Type"] = "application/xml";
  input_headers["Host"] = "s3.seagate.com";

  fake_in_headers(input_headers);

  EXPECT_EQ(std::string("s3.seagate.com"), request->get_host_header());
}

TEST_F(S3RequestObjectTest, ReturnsHostName) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Type"] = "application/xml";
  input_headers["Host"] = "s3.seagate.com";

  fake_in_headers(input_headers);
  EXPECT_EQ(std::string("s3.seagate.com"), request->get_host_name());
}

TEST_F(S3RequestObjectTest, ReturnsHostNameWithPortStripped) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Type"] = "application/xml";
  input_headers["Host"] = "s3.seagate.com:8081";

  fake_in_headers(input_headers);
  EXPECT_EQ(std::string("s3.seagate.com"), request->get_host_name());
}

TEST_F(S3RequestObjectTest, ReturnsIPV4Address) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Type"] = "application/xml";
  input_headers["Host"] = "127.0.0.1";

  fake_in_headers(input_headers);
  EXPECT_EQ(std::string("127.0.0.1"), request->get_host_name());
}

TEST_F(S3RequestObjectTest, ReturnsIPV4AddressWithPortStripped) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Type"] = "application/xml";
  input_headers["Host"] = "127.0.0.1:8081";

  fake_in_headers(input_headers);
  EXPECT_EQ(std::string("127.0.0.1"), request->get_host_name());
}

TEST_F(S3RequestObjectTest, ReturnsIPV6Address) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Type"] = "application/xml";
  input_headers["Host"] = "2001:db8::1";

  fake_in_headers(input_headers);
  EXPECT_EQ(std::string("2001:db8::1"), request->get_host_name());
}

TEST_F(S3RequestObjectTest, ReturnsIPV6AddressWithPortStripped) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Type"] = "application/xml";
  input_headers["Host"] = "2001:db8:1f70::999:de8:7648:6e8:8081";

  fake_in_headers(input_headers);
  EXPECT_EQ(std::string("2001:db8:1f70::999:de8:7648:6e8"),
            request->get_host_name());
}

TEST_F(S3RequestObjectTest, ReturnsIPV6loopbackAddress) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Type"] = "application/xml";
  input_headers["Host"] = "::1";

  fake_in_headers(input_headers);
  EXPECT_EQ(std::string("::1"), request->get_host_name());
}

TEST_F(S3RequestObjectTest, ReturnsIPV6loopbackAddressWithStrippedPort) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Type"] = "application/xml";
  input_headers["Host"] = "::1:8081";

  fake_in_headers(input_headers);
  EXPECT_EQ(std::string("::1"), request->get_host_name());
}

TEST_F(S3RequestObjectTest, ReturnsValidContentLengthHeaderValue) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Length"] = "852";
  input_headers["Host"] = "s3.seagate.com";

  fake_in_headers(input_headers);

  EXPECT_EQ(852, request->get_content_length());
}

TEST_F(S3RequestObjectTest, ValidateContentLengthWithNonNumericTest1) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Length"] = "abcd";
  input_headers["Host"] = "s3.seagate.com";

  EXPECT_THROW(fake_in_headers(input_headers), std::invalid_argument);
  EXPECT_FALSE(request->validate_content_length());
}

TEST_F(S3RequestObjectTest, ValidateContentLengthWithNonNumericTest2) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Length"] = "123";
  input_headers["Host"] = "s3.seagate.com";
  input_headers["x-amz-decoded-content-length"] = "abcd";

  EXPECT_THROW(fake_in_headers(input_headers), std::invalid_argument);
  EXPECT_FALSE(request->validate_content_length());
}

TEST_F(S3RequestObjectTest, ValidateContentLengthValidTest1) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Length"] = "123";
  input_headers["Host"] = "s3.seagate.com";
  fake_in_headers(input_headers);
  EXPECT_TRUE(request->validate_content_length());
}

TEST_F(S3RequestObjectTest, ValidateContentLengthValidTest2) {
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Length"] = "123";
  input_headers["Host"] = "s3.seagate.com";
  input_headers["x-amz-decoded-content-length"] = "456";
  fake_in_headers(input_headers);

  EXPECT_TRUE(request->validate_content_length());
}

TEST_F(S3RequestObjectTest, ReturnsValidContentBody) {
  std::string content = "Hello World";
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Length"] = "11";

  fake_in_headers(input_headers);
  fake_buffer_in(content);

  EXPECT_EQ(content, request->get_full_body_content_as_string());
}

TEST_F(S3RequestObjectTest, ReturnsValidQueryStringValue) {
  fake_query_params("location=US&policy=test");

  EXPECT_EQ(std::string("US"), request->get_query_string_value("location"));
}

TEST_F(S3RequestObjectTest, ReturnsValidQueryStringNullValue) {
  fake_query_params("location=US&policy");

  EXPECT_EQ(std::string(""), request->get_query_string_value("policy"));
}

TEST_F(S3RequestObjectTest, HasValidQueryStringParam) {
  fake_query_params("location=US&policy=secure");

  EXPECT_TRUE(request->has_query_param_key("location"));
}

TEST_F(S3RequestObjectTest, MissingQueryStringParam) {
  fake_query_params("location=US&policy=secure");

  EXPECT_FALSE(request->has_query_param_key("acl"));
}

TEST_F(S3RequestObjectTest, SetsOutputHeader) {
  request->set_out_header_value("Content-Type", "application/xml");
  EXPECT_TRUE(evhtp_kvs_find_kv(ev_request->headers_out, "Content-Type") !=
              NULL);
}

TEST_F(S3RequestObjectTest, ReturnsValidObjectURI) {
  std::string bucket = "s3project";
  std::string object = "member";
  request->set_bucket_name(bucket);
  request->set_object_name(object);
  EXPECT_EQ(std::string("s3project/member"), request->get_object_uri());
}

TEST_F(S3RequestObjectTest, SetsBucketName) {
  std::string bucket = "s3bucket";
  request->set_bucket_name(bucket);
  EXPECT_EQ(std::string("s3bucket"), request->get_bucket_name());
}

TEST_F(S3RequestObjectTest, SetsObjectName) {
  std::string object = "Makefile";
  request->set_object_name(object);
  EXPECT_EQ(std::string("Makefile"), request->get_object_name());
}

TEST_F(S3RequestObjectTest, SetsApiName) {
  S3ApiType apitype = S3ApiType::bucket;
  request->set_api_type(apitype);
  EXPECT_EQ(S3ApiType::bucket, request->get_api_type());
}

TEST_F(S3RequestObjectTest, SetsUserName) {
  std::string user = "kaustubh";
  request->set_user_name(user);
  EXPECT_EQ(std::string("kaustubh"), request->get_user_name());
}

TEST_F(S3RequestObjectTest, SetsUserID) {
  std::string user_id = "rajesh";
  request->set_user_id(user_id);
  EXPECT_EQ(std::string("rajesh"), request->get_user_id());
}

TEST_F(S3RequestObjectTest, SetsAccountName) {
  std::string account = "arjun";
  request->set_account_name(account);
  EXPECT_EQ(std::string("arjun"), request->get_account_name());
}

TEST_F(S3RequestObjectTest, SetsAccountID) {
  std::string account_id = "bikrant";
  request->set_account_id(account_id);
  EXPECT_EQ(std::string("bikrant"), request->get_account_id());
}

TEST_F(S3RequestObjectTest,
       ValidateContentLengthSetOnceOnlyForEmptySendResponse) {
  // Content-Length Header should be set only once
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Length"] = "123";
  input_headers["Host"] = "siddhi.s3.seagate.com";
  input_headers["User-Agent"] = "Curl";
  fake_in_headers(input_headers);
  EXPECT_CALL(*mock_evhtp_obj_ptr, http_header_new(_, _, _, _)).Times(1);

  EXPECT_CALL(*mock_evhtp_obj_ptr,
              http_header_new(StrEq("Content-Length"), _, _, _)).Times(1);
  EXPECT_CALL(*mock_evhtp_obj_ptr, http_headers_add_header(_, _)).Times(2);
  EXPECT_CALL(*mock_evhtp_obj_ptr, http_send_reply(_, _)).Times(1);

  // Set once like HEAD obect test
  request_with_mock_http_event->set_out_header_value("Content-Length", "0");

  request_with_mock_http_event->send_response(S3HttpSuccess200);
}

TEST_F(S3RequestObjectTest, ValidateContentLengthSendResponseOnce) {
  // Content-Length Header should be set only once
  std::map<std::string, std::string> input_headers;
  input_headers["Content-Length"] = "123";
  input_headers["Host"] = "siddhi.s3.seagate.com";
  input_headers["User-Agent"] = "Curl";
  fake_in_headers(input_headers);
  EXPECT_CALL(*mock_evhtp_obj_ptr, http_header_new(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_evhtp_obj_ptr,
              http_header_new(StrEq("Content-Length"), _, _, _)).Times(1);
  EXPECT_CALL(*mock_evhtp_obj_ptr, http_headers_add_header(_, _)).Times(2);
  EXPECT_CALL(*mock_evhtp_obj_ptr, http_send_reply(_, _)).Times(1);

  request_with_mock_http_event->send_response(S3HttpSuccess200);
}

TEST_F(S3RequestObjectTest, SetStartClientRequestReadTimeout) {
  EXPECT_CALL(*mock_event_obj_ptr, new_event(_, _, _, _, _)).Times(1);
  EXPECT_CALL(*mock_event_obj_ptr, add_event(_, _)).Times(1);
  request_with_mock_http_event->set_start_client_request_read_timeout();
  request_with_mock_http_event->free_client_read_timer();
}

TEST_F(S3RequestObjectTest, StopClientReadTimerNull) {
  request_with_mock_http_event->client_read_timer_event = NULL;
  EXPECT_CALL(*mock_event_obj_ptr, pending_event(_, _, _)).Times(0);
  EXPECT_CALL(*mock_event_obj_ptr, del_event(_)).Times(0);
  request_with_mock_http_event->stop_client_read_timer();
}

TEST_F(S3RequestObjectTest, StopClientReadTimer) {
  request_with_mock_http_event->client_read_timer_event =
      (struct event *)0xffff;
  EXPECT_CALL(*mock_event_obj_ptr, pending_event(_, _, _))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_event_obj_ptr, del_event(_)).Times(2);
  request_with_mock_http_event->stop_client_read_timer();
}

TEST_F(S3RequestObjectTest, FreeReadTimer) {
  request_with_mock_http_event->client_read_timer_event =
      (struct event *)0xffff;
  EXPECT_CALL(*mock_event_obj_ptr, free_event(_)).Times(1);
  request_with_mock_http_event->free_client_read_timer();
  EXPECT_TRUE(request_with_mock_http_event->client_read_timer_event == NULL);
}

TEST_F(S3RequestObjectTest, FreeReadTimerNull) {
  EXPECT_CALL(*mock_event_obj_ptr, free_event(_)).Times(0);
  request_with_mock_http_event->free_client_read_timer();
}
