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
 * Author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 10-Mar-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_AUTH_RESPONSE_ERROR_H__
#define __S3_SERVER_S3_AUTH_RESPONSE_ERROR_H__

#include <gtest/gtest_prod.h>
#include <string>

class S3AuthResponseError {
  std::string xml_content;
  bool is_valid;

  std::string error_code;
  std::string error_message;
  std::string request_id;

  bool parse_and_validate();

 public:
  S3AuthResponseError(std::string xml);
  S3AuthResponseError(std::string error_code_, std::string error_message_,
                      std::string request_id_);

  bool isOK() const;

  const std::string& get_code() const;
  const std::string& get_message() const;
  const std::string& get_request_id() const;

  FRIEND_TEST(S3AuthResponseErrorTest, ConstructorTest);
  FRIEND_TEST(S3AuthResponseErrorTest, ValidateMustFailForEmptyXML);
  FRIEND_TEST(S3AuthResponseErrorTest, ValidateMustFailForInvalidXML);
  FRIEND_TEST(S3AuthResponseErrorTest, ValidateMustFailForEmptyErrorCode);
  FRIEND_TEST(S3AuthResponseErrorTest, ValidateMustFailForMissingErrorCode);
};

#endif
