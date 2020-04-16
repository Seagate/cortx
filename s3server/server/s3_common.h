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

#ifndef __S3_SERVER_S3_COMMON_H__
#define __S3_SERVER_S3_COMMON_H__

#include <stdlib.h>
#include <string>
#include <cstring>
#include <map>

#ifdef __cplusplus
#define EXTERN_C_BLOCK_BEGIN extern "C" {
#define EXTERN_C_BLOCK_END }
#define EXTERN_C_FUNC extern "C"
#else
#define EXTERN_C_BLOCK_BEGIN
#define EXTERN_C_BLOCK_END
#define EXTERN_C_FUNC
#endif

#define MAX_COLLISION_RETRY_COUNT 20

#define ACCOUNT_USER_INDEX_NAME "ACCOUNTUSERINDEX"

// We reserve 255 oids after M0_CLOVIS_ID_APP for S3 servers internal use.
#define S3_OID_RESERVED_COUNT 255
// First OID after M0_CLOVIS_ID_APP is reserved for S3 root AccountUserIndex.
// see s3_user_account_index_metadata.h/cc
#define S3_ROOT_ACC_USER_IDX_OFFSET 1

// Part sizes in bytes for multipart operation
// 5242880 -- 5MB
#define MINIMUM_ALLOWED_PART_SIZE 5242880
// 5368709120 -- 5GB
#define MAXIMUM_ALLOWED_PART_SIZE 5368709120
// Minmimum part number for multipart operations
#define MINIMUM_PART_NUMBER 1
// Maxmimum part number for multipart operations
#define MAXIMUM_PART_NUMBER 10000

enum class S3ApiType {
  service,
  bucket,
  object,
  management,      // Currently only used by Auth Server
  faultinjection,  // Non S3 Apitype used internally for system tests
  unsupported      // Invalid or Unsupported API
};

enum class MeroApiType {
  index,
  keyval,
  object,
  faultinjection,
  unsupported  // Invalid or Unsupported API
};

enum class MeroOperationCode {
  none
};

enum class S3OperationCode {
  none,  // Operation on current object.

  // Common Operations
  acl,

  // Bucket Operations
  encryption,
  location,
  policy,
  logging,
  lifecycle,
  cors,
  notification,
  replicaton,
  tagging,
  requestPayment,
  versioning,
  website,
  listuploads,
  analytics,
  inventory,
  metrics,
  replication,
  accelerate,
  versions,

  // Object Operations
  initupload,
  partupload,
  completeupload,
  abortupload,
  multidelete,
  multipart,
  torrent,
  selectcontent,
  restore
};

enum class S3AuthClientOpType {
  authentication,
  authorization,
  aclvalidation,
  policyvalidation
};

struct compare {
  bool operator()(const std::string& lhs, const std::string& rhs) const {
    return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
  }
};

extern std::map<std::string, S3OperationCode, compare> S3OperationString;

inline std::string operation_code_to_str(S3OperationCode code) {
  switch (code) {
    case S3OperationCode::none:
      return "S3OperationCode::none";
    case S3OperationCode::acl:
      return "S3OperationCode::acl";
    case S3OperationCode::encryption:
      return "S3OperationCode::encryption";
    case S3OperationCode::policy:
      return "S3OperationCode::policy";
    case S3OperationCode::location:
      return "S3OperationCode::location";
    case S3OperationCode::multipart:
      return "S3OperationCode::multipart";
    case S3OperationCode::multidelete:
      return "S3OperationCode::multidelete";
    case S3OperationCode::requestPayment:
      return "S3OperationCode::requestPayment";
    case S3OperationCode::lifecycle:
      return "S3OperationCode::lifecycle";
    case S3OperationCode::cors:
      return "S3OperationCode::cors";
    case S3OperationCode::analytics:
      return "S3OperationCode::analytics";
    case S3OperationCode::inventory:
      return "S3OperationCode::inventory";
    case S3OperationCode::metrics:
      return "S3OperationCode::metrics";
    case S3OperationCode::tagging:
      return "S3OperationCode::tagging";
    case S3OperationCode::website:
      return "S3OperationCode::website";
    case S3OperationCode::replication:
      return "S3OperationCode::replication";
    case S3OperationCode::accelerate:
      return "S3OperationCode::accelerate";
    case S3OperationCode::logging:
      return "S3OperationCode::logging";
    case S3OperationCode::notification:
      return "S3OperationCode::notification";
    case S3OperationCode::torrent:
      return "S3OperationCode::torrent";
    case S3OperationCode::versioning:
      return "S3OperationCode::versioning";
    case S3OperationCode::versions:
      return "S3OperationCode::versions";
    case S3OperationCode::selectcontent:
      return "S3OperationCode::selectcontent";
    case S3OperationCode::restore:
      return "S3OperationCode::restore";
    default:
      return "S3OperationCode::Unknown";
  }
}

