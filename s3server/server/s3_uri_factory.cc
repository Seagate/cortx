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
 * Original creation date: 10-Nov-2015
 */

#include "s3_uri.h"
#include "s3_log.h"

S3URI* S3UriFactory::create_uri_object(
    S3UriType uri_type, std::shared_ptr<S3RequestObject> request) {
  s3_log(S3_LOG_DEBUG, request->get_request_id(), "Entering\n");

  switch (uri_type) {
    case S3UriType::path_style:
      s3_log(S3_LOG_DEBUG, request->get_request_id(),
             "Creating path_style object\n");
      return new S3PathStyleURI(request);
    case S3UriType::virtual_host_style:
      s3_log(S3_LOG_DEBUG, request->get_request_id(),
             "Creating virtual_host_style object\n");
      return new S3VirtualHostStyleURI(request);
    default:
      break;
  };
  s3_log(S3_LOG_DEBUG, request->get_request_id(), "Exiting\n");
  return NULL;
}
