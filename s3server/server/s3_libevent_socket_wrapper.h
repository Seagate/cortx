/*
 * COPYRIGHT 2019 SEAGATE LLC
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
 * Original author:  Dmitrii Surnin <dmitrii.surnin@seagate.com>
 * Original creation date: 10-June-2019
 */

#pragma once

#ifndef __S3_LIBEVENT_SOCKET_WRAPPER_H__
#define __S3_LIBEVENT_SOCKET_WRAPPER_H__

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/dns.h>
#include <event2/util.h>

class S3LibeventSocketWrapper {
 public:
  virtual ~S3LibeventSocketWrapper() {}

  virtual struct bufferevent *bufferevent_socket_new(struct event_base *base,
                                                     evutil_socket_t fd,
                                                     int options) {
    return ::bufferevent_socket_new(base, fd, options);
  }

  virtual void bufferevent_setcb(struct bufferevent *bufev,
                                 bufferevent_data_cb readcb,
                                 bufferevent_data_cb writecb,
                                 bufferevent_event_cb eventcb, void *cbarg) {
    ::bufferevent_setcb(bufev, readcb, writecb, eventcb, cbarg);
  }

  virtual int bufferevent_disable(struct bufferevent *bufev, short event) {
    return ::bufferevent_disable(bufev, event);
  }

  virtual int bufferevent_enable(struct bufferevent *bufev, short event) {
    return ::bufferevent_enable(bufev, event);
  }

  virtual int evbuffer_add(struct evbuffer *buf, const void *data,
                           size_t datlen) {
    return ::evbuffer_add(buf, data, datlen);
  }

  virtual int bufferevent_socket_connect_hostname(struct bufferevent *bufev,
                                                  struct evdns_base *evdns_base,
                                                  int family,
                                                  const char *hostname,
                                                  int port) {
    return ::bufferevent_socket_connect_hostname(bufev, evdns_base, family,
                                                 hostname, port);
  }

  virtual void bufferevent_free(struct bufferevent *bufev) {
    ::bufferevent_free(bufev);
  }

  virtual struct evdns_base *evdns_base_new(struct event_base *event_base,
                                            int initialize_nameservers) {
    return ::evdns_base_new(event_base, initialize_nameservers);
  }

  virtual void evdns_base_free(struct evdns_base *base, int fail_requests) {
    ::evdns_base_free(base, fail_requests);
  }
};

#endif
