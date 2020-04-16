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

#include "s3_audit_info_logger_rsyslog_tcp.h"
#include "s3_log.h"
#include <netinet/tcp.h>
#include <errno.h>
#include <stdexcept>

void S3AuditInfoLoggerRsyslogTcp::eventcb(struct bufferevent *bev, short events,
                                          void *ptr) {
  s3_log(S3_LOG_DEBUG, "", "Entering event %d ptr %p\n", (int)events, ptr);

  if (ptr == nullptr) {
    s3_log(S3_LOG_FATAL, "", "Exiting. Unexpected event param\n");
    return;
  }

  S3AuditInfoLoggerRsyslogTcp *self = (S3AuditInfoLoggerRsyslogTcp *)ptr;
  if (self->connecting) {
    s3_log(S3_LOG_DEBUG, "", "events on connecting %d\n", (int)events);
    if (events & BEV_EVENT_CONNECTED) {
      self->connecting = false;
      self->curr_retry = 0;
      evutil_socket_t fd = bufferevent_getfd(bev);
      int one = 1;
      if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) != 0) {
        s3_log(S3_LOG_FATAL, "", "Exiting. setsockopt errno %d\n", errno);
        self->curr_retry = self->max_retry + 1;
      }
    }
  } else {
    s3_log(S3_LOG_DEBUG, "", "events on processing %d\n", (int)events);
    if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF | BEV_EVENT_TIMEOUT)) {
      s3_log(S3_LOG_ERROR, "", "Operation error. Event %d Retries %d\n",
             (int)events, self->curr_retry);
      self->connect();
    }
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3AuditInfoLoggerRsyslogTcp::connect() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");

  ++curr_retry;
  if (curr_retry > max_retry) {
    s3_log(S3_LOG_FATAL, "", "Retry count %d exceeds %d. Exiting\n", curr_retry,
           max_retry);
    return;
  }

  connecting = (0 == socket_api->bufferevent_socket_connect_hostname(
                         bev, base_dns, AF_UNSPEC, host.c_str(), port));
  if (!connecting) {
    s3_log(S3_LOG_FATAL, "", "Cannot connect to %s:%d\n", host.c_str(), port);
    curr_retry = max_retry + 1;
  }

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

S3AuditInfoLoggerRsyslogTcp::S3AuditInfoLoggerRsyslogTcp(
    struct event_base *base, std::string dest_host, int dest_port,
    int retry_cnt, std::string message_id, int facility, int severity,
    std::string hostname, std::string app, std::string app_procid,
    S3LibeventSocketWrapper *sock_api)
    : host(dest_host),
      port(dest_port),
      max_retry(retry_cnt),
      curr_retry(0),
      socket_api(nullptr),
      managed_api(false),
      bev(nullptr),
      base_event(base),
      base_dns(nullptr),
      connecting(false) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");

  if (base == nullptr) {
    s3_log(S3_LOG_ERROR, "", "Exiting. Base event is NULL\n");
    throw std::invalid_argument("Base event is NULL");
  }

  managed_api = (sock_api == nullptr);
  if (sock_api == nullptr) {
    socket_api = new S3LibeventSocketWrapper();
  } else {
    socket_api = sock_api;
  }

  if (socket_api == nullptr) {
    s3_log(S3_LOG_ERROR, "", "Exiting. Socket API is NULL\n");
    throw std::runtime_error("Socket API is NULL");
  }

  bev =
      socket_api->bufferevent_socket_new(base_event, -1, BEV_OPT_CLOSE_ON_FREE);
  if (bev == nullptr) {
    s3_log(S3_LOG_ERROR, "", "Exiting. Cannot create socket\n");
    throw std::runtime_error("Cannot create socket");
  }

  base_dns = socket_api->evdns_base_new(base_event, 1);
  if (base_dns == nullptr) {
    s3_log(S3_LOG_ERROR, "", "Exiting. Cannot create DNS rslv\n");
    throw std::runtime_error("Cannot create DNS rslv");
  }

  socket_api->bufferevent_setcb(
      bev, NULL, NULL, &S3AuditInfoLoggerRsyslogTcp::eventcb, (void *)this);
  socket_api->bufferevent_enable(bev, EV_WRITE);
  socket_api->bufferevent_disable(bev, EV_READ);

  // rsyslog rfc5424 messge format
  // <PRI>VERSION TIMESTAMP HOSTNAME APP-NAME PROCID MSGID SD MSG
  message_template = "<" + std::to_string(facility * 8 + severity) + ">1 " +
                     "- " + hostname + " " + app + " " + app_procid + " " +
                     message_id + " " + "-";

  connect();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

S3AuditInfoLoggerRsyslogTcp::~S3AuditInfoLoggerRsyslogTcp() {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  if (base_dns != nullptr) {
    socket_api->evdns_base_free(base_dns, 0);
    base_dns = nullptr;
  }
  if (bev != nullptr) {
    socket_api->bufferevent_free(bev);
    bev = nullptr;
  }
  if (managed_api) {
    delete socket_api;
  }
  socket_api = nullptr;
  connecting = false;
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

int S3AuditInfoLoggerRsyslogTcp::save_msg(
    std::string const &cur_request_id, std::string const &audit_logging_msg) {
  s3_log(S3_LOG_DEBUG, cur_request_id, "Entering\n");

  if (curr_retry > max_retry) {
    s3_log(S3_LOG_ERROR, cur_request_id, "Exiting. Retries %d exceeds %d\n",
           curr_retry, max_retry);
    return -1;
  }

  struct evbuffer *output = bufferevent_get_output(bev);
  std::string msg_to_snd = message_template + " " + audit_logging_msg;
  int ret =
      socket_api->evbuffer_add(output, msg_to_snd.c_str(), msg_to_snd.length());
  if (ret != 0) {
    s3_log(S3_LOG_FATAL, cur_request_id, "Exiting. Cannot write to buffer\n");
    curr_retry = max_retry + 1;
    return -1;
  }

  s3_log(S3_LOG_DEBUG, cur_request_id, "Exiting\n");
  return 0;
}
