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
 * Original author:  Prashanth vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 16-June-2018
 */

#include "s3_action_base.h"
#include "s3_api_handler.h"
#include "s3_account_delete_metadata_action.h"

void S3ManagementAPIHandler::create_action() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering");
  s3_log(S3_LOG_DEBUG, request_id, "Action operation code = %d\n",
         operation_code);
  switch (operation_code) {
    case S3OperationCode::none:
      // Perform operation on Service.
      switch (request->http_verb()) {
        case S3HttpVerb::DELETE:
          action = std::make_shared<S3AccountDeleteMetadataAction>(request);
          s3_log(S3_LOG_DEBUG, request_id, "S3AccountDeleteMetadataAction");
          break;
        default:
          // should never be here.
          return;
      };
      break;
    default:
      // should never be here.
      return;
  };  // switch operation_code
  s3_log(S3_LOG_DEBUG, request_id, "Exiting");
}
