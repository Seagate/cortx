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
 * Original creation date: 1-Oct-2015
 */

#include "s3_abort_multipart_action.h"
#include "s3_api_handler.h"
#include "s3_delete_object_action.h"
#include "s3_get_multipart_part_action.h"
#include "s3_get_object_acl_action.h"
#include "s3_get_object_action.h"
#include "s3_head_object_action.h"
#include "s3_log.h"
#include "s3_post_complete_action.h"
#include "s3_post_multipartobject_action.h"
#include "s3_put_chunk_upload_object_action.h"
#include "s3_put_multiobject_action.h"
#include "s3_put_object_acl_action.h"
#include "s3_put_object_action.h"
#include "s3_put_object_tagging_action.h"
#include "s3_get_object_tagging_action.h"
#include "s3_delete_object_tagging_action.h"
#include "s3_stats.h"

void S3ObjectAPIHandler::create_action() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  s3_log(S3_LOG_DEBUG, request_id, "Operation code = %d\n", operation_code);

  switch (operation_code) {
    case S3OperationCode::acl:
      switch (request->http_verb()) {
        case S3HttpVerb::GET:
          request->set_action_str("GetObjectAcl");
          action = std::make_shared<S3GetObjectACLAction>(request);
          s3_stats_inc("get_object_acl_request_count");
          break;
        case S3HttpVerb::PUT:
          request->set_action_str("PutObjectAcl");
          action = std::make_shared<S3PutObjectACLAction>(request);
          s3_stats_inc("put_object_acl_request_count");
          break;
        default:
          // should never be here.
          return;
      };
      break;
    case S3OperationCode::multipart:
      switch (request->http_verb()) {
        case S3HttpVerb::POST:
          if (request->has_query_param_key("uploadid")) {
            // Complete multipart upload
            request->set_action_str("PostComplete");
            action = std::make_shared<S3PostCompleteAction>(request);
            s3_stats_inc("post_multipart_complete_request_count");
          } else {
            // Initiate Multipart
            request->set_action_str("PostMultipart");
            action = std::make_shared<S3PostMultipartObjectAction>(request);
            s3_stats_inc("post_multipart_initiate_request_count");
          }
          break;
        case S3HttpVerb::PUT:
          if (!request->get_header_value("x-amz-copy-source").empty()) {
            // Copy Object in part upload not yet supported.
            // Do nothing = unsupported API
          } else {
            // Multipart part uploads
            request->set_object_size(request->get_data_length());
            request->set_action_str("PutMultiObject");
            action = std::make_shared<S3PutMultiObjectAction>(request);
            s3_stats_inc("put_multipart_part_request_count");
          }
          break;
        case S3HttpVerb::GET:
          // Multipart part listing
          request->set_action_str("ListMultiPartUploadParts");
          action = std::make_shared<S3GetMultipartPartAction>(request);
          s3_stats_inc("get_multipart_parts_request_count");
          break;
        case S3HttpVerb::DELETE:
          // Multipart abort
          request->set_action_str("AbortMultipartUpload");
          action = std::make_shared<S3AbortMultipartAction>(request);
          s3_stats_inc("abort_multipart_request_count");
          break;
        default:
          break;
      };
      break;
    case S3OperationCode::none:
      // Perform operation on Object.
      switch (request->http_verb()) {
        case S3HttpVerb::HEAD:
          request->set_action_str("HeadObject");
          action = std::make_shared<S3HeadObjectAction>(request);
          s3_stats_inc("head_object_request_count");
          break;
        case S3HttpVerb::PUT:
          if (request->get_header_value("x-amz-content-sha256") ==
              "STREAMING-AWS4-HMAC-SHA256-PAYLOAD") {
            // chunk upload
            request->set_object_size(request->get_data_length());
            request->set_action_str("PutChunkUploadObject");
            action = std::make_shared<S3PutChunkUploadObjectAction>(request);
            s3_stats_inc("put_object_chunkupload_request_count");
          } else if (!request->get_header_value("x-amz-copy-source").empty()) {
            // Copy Object not yet supported.
            // Do nothing = unsupported API
          } else {
            // single chunk upload
            request->set_action_str("PutObject");
            request->set_object_size(request->get_data_length());
            action = std::make_shared<S3PutObjectAction>(request);
            s3_stats_inc("put_object_request_count");
          }
          break;
        case S3HttpVerb::GET:
          request->set_action_str("GetObject");
          action = std::make_shared<S3GetObjectAction>(request);
          s3_stats_inc("get_object_request_count");
          break;
        case S3HttpVerb::DELETE:
          request->set_action_str("DeleteObject");
          action = std::make_shared<S3DeleteObjectAction>(request);
          s3_stats_inc("delete_object_request_count");
          break;
        case S3HttpVerb::POST:
          // action = std::make_shared<S3PostObjectAction>(request);
          break;
        default:
          // should never be here.
          return;
      };
      break;
    case S3OperationCode::torrent:
      switch (request->http_verb()) {
        case S3HttpVerb::GET:
          s3_stats_inc("get_object_torrent_count");
          break;
        default:
          return;
      }
      break;
    case S3OperationCode::tagging:
      switch (request->http_verb()) {
        case S3HttpVerb::GET:
          request->set_action_str("GetObjectTagging");
          action = std::make_shared<S3GetObjectTaggingAction>(request);
          s3_stats_inc("get_object_tagging_count");
          break;
        case S3HttpVerb::PUT:
          request->set_action_str("PutObjectTagging");
          action = std::make_shared<S3PutObjectTaggingAction>(request);
          s3_stats_inc("put_object_tagging_count");
          break;
        case S3HttpVerb::DELETE:
          request->set_action_str("DeleteObjectTagging");
          action = std::make_shared<S3DeleteObjectTaggingAction>(request);
          s3_stats_inc("delete_object_tagging_count");
          break;
        default:
          return;
      }
      break;
    case S3OperationCode::selectcontent:
      switch (request->http_verb()) {
        case S3HttpVerb::POST:
          s3_stats_inc("select_object_content_count");
          break;
        default:
          return;
      }
      break;
    case S3OperationCode::restore:
      switch (request->http_verb()) {
        case S3HttpVerb::POST:
          s3_stats_inc("post_object_restore_count");
          break;
        default:
          return;
      }
      break;
    default:
      // should never be here.
      return;
  };  // switch operation_code
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
