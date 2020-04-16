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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 15-Mar-2016
 */

#include <stdlib.h>

#include "s3_chunk_payload_parser.h"
#include "s3_iem.h"
#include "s3_log.h"
#include "s3_option.h"

extern S3Option *g_option_instance;

S3ChunkDetail::S3ChunkDetail() : chunk_size(0), ready(false), chunk_number(0) {}

void S3ChunkDetail::reset() {
  ready = false;
  chunk_size = 0;
  signature = "";
  payload_hash = "";
  hash_ctx.reset();
}

void S3ChunkDetail::debug_dump() {
  s3_log(S3_LOG_DEBUG, "",
         "Chunk Details start: chunk_number = [%d], size = [%zu],\n\
         signature = [%s]\nHash = [%s]\nChunk Details end.\n",
         chunk_number, chunk_size, signature.c_str(), payload_hash.c_str());
}

bool S3ChunkDetail::is_ready() { return ready; }

void S3ChunkDetail::add_size(size_t size) { chunk_size = size; }

void S3ChunkDetail::add_signature(const std::string &sign) { signature = sign; }

bool S3ChunkDetail::update_hash(const void *data_ptr, size_t data_len) {
  if (data_ptr == NULL) {
    std::string emptee_string = "";
    return hash_ctx.Update(emptee_string.c_str(), emptee_string.length());
  }
  return hash_ctx.Update((const char *)data_ptr, data_len);
}

bool S3ChunkDetail::fini_hash() {
  bool status = hash_ctx.Finalize();
  if (status) {
    payload_hash = hash_ctx.get_hex_hash();
    ready = true;
  }
  return status;
}

size_t S3ChunkDetail::get_size() { return chunk_size; }

const std::string &S3ChunkDetail::get_signature() { return signature; }

std::string S3ChunkDetail::get_payload_hash() { return payload_hash; }

S3ChunkPayloadParser::S3ChunkPayloadParser()
    : parser_state(ChunkParserState::c_start),
      chunk_data_size_to_read(0),
      content_length(0),
      chunk_sig_key_q_const(S3_AWS_CHUNK_KEY),
      chunk_sig_key_char_state(S3_AWS_CHUNK_KEY) {
  s3_log(S3_LOG_DEBUG, "", "Constructor\n");

  evbuf_t *spare_buffer = evbuffer_new();
  // Lets just preallocate space in evbuf to max we intend
  evbuffer_expand(spare_buffer,
                  g_option_instance->get_libevent_pool_buffer_size());
  // We will never write more than this in single spare buffer
  spare_buffers.push_back(spare_buffer);
  s3_log(S3_LOG_DEBUG, "", "spare_buffers.size(%zu)\n", spare_buffers.size());
}

S3ChunkPayloadParser::~S3ChunkPayloadParser() {
  s3_log(S3_LOG_DEBUG, "", "Destructor\n");
  evbuf_t *buf = NULL;
  while (!spare_buffers.empty()) {
    buf = spare_buffers.front();
    spare_buffers.pop_front();
    evbuffer_free(buf);
  }
}

void S3ChunkPayloadParser::reset_parser_state() {
  chunk_data_size_to_read = 0;
  current_chunk_size = "";
  current_chunk_signature = "";
  chunk_sig_key_char_state = chunk_sig_key_q_const;
  current_chunk_detail.reset();
  current_chunk_detail.incr_chunk_number();
}

void S3ChunkPayloadParser::add_to_spare(const void *data, size_t len) {
  s3_log(S3_LOG_DEBUG, "", "data(%p), len(%zu)\n", data, len);
  evbuf_t *spare = spare_buffers.front();
  size_t buf_size = g_option_instance->get_libevent_pool_buffer_size();
  size_t free_in_current_spare = buf_size - evbuffer_get_length(spare);
  // Distribute across spares
  s3_log(S3_LOG_DEBUG, "", "spare_buffers.size(%zu)\n", spare_buffers.size());
  if (len <= free_in_current_spare) {
    s3_log(S3_LOG_DEBUG, "", "Adding all in free_in_current_spare(%zu)\n",
           free_in_current_spare);
    evbuffer_add(spare, data, len);
  } else {
    // add in current spare with free space
    s3_log(S3_LOG_DEBUG, "", "Adding possible in free_in_current_spare(%zu)\n",
           free_in_current_spare);
    evbuffer_add(spare, data, free_in_current_spare);
    len -= free_in_current_spare;

    // move full spare to ready and pick up next spare
    ready_buffers.push_back(spare);
    spare_buffers.pop_front();

    // add remaining in next spare
    assert(!spare_buffers.empty());
    spare = spare_buffers.front();
    evbuffer_add(
        spare, (const void *)((const char *)data + free_in_current_spare), len);
  }
  s3_log(S3_LOG_DEBUG, "", "spare_buffers.size(%zu)\n", spare_buffers.size());
}

