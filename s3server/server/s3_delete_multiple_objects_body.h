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

#ifndef __S3_SERVER_S3_DELETE_MULTIPLE_OBJECTS_BODY_H__
#define __S3_SERVER_S3_DELETE_MULTIPLE_OBJECTS_BODY_H__

#include <string>
#include <vector>

class S3DeleteMultipleObjectsBody {
  std::string xml_content;
  bool is_valid;

  // std::vector<std::unique_ptr<DeleteObjectInfo>> object_list;
  // Version id are yet to be implemented. The order should be maintained
  // Split in 2 for faster access.
  std::vector<std::string> object_keys;
  std::vector<std::string> version_ids;
  bool quiet;

  bool parse_and_validate();

 public:
  S3DeleteMultipleObjectsBody();
  void initialize(std::string& xml);

  bool isOK();

  // returns the count of objects in request to be deleted.
  int get_count() {
    return object_keys.size();
    // return object_list.size();
  }

  std::vector<std::string> get_keys(size_t index, size_t count) {
    if (index + count <= object_keys.size()) {
      std::vector<std::string> sub(object_keys.begin() + index,
                                   object_keys.begin() + index + count);
      return sub;
    } else if (index < object_keys.size()) {
      std::vector<std::string> sub(object_keys.begin() + index,
                                   object_keys.end());
      return sub;
    } else {
      std::vector<std::string> sub;
      return sub;
    }
  }

  std::vector<std::string> get_version_ids(size_t index, size_t count) {
    if (index + count <= version_ids.size()) {
      std::vector<std::string> sub(version_ids.begin() + index,
                                   version_ids.begin() + index + count);
      return sub;
    } else if (index < version_ids.size()) {
      std::vector<std::string> sub(version_ids.begin() + index,
                                   object_keys.end());
      return sub;
    } else {
      std::vector<std::string> sub;
      return sub;
    }
  }

  bool is_quiet() { return quiet; }
};

#endif
