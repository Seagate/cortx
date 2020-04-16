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

#include "s3_service_list_response.h"
#include "s3_common_utilities.h"

S3ServiceListResponse::S3ServiceListResponse() {
  s3_log(S3_LOG_DEBUG, "", "Constructor\n");
  bucket_list.clear();
}

void S3ServiceListResponse::set_owner_name(std::string name) {
  owner_name = name;
}

void S3ServiceListResponse::set_owner_id(std::string id) { owner_id = id; }

void S3ServiceListResponse::add_bucket(
    std::shared_ptr<S3BucketMetadata> bucket) {
  bucket_list.push_back(bucket);
}

// clang-format off
std::string& S3ServiceListResponse::get_xml() {
  response_xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
  response_xml +=
      "<ListAllMyBucketsResult>"
      "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01\">";
  response_xml += "<Owner>";
  response_xml += S3CommonUtilities::format_xml_string("ID", owner_id);
  response_xml +=
      S3CommonUtilities::format_xml_string("DisplayName", owner_name);
  response_xml += "</Owner>";
  response_xml += "<Buckets>";
  for (auto&& bucket : bucket_list) {
    response_xml += "<Bucket>";
    response_xml +=
        S3CommonUtilities::format_xml_string("Name", bucket->get_bucket_name());
    response_xml += S3CommonUtilities::format_xml_string(
        "CreationDate", bucket->get_creation_time());
    response_xml += "</Bucket>";
  }
  response_xml += "</Buckets>";
  response_xml += "</ListAllMyBucketsResult>";

  return response_xml;
}
// clang-format on
