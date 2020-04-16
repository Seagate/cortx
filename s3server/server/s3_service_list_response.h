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

#ifndef __S3_SERVER_S3_SERVICE_LIST_RESPONSE_H__
#define __S3_SERVER_S3_SERVICE_LIST_RESPONSE_H__

#include <memory>
#include <string>
#include <vector>

#include "s3_bucket_metadata.h"

class S3ServiceListResponse {
  std::string owner_name;
  std::string owner_id;
  std::vector<std::shared_ptr<S3BucketMetadata>> bucket_list;

  // Generated xml response
  std::string response_xml;

 public:
  S3ServiceListResponse();
  void set_owner_name(std::string name);
  void set_owner_id(std::string id);
  void add_bucket(std::shared_ptr<S3BucketMetadata> bucket);
  std::string& get_xml();
  int get_bucket_count() const { return bucket_list.size(); }
};

#endif
