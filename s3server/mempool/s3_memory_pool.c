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

#include "s3_memory_pool.h"

/**
 * Return the number of buffers we can allocate w.r.t max threshold and
 * available memory space.
 */
static int pool_can_expand_by(struct mempool *pool) {
  int available_space = 0;

  if (pool == NULL) {
    return 0;
  }

  if (pool->mem_get_free_space_func) {
    available_space = pool->mem_get_free_space_func();
  } else {
    if (pool->max_memory_threshold > 0) {
      available_space =
          pool->max_memory_threshold -
          (pool->total_bufs_allocated_by_pool * pool->mempool_item_size);
    }
  }

  // We can expand by at least (available_space / pool->mempool_item_size)
  // buffer count
  return available_space / pool->mempool_item_size;
}

/**
 * Internal function to preallocate items to memory pool.
 * args:
 * pool (in) pointer to memory pool
 * items_count_to_allocate (in) Extra items to be allocated
 * returns:
 * 0 on success, otherwise an error
 */
int freelist_allocate(struct mempool *pool, int items_count_to_allocate) {
  int i;
  int rc = 0;
  void *buf = NULL;
  struct memory_pool_element *pool_item = NULL;

  if (pool == NULL) {
    return S3_MEMPOOL_INVALID_ARG;
  }

  for (i = 0; i < items_count_to_allocate; i++) {
    if (pool->flags & CREATE_ALIGNED_MEMORY) {
      rc = posix_memalign(&buf, pool->alignment, pool->mempool_item_size);
    } else {
      buf = malloc(pool->mempool_item_size);
    }
    if (buf == NULL || rc != 0) {
      return S3_MEMPOOL_ERROR;
    }
    pool->total_bufs_allocated_by_pool++;
    /* Put the allocated memory into the list */
    pool_item = (struct memory_pool_element *)buf;

    pool_item->next = pool->free_list;
    pool->free_list = pool_item;
    /* memory is pre appended to list */

    /* Increase the free list count */
    pool->free_bufs_in_pool++;
    if (pool->mem_mark_used_space_func) {
      pool->mem_mark_used_space_func(pool->mempool_item_size);
    }
  }
  return 0;
}

int mempool_create(size_t pool_item_size, size_t pool_initial_size,
                   size_t pool_expansion_size, size_t pool_max_threshold_size,
                   int flags, MemoryPoolHandle *handle) {
  int rc;
  int bufs_to_allocate;
  struct mempool *pool = NULL;

  /* pool_max_threshold_size == 0 is possible when
     func_mem_available_callback_type is used. */
  if (pool_item_size == 0 || pool_expansion_size == 0 || handle == NULL) {
    return S3_MEMPOOL_INVALID_ARG;
  }

  /* Minimum size of the pool's buffer will be sizeof pointer */
  if (pool_item_size < sizeof(struct memory_pool_element)) {
    pool_item_size = sizeof(struct memory_pool_element);
  }

  *handle = NULL;

  pool = (struct mempool *)calloc(1, sizeof(struct mempool));
  if (pool == NULL) {
    return S3_MEMPOOL_ERROR;
  }

  /* flag that can be used to figure out whether we are doing preallocation of
   * items when creating pool */
  if (pool_initial_size != 0) {
    pool->flags |= PREALLOCATE_MEM_ON_CREATE;
  }

  pool->flags |= flags;
  pool->mempool_item_size = pool_item_size;
  if (flags & CREATE_ALIGNED_MEMORY) {
    pool->alignment = MEMORY_ALIGNMENT;
  }

  if ((pool->flags & ENABLE_LOCKING) != 0) {
    rc = pthread_mutex_init(&pool->lock, NULL);
    if (rc != 0) {
      free(pool);
      return S3_MEMPOOL_ERROR;
    }
  }

  *handle = (MemoryPoolHandle)pool;

  pool->expandable_size = pool_expansion_size;
  pool->max_memory_threshold = pool_max_threshold_size;
  /* Figure out the size of free list to be preallocated from given initial pool
   * size */
  bufs_to_allocate = pool_initial_size / pool_item_size;

  /* Allocate the free list */
  if (bufs_to_allocate > 0) {
    rc = freelist_allocate(pool, bufs_to_allocate);
    if (rc != 0) {
      goto fail;
    }
  }
  return 0;

fail:
  mempool_destroy(handle);
  *handle = NULL;
  return S3_MEMPOOL_ERROR;
}

