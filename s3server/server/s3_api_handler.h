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

#ifndef __S3_SERVER_S3_API_HANDLER_H__
#define __S3_SERVER_S3_API_HANDLER_H__

#include <memory>

#include "gtest/gtest_prod.h"

#include "s3_action_base.h"
#include "s3_common.h"
#include "s3_request_object.h"

class S3APIHandler {
 protected:
  std::shared_ptr<S3RequestObject> request;
  std::shared_ptr<S3Action> action;

  S3OperationCode operation_code;

  // Only for Unit testing
  std::shared_ptr<S3APIHandler> _get_self_ref() { return self_ref; }
  std::shared_ptr<S3Action> _get_action() { return action; }
  std::string request_id;

 private:
  std::shared_ptr<S3APIHandler> self_ref;

 public:
  S3APIHandler(std::shared_ptr<S3RequestObject> req, S3OperationCode op_code)
      : request(req), operation_code(op_code) {
    request_id = request->get_request_id();
  }
  virtual ~S3APIHandler() {}

  virtual void create_action() = 0;

  virtual void dispatch() {
    s3_log(S3_LOG_DEBUG, request_id, "Entering");

    if (action) {
      action->manage_self(action);
      action->start();
    } else {
      request->respond_unsupported_api();
    }
    i_am_done();

    s3_log(S3_LOG_DEBUG, "", "Exiting");
  }

  // Self destructing object.
  void manage_self(std::shared_ptr<S3APIHandler> ref) { self_ref = ref; }
  // This *MUST* be the last thing on object. Called @ end of dispatch.
  void i_am_done() { self_ref.reset(); }

  FRIEND_TEST(S3APIHandlerTest, ConstructorTest);
  FRIEND_TEST(S3APIHandlerTest, ManageSelfAndReset);
  FRIEND_TEST(S3APIHandlerTest, DispatchActionTest);
  FRIEND_TEST(S3APIHandlerTest, DispatchUnSupportedAction);
};

class S3ServiceAPIHandler : public S3APIHandler {
 public:
  S3ServiceAPIHandler(std::shared_ptr<S3RequestObject> req,
                      S3OperationCode op_code)
      : S3APIHandler(req, op_code) {}

  virtual void create_action();

  FRIEND_TEST(S3ServiceAPIHandlerTest, ConstructorTest);
  FRIEND_TEST(S3ServiceAPIHandlerTest, ManageSelfAndReset);
  FRIEND_TEST(S3ServiceAPIHandlerTest, ShouldCreateS3GetServiceAction);
  FRIEND_TEST(S3ServiceAPIHandlerTest, OperationNoneDefaultNoAction);
};

class S3BucketAPIHandler : public S3APIHandler {
 public:
  S3BucketAPIHandler(std::shared_ptr<S3RequestObject> req,
                     S3OperationCode op_code)
      : S3APIHandler(req, op_code) {}

  virtual void create_action();

  FRIEND_TEST(S3BucketAPIHandlerTest, ConstructorTest);
  FRIEND_TEST(S3BucketAPIHandlerTest, ManageSelfAndReset);
  FRIEND_TEST(S3BucketAPIHandlerTest, ShouldCreateS3GetBucketlocationAction);
  FRIEND_TEST(S3BucketAPIHandlerTest, ShouldNotCreateS3PutBucketlocationAction);
  FRIEND_TEST(S3BucketAPIHandlerTest, ShouldNotHaveAction4OtherHttpOps);
  FRIEND_TEST(S3BucketAPIHandlerTest,
              ShouldCreateS3DeleteMultipleObjectsAction);
  FRIEND_TEST(S3BucketAPIHandlerTest, ShouldCreateS3GetBucketACLAction);
  FRIEND_TEST(S3BucketAPIHandlerTest, ShouldCreateS3PutBucketACLAction);
  FRIEND_TEST(S3BucketAPIHandlerTest, ShouldCreateS3GetMultipartBucketAction);
  FRIEND_TEST(S3BucketAPIHandlerTest, ShouldCreateS3GetBucketPolicyAction);
  FRIEND_TEST(S3BucketAPIHandlerTest, ShouldCreateS3PutBucketPolicyAction);
  FRIEND_TEST(S3BucketAPIHandlerTest, ShouldCreateS3DeleteBucketPolicyAction);
  FRIEND_TEST(S3BucketAPIHandlerTest, ShouldCreateS3HeadBucketAction);
  FRIEND_TEST(S3BucketAPIHandlerTest, ShouldCreateS3GetBucketAction);
  FRIEND_TEST(S3BucketAPIHandlerTest, ShouldCreateS3PutBucketAction);
  FRIEND_TEST(S3BucketAPIHandlerTest, ShouldCreateS3DeleteBucketAction);
  FRIEND_TEST(S3BucketAPIHandlerTest, OperationNoneDefaultNoAction);
};

class S3ObjectAPIHandler : public S3APIHandler {
 public:
  S3ObjectAPIHandler(std::shared_ptr<S3RequestObject> req,
                     S3OperationCode op_code)
      : S3APIHandler(req, op_code) {}

  virtual void create_action();

  FRIEND_TEST(S3ObjectAPIHandlerTest, ConstructorTest);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ManageSelfAndReset);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ShouldCreateS3GetObjectACLAction);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ShouldCreateS3PutObjectACLAction);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ShouldNotHaveAction4OtherHttpOps);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ShouldCreateS3PostCompleteAction);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ShouldCreateS3PostMultipartObjectAction);
  FRIEND_TEST(S3ObjectAPIHandlerTest, DoesNotSupportCopyPart);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ShouldCreateS3PutMultiObjectAction);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ShouldCreateS3GetMultipartPartAction);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ShouldCreateS3AbortMultipartAction);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ShouldCreateS3HeadObjectAction);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ShouldCreateS3PutChunkUploadObjectAction);
  FRIEND_TEST(S3ObjectAPIHandlerTest, DoesNotSupportCopyObject);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ShouldCreateS3PutObjectAction);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ShouldCreateS3GetObjectAction);
  FRIEND_TEST(S3ObjectAPIHandlerTest, ShouldCreateS3DeleteObjectAction);
  FRIEND_TEST(S3ObjectAPIHandlerTest, NoAction);
};

class S3ManagementAPIHandler : public S3APIHandler {
 public:
  S3ManagementAPIHandler(std::shared_ptr<S3RequestObject> req,
                         S3OperationCode op_code)
      : S3APIHandler(req, op_code) {}

  virtual void create_action();
};

class S3FaultinjectionAPIHandler : public S3APIHandler {
 public:
  S3FaultinjectionAPIHandler(std::shared_ptr<S3RequestObject> req,
                             S3OperationCode op_code)
      : S3APIHandler(req, op_code) {}

  virtual void create_action();
};

class S3APIHandlerFactory {
 public:
  virtual ~S3APIHandlerFactory() {}

  virtual std::shared_ptr<S3APIHandler> create_api_handler(
      S3ApiType api_type, std::shared_ptr<S3RequestObject> request,
      S3OperationCode op_code);

  FRIEND_TEST(S3APIHandlerFactoryTest, ShouldCreateS3ServiceAPIHandler);
  FRIEND_TEST(S3APIHandlerFactoryTest, ShouldCreateS3BucketAPIHandler);
  FRIEND_TEST(S3APIHandlerFactoryTest, ShouldCreateS3ObjectAPIHandler);
  FRIEND_TEST(S3APIHandlerFactoryTest, ShouldCreateS3FaultinjectionAPIHandler);
};

#endif