inline std::string api_type_to_str(S3ApiType type) {
  switch (type) {
    case S3ApiType::service:
      return "SERVICE";
    case S3ApiType::bucket:
      return "BUCKET";
    case S3ApiType::object:
      return "OBJECT";
    case S3ApiType::management:
      return "MANAGEMENT";
    case S3ApiType::faultinjection:
      return "FAULTINJECTION";
    case S3ApiType::unsupported:
      return "UNSUPPORTED";
    default:
      return "UNKNOWN";
  }
}

inline std::string api_type_to_str(MeroApiType type) {
  switch (type) {
    case MeroApiType::index:
      return "INDEX";
    case MeroApiType::keyval:
      return "KEYVAL";
    case MeroApiType::object:
      return "OBJECT";
    case MeroApiType::faultinjection:
      return "FAULTINJECTION";
    case MeroApiType::unsupported:
      return "UNSUPPORTED";
    default:
      return "UNKNOWN";
  }
}

inline std::string operation_code_to_audit_str(S3OperationCode code) {
  switch (code) {
    case S3OperationCode::none:
      return "NONE";
    case S3OperationCode::acl:
      return "ACL";
    case S3OperationCode::encryption:
      return "ENCRYPTION";
    case S3OperationCode::policy:
      return "POLICY";
    case S3OperationCode::location:
      return "LOCATION";
    case S3OperationCode::multipart:
      return "MULTIPART";
    case S3OperationCode::multidelete:
      return "MULTIDELETE";
    case S3OperationCode::requestPayment:
      return "REQUESTPAYMENT";
    case S3OperationCode::lifecycle:
      return "LIFECYCLE";
    case S3OperationCode::cors:
      return "CORS";
    case S3OperationCode::analytics:
      return "ANALYTICS";
    case S3OperationCode::inventory:
      return "INVENTORY";
    case S3OperationCode::metrics:
      return "METRICS";
    case S3OperationCode::tagging:
      return "TAGGING";
    case S3OperationCode::website:
      return "WEBSITE";
    case S3OperationCode::replication:
      return "REPLICATION";
    case S3OperationCode::accelerate:
      return "ACCELERATE";
    case S3OperationCode::logging:
      return "LOGGING";
    case S3OperationCode::notification:
      return "NOTIFICATION";
    case S3OperationCode::torrent:
      return "TORRENT";
    case S3OperationCode::versioning:
      return "VERSIONING";
    case S3OperationCode::versions:
      return "VERSIONS";
    case S3OperationCode::selectcontent:
      return "SELECTCONTENT";
    case S3OperationCode::restore:
      return "RESTORE";
    case S3OperationCode::listuploads:
      return "LISTUPLOADS";
    case S3OperationCode::initupload:
      return "INITUPLOAD";
    case S3OperationCode::partupload:
      return "PARTUPLOAD";
    case S3OperationCode::completeupload:
      return "COMPLETEUPLOAD";
    case S3OperationCode::abortupload:
      return "ABORTUPLOAD";
    default:
      return "UNKNOWN";
  }
}

inline std::string operation_code_to_audit_str(MeroOperationCode code) {
  switch (code) {
    case MeroOperationCode::none:
      return "NONE";
    default:
      return "UNKNOWN";
  }
}

enum class S3AsyncOpStatus {
  unknown,
  inprogress,
  success,
  failed,
  connection_failed
};

enum class S3IOOpStatus { saved, failed };

enum class S3ClovisEntityType { realm, object, index };

// returns 'true' if numeric value of string 'x' is less than that of 'y'
struct S3NumStrComparator {
  bool operator()(const std::string &x, const std::string &y) const {
    return strtoul(x.c_str(), NULL, 0) < strtoul(y.c_str(), NULL, 0);
  }
};

inline std::string clovis_entity_type_to_string(S3ClovisEntityType type) {
  switch (type) {
    case S3ClovisEntityType::realm:
      return "realm";
    case S3ClovisEntityType::object:
      return "object";
    case S3ClovisEntityType::index:
      return "index";
    default:
      return "Unknown entity type";
  }
}

#endif
