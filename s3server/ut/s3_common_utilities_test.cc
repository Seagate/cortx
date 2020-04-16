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
 * Original author:  Prashanth Vanaparthy  <prashanth.vanaparthy@seagate.com>
 * Original creation date: 21-Aug-2018
 */

#include <gtest/gtest.h>
#include "s3_common_utilities.h"

class S3CommonUtilitiesTest : public testing::Test {};

TEST_F(S3CommonUtilitiesTest, StringHasOnlyDigits) {
  std::string input_string = "123456";
  EXPECT_TRUE(S3CommonUtilities::string_has_only_digits(input_string));
}

TEST_F(S3CommonUtilitiesTest, StringHasChar) {
  std::string input_string = "String";
  EXPECT_FALSE(S3CommonUtilities::string_has_only_digits(input_string));
}

TEST_F(S3CommonUtilitiesTest, StringHasCharAndNumericValues) {
  std::string input_string = "String123";
  EXPECT_FALSE(S3CommonUtilities::string_has_only_digits(input_string));
}

TEST_F(S3CommonUtilitiesTest, StringHasNumericValuesAndSpace) {
  std::string input_string = " 123 ";
  EXPECT_FALSE(S3CommonUtilities::string_has_only_digits(input_string));
}

TEST_F(S3CommonUtilitiesTest, StringHasNumericValuesAndSpecialChars) {
  std::string input_string = "*123?";
  EXPECT_FALSE(S3CommonUtilities::string_has_only_digits(input_string));
}

