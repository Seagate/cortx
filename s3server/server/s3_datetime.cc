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

#include "s3_datetime.h"
#include <string.h>
#include "s3_log.h"

S3DateTime::S3DateTime() : is_valid(true) {
  memset(&point_in_time, 0, sizeof(struct tm));
}

bool S3DateTime::is_OK() { return is_valid; }

void S3DateTime::init_current_time() {
  time_t t = time(NULL);
  struct tm *tmp = gmtime_r(&t, &point_in_time);
  if (tmp == NULL) {
    s3_log(S3_LOG_ERROR, "", "gmtime error\n");
    is_valid = false;
  }
}

void S3DateTime::init_with_fmt(std::string time_str, std::string format) {
  memset(&point_in_time, 0, sizeof(struct tm));
  strptime(time_str.c_str(), format.c_str(), &point_in_time);
}

void S3DateTime::init_with_gmt(std::string time_str) {
  init_with_fmt(time_str, S3_GMT_DATETIME_FORMAT);
}

void S3DateTime::init_with_iso(std::string time_str) {
  init_with_fmt(time_str, S3_ISO_DATETIME_FORMAT);
}

std::string S3DateTime::get_isoformat_string() {
  return get_format_string(S3_ISO_DATETIME_FORMAT);
}

std::string S3DateTime::get_gmtformat_string() {
  return get_format_string(S3_GMT_DATETIME_FORMAT);
}

std::string S3DateTime::get_format_string(std::string format) {
  std::string formatted_time = "";
  char timebuffer[100] = {0};
  if (is_OK()) {
    if (strftime(timebuffer, sizeof(timebuffer), format.c_str(),
                 &point_in_time) == 0) {
      s3_log(S3_LOG_ERROR, "", "strftime returned 0\n");
      is_valid = false;
    } else {
      is_valid = true;
      formatted_time = timebuffer;
    }
  }
  return formatted_time;
}
