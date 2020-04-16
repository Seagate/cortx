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
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 1-June-2019
 */

#include "mero_api_handler.h"
#include "s3_log.h"

std::shared_ptr<MeroAPIHandler> MeroAPIHandlerFactory::create_api_handler(
    MeroApiType api_type, std::shared_ptr<MeroRequestObject> request,
    MeroOperationCode op_code) {
  std::shared_ptr<MeroAPIHandler> handler;
  std::string request_id = request->get_request_id();
  request->set_operation_code(op_code);
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  switch (api_type) {
    case MeroApiType::index:
      s3_log(S3_LOG_DEBUG, request_id, "api_type = MeroApiType::index\n");
      handler = std::make_shared<MeroIndexAPIHandler>(request, op_code);
      break;
    case MeroApiType::keyval:
      s3_log(S3_LOG_DEBUG, request_id, "api_type = MeroApiType::keyval\n");
      handler = std::make_shared<MeroKeyValueAPIHandler>(request, op_code);
      break;
    case MeroApiType::object:
      s3_log(S3_LOG_DEBUG, request_id, "api_type = MeroApiType::object\n");
      handler = std::make_shared<MeroObjectAPIHandler>(request, op_code);
      break;
    case MeroApiType::faultinjection:
      s3_log(S3_LOG_DEBUG, request_id,
             "api_type = MeroApiType::faultinjection\n");
      // TODO
      // handler = std::make_shared<MeroFaultinjectionAPIHandler>(request,
      // op_code);
      break;
    default:
      break;
  };
  if (handler) {
    s3_log(S3_LOG_DEBUG, request_id, "HTTP Action is %s\n",
           request->get_http_verb_str(request->http_verb()));
    handler->create_action();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return handler;
}
