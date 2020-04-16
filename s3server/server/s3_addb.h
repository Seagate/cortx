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
 * Original author: Evgeniy Brazhnikov
 * Original creation date: 4-Oct-2019
 */

#pragma once

#ifndef __S3_SERVER_ADDB_H__
#define __S3_SERVER_ADDB_H__

#include <cstdint>
#include <vector>
#include <addb2/addb2.h>

#include "s3_addb_plugin_auto.h"
#include "s3_addb_map.h"

// This header defines what is needed to use Mero ADDB.
//
// Mero ADDB is a subsystem which allows to efficiently store certain run-time
// metrics.  They are called ADDB log entries.  Every entry is time stamped
// (with nanosecond precision), and contains up to 15 unsigned 64 bit integers.
// It is up to us what information to store there.  The convention is that the
// first integer is an "action type id" (see enum S3AddbActionTypeId).  This ID
// must be within a range designated to S3 server.  It serves later as a means
// to distinguish between different kinds of log entries that we save to ADDB.
//
// The very first basic usage for ADDB is performance monitoring: s3 server
// will track every API request, and create ADDB log entries when the request
// goes through it's stages, and when it changes state, and also track which
// Clovis operations it executes.

// Initialize addb subsystem (see detailed comments in the implementation).
// Depends on s3_log.
int s3_addb_init();

// Used for s3starup calls without any request-objects.
#define S3_ADDB_STARTUP_REQUESTS_ID 1
// Default addb beginning request id.
#define S3_ADDB_FIRST_GENERIC_REQUESTS_ID 2

// Macro to create ADDB log entry.
//
// addb_action_type_id parameter must be a value from enum S3AddbActionTypeId.
// See also Action::get_addb_action_type_id().
//
// Other parameters must be values of type uint64_t (or implicitly
// convertible).  Current limit is 14 parameters (totals to 15 integers in one
// addb entry since we include addb_action_id).
#define ADDB(addb_action_type_id, ...)                                        \
  do {                                                                        \
    const uint64_t addb_id__ = (addb_action_type_id);                         \
    if (FLAGS_addb) {                                                         \
      if (addb_id__ < S3_ADDB_RANGE_START || addb_id__ > S3_ADDB_RANGE_END) { \
        s3_log(S3_LOG_FATAL, "", "Invalid ADDB Action Type ID %" PRIu64 "\n", \
               addb_id__);                                                    \
      } else {                                                                \
        uint64_t addb_params__[] = {__VA_ARGS__};                             \
        constexpr auto addb_params_size__ =                                   \
            sizeof addb_params__ / sizeof addb_params__[0];                   \
        m0_addb2_add(addb_id__, addb_params_size__, addb_params__);           \
      }                                                                       \
    }                                                                         \
  } while (false)

#endif
