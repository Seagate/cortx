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
 * Original creation date: 22-Oct-2019
 */

/*
 * This program complements the 'server'.
 * It can send GET and PUT requests only.
 * GET request should have form http://<host>:60080/<size>
 * The server will return a correct HTTP responce
 * with 'size' bytes of zero as body.
 * The program can also send PUT requests
 * with a body of specified amount of zero-bytes.
 * The program uses 'libcurl' engine and requires 'libcurl-devel'
 * package for building and 'curl' package for running.
 */

#include <cstring>
#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <exception>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <gflags/gflags.h>

#include <curl/curl.h>

const char sz_usage[] =
  "Usage: client [-put] [-port N] <server_IP> <size>\n"
  "'size' is a decimal number with opt. suffix: 'k', 'm' or 'g'\n";

DEFINE_bool(put, false, "Use PUT instead of GET");
DEFINE_int32(port, 60080, "TCP port for listerning connections");

const char *sz_ip;
static sockaddr_in sin;
static unsigned long file_size;
static unsigned long sent;

static size_t read_callback(
    char *pb,
    size_t size,
    size_t nitems,
    void *p_user_data)
{
  auto len = size * nitems;
  auto rest = file_size - sent;

  if ( len > rest )
  {
    len = rest;
  }
  if ( len )
  {
    memset(pb, 0, len);
    sent += len;
  }
  return len;
}

static size_t write_callback(
    char *ptr,
    size_t size,
    size_t nmemb,
    void *p_userdata)
{
  return size * nmemb;
}

static int parse_args(int argc, char **argv)
{
  if (argc < 3 || argc > 4)
  {
    return 1;
  }
  int idx = 1;
  char *sz_arg = argv[idx];

  if ( ! inet_aton(sz_arg, &sin.sin_addr))
  {
    return 1;
  }
  sz_ip = sz_arg;

  if (argc < ++idx)
  {
    return 1;
  }
  sz_arg = argv[idx];

  std::string s_arg(sz_arg);
  size_t suffix_offset = 0;

  try
  {
    file_size = std::stoul(s_arg, &suffix_offset);
  }
  catch( const std::exception& ex )
  {
    std::cout << ex.what();
    return 1;
  }
  catch ( ... )
  {
    std::cout << "Unknown exception\n";
    return 1;
  }
  if ( suffix_offset + 1 == s_arg.length())
  {
    switch ( s_arg[ suffix_offset ] )
    {
      case 'k':
      case 'K':
        file_size *= 1024;
        break;
      case 'm':
      case 'M':
        file_size *= 1024 * 1024;
        break;
      case 'g':
      case 'G':
        file_size *= 1024 * 1024 * 1024;
    }
  }
  else if ( suffix_offset != s_arg.length())
  {
    return 1;
  }
  return 0;
}

static std::string build_url()
{
  std::ostringstream oss;

  oss << "http://" << sz_ip << ":" << FLAGS_port << "/" << file_size;

  return oss.str();
}

int main(int argc, char **argv)
{
  gflags::SetUsageMessage(sz_usage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (parse_args(argc, argv))
  {
    std::cout << sz_usage;
    return 1;
  }
  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> sp_curl(
      curl_easy_init(),
      &curl_easy_cleanup
      );
  std::string s_url = build_url();
  std::cout << "URL: " << s_url << '\n';

  struct curl_slist *p_headers_list = NULL;

  curl_easy_setopt(sp_curl.get(), CURLOPT_URL, s_url.c_str());

  if (FLAGS_put)
  {
    curl_easy_setopt(sp_curl.get(), CURLOPT_UPLOAD, 1);

    curl_easy_setopt(sp_curl.get(),
        CURLOPT_INFILESIZE_LARGE,
        (curl_off_t)file_size);

    curl_easy_setopt(sp_curl.get(),
        CURLOPT_READFUNCTION,
        &read_callback);

    p_headers_list = curl_slist_append(p_headers_list,
        "Content-Type: application/octet-stream");

    p_headers_list = curl_slist_append(
        p_headers_list, "Expect:");
  }
  else // GET
  {
    curl_easy_setopt(sp_curl.get(), CURLOPT_WRITEFUNCTION, &write_callback);
  }
  p_headers_list = curl_slist_append(
      p_headers_list, "Connection: close");

  curl_easy_setopt(sp_curl.get(), CURLOPT_HTTPHEADER, p_headers_list);

  auto ret = curl_easy_perform(sp_curl.get());

  curl_slist_free_all(p_headers_list);

  if ( CURLE_OK != ret )
  {
    std::cout << curl_easy_strerror(ret);
  }
  return 0;
}
