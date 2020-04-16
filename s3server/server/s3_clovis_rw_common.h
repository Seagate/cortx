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

#ifndef __S3_SERVER_S3_CLOVIS_RW_COMMON_H__
#define __S3_SERVER_S3_CLOVIS_RW_COMMON_H__

#include "s3_common.h"

EXTERN_C_BLOCK_BEGIN
#include "mero/init.h"
#include "module/instance.h"

#include "clovis/clovis.h"

/* libevhtp */
#include <evhtp.h>

void clovis_op_done_on_main_thread(evutil_socket_t, short events,
                                   void *user_data);

void s3_clovis_op_stable(struct m0_clovis_op *op);

void s3_clovis_op_failed(struct m0_clovis_op *op);
// funtion is to handle clovis pre launch opeariton failures in async way
void s3_clovis_op_pre_launch_failure(void *application_context, int rc);
void s3_clovis_dummy_op_stable(evutil_socket_t, short events, void *user_data);

void s3_clovis_dummy_op_failed(evutil_socket_t, short events, void *user_data);

EXTERN_C_BLOCK_END

// Clovis operation context from application perspective.
// When multiple ops are launched in single call,
// op_index_in_launch indicates index in ops array to
// identify specific operation.
struct s3_clovis_context_obj {
  int op_index_in_launch;
  void *application_context;
  int is_fake_failure;  // 0 = false, 1 = true
};

#endif
