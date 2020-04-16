/*
 * COPYRIGHT 2019 SEAGATE LLC
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
 * Original author:  Dmitrii Surnin <dmitrii.surnin@seagate.com>
 * Original creation date: 11-July-2019
 */

#include "s3_fake_clovis_kvs.h"
#include "s3_log.h"

#include <algorithm>

std::unique_ptr<S3FakeClovisKvs> S3FakeClovisKvs::inst;

S3FakeClovisKvs::S3FakeClovisKvs() : in_mem_kv() {}

int S3FakeClovisKvs::kv_read(struct m0_uint128 const &oid,
                             struct s3_clovis_kvs_op_context const &kv) {
  s3_log(S3_LOG_DEBUG, "", "Entering with oid %" SCNx64 " : %" SCNx64 "\n",
         oid.u_hi, oid.u_lo);
  if (in_mem_kv.count(oid) == 0) {
    s3_log(S3_LOG_DEBUG, "", "Exiting NOENT\n");
    return -ENOENT;
  }

  KeyVal &obj_kv = in_mem_kv[oid];
  int cnt = kv.values->ov_vec.v_nr;
  for (int i = 0; i < cnt; ++i) {
    std::string search_key((char *)kv.keys->ov_buf[i],
                           kv.keys->ov_vec.v_count[i]);
    if (obj_kv.count(search_key) == 0) {
      kv.rcs[i] = -ENOENT;
      s3_log(S3_LOG_DEBUG, "", "k:>%s v:>ENOENT\n", search_key.c_str());
      continue;
    }

    std::string found_val = obj_kv[search_key];
    kv.rcs[i] = 0;
    kv.values->ov_vec.v_count[i] = found_val.length();
    kv.values->ov_buf[i] = strdup(found_val.c_str());
    s3_log(S3_LOG_DEBUG, "", "k:>%s v:>%s\n", search_key.c_str(),
           found_val.c_str());
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting 0\n");
  return 0;
}

int S3FakeClovisKvs::kv_next(struct m0_uint128 const &oid,
                             struct s3_clovis_kvs_op_context const &kv) {
  s3_log(S3_LOG_DEBUG, "", "Entering with oid %" SCNx64 " : %" SCNx64 "\n",
         oid.u_hi, oid.u_lo);
  if (in_mem_kv.count(oid) == 0) {
    s3_log(S3_LOG_DEBUG, "", "Exiting ENOENT\n");
    return -ENOENT;
  }

  KeyVal &obj_kv = in_mem_kv[oid];
  int cnt = kv.values->ov_vec.v_nr;
  auto val_it = std::begin(obj_kv);
  if (kv.keys->ov_vec.v_count[0] > 0) {
    std::string search_key((char *)kv.keys->ov_buf[0],
                           kv.keys->ov_vec.v_count[0]);

    kv.keys->ov_vec.v_count[0] = 0;
    // do not free - done in upper level
    kv.keys->ov_buf[0] = nullptr;

    val_it = obj_kv.find(search_key);
    if (val_it == std::end(obj_kv)) {
      val_it = std::find_if(
          std::begin(obj_kv), std::end(obj_kv),
          [&search_key](const std::pair<std::string, std::string> & itv)
              ->bool { return itv.first.find(search_key) == 0; });
    } else {
      // found full value, should take next
      ++val_it;
    }
    if (val_it == std::end(obj_kv)) {
      s3_log(S3_LOG_DEBUG, "", "Exiting k:>%s ENOENT\n", search_key.c_str());
      return -ENOENT;
    }
    s3_log(S3_LOG_DEBUG, "", "Initial k:>%s found\n", search_key.c_str());
  }

  for (int i = 0; i < cnt; ++i) {
    kv.rcs[i] = -ENOENT;
    if (val_it == std::end(obj_kv)) {
      continue;
    }

    kv.rcs[i] = 0;

    std::string tmp = val_it->first;
    kv.keys->ov_vec.v_count[i] = tmp.length();
    kv.keys->ov_buf[i] = strdup(tmp.c_str());

    tmp = val_it->second;
    kv.values->ov_vec.v_count[i] = tmp.length();
    kv.values->ov_buf[i] = strdup(tmp.c_str());

    s3_log(S3_LOG_DEBUG, "", "Got k:>%s v:>%s\n", (char *)kv.keys->ov_buf[i],
           (char *)kv.values->ov_buf[i]);

    ++val_it;
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting 0\n");
  return 0;
}

int S3FakeClovisKvs::kv_write(struct m0_uint128 const &oid,
                              struct s3_clovis_kvs_op_context const &kv) {
  s3_log(S3_LOG_DEBUG, "", "Entering with oid %" SCNx64 " : %" SCNx64 "\n",
         oid.u_hi, oid.u_lo);
  KeyVal &obj_kv = in_mem_kv[oid];

  int cnt = kv.values->ov_vec.v_nr;
  for (int i = 0; i < cnt; ++i) {
    std::string nkey((char *)kv.keys->ov_buf[i], kv.keys->ov_vec.v_count[i]);
    std::string nval((char *)kv.values->ov_buf[i],
                     kv.values->ov_vec.v_count[i]);
    obj_kv[nkey] = nval;
    kv.rcs[i] = 0;

    s3_log(S3_LOG_DEBUG, "", "Add k:>%s -> v:>%s\n", nkey.c_str(),
           nval.c_str());
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting 0\n");
  return 0;
}

int S3FakeClovisKvs::kv_del(struct m0_uint128 const &oid,
                            struct s3_clovis_kvs_op_context const &kv) {
  s3_log(S3_LOG_DEBUG, "", "Entering with oid %" SCNx64 " : %" SCNx64 "\n",
         oid.u_hi, oid.u_lo);
  if (in_mem_kv.count(oid) == 0) {
    s3_log(S3_LOG_DEBUG, "", "Exiting NOENT\n");
    return -ENOENT;
  }

  KeyVal &obj_kv = in_mem_kv[oid];

  int cnt = kv.values->ov_vec.v_nr;
  for (int i = 0; i < cnt; ++i) {
    std::string nkey((char *)kv.keys->ov_buf[i], kv.keys->ov_vec.v_count[i]);
    kv.rcs[i] = -ENOENT;
    if (obj_kv.erase(nkey) > 0) {
      kv.rcs[i] = 0;
    }

    s3_log(S3_LOG_DEBUG, "", "Del k:>%s -> %d\n", nkey.c_str(), kv.rcs[i]);
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting 0\n");
  return 0;
}
