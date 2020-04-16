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
 * Original creation date: 15-Mar-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_SHA256_H__
#define __S3_SERVER_S3_SHA256_H__

#include <assert.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <string>

class S3sha256 {
 private:
  unsigned char hash[SHA256_DIGEST_LENGTH] = {'\0'};
  char hex_hash[SHA256_DIGEST_LENGTH * 2] = {'\0'};
  SHA256_CTX context;
  int status;

 public:
  S3sha256();
  void reset();
  bool Update(const char *input, size_t length);
  bool Finalize();

  std::string get_hex_hash();
};
#endif
