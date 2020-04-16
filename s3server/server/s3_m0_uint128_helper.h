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

#pragma once

#ifndef __S3_M0_UINT128_HELPER_h__
#define __S3_M0_UINT128_HELPER_h__

#include <string>
#include "clovis_helpers.h"

class S3M0Uint128Helper {
 public:
  S3M0Uint128Helper() = delete;

  static std::pair<std::string, std::string> to_string_pair(
      const m0_uint128 &id);
  static std::string to_string(const m0_uint128 &id);
  static m0_uint128 to_m0_uint128(const std::string &id_u_lo,
                                  const std::string &id_u_hi);
  static m0_uint128 to_m0_uint128(const std::string &id_str);
};
#endif
