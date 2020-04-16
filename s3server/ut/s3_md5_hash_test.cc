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
 * Original creation date: 12-Nov-2015
 */

#include "s3_md5_hash.h"
#include "gtest/gtest.h"

TEST(MD5HashTest, Constructor) {
  MD5hash md5hashobj;
  EXPECT_NE(0, md5hashobj.status);
}

TEST(MD5HashTest, Update) {
  char input[] = "abcdefghijklmnopqrstuvwxyz";
  int ret;
  MD5hash md5hashobj;

  ret = md5hashobj.Update(input, sizeof(input));
  EXPECT_EQ(0, ret);

  ret = md5hashobj.Update(NULL, 5);
  EXPECT_NE(0, ret);
}

TEST(MD5HashTest, FinalBasic) {
  char input[] = "abcdefghijklmnopqrstuvwxyz";
  int ret;
  MD5hash md5hashobj;

  md5hashobj.Update(input, 26);
  ret = md5hashobj.Finalize();
  EXPECT_EQ(0, ret);
  // MD5 value from RFC 1321
  // https://www.ietf.org/rfc/rfc1321.txt
  std::string s_hash = md5hashobj.get_md5_string();
  EXPECT_STREQ("c3fcd3d76192e4007dfb496cca67e13b", s_hash.c_str());
}

TEST(MD5HashTest, FinalEmptyStr) {
  char input[] = "";
  int ret;
  MD5hash md5hashobj;
  md5hashobj.Update(input, 0);
  ret = md5hashobj.Finalize();
  EXPECT_EQ(0, ret);
  std::string s_hash = md5hashobj.get_md5_string();
  EXPECT_STREQ("d41d8cd98f00b204e9800998ecf8427e", s_hash.c_str());
}

TEST(MD5HashTest, FinalNumeral) {
  char input[] =
      "123456789012345678901234567890123456789012345678901234567890123456789012"
      "34567890";
  int ret;
  MD5hash md5hashobj;
  md5hashobj.Update(input, 80);
  ret = md5hashobj.Finalize();
  EXPECT_EQ(0, ret);
  std::string s_hash = md5hashobj.get_md5_string();
  EXPECT_STREQ("57edf4a22be3c955ac49da2e2107b67a", s_hash.c_str());
}

TEST(MD5HashTest, GetMD5) {
  char input[] = "abcdefghijklmnopqrstuvwxyz";
  MD5hash md5hashobj;
  md5hashobj.Update(input, 26);
  md5hashobj.Finalize();
  std::string str = md5hashobj.get_md5_string();
  EXPECT_STREQ("c3fcd3d76192e4007dfb496cca67e13b", str.c_str());
}
