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
 * Original author:  Swapnil Belapurkar <swapnil.belapurkar@seagate.com>
 * Original creation date: 05-Apr-2017
 */

#pragma once

#ifndef __UT_MOCK_SOCKET_INTERFACE_H__
#define __UT_MOCK_SOCKET_INTERFACE_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "s3_stats.h"

/* Mock class compliant with Google Mock framework.
 * This class mocks SocketInterface defined in server/socket_wrapper.h.
*/
class MockSocketInterface : public SocketInterface {
 public:
  MOCK_METHOD3(socket, int(int domain, int type, int protocol));
  MOCK_METHOD3(fcntl, int(int fd, int cmd, int flags));
  MOCK_METHOD2(inet_aton, int(const char *cp, struct in_addr *inp));
  MOCK_METHOD1(close, int(int fd));
  MOCK_METHOD6(sendto,
               ssize_t(int sockfd, const void *buf, size_t len, int flags,
                       const struct sockaddr *dest_addr, socklen_t addrlen));
};

#endif
