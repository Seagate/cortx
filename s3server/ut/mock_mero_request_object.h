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
 * Original author:  Rajesh Nambiar   <rajesh.nambiarr@seagate.com>
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 09-Nov-2015
 */

#pragma once

#ifndef __S3_UT_MOCK_MERO_REQUEST_OBJECT_H__
#define __S3_UT_MOCK_MERO_REQUEST_OBJECT_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mero_request_object.h"

class MockMeroRequestObject : public MeroRequestObject {
 public:
  MockMeroRequestObject(
      evhtp_request_t *req, EvhtpInterface *evhtp_obj_ptr,
      std::shared_ptr<S3AsyncBufferOptContainerFactory> async_buf_factory =
          nullptr)
      : MeroRequestObject(req, evhtp_obj_ptr, async_buf_factory) {}
  MOCK_METHOD0(c_get_full_path, const char *());
  MOCK_METHOD0(c_get_full_encoded_path, const char *());
  MOCK_METHOD0(get_host_header, std::string());
  MOCK_METHOD0(get_data_length, size_t());
  MOCK_METHOD0(get_data_length_str, std::string());
  MOCK_METHOD0(get_full_body_content_as_string, std::string &());
  MOCK_METHOD0(http_verb, S3HttpVerb());
  MOCK_METHOD0(c_get_uri_query, const char *());
  MOCK_METHOD0(get_query_parameters,
               const std::map<std::string, std::string, compare> &());
  MOCK_METHOD0(get_request, evhtp_request_t *());
  MOCK_METHOD0(get_content_length, size_t());
  MOCK_METHOD0(get_content_length_str, std::string());
  MOCK_METHOD0(get_key_name, const std::string &());
  MOCK_METHOD0(get_object_oid_lo, const std::string &());
  MOCK_METHOD0(get_object_oid_hi, const std::string &());
  MOCK_METHOD0(get_index_id_lo, const std::string &());
  MOCK_METHOD0(get_index_id_hi, const std::string &());
  MOCK_METHOD1(set_key_name, void(const std::string &key));
  MOCK_METHOD1(set_object_oid_lo, void(const std::string &oid_lo));
  MOCK_METHOD1(set_object_oid_hi, void(const std::string &oid_hi));
  MOCK_METHOD1(set_index_id_lo, void(const std::string &index_lo));
  MOCK_METHOD1(set_index_id_hi, void(const std::string &index_hi));
  MOCK_METHOD0(has_all_body_content, bool());
  MOCK_METHOD0(is_chunked, bool());
  MOCK_METHOD0(pause, void());
  MOCK_METHOD1(resume, void(bool set_read_timeout_timer));
  MOCK_METHOD1(has_query_param_key, bool(std::string key));
  MOCK_METHOD1(set_api_type, void(MeroApiType));
  MOCK_METHOD1(get_query_string_value, std::string(std::string key));
  MOCK_METHOD0(get_api_type, MeroApiType());
  MOCK_METHOD1(respond_retry_after, void(int retry_after_in_secs));
  MOCK_METHOD2(set_out_header_value, void(std::string, std::string));
  MOCK_METHOD0(get_in_headers_copy, std::map<std::string, std::string> &());
  MOCK_METHOD2(send_response, void(int, std::string));
  MOCK_METHOD1(send_reply_start, void(int code));
  MOCK_METHOD2(send_reply_body, void(char *data, int length));
  MOCK_METHOD0(send_reply_end, void());
  MOCK_METHOD0(is_chunk_detail_ready, bool());
  MOCK_METHOD0(pop_chunk_detail, S3ChunkDetail());

  MOCK_METHOD2(listen_for_incoming_data,
               void(std::function<void()> callback, size_t notify_on_size));
  MOCK_METHOD1(get_header_value, std::string(std::string key));
};

#endif

