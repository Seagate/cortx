/*
 * COPYRIGHT 2019 SEAGATE LLC
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
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 30-JULY-2019
 */

#include "s3_m0_uint128_helper.h"
#include "base64.h"

std::pair<std::string, std::string> S3M0Uint128Helper::to_string_pair(
    const m0_uint128 &id) {
  return std::make_pair(
      base64_encode((unsigned char const *)&id.u_hi, sizeof(id.u_hi)),
      base64_encode((unsigned char const *)&id.u_lo, sizeof(id.u_lo)));
}

std::string S3M0Uint128Helper::to_string(const m0_uint128 &id) {
  return base64_encode((unsigned char const *)&id.u_hi, sizeof(id.u_hi)) + "-" +
         base64_encode((unsigned char const *)&id.u_lo, sizeof(id.u_lo));
}

m0_uint128 S3M0Uint128Helper::to_m0_uint128(const std::string &id_u_lo,
                                            const std::string &id_u_hi) {
  m0_uint128 id = {0ULL, 0ULL};
  std::string dec_id_u_hi = base64_decode(id_u_hi);
  std::string dec_id_u_lo = base64_decode(id_u_lo);
  if ((dec_id_u_hi.size() == sizeof(id.u_hi)) &&
      (dec_id_u_lo.size() == sizeof(id.u_lo))) {
    memcpy((void *)&id.u_hi, dec_id_u_hi.c_str(), sizeof(id.u_hi));
    memcpy((void *)&id.u_lo, dec_id_u_lo.c_str(), sizeof(id.u_lo));
  }
  return id;
}

m0_uint128 S3M0Uint128Helper::to_m0_uint128(const std::string &id_str) {
  m0_uint128 id = {0ULL, 0ULL};
  std::size_t delim_pos = id_str.find("-");
  if (delim_pos != std::string::npos) {
    std::string dec_id_u_hi = base64_decode(id_str.substr(0, delim_pos));
    std::string dec_id_u_lo = base64_decode(id_str.substr(delim_pos + 1));
    if ((dec_id_u_hi.size() == sizeof(id.u_hi)) &&
        (dec_id_u_lo.size() == sizeof(id.u_lo))) {
      memcpy((void *)&id.u_hi, dec_id_u_hi.c_str(), sizeof(id.u_hi));
      memcpy((void *)&id.u_lo, dec_id_u_lo.c_str(), sizeof(id.u_lo));
    }
  }
  return id;
}