TEST_F(S3CommonUtilitiesTest, LeftTrimOfStringWithSpaces) {
  std::string input_string = "    teststring";
  EXPECT_STREQ("teststring", S3CommonUtilities::ltrim(input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, LeftTrimOfStringWithTabs) {
  std::string input_string = "\tteststring";
  EXPECT_STREQ("teststring", S3CommonUtilities::ltrim(input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, LeftTrimOfStringWithCarriageReturnAndNewline) {
  std::string input_string = "\r\nteststring";
  EXPECT_STREQ("teststring", S3CommonUtilities::ltrim(input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, LeftTrimOfStringWithNewline) {
  std::string input_string = "\nteststring";
  EXPECT_STREQ("teststring", S3CommonUtilities::ltrim(input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, LeftTrimOfString) {
  std::string input_string = "teststring";
  EXPECT_STREQ("teststring", S3CommonUtilities::ltrim(input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, RightTrimOfStringWithSpaces) {
  std::string input_string = "teststring    ";
  EXPECT_STREQ("teststring", S3CommonUtilities::rtrim(input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, RightTrimOfStringWithTabs) {
  std::string input_string = "teststring\t";
  EXPECT_STREQ("teststring", S3CommonUtilities::rtrim(input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, RightTrimOfStringWithCarriageReturnAndNewline) {
  std::string input_string = "teststring\r\n";
  EXPECT_STREQ("teststring", S3CommonUtilities::rtrim(input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, RightTrimOfStringWithNewline) {
  std::string input_string = "teststring\n";
  EXPECT_STREQ("teststring", S3CommonUtilities::rtrim(input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, RightTrimOfString) {
  std::string input_string = "teststring";
  EXPECT_STREQ("teststring", S3CommonUtilities::rtrim(input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, TrimOfStringLeftAndRight) {
  std::string input_string = "   test string   ";
  EXPECT_STREQ("test string", S3CommonUtilities::trim(input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, S3xmlEncodeSpecialCharsTest1) {
  std::string input_string = "abc&d";
  EXPECT_STREQ("abc&amp;d", S3CommonUtilities::s3xmlEncodeSpecialChars(
                                input_string).c_str());
  EXPECT_STRNE("abc&d", S3CommonUtilities::s3xmlEncodeSpecialChars(input_string)
                            .c_str());
}

TEST_F(S3CommonUtilitiesTest, S3xmlEncodeSpecialCharsTest2) {
  std::string input_string = "abc<d";
  EXPECT_STREQ("abc&lt;d", S3CommonUtilities::s3xmlEncodeSpecialChars(
                               input_string).c_str());
  EXPECT_STRNE("abc<d", S3CommonUtilities::s3xmlEncodeSpecialChars(input_string)
                            .c_str());
}

TEST_F(S3CommonUtilitiesTest, S3xmlEncodeSpecialCharsTest3) {
  std::string input_string = "abc>d";
  EXPECT_STREQ("abc&gt;d", S3CommonUtilities::s3xmlEncodeSpecialChars(
                               input_string).c_str());
  EXPECT_STRNE("abc>d", S3CommonUtilities::s3xmlEncodeSpecialChars(input_string)
                            .c_str());
}

TEST_F(S3CommonUtilitiesTest, S3xmlEncodeSpecialCharsTest4) {
  std::string input_string = "abc\"d";
  EXPECT_STREQ("abc&quot;d", S3CommonUtilities::s3xmlEncodeSpecialChars(
                                 input_string).c_str());
  EXPECT_STRNE("abc\"d", S3CommonUtilities::s3xmlEncodeSpecialChars(
                             input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, S3xmlEncodeSpecialCharsTest5) {
  std::string input_string = "abcd\r";
  EXPECT_STREQ("abcd&#13;", S3CommonUtilities::s3xmlEncodeSpecialChars(
                                input_string).c_str());
  EXPECT_STRNE("abcd\r", S3CommonUtilities::s3xmlEncodeSpecialChars(
                             input_string).c_str());
}
TEST_F(S3CommonUtilitiesTest, S3xmlEncodeSpecialCharsTest6) {
  std::string input_string = "";
  EXPECT_STREQ(
      "", S3CommonUtilities::s3xmlEncodeSpecialChars(input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, S3xmlEncodeSpecialCharsTest7) {
  std::string input_string = "abcd";
  EXPECT_STREQ(
      "abcd", S3CommonUtilities::s3xmlEncodeSpecialChars(input_string).c_str());
}

TEST_F(S3CommonUtilitiesTest, FindAndReplaceallTest1) {
  std::string input_string = "test&string";
  std::string search_string = "&";
  std::string replace_string = "&amp;";
  S3CommonUtilities::find_and_replaceall(input_string, search_string,
                                         replace_string);
  EXPECT_STREQ("test&amp;string", input_string.c_str());
}

TEST_F(S3CommonUtilitiesTest, FindAndReplaceallWithEmptySearchString) {
  std::string input_string = "test&string";
  std::string search_string = "";
  std::string replace_string = "&amp;";
  S3CommonUtilities::find_and_replaceall(input_string, search_string,
                                         replace_string);
  // no chaange in input string
  EXPECT_STREQ("test&string", input_string.c_str());
}

TEST_F(S3CommonUtilitiesTest, FindAndReplaceallWithEmptyReplaceString) {
  std::string input_string = "test&string";
  std::string search_string = "&";
  std::string replace_string = "";
  S3CommonUtilities::find_and_replaceall(input_string, search_string,
                                         replace_string);
  EXPECT_STREQ("teststring", input_string.c_str());
}

TEST_F(S3CommonUtilitiesTest, FindAndReplaceallWithLargeReplaceString) {
  std::string input_string = "test&string";
  std::string search_string = "&";
  std::string replace_string = "replacestring_large";
  S3CommonUtilities::find_and_replaceall(input_string, search_string,
                                         replace_string);
  EXPECT_STREQ("testreplacestring_largestring", input_string.c_str());
}

TEST_F(S3CommonUtilitiesTest,
       FindAndReplaceallReplaceStringSmallerThanSearchString) {
  std::string input_string = "test&string";
  std::string search_string = "&string";
  std::string replace_string = "&";
  S3CommonUtilities::find_and_replaceall(input_string, search_string,
                                         replace_string);
  EXPECT_STREQ("test&", input_string.c_str());
}

TEST_F(S3CommonUtilitiesTest, FindAndReplaceallReplaceStringAtMultiplePlaces) {
  std::string input_string = "&test&string&abcd&";
  std::string search_string = "&";
  std::string replace_string = "%26";
  S3CommonUtilities::find_and_replaceall(input_string, search_string,
                                         replace_string);
  EXPECT_STREQ("%26test%26string%26abcd%26", input_string.c_str());
}

TEST_F(S3CommonUtilitiesTest,
       FindAndReplaceallEmptyReplaceStringAtMultiplePlaces) {
  std::string input_string = "&test&string&abcd&";
  std::string search_string = "&";
  std::string replace_string = "";
  S3CommonUtilities::find_and_replaceall(input_string, search_string,
                                         replace_string);
  EXPECT_STREQ("teststringabcd", input_string.c_str());
}

TEST_F(S3CommonUtilitiesTest, FindAndReplaceallReplaceString2) {
  std::string input_string = "test";
  std::string search_string = "test";
  std::string replace_string = "testtest";
  S3CommonUtilities::find_and_replaceall(input_string, search_string,
                                         replace_string);
  EXPECT_STREQ("testtest", input_string.c_str());
}

TEST_F(S3CommonUtilitiesTest, FindAndReplaceallReplaceString3) {
  std::string input_string = "&";
  std::string search_string = "&";
  std::string replace_string = "&&";
  S3CommonUtilities::find_and_replaceall(input_string, search_string,
                                         replace_string);
  EXPECT_STREQ("&&", input_string.c_str());
}

TEST_F(S3CommonUtilitiesTest, FindAndReplaceallReplaceString4) {
  std::string input_string = "test";
  std::string search_string = "test";
  std::string replace_string = "";
  S3CommonUtilities::find_and_replaceall(input_string, search_string,
                                         replace_string);
  EXPECT_STREQ("", input_string.c_str());
}

TEST_F(S3CommonUtilitiesTest, IsYamlValueNullTest1) {
  std::string input_string = "~";
  EXPECT_TRUE(S3CommonUtilities::is_yaml_value_null(input_string));
}

TEST_F(S3CommonUtilitiesTest, IsYamlValueNullTest2) {
  std::string input_string = "Null";
  EXPECT_TRUE(S3CommonUtilities::is_yaml_value_null(input_string));
}

TEST_F(S3CommonUtilitiesTest, IsYamlValueNullTest3) {
  std::string input_string = "null";
  EXPECT_TRUE(S3CommonUtilities::is_yaml_value_null(input_string));
}

TEST_F(S3CommonUtilitiesTest, IsYamlValueNullTest4) {
  std::string input_string = "NULL";
  EXPECT_TRUE(S3CommonUtilities::is_yaml_value_null(input_string));
}

TEST_F(S3CommonUtilitiesTest, IsYamlValueNullTest5) {
  std::string input_string = "abcd";
  EXPECT_FALSE(S3CommonUtilities::is_yaml_value_null(input_string));
}

TEST_F(S3CommonUtilitiesTest, IsYamlValueNullTest6) {
  std::string input_string = "";
  EXPECT_FALSE(S3CommonUtilities::is_yaml_value_null(input_string));
}
