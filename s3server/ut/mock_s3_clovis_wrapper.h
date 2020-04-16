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
 * Original author:  Rajesh Nambiar <rajesh.nambiar@seagate.com>
 * Original creation date: 1-Dec-2015
 */

#pragma once

#ifndef __S3_UT_MOCK_S3_CLOVIS_WRAPPER_H__
#define __S3_UT_MOCK_S3_CLOVIS_WRAPPER_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <functional>
#include <iostream>
#include "clovis_helpers.h"
#include "s3_clovis_rw_common.h"
#include "s3_clovis_wrapper.h"

class MockS3Clovis : public ClovisAPI {
 public:
  MockS3Clovis() : ClovisAPI() {}
  MOCK_METHOD3(clovis_idx_init,
               void(struct m0_clovis_idx *idx, struct m0_clovis_realm *parent,
                    const struct m0_uint128 *id));
  MOCK_METHOD1(clovis_idx_fini, void(struct m0_clovis_idx *idx));
  MOCK_METHOD4(clovis_obj_init,
               void(struct m0_clovis_obj *obj, struct m0_clovis_realm *parent,
                    const struct m0_uint128 *id, int layout_id));
  MOCK_METHOD1(clovis_obj_fini, void(struct m0_clovis_obj *obj));
  MOCK_METHOD2(clovis_entity_open,
               int(struct m0_clovis_entity *entity, struct m0_clovis_op **op));
  MOCK_METHOD2(clovis_entity_create,
               int(struct m0_clovis_entity *entity, struct m0_clovis_op **op));
  MOCK_METHOD2(clovis_entity_delete,
               int(struct m0_clovis_entity *entity, struct m0_clovis_op **op));
  MOCK_METHOD3(clovis_op_setup,
               void(struct m0_clovis_op *op, const struct m0_clovis_op_ops *ops,
                    m0_time_t linger));
  MOCK_METHOD7(clovis_idx_op,
               int(struct m0_clovis_idx *idx, enum m0_clovis_idx_opcode opcode,
                   struct m0_bufvec *keys, struct m0_bufvec *vals, int *rcs,
                   unsigned int flags, struct m0_clovis_op **op));
  MOCK_METHOD7(clovis_obj_op,
               int(struct m0_clovis_obj *obj, enum m0_clovis_obj_opcode opcode,
                   struct m0_indexvec *ext, struct m0_bufvec *data,
                   struct m0_bufvec *attr, uint64_t mask,
                   struct m0_clovis_op **op));
  MOCK_METHOD4(clovis_op_launch,
               void(uint64_t, struct m0_clovis_op **, uint32_t, ClovisOpType));
  MOCK_METHOD3(clovis_op_wait,
               int(struct m0_clovis_op *op, uint64_t bits, m0_time_t to));
  MOCK_METHOD1(clovis_sync_op_init, int(struct m0_clovis_op **sync_op));
  MOCK_METHOD2(clovis_sync_entity_add, int(struct m0_clovis_op *sync_op,
                                           struct m0_clovis_entity *entity));
  MOCK_METHOD2(clovis_sync_op_add,
               int(struct m0_clovis_op *sync_op, struct m0_clovis_op *op));
  MOCK_METHOD1(clovis_op_rc, int(const struct m0_clovis_op *op));
  MOCK_METHOD1(m0_h_ufid_next, int(struct m0_uint128 *ufid));
};
#endif
