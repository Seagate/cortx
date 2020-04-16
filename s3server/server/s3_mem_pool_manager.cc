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
 * Original creation date: 01-Jun-2017
 */

#include "s3_mem_pool_manager.h"

S3MempoolManager *S3MempoolManager::instance = NULL;

size_t S3MempoolManager::free_space = 0;

extern "C" size_t mem_get_free_space_func() {
  return S3MempoolManager::free_space;
}

extern "C" void mem_mark_used_space_func(size_t space_used) {
  S3MempoolManager::free_space -= space_used;
}

extern "C" void mem_mark_free_space_func(size_t space_freed) {
  S3MempoolManager::free_space += space_freed;
}

S3MempoolManager::S3MempoolManager(size_t max_mem)
    : total_memory_threshold(max_mem) {}

int S3MempoolManager::initialize(std::vector<int> unit_sizes,
                                 int initial_buffer_count_per_pool,
                                 size_t expandable_count) {
  s3_log(S3_LOG_DEBUG, "",
         "initial_buffer_count_per_pool = %d, expandable_count = %zu",
         initial_buffer_count_per_pool, expandable_count);
  // S3MempoolManager::free_space must be init before pool creation as pool
  // creation will allocate memory and reduce S3MempoolManager::free_space
  S3MempoolManager::free_space = total_memory_threshold;

  for (auto unit_size : unit_sizes) {
    MemoryPoolHandle handle;
    s3_log(S3_LOG_INFO, "", "Creating memory pool for unit_size = %d.\n",
           unit_size);
    int rc = mempool_create_with_shared_mem(
        unit_size, initial_buffer_count_per_pool * unit_size,
        expandable_count * unit_size, mem_get_free_space_func,
        mem_mark_used_space_func, mem_mark_free_space_func,
        CREATE_ALIGNED_MEMORY, &handle);

    if (rc != 0) {
      s3_log(S3_LOG_ERROR, "", "FATAL: Memory pool creation failed!\n");
      if (rc == S3_MEMPOOL_THRESHOLD_EXCEEDED) {
        s3_log(S3_LOG_ERROR, "",
               "Pool allocation crossing current threshold value %zu, increase "
               "threshold value (S3_CLOVIS_READ_POOL_MAX_THRESHOLD) or "
               "decrease pool allocation size "
               "(S3_CLOVIS_READ_POOL_INITIAL_BUFFER_COUNT) \n",
               mem_get_free_space_func());
      }
      return rc;
    }
    pool_of_mem_pool[unit_size] = handle;

    struct pool_info poolinfo = {0};
    mempool_getinfo(handle, &poolinfo);
    s3_log(S3_LOG_DEBUG, "", "Created pool with [[ \n");
    s3_log(S3_LOG_DEBUG, "", "poolinfo.mempool_item_size = %zu\n",
           poolinfo.mempool_item_size);
    s3_log(S3_LOG_DEBUG, "", "poolinfo.free_bufs_in_pool = %d\n",
           poolinfo.free_bufs_in_pool);
    s3_log(S3_LOG_DEBUG, "", "poolinfo.number_of_bufs_shared = %d\n",
           poolinfo.number_of_bufs_shared);
    s3_log(S3_LOG_DEBUG, "", "poolinfo.expandable_size = %zu\n",
           poolinfo.expandable_size);
    s3_log(S3_LOG_DEBUG, "", "poolinfo.total_bufs_allocated_by_pool = %d\n",
           poolinfo.total_bufs_allocated_by_pool);
    s3_log(S3_LOG_DEBUG, "", "poolinfo.flags = %d ]]\n", poolinfo.flags);
  }
  return 0;
}

void S3MempoolManager::free_pools() {
  for (auto &mem_pool : pool_of_mem_pool) {
    s3_log(S3_LOG_DEBUG, "", "Freeing memory pool for unit_size = %zu.\n",
           mem_pool.first);
    mempool_destroy(&(mem_pool.second));
    s3_log(S3_LOG_DEBUG, "",
           "Free memory pool successful for unit_size = %zu.\n",
           mem_pool.first);
  }
  pool_of_mem_pool.clear();
}

//  Return the buffer of give unit_size
void *S3MempoolManager::get_buffer_for_unit_size(size_t unit_size) {
  s3_log(S3_LOG_DEBUG, "", "Entering with unit_size[%zu]\n", unit_size);
  auto item = pool_of_mem_pool.find(unit_size);
  if (item != pool_of_mem_pool.end()) {
    // We have required memory pool.
    MemoryPoolHandle handle = item->second;
    int max_retries = 3;
    while (max_retries > 0) {
      void *buffer = (void *)mempool_getbuffer(handle, ZEROED_ALLOCATION);
      // If buffer is NULL, check if other pool can release some memory
      if (buffer == NULL) {
        s3_log(S3_LOG_DEBUG, "", "Allocation error for unit_size[%zu]\n",
               unit_size);
        if (free_any_unused()) {
          max_retries--;
          continue;
        } else {
          // No free memory, just return out-of-mem
          break;
        }
      }
      return buffer;
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Failed allocation for unit_size[%zu]\n", unit_size);
  // Should never be here
  return NULL;
}

// Releases the give buffer back to pool
int S3MempoolManager::release_buffer_for_unit_size(void *buf,
                                                   size_t unit_size) {
  s3_log(S3_LOG_DEBUG, "", "Entering with unit_size[%zu]\n", unit_size);
  auto item = pool_of_mem_pool.find(unit_size);
  if (item != pool_of_mem_pool.end()) {
    s3_log(S3_LOG_DEBUG, "",
           "Found pool for unit_size[%zu] to release memory\n", unit_size);
    // We have required memory pool.
    MemoryPoolHandle handle = item->second;
    return mempool_releasebuffer(handle, buf);
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting: Not found unit_size[%zu]\n", unit_size);
  // Should never be here
  return S3_MEMPOOL_ERROR;
}

size_t S3MempoolManager::get_free_space_for(size_t unit_size) {
  auto item = pool_of_mem_pool.find(unit_size);
  if (item != pool_of_mem_pool.end()) {
    // We have required memory pool.
    MemoryPoolHandle handle = item->second;
    size_t free_bytes = 0;
    mempool_free_space(handle, &free_bytes);
    return free_bytes + S3MempoolManager::free_space;
  }
  return 0;
}

bool S3MempoolManager::free_any_unused() {
  // <non_zero free_space, handle>
  std::map<size_t, MemoryPoolHandle> free_space_map;

  // Find free space in all mem pools
  for (auto &mem_pool : pool_of_mem_pool) {
    size_t free_bytes = 0;
    mempool_free_space(mem_pool.second, &free_bytes);
    if (free_bytes > 0) {
      free_space_map[free_bytes] = mem_pool.second;
    }
  }

  // Request pool with largest free size to free up half the space
  if (free_space_map.empty()) {
    return false;
  } else {
    struct pool_info poolinfo = {0};
    mempool_getinfo(free_space_map.rbegin()->second, &poolinfo);
    s3_log(S3_LOG_DEBUG, "", "Downsizing mempool with unit_size = %zu\n",
           poolinfo.mempool_item_size);

    size_t size_to_reduce = free_space_map.rbegin()->first / 2;
    if (size_to_reduce < poolinfo.mempool_item_size) {
      size_to_reduce = poolinfo.mempool_item_size;
    }

    int rc = mempool_downsize(free_space_map.rbegin()->second, size_to_reduce);
    if (rc == 0) {
      return true;
    } else {
      return false;
    }
  }
}
