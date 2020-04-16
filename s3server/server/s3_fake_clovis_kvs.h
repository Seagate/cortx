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

#pragma once

#ifndef __S3_SERVER_FAKE_CLOVIS_KVS__H__
#define __S3_SERVER_FAKE_CLOVIS_KVS__H__

#include "s3_clovis_context.h"

#include <memory>
#include <string>

class S3FakeClovisKvs {
 private:
  S3FakeClovisKvs();

  typedef std::map<std::string, std::string> KeyVal;
  struct Uint128Comp {
    bool operator()(struct m0_uint128 const &a,
                    struct m0_uint128 const &b) const {
      return std::memcmp((void *)&a, (void *)&b, sizeof(a)) < 0;
    }
  };

  std::map<struct m0_uint128, KeyVal, Uint128Comp> in_mem_kv;

 private:
  static std::unique_ptr<S3FakeClovisKvs> inst;

 public:
  virtual ~S3FakeClovisKvs() {}

  int kv_read(struct m0_uint128 const &oid,
              struct s3_clovis_kvs_op_context const &kv);

  int kv_next(struct m0_uint128 const &oid,
              struct s3_clovis_kvs_op_context const &kv);

  int kv_write(struct m0_uint128 const &oid,
               struct s3_clovis_kvs_op_context const &kv);

  int kv_del(struct m0_uint128 const &oid,
             struct s3_clovis_kvs_op_context const &kv);

 public:
  static S3FakeClovisKvs *instance() {
    if (!inst) {
      inst = std::unique_ptr<S3FakeClovisKvs>(new S3FakeClovisKvs());
    }
    return inst.get();
  }
};

#endif
