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
 * Original creation date: 26-Dec-2019
 */

#pragma once

#ifndef __S3_SERVER_ATEXIT_H__

#include <functional>

class AtExit {
 private:
  std::function<void()> handler;
  bool enabled = true;

 public:
  AtExit(std::function<void()> handler_) : handler(handler_) {}
  ~AtExit() { call_now(); }
  void cancel() { enabled = false; }
  void reenable() { enabled = true; }
  void call_now() {
    if (enabled && handler) handler();
    enabled = false;
  }
};

#endif
