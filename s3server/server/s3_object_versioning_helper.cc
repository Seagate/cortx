/*
 * COPYRIGHT 2018 SEAGATE LLC
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
 * Original creation date: 01-Feb-2019
 */

#include "s3_object_versioning_helper.h"
#include <chrono>
#include "base64.h"
#include "s3_common_utilities.h"

unsigned long long S3ObjectVersioingHelper::get_epoch_time_in_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string S3ObjectVersioingHelper::generate_new_epoch_time() {
  std::string milliseconds_since_epoch_str;
  // get current epoch time in ms
  unsigned long long milliseconds_since_epoch = get_epoch_time_in_ms();
  // flip bits
  milliseconds_since_epoch = ~milliseconds_since_epoch;
  // convert retrived current epoch time into string
  milliseconds_since_epoch_str = std::to_string(milliseconds_since_epoch);
  return milliseconds_since_epoch_str;
}

std::string S3ObjectVersioingHelper::get_versionid_from_epoch_time(
    const std::string& milliseconds_since_epoch) {
  // encode the current epoch time
  std::string versionid = base64_encode(
      reinterpret_cast<const unsigned char*>(milliseconds_since_epoch.c_str()),
      milliseconds_since_epoch.length());
  // remove padding characters that is '='
  // to make version id as url encoding format
  S3CommonUtilities::find_and_replaceall(versionid, "=", "");
  return versionid;
}

std::string S3ObjectVersioingHelper::generate_keyid_from_versionid(
    std::string versionid) {
  // add padding if requried;
  versionid = versionid.append((3 - versionid.size() % 3) % 3, '=');
  // base64 decoding of versionid
  // decoded versionid is the epoch time in milli seconds
  std::string milliseconds_since_epoch = base64_decode(versionid);
  return milliseconds_since_epoch;
}