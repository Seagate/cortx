
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
 * Original author:  Swapnil Belapurkar <swapnil.belapurkar@seagate.com>
 * Original creation date: 10-May-2017
 */

#include "s3_auth_response_error.h"
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <memory>
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::Eq;
using ::testing::StrEq;

class S3AuthResponseErrorTest : public testing::Test {
 protected:
  S3AuthResponseErrorTest() {
    response_under_test = std::make_shared<S3AuthResponseError>(auth_error_xml);
  }
  std::shared_ptr<S3AuthResponseError> response_under_test;
  std::string auth_error_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" "
      "standalone=\"no\"?><ErrorResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/"
      "\"><Error><Code>SignatureDoesNotMatch</"
      "Code><Message>UnitTestErrorMessage.</Message>"
      "</Error><RequestId>1111</RequestId></ErrorResponse>";
};

// TO validate if internal fields of S3AuthResponseError are correctly set.
TEST_F(S3AuthResponseErrorTest, ConstructorTest) {
  EXPECT_TRUE(response_under_test->is_valid);
}

// To verify if error code field is correctly set.
TEST_F(S3AuthResponseErrorTest, GetErrorCode) {
  EXPECT_STREQ(response_under_test->get_code().c_str(),
               "SignatureDoesNotMatch");
}

// To verify if error message field is correctly set.
TEST_F(S3AuthResponseErrorTest, GetErrorMessage) {
  EXPECT_STREQ(response_under_test->get_message().c_str(),
               "UnitTestErrorMessage.");
}

// To verify if request id field is correctly set.
TEST_F(S3AuthResponseErrorTest, GetRequestId) {
  EXPECT_STREQ(response_under_test->get_request_id().c_str(), "1111");
}

// Test with empty XML.
TEST_F(S3AuthResponseErrorTest, ValidateMustFailForEmptyXML) {
  std::string empty_string("");
  response_under_test = std::make_shared<S3AuthResponseError>(empty_string);
  EXPECT_FALSE(response_under_test->is_valid);
}

// Test with invalid XML.
TEST_F(S3AuthResponseErrorTest, ValidateMustFailForInvalidXML) {
  std::string auth_error_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" "
      "standalone=\"no\"?><ErrorResponse ";
  response_under_test = std::make_shared<S3AuthResponseError>(auth_error_xml);
  EXPECT_FALSE(response_under_test->is_valid);
}

// Test with no error code field.
TEST_F(S3AuthResponseErrorTest, ValidateMustFailForEmptyErrorCode) {
  std::string auth_error_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" "
      "standalone=\"no\"?><ErrorResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/"
      "\"><Error><Code></"
      "Code><Message>UnitTestErrorMessage.</Message>"
      "</Error><RequestId>1111</RequestId></ErrorResponse>";
  response_under_test = std::make_shared<S3AuthResponseError>(auth_error_xml);
  EXPECT_FALSE(response_under_test->is_valid);
}

// Test with no error code field.
TEST_F(S3AuthResponseErrorTest, ValidateMustFailForMissingErrorCode) {
  std::string auth_error_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" "
      "standalone=\"no\"?><ErrorResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/"
      "\"><Error><NoCode></"
      "NoCode><Message>UnitTestErrorMessage.</Message>"
      "</Error><RequestId>1111</RequestId></ErrorResponse>";
  response_under_test = std::make_shared<S3AuthResponseError>(auth_error_xml);
  EXPECT_FALSE(response_under_test->is_valid);
}
