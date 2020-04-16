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
 * Original creation date: 3-Dec-2015
 */

#pragma once

#ifndef __S3_SERVER_S3_ASYNC_BUFFER_H__
#define __S3_SERVER_S3_ASYNC_BUFFER_H__

#include <deque>
#include <string>
#include <tuple>

#include <evhtp.h>
#include "s3_log.h"

#include <gtest/gtest_prod.h>

// Wrapper around evhtp evbuf_t buffers for incoming data.
class S3AsyncBufferContainer {
 private:
  // Remembers all the incoming data buffers
  std::deque<evbuf_t*> buffered_input;
  size_t buffered_input_length;

  bool is_expecting_more;

  // Manages read state. stores count of bufs shared outside for consumption.
  size_t count_bufs_shared_for_read;

 public:
  S3AsyncBufferContainer();
  virtual ~S3AsyncBufferContainer();

  // Call this to indicate that no more data will be added to buffer.
  void freeze();

  bool is_freezed();

  size_t length();

  void add_content(evbuf_t* buf);

  // Call this to get at least expected_content_size of data buffers.
  // Anything less is returned only if there is no more data to be filled in.
  // Call mark_size_of_data_consumed() to drain the data that was consumed
  // after get_buffers_ref call.
  std::deque<std::tuple<void*, size_t> > get_buffers_ref(
      size_t expected_content_size);

  // Pull up all the data in contiguous memory and releases internal buffers
  // only if all data is in and its freezed. Use check is_freezed()
  std::string get_content_as_string();

  void mark_size_of_data_consumed(size_t size_consumed);

  // Unit Testing only
  FRIEND_TEST(S3AsyncBufferContainerTest,
              DataBufferIsAddedToQueueAndCanBeRetrieved);
};

#endif
