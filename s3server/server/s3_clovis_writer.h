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
 * Original creation date: 1-Oct-2015
 */

#pragma once

#ifndef __S3_SERVER_S3_CLOVIS_WRITER_H__
#define __S3_SERVER_S3_CLOVIS_WRITER_H__

#include <deque>
#include <functional>
#include <memory>

#include "s3_asyncop_context_base.h"
#include "s3_clovis_context.h"
#include "s3_clovis_wrapper.h"
#include "s3_log.h"
#include "s3_md5_hash.h"
#include "s3_request_object.h"

class S3ClovisWriterContext : public S3AsyncOpContextBase {
  // Basic Operation context.
  struct s3_clovis_op_context* clovis_op_context = NULL;

  // Read/Write Operation context.
  struct s3_clovis_rw_op_context* clovis_rw_op_context = NULL;

 public:
  S3ClovisWriterContext(std::shared_ptr<RequestObject> req,
                        std::function<void()> success_callback,
                        std::function<void()> failed_callback,
                        int ops_count = 1,
                        std::shared_ptr<ClovisAPI> clovis_api = nullptr);

  ~S3ClovisWriterContext();

  struct s3_clovis_op_context* get_clovis_op_ctx();

  // Call this when you want to do write op.
  void init_write_op_ctx(size_t clovis_buf_count) {
    clovis_rw_op_context = create_basic_rw_op_ctx(clovis_buf_count, 0, false);
  }

  struct s3_clovis_rw_op_context* get_clovis_rw_op_ctx() {
    return clovis_rw_op_context;
  }
};

enum class S3ClovisWriterOpState {
  start,
  failed_to_launch,
  failed,
  creating,
  created,
  saved,
  writing,
  deleting,
  deleted,
  success,
  exists,   // Object already exists
  missing,  // Object does not exists
};

class S3ClovisWriter {

  std::shared_ptr<RequestObject> request;
  std::unique_ptr<S3ClovisWriterContext> open_context;
  std::unique_ptr<S3ClovisWriterContext> create_context;
  std::unique_ptr<S3ClovisWriterContext> writer_context;
  std::unique_ptr<S3ClovisWriterContext> delete_context;
  std::shared_ptr<ClovisAPI> s3_clovis_api;

  // Used to report to caller
  std::function<void()> handler_on_success;
  std::function<void()> handler_on_failed;

  std::vector<struct m0_uint128> oid_list;
  std::vector<int> layout_ids;

  S3ClovisWriterOpState state;

  std::string content_md5;
  uint64_t last_index;
  std::string request_id;
  // md5 for the content written to clovis.
  MD5hash md5crypt;

  // maintain state for debugging.
  size_t size_in_current_write;
  size_t total_written;
  size_t n_initialized_contexts = 0;

  bool is_object_opened;
  struct s3_clovis_obj_context* obj_ctx;

  void* place_holder_for_last_unit;

  // buffer currently used to write, will be freed on completion
  std::shared_ptr<S3AsyncBufferOptContainer> write_async_buffer;

  // Write - single object, delete - multiple objects supported
  int open_objects();
  void open_objects_successful();
  void open_objects_failed();

  void write_content();
  void write_content_successful();
  void write_content_failed();

  void delete_objects();
  void delete_objects_successful();
  void delete_objects_failed();

  void clean_up_contexts();

 public:
  // struct m0_uint128 id;
  S3ClovisWriter(std::shared_ptr<RequestObject> req,
                 struct m0_uint128 object_id, uint64_t offset = 0,
                 std::shared_ptr<ClovisAPI> clovis_api = nullptr);
  S3ClovisWriter(std::shared_ptr<RequestObject> req, uint64_t offset = 0,
                 std::shared_ptr<ClovisAPI> clovis_api = nullptr);
  virtual ~S3ClovisWriter();

  virtual S3ClovisWriterOpState get_state() { return state; }

  virtual struct m0_uint128 get_oid() {
    assert(oid_list.size() == 1);
    return oid_list[0];
  }

  virtual int get_layout_id() {
    assert(layout_ids.size() == 1);
    return layout_ids[0];
  }

  virtual void set_oid(struct m0_uint128 id) {
    is_object_opened = false;
    oid_list.clear();
    oid_list.push_back(id);
  }

  virtual void set_layout_id(int id) {
    layout_ids.clear();
    layout_ids.push_back(id);
  }

  // This concludes the md5 calculation
  virtual std::string get_content_md5() {
    // Complete MD5 computation and remember
    if (content_md5.empty()) {
      md5crypt.Finalize();
      content_md5 = md5crypt.get_md5_string();
    }
    s3_log(S3_LOG_DEBUG, request_id, "content_md5 of data written = %s\n",
           content_md5.c_str());
    return content_md5;
  }

  virtual std::string get_content_md5_base64() {
    return md5crypt.get_md5_base64enc_string();
  }

  // async create
  virtual void create_object(std::function<void(void)> on_success,
                             std::function<void(void)> on_failed, int layoutid);
  void create_object_successful();
  void create_object_failed();

  // Async save operation.
  virtual void write_content(std::function<void(void)> on_success,
                             std::function<void(void)> on_failed,
                             std::shared_ptr<S3AsyncBufferOptContainer> buffer);

  // Async delete operation.
  virtual void delete_object(std::function<void(void)> on_success,
                             std::function<void(void)> on_failed, int layoutid);

  virtual void delete_objects(std::vector<struct m0_uint128> oids,
                              std::vector<int> layoutids,
                              std::function<void(void)> on_success,
                              std::function<void(void)> on_failed);

  virtual int get_op_ret_code_for(int index);
  virtual int get_op_ret_code_for_delete_op(int index);

  void set_up_clovis_data_buffers(struct s3_clovis_rw_op_context* rw_ctx,
                                  std::deque<evbuffer*>& data_items,
                                  size_t clovis_buf_count);

  // For Testing purpose
  FRIEND_TEST(S3ClovisWriterTest, Constructor);
  FRIEND_TEST(S3ClovisWriterTest, Constructor2);
  FRIEND_TEST(S3ClovisWriterTest, CreateObjectTest);
  FRIEND_TEST(S3ClovisWriterTest, CreateObjectSuccessfulTest);
  FRIEND_TEST(S3ClovisWriterTest, CreateObjectFailedTest);
  FRIEND_TEST(S3ClovisWriterTest, CreateObjectEntityCreateFailTest);
  FRIEND_TEST(S3ClovisWriterTest, DeleteObjectTest);
  FRIEND_TEST(S3ClovisWriterTest, DeleteObjectSuccessfulTest);
  FRIEND_TEST(S3ClovisWriterTest, DeleteObjectFailedTest);
  FRIEND_TEST(S3ClovisWriterTest, DeleteObjectsTest);
  FRIEND_TEST(S3ClovisWriterTest, DeleteObjectsSuccessfulTest);
  FRIEND_TEST(S3ClovisWriterTest, DeleteObjectsFailedTest);
  FRIEND_TEST(S3ClovisWriterTest, DeleteObjectclovisEntityDeleteFailedTest);
  FRIEND_TEST(S3ClovisWriterTest, OpenObjectsTest);
  FRIEND_TEST(S3ClovisWriterTest, OpenObjectsEntityOpenFailedTest);
  FRIEND_TEST(S3ClovisWriterTest, OpenObjectsFailedTest);
  FRIEND_TEST(S3ClovisWriterTest, OpenObjectsFailedMissingTest);
  FRIEND_TEST(S3ClovisWriterTest, WriteContentSuccessfulTest);
  FRIEND_TEST(S3ClovisWriterTest, WriteContentFailedTest);
};

#endif
