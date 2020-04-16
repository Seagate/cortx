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

#ifndef __S3_SERVER_S3_CLOVIS_KVS_WRITER_H__
#define __S3_SERVER_S3_CLOVIS_KVS_WRITER_H__

#include <gtest/gtest_prod.h>
#include <functional>
#include <memory>

#include "s3_asyncop_context_base.h"
#include "s3_clovis_context.h"
#include "s3_clovis_wrapper.h"
#include "s3_log.h"
#include "s3_request_object.h"

class S3SyncClovisKVSWriterContext {
  // Basic Operation context.
  struct s3_clovis_idx_op_context* clovis_idx_op_context;
  bool has_clovis_idx_op_context;

  // Read/Write Operation context.
  struct s3_clovis_kvs_op_context* clovis_kvs_op_context;
  bool has_clovis_kvs_op_context;

  std::string request_id;
  int ops_count = 1;

 public:
  S3SyncClovisKVSWriterContext(std::string req_id, int ops_cnt)
      : request_id(std::move(req_id)), ops_count(ops_cnt) {

    s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
    // Create or write, we need op context
    clovis_idx_op_context = create_basic_idx_op_ctx(ops_count);
    has_clovis_idx_op_context = true;
    clovis_kvs_op_context = NULL;
    has_clovis_kvs_op_context = false;
  }

  ~S3SyncClovisKVSWriterContext() {
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
  void init_kvs_write_op_ctx(int no_of_keys) {
    clovis_kvs_op_context = create_basic_kvs_op_ctx(no_of_keys);
    has_clovis_kvs_op_context = true;
  }

  struct s3_clovis_kvs_op_context* get_clovis_kvs_op_ctx() {
    return clovis_kvs_op_context;
  }
};

// Async Clovis context is inherited from Sync Context & S3AsyncOpContextBase

class S3AsyncClovisKVSWriterContext : public S3SyncClovisKVSWriterContext,
                                      public S3AsyncOpContextBase {

  std::string request_id = "";

 public:
  S3AsyncClovisKVSWriterContext(std::shared_ptr<RequestObject> req,
                                std::function<void()> success_callback,
                                std::function<void()> failed_callback,
                                int ops_count = 1,
                                std::shared_ptr<ClovisAPI> clovis_api = nullptr)
      : S3SyncClovisKVSWriterContext(req ? req->get_request_id() : "",
                                     ops_count),
        S3AsyncOpContextBase(req, success_callback, failed_callback, ops_count,
                             clovis_api) {
    request_id = req ? req->get_request_id() : "";
    s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  }

  ~S3AsyncClovisKVSWriterContext() {
    s3_log(S3_LOG_DEBUG, request_id, "Destructor\n");
  }
};

enum class S3ClovisKVSWriterOpState {
  start,
  failed_to_launch,
  failed,
  created,
  deleted,
  exists,   // Key already exists
  missing,  // Key does not exists
  deleting  // Key is being deleted.
};

class S3ClovisKVSWriter {
 private:
  std::vector<struct m0_uint128> oid_list;
  std::vector<std::string> keys_list;  // used in delete multiple KV

  std::shared_ptr<RequestObject> request;
  std::shared_ptr<ClovisAPI> s3_clovis_api;
  std::unique_ptr<S3AsyncClovisKVSWriterContext> writer_context;
  std::unique_ptr<S3SyncClovisKVSWriterContext> sync_writer_context;
  std::unique_ptr<S3AsyncClovisKVSWriterContext> sync_context;
  std::string kvs_key;
  std::string kvs_value;

  std::string request_id;
  // Used to report to caller
  std::function<void()> handler_on_success;
  std::function<void()> handler_on_failed;

  S3ClovisKVSWriterOpState state;

  struct s3_clovis_idx_context* idx_ctx;

  void clean_up_contexts();

 public:
  S3ClovisKVSWriter(std::shared_ptr<RequestObject> req,
                    std::shared_ptr<ClovisAPI> clovis_api = nullptr);

  S3ClovisKVSWriter(std::string request_id,
                    std::shared_ptr<ClovisAPI> clovis_api = nullptr);
  virtual ~S3ClovisKVSWriter();

  virtual S3ClovisKVSWriterOpState get_state() { return state; }

  struct m0_uint128 get_oid() {
    return oid_list[0];
  }

  // async create
  virtual void create_index(std::string index_name,
                            std::function<void(void)> on_success,
                            std::function<void(void)> on_failed);
  void create_index_successful();
  void create_index_failed();

  virtual void create_index_with_oid(struct m0_uint128 idx_id,
                                     std::function<void(void)> on_success,
                                     std::function<void(void)> on_failed);

  // Sync clovis is currently done using clovis_idx_op

  // void sync_index(std::function<void(void)> on_success,
  //                 std::function<void(void)> on_failed, int index_count = 1);
  // void sync_index_successful();
  // void sync_index_failed();

  // void sync_keyval(std::function<void(void)> on_success,
  //                  std::function<void(void)> on_failed);
  // void sync_keyval_successful();
  // void sync_keyval_failed();

