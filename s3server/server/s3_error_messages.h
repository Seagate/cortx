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

#ifndef __S3_SERVER_S3_ERROR_MESSAGES_H__
#define __S3_SERVER_S3_ERROR_MESSAGES_H__

#include <gtest/gtest_prod.h>
#include <map>
#include <string>
#include "s3_log.h"

class S3ErrorDetails {
  std::string description;
  int http_return_code;

 public:
  S3ErrorDetails() : description("Unknown Error"), http_return_code(520) {}

  S3ErrorDetails(std::string message, int http_code) {
    description = message;
    http_return_code = http_code;
  }

  std::string& get_message() { return description; }

  int get_http_status_code() { return http_return_code; }

  FRIEND_TEST(S3ErrorDetailsTest, DefaultConstructor);
  FRIEND_TEST(S3ErrorDetailsTest, ConstructorWithMsg);
};

class S3ErrorMessages {
 private:
  static S3ErrorMessages* instance;
  S3ErrorMessages(std::string config_file);
  ~S3ErrorMessages();

  std::map<std::string, S3ErrorDetails> error_list;

 public:
  // Loads messages and creates singleton
  static void init_messages(
      std::string config_file =
          "/opt/seagate/s3/resources/s3_error_messages.json");

  // Cleans up the singleton instance
  static void finalize();

  static S3ErrorMessages* get_instance();

  S3ErrorDetails& get_details(std::string code);

  FRIEND_TEST(S3ErrorMessagesTest, Constructor);
};

#endif
