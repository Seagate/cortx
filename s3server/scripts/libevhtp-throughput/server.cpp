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
 * Original author:  Eugeniy Brazhnikov   <brazhnikov.evgeniy@seagate.com>
 * Original creation date: 15-Oct-2019
 */

/*
 * The program uses 'libevhtp' engine for treating HTTP requests.
 * It accepts HTTP GET request like http://<host>:60080/<size> and
 * responds with data of 'size' bytes (of zero).
 * Also the program accepts HTTP PUT requests, discards any data
 * received and returns HTTP 201.
 * The program calculates bytes both sent or received and output
 * the amount every second (if >= 1M).
 */

#include <cstdlib>
#include <iostream>
#include <memory>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <gflags/gflags.h>

#include <event2/event.h>
#include <event2/thread.h>
#include <evhtp.h>

#define POOL_BUFFER_SIZE          16384
#define POOL_INITIAL_SIZE     419430400
#define POOL_EXPANDABLE_SIZE  104857600
#define POOL_MAX_THRESHOLD   1048576000
#define MAX_READ_SIZE             16384
#define CREATE_ALIGNED_MEMORY 0x0001

DEFINE_int32(port, 60080, "TCP port for listerning connections");
DEFINE_bool(pool, false, "Use s3server-customized memory pool");

static unsigned long long g_counter;

static void fn_timer(evutil_socket_t fd, short events, void *arg)
{
  auto mbs = g_counter / (1024 * 1024);

  if (mbs)
  {
    std::cout << mbs << " MB/sec" << std::endl;
  }
  g_counter = 0;
}

struct RequestState
{
  const char *p_buf;
  size_t bytes_sent;
  size_t content_length;
};

const char dummy_reply[] =
    "<html><head>"
    "<title>Test server upon &apos;libevhtp&apos; engine</title>"
    "</head><body>"
    "GET request: &lt;host&gt;&colon;60080&sol;&lt;size&gt;<br/>"
    "Examples of size: 4090, 102K, 5M, 1G etc.<br/>"
    "The program accepts any valid PUT requests."
    "</body></html>";

#define MAX_SEND_SIZE 16384

static char zero_buf[MAX_SEND_SIZE];

static void on_request_cb(
    evhtp_request_t *p_req,
    void *arg
    ) noexcept
{
  //std::cout << "on_request_cb\n";

  switch (p_req->method)
  {
    case htp_method_GET:
      break;
    case htp_method_PUT:
      evhtp_headers_add_header(
          p_req->headers_out,
          evhtp_header_new("Content-Length", "0", 0, 0)
          );
      evhtp_send_reply(p_req, EVHTP_RES_CREATED);
      return;
    default:
      evhtp_send_reply(p_req, EVHTP_RES_METHNALLOWED);
      return;
  }
  // htp_method_GET
  RequestState *p_state = static_cast<RequestState *>(p_req->cbarg);

  p_state->p_buf = dummy_reply;
  p_state->content_length = sizeof(dummy_reply) - 1;

  const char *sz_content_type = "text/html";

  const char *sz_path = p_req->uri->path->full;
  //std::cout << "path: " << sz_path << '\n';

  if (sz_path[1])
  {
    std::string spath(sz_path + 1);
    size_t idx = 0;
    unsigned long content_length = 0;

    try
    {
      content_length = std::stoul(spath, &idx);
    }
    catch( const std::exception& ex )
    {
      std::cout << ex.what() << '\n';
    }
    catch ( ... )
    {}
    if (content_length)
    {
      if (idx < spath.length())
      {
        switch (spath[idx])
        {
          case 'k':
          case 'K':
            content_length *= 1024;
            break;
          case 'm':
          case 'M':
            content_length *= 1024 * 1024;
            break;
          case 'g':
          case 'G':
            content_length *= 1024 * 1024 * 1024;
        }
      }
      p_state->p_buf = zero_buf;
      p_state->content_length = content_length;

      sz_content_type = "application/octet-stream";
    }
  }
  evhtp_header_t *p_header = evhtp_header_new(
      "Content-Type",
      sz_content_type,
      0, 0);
  evhtp_headers_add_header(p_req->headers_out, p_header);

  p_header = evhtp_header_new(
      "Content-Length",
      std::to_string(p_state->content_length).c_str(),
      0, 1);
  evhtp_headers_add_header(p_req->headers_out, p_header);

  evhtp_send_reply_start(p_req, EVHTP_RES_200);
}

