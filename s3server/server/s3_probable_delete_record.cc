/*
 * COPYRIGHT 2020 SEAGATE LLC
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
 * Original creation date: 25-Feb-2020
 */

#include <assert.h>
#include <json/json.h>

#include "s3_datetime.h"
#include "s3_log.h"
#include "s3_m0_uint128_helper.h"
#include "s3_option.h"
#include "s3_probable_delete_record.h"

extern struct m0_uint128 global_instance_id;

S3ProbableDeleteRecord::S3ProbableDeleteRecord(
    std::string rec_key, struct m0_uint128 old_oid,
    std::string obj_key_in_index, struct m0_uint128 new_oid, int layout_id,
    struct m0_uint128 obj_list_idx_oid,
    struct m0_uint128 objs_version_list_idx_oid, std::string ver_key_in_index,
    bool force_del, bool is_multipart, struct m0_uint128 part_list_oid)
    : record_key(rec_key),
      old_object_oid(old_oid),
      object_key_in_index(obj_key_in_index),
      current_object_oid(new_oid),
      object_layout_id(layout_id),
      object_list_idx_oid(obj_list_idx_oid),
      objects_version_list_idx_oid(objs_version_list_idx_oid),
      version_key_in_index(ver_key_in_index),
      force_delete(force_del),
      is_multipart(is_multipart),
      part_list_idx_oid(part_list_oid) {
  // Assertions
  s3_log(S3_LOG_DEBUG, "", "object_key_in_index = %s\n",
         object_key_in_index.c_str());
  assert(!object_key_in_index.empty());
  s3_log(S3_LOG_DEBUG, "", "object_layout_id = %d\n", object_layout_id);
  assert(object_layout_id != -1);
  s3_log(S3_LOG_DEBUG, "", "object_list_idx_oid = %" SCNx64 " : %" SCNx64 "\n",
         object_list_idx_oid.u_hi, object_list_idx_oid.u_lo);
  assert(object_list_idx_oid.u_hi || object_list_idx_oid.u_lo);
  if (is_multipart) {
    assert(part_list_idx_oid.u_hi || part_list_idx_oid.u_lo);
    s3_log(S3_LOG_DEBUG, "", "part_list_idx_oid = %" SCNx64 " : %" SCNx64 "\n",
           part_list_idx_oid.u_hi, part_list_idx_oid.u_lo);
  } else {
    s3_log(S3_LOG_DEBUG, "",
           "objects_version_list_idx_oid = %" SCNx64 " : %" SCNx64 "\n",
           objects_version_list_idx_oid.u_hi,
           objects_version_list_idx_oid.u_lo);
    assert(objects_version_list_idx_oid.u_hi ||
           objects_version_list_idx_oid.u_lo);
  }
}

std::string S3ProbableDeleteRecord::to_json() {
  Json::Value root;

  // current_object_oid is key for this record, hence not stored in json

  // Will be 0 for new object record or first object.
  root["old_oid"] = S3M0Uint128Helper::to_string(old_object_oid);
  root["object_key_in_index"] =
      object_key_in_index;  // Object key in object list index
  root["object_layout_id"] =
      object_layout_id;  // Of current object to be possibly deleted

  root["object_list_index_oid"] =
      S3M0Uint128Helper::to_string(object_list_idx_oid);
  root["objects_version_list_index_oid"] =
      S3M0Uint128Helper::to_string(objects_version_list_idx_oid);
  root["clovis_process_fid"] =
      S3Option::get_instance()->get_clovis_process_fid().c_str();
  // LC - LIfecyle counter
  root["global_instance_id"] = S3M0Uint128Helper::to_string(global_instance_id);
  root["force_delete"] = force_delete ? "true" : "false";

  if (is_multipart) {
    root["is_multipart"] = "true";
    root["part_list_idx_oid"] = S3M0Uint128Helper::to_string(part_list_idx_oid);
  } else {
    root["is_multipart"] = "false";
    root["version_key_in_index"] = version_key_in_index;
  }

  S3DateTime current_time;
  current_time.init_current_time();
  root["create_timestamp"] = current_time.get_isoformat_string();

  Json::FastWriter fastWriter;
  return fastWriter.write(root);
}
