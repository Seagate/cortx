/*
 * COPYRIGHT 2018 SEAGATE LLC
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
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 25-July-2018
 */

#pragma once

#ifndef __S3_SERVER_S3_HEAD_SERVICE_ACTION_H__
#define __S3_SERVER_S3_HEAD_SERVICE_ACTION_H__

#include <memory>
#include "s3_action_base.h"

class S3HeadServiceAction : public S3Action {
 public:
  S3HeadServiceAction(std::shared_ptr<S3RequestObject> req);
  void setup_steps();

  void send_response_to_s3_client();

  // google unit tests
  FRIEND_TEST(S3HeadServiceActionTest, SendResponseToClientServiceUnavailable);
};

#endif
