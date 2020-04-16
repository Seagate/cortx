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
 * Original creation date: 8-Oct-2019
 */

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <gflags/gflags.h>

DEFINE_string(ip, "127.0.0.1", "Server's IP address");
DEFINE_int32(port, 60001, "TCP port for connection");
DEFINE_bool(recv, false, "Receive data from the client");

char buff[4096];

static bool f_exit;

static void sig_handler(int s)
{
  f_exit = true;
}

int main(int argc, char **argv)
{
  gflags::ParseCommandLineFlags(&argc, &argv, false);

  struct sockaddr_in sin;
  memset(&sin, 0, sizeof sin);

  if (!inet_aton(FLAGS_ip.c_str(), &sin.sin_addr))
  {
    std::cout << "Bad IP address\n";
    return 1;
  }
  if (FLAGS_port < 1024 || FLAGS_port > 65535)
  {
    std::cout << "Illegal port number (" << FLAGS_port << ")\n";
    return 1;
  }
  sin.sin_family = AF_INET;
  sin.sin_port = htons(FLAGS_port);

  int fd = socket(AF_INET, SOCK_STREAM, 0);

  if (fd < 0)
  {
    perror("socket()");
    return 1;
  }
  if (connect(fd, (struct sockaddr*)&sin, sizeof sin) < 0)
  {
    perror("connect()");
    return 1;
  }
  signal(SIGINT, sig_handler);

  unsigned long long total_bytes = 0;
  time_t start_time = time(nullptr);
  ssize_t bytes;

  while(!f_exit)
  {
    if (FLAGS_recv)
    {
      bytes = recv(fd, buff, sizeof buff, 0);
    }
    else
    {
      bytes = send(fd, buff, sizeof buff, 0);
    }
    if (bytes <=0 )
    {
      break;
    }
    total_bytes += bytes;
  }
  time_t elapsed = time(nullptr) - start_time;
  unsigned mbs = total_bytes / (1024 * 1024);

  std::cout << "Total " << mbs << " MBs during " << elapsed << "sec\n"
            << "Speed: " << mbs / elapsed << " MB/s\n";

  return 0;
}
