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
 * Original creation date: 24-Jan-2017
 */

#include "s3_async_buffer_opt.h"
#include "gtest/gtest.h"
#include "s3_option.h"

extern S3Option *g_option_instance;

// To use a test fixture, derive a class from testing::Test.
class S3AsyncBufferOptContainerTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.

  S3AsyncBufferOptContainerTest()
      : nfourk_buffer(g_option_instance->get_libevent_pool_buffer_size(), 'A') {
    buffer = new S3AsyncBufferOptContainer(
        g_option_instance->get_libevent_pool_buffer_size());
  }

  ~S3AsyncBufferOptContainerTest() { delete buffer; }

  evbuf_t *allocate_evbuf() {
    evbuf_t *buffer = evbuffer_new();
    // Lets just preallocate space in evbuf to max we intend
    evbuffer_expand(buffer, g_option_instance->get_libevent_pool_buffer_size());
    return buffer;
  }

  // A helper functions
  evbuf_t *get_evbuf_t_with_data(std::string content) {
    evbuf_t *buf = allocate_evbuf();
    // buf lifetime is managed by S3AsyncBufferOptContainer
    evbuffer_add(buf, content.c_str(), content.length());
    return buf;
  }

  // gets the internal data pointer with length from evbuf_t
  const char *get_datap_4_evbuf_t(evbuf_t *buf) {
    return (const char *)evbuffer_pullup(buf, evbuffer_get_length(buf));
  }

  // Declares the variables your tests want to use.
  S3AsyncBufferOptContainer *buffer;
  std::string nfourk_buffer;
};

TEST_F(S3AsyncBufferOptContainerTest, HasProperInitState) {
  EXPECT_EQ(0, buffer->get_content_length());
  EXPECT_FALSE(buffer->is_freezed());
  EXPECT_STREQ("", buffer->get_content_as_string().c_str());
}

TEST_F(S3AsyncBufferOptContainerTest, IsProperlyLockedWithFreeze) {
  buffer->freeze();
  EXPECT_TRUE(buffer->is_freezed());
  EXPECT_STREQ("", buffer->get_content_as_string().c_str());
}

TEST_F(S3AsyncBufferOptContainerTest, DataBufferAddedToQueueAndCanBeRetrieved) {
  bool is_last_buf = true;
  buffer->add_content(get_evbuf_t_with_data("Hello World"), true, is_last_buf,
                      true);
  buffer->freeze();
  EXPECT_TRUE(buffer->is_freezed());
  EXPECT_STREQ("Hello World", buffer->get_content_as_string().c_str());
}

TEST_F(S3AsyncBufferOptContainerTest,
       DataBufferAddedToQueueAndCanBeRetrieved1) {
  bool is_last_buf = true;
  buffer->add_content(get_evbuf_t_with_data(nfourk_buffer), true, is_last_buf,
                      true);
  buffer->freeze();
  EXPECT_TRUE(buffer->is_freezed());
  EXPECT_EQ(nfourk_buffer, buffer->get_content_as_string().c_str());
}

TEST_F(S3AsyncBufferOptContainerTest,
       MultipleDataBufferAddedToQueueAndCanBeRetrieved) {
  buffer->add_content(get_evbuf_t_with_data(nfourk_buffer), false, false, true);
  bool is_last_buf = true;
  buffer->add_content(get_evbuf_t_with_data("ABCD"), true, is_last_buf, true);
  buffer->freeze();
  EXPECT_TRUE(buffer->is_freezed());
  EXPECT_EQ(nfourk_buffer.length() + 4,
            buffer->get_content_as_string().length());
}

TEST_F(S3AsyncBufferOptContainerTest,
       DataInTransitAndAskingForMoreReturnsEmptyQ) {
  buffer->add_content(get_evbuf_t_with_data(nfourk_buffer), false, false, true);

  EXPECT_EQ(nfourk_buffer.length(), buffer->get_content_length());
  EXPECT_FALSE(buffer->is_freezed());

  auto ret = buffer->get_buffers(2 * nfourk_buffer.length());
  // Returns empty Q
  EXPECT_TRUE(ret.first.empty());
  // Returns zero size
  EXPECT_TRUE(ret.second == 0);
}

TEST_F(S3AsyncBufferOptContainerTest,
       DataInTransitAndAskingForLessReturnsValidData) {
  // Add 2 buffers and fetch 1
  buffer->add_content(get_evbuf_t_with_data(nfourk_buffer), false, false, true);
  buffer->add_content(get_evbuf_t_with_data(nfourk_buffer), false, false, true);

  EXPECT_EQ(nfourk_buffer.length() * 2, buffer->get_content_length());
  EXPECT_FALSE(buffer->is_freezed());

  // Ask for first part
  auto ret = buffer->get_buffers(nfourk_buffer.length());

  EXPECT_EQ(1, ret.first.size());
  evbuf_t *buf = ret.first.front();
  EXPECT_EQ(nfourk_buffer.length(), evbuffer_get_length(buf));
  EXPECT_EQ(nfourk_buffer.length(), ret.second);
  EXPECT_EQ(0, strncmp(nfourk_buffer.c_str(), get_datap_4_evbuf_t(buf),
                       nfourk_buffer.length()));

  // Pending length in async buffer
  EXPECT_EQ(nfourk_buffer.length(), buffer->get_content_length());

  buffer->flush_used_buffers();
}

TEST_F(S3AsyncBufferOptContainerTest,
       AllDataReceivedAndAskingForMoreReturnsAllAvailable) {
  bool is_last_buf = true;
  buffer->add_content(get_evbuf_t_with_data(nfourk_buffer), false, false, true);
  buffer->add_content(get_evbuf_t_with_data("Seagate"), false, is_last_buf,
                      true);

  EXPECT_EQ(nfourk_buffer.length() + 7, buffer->get_content_length());
  EXPECT_TRUE(buffer->is_freezed());

  auto ret = buffer->get_buffers(2 * nfourk_buffer.length());
  EXPECT_EQ(nfourk_buffer.length() + 7, ret.second);
  EXPECT_EQ(2, ret.first.size());

  evbuf_t *first_buf = ret.first.front();
  EXPECT_EQ(0, strncmp(nfourk_buffer.c_str(), get_datap_4_evbuf_t(first_buf),
                       nfourk_buffer.length()));

  evbuf_t *last_buf = ret.first.back();
  EXPECT_EQ(0, strncmp("Seagate", get_datap_4_evbuf_t(last_buf), 7));
}
