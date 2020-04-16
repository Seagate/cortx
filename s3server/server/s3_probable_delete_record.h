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

#pragma once

#ifndef __S3_SERVER_S3_PROBABLE_DELETE_RECORD_H__
#define __S3_SERVER_S3_PROBABLE_DELETE_RECORD_H__

#include <gtest/gtest_prod.h>
#include <string>

#include "s3_clovis_rw_common.h"

// Record is stored as a value in global probable delete records list index and
// the "key" is
//    For new objects key is current object OID.
//    For old objects key is old-oid + '-' + new-oid

class S3ProbableDeleteRecord {
  std::string record_key;
  struct m0_uint128 old_object_oid;  // oid of old object, if current object is
                                     // replacing old object.
  std::string object_key_in_index;   // current object key, candidate for
                                     // probable delete
  struct m0_uint128 current_object_oid;
  int object_layout_id;                   // current object layout id
  struct m0_uint128 object_list_idx_oid;  // multipart idx in case of multipart
  struct m0_uint128 objects_version_list_idx_oid;
  std::string version_key_in_index;  // Version key for current object
  bool force_delete;  // when force_delete = true, background delete will simply
                      // delete object
  bool is_multipart;
  struct m0_uint128 part_list_idx_oid;  // present in case of multipart

 public:
  S3ProbableDeleteRecord(std::string rec_key, struct m0_uint128 old_oid,
                         std::string obj_key_in_index,
                         struct m0_uint128 new_oid, int layout_id,
                         struct m0_uint128 obj_list_idx_oid,
                         struct m0_uint128 objs_version_list_idx_oid,
                         std::string ver_key_in_index, bool force_del = false,
                         bool is_multipart = false,
                         struct m0_uint128 part_list_oid = {0ULL, 0ULL});
  virtual ~S3ProbableDeleteRecord() {}

  virtual const std::string& get_key() const { return record_key; }

  virtual struct m0_uint128 get_current_object_oid() const {
    return current_object_oid;
  }

  // Override force delete flag
  virtual void set_force_delete(bool flag) { force_delete = flag; }

  virtual std::string to_json();
};

#endif
