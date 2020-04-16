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
 * Original author: Evgeniy Brazhnikov
 * Original creation date: 7-Oct-2019
 */

#include <iostream>
#include <iomanip>
#include <memory>

/* For sockaddr_in */
#include <netinet/in.h>
/* For socket functions */
#include <sys/socket.h>
/* For fcntl */
#include <fcntl.h>

#include <arpa/inet.h>

#include <event2/event.h>
#include <event2/listener.h>
#include <gflags/gflags.h>

#include <unistd.h>
#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define INTERVAL_SECONDS 10

DEFINE_int32(port, 60001, "TCP port for listerning connections");
DEFINE_bool(send, false, "Send data to client");

static unsigned long long g_counter;

static void fn_timer(evutil_socket_t fd, short events, void *arg)
{
  auto mbs = g_counter / (1024 * 1024);
  g_counter = 0;

  if (!mbs) return;

  std::cout << "Total " << mbs << " MBs ( "
            << std::setprecision(1) << std::fixed
            << (double)mbs / INTERVAL_SECONDS << " MB/sec )\n";
}

static void remove_event(struct event *ev)
{
  event_del(ev);
  event_free(ev);
}

static char buf[4096];

static void fn_read(evutil_socket_t fd, short events, void *arg)
{
  int result = 0;

  // Limit this to only read a portion at a time, not read out everything,
  // otherwise it blocks main loop forever.
  for (int size = 0; size < 16 * 1024 * 1022; size += result)
  {
    result = recv(fd, buf, sizeof buf, 0);

    if (result > 0)
      g_counter += result;
    else
      break;
  }
  if (0 == result || EAGAIN != errno)
  {
    remove_event((struct event*)arg);
  }
}

static void fn_write(evutil_socket_t fd, short events, void *arg)
{
  int result = 0;

  for (;;)
  {
    result = send(fd, buf, sizeof buf, 0);

    if (result > 0) g_counter += result;
    else break;
  }
  if (0 == result || EAGAIN != errno)
  {
    remove_event((struct event*)arg);
  }
}

static void fn_accept(
    struct evconnlistener *listener,
    evutil_socket_t fd,
    struct sockaddr *addr,
    int sockaddr_len,
    void *arg)
{
  struct sockaddr_in *sin = (struct sockaddr_in*)addr;

  std::cout << "Connection from " << inet_ntoa(sin->sin_addr)
            << ':' << ntohs(sin->sin_port) << '\n';

  struct event_base *ev_base = evconnlistener_get_base(listener);
  evutil_make_socket_nonblocking(fd);

  if(FLAGS_send)
  {
    struct event *w_event = event_new(
        ev_base, fd, EV_WRITE|EV_PERSIST,
        fn_write, event_self_cbarg()
        );
    event_add(w_event, nullptr);
  }
  struct event *r_event = event_new(
      ev_base, fd, EV_READ|EV_PERSIST,
      fn_read, event_self_cbarg()
      );
  event_add(r_event, NULL);
}

int main(int argc, char **argv)
{
  gflags::ParseCommandLineFlags(&argc, &argv, false);

  if (FLAGS_port < 1024 || FLAGS_port > 65535)
  {
    std::cout << "Illegal port number (" << FLAGS_port << ")\n";
    return 1;
  }
  struct event_base *ev_base = event_base_new();

  if (!ev_base)
  {
    std::cout << "event_base_new()\n";
    return 1;
  }
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof sin);
  sin.sin_family = AF_INET;
  sin.sin_port = htons(FLAGS_port);

  struct evconnlistener *listener = evconnlistener_new_bind(
      ev_base, fn_accept, nullptr,
      LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
      -1, (struct sockaddr *)&sin, sizeof sin
      );
  if (!listener)
  {
    std::cout << "evconnlistener()\n";
    return 1;
  }
  std::cout << "Socket is opened and ready for "
            << (FLAGS_send ? "SEND" : "RECEIVE")
            << '\n';

  struct event *ev_timer = event_new(
      ev_base, -1, EV_PERSIST, fn_timer,
      event_self_cbarg());

  struct timeval ten_sec = {INTERVAL_SECONDS};
  event_add(ev_timer, &ten_sec);

  event_base_dispatch(ev_base);

  evconnlistener_free(listener);
  event_base_free(ev_base);

  gflags::ShutDownCommandLineFlags();

  return 0;
}
