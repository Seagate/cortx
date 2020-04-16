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
 * Original author:  Rajesh Nambiar   <rajesh.nambiarr@seagate.com>
 * Original creation date: 31-Jan-2017
 */

#include "s3_url_encode.h"
#include "gtest/gtest.h"

TEST(S3urlencodeTest, EscapeChar) {
  std::string destination;
  escape_char(' ', destination);
  EXPECT_EQ("%20", destination);
}

TEST(S3urlencodeTest, IsEncodingNeeded) {
  EXPECT_TRUE(char_needs_url_encoding(' '));
  EXPECT_TRUE(char_needs_url_encoding('/'));
  EXPECT_FALSE(char_needs_url_encoding('A'));
}

TEST(S3urlencodeTest, urlEncode) {
  EXPECT_EQ("http%3A%2F%2Ftest%20this", url_encode("http://test this"));
  EXPECT_EQ("abcd", url_encode("abcd"));
}

TEST(S3urlencodeTest, InvalidUrlEncode) {
  EXPECT_EQ("", url_encode(NULL));
  EXPECT_EQ("", url_encode(""));
}
