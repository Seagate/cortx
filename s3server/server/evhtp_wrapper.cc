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

#include "evhtp_wrapper.h"

void EvhtpWrapper::http_request_pause(evhtp_request_t *request) {
  evhtp_request_pause(request);
}

void EvhtpWrapper::http_request_resume(evhtp_request_t *request) {
  evhtp_request_resume(request);
}

evhtp_proto EvhtpWrapper::http_request_get_proto(evhtp_request_t *request) {
  return evhtp_request_get_proto(request);
}

int EvhtpWrapper::http_kvs_for_each(evhtp_kvs_t *kvs, evhtp_kvs_iterator cb,
                                    void *arg) {
  return evhtp_kvs_for_each(kvs, cb, arg);
}

const char *EvhtpWrapper::http_header_find(evhtp_headers_t *headers,
                                           const char *key) {
  return evhtp_header_find(headers, key);
}

void EvhtpWrapper::http_headers_add_header(evhtp_headers_t *headers,
                                           evhtp_header_t *header) {
  evhtp_headers_add_header(headers, header);
}

evhtp_header_t *EvhtpWrapper::http_header_new(const char *key, const char *val,
                                              char kalloc, char valloc) {
  return evhtp_header_new(key, val, kalloc, valloc);
}

const char *EvhtpWrapper::http_kv_find(evhtp_kvs_t *kvs, const char *key) {
  return evhtp_kv_find(kvs, key);
}

evhtp_kv_t *EvhtpWrapper::http_kvs_find_kv(evhtp_kvs_t *kvs, const char *key) {
  return evhtp_kvs_find_kv(kvs, key);
}

void EvhtpWrapper::http_send_reply(evhtp_request_t *request, evhtp_res code) {
  evhtp_send_reply(request, code);
}

void EvhtpWrapper::http_send_reply_start(evhtp_request_t *request,
                                         evhtp_res code) {
  evhtp_send_reply_start(request, code);
}

void EvhtpWrapper::http_send_reply_body(evhtp_request_t *request,
                                        evbuf_t *buf) {
  evhtp_send_reply_body(request, buf);
}

void EvhtpWrapper::http_send_reply_end(evhtp_request_t *request) {
  evhtp_send_reply_end(request);
}

// Libevent wrappers
size_t EvhtpWrapper::evbuffer_get_length(const struct evbuffer *buf) {
  return ::evbuffer_get_length(buf);
}
