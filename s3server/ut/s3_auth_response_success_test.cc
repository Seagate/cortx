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
 * Original author:  Swapnil Belapurkar  <swapnil.belapurkar@seagate.com>
 * Original creation date: 16-May-2017
 */

#include "s3_auth_response_success.h"
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <memory>
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::Eq;
using ::testing::StrEq;

class S3AuthResponseSuccessTest : public testing::Test {
 protected:
  S3AuthResponseSuccessTest() {
    auth_response_under_test =
        std::make_shared<S3AuthResponseSuccess>(auth_success_xml);
  }

  std::shared_ptr<S3AuthResponseSuccess> auth_response_under_test;
  std::string auth_success_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>"
      "<AuthenticateUserResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
      "<AuthenticateUserResult><UserId>123</UserId><UserName>tester</UserName>"
      "<AccountId>12345</AccountId><AccountName>s3_test</AccountName>"
      "<SignatureSHA256>UvRt9pMPxbsSfHzEi9CIBOmDxJs=</SignatureSHA256></"
      "AuthenticateUserResult>"
      "<ResponseMetadata><RequestId>1111</RequestId></ResponseMetadata></"
      "AuthenticateUserResponse>";
};

// To validate if constructor has initialized internal field
// correctly for valid XML.
TEST_F(S3AuthResponseSuccessTest, ConstructorTest) {
  EXPECT_TRUE(auth_response_under_test->isOK());
}

// Validate uer name retrieved from XML.
TEST_F(S3AuthResponseSuccessTest, GetUserName) {
  EXPECT_EQ(auth_response_under_test->get_user_name(), "tester");
}

// Validate user id parsed from XML.
TEST_F(S3AuthResponseSuccessTest, GetUserId) {
  EXPECT_EQ(auth_response_under_test->get_user_id(), "123");
}

// Validate account name parsed from XML.
TEST_F(S3AuthResponseSuccessTest, GetAccountName) {
  EXPECT_EQ(auth_response_under_test->get_account_name(), "s3_test");
}

// Validate account id parsed from XML.
TEST_F(S3AuthResponseSuccessTest, GetAccountId) {
  EXPECT_EQ(auth_response_under_test->get_account_id(), "12345");
}

// Validate SHA256 signature parsed from XML.
TEST_F(S3AuthResponseSuccessTest, GetSignatureSHA256) {
  EXPECT_EQ(auth_response_under_test->get_signature_sha256(),
            "UvRt9pMPxbsSfHzEi9CIBOmDxJs=");
}

// Validate request id parsed from xml.
TEST_F(S3AuthResponseSuccessTest, GetRequestId) {
  EXPECT_EQ(auth_response_under_test->get_request_id(), "1111");
}

// Test with empty XML.
TEST_F(S3AuthResponseSuccessTest, ValidateMustFailForEmptyXML) {
  std::string empty_string("");
  auth_response_under_test =
      std::make_shared<S3AuthResponseSuccess>(empty_string);
  EXPECT_FALSE(auth_response_under_test->isOK());
}

// Test with invalid XML.
TEST_F(S3AuthResponseSuccessTest, ValidateMustFailForInvalidXML) {
  std::string invalid_auth_success_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" "
      "standalone=\"no\"?><ErrorResponse ";
  auth_response_under_test =
      std::make_shared<S3AuthResponseSuccess>(invalid_auth_success_xml);
  EXPECT_FALSE(auth_response_under_test->isOK());
}

// Test with no error code field.
TEST_F(S3AuthResponseSuccessTest, ValidateMustFailForEmptyUserName) {
  std::string invalid_auth_success_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>"
      "<AuthenticateUserResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
      "<AuthenticateUserResult><UserId>123</UserId>"
      "<AccountId>12345</AccountId><AccountName>s3_test</AccountName>"
      "<SignatureSHA256>UvRt9pMPxbsSfHzEi9CIBOmDxJs=</SignatureSHA256></"
      "AuthenticateUserResult>"
      "<ResponseMetadata><RequestId>1111</RequestId></ResponseMetadata></"
      "AuthenticateUserResponse>";
  auth_response_under_test =
      std::make_shared<S3AuthResponseSuccess>(invalid_auth_success_xml);
  EXPECT_FALSE(auth_response_under_test->isOK());
}

