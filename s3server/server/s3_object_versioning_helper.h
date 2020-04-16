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

#pragma once

#ifndef __S3_SERVER_S3_OBJECT_VERSIONING_HELPER_H__
#define __S3_SERVER_S3_OBJECT_VERSIONING_HELPER_H__

#include <iostream>

class S3ObjectVersioingHelper {
 public:
  S3ObjectVersioingHelper() = delete;
  S3ObjectVersioingHelper(const S3ObjectVersioingHelper&) = delete;
  S3ObjectVersioingHelper& operator=(const S3ObjectVersioingHelper&) = delete;

 private:
  // method returns epoch time value in ms
  static unsigned long long get_epoch_time_in_ms();

 public:
  static std::string generate_new_epoch_time();

  // get version id from the epoch time value
  static std::string get_versionid_from_epoch_time(
      const std::string& milliseconds_since_epoch);

  // get_epoch_time_from_versionid
  static std::string generate_keyid_from_versionid(std::string versionid);
};
#endif
