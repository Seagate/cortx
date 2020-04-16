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

#include <unistd.h>

#include "s3_common.h"

#include "s3_clovis_kvs_writer.h"
#include "s3_clovis_rw_common.h"
#include "s3_option.h"
#include "s3_uri_to_mero_oid.h"
#include "s3_stats.h"

extern struct m0_clovis_realm clovis_uber_realm;
extern struct m0_clovis_container clovis_container;

S3ClovisKVSWriter::S3ClovisKVSWriter(std::shared_ptr<RequestObject> req,
                                     std::shared_ptr<ClovisAPI> clovis_api)
    : request(req), state(S3ClovisKVSWriterOpState::start), idx_ctx(nullptr) {
  request_id = request->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  if (clovis_api) {
    s3_clovis_api = clovis_api;
  } else {
    s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
  }
}

S3ClovisKVSWriter::S3ClovisKVSWriter(std::string req_id,
                                     std::shared_ptr<ClovisAPI> clovis_api)
    : state(S3ClovisKVSWriterOpState::start), idx_ctx(nullptr) {
  request_id = std::move(req_id);
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  if (clovis_api) {
    s3_clovis_api = std::move(clovis_api);
  } else {
    s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
  }
}

S3ClovisKVSWriter::~S3ClovisKVSWriter() {
  s3_log(S3_LOG_DEBUG, request_id, "Destructor\n");
  clean_up_contexts();
}

void S3ClovisKVSWriter::clean_up_contexts() {
  writer_context = nullptr;
  sync_context = nullptr;
  if (idx_ctx) {
    for (size_t i = 0; i < idx_ctx->idx_count; i++) {
      s3_clovis_api->clovis_idx_fini(&idx_ctx->idx[i]);
    }
    free_idx_context(idx_ctx);
    idx_ctx = nullptr;
  }
}

