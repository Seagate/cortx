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

#pragma once

#ifndef __S3_SERVER_S3_URI_H__
#define __S3_SERVER_S3_URI_H__

#include <memory>
#include <string>

#include "s3_common.h"
#include "s3_request_object.h"

enum class S3UriType { path_style, virtual_host_style, unsupported };

class S3URI {
 protected:
  std::shared_ptr<S3RequestObject> request;

  S3OperationCode operation_code;
  S3ApiType s3_api_type;

  std::string bucket_name;
  std::string object_name;
  std::string request_id;

 private:
  void setup_operation_code();

 public:
  explicit S3URI(std::shared_ptr<S3RequestObject> req);
  virtual ~S3URI() {}

  virtual S3ApiType get_s3_api_type();

  virtual std::string& get_bucket_name();
  virtual std::string& get_object_name();  // Object key

  virtual S3OperationCode get_operation_code();
};

class S3PathStyleURI : public S3URI {
 public:
  explicit S3PathStyleURI(std::shared_ptr<S3RequestObject> req);
};

class S3VirtualHostStyleURI : public S3URI {
  std::string host_header;

  // helper
  void setup_bucket_name();

 public:
  explicit S3VirtualHostStyleURI(std::shared_ptr<S3RequestObject> req);
};

class S3UriFactory {
 public:
  virtual ~S3UriFactory() {}

  virtual S3URI* create_uri_object(S3UriType uri_type,
                                   std::shared_ptr<S3RequestObject> request);
};

#endif
