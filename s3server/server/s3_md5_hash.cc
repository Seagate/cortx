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
 * Original creation date: 1-Oct-2015
 */

#include "base64.h"

#include "s3_md5_hash.h"

MD5hash::MD5hash() { status = MD5_Init(&md5ctx); }

int MD5hash::Update(const char *input, size_t length) {
  if (input == NULL) {
    return -1;
  }
  if (status > 0) {
    status = MD5_Update(&md5ctx, input, length);
  }
  if (status < 1) {
    return -1;  // failure
  }
  return 0;  // success
}

int MD5hash::Finalize() {
  if (is_finalized) {
    return 0;
  }
  if (status > 0) {
    status = MD5_Final(md5_digest, &md5ctx);
  }
  if (status < 1) {
    return -1;  // failure
  }
  is_finalized = true;
  return 0;
}

const char hex_tbl[] = "0123456789abcdef";

std::string MD5hash::get_md5_string() {
  if (Finalize() < 0) {
    return std::string();  // failure
  }
  std::string s_hex;
  s_hex.reserve(MD5_DIGEST_LENGTH * 2);

  for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
    const unsigned ch = md5_digest[i] & 255;

    s_hex += hex_tbl[ch >> 4];
    s_hex += hex_tbl[ch & 15];
  }
  return s_hex;
}

std::string MD5hash::get_md5_base64enc_string() {
  if (Finalize() < 0) {
    return std::string();  // failure
  }
  return base64_encode(md5_digest, MD5_DIGEST_LENGTH);
}
