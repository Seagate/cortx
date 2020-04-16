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

#include "s3_clovis_reader.h"
#include "s3_clovis_rw_common.h"
#include "s3_option.h"
#include "s3_uri_to_mero_oid.h"
#include "s3_addb.h"

extern struct m0_clovis_realm clovis_uber_realm;

S3ClovisReader::S3ClovisReader(std::shared_ptr<RequestObject> req,
                               struct m0_uint128 id, int layoutid,
                               std::shared_ptr<ClovisAPI> clovis_api)
    : request(req),
      s3_clovis_api(clovis_api),
      state(S3ClovisReaderOpState::start),
      clovis_rw_op_context(NULL),
      iteration_index(0),
      num_of_blocks_to_read(0),
      last_index(0),
      is_object_opened(false),
      obj_ctx(nullptr) {
  request_id = request->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  if (clovis_api) {
    s3_clovis_api = clovis_api;
  } else {
    s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
  }

  oid = id;
  layout_id = layoutid;
}

S3ClovisReader::~S3ClovisReader() { clean_up_contexts(); }

void S3ClovisReader::clean_up_contexts() {
  // op contexts need to be free'ed before object
  open_context = nullptr;
  reader_context = nullptr;
  if (obj_ctx) {
    for (size_t i = 0; i < obj_ctx->obj_count; i++) {
      s3_clovis_api->clovis_obj_fini(&obj_ctx->objs[i]);
    }
    free_obj_context(obj_ctx);
    obj_ctx = nullptr;
  }
}