static void on_request_error(
    evhtp_request_t *ev_req,
    evhtp_error_flags errtype,
    void *arg
    ) noexcept
{
  std::cout << "on_request_error() " << (unsigned)errtype << '\n';
}

static evhtp_res on_client_request_fini(
    evhtp_request_t *p_req,
    void *arg
    ) noexcept
{
  //std::cout << "on_client_request_fini()\n";

  if (p_req && p_req->cbarg)
  {
    free(p_req->cbarg);
    p_req->cbarg = NULL;
  }
  return EVHTP_RES_OK;
}

static int treat_header(
    evhtp_kv_t *p_kv,
    void * arg
    ) noexcept
{
  std::cout << p_kv->key << ": " << p_kv->val << '\n';
  RequestState *p_state = static_cast<RequestState *>(arg);

  if ( !strcasecmp(p_kv->key, "Content-Length"))
  {
    std::string s_val(p_kv->val, p_kv->vlen);
    p_state->content_length = std::stoul(s_val);
  }
  return 0;
}

static evhtp_res on_headers_fini_cb(
    evhtp_request_t *p_req,
    evhtp_headers_t *p_hdrs,
    void *arg
    ) noexcept
{
  //std::cout << "on_headers_fini_cb()\n";
  evhtp_headers_for_each(p_hdrs, &treat_header, p_req->cbarg);

  return EVHTP_RES_OK;
}

static evhtp_res on_read_cb(
    evhtp_request_t *p_req,
    evbuf_t *ev_buf,
    void *arg
    ) noexcept
{
  RequestState *p_state = static_cast<RequestState *>(p_req->cbarg);

  const auto buf_len = evbuffer_get_length(ev_buf);
  p_state->bytes_sent += buf_len;
  g_counter += buf_len;

  //std::cout << "Received " << p_state->bytes_sent
  //    << " from " << p_state->content_length << '\n';

  evbuffer_drain(ev_buf, -1);

  return EVHTP_RES_OK;
}

static evhtp_res on_write_cb(
    evhtp_connection_t * p_conn,
    void * arg
    ) noexcept
{
  //std::cout << "on_write_cb()\n";

  evhtp_request_t *p_req = p_conn->request;

  if ( ! p_req ||
      evhtp_request_get_method(p_req) != htp_method_GET )
  {
    return EVHTP_RES_OK;
  }
  RequestState *p_state = static_cast<RequestState *>(p_req->cbarg);

  if (p_state->bytes_sent < p_state->content_length)
  {
    auto bytes_to_send = p_state->content_length - p_state->bytes_sent;

    if ( bytes_to_send > MAX_SEND_SIZE)
    {
      bytes_to_send = MAX_SEND_SIZE;
    }
    evbuf_t *p_ev_buf = evbuffer_new();

    if (p_ev_buf)
    {
      evbuffer_add_reference(
          p_ev_buf,
          p_state->p_buf, bytes_to_send,
          NULL, NULL);

      evhtp_send_reply_body(p_req, p_ev_buf);
      evbuffer_free(p_ev_buf);

      p_state->bytes_sent += bytes_to_send;
      g_counter += bytes_to_send;

      if (p_state->bytes_sent >= p_state->content_length)
      {
        evhtp_send_reply_end(p_req);
      }
    }
  }
  return EVHTP_RES_OK;
}

static evhtp_res on_headers_start_cb(
    evhtp_request_t * p_req,
    void * arg
    ) noexcept
{
  //std::cout << "on_headers_start_cb()\n";

  p_req->cbarg = calloc(1, sizeof(RequestState));

  if ( ! p_req->cbarg)
  {
    return EVHTP_RES_ERROR;
  }
  evhtp_set_hook(
      &p_req->hooks, evhtp_hook_on_error,
      (evhtp_hook)on_request_error,
      NULL);
  return EVHTP_RES_OK;
}

static evhtp_res on_conn_err_cb(
    evhtp_connection_t * p_conn,
    evhtp_error_flags errtype,
    void * arg
    ) noexcept
{
  std::cout << "on_conn_err_cb() " << (unsigned)errtype << '\n';

  return EVHTP_RES_ERROR;
}

