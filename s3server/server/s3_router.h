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

#ifndef __S3_SERVER_S3_ROUTER_H__
#define __S3_SERVER_S3_ROUTER_H__

#include <string>
/* libevhtp */
#include <evhtp.h>

#include <gtest/gtest_prod.h>

#include "s3_api_handler.h"
#include "mero_api_handler.h"
#include "s3_uri.h"
#include "mero_uri.h"

class Router {
 protected:
  // Some way to map URL pattern with handler.
  bool is_default_endpoint(std::string &endpoint);
  bool is_exact_valid_endpoint(std::string &endpoint);
  bool is_subdomain_match(std::string &endpoint);

 public:
  Router() = default;
  virtual ~Router() {};
  // Dispatch to registered handlers.
  virtual void dispatch(std::shared_ptr<RequestObject> request) = 0;
};

// Not thread-safe, but we are single threaded.
class S3Router : public Router {
 private:
  S3APIHandlerFactory *api_handler_factory;
  S3UriFactory *uri_factory;

 public:
  S3Router(S3APIHandlerFactory *api_creator, S3UriFactory *uri_creator);
  virtual ~S3Router();
  // Dispatch to registered handlers.
  void dispatch(std::shared_ptr<RequestObject> request);

  // For Unit testing only.
  FRIEND_TEST(S3RouterTest, ReturnsTrueForMatchingDefaultEP);
  FRIEND_TEST(S3RouterTest, ReturnsFalseForMisMatchOfDefaultEP);
  FRIEND_TEST(S3RouterTest, ReturnsFalseForEmptyDefaultEP);
  FRIEND_TEST(S3RouterTest, ReturnsTrueForMatchingEP);
  FRIEND_TEST(S3RouterTest, ReturnsTrueForMatchingRegionEP);
  FRIEND_TEST(S3RouterTest, ReturnsFalseForMisMatchRegionEP);
  FRIEND_TEST(S3RouterTest, ReturnsFalseForEmptyRegionEP);
  FRIEND_TEST(S3RouterTest, ReturnsTrueForMatchingSubEP);
  FRIEND_TEST(S3RouterTest, ReturnsTrueForMatchingSubRegionEP);
  FRIEND_TEST(S3RouterTest, ReturnsFalseForMisMatchSubRegionEP);
  FRIEND_TEST(S3RouterTest, ReturnsFalseForInvalidEP);
  FRIEND_TEST(S3RouterTest, ReturnsFalseForEmptyEP);
  FRIEND_TEST(S3RouterTest, ReturnsTrueForMatchingEUSubRegionEP);
};

class MeroRouter : public Router {
 private:
  MeroAPIHandlerFactory *api_handler_factory;
  MeroUriFactory *uri_factory;

 public:
  MeroRouter(MeroAPIHandlerFactory *api_creator, MeroUriFactory *uri_creator);
  virtual ~MeroRouter();
  // Dispatch to registered handlers.
  void dispatch(std::shared_ptr<RequestObject> request);

  // For Unit testing only.
  // FRIEND_TEST(S3RouterTest, ReturnsTrueForMatchingDefaultEP);
  // FRIEND_TEST(S3RouterTest, ReturnsFalseForMisMatchOfDefaultEP);
  // FRIEND_TEST(S3RouterTest, ReturnsFalseForEmptyDefaultEP);
  // FRIEND_TEST(S3RouterTest, ReturnsTrueForMatchingEP);
  // FRIEND_TEST(S3RouterTest, ReturnsTrueForMatchingRegionEP);
  // FRIEND_TEST(S3RouterTest, ReturnsFalseForMisMatchRegionEP);
  // FRIEND_TEST(S3RouterTest, ReturnsFalseForEmptyRegionEP);
  // FRIEND_TEST(S3RouterTest, ReturnsTrueForMatchingSubEP);
  // FRIEND_TEST(S3RouterTest, ReturnsTrueForMatchingSubRegionEP);
  // FRIEND_TEST(S3RouterTest, ReturnsFalseForMisMatchSubRegionEP);
  // FRIEND_TEST(S3RouterTest, ReturnsFalseForInvalidEP);
  // FRIEND_TEST(S3RouterTest, ReturnsFalseForEmptyEP);
  // FRIEND_TEST(S3RouterTest, ReturnsTrueForMatchingEUSubRegionEP);
};

#endif

// S3 URL Patterns for various APIs.

// List Buckets  -> http://s3.seagate.com/ GET

// Bucket APIs   -> http://s3.seagate.com/bucketname
// Host Header = s3.seagate.com or empty
//               -> http://bucketname.s3.seagate.com
// Host Header = *.s3.seagate.com
// Host Header = anything else = bucketname
// location      -> http://s3.seagate.com/bucketname?location
// ACL           -> http://s3.seagate.com/bucketname?acl
//               -> http://bucketname.s3.seagate.com/?acl
// Policy        -> http://s3.seagate.com/bucketname?policy
// Logging       -> http://s3.seagate.com/bucketname?logging
// Lifecycle     -> http://s3.seagate.com/bucketname?lifecycle
// CORs          -> http://s3.seagate.com/bucketname?cors
// notification  -> http://s3.seagate.com/bucketname?notification
// replicaton    -> http://s3.seagate.com/bucketname?replicaton
// tagging       -> http://s3.seagate.com/bucketname?tagging
// requestPayment-> http://s3.seagate.com/bucketname?requestPayment
// versioning    -> http://s3.seagate.com/bucketname?versioning
// website       -> http://s3.seagate.com/bucketname?website
// list multipart uploads in progress
//               -> http://s3.seagate.com/bucketname?uploads

// Object APIs   -> http://s3.seagate.com/bucketname/ObjectKey
// Host Header = s3.seagate.com or empty
//               -> http://bucketname.s3.seagate.com/ObjectKey
// Host Header = *.s3.seagate.com
// Host Header = anything else = bucketname

// ACL           -> http://s3.seagate.com/bucketname/ObjectKey?acl
//               -> http://bucketname.s3.seagate.com/ObjectKey?acl

// Multiple dels -> http://s3.seagate.com/bucketname?delete

// Initiate multipart upload
//               -> http://s3.seagate.com/bucketname/ObjectKey?uploads
// Upload part   ->
// http://s3.seagate.com/bucketname/ObjectKey?partNumber=PartNumber&uploadId=UploadId
// Complete      -> http://s3.seagate.com/bucketname/ObjectKey?uploadId=UploadId
// Abort         -> http://s3.seagate.com/bucketname/ObjectKey?uploadId=UploadId
// DEL
