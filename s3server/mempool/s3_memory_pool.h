/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original creation date: 5-Dec-2016
 */

#pragma once

#ifndef __MERO_FE_S3_SERVER_S3_MEMORY_POOL_H__
#define __MERO_FE_S3_SERVER_S3_MEMORY_POOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CREATE_ALIGNED_MEMORY 0x0001
#define ENABLE_LOCKING 0x0002
#define ZEROED_ALLOCATION 0x0004
#define PREALLOCATE_MEM_ON_CREATE 0x0008

#define MEMORY_ALIGNMENT 4096
#define S3_MEMPOOL_ERROR -1
#define S3_MEMPOOL_INVALID_ARG -2
#define S3_MEMPOOL_THRESHOLD_EXCEEDED -3

typedef void *MemoryPoolHandle;

// This call back is supposed to return the total space used.
// Useful when outside memory should be considered for max threshold cap.
// Outside memory could be case when multiple memory pools are used.
typedef size_t (*func_mem_available_callback_type)(void);

typedef void (*func_mark_mem_used_callback_type)(size_t);
typedef void (*func_mark_mem_free_callback_type)(size_t);

struct memory_pool_element {
  struct memory_pool_element *next;
};

struct mempool {
  int flags; /* CREATE_ALIGNED_MEMORY, ENABLE_LOCKING, ZEROED_ALLOCATION */
  int free_bufs_in_pool;     /* Number of items on free list */
  int number_of_bufs_shared; /* Number of bufs shared from pool to pool user */
  int total_bufs_allocated_by_pool; /* Total buffers currently allocated by
                                         pool via native method */
  func_mem_available_callback_type
      mem_get_free_space_func; /* If this pool shares the max_memory_threshold
                                  with say other pool, this callback should
                                  return the available space which can be
                                  allocated */
  func_mark_mem_used_callback_type
      mem_mark_used_space_func; /* This is used to indicate to user of pool that
                                   new memory is allocated
                                   (posix_memalign/malloc) within pool, so user
                                   of pool can track it w.r.t max threshold. */
  func_mark_mem_free_callback_type
      mem_mark_free_space_func; /* Whenever pool frees any memory, use this to
                                   indicate to user of pool that memory was
                                   free'd with actual free() sys call. */
  int alignment;            /* Memory aligment */
  size_t max_memory_threshold; /* Maximum memory that the system can have from
                                  pool */
  size_t mempool_item_size;    /* Size of items managed by this pool */
  size_t expandable_size;      /* pool expansion rate when free list is empty */
  pthread_mutex_t lock;        /* lock, in case of synchronous operation */
  struct memory_pool_element
      *free_list; /* list of free items available for reuse */
};

struct pool_info {
  int flags;
  int free_bufs_in_pool;
  int number_of_bufs_shared;
  int total_bufs_allocated_by_pool;
  size_t mempool_item_size;
  size_t expandable_size;
};

/**
 * Flow:
 * 1) Create a memory pool and obtain the handle to it
 * 2) Provide handle to the memory pool APIs to manage pool
 * 3) Destroy the pool when its not needed
 *
 * Example 1: Simple Memory pool
 * 1) A simple memory pool is created using mempool_create api, user supplies
 *     initial pool size as 0, The pool initially doesn't have any items in it.
 * 2) When user asks for item using mempool_getbuffer, it allocates it by using
 *    native method(malloc/posix_memalign) as the free list is empty.
 * 3) When user is done with item, it releases the item using pool api
 *    (mempool_releasebuffer), mempool api will hook the item to the
 *    free list of the pool.
 * 4) Next time when user asks for the item, it will be given the item (which
 *    it released before) from the free list of the pool
 * 5) Consecutive release buffer operations via api mempool_releasebuffer api
 *    will preappend to list or free the item depending on the maximum allowed
 *    free list (configurable)
 *
 *
 *     mempool_create() --> mempool_getbuffer() --> mempool_releasebuffer() -->
 * mempool_destroy()
 *
 * Example 2: Memory Pool with preallocated items, dynamic expansion and with
 * cap on maximum item(memory) utilization
 * 1) Create the pool with api mempool_create, the pool will have some initial
 *    number of items as specified by user
 * 2) When user asks for item it will be allocated from the pool (free list
 *    will not be empty), whenever the freelist is empty then the pool will
 *    dynamically expand the free list size by expansion size
 *    (specified when creating pool), error will be returned in case if the
 *    memory consumed via pool crosses the threshold value(set when creating
 * pool).
 * 3) Whenever user releases the item via mempool_releasebuffer, it will hook
 *    the item to the list or free it depending on the maximum free list size
 *
 *     mempool_create() --> mempool_getbuffer() --> mempool_releasebuffer() -->
 *     mempool_destroy()
 */

