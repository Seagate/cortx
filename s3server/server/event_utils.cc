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

#include "event_utils.h"
#include "s3_log.h"

void RecurringEventBase::libevent_callback(evutil_socket_t fd, short event,
                                           void *arg) {
  s3_log(S3_LOG_DEBUG, "", "Entering");
  RecurringEventBase *evt_base = static_cast<RecurringEventBase *>(arg);
  if (evt_base) {
    evt_base->action_callback();
  }
}

int RecurringEventBase::add_evtimer(struct timeval &tv) {
  s3_log(S3_LOG_DEBUG, "", "Entering with sec=%" PRIu32 " usec=%" PRIu32,
         (uint32_t)tv.tv_sec, (uint32_t)tv.tv_usec);
  if (!event_obj) {
    return -EINVAL;
  }
  evbase_t *base = evbase;
  if (!base) {
    return -EINVAL;
  }
  del_evtimer();  // clean up first, in case it was already added
  evt = event_obj->new_event(
      base, -1, EV_PERSIST, libevent_callback,
      static_cast<void *>(static_cast<RecurringEventBase *>(this)));
  if (!evt) {
    return -ENOMEM;
  }
  event_obj->add_event(evt, &tv);
  return 0;
}

void RecurringEventBase::del_evtimer() {
  s3_log(S3_LOG_DEBUG, "", "Entering");
  if (evt && event_obj) {
    event_obj->del_event(evt);
    event_obj->free_event(evt);
    evt = nullptr;
  }
}
