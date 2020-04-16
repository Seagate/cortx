/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original author:  Vinayak Kale <vinayak.kale@seagate.com>
 * Original creation date: 30-March-2017
 */

#pragma once

#ifndef __S3_SERVER_SOCKET_WRAPPER_H__
#define __S3_SERVER_SOCKET_WRAPPER_H__

#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// A wrapper class for socket system calls so that we are able to mock those
// syscalls in tests. For Prod (non-test) this will just forward the calls.
class SocketInterface {
 public:
  virtual int socket(int domain, int type, int protocol) = 0;
  virtual int fcntl(int fd, int cmd, int flags) = 0;
  virtual int inet_aton(const char *cp, struct in_addr *inp) = 0;
  virtual int close(int fd) = 0;
  virtual ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                         const struct sockaddr *dest_addr,
                         socklen_t addrlen) = 0;
  virtual ~SocketInterface(){};
};

class SocketWrapper : public SocketInterface {
 public:
  int socket(int domain, int type, int protocol) {
    return ::socket(domain, type, protocol);
  }

  int fcntl(int fd, int cmd, int flags) { return ::fcntl(fd, cmd, flags); }

  int inet_aton(const char *cp, struct in_addr *inp) {
    return ::inet_aton(cp, inp);
  }

  int close(int fd) { return ::close(fd); }

  ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen) {
    return ::sendto(sockfd, buf, len, flags, dest_addr, addrlen);
  }
};

#endif  // __S3_SERVER_SOCKET_WRAPPER_H__
