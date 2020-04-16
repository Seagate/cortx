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

#include "s3_chunk_payload_parser.h"
#include "gtest/gtest.h"
#include "s3_option.h"

extern S3Option* g_option_instance;

// To use a test fixture, derive a class from testing::Test.
class S3ChunkPayloadParserTest : public testing::Test {
 protected:  // You should make the members protected s.t. they can be
             // accessed from sub-classes.

  S3ChunkPayloadParserTest()
      : nfourk_buffer(g_option_instance->get_libevent_pool_buffer_size(), 'A') {
    parser = new S3ChunkPayloadParser();
  }

  ~S3ChunkPayloadParserTest() { delete parser; }

  evbuf_t* allocate_evbuf() {
    evbuf_t* buffer = evbuffer_new();
    // Lets just preallocate space in evbuf to max we intend
    evbuffer_expand(buffer, g_option_instance->get_libevent_pool_buffer_size());
    return buffer;
  }

  // A helper functions
  // Returns an evbuf_t with given data. Caller to manage memory
  evbuf_t* get_evbuf_t_with_data(std::string content) {
    evbuf_t* buf = allocate_evbuf();
    evbuffer_add(buf, content.c_str(), content.length());
    return buf;
  }

  // gets the internal data pointer with length from evbuf_t
  const char* get_datap_4_evbuf_t(evbuf_t* buf) {
    return (const char*)evbuffer_pullup(buf, evbuffer_get_length(buf));
  }

  // Declares the variables your tests want to use.
  S3ChunkPayloadParser* parser;
  std::string nfourk_buffer;
};

TEST_F(S3ChunkPayloadParserTest, HasProperInitState) {
  EXPECT_EQ(ChunkParserState::c_start, parser->get_state());
  EXPECT_FALSE(parser->is_chunk_detail_ready());
  EXPECT_TRUE(parser->current_chunk_size.empty());
  EXPECT_EQ(0, parser->chunk_data_size_to_read);
  EXPECT_EQ(0, parser->content_length);
  EXPECT_TRUE(parser->current_chunk_signature.empty());
  EXPECT_TRUE(parser->chunk_details.empty());
  EXPECT_TRUE(parser->ready_buffers.empty());
  EXPECT_EQ(1, parser->spare_buffers.size());
}

TEST_F(S3ChunkPayloadParserTest, AddToSpareBufferEqualToSpareSize) {
  parser->add_to_spare(nfourk_buffer.c_str(), nfourk_buffer.length());
  // Allocate spare like parser::run()
  parser->spare_buffers.push_back(allocate_evbuf());

  parser->add_to_spare(nfourk_buffer.c_str(), nfourk_buffer.length());

  EXPECT_EQ(1, parser->spare_buffers.size());
  EXPECT_EQ(1, parser->ready_buffers.size());

  EXPECT_EQ(nfourk_buffer.length(),
            evbuffer_get_length(parser->ready_buffers.front()));
  EXPECT_EQ(nfourk_buffer.length(),
            evbuffer_get_length(parser->spare_buffers.front()));
}

TEST_F(S3ChunkPayloadParserTest, AddToSpareBufferNotEqualToStdSpareSize) {
  parser->add_to_spare(nfourk_buffer.c_str(), nfourk_buffer.length());
  // Allocate spare like parser::run()
  parser->spare_buffers.push_back(allocate_evbuf());

  parser->add_to_spare("ABCD", 4);

  EXPECT_EQ(1, parser->spare_buffers.size());
  EXPECT_EQ(1, parser->ready_buffers.size());

  EXPECT_EQ(nfourk_buffer.length(),
            evbuffer_get_length(parser->ready_buffers.front()));
  EXPECT_EQ(4, evbuffer_get_length(parser->spare_buffers.front()));
}

TEST_F(S3ChunkPayloadParserTest, AddToSpareBufferNotEqualToStdSpareSize1) {
  parser->add_to_spare(nfourk_buffer.c_str(), nfourk_buffer.length() - 10);
  // Allocate spare like parser::run()
  parser->spare_buffers.push_back(allocate_evbuf());

  parser->add_to_spare(nfourk_buffer.c_str(), nfourk_buffer.length());

  EXPECT_EQ(1, parser->spare_buffers.size());
  EXPECT_EQ(1, parser->ready_buffers.size());

  EXPECT_EQ(nfourk_buffer.length(),
            evbuffer_get_length(parser->ready_buffers.front()));
  EXPECT_EQ(nfourk_buffer.length() - 10,
            evbuffer_get_length(parser->spare_buffers.front()));
}

TEST_F(S3ChunkPayloadParserTest, AddToSpareBufferNotEqualToStdSpareSize2) {
  parser->add_to_spare(nfourk_buffer.c_str(), nfourk_buffer.length() - 10);

  // Allocate spare like parser::run()
  parser->spare_buffers.push_back(allocate_evbuf());
  parser->add_to_spare(nfourk_buffer.c_str(), nfourk_buffer.length());

  // Allocate spare like parser::run()
  parser->spare_buffers.push_back(allocate_evbuf());
  parser->add_to_spare(nfourk_buffer.c_str(), nfourk_buffer.length());

  EXPECT_EQ(1, parser->spare_buffers.size());
  EXPECT_EQ(2, parser->ready_buffers.size());

  EXPECT_EQ(nfourk_buffer.length(),
            evbuffer_get_length(parser->ready_buffers.front()));
  parser->ready_buffers.pop_front();
  EXPECT_EQ(nfourk_buffer.length(),
            evbuffer_get_length(parser->ready_buffers.front()));

  EXPECT_EQ(nfourk_buffer.length() - 10,
            evbuffer_get_length(parser->spare_buffers.front()));
}
