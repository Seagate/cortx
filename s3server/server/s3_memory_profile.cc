/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original creation date: 6-Feb-2017
 */

#include <event2/event.h>

#include "s3_clovis_layout.h"
#include "s3_log.h"
#include "s3_memory_pool.h"
#include "s3_memory_profile.h"
#include "s3_option.h"

extern S3Option* g_option_instance;

size_t S3MemoryProfile::memory_per_put_request(int layout_id) {
  return g_option_instance->get_clovis_write_payload_size(layout_id) *
         g_option_instance->get_read_ahead_multiple();
}

bool S3MemoryProfile::we_have_enough_memory_for_put_obj(int layout_id) {
#ifdef S3_GOOGLE_TEST
  return true;
#endif
  size_t free_space_in_libevent_mempool = 0;
  event_mempool_free_space(&free_space_in_libevent_mempool);
  s3_log(S3_LOG_DEBUG, "", "free_space_in_libevent_mempool = %zu\n",
         free_space_in_libevent_mempool);

  size_t min_mem_for_put_obj = memory_per_put_request(layout_id);

  s3_log(S3_LOG_DEBUG, "", "min_mem_for_put_obj = %zu\n", min_mem_for_put_obj);

  return (free_space_in_libevent_mempool > min_mem_for_put_obj);
}

bool S3MemoryProfile::free_memory_in_pool_above_threshold_limits() {
#ifdef S3_GOOGLE_TEST
  return true;
#endif
  size_t free_space_in_libevent_mempool = 0;
  // libevent uses ev_buffers for some internal tasks so
  // memory pool can be exhausted under high load
  const auto libevent_pool_reserve_size =
      g_option_instance->get_libevent_pool_reserve_size();
  event_mempool_free_space(&free_space_in_libevent_mempool);
  s3_log(S3_LOG_DEBUG, "", "free_space_in_libevent_mempool = %zu\n",
         free_space_in_libevent_mempool);
  if (free_space_in_libevent_mempool < libevent_pool_reserve_size) {
    s3_log(
        S3_LOG_WARN, "",
        "Free space in libevent mempool is less than "
        "S3_LIBEVENT_POOL_RESERVE_SIZE defined in config file (s3config.yaml)");
    struct pool_info poolinfo;
    int rc = event_mempool_getinfo(&poolinfo);
    if (rc != 0) {
      s3_log(S3_LOG_FATAL, "", "Issue with memory pool!\n");
    } else {
      s3_log(S3_LOG_WARN, "",
             "mempool info: mempool_item_size = %zu "
             "free_bufs_in_pool = %d "
             "number_of_bufs_shared = %d "
             "total_bufs_allocated_by_pool = %d\n",
             poolinfo.mempool_item_size, poolinfo.free_bufs_in_pool,
             poolinfo.number_of_bufs_shared,
             poolinfo.total_bufs_allocated_by_pool);
    }
    return false;
  }
  const auto libevent_pool_max_threshold =
      g_option_instance->get_libevent_pool_max_threshold();
  const auto libevent_pool_reserve_percent =
      g_option_instance->get_libevent_pool_reserve_percent();
  if (libevent_pool_reserve_percent / 100.0 * libevent_pool_max_threshold >
      free_space_in_libevent_mempool) {
    s3_log(S3_LOG_WARN, "",
           "Free space (in percent) in libevent mempool is less than "
           "S3_LIBEVENT_POOL_RESERVE_PERCENT defined in config file "
           "(s3config.yaml)");

    struct pool_info poolinfo;
    int rc = event_mempool_getinfo(&poolinfo);
    if (rc != 0) {
      s3_log(S3_LOG_FATAL, "", "Issue with memory pool!\n");
    } else {
      s3_log(S3_LOG_WARN, "",
             "mempool info: mempool_item_size = %zu "
             "free_bufs_in_pool = %d "
             "number_of_bufs_shared = %d "
             "total_bufs_allocated_by_pool = %d\n",
             poolinfo.mempool_item_size, poolinfo.free_bufs_in_pool,
             poolinfo.number_of_bufs_shared,
             poolinfo.total_bufs_allocated_by_pool);
    }
    return false;
  }
  return true;
}

