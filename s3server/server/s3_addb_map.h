/*
 * COPYRIGHT 2020 SEAGATE LLC
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
 * Original author:  Dmitrii Surnin   <dmitrii.surnin@seagate.com>
 * Original creation date: 23-Jan-2020
 */

#pragma once

#ifndef __S3_SERVER_ADDB_MAP_H__
#define __S3_SERVER_ADDB_MAP_H__

#include <addb2/addb2_internal.h>

typedef enum {
  ACTS_START = 0,
  ACTS_RUNNING,
  ACTS_COMPLETE,
  ACTS_PAUSED,
  ACTS_STOPPED,  // Aborted
  ACTS_ERROR,

  // Used to distinguish ActionState values from task_list indexes.
  ADDB_TASK_LIST_OFFSET
} ActionState;

extern const char* g_s3_to_addb_idx_func_name_map[];
extern const uint64_t g_s3_to_addb_idx_func_name_map_size;

const char* addb_idx_to_s3_state(uint64_t addb_idx);

#endif