int mempool_create_with_shared_mem(
    size_t pool_item_size, size_t pool_initial_size, size_t pool_expansion_size,
    func_mem_available_callback_type mem_get_free_space_func,
    func_mark_mem_used_callback_type mem_mark_used_space_func,
    func_mark_mem_free_callback_type mem_mark_free_space_func, int flags,
    MemoryPoolHandle *p_handle) {
  int rc = 0;
  struct mempool *pool = NULL;
  if (mem_get_free_space_func == NULL || mem_mark_used_space_func == NULL ||
      mem_mark_free_space_func == NULL || p_handle == NULL) {
    return S3_MEMPOOL_INVALID_ARG;
  }

  if (pool_initial_size > mem_get_free_space_func()) {
    return S3_MEMPOOL_THRESHOLD_EXCEEDED;
  }

  rc = mempool_create(pool_item_size, pool_initial_size, pool_expansion_size, 0,
                      flags, p_handle);
  if (rc != 0) {
    return rc;
  }

  pool = (struct mempool *)*p_handle;

  pool->mem_get_free_space_func = mem_get_free_space_func;
  pool->mem_mark_used_space_func = mem_mark_used_space_func;
  pool->mem_mark_free_space_func = mem_mark_free_space_func;

  /* Explicitly mark used space, since mempool_create -> freelist_allocate
     dont have the function callbacks set. */
  pool->mem_mark_used_space_func(pool->total_bufs_allocated_by_pool *
                                 pool->mempool_item_size);

  return 0;
}

void *mempool_getbuffer(MemoryPoolHandle handle, int flags) {
  int rc;
  int bufs_to_allocate;
  int bufs_that_can_be_allocated = 0;
  struct memory_pool_element *pool_item = NULL;
  struct mempool *pool = (struct mempool *)handle;

  if (pool == NULL) {
    return NULL;
  }

  if ((pool->flags & ENABLE_LOCKING) != 0) {
    pthread_mutex_lock(&pool->lock);
  }

  /* If the free list is empty then expand the pool's free list */
  if (pool->free_bufs_in_pool == 0) {
    bufs_to_allocate = pool->expandable_size / pool->mempool_item_size;
    bufs_that_can_be_allocated = pool_can_expand_by(pool);
    if (bufs_that_can_be_allocated > 0) {
      /* We can at least allocate
         min(bufs_that_can_be_allocated, bufs_to_allocate) */
      bufs_to_allocate = ((bufs_to_allocate > bufs_that_can_be_allocated)
                              ? bufs_that_can_be_allocated
                              : bufs_to_allocate);

      rc = freelist_allocate(pool, bufs_to_allocate);
      if (rc != 0) {
        if ((pool->flags & ENABLE_LOCKING) != 0) {
          pthread_mutex_unlock(&pool->lock);
        }
        return NULL;
      }
    } else {
      /* We cannot allocate any more buffers, reached max threshold */
      if ((pool->flags & ENABLE_LOCKING) != 0) {
        pthread_mutex_unlock(&pool->lock);
      }
      return NULL;
    }
  }

  /* Done with expansion of pool in case of pre allocated pools */

  /* Logic of allocation from free list */
  /* If there is an item on the pool's free list, then take that... */
  if (pool->free_list != NULL) {
    pool_item = pool->free_list;
    pool->free_list = pool_item->next;
    pool->free_bufs_in_pool--;

    if ((flags & ZEROED_ALLOCATION) != 0) {
      memset(pool_item, 0, pool->mempool_item_size);
    }
  }

  if (pool_item) {
    pool->number_of_bufs_shared++;
  }

  if ((pool->flags & ENABLE_LOCKING) != 0) {
    pthread_mutex_unlock(&pool->lock);
  }

  return (void *)pool_item;
}

int mempool_releasebuffer(MemoryPoolHandle handle, void *buf) {
  struct mempool *pool = (struct mempool *)handle;
  struct memory_pool_element *pool_item = (struct memory_pool_element *)buf;

  if ((pool == NULL) || (pool_item == NULL)) {
    return S3_MEMPOOL_INVALID_ARG;
  }

  if ((pool->flags & ENABLE_LOCKING) != 0) {
    pthread_mutex_lock(&pool->lock);
  }

  // Add the buffer back to pool
  pool_item->next = pool->free_list;
  pool->free_list = pool_item;
  pool->free_bufs_in_pool++;
  pool_item = NULL;

  pool->number_of_bufs_shared--;

  if ((pool->flags & ENABLE_LOCKING) != 0) {
    pthread_mutex_unlock(&pool->lock);
  }

  return 0;
}

int mempool_getinfo(MemoryPoolHandle handle, struct pool_info *poolinfo) {
  struct mempool *pool = (struct mempool *)handle;

  if ((pool == NULL) || (poolinfo == NULL)) {
    return S3_MEMPOOL_INVALID_ARG;
  }

  if (pool->flags & ENABLE_LOCKING) {
    pthread_mutex_lock(&pool->lock);
  }

  poolinfo->mempool_item_size = pool->mempool_item_size;
  poolinfo->free_bufs_in_pool = pool->free_bufs_in_pool;
  poolinfo->number_of_bufs_shared = pool->number_of_bufs_shared;
  poolinfo->expandable_size = pool->expandable_size;
  poolinfo->total_bufs_allocated_by_pool = pool->total_bufs_allocated_by_pool;
  poolinfo->flags = pool->flags;

  if ((pool->flags & ENABLE_LOCKING) != 0) {
    pthread_mutex_unlock(&pool->lock);
  }
  return 0;
}

