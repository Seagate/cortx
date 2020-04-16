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
 * Original author:  Ivan Tishchenko     <ivan.tishchenko@seagate.com>
 * Original creation date: 24-Dec-2019
 */

#pragma once

#ifndef __S3_SERVER_EVENT_UTILS_H__
#define __S3_SERVER_EVENT_UTILS_H__

#include <memory>

#include <gtest/gtest_prod.h>

#include "s3_common.h"
#include "event_wrapper.h"
#include "evhtp_wrapper.h"

// Allows to create recurring events using libevent. I.e. to schedule certain
// action to happen regularly at specified interval.
//
// Usage:
//
// // Define:
// class MyClass : public RecurringEventBase {
//  public:
//   virtual void action_callback(void) noexcept {
//     // do my stuff here
//   }
// };
//
// // Instantiate & Register:
// MyClass evt(std::make_shared<EventWrapper>(),
//             S3Option::get_instance()->get_eventbase());
// struct timeval tv;
// tv.tv_sec = 1;
// tv.tv_usec = 0;
// evt.add_evtimer(tv);
//
// Now "my stuff" will be issued every 1 second.
// Note that destructor deregisters the event, so MyClass instance must be
// preserved for the entire duration of the period when action recurrence is
// needed.
class RecurringEventBase {
 private:
  std::shared_ptr<EventInterface> event_obj;
  struct event *evt = nullptr;
  evbase_t *evbase = nullptr;

 protected:
  static void libevent_callback(evutil_socket_t fd, short event, void *arg);

 public:
  RecurringEventBase(std::shared_ptr<EventInterface> event_obj_ptr,
                     evbase_t *evbase_ = nullptr)
      : event_obj(std::move(event_obj_ptr)), evbase(evbase_) {}
  virtual ~RecurringEventBase() { del_evtimer(); }

  // Registers the event with libevent, recurrence specified by 'tv'.
  int add_evtimer(struct timeval &tv);
  // Can be used to deregister an event (can be re-registered later with same
  // or different recurrence interval, using add_evtimer);
  void del_evtimer();
  // This callback will be called on every occurrence of the event.
  virtual void action_callback(void) noexcept = 0;

  FRIEND_TEST(RecurringEventBaseTest, TestEventCallback);
};

#endif
