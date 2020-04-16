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

#include "s3_clovis_rw_common.h"
#include "s3_asyncop_context_base.h"
#include "s3_iem.h"
#include "s3_log.h"
#include "s3_post_to_main_loop.h"
#include "s3_clovis_kvs_reader.h"
#include "s3_clovis_kvs_writer.h"
#include "s3_fake_clovis_kvs.h"

/*
 *  <IEM_INLINE_DOCUMENTATION>
 *    <event_code>047003001</event_code>
 *    <application>S3 Server</application>
 *    <submodule>Clovis</submodule>
 *    <description>Clovis connection failed</description>
 *    <audience>Service</audience>
 *    <details>
 *      Clovis operation failed due to connection failure.
 *      The data section of the event has following keys:
 *        time - timestamp.
 *        node - node name.
 *        pid  - process-id of s3server instance, useful to identify logfile.
 *        file - source code filename.
 *        line - line number within file where error occurred.
 *    </details>
 *    <service_actions>
 *      Save the S3 server log files. Check status of Mero service, restart it
 *      if needed. Restart S3 server if needed.
 *      If problem persists, contact development team for further investigation.
 *    </service_actions>
 *  </IEM_INLINE_DOCUMENTATION>
 */

// This is run on main thread.
void clovis_op_done_on_main_thread(evutil_socket_t, short events,
                                   void *user_data) {
  std::string request_id;
  if (user_data == NULL) {
    s3_log(S3_LOG_DEBUG, "", "Entering\n");
    s3_log(S3_LOG_ERROR, "", "Input argument user_data is NULL\n");
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  struct user_event_context *user_context =
      (struct user_event_context *)user_data;
  S3AsyncOpContextBase *context = (S3AsyncOpContextBase *)user_context->app_ctx;
  if (context == NULL) {
    s3_log(S3_LOG_ERROR, "", "context pointer is NULL\n");
  }
  if (context->get_request() != NULL) {
    request_id = context->get_request()->get_request_id();
  }
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  struct event *s3user_event = (struct event *)user_context->user_event;
  if (s3user_event == NULL) {
    s3_log(S3_LOG_ERROR, request_id, "User event is NULL\n");
  }
  context->log_timer();

  if (context->is_at_least_one_op_successful()) {
    context->on_success_handler()();  // Invoke the handler.
  } else {
    int error_code = context->get_errno_for(0);
    if ((error_code == -ETIMEDOUT) || (error_code == -ESHUTDOWN) ||
        (error_code == -ECONNREFUSED) || (error_code == -EHOSTUNREACH) ||
        (error_code == -ENOTCONN) || (error_code == -ECANCELED)) {
      s3_iem(LOG_ALERT, S3_IEM_CLOVIS_CONN_FAIL, S3_IEM_CLOVIS_CONN_FAIL_STR,
             S3_IEM_CLOVIS_CONN_FAIL_JSON);
    }
    context->on_failed_handler()();  // Invoke the handler.
  }
  free(user_data);
  // Free user event
  if (s3user_event) event_free(s3user_event);
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

// Clovis callbacks, run in clovis thread
void s3_clovis_op_stable(struct m0_clovis_op *op) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  struct s3_clovis_context_obj *ctx =
      (struct s3_clovis_context_obj *)op->op_datum;

  S3AsyncOpContextBase *app_ctx =
      (S3AsyncOpContextBase *)ctx->application_context;
  int clovis_rc = app_ctx->get_clovis_api()->clovis_op_rc(op);
  std::string request_id = app_ctx->get_request()->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  s3_log(S3_LOG_DEBUG, request_id, "Return code = %d op_code = %d\n", clovis_rc,
         op->op_code);

  s3_log(S3_LOG_DEBUG, request_id, "op_index_in_launch = %d\n",
         ctx->op_index_in_launch);

  app_ctx->set_op_errno_for(ctx->op_index_in_launch, clovis_rc);

  if (0 == clovis_rc) {
    app_ctx->set_op_status_for(ctx->op_index_in_launch,
                               S3AsyncOpStatus::success, "Success.");
  } else {
    app_ctx->set_op_status_for(ctx->op_index_in_launch, S3AsyncOpStatus::failed,
                               "Operation Failed.");
  }
  free(ctx);
  if (app_ctx->incr_response_count() == app_ctx->get_ops_count()) {
    struct user_event_context *user_ctx = (struct user_event_context *)calloc(
        1, sizeof(struct user_event_context));
    user_ctx->app_ctx = app_ctx;
    app_ctx->stop_timer();

#ifdef S3_GOOGLE_TEST
    evutil_socket_t test_sock = 0;
    short events = 0;
    clovis_op_done_on_main_thread(test_sock, events, (void *)user_ctx);
#else
    S3PostToMainLoop((void *)user_ctx)(clovis_op_done_on_main_thread,
                                       request_id);
#endif  // S3_GOOGLE_TEST
  }
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

void s3_clovis_op_failed(struct m0_clovis_op *op) {
  struct s3_clovis_context_obj *ctx =
      (struct s3_clovis_context_obj *)op->op_datum;

  S3AsyncOpContextBase *app_ctx =
      (S3AsyncOpContextBase *)ctx->application_context;
  std::string request_id = app_ctx->get_request()->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  int clovis_rc = app_ctx->get_clovis_api()->clovis_op_rc(op);
  s3_log(S3_LOG_ERROR, request_id, "Error code = %d\n", clovis_rc);

  s3_log(S3_LOG_DEBUG, request_id, "op_index_in_launch = %d\n",
         ctx->op_index_in_launch);

  app_ctx->set_op_errno_for(ctx->op_index_in_launch, clovis_rc);
  app_ctx->set_op_status_for(ctx->op_index_in_launch, S3AsyncOpStatus::failed,
                             "Operation Failed.");
  // If we faked failure reset clovis internal code.
  if (ctx->is_fake_failure) {
    op->op_rc = 0;
    ctx->is_fake_failure = 0;
  }
  free(ctx);
  if (app_ctx->incr_response_count() == app_ctx->get_ops_count()) {
    struct user_event_context *user_ctx = (struct user_event_context *)calloc(
        1, sizeof(struct user_event_context));
    user_ctx->app_ctx = app_ctx;
    app_ctx->stop_timer(false);
#ifdef S3_GOOGLE_TEST
    evutil_socket_t test_sock = 0;
    short events = 0;
    clovis_op_done_on_main_thread(test_sock, events, (void *)user_ctx);
#else
    S3PostToMainLoop((void *)user_ctx)(clovis_op_done_on_main_thread,
                                       request_id);
#endif  // S3_GOOGLE_TEST
  }
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

void s3_clovis_op_pre_launch_failure(void *application_context, int rc) {
  S3AsyncOpContextBase *app_ctx = (S3AsyncOpContextBase *)application_context;
  std::string request_id = app_ctx->get_request()->get_request_id();
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  s3_log(S3_LOG_DEBUG, request_id, "Error code = %d\n", rc);
  app_ctx->set_op_errno_for(0, rc);
  app_ctx->set_op_status_for(0, S3AsyncOpStatus::failed, "Operation Failed.");
  struct user_event_context *user_ctx =
      (struct user_event_context *)calloc(1, sizeof(struct user_event_context));
  user_ctx->app_ctx = app_ctx;
#ifdef S3_GOOGLE_TEST
  evutil_socket_t test_sock = 0;
  short events = 0;
  clovis_op_done_on_main_thread(test_sock, events, (void *)user_ctx);
#else
  S3PostToMainLoop((void *)user_ctx)(clovis_op_done_on_main_thread, request_id);
#endif  // S3_GOOGLE_TEST
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}

void s3_clovis_dummy_op_stable(evutil_socket_t, short events, void *user_data) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  struct user_event_context *user_context =
      (struct user_event_context *)user_data;
  struct m0_clovis_op *op = (struct m0_clovis_op *)user_context->app_ctx;
  // This can be mocked from GTest but system tests call this method too,
  // where m0_clovis_rc can't be mocked.
  op->op_rc = 0;  // fake success

  if (op->op_code == M0_CLOVIS_IC_GET) {
    struct s3_clovis_context_obj *ctx =
        (struct s3_clovis_context_obj *)op->op_datum;

    S3ClovisKVSReaderContext *read_ctx =
        (S3ClovisKVSReaderContext *)ctx->application_context;

    op->op_rc = S3FakeClovisKvs::instance()->kv_read(
        op->op_entity->en_id, *read_ctx->get_clovis_kvs_op_ctx());
  } else if (M0_CLOVIS_IC_NEXT == op->op_code) {
    struct s3_clovis_context_obj *ctx =
        (struct s3_clovis_context_obj *)op->op_datum;

    S3ClovisKVSReaderContext *read_ctx =
        (S3ClovisKVSReaderContext *)ctx->application_context;

    op->op_rc = S3FakeClovisKvs::instance()->kv_next(
        op->op_entity->en_id, *read_ctx->get_clovis_kvs_op_ctx());
  } else if (M0_CLOVIS_IC_PUT == op->op_code) {
    struct s3_clovis_context_obj *ctx =
        (struct s3_clovis_context_obj *)op->op_datum;

    S3AsyncClovisKVSWriterContext *write_ctx =
        (S3AsyncClovisKVSWriterContext *)ctx->application_context;

    op->op_rc = S3FakeClovisKvs::instance()->kv_write(
        op->op_entity->en_id, *write_ctx->get_clovis_kvs_op_ctx());
  } else if (M0_CLOVIS_IC_DEL == op->op_code) {
    struct s3_clovis_context_obj *ctx =
        (struct s3_clovis_context_obj *)op->op_datum;

    S3AsyncClovisKVSWriterContext *write_ctx =
        (S3AsyncClovisKVSWriterContext *)ctx->application_context;

    op->op_rc = S3FakeClovisKvs::instance()->kv_del(
        op->op_entity->en_id, *write_ctx->get_clovis_kvs_op_ctx());
  }

  // Free user event
  event_free((struct event *)user_context->user_event);
  s3_clovis_op_stable(op);
  free(user_data);
}

void s3_clovis_dummy_op_failed(evutil_socket_t, short events, void *user_data) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  struct user_event_context *user_context =
      (struct user_event_context *)user_data;
  struct m0_clovis_op *op = (struct m0_clovis_op *)user_context->app_ctx;
  struct s3_clovis_context_obj *ctx =
      (struct s3_clovis_context_obj *)op->op_datum;

  // This can be mocked from GTest but system tests call this method too
  // where m0_clovis_rc can't be mocked.
  op->op_rc = -ETIMEDOUT;  // fake network failure
  ctx->is_fake_failure = 1;
  // Free user event
  event_free((struct event *)user_context->user_event);
  s3_clovis_op_failed(op);
  free(user_data);
}
