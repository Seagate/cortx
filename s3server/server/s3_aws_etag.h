/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original creation date: 5-Feb-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_AWS_ETAG_H__
#define __S3_SERVER_S3_AWS_ETAG_H__

#include <gtest/gtest_prod.h>
#include <string>
#include "s3_log.h"

// Used to generate Etag for multipart uploads.
class S3AwsEtag {
  std::string hex_etag;
  std::string final_etag;
  int part_count;

  // Helpers
  int hex_to_dec(char ch);
  std::string convert_hex_bin(std::string hex);

 public:
  S3AwsEtag() : part_count(0) {}

  void add_part_etag(std::string etag);
  std::string finalize();
  std::string get_final_etag();
  FRIEND_TEST(S3AwsEtagTest, Constructor);
  FRIEND_TEST(S3AwsEtagTest, HexToDec);
  FRIEND_TEST(S3AwsEtagTest, HexToDecInvalid);
  FRIEND_TEST(S3AwsEtagTest, HexToBinary);
  FRIEND_TEST(S3AwsEtagTest, AddPartEtag);
  FRIEND_TEST(S3AwsEtagTest, Finalize);
  FRIEND_TEST(S3AwsEtagTest, GetFinalEtag);
};

#endif
