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
 * Original creation date: 4-Jan-2016
 */

#pragma once

#ifndef __S3_SERVER_EVHTP_WRAPPER_H__
#define __S3_SERVER_EVHTP_WRAPPER_H__

#include "s3_common.h"

EXTERN_C_BLOCK_BEGIN

/* libevhtp */
#include <evhtp.h>

EXTERN_C_BLOCK_END

// A wrapper class for libevhtp functions so that we are able to mock
// c functions in tests. For Prod (non-test) this will just forward the calls.
class EvhtpInterface {
 public:
  virtual ~EvhtpInterface() {}
  virtual void http_request_pause(evhtp_request_t *request) = 0;
  virtual void http_request_resume(evhtp_request_t *request) = 0;
  virtual evhtp_proto http_request_get_proto(evhtp_request_t *request) = 0;

  virtual int http_kvs_for_each(evhtp_kvs_t *kvs, evhtp_kvs_iterator cb,
                                void *arg) = 0;
  virtual const char *http_header_find(evhtp_headers_t *headers,
                                       const char *key) = 0;

  virtual void http_headers_add_header(evhtp_headers_t *headers,
                                       evhtp_header_t *header) = 0;
  virtual evhtp_header_t *http_header_new(const char *key, const char *val,
                                          char kalloc, char valloc) = 0;
  virtual const char *http_kv_find(evhtp_kvs_t *kvs, const char *key) = 0;
  virtual evhtp_kv_t *http_kvs_find_kv(evhtp_kvs_t *kvs, const char *key) = 0;

  virtual void http_send_reply(evhtp_request_t *request, evhtp_res code) = 0;
  virtual void http_send_reply_start(evhtp_request_t *request,
                                     evhtp_res code) = 0;
  virtual void http_send_reply_body(evhtp_request_t *request, evbuf_t *buf) = 0;
  virtual void http_send_reply_end(evhtp_request_t *request) = 0;

  // Libevent wrappers
  virtual size_t evbuffer_get_length(const struct evbuffer *buf) = 0;
};

class EvhtpWrapper : public EvhtpInterface {
 public:
  void http_request_pause(evhtp_request_t *request);
  void http_request_resume(evhtp_request_t *request);
  evhtp_proto http_request_get_proto(evhtp_request_t *request);

  int http_kvs_for_each(evhtp_kvs_t *kvs, evhtp_kvs_iterator cb, void *arg);
  const char *http_header_find(evhtp_headers_t *headers, const char *key);

  void http_headers_add_header(evhtp_headers_t *headers,
                               evhtp_header_t *header);
  evhtp_header_t *http_header_new(const char *key, const char *val, char kalloc,
                                  char valloc);
  const char *http_kv_find(evhtp_kvs_t *kvs, const char *key);
  evhtp_kv_t *http_kvs_find_kv(evhtp_kvs_t *kvs, const char *key);

  void http_send_reply(evhtp_request_t *request, evhtp_res code);
  void http_send_reply_start(evhtp_request_t *request, evhtp_res code);
  void http_send_reply_body(evhtp_request_t *request, evbuf_t *buf);
  void http_send_reply_end(evhtp_request_t *request);

  // Libevent wrappers
  size_t evbuffer_get_length(const struct evbuffer *buf);
};

#endif
