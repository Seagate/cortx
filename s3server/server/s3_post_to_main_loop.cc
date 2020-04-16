/*
 * COPYRIGHT 2015 SEAGATE LLC
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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#include "s3_post_to_main_loop.h"
#include "s3_option.h"

void S3PostToMainLoop::operator()(user_event_on_main_loop callback,
                                  const std::string &request_id) {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  struct event *ev_user = NULL;
  struct user_event_context *user_context =
      (struct user_event_context *)context;
  struct event_base *base = S3Option::get_instance()->get_eventbase();

  if (base == NULL) {
    s3_log(S3_LOG_ERROR, request_id, "ERROR: event base is NULL\n");
    return;
  }
  s3_log(S3_LOG_DEBUG, request_id, "Raise event to switch to main thread.\n");

  ev_user = event_new(base, -1, EV_WRITE | EV_READ | EV_TIMEOUT, callback,
                      (void *)(user_context));
  user_context->user_event =
      (void *)ev_user;  // remember so we can free this event.
  event_active(ev_user, EV_READ | EV_WRITE | EV_TIMEOUT, 1);
  s3_log(S3_LOG_DEBUG, request_id, "Exiting\n");
}
