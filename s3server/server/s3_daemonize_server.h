/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original creation date: 12-April-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_DAEMONIZE_SERVER_H__
#define __S3_SERVER_S3_DAEMONIZE_SERVER_H__

#include <signal.h>
#include <string>
#include "s3_option.h"

class S3Daemonize {
  int noclose;
  std::string pidfilename;
  int write_to_pidfile();
  S3Option *option_instance;

 public:
  S3Daemonize();
  void daemonize();
  void wait_for_termination();
  int delete_pidfile();
  void register_signals();
  int get_s3daemon_chdir();
  int get_s3daemon_redirection();
};

#endif
