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
 * Original creation date: 30-Jan-2017
 */

#include "s3_aws_etag.h"
#include "gtest/gtest.h"

class S3AwsEtagTest : public testing::Test {
 protected:
  void SetUp() { s3AwsEtag_ptr = new S3AwsEtag(); }

  void TearDown() { delete s3AwsEtag_ptr; }

  S3AwsEtag *s3AwsEtag_ptr;
};

TEST_F(S3AwsEtagTest, Constructor) {
  EXPECT_EQ(0, s3AwsEtag_ptr->part_count);
  EXPECT_EQ("", s3AwsEtag_ptr->hex_etag);
}

TEST_F(S3AwsEtagTest, HexToDec) {
  EXPECT_EQ(0, s3AwsEtag_ptr->hex_to_dec('0'));
  EXPECT_EQ(1, s3AwsEtag_ptr->hex_to_dec('1'));
  EXPECT_EQ(10, s3AwsEtag_ptr->hex_to_dec('A'));
  EXPECT_EQ(11, s3AwsEtag_ptr->hex_to_dec('B'));
  EXPECT_EQ(10, s3AwsEtag_ptr->hex_to_dec('a'));
}

TEST_F(S3AwsEtagTest, HexToDecInvalid) {
  EXPECT_EQ(-1, s3AwsEtag_ptr->hex_to_dec('*'));
}

TEST_F(S3AwsEtagTest, HexToBinary) {
  std::string binary = s3AwsEtag_ptr->convert_hex_bin("abc");
  EXPECT_NE("", binary.c_str());
}

TEST_F(S3AwsEtagTest, AddPartEtag) {
  s3AwsEtag_ptr->hex_etag = "c1d9";
  s3AwsEtag_ptr->add_part_etag("abcd");
  EXPECT_EQ("c1d9abcd", s3AwsEtag_ptr->hex_etag);
  EXPECT_EQ(1, s3AwsEtag_ptr->part_count);
}

TEST_F(S3AwsEtagTest, Finalize) {
  std::string final_etag;
  int part_num_delimiter;
  s3AwsEtag_ptr->hex_etag = "c1d9";
  final_etag = s3AwsEtag_ptr->finalize();
  part_num_delimiter = final_etag.find("-");
  EXPECT_NE(std::string::npos, part_num_delimiter);
  EXPECT_EQ(s3AwsEtag_ptr->part_count,
            atoi(final_etag.substr(part_num_delimiter + 1).c_str()));

  s3AwsEtag_ptr->add_part_etag("abcd");
  final_etag = s3AwsEtag_ptr->finalize();
  part_num_delimiter = final_etag.find("-");
  EXPECT_NE(std::string::npos, part_num_delimiter);
  EXPECT_EQ(s3AwsEtag_ptr->part_count,
            atoi(final_etag.substr(part_num_delimiter + 1).c_str()));
}

TEST_F(S3AwsEtagTest, GetFinalEtag) {
  std::string final_etag;
  s3AwsEtag_ptr->hex_etag = "c1d9";
  final_etag = s3AwsEtag_ptr->finalize();
  EXPECT_NE("", final_etag.c_str());
}
