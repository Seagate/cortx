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

#pragma once

#ifndef __MERO_FE_S3_SERVER_S3_ASYNC_BUFFER_OPT_H__
#define __MERO_FE_S3_SERVER_S3_ASYNC_BUFFER_OPT_H__

#include <deque>
#include <string>
#include <utility>

#include <evhtp.h>
#include "s3_log.h"

#include <gtest/gtest_prod.h>

// Zero-copy Optimized version of S3AsyncBufferContainer
// Constraints: Each block of data within async buffer
// is contiguous bytes of block configured to be 4k/8k/16.
// Wrapper around evhtp evbuf_t buffers for incoming data.
class S3AsyncBufferOptContainer {
 private:
  // Remembers all the incoming data buffers
  // buf which is completely filled with data and ready for use
  std::deque<evbuf_t *> ready_q;
  // buf given out for consumption
  std::deque<evbuf_t *> processing_q;

  size_t size_of_each_evbuf;  // ideally 4k/8k/16k

  // Actual content within buffer
  size_t content_length;

  bool is_expecting_more;

  // Manages read state. stores count of bufs shared outside for consumption.
  size_t count_bufs_shared_for_read;

 public:
  S3AsyncBufferOptContainer(size_t size_of_each_buf);
  virtual ~S3AsyncBufferOptContainer();

  // Call this to indicate that no more data will be added to buffer.
  void freeze();

  virtual bool is_freezed();

  virtual size_t get_content_length();

  bool add_content(evbuf_t *buf, bool is_first_buf, bool is_last_buf = false,
                   bool is_put_request = false);

  // Call this to get at least expected_content_size of data buffers.
  // Anything less is returned only if there is no more data to be filled in.
  // Call flush_used_buffers() to drain the data that was consumed
  // after get_buffers call.
  // expected_content_size should be multiple of libevent read mempool item size
  std::pair<std::deque<evbuf_t *>, size_t> get_buffers(
      size_t expected_content_size);

  // Pull up all the data in contiguous memory and releases internal buffers
  // only if all data is in and its freezed. Use check is_freezed()
  std::string get_content_as_string();

  // flush buffers received using get_buffers
  void flush_used_buffers();
};

#endif
