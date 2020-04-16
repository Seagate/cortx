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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#pragma once

#ifndef __S3_SERVER_S3_DATETIME_H__
#define __S3_SERVER_S3_DATETIME_H__

#include <stdlib.h>
#include <time.h>
#include <string>

// UTC == GMT we always save/give out UTC/GMT time with different format
#define S3_GMT_DATETIME_FORMAT "%a, %d %b %Y %H:%M:%S GMT"
#define S3_ISO_DATETIME_FORMAT "%Y-%m-%dT%T.000Z"

// Helper to store DateTime in KV store in Json
class S3DateTime {
  struct tm point_in_time;
  bool is_valid;

  void init_with_fmt(std::string time_str, std::string format);
  std::string get_format_string(std::string format);

 public:
  S3DateTime();
  void init_current_time();
  void init_with_gmt(std::string time_str);
  void init_with_iso(std::string time_str);

  // Returns if the Object state is valid.
  bool is_OK();

  std::string get_isoformat_string();
  std::string get_gmtformat_string();
  friend class S3DateTimeTest;
};

#endif