void S3ClovisKVSWriter::create_index(std::string index_name,
                                     std::function<void(void)> on_success,
                                     std::function<void(void)> on_failed) {
  s3_log(S3_LOG_INFO, request_id, "Entering with index_name = %s\n",
         index_name.c_str());

  struct m0_uint128 id = {0ULL, 0ULL};
  S3UriToMeroOID(s3_clovis_api, index_name.c_str(), request_id, &id,
                 S3ClovisEntityType::index);

  create_index_with_oid(id, on_success, on_failed);

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::create_index_with_oid(
    struct m0_uint128 idx_oid, std::function<void(void)> on_success,
    std::function<void(void)> on_failed) {
  s3_log(S3_LOG_INFO, request_id,
         "Entering with idx_oid = %" SCNx64 " : %" SCNx64 "\n", idx_oid.u_hi,
         idx_oid.u_lo);
  int rc = 0;
  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  oid_list.clear();
  oid_list.push_back(idx_oid);

  if (idx_ctx) {
    // clean up any old allocations
    clean_up_contexts();
  }
  idx_ctx = create_idx_context(1);

  writer_context.reset(new S3AsyncClovisKVSWriterContext(
      request, std::bind(&S3ClovisKVSWriter::create_index_successful, this),
      std::bind(&S3ClovisKVSWriter::create_index_failed, this), 1,
      s3_clovis_api));

  struct s3_clovis_idx_op_context *idx_op_ctx =
      writer_context->get_clovis_idx_op_ctx();

  struct s3_clovis_context_obj *op_ctx = (struct s3_clovis_context_obj *)calloc(
      1, sizeof(struct s3_clovis_context_obj));

  op_ctx->op_index_in_launch = 0;
  op_ctx->application_context = (void *)writer_context.get();

  idx_op_ctx->cbs->oop_executed = NULL;
  idx_op_ctx->cbs->oop_stable = s3_clovis_op_stable;
  idx_op_ctx->cbs->oop_failed = s3_clovis_op_failed;

  s3_clovis_api->clovis_idx_init(&(idx_ctx->idx[0]), &clovis_uber_realm,
                                 &idx_oid);

  rc = s3_clovis_api->clovis_entity_create(&(idx_ctx->idx[0].in_entity),
                                           &(idx_op_ctx->ops[0]));
  if (rc != 0) {
    state = S3ClovisKVSWriterOpState::failed_to_launch;
    s3_log(S3_LOG_ERROR, request_id,
           "clovis_entity_create failed with return code: (%d)\n", rc);
    s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
    return;
  }

  idx_op_ctx->ops[0]->op_datum = (void *)op_ctx;
  s3_clovis_api->clovis_op_setup(idx_op_ctx->ops[0], idx_op_ctx->cbs, 0);

  writer_context->start_timer_for("create_index_op");

  s3_clovis_api->clovis_op_launch(request->addb_request_id, idx_op_ctx->ops, 1,
                                  ClovisOpType::createidx);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::create_index_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_stats_inc("create_index_op_success_count");
  state = S3ClovisKVSWriterOpState::created;
  this->handler_on_success();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::create_index_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (state != S3ClovisKVSWriterOpState::failed_to_launch) {
    if (writer_context->get_errno_for(0) == -EEXIST) {
      state = S3ClovisKVSWriterOpState::exists;
      s3_log(S3_LOG_DEBUG, request_id, "Index already exists\n");
    } else {
      s3_log(S3_LOG_DEBUG, request_id, "Index creation failed\n");
      state = S3ClovisKVSWriterOpState::failed;
    }
  }
  this->handler_on_failed();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

// Sync clovis is currently done using clovis_idx_op

// void S3ClovisKVSWriter::sync_index(std::function<void(void)> on_success,
//                                    std::function<void(void)> on_failed,
//                                    int index_count) {
//   s3_log(S3_LOG_INFO, request_id, "Entering with index_count = %d\n",
//          index_count);
//   int rc = 0;
//   this->handler_on_success = on_success;
//   this->handler_on_failed = on_failed;
//   sync_context.reset(new S3AsyncClovisKVSWriterContext(
//       request, std::bind(&S3ClovisKVSWriter::sync_index_successful, this),
//       std::bind(&S3ClovisKVSWriter::sync_index_failed, this), 1,
//       s3_clovis_api));
//   struct s3_clovis_idx_op_context *idx_op_ctx =
//       sync_context->get_clovis_idx_op_ctx();

//   struct s3_clovis_context_obj *op_ctx = (struct s3_clovis_context_obj
// *)calloc(
//       1, sizeof(struct s3_clovis_context_obj));
//   op_ctx->op_index_in_launch = 0;
//   op_ctx->application_context = (void *)sync_context.get();

//   idx_op_ctx->cbs->oop_executed = NULL;
//   idx_op_ctx->cbs->oop_stable = s3_clovis_op_stable;
//   idx_op_ctx->cbs->oop_failed = s3_clovis_op_failed;
//   rc = s3_clovis_api->clovis_sync_op_init(&idx_op_ctx->sync_op);
//   if (rc != 0) {
//     s3_log(S3_LOG_ERROR, request_id, "m0_clovis_sync_op_init\n");
//     state = S3ClovisKVSWriterOpState::failed_to_launch;
//     s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
//     return;
//   }
//   for (int i = 0; i < index_count; i++) {
//     rc = s3_clovis_api->clovis_sync_entity_add(idx_op_ctx->sync_op,
//                                                &(idx_ctx->idx[i].in_entity));
//     if (rc != 0) {
//       s3_log(S3_LOG_ERROR, request_id, "m0_clovis_sync_entity_add failed\n");
//       state = S3ClovisKVSWriterOpState::failed_to_launch;
//       s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
//       return;
//     }
//   }
//   idx_op_ctx->sync_op->op_datum = (void *)op_ctx;

//   s3_clovis_api->clovis_op_setup(idx_op_ctx->sync_op, idx_op_ctx->cbs, 0);
//   sync_context->start_timer_for("sync_index_op");
//   s3_clovis_api->clovis_op_launch(request->addb_request_id,
//                                   &idx_op_ctx->sync_op, 1,
//                                   ClovisOpType::createidx);
//   s3_log(S3_LOG_DEBUG, "", "Exiting\n");
// }

// void S3ClovisKVSWriter::sync_index_successful() {
//   s3_log(S3_LOG_INFO, request_id, "Entering\n");
//   s3_stats_inc("sync_index_op_success_count");
//   if (state == S3ClovisKVSWriterOpState::deleting) {
//     state = S3ClovisKVSWriterOpState::deleted;
//   } else {
//     state = S3ClovisKVSWriterOpState::saved;
//   }
//   this->handler_on_success();
//   s3_log(S3_LOG_DEBUG, "", "Exiting\n");
// }

// void S3ClovisKVSWriter::sync_index_failed() {
//   s3_log(S3_LOG_INFO, request_id, "Entering\n");
//   if (state != S3ClovisKVSWriterOpState::failed_to_launch) {
//     if (sync_context->get_errno_for(0) == -ENOENT) {
//       state = S3ClovisKVSWriterOpState::missing;
//       s3_log(S3_LOG_DEBUG, request_id, "Index syncing failed as its
// missing\n");
//     } else {
//       s3_log(S3_LOG_DEBUG, request_id, "Index syncing failed\n");
//       state = S3ClovisKVSWriterOpState::failed;
//     }
//   }
//   this->handler_on_failed();
//   s3_log(S3_LOG_DEBUG, "", "Exiting\n");
// }

void S3ClovisKVSWriter::delete_index(struct m0_uint128 idx_oid,
                                     std::function<void(void)> on_success,
                                     std::function<void(void)> on_failed) {
  s3_log(S3_LOG_INFO, request_id,
         "Entering with idx_oid = %" SCNx64 " : %" SCNx64 "\n", idx_oid.u_hi,
         idx_oid.u_lo);
  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  oid_list.clear();
  oid_list.push_back(idx_oid);
  state = S3ClovisKVSWriterOpState::deleting;

  if (idx_ctx) {
    // clean up any old allocations
    clean_up_contexts();
  }
  idx_ctx = create_idx_context(1);

  writer_context.reset(new S3AsyncClovisKVSWriterContext(
      request, std::bind(&S3ClovisKVSWriter::delete_index_successful, this),
      std::bind(&S3ClovisKVSWriter::delete_index_failed, this), 1,
      s3_clovis_api));

  struct s3_clovis_idx_op_context *idx_op_ctx =
      writer_context->get_clovis_idx_op_ctx();

  struct s3_clovis_context_obj *op_ctx = (struct s3_clovis_context_obj *)calloc(
      1, sizeof(struct s3_clovis_context_obj));

  op_ctx->op_index_in_launch = 0;
  op_ctx->application_context = (void *)writer_context.get();

  idx_op_ctx->cbs->oop_executed = NULL;
  idx_op_ctx->cbs->oop_stable = s3_clovis_op_stable;
  idx_op_ctx->cbs->oop_failed = s3_clovis_op_failed;

  s3_clovis_api->clovis_idx_init(&(idx_ctx->idx[0]), &clovis_uber_realm,
                                 &idx_oid);
  int rc = s3_clovis_api->clovis_entity_open(&(idx_ctx->idx[0].in_entity),
                                             &(idx_op_ctx->ops[0]));
  if (rc != 0) {
    s3_log(S3_LOG_ERROR, request_id,
           "clovis_entity_open failed with return code: (%d)\n", rc);
    state = S3ClovisKVSWriterOpState::failed_to_launch;
    s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
    return;
  }

  rc = s3_clovis_api->clovis_entity_delete(&(idx_ctx->idx[0].in_entity),
                                           &(idx_op_ctx->ops[0]));
  if (rc != 0) {
    s3_log(S3_LOG_ERROR, request_id,
           "clovis_entity_delete failed with return code: (%d)\n", rc);
    state = S3ClovisKVSWriterOpState::failed_to_launch;
    s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
    return;
  }

  idx_op_ctx->ops[0]->op_datum = (void *)op_ctx;

  s3_clovis_api->clovis_op_setup(idx_op_ctx->ops[0], idx_op_ctx->cbs, 0);
  writer_context->start_timer_for("delete_index_op");

  s3_clovis_api->clovis_op_launch(request->addb_request_id, idx_op_ctx->ops, 1,
                                  ClovisOpType::deleteidx);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::delete_index_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  state = S3ClovisKVSWriterOpState::deleted;
  this->handler_on_success();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::delete_index_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (state != S3ClovisKVSWriterOpState::failed_to_launch) {
    if (writer_context->get_errno_for(0) == -ENOENT) {
      s3_log(S3_LOG_DEBUG, request_id, "Index doesn't exists\n");
      state = S3ClovisKVSWriterOpState::missing;
    } else {
      s3_log(S3_LOG_ERROR, request_id, "Deletion of Index failed\n");
      state = S3ClovisKVSWriterOpState::failed;
    }
  }
  this->handler_on_failed();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::delete_indexes(std::vector<struct m0_uint128> oids,
                                       std::function<void(void)> on_success,
                                       std::function<void(void)> on_failed) {
  s3_log(S3_LOG_INFO, request_id, "Entering with %zu indexes\n", oids.size());

  {
    for (auto &oid : oids) {
      s3_log(S3_LOG_DEBUG, request_id, "oid = %" SCNx64 " : %" SCNx64 "\n",
             oid.u_hi, oid.u_lo);
    }
  }

  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  state = S3ClovisKVSWriterOpState::deleting;
  oid_list.clear();
  oid_list = oids;

  if (idx_ctx) {
    // clean up any old allocations
    clean_up_contexts();
  }
  idx_ctx = create_idx_context(oid_list.size());

  writer_context.reset(new S3AsyncClovisKVSWriterContext(
      request, std::bind(&S3ClovisKVSWriter::delete_indexes_successful, this),
      std::bind(&S3ClovisKVSWriter::delete_indexes_failed, this), oids.size(),
      s3_clovis_api));

  struct s3_clovis_idx_op_context *idx_op_ctx =
      writer_context->get_clovis_idx_op_ctx();

  size_t ops_count = oid_list.size();
  for (size_t i = 0; i < ops_count; ++i) {
    struct s3_clovis_context_obj *op_ctx =
        (struct s3_clovis_context_obj *)calloc(
            1, sizeof(struct s3_clovis_context_obj));

    op_ctx->op_index_in_launch = i;
    op_ctx->application_context = (void *)writer_context.get();

    idx_op_ctx->cbs[i].oop_executed = NULL;
    idx_op_ctx->cbs[i].oop_stable = s3_clovis_op_stable;
    idx_op_ctx->cbs[i].oop_failed = s3_clovis_op_failed;

    s3_clovis_api->clovis_idx_init(&idx_ctx->idx[i], &clovis_uber_realm,
                                   &oid_list[i]);
    int rc = s3_clovis_api->clovis_entity_open(&(idx_ctx->idx[i].in_entity),
                                               &(idx_op_ctx->ops[i]));
    if (rc != 0) {
      s3_log(S3_LOG_ERROR, request_id,
             "clovis_entity_open failed with return code: (%d)\n", rc);
      state = S3ClovisKVSWriterOpState::failed_to_launch;
      s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
      return;
    }
    rc = s3_clovis_api->clovis_entity_delete(&(idx_ctx->idx[i].in_entity),
                                             &(idx_op_ctx->ops[i]));
    if (rc != 0) {
      s3_log(S3_LOG_ERROR, request_id,
             "clovis_entity_delete failed with return code: (%d)\n", rc);
      state = S3ClovisKVSWriterOpState::failed_to_launch;
      s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
      return;
    }

    idx_op_ctx->ops[i]->op_datum = (void *)op_ctx;
    s3_clovis_api->clovis_op_setup(idx_op_ctx->ops[i], &idx_op_ctx->cbs[i], 0);
  }

  writer_context->start_timer_for("delete_index_op");

  s3_clovis_api->clovis_op_launch(request->addb_request_id, idx_op_ctx->ops,
                                  oids.size(), ClovisOpType::deleteidx);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::delete_indexes_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  state = S3ClovisKVSWriterOpState::deleted;
  this->handler_on_success();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::delete_indexes_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_ERROR, request_id, "Deletion of Index failed\n");
  if (state != S3ClovisKVSWriterOpState::failed_to_launch) {
    state = S3ClovisKVSWriterOpState::failed;
  }
  this->handler_on_failed();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::put_keyval(
    struct m0_uint128 oid, const std::map<std::string, std::string> &kv_list,
    std::function<void(void)> on_success, std::function<void(void)> on_failed) {
  s3_log(S3_LOG_INFO, request_id, "Entering with oid = %" SCNx64 " : %" SCNx64
                                  " and %zu key/value pairs\n",
         oid.u_hi, oid.u_lo, kv_list.size());
  {
    for (auto &kv : kv_list) {
      s3_log(S3_LOG_DEBUG, request_id, "key = %s, value = %s\n",
             kv.first.c_str(), kv.second.c_str());
    }
  }

  oid_list.clear();
  oid_list.push_back(oid);

  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  if (idx_ctx) {
    // clean up any old allocations
    clean_up_contexts();
  }
  idx_ctx = create_idx_context(1);

  writer_context.reset(new S3AsyncClovisKVSWriterContext(
      request, std::bind(&S3ClovisKVSWriter::put_keyval_successful, this),
      std::bind(&S3ClovisKVSWriter::put_keyval_failed, this), 1,
      s3_clovis_api));

  writer_context->init_kvs_write_op_ctx(kv_list.size());

  // Ret code can be ignored as its already handled in async case.
  put_keyval_impl(kv_list, true);
}

int S3ClovisKVSWriter::put_keyval_impl(
    const std::map<std::string, std::string> &kv_list, bool is_async) {

  int rc = 0;
  struct s3_clovis_idx_op_context *idx_op_ctx = NULL;
  struct s3_clovis_kvs_op_context *kvs_ctx = NULL;
  struct s3_clovis_context_obj *op_ctx = NULL;

  if (is_async) {

    idx_op_ctx = writer_context->get_clovis_idx_op_ctx();
    kvs_ctx = writer_context->get_clovis_kvs_op_ctx();
    op_ctx = (struct s3_clovis_context_obj *)calloc(
        1, sizeof(struct s3_clovis_context_obj));

    op_ctx->op_index_in_launch = 0;
    op_ctx->application_context = (void *)writer_context.get();

    // Operation callbacks are used in async mode.
    if (idx_op_ctx->cbs->oop_executed) {
      idx_op_ctx->cbs->oop_executed = NULL;
    }
    idx_op_ctx->cbs->oop_stable = s3_clovis_op_stable;
    idx_op_ctx->cbs->oop_failed = s3_clovis_op_failed;

  } else {

    idx_op_ctx = sync_writer_context->get_clovis_idx_op_ctx();
    kvs_ctx = sync_writer_context->get_clovis_kvs_op_ctx();
    op_ctx = (struct s3_clovis_context_obj *)calloc(
        1, sizeof(struct s3_clovis_context_obj));

    op_ctx->op_index_in_launch = 0;
    op_ctx->application_context = (void *)sync_writer_context.get();
  }

  size_t i = 0;
  for (auto &kv : kv_list) {
    set_up_key_value_store(kvs_ctx, kv.first, kv.second, i);
    i++;
  }
  s3_clovis_api->clovis_idx_init(&(idx_ctx->idx[0]), &clovis_container.co_realm,
                                 &oid_list[0]);

  rc = s3_clovis_api->clovis_idx_op(
      &(idx_ctx->idx[0]), M0_CLOVIS_IC_PUT, kvs_ctx->keys, kvs_ctx->values,
      kvs_ctx->rcs, M0_OIF_OVERWRITE | M0_OIF_SYNC_WAIT, &(idx_op_ctx->ops[0]));
  if (rc != 0) {
    s3_log(S3_LOG_ERROR, request_id, "m0_clovis_idx_op failed\n");
    state = S3ClovisKVSWriterOpState::failed_to_launch;
    s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
    return rc;
  } else {
    s3_log(S3_LOG_DEBUG, request_id, "m0_clovis_idx_op suceeded\n");
  }

  idx_op_ctx->ops[0]->op_datum = (void *)op_ctx;
  s3_clovis_api->clovis_op_setup(idx_op_ctx->ops[0], idx_op_ctx->cbs, 0);

  s3_log(S3_LOG_DEBUG, request_id, "clovis_op_setup done\n");

  if (is_async) {

    writer_context->start_timer_for("put_keyval");
  }

  s3_clovis_api->clovis_op_launch(
      (is_async ? request->addb_request_id : S3_ADDB_STARTUP_REQUESTS_ID),
      &(idx_op_ctx->ops[0]), 1, ClovisOpType::putkv);

  if (!is_async) {
    s3_log(S3_LOG_DEBUG, request_id, "Waiting for clovis put KV to complete\n");
    rc = s3_clovis_api->clovis_op_wait(
        (idx_op_ctx->ops[0]), M0_BITS(M0_CLOVIS_OS_FAILED, M0_CLOVIS_OS_STABLE),
        m0_time_from_now(S3Option::get_instance()->get_clovis_op_wait_period(),
                         0));
    if (rc < 0) {
      return rc;
    }
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return 0;
}

int S3ClovisKVSWriter::put_keyval_sync(
    struct m0_uint128 oid, const std::map<std::string, std::string> &kv_list) {
  s3_log(S3_LOG_INFO, request_id, "Entering with oid = %" SCNx64 " : %" SCNx64
                                  " and %zu key/value pairs\n",
         oid.u_hi, oid.u_lo, kv_list.size());

  /* For the moment sync kvs operation for fake kvs is not supported */
  assert(S3Option::get_instance()->is_sync_kvs_allowed());

  {
    for (const auto &kv : kv_list) {
      s3_log(S3_LOG_DEBUG, request_id, "key = %s, value = %s\n",
             kv.first.c_str(), kv.second.c_str());
    }
  }

  oid_list.clear();
  oid_list.push_back(oid);

  if (idx_ctx) {
    // clean up any old allocations
    clean_up_contexts();
  }
  idx_ctx = create_idx_context(1);

  sync_writer_context.reset(new S3SyncClovisKVSWriterContext(request_id, 1));

  sync_writer_context->init_kvs_write_op_ctx(kv_list.size());

  return put_keyval_impl(kv_list, false);
}

void S3ClovisKVSWriter::put_keyval(struct m0_uint128 oid, std::string key,
                                   std::string val,
                                   std::function<void(void)> on_success,
                                   std::function<void(void)> on_failed) {
  s3_log(S3_LOG_INFO, request_id, "Entering with oid = %" SCNx64 " : %" SCNx64
                                  " key = %s and value = %s\n",
         oid.u_hi, oid.u_lo, key.c_str(), val.c_str());

  int rc = 0;
  oid_list.clear();
  oid_list.push_back(oid);

  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  if (idx_ctx) {
    // clean up any old allocations
    clean_up_contexts();
  }
  idx_ctx = create_idx_context(1);

  writer_context.reset(new S3AsyncClovisKVSWriterContext(
      request, std::bind(&S3ClovisKVSWriter::put_keyval_successful, this),
      std::bind(&S3ClovisKVSWriter::put_keyval_failed, this), 1,
      s3_clovis_api));

  // Only one key value passed
  writer_context->init_kvs_write_op_ctx(1);

  struct s3_clovis_idx_op_context *idx_op_ctx =
      writer_context->get_clovis_idx_op_ctx();
  struct s3_clovis_kvs_op_context *kvs_ctx =
      writer_context->get_clovis_kvs_op_ctx();

  struct s3_clovis_context_obj *op_ctx = (struct s3_clovis_context_obj *)calloc(
      1, sizeof(struct s3_clovis_context_obj));

  op_ctx->op_index_in_launch = 0;
  op_ctx->application_context = (void *)writer_context.get();

  idx_op_ctx->cbs->oop_executed = NULL;
  idx_op_ctx->cbs->oop_stable = s3_clovis_op_stable;
  idx_op_ctx->cbs->oop_failed = s3_clovis_op_failed;

  set_up_key_value_store(kvs_ctx, key, val);

  s3_clovis_api->clovis_idx_init(&(idx_ctx->idx[0]), &clovis_container.co_realm,
                                 &oid_list[0]);

  rc = s3_clovis_api->clovis_idx_op(
      &(idx_ctx->idx[0]), M0_CLOVIS_IC_PUT, kvs_ctx->keys, kvs_ctx->values,
      kvs_ctx->rcs, M0_OIF_OVERWRITE, &(idx_op_ctx->ops[0]));
  if (rc != 0) {
    s3_log(S3_LOG_ERROR, request_id, "m0_clovis_idx_op failed\n");
    state = S3ClovisKVSWriterOpState::failed_to_launch;
    s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
    return;
  } else {
    s3_log(S3_LOG_DEBUG, request_id, "m0_clovis_idx_op suceeded\n");
  }

  idx_op_ctx->ops[0]->op_datum = (void *)op_ctx;
  s3_clovis_api->clovis_op_setup(idx_op_ctx->ops[0], idx_op_ctx->cbs, 0);

  writer_context->start_timer_for("put_keyval");

  s3_clovis_api->clovis_op_launch(request->addb_request_id, idx_op_ctx->ops, 1,
                                  ClovisOpType::putkv);

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::put_keyval_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_stats_inc("put_keyval_success_count");
  // todo: Add check, verify if (kvs_ctx->rcs == 0)
  // do this when cassandra + mero-kvs rcs implementation completed
  // in clovis
  state = S3ClovisKVSWriterOpState::created;
  this->handler_on_success();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::put_keyval_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_ERROR, request_id, "Writing of key value failed\n");
  if (state != S3ClovisKVSWriterOpState::failed_to_launch) {
    state = S3ClovisKVSWriterOpState::failed;
  }
  this->handler_on_failed();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

// Sync clovis is currently done using clovis_idx_op

// void S3ClovisKVSWriter::sync_keyval(std::function<void(void)> on_success,
//                                     std::function<void(void)> on_failed) {
//   s3_log(S3_LOG_INFO, request_id, "Entering\n");
//   int rc = 0;
//   this->handler_on_success = on_success;
//   this->handler_on_failed = on_failed;

//   sync_context.reset(new S3AsyncClovisKVSWriterContext(
//       request, std::bind(&S3ClovisKVSWriter::sync_keyval_successful, this),
//       std::bind(&S3ClovisKVSWriter::sync_keyval_failed, this), 1,
//       s3_clovis_api));

//   struct s3_clovis_idx_op_context *idx_op_ctx =
//       sync_context->get_clovis_idx_op_ctx();

//   struct s3_clovis_context_obj *op_ctx = (struct s3_clovis_context_obj
// *)calloc(
//       1, sizeof(struct s3_clovis_context_obj));

//   op_ctx->op_index_in_launch = 0;
//   op_ctx->application_context = (void *)sync_context.get();

//   idx_op_ctx->cbs->oop_executed = NULL;
//   idx_op_ctx->cbs->oop_stable = s3_clovis_op_stable;
//   idx_op_ctx->cbs->oop_failed = s3_clovis_op_failed;

//   rc = s3_clovis_api->clovis_sync_op_init(&idx_op_ctx->sync_op);
//   if (rc != 0) {
//     s3_log(S3_LOG_ERROR, request_id, "m0_clovis_sync_op_init failed\n");
//     state = S3ClovisKVSWriterOpState::failed_to_launch;
//     s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
//     return;
//   }

//   rc = s3_clovis_api->clovis_sync_op_add(
//       idx_op_ctx->sync_op, writer_context->get_clovis_idx_op_ctx()->ops[0]);
//   if (rc != 0) {
//     s3_log(S3_LOG_ERROR, request_id, "m0_clovis_sync_entity_add failed\n");
//     state = S3ClovisKVSWriterOpState::failed_to_launch;
//     s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
//     return;
//   }

//   idx_op_ctx->sync_op->op_datum = (void *)op_ctx;
//   s3_clovis_api->clovis_op_setup(idx_op_ctx->sync_op, idx_op_ctx->cbs, 0);
//   sync_context->start_timer_for("sync_keyval_op");
//   if (state == S3ClovisKVSWriterOpState::deleting) {
//     s3_clovis_api->clovis_op_launch(request->addb_request_id,
//                                     &idx_op_ctx->sync_op, 1,
//                                     ClovisOpType::deletekv);
//   } else {
//     s3_clovis_api->clovis_op_launch(
//         request->addb_request_id, &idx_op_ctx->sync_op, 1,
// ClovisOpType::putkv);
//   }

//   s3_log(S3_LOG_DEBUG, "", "Exiting\n");
// }

// void S3ClovisKVSWriter::sync_keyval_successful() {
//   s3_log(S3_LOG_INFO, request_id, "Entering\n");
//   s3_stats_inc("sync_keyval_op_success_count");
//   if (state == S3ClovisKVSWriterOpState::deleting) {
//     state = S3ClovisKVSWriterOpState::deleted;
//   } else {
//     state = S3ClovisKVSWriterOpState::saved;
//   }
//   s3_log(S3_LOG_DEBUG, "", "Exiting\n");
// }

// void S3ClovisKVSWriter::sync_keyval_failed() {
//   s3_log(S3_LOG_INFO, request_id, "Entering\n");
//   if (state != S3ClovisKVSWriterOpState::failed_to_launch) {
//     state = S3ClovisKVSWriterOpState::failed;
//   }
//   this->handler_on_failed();
//   s3_log(S3_LOG_DEBUG, "", "Exiting\n");
// }

void S3ClovisKVSWriter::delete_keyval(struct m0_uint128 oid, std::string key,
                                      std::function<void(void)> on_success,
                                      std::function<void(void)> on_failed) {
  s3_log(S3_LOG_INFO, request_id,
         "Entering with oid %" SCNx64 " : %" SCNx64 " and key %s\n", oid.u_hi,
         oid.u_lo, key.c_str());
  std::vector<std::string> keys;
  keys.push_back(key);

  delete_keyval(oid, keys, on_success, on_failed);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::delete_keyval(struct m0_uint128 oid,
                                      std::vector<std::string> keys,
                                      std::function<void(void)> on_success,
                                      std::function<void(void)> on_failed) {
  s3_log(S3_LOG_INFO, request_id,
         "Entering with oid %" SCNx64 " : %" SCNx64 " and %zu keys\n", oid.u_hi,
         oid.u_lo, keys.size());
  int rc;
  for (auto key : keys) {
    s3_log(S3_LOG_DEBUG, request_id, "key = %s\n", key.c_str());
  }

  state = S3ClovisKVSWriterOpState::deleting;
  oid_list.clear();
  oid_list.push_back(oid);
  keys_list.clear();
  keys_list = keys;

  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  if (idx_ctx) {
    // clean up any old allocations
    clean_up_contexts();
  }
  idx_ctx = create_idx_context(1);

  writer_context.reset(new S3AsyncClovisKVSWriterContext(
      request, std::bind(&S3ClovisKVSWriter::delete_keyval_successful, this),
      std::bind(&S3ClovisKVSWriter::delete_keyval_failed, this), 1,
      s3_clovis_api));

  // Only one key value passed
  writer_context->init_kvs_write_op_ctx(keys_list.size());

  struct s3_clovis_idx_op_context *idx_op_ctx =
      writer_context->get_clovis_idx_op_ctx();
  struct s3_clovis_kvs_op_context *kvs_ctx =
      writer_context->get_clovis_kvs_op_ctx();

  struct s3_clovis_context_obj *op_ctx = (struct s3_clovis_context_obj *)calloc(
      1, sizeof(struct s3_clovis_context_obj));

  op_ctx->op_index_in_launch = 0;
  op_ctx->application_context = (void *)writer_context.get();

  idx_op_ctx->cbs->oop_executed = NULL;
  idx_op_ctx->cbs->oop_stable = s3_clovis_op_stable;
  idx_op_ctx->cbs->oop_failed = s3_clovis_op_failed;

  int i = 0;
  for (auto key : keys_list) {
    kvs_ctx->keys->ov_vec.v_count[i] = key.length();
    kvs_ctx->keys->ov_buf[i] = calloc(1, key.length());
    memcpy(kvs_ctx->keys->ov_buf[i], (void *)key.c_str(), key.length());
    ++i;
  }

  s3_clovis_api->clovis_idx_init(&(idx_ctx->idx[0]), &clovis_container.co_realm,
                                 &oid_list[0]);
  rc = s3_clovis_api->clovis_idx_op(&(idx_ctx->idx[0]), M0_CLOVIS_IC_DEL,
                                    kvs_ctx->keys, NULL, kvs_ctx->rcs,
                                    M0_OIF_SYNC_WAIT, &(idx_op_ctx->ops[0]));
  if (rc != 0) {
    s3_log(S3_LOG_ERROR, request_id, "m0_clovis_idx_op failed\n");
    state = S3ClovisKVSWriterOpState::failed_to_launch;
    s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
    return;
  } else {
    s3_log(S3_LOG_DEBUG, request_id, "m0_clovis_idx_op suceeded\n");
  }

  idx_op_ctx->ops[0]->op_datum = (void *)op_ctx;
  s3_clovis_api->clovis_op_setup(idx_op_ctx->ops[0], idx_op_ctx->cbs, 0);

  writer_context->start_timer_for("delete_keyval");

  s3_clovis_api->clovis_op_launch(request->addb_request_id, idx_op_ctx->ops, 1,
                                  ClovisOpType::deletekv);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::delete_keyval_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // todo: Add check, verify if (kvs_ctx->rcs == 0)
  // do this when cassandra + mero-kvs rcs implementation completed
  // in clovis
  state = S3ClovisKVSWriterOpState::deleted;
  this->handler_on_success();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::delete_keyval_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (state != S3ClovisKVSWriterOpState::failed_to_launch) {
    if (writer_context->get_errno_for(0) == -ENOENT) {
      s3_log(S3_LOG_DEBUG, request_id, "The key doesn't exist\n");
      state = S3ClovisKVSWriterOpState::missing;
    } else {
      s3_log(S3_LOG_ERROR, request_id, "Deleting the value for a key failed\n");
      state = S3ClovisKVSWriterOpState::failed;
    }
  }
  this->handler_on_failed();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisKVSWriter::set_up_key_value_store(
    struct s3_clovis_kvs_op_context *kvs_ctx, const std::string &key,
    const std::string &val, size_t pos) {
  // TODO - clean up these buffers
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  kvs_ctx->keys->ov_vec.v_count[pos] = key.length();
  kvs_ctx->keys->ov_buf[pos] = (char *)malloc(key.length());
  memcpy(kvs_ctx->keys->ov_buf[pos], (void *)key.c_str(), key.length());

  kvs_ctx->values->ov_vec.v_count[pos] = val.length();
  kvs_ctx->values->ov_buf[pos] = (char *)malloc(val.length());
  memcpy(kvs_ctx->values->ov_buf[pos], (void *)val.c_str(), val.length());

  s3_log(S3_LOG_DEBUG, request_id, "Keys and value in clovis buffer\n");
  s3_log(S3_LOG_DEBUG, request_id, "kvs_ctx->keys->ov_buf[%lu] = %s\n", pos,
         std::string((char *)kvs_ctx->keys->ov_buf[pos],
                     kvs_ctx->keys->ov_vec.v_count[pos]).c_str());
  s3_log(S3_LOG_DEBUG, request_id, "kvs_ctx->vals->ov_buf[%lu] = %s\n", pos,
         std::string((char *)kvs_ctx->values->ov_buf[pos],
                     kvs_ctx->values->ov_vec.v_count[pos]).c_str());
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
