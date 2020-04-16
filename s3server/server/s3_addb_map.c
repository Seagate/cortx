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

#include "s3_addb_map.h"

#include <assert.h>
#include <string.h>

const char* action_state_map[ADDB_TASK_LIST_OFFSET] = {
    "START", "RUNNING", "COMPLETE", "PAUSED", "STOPPED", "ERROR"};

const char* addb_idx_to_s3_state(uint64_t addb_idx) {
  if (addb_idx < ADDB_TASK_LIST_OFFSET) {
    assert(action_state_map[addb_idx]);
    return action_state_map[addb_idx];
  }

  uint64_t map_idx = addb_idx - ADDB_TASK_LIST_OFFSET;

  assert(map_idx < g_s3_to_addb_idx_func_name_map_size);
  assert(g_s3_to_addb_idx_func_name_map[map_idx]);

  return g_s3_to_addb_idx_func_name_map[map_idx];
}