int mempool_free_space(MemoryPoolHandle handle, size_t *free_bytes) {
  struct mempool *pool = (struct mempool *)handle;

  if ((pool == NULL) || (free_bytes == NULL)) {
    return S3_MEMPOOL_INVALID_ARG;
  }

  if (pool->flags & ENABLE_LOCKING) {
    pthread_mutex_lock(&pool->lock);
  }

  *free_bytes = pool->mempool_item_size * pool->free_bufs_in_pool;

  if ((pool->flags & ENABLE_LOCKING) != 0) {
    pthread_mutex_unlock(&pool->lock);
  }

  return 0;
}

int mempool_getbuffer_size(MemoryPoolHandle handle, size_t *buffer_size) {
  struct mempool *pool = (struct mempool *)handle;

  if ((pool == NULL) || (buffer_size == NULL)) {
    return S3_MEMPOOL_INVALID_ARG;
  }
  *buffer_size = pool->mempool_item_size;
  return 0;
}

int mempool_downsize(MemoryPoolHandle handle, size_t mem_to_free) {
  struct mempool *pool = NULL;
  struct memory_pool_element *pool_item = NULL;
  int bufs_to_free = 0;
  int count = 0;

  if (handle == NULL) {
    return S3_MEMPOOL_INVALID_ARG;
  }

  pool = (struct mempool *)handle;

  /* pool is NULL or mem_to_free is not multiple of pool->mempool_item_size */
  if (pool == NULL || (mem_to_free % pool->mempool_item_size > 0)) {
    return S3_MEMPOOL_INVALID_ARG;
  }

  if ((pool->flags & ENABLE_LOCKING) != 0) {
    pthread_mutex_lock(&pool->lock);
  }

  /* Only free what we can free */
  bufs_to_free = mem_to_free / pool->mempool_item_size;
  if (bufs_to_free > pool->free_bufs_in_pool) {
    bufs_to_free = pool->free_bufs_in_pool;
  }

  /* Free the items in free list */
  if (bufs_to_free > 0) {
    pool_item = pool->free_list;
    count = 0;
    while (count < bufs_to_free && pool_item != NULL) {
      count++;
      pool->free_list = pool_item->next;
      free(pool_item);
      pool->total_bufs_allocated_by_pool--;
      pool->free_bufs_in_pool--;
      pool_item = pool->free_list;
    }
    if (pool->mem_mark_free_space_func) {
      pool->mem_mark_free_space_func(bufs_to_free * pool->mempool_item_size);
    }
  }

  if ((pool->flags & ENABLE_LOCKING) != 0) {
    pthread_mutex_unlock(&pool->lock);
  }
  return 0;
}

int mempool_destroy(MemoryPoolHandle *handle) {
  struct mempool *pool = NULL;
  struct memory_pool_element *pool_item;

  if (handle == NULL) {
    return S3_MEMPOOL_INVALID_ARG;
  }

  pool = (struct mempool *)*handle;
  if (pool == NULL) {
    return S3_MEMPOOL_INVALID_ARG;
  }

  if ((pool->flags & ENABLE_LOCKING) != 0) {
    pthread_mutex_lock(&pool->lock);
  }

  if (*handle == NULL) {
    return S3_MEMPOOL_INVALID_ARG;
  }

  /* reset the handle */
  *handle = NULL;
  /* Free the items in free list */
  pool_item = pool->free_list;
  while (pool_item != NULL) {
    pool->free_list = pool_item->next;
    free(pool_item);
#if 0
    /* Need this if below asserts are there */
    pool->total_bufs_allocated_by_pool--;
    pool->free_bufs_in_pool--;
#endif
    pool_item = pool->free_list;
  }
  pool->free_list = NULL;

  /* TODO: libevhtp/libevent seems to hold some references and not release back
   * to pool. Bug will be logged for this to investigate.
   */
  /* Assert if there are leaks */
  /*
    assert(pool->total_bufs_allocated_by_pool == 0);
    assert(pool->number_of_bufs_shared == 0);
    assert(pool->free_bufs_in_pool == 0);
  */

  if ((pool->flags & ENABLE_LOCKING) != 0) {
    pthread_mutex_unlock(&pool->lock);
    pthread_mutex_destroy(&pool->lock);
  }

  free(pool);
  pool = NULL;
  return 0;
}
