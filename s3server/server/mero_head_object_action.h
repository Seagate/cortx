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
 * Original author:  Dattaprasad Govekar   <dattaprasad.govekar@seagate.com>
 * Original creation date: 06-Aug-2019
 */

#pragma once

#ifndef __MERO_HEAD_OBJECT_ACTION_H__
#define __MERO_HEAD_OBJECT_ACTION_H__

#include <gtest/gtest_prod.h>
#include <memory>

#include "s3_factory.h"
#include "mero_action_base.h"

class MeroHeadObjectAction : public MeroAction {
  int layout_id;
  m0_uint128 oid;
  std::shared_ptr<S3ClovisReader> clovis_reader;

  std::shared_ptr<S3ClovisReaderFactory> clovis_reader_factory;

 public:
  MeroHeadObjectAction(std::shared_ptr<MeroRequestObject> req,
                       std::shared_ptr<S3ClovisReaderFactory> reader_factory =
                           nullptr);

  void setup_steps();
  void validate_request();

  void check_object_exist();
  void check_object_exist_success();
  void check_object_exist_failure();
  void send_response_to_s3_client();
};
#endif