bool S3ClovisReader::read_object_data(size_t num_of_blocks,
                                      std::function<void(void)> on_success,
                                      std::function<void(void)> on_failed) {
  s3_log(S3_LOG_INFO, request_id,
         "Entering with num_of_blocks = %zu from last_index = %zu\n",
         num_of_blocks, last_index);

  bool rc = true;
  state = S3ClovisReaderOpState::reading;
  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  assert(this->handler_on_success != NULL);
  assert(this->handler_on_failed != NULL);

  num_of_blocks_to_read = num_of_blocks;

  if (is_object_opened) {
    rc = read_object();
  } else {
    int retcode =
        open_object(std::bind(&S3ClovisReader::open_object_successful, this),
                    std::bind(&S3ClovisReader::open_object_failed, this));
    if (retcode != 0) {
      this->handler_on_failed();
      rc = false;
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return rc;
}

bool S3ClovisReader::check_object_exist(std::function<void(void)> on_success,
                                        std::function<void(void)> on_failed) {
  bool rc = true;
  this->handler_on_success = on_success;
  this->handler_on_failed = on_failed;

  assert(this->handler_on_success != NULL);
  assert(this->handler_on_failed != NULL);

  if (is_object_opened) {
    this->handler_on_success();
  } else {
    int retcode =
        open_object(std::bind(&S3ClovisReader::open_object_successful, this),
                    std::bind(&S3ClovisReader::open_object_failed, this));
    if (retcode != 0) {
      rc = false;
    }
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return rc;
}

int S3ClovisReader::open_object(std::function<void(void)> on_success,
                                std::function<void(void)> on_failed) {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  int rc = 0;

  is_object_opened = false;

  // Reader always deals with one object
  if (obj_ctx) {
    // clean up any old allocations
    clean_up_contexts();
  }
  obj_ctx = create_obj_context(1);

  open_context.reset(
      new S3ClovisReaderContext(request, on_success, on_failed, layout_id));

  struct s3_clovis_context_obj *op_ctx = (struct s3_clovis_context_obj *)calloc(
      1, sizeof(struct s3_clovis_context_obj));

  op_ctx->op_index_in_launch = 0;
  op_ctx->application_context = (void *)open_context.get();
  struct s3_clovis_op_context *ctx = open_context->get_clovis_op_ctx();

  ctx->cbs[0].oop_executed = NULL;
  ctx->cbs[0].oop_stable = s3_clovis_op_stable;
  ctx->cbs[0].oop_failed = s3_clovis_op_failed;

  s3_clovis_api->clovis_obj_init(&obj_ctx->objs[0], &clovis_uber_realm, &oid,
                                 layout_id);

  rc = s3_clovis_api->clovis_entity_open(&(obj_ctx->objs[0].ob_entity),
                                         &(ctx->ops[0]));
  if (rc != 0) {
    s3_log(S3_LOG_WARN, request_id,
           "Clovis API: clovis_entity_open failed with error code %d\n", rc);
    state = S3ClovisReaderOpState::failed_to_launch;
    s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
    return rc;
  }

  ctx->ops[0]->op_datum = (void *)op_ctx;
  s3_clovis_api->clovis_op_setup(ctx->ops[0], &ctx->cbs[0], 0);

  s3_log(S3_LOG_INFO, request_id,
         "Clovis API: openobj(oid: ("
         "%" SCNx64 " : %" SCNx64 "))\n",
         oid.u_hi, oid.u_lo);

  s3_clovis_api->clovis_op_launch(request->addb_request_id, ctx->ops, 1,
                                  ClovisOpType::openobj);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return rc;
}

void S3ClovisReader::open_object_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_INFO, request_id,
         "Clovis API Successful: openobj(oid: ("
         "%" SCNx64 " : %" SCNx64 "))\n",
         oid.u_hi, oid.u_lo);
  is_object_opened = true;

  if (state == S3ClovisReaderOpState::reading) {
    if (!read_object()) {
      // read cannot be launched, out-of-memory
      if (state != S3ClovisReaderOpState::failed_to_launch) {
        this->handler_on_failed();
      }
    }
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  } else {
    this->handler_on_success();
  }
}

void S3ClovisReader::open_object_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (state != S3ClovisReaderOpState::failed_to_launch) {
    s3_log(S3_LOG_DEBUG, request_id, "errno = %d\n",
           open_context->get_errno_for(0));
    is_object_opened = false;
    if (open_context->get_errno_for(0) == -ENOENT) {
      state = S3ClovisReaderOpState::missing;
      s3_log(S3_LOG_DEBUG, request_id, "Object doesn't exists\n");
    } else {
      state = S3ClovisReaderOpState::failed;
      s3_log(S3_LOG_ERROR, request_id, "Object initialization failed\n");
    }
  }
  this->handler_on_failed();

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

bool S3ClovisReader::read_object() {
  int rc;
  s3_log(S3_LOG_INFO, request_id,
         "Entering with num_of_blocks_to_read = %zu from last_index = %zu\n",
         num_of_blocks_to_read, last_index);

  assert(is_object_opened);

  reader_context.reset(new S3ClovisReaderContext(
      request, std::bind(&S3ClovisReader::read_object_successful, this),
      std::bind(&S3ClovisReader::read_object_failed, this), layout_id));

  /* Read the requisite number of blocks from the entity */
  if (!reader_context->init_read_op_ctx(num_of_blocks_to_read, &last_index)) {
    // out-of-memory
    state = S3ClovisReaderOpState::ooo;
    s3_log(S3_LOG_ERROR, request_id,
           "Clovis API failed: openobj(oid: ("
           "%" SCNx64 " : %" SCNx64 "), out-of-memory)\n",
           oid.u_hi, oid.u_lo);
    return false;
  }

  struct s3_clovis_op_context *ctx = reader_context->get_clovis_op_ctx();
  struct s3_clovis_rw_op_context *rw_ctx =
      reader_context->get_clovis_rw_op_ctx();

  // Remember, so buffers can be iterated.
  clovis_rw_op_context = rw_ctx;
  iteration_index = 0;

  struct s3_clovis_context_obj *op_ctx = (struct s3_clovis_context_obj *)calloc(
      1, sizeof(struct s3_clovis_context_obj));

  op_ctx->op_index_in_launch = 0;
  op_ctx->application_context = (void *)reader_context.get();

  ctx->cbs[0].oop_executed = NULL;
  ctx->cbs[0].oop_stable = s3_clovis_op_stable;
  ctx->cbs[0].oop_failed = s3_clovis_op_failed;

  /* Create the read request */
  rc = s3_clovis_api->clovis_obj_op(&obj_ctx->objs[0], M0_CLOVIS_OC_READ,
                                    rw_ctx->ext, rw_ctx->data, rw_ctx->attr, 0,
                                    &ctx->ops[0]);
  if (rc != 0) {
    s3_log(S3_LOG_WARN, request_id,
           "Clovis API: clovis_obj_op failed with error code %d\n", rc);
    state = S3ClovisReaderOpState::failed_to_launch;
    s3_clovis_op_pre_launch_failure(op_ctx->application_context, rc);
    return false;
  }

  ctx->ops[0]->op_datum = (void *)op_ctx;
  s3_clovis_api->clovis_op_setup(ctx->ops[0], &ctx->cbs[0], 0);

  reader_context->start_timer_for("read_object_data");

  s3_log(S3_LOG_INFO, request_id,
         "Clovis API: readobj(operation: M0_CLOVIS_OC_READ, oid: ("
         "%" SCNx64 " : %" SCNx64 "))\n",
         oid.u_hi, oid.u_lo);

  s3_clovis_api->clovis_op_launch(request->addb_request_id, ctx->ops, 1,
                                  ClovisOpType::readobj);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return true;
}

void S3ClovisReader::read_object_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_INFO, request_id,
         "Clovis API Successful: readobj(oid: ("
         "%" SCNx64 " : %" SCNx64 "))\n",
         oid.u_hi, oid.u_lo);
  state = S3ClovisReaderOpState::success;
  this->handler_on_success();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3ClovisReader::read_object_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_DEBUG, request_id, "errno = %d\n",
         reader_context->get_errno_for(0));
  if (reader_context->get_errno_for(0) == -ENOENT) {
    s3_log(S3_LOG_DEBUG, request_id, "Object doesn't exist\n");
    state = S3ClovisReaderOpState::missing;
  } else {
    s3_log(S3_LOG_ERROR, request_id, "Reading of object failed\n");
    state = S3ClovisReaderOpState::failed;
  }
  this->handler_on_failed();
}

// Returns size of data in first block and 0 if there is no content,
// and content in data.
size_t S3ClovisReader::get_first_block(char **data) {
  iteration_index = 0;
  return get_next_block(data);
}

// Returns size of data in next block and -1 if there is no content or done
size_t S3ClovisReader::get_next_block(char **data) {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_DEBUG, request_id,
         "num_of_blocks_to_read = %zu from iteration_index = %zu\n",
         num_of_blocks_to_read, iteration_index);
  size_t data_read = 0;
  if (iteration_index == num_of_blocks_to_read) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return 0;
  }

  *data = (char *)clovis_rw_op_context->data->ov_buf[iteration_index];
  data_read = clovis_rw_op_context->data->ov_vec.v_count[iteration_index];
  iteration_index++;
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return data_read;
}
