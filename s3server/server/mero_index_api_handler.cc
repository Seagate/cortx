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
#include "mero_delete_index_action.h"
#include "mero_head_index_action.h"
#include "mero_kvs_listing_action.h"
#include "s3_log.h"
#include "s3_stats.h"

void MeroIndexAPIHandler::create_action() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  s3_log(S3_LOG_DEBUG, request_id, "Operation code = %d\n", operation_code);

  switch (operation_code) {
    case MeroOperationCode::none:
      // Perform operation on Object.
      switch (request->http_verb()) {
        case S3HttpVerb::GET:
          action = std::make_shared<MeroKVSListingAction>(request);
          s3_stats_inc("mero_http_kvs_list_count");
          break;
        case S3HttpVerb::DELETE:
          if (!request->get_index_id_lo().empty() ||
              !request->get_index_id_hi().empty()) {
            // Index id must be present in request /indexes/123-456
            action = std::make_shared<MeroDeleteIndexAction>(request);
            s3_stats_inc("mero_http_index_delete_count");
          }  // else we dont support delete all indexes DEL /indexes/
          break;
        case S3HttpVerb::HEAD:
          if (!request->get_index_id_lo().empty() ||
              !request->get_index_id_hi().empty()) {
            action = std::make_shared<MeroHeadIndexAction>(request);
            s3_stats_inc("mero_http_index_head_count");
          }
          break;
        default:
          // should never be here.
          // Unsupported APIs
          return;
      };
      break;
    default:
      // should never be here.
      return;
  };  // switch operation_code
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