/*
 *  <IEM_INLINE_DOCUMENTATION>
 *    <event_code>047002001</event_code>
 *    <application>S3 Server</application>
 *    <submodule>Chunk upload</submodule>
 *    <description>Chunk parsing failed</description>
 *    <audience>Development</audience>
 *    <details>
 *      Logical error occurred during chunk parsing.
 *      The data section of the event has following keys:
 *        time - timestamp.
 *        node - node name.
 *        pid  - process-id of s3server instance, useful to identify logfile.
 *        file - source code filename.
 *        line - line number within file where error occurred.
 *    </details>
 *    <service_actions>
 *      Save the S3 server log files. Contact development team for further
 *      investigation.
 *    </service_actions>
 *  </IEM_INLINE_DOCUMENTATION>
 */

std::deque<evbuf_t *> S3ChunkPayloadParser::run(evbuf_t *buf) {
  ready_buffers.clear();  // will be filled with add_to_spare

  size_t len_in_buf = evbuffer_get_length(buf);
  s3_log(S3_LOG_DEBUG, "", "Parsing evbuffer_get_length(buf) = %zu\n",
         len_in_buf);

  size_t num_of_extents = evbuffer_peek(buf, len_in_buf, NULL, NULL, 0);
  s3_log(S3_LOG_DEBUG, "", "num_of_extents = %zu\n", num_of_extents);

  /* do the actual peek */
  struct evbuffer_iovec *vec_in = (struct evbuffer_iovec *)calloc(
      num_of_extents, sizeof(struct evbuffer_iovec));

  /* do the actual peek at data */
  evbuffer_peek(buf, len_in_buf, NULL /*start of buffer*/, vec_in,
                num_of_extents);

  for (size_t i = 0; i < num_of_extents; i++) {
    // std::string chunk_debug((const char*)vec_in[i].iov_base,
    // vec_in[i].iov_len);
    // s3_log(S3_LOG_DEBUG, "", "vec_in[i].iov_base = %s\n",
    // chunk_debug.c_str());
    // s3_log(S3_LOG_DEBUG, "", "vec_in[i].iov_len = %zu\n", vec_in[i].iov_len);
    // Within current vec
    for (size_t j = 0; j < vec_in[i].iov_len; j++) {
      // Parsing Syntax:
      // string(IntHexBase(chunk-size)) + ";chunk-signature=" + signature + \r\n
      // + chunk-data + \r\n
      if (parser_state == ChunkParserState::c_start) {
        reset_parser_state();
        // reset_parsing = false;
        parser_state = ChunkParserState::c_chunk_size;
      }
      switch (parser_state) {
        case ChunkParserState::c_error: {
          s3_log(S3_LOG_ERROR, "",
                 "ChunkParserState::c_error. i(%zu), j(%zu)\n", i, j);
          free(vec_in);
          return ready_buffers;
        }
        case ChunkParserState::c_start: {
          s3_log(S3_LOG_DEBUG, "",
                 "ChunkParserState::c_start. i(%zu), j(%zu)\n", i, j);
        }  // dummycase will never happen, see 1 line above
        case ChunkParserState::c_chunk_size: {
          s3_log(S3_LOG_DEBUG, "",
                 "ChunkParserState::c_chunk_size. i(%zu), j(%zu)\n", i, j);
          const char *chptr = (const char *)vec_in[i].iov_base;

          if (chptr[j] == ';') {
            parser_state = ChunkParserState::c_chunk_signature_key;
            chunk_sig_key_char_state = chunk_sig_key_q_const;
            chunk_data_size_to_read =
                strtol(current_chunk_size.c_str(), NULL, 16);
            current_chunk_detail.add_size(chunk_data_size_to_read);
            s3_log(S3_LOG_DEBUG, "", "current_chunk_size = [%s]\n",
                   current_chunk_size.c_str());
            s3_log(S3_LOG_DEBUG, "", "chunk_data_size_to_read (int) = [%zu]\n",
                   chunk_data_size_to_read);

            // ignore the semicolon
          } else {
            current_chunk_size.push_back(chptr[j]);
          }
          break;
        }
        case ChunkParserState::c_chunk_signature_key: {
          s3_log(S3_LOG_DEBUG, "",
                 "ChunkParserState::c_chunk_signature_key. i(%zu), j(%zu)\n", i,
                 j);
          const char *chptr = (const char *)vec_in[i].iov_base;

          if (chunk_sig_key_char_state.empty()) {
            // ignore '='
            if (chptr[j] == '=') {
              parser_state = ChunkParserState::c_chunk_signature_value;
            } else {
              parser_state = ChunkParserState::c_error;
              free(vec_in);
              return ready_buffers;
            }
          } else {
            if (chptr[j] == chunk_sig_key_char_state.front()) {
              chunk_sig_key_char_state.pop();
            } else {
              parser_state = ChunkParserState::c_error;
              free(vec_in);
              return ready_buffers;
            }
          }
          break;
        }
        case ChunkParserState::c_chunk_signature_value: {
          s3_log(S3_LOG_DEBUG, "",
                 "ChunkParserState::c_chunk_signature_value. i(%zu), j(%zu)\n",
                 i, j);
          const char *chptr = (const char *)vec_in[i].iov_base;
          if ((unsigned char)chptr[j] == CR) {
            parser_state = ChunkParserState::c_cr;
          } else {
            current_chunk_signature.push_back(chptr[j]);
          }
          break;
        }
        case ChunkParserState::c_cr: {
          s3_log(S3_LOG_DEBUG, "", "ChunkParserState::c_cr. i(%zu), j(%zu)\n",
                 i, j);
          const char *chptr = (const char *)vec_in[i].iov_base;
          if ((unsigned char)chptr[j] == LF) {
            // CRLF means we are done with signature
            parser_state = ChunkParserState::c_chunk_data;
            current_chunk_detail.add_signature(current_chunk_signature);
            s3_log(S3_LOG_DEBUG, "", "current_chunk_signature = [%s]\n",
                   current_chunk_signature.c_str());
          } else {
            // what we detected as CR was part of signature, move back
            current_chunk_signature.push_back(CR);
            if ((unsigned char)chptr[j] == CR) {
              parser_state = ChunkParserState::c_cr;
            } else {
              current_chunk_signature.push_back(chptr[j]);
            }
          }
          break;
        }
        case ChunkParserState::c_chunk_data: {
          s3_log(S3_LOG_DEBUG, "",
                 "ChunkParserState::c_chunk_data. i(%zu), j(%zu)\n", i, j);

          // we have to read 'chunk_data_size_to_read' data, so we have 3 cases.
          // 1. current payload has "all" and "only" data related to current
          // chunk, optionally crlf
          // 2. current payload has "partial" and "only" data related to current
          // chunk.
          // 3. current payload had "all" data of current chunk and some part of
          // next chunk
          s3_log(S3_LOG_DEBUG, "", "chunk_data_size_to_read(%zu)\n",
                 chunk_data_size_to_read);
          if (chunk_data_size_to_read > 0) {
            // How much data does current vec has for current chunk?
            void *data_ptr = (void *)((const char *)(vec_in[i].iov_base) + j);
            size_t data_len = 0;
            s3_log(S3_LOG_DEBUG, "", "vec_in[i].iov_len(%zu)\n",
                   vec_in[i].iov_len);
            if (chunk_data_size_to_read <= vec_in[i].iov_len - j) {
              // current vec has all data and possibly next some of chunk
              data_len = chunk_data_size_to_read;
            } else {
              // Current vec has only data
              data_len = vec_in[i].iov_len - j;
            }
            add_to_spare(data_ptr, data_len);
            current_chunk_detail.update_hash(data_ptr, data_len);
            chunk_data_size_to_read -= data_len;
            content_length -= data_len;
            s3_log(S3_LOG_DEBUG, "", "chunk_data_size_to_read(%zu)\n",
                   chunk_data_size_to_read);
            j += data_len - 1;

            if (chunk_data_size_to_read == 0) {
              s3_log(S3_LOG_DEBUG, "",
                     "goto c_chunk_data_end_cr i(%zu), j(%zu)\n", i, j);
              // Means we are moving on to crlf followed by next chunk.
              parser_state = ChunkParserState::c_chunk_data_end_cr;
            } else if (chunk_data_size_to_read < 0) {
              parser_state = ChunkParserState::c_error;
              free(vec_in);
              return ready_buffers;
            } else {
              // we are expecting more data, keep going with same parser state
            }
          } else {
            // This can be last chunk with size 0
            s3_log(S3_LOG_DEBUG, "", "Last chunk of size 0 i(%zu), j(%zu)\n", i,
                   j);
            parser_state = ChunkParserState::c_chunk_data_end_cr;
            j--;
            current_chunk_detail.update_hash(NULL);
          }
          break;
        }
        case ChunkParserState::c_chunk_data_end_cr: {
          s3_log(S3_LOG_DEBUG, "",
                 "ChunkParserState::c_chunk_data_end_cr. i(%zu), j(%zu)\n", i,
                 j);
          const char *chptr = (const char *)vec_in[i].iov_base;

          if ((unsigned char)chptr[j] == CR) {
            parser_state = ChunkParserState::c_chunk_data_end_lf;
          } else {
            parser_state = ChunkParserState::c_error;
            free(vec_in);
            return ready_buffers;
          }
          break;
        }
        case ChunkParserState::c_chunk_data_end_lf: {
          s3_log(S3_LOG_DEBUG, "",
                 "ChunkParserState::c_chunk_data_end_lf. i(%zu), j(%zu)\n", i,
                 j);
          const char *chptr = (const char *)vec_in[i].iov_base;

          if ((unsigned char)chptr[j] == LF) {
            // CRLF means we are done with data
            parser_state = ChunkParserState::c_start;
            current_chunk_detail.fini_hash();
            current_chunk_detail.debug_dump();
            chunk_details.push(current_chunk_detail);
          } else {
            parser_state = ChunkParserState::c_error;
            free(vec_in);
            return ready_buffers;
          }
          break;
        }
        default: {
          s3_log(S3_LOG_ERROR, "", "Invalid ChunkParserState. i(%zu), j(%zu)\n",
                 i, j);
          s3_iem(LOG_ERR, S3_IEM_CHUNK_PARSING_FAIL,
                 S3_IEM_CHUNK_PARSING_FAIL_STR, S3_IEM_CHUNK_PARSING_FAIL_JSON);
          parser_state = ChunkParserState::c_error;
          free(vec_in);
          return ready_buffers;
        }
      };  // switch
    }     // Inner for
  }       // for num_of_extents
  free(vec_in);

  s3_log(S3_LOG_DEBUG, "", "content_length(%zu)\n", content_length);
  if (content_length == 0) {
    // we have all required data, move partially filled spares to ready.
    while (!spare_buffers.empty()) {
      evbuf_t *spare = spare_buffers.front();
      s3_log(S3_LOG_DEBUG, "", "move evbuffer_get_length(spare)(%zu)\n",
             evbuffer_get_length(spare));
      if (evbuffer_get_length(spare) > 0) {
        ready_buffers.push_back(spare);
      } else {
        evbuffer_free(spare);
      }
      spare_buffers.pop_front();
      s3_log(S3_LOG_DEBUG, "", "spare_buffers.size(%zu)\n",
             spare_buffers.size());
    }
  }
  evbuffer_drain(buf, -1);
  spare_buffers.push_back(buf);
  return ready_buffers;
}

bool S3ChunkPayloadParser::is_chunk_detail_ready() {
  if (!chunk_details.empty() && chunk_details.front().is_ready()) {
    return true;
  }
  return false;
}

// Check if its ready before pop
S3ChunkDetail S3ChunkPayloadParser::pop_chunk_detail() {
  if (chunk_details.front().is_ready()) {
    S3ChunkDetail detail = chunk_details.front();
    chunk_details.pop();
    return detail;
  }
  S3ChunkDetail detail;
  return detail;
}
