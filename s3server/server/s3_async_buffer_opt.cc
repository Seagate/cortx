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
 * Original creation date: 9-Jan-2017
 */

#include <assert.h>

#include "s3_async_buffer_opt.h"

S3AsyncBufferOptContainer::S3AsyncBufferOptContainer(size_t size_of_each_buf)
    : size_of_each_evbuf(size_of_each_buf),
      content_length(0),
      is_expecting_more(true),
      count_bufs_shared_for_read(0) {
  s3_log(S3_LOG_DEBUG, "", "Constructor with size_of_each_buf = %zu\n",
         size_of_each_buf);
  // Should be multiple of 4k (Clovis requirement)
  assert(size_of_each_evbuf % 4096 == 0);
}

S3AsyncBufferOptContainer::~S3AsyncBufferOptContainer() {
  s3_log(S3_LOG_DEBUG, "", "Destructor\n");
  // release all memory
  evbuf_t* buf = NULL;
  while (!ready_q.empty()) {
    buf = ready_q.front();
    ready_q.pop_front();
    evbuffer_free(buf);
  }
  while (!processing_q.empty()) {
    buf = processing_q.front();
    processing_q.pop_front();
    evbuffer_free(buf);
  }
}

// Call this to indicate that no more data will be added to buffer.
void S3AsyncBufferOptContainer::freeze() {
  s3_log(S3_LOG_DEBUG, "", "Async buffer freezed\n");
  is_expecting_more = false;
}

bool S3AsyncBufferOptContainer::is_freezed() { return !is_expecting_more; }

size_t S3AsyncBufferOptContainer::get_content_length() {
  return content_length;
}

bool S3AsyncBufferOptContainer::add_content(evbuf_t* buf, bool is_first_buf,
                                            bool is_last_buf,
                                            bool is_put_request) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  if (!buf) {
    s3_log(S3_LOG_ERROR, "", "buf == NULL");
    return false;
  }
  size_t len = evbuffer_get_length(buf);
  s3_log(S3_LOG_DEBUG, "", "buf with len %zu\n", len);
  if (is_last_buf) {
    freeze();
  } else if (size_of_each_evbuf != len) {
    // In case of PUT request wherein we seggregate header from data,
    // the size of data will be same as that of buffer
    // length other than last buffer. However in case of say POST request
    // wherein we dont seggregate header from data, initial data length
    // may not be same as buffer length (as read contained even header)
    // hence if its not a PUT request and first buffer then its fine
    if (is_put_request || (!is_put_request && !is_first_buf)) {
      // Write content with zero data happens because pool is fully utilized,
      // and
      // libevent buffers fail to allocate memory from pool.  For some reason,
      // libevent *does not* check for memory allocation failures, and silently
      // continues working with empty buffers.  All data is skipped, but no
      // error
      // is raised.  So in the end all these empty buffers are sent to consume
      // data, and then down here.
      s3_log(S3_LOG_ERROR, "",
             "Wrong data size in buffer. Should be %zu. Actual - %zu\n",
             size_of_each_evbuf, len);
      return false;
    }
  }
  ready_q.push_back(buf);
  content_length += len;
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");

  return true;
}

// Call this to get at least expected_content_size of data buffers.
// Anything less is returned only if there is no more data to be filled in.
// Call flush_used_buffers() to drain the data that was consumed
// after get_buffers call.
// expected_content_size should be multiple of libevent read mempool item size
// except for the last payload
std::pair<std::deque<evbuf_t*>, size_t> S3AsyncBufferOptContainer::get_buffers(
    size_t expected_content_size) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  s3_log(S3_LOG_DEBUG, "", "get_buffers with expected_content_size = %zu\n",
         expected_content_size);

  size_t size_of_bufs_returned = 0;
  // previously returned bufs should be marked consumed
  assert(processing_q.empty());

  // reset
  processing_q.clear();
  count_bufs_shared_for_read = 0;

  size_t size_we_can_share = get_content_length();
  s3_log(S3_LOG_DEBUG, "", "get_buffers with size_we_can_share = %zu\n",
         size_we_can_share);
  if (size_we_can_share >= expected_content_size || !(is_expecting_more)) {
    // Count how many bufs to return.
    count_bufs_shared_for_read = expected_content_size / size_of_each_evbuf;
    if (!is_expecting_more &&
        (expected_content_size % size_of_each_evbuf != 0)) {
      // We have all data, so if last chunk is present it can be less than
      // size_of_each_evbuf, share all
      count_bufs_shared_for_read++;
    }

    evbuf_t* buf = NULL;
    size_t len = 0;
    assert(ready_q.size() >= count_bufs_shared_for_read);
    for (size_t i = 0; i < count_bufs_shared_for_read; ++i) {
      buf = ready_q.front();
      processing_q.push_back(buf);
      ready_q.pop_front();
      len = evbuffer_get_length(buf);
      size_of_bufs_returned += len;
      content_length -= len;
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting with size_of_bufs_returned = %zu\n",
         size_of_bufs_returned);
  return std::make_pair(processing_q, size_of_bufs_returned);
}

void S3AsyncBufferOptContainer::flush_used_buffers() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");

  assert(!processing_q.empty());

  evbuf_t* buf = NULL;
  size_t len = 0;
  size_t size_consumed = 0;
  while (!processing_q.empty()) {
    buf = processing_q.front();
    processing_q.pop_front();
    len = evbuffer_get_length(buf);
    size_consumed += len;
    evbuffer_free(buf);
    --count_bufs_shared_for_read;
  }
  s3_log(S3_LOG_DEBUG, "", "Freed evbuffer of len = %zu\n", size_consumed);

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return;
}

std::string S3AsyncBufferOptContainer::get_content_as_string() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  std::string content = "";

  // We should not have returned any bufs out for processing.
  assert(processing_q.empty());

  if (is_freezed()) {
    size_t len_in_buf = 0;
    size_t num_of_extents = 0;
    struct evbuffer_iovec* vec_in = NULL;
    bool out_of_memory = false;
    evbuf_t* buf = NULL;
    while (!ready_q.empty()) {
      buf = ready_q.front();
      ready_q.pop_front();

      len_in_buf = evbuffer_get_length(buf);

      num_of_extents = evbuffer_peek(buf, len_in_buf, NULL, NULL, 0);

      /* do the actual peek in buf */
      vec_in = (struct evbuffer_iovec*)calloc(num_of_extents,
                                              sizeof(struct evbuffer_iovec));
      if (vec_in == NULL) {
        s3_log(S3_LOG_ERROR, "", "Fatal: Out of memory.\n");
        out_of_memory = true;
        content = "";  // we dont return partial data
        evbuffer_free(buf);
        break;
      }
      /* do the actual peek at data inside buf */
      evbuffer_peek(buf, len_in_buf, NULL /*start of buffer*/, vec_in,
                    num_of_extents);

      for (size_t i = 0; i < num_of_extents; i++) {
        content.append((char*)vec_in[i].iov_base, vec_in[i].iov_len);
      }
      free(vec_in);
      evbuffer_free(buf);
    }
    while (out_of_memory && !ready_q.empty()) {
      // Free up what we can and return.
      buf = ready_q.front();
      ready_q.pop_front();
      evbuffer_free(buf);
    }
  }
  content_length = 0;  // Everything is returned.
  s3_log(S3_LOG_DEBUG, "", "Content size = %zu\n", content.length());
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return content;
}
