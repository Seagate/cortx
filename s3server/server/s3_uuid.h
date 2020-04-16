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
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original creation date: 8-Jan-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_UUID_H__
#define __S3_SERVER_S3_UUID_H__
#define MAX_UUID_STR_SIZE 37

#include <uuid/uuid.h>

class S3Uuid {
  uuid_t uuid;
  std::string str_uuid;

 public:
  S3Uuid() {
    char cstr_uuid[MAX_UUID_STR_SIZE];
    uuid_generate(uuid);
    uuid_unparse(uuid, cstr_uuid);
    str_uuid = cstr_uuid;
  }

  bool is_null() { return uuid_is_null(uuid); }

  int parse() { return uuid_parse(str_uuid.c_str(), uuid); }

  std::string get_string_uuid() { return str_uuid; }

  bool operator==(const S3Uuid& seconduuid) {
    return uuid_compare(this->uuid, seconduuid.uuid) == 0;
  }

  bool operator!=(const S3Uuid& second_uuid) {
    return uuid_compare(this->uuid, second_uuid.uuid) != 0;
  }

  const unsigned char* ptr() const { return uuid; }
};
#endif
