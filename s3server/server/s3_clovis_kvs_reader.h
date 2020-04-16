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
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#pragma once

#ifndef __S3_SERVER_S3_CLOVIS_KVS_READER_H__
#define __S3_SERVER_S3_CLOVIS_KVS_READER_H__

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "s3_asyncop_context_base.h"
#include "s3_clovis_context.h"
#include "s3_clovis_wrapper.h"
#include "s3_log.h"
#include "s3_request_object.h"

class S3ClovisKVSReaderContext : public S3AsyncOpContextBase {
  // Basic Operation context.
  struct s3_clovis_idx_op_context* clovis_idx_op_context;
  bool has_clovis_idx_op_context;

  // Read/Write Operation context.
  struct s3_clovis_kvs_op_context* clovis_kvs_op_context;
  bool has_clovis_kvs_op_context;

 public:
  S3ClovisKVSReaderContext(std::shared_ptr<RequestObject> req,
                           std::function<void()> success_callback,
                           std::function<void()> failed_callback,
                           std::shared_ptr<ClovisAPI> clovis_api = nullptr)
      : S3AsyncOpContextBase(req, success_callback, failed_callback, 1,
                             clovis_api) {
    request_id = request->get_request_id();
    s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

    // Create or write, we need op context
    clovis_idx_op_context = create_basic_idx_op_ctx(1);
    has_clovis_idx_op_context = true;

    clovis_kvs_op_context = NULL;
    has_clovis_kvs_op_context = false;
  }

  ~S3ClovisKVSReaderContext() {
    s3_log(S3_LOG_DEBUG, request_id, "Destructor\n");

    if (has_clovis_idx_op_context) {
      free_basic_idx_op_ctx(clovis_idx_op_context);
    }
    if (has_clovis_kvs_op_context) {
      free_basic_kvs_op_ctx(clovis_kvs_op_context);
    }
  }

  struct s3_clovis_idx_op_context* get_clovis_idx_op_ctx() {
    return clovis_idx_op_context;
  }

  // Call this when you want to do write op.
  void init_kvs_read_op_ctx(int no_of_keys) {
    clovis_kvs_op_context = create_basic_kvs_op_ctx(no_of_keys);
    has_clovis_kvs_op_context = true;
  }

  struct s3_clovis_kvs_op_context* get_clovis_kvs_op_ctx() {
    return clovis_kvs_op_context;
  }
};

enum class S3ClovisKVSReaderOpState {
  empty,
  start,
  failed_to_launch,
  failed,
  present,
  missing,
};

class S3ClovisKVSReader {
 private:
  struct m0_uint128 id;

  std::shared_ptr<RequestObject> request;
  std::unique_ptr<S3ClovisKVSReaderContext> reader_context;
  std::shared_ptr<ClovisAPI> s3_clovis_api;

  std::string request_id;

  // Used to report to caller
  std::function<void()> handler_on_success;
  std::function<void()> handler_on_failed;

  S3ClovisKVSReaderOpState state;

  // Holds references to keys and values after the read so it can be consumed.
  struct s3_clovis_kvs_op_context* clovis_kvs_op_context;
  std::string last_value;
  // Map to hold last result: first element is `key`, the second is a pair of
  // `return status` & the `value` corresponding to the `key`.
  // `value` is valid if `return status` is 0.
  std::map<std::string, std::pair<int, std::string>> last_result_keys_values;
  size_t iteration_index;

  struct s3_clovis_idx_context* idx_ctx;

  // save reference of key buffer in case of next_keyval op. It needs to be
  // freed post op if any keys were returned successfully.
  void* key_ref_copy;

  void clean_up_contexts();

 public:
  S3ClovisKVSReader(std::shared_ptr<RequestObject> req,
                    std::shared_ptr<ClovisAPI> clovis_api = nullptr);
  virtual ~S3ClovisKVSReader();

  virtual S3ClovisKVSReaderOpState get_state() { return state; }

  // Async get operation.
  // Note -- get_keyval() is called with oid of index
  // void get_keyval(std::string index_name, std::vector<std::string> keys,
  // std::function<void(void)> on_success, std::function<void(void)> on_failed);
  // void get_keyval(std::string index_name, std::string key,
  // std::function<void(void)> on_success, std::function<void(void)> on_failed);
  virtual void get_keyval(struct m0_uint128 oid, std::vector<std::string> keys,
                          std::function<void(void)> on_success,
                          std::function<void(void)> on_failed);
  virtual void get_keyval(struct m0_uint128 oid, std::string key,
                          std::function<void(void)> on_success,
                          std::function<void(void)> on_failed);

  void get_keyval_successful();
  void get_keyval_failed();

  // When looking up for just one KV, use this else for multiple use
  // get_key_values
  virtual std::string get_value() { return last_value; }

  // Used to iterate over the key-value pairs.
  // If the input key is empty string "", it returns the first count key-value
  // pairs
  // If input key is specified, it returns the next count key-value pairs
  // Async call resutls can be accessed using get_values().
  // Default behaviour of this API is exclude input key from results.
  // If input key should be included in the results, then flag should be
  // specified as 0.
  virtual void next_keyval(struct m0_uint128 idx_oid, std::string key,
                           size_t nr_kvp, std::function<void(void)> on_success,
                           std::function<void(void)> on_failed,
                           unsigned int flag = M0_OIF_EXCLUDE_START_KEY);
  void next_keyval_successful();
  void next_keyval_failed();

  virtual void lookup_index(struct m0_uint128 idx_oid,
                            std::function<void(void)> on_success,
                            std::function<void(void)> on_failed);

  void lookup_index_successful();
  void lookup_index_failed();

  // returns the fetched kv pairs
  virtual std::map<std::string, std::pair<int, std::string>>& get_key_values() {
    return last_result_keys_values;
  }

  // friends declaration for Unit test
  FRIEND_TEST(S3ClovisKvsReaderTest, Constructor);
  FRIEND_TEST(S3ClovisKvsReaderTest, CleanupContexts);
  FRIEND_TEST(S3ClovisKvsReaderTest, GetKeyvalTest);
  FRIEND_TEST(S3ClovisKvsReaderTest, GetKeyvalFailTest);
  FRIEND_TEST(S3ClovisKvsReaderTest, GetKeyvalIdxPresentTest);
  FRIEND_TEST(S3ClovisKvsReaderTest, GetKeyvalTestEmpty);
  FRIEND_TEST(S3ClovisKvsReaderTest, GetKeyvalSuccessfulTest);
  FRIEND_TEST(S3ClovisKvsReaderTest, GetKeyvalFailedTest);
  FRIEND_TEST(S3ClovisKvsReaderTest, GetKeyvalFailedTestMissing);
  FRIEND_TEST(S3ClovisKvsReaderTest, NextKeyvalTest);
  FRIEND_TEST(S3ClovisKvsReaderTest, NextKeyvalIdxPresentTest);
  FRIEND_TEST(S3ClovisKvsReaderTest, NextKeyvalSuccessfulTest);
  FRIEND_TEST(S3ClovisKvsReaderTest, NextKeyvalFailedTest);
  FRIEND_TEST(S3ClovisKvsReaderTest, NextKeyvalFailedTestMissing);
};

#endif
