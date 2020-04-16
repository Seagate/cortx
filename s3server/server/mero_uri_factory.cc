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
 * Original creation date: 1-June-2010
 */

#include "mero_uri.h"
#include "s3_log.h"

MeroURI* MeroUriFactory::create_uri_object(
    MeroUriType uri_type, std::shared_ptr<MeroRequestObject> request) {
  s3_log(S3_LOG_DEBUG, request->get_request_id(), "Entering\n");

  switch (uri_type) {
    case MeroUriType::path_style:
      s3_log(S3_LOG_DEBUG, request->get_request_id(),
             "Creating mero path_style object\n");
      return new MeroPathStyleURI(request);
    default:
      break;
  };
  s3_log(S3_LOG_DEBUG, request->get_request_id(), "Exiting\n");
  return NULL;
}
