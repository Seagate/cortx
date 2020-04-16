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
 * Original creation date: 1-Oct-2019
 */

#pragma once

#ifndef __S3_SERVER_MERO_URI_H__
#define __S3_SERVER_MERO_URI_H__

#include <memory>
#include <string>

#include "s3_common.h"
#include "mero_request_object.h"

enum class MeroUriType {
  path_style,
  unsupported
};

class MeroURI {
 protected:
  std::shared_ptr<MeroRequestObject> request;

  std::string key_name;
  std::string index_id_lo;
  std::string index_id_hi;
  std::string object_oid_lo;
  std::string object_oid_hi;

  MeroOperationCode operation_code;
  MeroApiType mero_api_type;

  std::string request_id;

 private:
  void setup_operation_code();

 public:
  explicit MeroURI(std::shared_ptr<MeroRequestObject> req);
  virtual ~MeroURI() {}

  virtual MeroApiType get_mero_api_type();
  virtual std::string& get_key_name();
  virtual std::string& get_object_oid_lo();
  virtual std::string& get_object_oid_hi();
  virtual std::string& get_index_id_lo();
  virtual std::string& get_index_id_hi();
  virtual MeroOperationCode get_operation_code();
};

class MeroPathStyleURI : public MeroURI {
 public:
  explicit MeroPathStyleURI(std::shared_ptr<MeroRequestObject> req);
};

class MeroUriFactory {
 public:
  virtual ~MeroUriFactory() {}

  virtual MeroURI* create_uri_object(
      MeroUriType uri_type, std::shared_ptr<MeroRequestObject> request);
};

#endif
