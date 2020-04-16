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

#include <json/json.h>
#include <fstream>
#include <iostream>

#include "s3_error_messages.h"

S3ErrorMessages* S3ErrorMessages::instance = NULL;

S3ErrorMessages::S3ErrorMessages(std::string config_file) {
  Json::Value jsonroot;
  Json::Reader reader;
  std::ifstream json_file(config_file.c_str(), std::ifstream::binary);
  bool parsingSuccessful = reader.parse(json_file, jsonroot);
  if (!parsingSuccessful) {
    s3_log(S3_LOG_FATAL, "", "Json Parsing failed for file: %s.\n",
           config_file.c_str());
    s3_log(S3_LOG_FATAL, "", "Error: %s\n",
           reader.getFormattedErrorMessages().c_str());
    exit(1);
    return;
  }

  Json::Value::Members members = jsonroot.getMemberNames();
  for (auto it : members) {
    error_list[it] = S3ErrorDetails(jsonroot[it]["Description"].asString(),
                                    jsonroot[it]["httpcode"].asInt());
  }
}

S3ErrorMessages::~S3ErrorMessages() {
  // To keep jsoncpp happy in clean up.
  error_list.clear();
}

S3ErrorDetails& S3ErrorMessages::get_details(std::string code) {
  return error_list[code];
}

void S3ErrorMessages::init_messages(std::string config_file) {
  if (!instance) {
    instance = new S3ErrorMessages(config_file);
  }
}

void S3ErrorMessages::finalize() {
  if (!instance) {
    delete instance;
  }
}

S3ErrorMessages* S3ErrorMessages::get_instance() {
  if (!instance) {
    S3ErrorMessages::init_messages();
  }
  return instance;
}
