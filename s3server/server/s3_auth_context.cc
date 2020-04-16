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
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#include <event2/thread.h>

#include "s3_auth_context.h"
#include "s3_log.h"
#include "s3_option.h"

extern evhtp_ssl_ctx_t *g_ssl_auth_ctx;

struct s3_auth_op_context *create_basic_auth_op_ctx(
    struct event_base *eventbase, S3AuthClientOpType type) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  S3Option *option_instance = S3Option::get_instance();
  struct s3_auth_op_context *ctx =
      (struct s3_auth_op_context *)calloc(1, sizeof(struct s3_auth_op_context));
  ctx->evbase = eventbase;
  if (option_instance->is_s3_ssl_auth_enabled()) {
    ctx->conn = evhtp_connection_ssl_new(
        ctx->evbase, option_instance->get_auth_ip_addr().c_str(),
        option_instance->get_auth_port(), g_ssl_auth_ctx);
  } else {
    ctx->conn = evhtp_connection_new(
        ctx->evbase, option_instance->get_auth_ip_addr().c_str(),
        option_instance->get_auth_port());
  }

  if (type == S3AuthClientOpType::authentication) {
    ctx->authrequest = evhtp_request_new(NULL, ctx->evbase);
  } else if (type == S3AuthClientOpType::authorization) {
    ctx->authorization_request = evhtp_request_new(NULL, ctx->evbase);
  } else if (type == S3AuthClientOpType::aclvalidation) {
    ctx->aclvalidation_request = evhtp_request_new(NULL, ctx->evbase);
  } else if (type == S3AuthClientOpType::policyvalidation) {
    ctx->policyvalidation_request = evhtp_request_new(NULL, ctx->evbase);
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return ctx;
}

int free_basic_auth_client_op_ctx(struct s3_auth_op_context *ctx) {
  s3_log(S3_LOG_DEBUG, "", "Called\n");
  free(ctx);
  ctx = NULL;
  return 0;
}