static evhtp_res on_post_accept_cb(
    evhtp_connection_t *p_conn,
    void *arg
    ) noexcept
{
  struct sockaddr_in *p_sin = (struct sockaddr_in *)p_conn->saddr;

  std::cout << "Connection from "
      << inet_ntoa(p_sin->sin_addr) << ':'
      << ntohs(p_sin->sin_port) << '\n';

  evhtp_set_hook(
      &p_conn->hooks, evhtp_hook_on_conn_error,
      (evhtp_hook)on_conn_err_cb,
      NULL);
  evhtp_set_hook(
      &p_conn->hooks, evhtp_hook_on_headers_start,
      (evhtp_hook)on_headers_start_cb,
      NULL);
  evhtp_set_hook(
      &p_conn->hooks, evhtp_hook_on_headers,
      (evhtp_hook)on_headers_fini_cb,
      NULL);
  evhtp_set_hook(
      &p_conn->hooks, evhtp_hook_on_read,
      (evhtp_hook)on_read_cb,
      NULL);
  evhtp_set_hook(
      &p_conn->hooks, evhtp_hook_on_write,
      (evhtp_hook)on_write_cb,
      NULL);
  evhtp_set_hook(
      &p_conn->hooks, evhtp_hook_on_request_fini,
      (evhtp_hook)on_client_request_fini,
      NULL);

  return EVHTP_RES_OK;
}

int main(int argc, char **argv)
{
  gflags::SetUsageMessage("Starts libevhtp test server");
  gflags::ParseCommandLineFlags(&argc, &argv, false);

  if (FLAGS_port < 1024 || FLAGS_port > 65535)
  {
    std::cout << "Illegal port number (" << FLAGS_port << ")\n";
    return 1;
  }

  if (FLAGS_pool)
  {
    if (event_use_mempool(POOL_BUFFER_SIZE,
            POOL_INITIAL_SIZE, POOL_EXPANDABLE_SIZE,
            POOL_MAX_THRESHOLD, CREATE_ALIGNED_MEMORY))
    {
      std::cout << "event_use_mempool() failed\n";
      return 1;
    }
    event_set_max_read(MAX_READ_SIZE);
    evhtp_set_low_watermark(MAX_READ_SIZE);
  }
  evthread_use_pthreads();

  std::unique_ptr<evbase_t, decltype(&event_base_free)> sp_ev_base(
      event_base_new(),
      &event_base_free );

  if( ! sp_ev_base )
  {
    std::cout << "event_base_new() failed!\n";
    return 1;
  }
  if(evthread_make_base_notifiable(sp_ev_base.get()) < 0)
  {
    std::cout << "evthread_make_base_notifiable() failed!\n";
    return 1;
  }
  std::unique_ptr<evhtp_t, decltype(&evhtp_free)> sp_htp(
      evhtp_new(sp_ev_base.get(), NULL),
      &evhtp_free );

  if( ! sp_htp )
  {
    std::cout << "evhtp_new() failed!\n";
    return 1;
  }
  struct timeval tv = {10};
  evhtp_set_timeouts(sp_htp.get(), &tv, &tv);

  evhtp_set_parser_flags(sp_htp.get(),
    EVHTP_PARSE_QUERY_FLAG_ALLOW_NULL_VALS | EVHTP_PARSE_QUERY_FLAG_ALLOW_EMPTY_VALS
    );
  evhtp_set_post_accept_cb(sp_htp.get(), on_post_accept_cb, NULL);
  evhtp_set_gencb(sp_htp.get(), on_request_cb, NULL);

  if (evhtp_bind_socket(sp_htp.get(), "0.0.0.0", FLAGS_port, 256) < 0)
  {
    std::cout << "evhtp_bind_socket() failed!\n";
    return 1;
  }
  struct event *p_ev_timer = event_new(
    sp_ev_base.get(), -1, EV_PERSIST,
    &fn_timer, event_self_cbarg());

  struct timeval one_sec = {1};
  event_add(p_ev_timer, &one_sec);

  event_base_loop(sp_ev_base.get(), EVLOOP_NO_EXIT_ON_EMPTY);

  event_del(p_ev_timer);
  event_free(p_ev_timer);

  evhtp_unbind_socket(sp_htp.get());

  gflags::ShutDownCommandLineFlags();

  return 0;
}
