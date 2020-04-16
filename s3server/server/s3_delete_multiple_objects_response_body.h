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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 3-Feb-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_DELETE_MULTIPLE_OBJECTS_RESPONSE_BODY_H__
#define __S3_SERVER_S3_DELETE_MULTIPLE_OBJECTS_RESPONSE_BODY_H__

class SuccessDeleteKey {
  std::string object_key;
  std::string version;

 public:
  SuccessDeleteKey(std::string key, std::string id = "") : object_key(key) {}

  std::string to_xml() {
    std::string xml = "";
    xml +=
        "<Deleted>\n"
        "  <Key>" +
        object_key +
        "</Key>\n"
        //  "  <VersionId>" + version + "</VersionId>\n"
        //  "  <DeleteMarker>" + true + "</DeleteMarker>\n"
        //  "  <DeleteMarkerVersionId>" + true + "</DeleteMarkerVersionId>\n"
        "</Deleted>\n";
    return xml;
  }
};

class ErrorDeleteKey {
  std::string object_key;
  std::string error_code;
  std::string error_message;

 public:
  ErrorDeleteKey(std::string key, std::string code, std::string msg)
      : object_key(key), error_code(code), error_message(msg) {}

  std::string to_xml() {
    std::string xml = "";
    xml +=
        "<Error>\n"
        "  <Key>" +
        object_key +
        "</Key>\n"
        //  "  <VersionId>" + owner_name + "</VersionId>\n"
        "  <Code>" +
        error_code +
        "</Code>\n"
        "  <Message>" +
        error_message +
        "</Message>\n"
        "</Error>\n";
    return xml;
  }
};

class S3DeleteMultipleObjectsResponseBody {
  std::vector<SuccessDeleteKey> success;
  // key, errorcode
  std::vector<ErrorDeleteKey> error;

  std::string response_xml;

 public:
  void add_success(std::string key, std::string version = "") {
    success.push_back(SuccessDeleteKey(key, version));
  }

  size_t get_success_count() { return success.size(); }

  void add_failure(std::string key, std::string code,
                   std::string message = "") {
    error.push_back(ErrorDeleteKey(key, code, message));
  }

  size_t get_failure_count() { return error.size(); }

  std::string& to_xml() {
    response_xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    response_xml +=
        "<DeleteResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">";
    for (auto sitem : success) {
      response_xml += sitem.to_xml();
    }
    for (auto eitem : error) {
      response_xml += eitem.to_xml();
    }
    response_xml += "</DeleteResult>";
    return response_xml;
  }
};

#endif