// Test with no error code field.
TEST_F(S3AuthResponseSuccessTest, ValidateMustFailForMissingUserName) {
  std::string invalid_auth_success_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>"
      "<AuthenticateUserResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
      "<AuthenticateUserResult><UserId>123</UserId><UserName></UserName>"
      "<AccountId>12345</AccountId><AccountName>s3_test</AccountName>"
      "<SignatureSHA256>UvRt9pMPxbsSfHzEi9CIBOmDxJs=</SignatureSHA256></"
      "AuthenticateUserResult>"
      "<ResponseMetadata><RequestId>1111</RequestId></ResponseMetadata></"
      "AuthenticateUserResponse>";
  auth_response_under_test =
      std::make_shared<S3AuthResponseSuccess>(invalid_auth_success_xml);
  EXPECT_FALSE(auth_response_under_test->isOK());
}

// Test with no error code field.
TEST_F(S3AuthResponseSuccessTest, ValidateMustFailForEmptyUserId) {
  std::string invalid_auth_success_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>"
      "<AuthenticateUserResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
      "<AuthenticateUserResult><UserId></UserId><UserName>tester</UserName>"
      "<AccountId>12345</AccountId><AccountName>s3_test</AccountName>"
      "<SignatureSHA256>UvRt9pMPxbsSfHzEi9CIBOmDxJs=</SignatureSHA256></"
      "AuthenticateUserResult>"
      "<ResponseMetadata><RequestId>1111</RequestId></ResponseMetadata></"
      "AuthenticateUserResponse>";
  auth_response_under_test =
      std::make_shared<S3AuthResponseSuccess>(invalid_auth_success_xml);
  EXPECT_FALSE(auth_response_under_test->isOK());
}

// Test with no error code field.
TEST_F(S3AuthResponseSuccessTest, ValidateMustFailForMissingUserId) {
  std::string invalid_auth_success_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>"
      "<AuthenticateUserResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
      "<AuthenticateUserResult><UserName>tester</UserName>"
      "<AccountId>12345</AccountId><AccountName>s3_test</AccountName>"
      "<SignatureSHA256>UvRt9pMPxbsSfHzEi9CIBOmDxJs=</SignatureSHA256></"
      "AuthenticateUserResult>"
      "<ResponseMetadata><RequestId>1111</RequestId></ResponseMetadata></"
      "AuthenticateUserResponse>";
  auth_response_under_test =
      std::make_shared<S3AuthResponseSuccess>(invalid_auth_success_xml);
  EXPECT_FALSE(auth_response_under_test->isOK());
}

// Test with no error code field.
TEST_F(S3AuthResponseSuccessTest, ValidateMustFailForEmptyAccountName) {
  std::string invalid_auth_success_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>"
      "<AuthenticateUserResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
      "<AuthenticateUserResult><UserId>123</UserId><UserName>tester</UserName>"
      "<AccountId>12345</AccountId><AccountName></AccountName>"
      "<SignatureSHA256>UvRt9pMPxbsSfHzEi9CIBOmDxJs=</SignatureSHA256></"
      "AuthenticateUserResult>"
      "<ResponseMetadata><RequestId>1111</RequestId></ResponseMetadata></"
      "AuthenticateUserResponse>";
  auth_response_under_test =
      std::make_shared<S3AuthResponseSuccess>(invalid_auth_success_xml);
  EXPECT_FALSE(auth_response_under_test->isOK());
}

// Test with no error code field.
TEST_F(S3AuthResponseSuccessTest, ValidateMustFailForMissingAccountName) {
  std::string invalid_auth_success_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>"
      "<AuthenticateUserResponse "
      "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
      "<AuthenticateUserResult><UserId>123</UserId><UserName>tester</UserName>"
      "<AccountId>12345</AccountId>"
      "<SignatureSHA256>UvRt9pMPxbsSfHzEi9CIBOmDxJs=</SignatureSHA256></"
      "AuthenticateUserResult>"
      "<ResponseMetadata><RequestId>1111</RequestId></ResponseMetadata></"
      "AuthenticateUserResponse>";
  auth_response_under_test =
      std::make_shared<S3AuthResponseSuccess>(invalid_auth_success_xml);
  EXPECT_FALSE(auth_response_under_test->isOK());
}
