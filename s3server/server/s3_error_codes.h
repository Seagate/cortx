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

#ifndef __S3_SERVER_S3_ERROR_CODES_H__
#define __S3_SERVER_S3_ERROR_CODES_H__

#include "s3_error_messages.h"

#define S3HttpSuccess200 EVHTP_RES_OK
#define S3HttpSuccess201 EVHTP_RES_CREATED
#define S3HttpSuccess204 EVHTP_RES_NOCONTENT
#define S3HttpSuccess206 EVHTP_RES_PARTIAL
#define S3HttpFailed400 EVHTP_RES_400
#define S3HttpFailed401 EVHTP_RES_UNAUTH
#define S3HttpFailed403 EVHTP_RES_FORBIDDEN
#define S3HttpFailed404 EVHTP_RES_NOTFOUND
#define S3HttpFailed405 EVHTP_RES_METHNALLOWED
#define S3HttpFailed409 EVHTP_RES_CONFLICT
#define S3HttpFailed500 EVHTP_RES_500
#define S3HttpFailed503 EVHTP_RES_SERVUNAVAIL

/* Example error message:
  <?xml version="1.0" encoding="UTF-8"?>
  <Error>
    <Code>NoSuchKey</Code>
    <Message>The resource you requested does not exist</Message>
    <Resource>/mybucket/myfoto.jpg</Resource>
    <RequestId>4442587FB7D0A2F9</RequestId>
  </Error>
 */

class S3Error {
  std::string code;  // Error codes are read from s3_error_messages.json
  std::string request_id;
  std::string resource_key;
  std::string auth_error_message;
  S3ErrorDetails& details;

  std::string xml_message;

 public:
  S3Error(std::string error_code, std::string req_id, std::string res_key,
          std::string error_message = "");

  int get_http_status_code();

  std::string& to_xml();
};

#endif
