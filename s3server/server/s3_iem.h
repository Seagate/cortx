/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original author:  Vinayak Kale <vinayak.kale@seagate.com>
 * Original creation date: 6-January-2017
 */

#pragma once

#ifndef __S3_SERVER_IEM_H__
#define __S3_SERVER_IEM_H__

#include <unistd.h>
#include "s3_log.h"
#include "s3_option.h"

extern S3Option* g_option_instance;

// Note:
// 1. Logs IEM message into syslog as well as to glog.
// 2. For each IEM: timestamp, nodename, pid, filename & line is included in
//    the JSON data format by default. If IEM event has any additional data to
//    send, then pass it as `json_fmt` param.
#define s3_iem(loglevel, event_code, event_description, json_fmt, ...)      \
  do {                                                                      \
    std::string timestamp = s3_get_timestamp();                             \
    s3_log(S3_LOG_INFO, "",                                                 \
           "IEC: " event_code ": " event_description                        \
           ": { \"time\": \"%s\", \"node\": \"%s\", \"pid\": "              \
           "%d, \"file\": \"%s\", \"line\": %d" json_fmt " }",              \
           timestamp.c_str(), g_option_instance->get_s3_nodename().c_str(), \
           getpid(), __FILE__, __LINE__, ##__VA_ARGS__);                    \
    if (loglevel == LOG_ALERT) {                                            \
      s3_syslog(loglevel, "IEC:AS" event_code ":" event_description);       \
    } else {                                                                \
      s3_syslog(LOG_ERR, "IEC:ES" event_code ":" event_description);        \
    }                                                                       \
  } while (0)

// IEM helper macros
// S3 Server IEM Event codes, description string & json data format
#define S3_IEM_AUTH_CONN_FAIL "0030010001"
#define S3_IEM_AUTH_CONN_FAIL_STR "Auth server connection failed"
#define S3_IEM_AUTH_CONN_FAIL_JSON ""

#define S3_IEM_CHUNK_PARSING_FAIL "0030020001"
#define S3_IEM_CHUNK_PARSING_FAIL_STR "Chunk parsing failed"
#define S3_IEM_CHUNK_PARSING_FAIL_JSON ""

#define S3_IEM_CLOVIS_CONN_FAIL "0030030001"
#define S3_IEM_CLOVIS_CONN_FAIL_STR "Clovis connection failed"
#define S3_IEM_CLOVIS_CONN_FAIL_JSON ""

#define S3_IEM_COLLISION_RES_FAIL "0030040001"
#define S3_IEM_COLLISION_RES_FAIL_STR "Collision resolution failed"
#define S3_IEM_COLLISION_RES_FAIL_JSON ""

#define S3_IEM_FATAL_HANDLER "0030050001"
#define S3_IEM_FATAL_HANDLER_STR "Fatal handler triggered"
#define S3_IEM_FATAL_HANDLER_JSON ", \"signal_number\": %d"

#define S3_IEM_DELETE_OBJ_FAIL "0030060001"
#define S3_IEM_DELETE_OBJ_FAIL_STR \
  "Delete object failed causing stale data in Mero"
#define S3_IEM_DELETE_OBJ_FAIL_JSON ""

#define S3_IEM_DELETE_IDX_FAIL "0030060002"
#define S3_IEM_DELETE_IDX_FAIL_STR \
  "Delete index failed causing stale data in Mero"
#define S3_IEM_DELETE_IDX_FAIL_JSON ""

#define S3_IEM_METADATA_CORRUPTED "0030060003"
#define S3_IEM_METADATA_CORRUPTED_STR "Metadata corrupted"
#define S3_IEM_METADATA_CORRUPTED_JSON ""

#endif  // __S3_SERVER_IEM_H__