/**
 * Create a memory pool object and do preallocation of free list in it, return
 * handle to it.
 * args:
 * pool_item_size (in) Size of each buffer items that this pool will manage,
 * minimum size is size of pointer
 * Note: In case of clovis the minimum buffer size is 4KB
 * pool_initial_size (in) Initial size of the pool
 * pool_expansion_size (in) pool expansion size when all free memory
 * being used.
 * pool_max_threshold_size (in) maximum allowed memory utilization(Consumed by
 * app. + free list in pool)
 * when done via the pool
 * flags (in) if ENABLE_LOCKING then pool synchronization with lock
 * p_handle (out) On success pool handle is returned here
 * returns:
 * 0 on success, otherwise an error
 */
int mempool_create(size_t pool_item_size, size_t pool_initial_size,
                   size_t pool_expansion_size, size_t pool_max_threshold_size,
                   int flags, MemoryPoolHandle *p_handle);

int mempool_create_with_shared_mem(
    size_t pool_item_size, size_t pool_initial_size, size_t pool_expansion_size,
    func_mem_available_callback_type mem_get_free_space_func,
    func_mark_mem_used_callback_type mem_mark_used_space_func,
    func_mark_mem_free_callback_type mem_mark_free_space_func, int flags,
    MemoryPoolHandle *p_handle);

/**
 * Allocate some buffer memory via memory pool
 * args:
 * flags (in) if ZEROED_ALLOCATION set then zero memory
 * handle (in) handle to memory pool obtained from mempool_create
 * returns:
 * 0 on success, otherwise an error
 */
void *mempool_getbuffer(MemoryPoolHandle handle, int flags);

/**
 * Hook/Free the item that was allocated earlier from the memory pool
 * (memory alloc) to pool's free list, based on the pool's free list capacity
 * args:
 * handle (in) Pool handle as returned by mempool_create
 * buf (in) item allocated earlier via mempool_getbuffer()
 * returns:
 * 0 on success, otherwise an error
 */
int mempool_releasebuffer(MemoryPoolHandle handle, void *buf);

/**
 * Returns info about the memory pool
 * args:
 * handle (in) Pool handle as returned by mempool_create
 * poolinfo (out) information about the pool
 * returns:
 * 0 on success, otherwise an error
 */
int mempool_getinfo(MemoryPoolHandle handle, struct pool_info *poolinfo);

/**
 * Returns total free space available in memory pool
 * args:
 * handle (in) Pool handle as returned by mempool_create
 * free_bytes (out) Total free bytes available in pool
 * returns:
 * 0 on success, otherwise an error
 */
int mempool_free_space(MemoryPoolHandle handle, size_t *free_bytes);

/**
 * Returns memory pool's buffer size
 * args:
 * handle (in) Pool handle as returned by mempool_create
 * buffer_size (out) memory pool's buffer size
 * returns:
 * 0 on success, otherwise an error
 */
int mempool_getbuffer_size(MemoryPoolHandle handle, size_t *buffer_size);

/**
 * Free up 'mem_to_free' space from this pool, so other pools can use.
 * args:
 * handle (in) Pool handle as returned by mempool_create
 * mem_to_free (in) Bytes to free, MUST be multiple of pool_item_size
 * returns:
 * 0 on success, otherwise an error
 */
int mempool_downsize(MemoryPoolHandle handle, size_t mem_to_free);

/**
 * Change the the pool's maximum threshold memory(Application consumed memory +
 * pools free memory), setting up maximum threshold will fail if it makes the
 * pool's maximum free list size 0.
 * In case if user wants to downsize the current maximum threshold value then it
 * may need to get the pool information and then set the new maximum threshold
 * value based on pool info, this is to ensure that max free list size of the
 * pool doesn't become 0 with this api.
 * args:
 * handle (in) Pool handle as returned by mempool_create
 * new_max_threshold (in) New max free size setting
 * returns:
 * 0 on success, otherwise an error
 */
int mempool_resize(MemoryPoolHandle handle, int new_max_threshold);

/**
 * Destroy mem pool object along with all memories in its free list
 * Call this api when the application is shutting down, Api will assert in case
 * if there is memory allocation done via pool api and it being freed without
 * pool's free api
 * args:
 * p_handle (in/out) pointer to handle, on return this will be set to NULL
 * returns:
 * 0 on success, otherwise an error
 */
int mempool_destroy(MemoryPoolHandle *p_handle);

#ifdef __cplusplus
}
#endif

#endif
