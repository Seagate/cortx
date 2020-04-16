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

#include "s3_sha256.h"

S3sha256::S3sha256() { reset(); }

void S3sha256::reset() { status = SHA256_Init(&context); }

bool S3sha256::Update(const char *input, size_t length) {
  if (input == NULL) {
    return false;
  }
  if (status == 0) {
    return false;  // failure
  }
  status = SHA256_Update(&context, input, length);
  if (status == 0) {
    return false;  // failure
  }
  return true;  // success
}

bool S3sha256::Finalize() {
  status = SHA256_Final(hash, &context);
  if (status == 0) {
    return false;  // failure
  }

  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    sprintf((char *)(&hex_hash[i * 2]), "%02x", (int)hash[i]);
  }
  return true;
}

std::string S3sha256::get_hex_hash() {
  if (status == 0) {
    return std::string("");  // failure
  }
  return std::string(hex_hash);
}
