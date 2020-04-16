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
 * Author:  Rajesh Nambiar <rajesh.nambiar@seagate.com>
 * Author:  Kaustubh Deorukhkar <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 11-Feb-2019
 */

#pragma once

#ifndef __S3_UT_MOCK_EVENT_WRAPPER_H__
#define __S3_UT_MOCK_EVENT_WRAPPER_H__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "event_wrapper.h"

class MockEventWrapper : public EventInterface {
 public:
  MockEventWrapper() {}
  MOCK_METHOD5(new_event,
               struct event *(struct event_base *base, evutil_socket_t fd,
                              short events,
                              void (*cb)(evutil_socket_t, short, void *),
                              void *arg));
  MOCK_METHOD1(del_event, void(struct event *s3_client_event));
  MOCK_METHOD1(free_event, void(struct event *s3_client_event));
  MOCK_METHOD2(add_event,
               void(struct event *s3_client_event, struct timeval *tv));
  MOCK_METHOD3(pending_event, bool(struct event *s3_client_event, short event,
                                   struct timeval *tv));
};

#endif
