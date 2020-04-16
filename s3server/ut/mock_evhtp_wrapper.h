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
 * Original creation date: 04-Jan-2016
 */

#pragma once

#ifndef __S3_UT_MOCK_EVHTP_WRAPPER_H__
#define __S3_UT_MOCK_EVHTP_WRAPPER_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "evhtp_wrapper.h"

class MockEvhtpWrapper : public EvhtpInterface {
 public:
  MockEvhtpWrapper() {}
  MOCK_METHOD1(http_request_pause, void(evhtp_request_t *request));
  MOCK_METHOD1(http_request_resume, void(evhtp_request_t *request));
  MOCK_METHOD1(http_request_get_proto, evhtp_proto(evhtp_request_t *request));

  MOCK_METHOD3(http_kvs_for_each,
               int(evhtp_kvs_t *kvs, evhtp_kvs_iterator cb, void *arg));
  MOCK_METHOD2(http_header_find,
               const char *(evhtp_headers_t *headers, const char *key));

  MOCK_METHOD2(http_headers_add_header,
               void(evhtp_headers_t *headers, evhtp_header_t *header));
  MOCK_METHOD4(http_header_new,
               evhtp_header_t *(const char *key, const char *val, char kalloc,
                                char valloc));
  MOCK_METHOD2(http_kv_find, const char *(evhtp_kvs_t *kvs, const char *key));
  MOCK_METHOD2(http_kvs_find_kv,
               evhtp_kv_t *(evhtp_kvs_t *kvs, const char *key));

  MOCK_METHOD2(http_send_reply, void(evhtp_request_t *request, evhtp_res code));
  MOCK_METHOD2(http_send_reply_start,
               void(evhtp_request_t *request, evhtp_res code));
  MOCK_METHOD2(http_send_reply_body,
               void(evhtp_request_t *request, evbuf_t *buf));
  MOCK_METHOD1(http_send_reply_end, void(evhtp_request_t *request));

  // Libevent wrappers
  MOCK_METHOD1(evbuffer_get_length, size_t(const struct evbuffer *buf));
};

#endif
