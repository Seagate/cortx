/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original author:  Abrarahmed Momin   <abrar.habib@seagate.com>
 * Original creation date: April-18-2017
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_factory.h"
#include "mock_s3_request_object.h"
#include "s3_put_fi_action.h"

using ::testing::Invoke;
using ::testing::AtLeast;
using ::testing::ReturnRef;

#define SETUP_FI_CHECKS_SUCCESS(FIPARAM)                                       \
  do {                                                                         \
    MockHeaderKeyVal.assign(FIPARAM);                                          \
    EXPECT_CALL(*(request_mock), get_header_value("x-seagate-faultinjection")) \
        .Times(AtLeast(1))                                                     \
        .WillRepeatedly(Return(MockHeaderKeyVal));                             \
    EXPECT_CALL(*request_mock, resume(_)).Times(1);                            \
    EXPECT_CALL(*request_mock, send_response(200, _)).Times(AtLeast(1));       \
  } while (0)

#define SETUP_FI_CHECKS_FAILED(FIPARAM)                                        \
  do {                                                                         \
    MockHeaderKeyVal.assign(FIPARAM);                                          \
    EXPECT_CALL(*(request_mock), get_header_value("x-seagate-faultinjection")) \
        .Times(AtLeast(1))                                                     \
        .WillRepeatedly(Return(MockHeaderKeyVal));                             \
    EXPECT_CALL(*request_mock, resume(_)).Times(1);                            \
    EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));  \
    EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));       \
  } while (0)

class S3PutFiActionTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.
  S3PutFiActionTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    request_mock = std::make_shared<MockS3RequestObject>(req, evhtp_obj_ptr);
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*request_mock, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    action_under_test_ptr = std::make_shared<S3PutFiAction>(request_mock);
  }

  std::shared_ptr<MockS3RequestObject> request_mock;
  std::shared_ptr<S3PutFiAction> action_under_test_ptr;
  std::string MockHeaderKeyVal;
};

TEST_F(S3PutFiActionTest, Constructor) {
  EXPECT_NE(0, action_under_test_ptr->number_of_tasks());
}

TEST_F(S3PutFiActionTest, SetFaultInjectionCmdEmpty) {
  SETUP_FI_CHECKS_FAILED("");

  action_under_test_ptr->set_fault_injection();
  EXPECT_STREQ("MalformedFICmd",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3PutFiActionTest, SetFaultInjectionTooManyParam) {
  SETUP_FI_CHECKS_FAILED("enable,always,testfp,param1,param2,param3");

  action_under_test_ptr->set_fault_injection();
  EXPECT_STREQ("MalformedFICmd",
               action_under_test_ptr->get_s3_error_code().c_str());
}

TEST_F(S3PutFiActionTest, SetFaultInjectionEnableAlways) {
  SETUP_FI_CHECKS_SUCCESS("enable,always,testfp");
  action_under_test_ptr->set_fault_injection();
}

TEST_F(S3PutFiActionTest, SetFaultInjectionEnableOnce) {
  SETUP_FI_CHECKS_SUCCESS("enable,once,testfp");
  action_under_test_ptr->set_fault_injection();
}

TEST_F(S3PutFiActionTest, SetFaultInjectionEnableRandom) {
  SETUP_FI_CHECKS_SUCCESS("enable,random,testfp,10");
  action_under_test_ptr->set_fault_injection();
}

TEST_F(S3PutFiActionTest, SetFaultInjectionEnableNTime) {
  SETUP_FI_CHECKS_SUCCESS("enable,enablen,testfp,10");
  action_under_test_ptr->set_fault_injection();
}

TEST_F(S3PutFiActionTest, SetFaultInjectionEnableOffN) {
  SETUP_FI_CHECKS_SUCCESS("enable,offnonm,testfp,10,10");
  action_under_test_ptr->set_fault_injection();
}

TEST_F(S3PutFiActionTest, SetFaultInjectionDisable) {
  SETUP_FI_CHECKS_SUCCESS("disable,noop,testfp");
  action_under_test_ptr->set_fault_injection();
}

TEST_F(S3PutFiActionTest, SetFaultInjectionTest) {
  SETUP_FI_CHECKS_SUCCESS("test,noop,testfp");
  action_under_test_ptr->set_fault_injection();
}

TEST_F(S3PutFiActionTest, SendResponseToClientSuccess) {
  EXPECT_CALL(*request_mock, resume(_)).Times(1);
  EXPECT_CALL(*request_mock, send_response(200, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}

TEST_F(S3PutFiActionTest, SendResponseToClientMalformedFICmd) {
  action_under_test_ptr->set_s3_error("MalformedFICmd");
  EXPECT_CALL(*request_mock, resume(_)).Times(1);
  EXPECT_CALL(*request_mock, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request_mock, send_response(400, _)).Times(AtLeast(1));
  action_under_test_ptr->send_response_to_s3_client();
}
