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

#include <utility>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "s3_auth_response_error.h"
#include "s3_log.h"

S3AuthResponseError::S3AuthResponseError(std::string xml)
    : xml_content(std::move(xml)) {
  s3_log(S3_LOG_DEBUG, "", "Constructor\n");
  is_valid = parse_and_validate();
}

S3AuthResponseError::S3AuthResponseError(std::string error_code_,
                                         std::string error_message_,
                                         std::string request_id_)
    : is_valid(false),
      error_code(std::move(error_code_)),
      error_message(std::move(error_message_)),
      request_id(std::move(request_id_)) {
  s3_log(S3_LOG_DEBUG, "", "Constructor\n");
}

bool S3AuthResponseError::isOK() const { return is_valid; }

const std::string& S3AuthResponseError::get_code() const { return error_code; }

const std::string& S3AuthResponseError::get_message() const {
  return error_message;
}

const std::string& S3AuthResponseError::get_request_id() const {
  return request_id;
}

bool S3AuthResponseError::parse_and_validate() {
  s3_log(S3_LOG_DEBUG, "", "Parsing Auth server Error response\n");

  if (xml_content.empty()) {
    s3_log(S3_LOG_ERROR, "", "XML response is NULL\n");
    is_valid = false;
    return false;
  }

  s3_log(S3_LOG_DEBUG, "", "Parsing error xml = %s\n", xml_content.c_str());
  xmlDocPtr document = xmlParseDoc((const xmlChar*)xml_content.c_str());
  if (document == NULL) {
    s3_log(S3_LOG_ERROR, "", "Auth response xml body is invalid.\n");
    is_valid = false;
    return is_valid;
  }

  xmlNodePtr root_node = xmlDocGetRootElement(document);
  if (root_node == NULL) {
    s3_log(S3_LOG_ERROR, "", "Auth response xml body is invalid.\n");
    xmlFreeDoc(document);
    is_valid = false;
    return is_valid;
  }

  xmlChar* key = NULL;
  xmlNodePtr child = root_node->xmlChildrenNode;

  while (child != NULL) {
    if ((!xmlStrcmp(child->name, (const xmlChar*)"Error"))) {
      for (xmlNode* child_node = child->children; child_node != NULL;
           child_node = child_node->next) {
        s3_log(S3_LOG_DEBUG, "", "child->name = %s\n",
               (const char*)child->name);
        key = xmlNodeGetContent(child_node);
        if ((!xmlStrcmp(child_node->name, (const xmlChar*)"Code"))) {
          s3_log(S3_LOG_DEBUG, "", "Code = %s\n", (const char*)key);
          error_code = (const char*)key;
        } else if ((!xmlStrcmp(child_node->name, (const xmlChar*)"Message"))) {
          s3_log(S3_LOG_DEBUG, "", "Message = %s\n", (const char*)key);
          error_message = (const char*)key;
        }
        if (key != NULL) {
          xmlFree(key);
          key = NULL;
        }
      }
    } else if ((!xmlStrcmp(child->name, (const xmlChar*)"RequestId"))) {
      key = xmlNodeGetContent(child);
      s3_log(S3_LOG_DEBUG, "", "RequestId = %s\n", (const char*)key);
      request_id = (const char*)key;
      if (key != NULL) {
        xmlFree(key);
        key = NULL;
      }
    }
    child = child->next;
  }  // while
  xmlFreeDoc(document);
  if (error_code.empty()) {
    is_valid = false;
  } else {
    is_valid = true;
  }
  return is_valid;
}