  // async delete
  virtual void delete_index(struct m0_uint128 idx_oid,
                            std::function<void(void)> on_success,
                            std::function<void(void)> on_failed);
  // void delete_index(std::string index_name,
  //                   std::function<void(void)> on_success,
  //                   std::function<void(void)> on_failed);
  void delete_index_successful();
  void delete_index_failed();

  virtual void delete_indexes(std::vector<struct m0_uint128> oids,
                              std::function<void(void)> on_success,
                              std::function<void(void)> on_failed);
  void delete_indexes_successful();
  void delete_indexes_failed();

  virtual void put_keyval(struct m0_uint128 oid,
                          const std::map<std::string, std::string>& kv_list,
                          std::function<void(void)> on_success,
                          std::function<void(void)> on_failed);
  // Async save operation.
  virtual void put_keyval(struct m0_uint128 oid, std::string key,
                          std::string val, std::function<void(void)> on_success,
                          std::function<void(void)> on_failed);
  virtual int put_keyval_impl(const std::map<std::string, std::string>& kv_list,
                              bool is_async);

  void put_keyval_successful();
  void put_keyval_failed();

  // Sync save operation.
  virtual int put_keyval_sync(
      struct m0_uint128 oid, const std::map<std::string, std::string>& kv_list);
  // Async delete operation.
  void delete_keyval(struct m0_uint128 oid, std::string key,
                     std::function<void(void)> on_success,
                     std::function<void(void)> on_failed);

  virtual void delete_keyval(struct m0_uint128 oid,
                             std::vector<std::string> keys,
                             std::function<void(void)> on_success,
                             std::function<void(void)> on_failed);

  void delete_keyval_successful();
  void delete_keyval_failed();

  void set_up_key_value_store(struct s3_clovis_kvs_op_context* kvs_ctx,
                              const std::string& key, const std::string& val,
                              size_t pos = 0);

  virtual int get_op_ret_code_for(int index) {
    return writer_context->get_errno_for(index);
  }

  virtual int get_op_ret_code_for_del_kv(int key_i) {
    return writer_context->get_clovis_kvs_op_ctx()->rcs[key_i];
  }

  // For Testing purpose
  FRIEND_TEST(S3ClovisKvsWritterTest, Constructor);
  FRIEND_TEST(S3ClovisKvsWritterTest, CleanupContexts);
  FRIEND_TEST(S3ClovisKvsWritterTest, CreateIndexIdxPresent);
  FRIEND_TEST(S3ClovisKvsWritterTest, CreateIndex);
  FRIEND_TEST(S3ClovisKvsWritterTest, CreateIndexSuccessful);
  FRIEND_TEST(S3ClovisKvsWritterTest, CreateIndexEntityCreateFailed);
  FRIEND_TEST(S3ClovisKvsWritterTest, CreateIndexFail);
  FRIEND_TEST(S3ClovisKvsWritterTest, CreateIndexFailExists);
  FRIEND_TEST(S3ClovisKvsWritterTest, SyncIndex);
  FRIEND_TEST(S3ClovisKvsWritterTest, SyncIndexSuccessful);
  FRIEND_TEST(S3ClovisKvsWritterTest, SyncIndexFailedMissingMetadata);
  FRIEND_TEST(S3ClovisKvsWritterTest, SyncIndexFailedFailedMetadata);
  FRIEND_TEST(S3ClovisKvsWritterTest, PutKeyVal);
  FRIEND_TEST(S3ClovisKvsWritterTest, PutKeyValSuccessful);
  FRIEND_TEST(S3ClovisKvsWritterTest, PutKeyValFailed);
  FRIEND_TEST(S3ClovisKvsWritterTest, PutKeyValEmpty);
  FRIEND_TEST(S3ClovisKvsWritterTest, DelIndexIdxPresent);
  FRIEND_TEST(S3ClovisKvsWritterTest, DelIndexEntityDeleteFailed);
  FRIEND_TEST(S3ClovisKvsWritterTest, DelIndexFailed);
  FRIEND_TEST(S3ClovisKvsWritterTest, SyncKeyVal);
  FRIEND_TEST(S3ClovisKvsWritterTest, SyncKeyvalSuccessful);
  FRIEND_TEST(S3ClovisKvsWritterTest, SyncKeyValFailed);
  FRIEND_TEST(S3ClovisKvsWritterTest, DelKeyVal);
  FRIEND_TEST(S3ClovisKvsWritterTest, DelKeyValSuccess);
  FRIEND_TEST(S3ClovisKvsWritterTest, DelKeyValFailed);
  FRIEND_TEST(S3ClovisKvsWritterTest, DelKeyValEmpty);
  FRIEND_TEST(S3BucketMetadataV1Test, CreateBucketListIndexSuccessful);
  FRIEND_TEST(S3PartMetadataTest, CreatePartIndexSuccessful);
  FRIEND_TEST(S3PartMetadataTest, CreatePartIndexSuccessfulOnlyCreateIndex);
  FRIEND_TEST(S3PartMetadataTest, CreatePartIndexSuccessfulSaveMetadata);
  FRIEND_TEST(S3NewAccountRegisterNotifyActionTest,
              CreateBucketListIndexSuccessful);
};

#endif
