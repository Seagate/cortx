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

#include "s3_error_codes.h"
#include "s3_common_utilities.h"

S3Error::S3Error(std::string error_code, std::string req_id,
                 std::string res_key, std::string error_message)
    : code(error_code),
      request_id(req_id),
      resource_key(res_key),
      auth_error_message(error_message),
      details(S3ErrorMessages::get_instance()->get_details(error_code)) {}

int S3Error::get_http_status_code() { return details.get_http_status_code(); }

std::string& S3Error::to_xml() {
  if (get_http_status_code() == -1) {
    // Object state is invalid, Wrong error code.
    xml_message = "";
    return xml_message;
  }
  // clang-format off
  xml_message = "";
  xml_message = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  xml_message += "<Error>\n";
  xml_message += S3CommonUtilities::format_xml_string("Code", code);
  if (auth_error_message.empty()) {
  xml_message +=
      S3CommonUtilities::format_xml_string("Message", details.get_message());
  } else {
    xml_message +=
        S3CommonUtilities::format_xml_string("Message", auth_error_message);
  }
  xml_message += S3CommonUtilities::format_xml_string("Resource", resource_key);
  xml_message += S3CommonUtilities::format_xml_string("RequestId", request_id);
  xml_message += "</Error>\n";
  // clang-format on
  return xml_message;
}
