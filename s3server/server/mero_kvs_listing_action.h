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
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 1-JUNE-2019
 */

#pragma once

#ifndef __S3_SERVER_MERO_KVS_LIST_ACTION_H__
#define __S3_SERVER_MERO_KVS_LIST_ACTION_H__

#include <memory>

#include "mero_action_base.h"
#include "s3_clovis_kvs_reader.h"
#include "mero_kv_list_response.h"

class MeroKVSListingAction : public MeroAction {
  std::shared_ptr<S3ClovisKVSReader> clovis_kv_reader;
  std::shared_ptr<ClovisAPI> mero_clovis_api;
  std::shared_ptr<S3ClovisKVSReaderFactory> mero_clovis_kvs_reader_factory;
  MeroKVListResponse kvs_response_list;
  m0_uint128 index_id;
  std::string last_key;  // last key during each iteration
  bool fetch_successful;

  // Request Input params
  std::string request_prefix;
  std::string request_delimiter;
  std::string request_marker_key;
  size_t max_keys;

 public:
  MeroKVSListingAction(
      std::shared_ptr<MeroRequestObject> req,
      std::shared_ptr<S3ClovisKVSReaderFactory> clovis_kvs_reader_factory =
          nullptr);
  void setup_steps();
  void validate_request();
  void get_next_key_value();
  void get_next_key_value_successful();
  void get_next_key_value_failed();

  void send_response_to_s3_client();
};

#endif
