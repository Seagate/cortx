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

#pragma once

#ifndef __S3_SERVER_S3_AUTH_CONTEXT_H__
#define __S3_SERVER_S3_AUTH_CONTEXT_H__

#include "s3_common.h"

EXTERN_C_BLOCK_BEGIN

#include <evhtp.h>

struct s3_auth_op_context {
  evbase_t* evbase;
  evhtp_connection_t* conn;
  evhtp_request_t* authrequest;               // for Authentication
  evhtp_request_t* authorization_request;     // For Authorization
  evhtp_request_t* aclvalidation_request;     // For AclValidation
  evhtp_request_t* policyvalidation_request;  // For PolicyValidation
  // evhtp_hook                auth_callback;
  // bool                      isfirstpass;
};

struct s3_auth_op_context* create_basic_auth_op_ctx(
    struct event_base* eventbase, S3AuthClientOpType type);

int free_basic_auth_client_op_ctx(struct s3_auth_op_context* ctx);

EXTERN_C_BLOCK_END

#endif
