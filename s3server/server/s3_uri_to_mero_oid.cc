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
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#include "clovis_helpers.h"
#include "s3_uri_to_mero_oid.h"
#include "fid/fid.h"
#include "murmur3_hash.h"
#include "s3_common.h"
#include "s3_log.h"
#include "s3_perf_logger.h"
#include "s3_stats.h"
#include "s3_timer.h"
#include "s3_iem.h"

int S3UriToMeroOID(std::shared_ptr<ClovisAPI> s3_clovis_api, const char *name,
                   const std::string &request_id, m0_uint128 *ufid,
                   S3ClovisEntityType type) {
  s3_log(S3_LOG_DEBUG, "", "Entering\n");
  int rc;
  S3Timer timer;
  struct m0_uint128 tmp_uint128;
  struct m0_fid index_fid;

  bool is_murmurhash_fid =
      S3Option::get_instance()->is_murmurhash_oid_enabled();
  if (ufid == NULL) {
    s3_log(S3_LOG_ERROR, request_id, "Invalid argument, ufid pointer is NULL");
    return -EINVAL;
  }
  if (name == NULL) {
    s3_log(S3_LOG_ERROR, request_id,
           "Invalid argument, input parameter 'name' is NULL\n");
    return -EINVAL;
  }

  timer.start();

  if (is_murmurhash_fid) {
    // Murmur Hash Algorithm usage for OID generation
    size_t len;
    uint64_t hash128_64[2];

    len = strlen(name);
    if (len == 0) {
      // oid should not be 0
      s3_log(S3_LOG_ERROR, request_id,
             "The input parameter 'name' is empty string\n");
      return -EINVAL;
    }
    MurmurHash3_x64_128(name, len, 0, &hash128_64);

    //
    // Reset the higher 37 bits, will be used by Mero
    // The lower 91 bits used by S3
    // https://jts.seagate.com/browse/CASTOR-2155

    hash128_64[0] = hash128_64[0] & 0x0000000007ffffff;
    tmp_uint128.u_hi = hash128_64[0];
    tmp_uint128.u_lo = hash128_64[1];

    // Ensure OID does not fall in clovis and S3 reserved range.
    struct m0_uint128 s3_range = {0ULL, 0ULL};
    s3_range.u_lo = S3_OID_RESERVED_COUNT;

    struct m0_uint128 reserved_range = {0ULL, 0ULL};
    m0_uint128_add(&reserved_range, &M0_CLOVIS_ID_APP, &s3_range);

    rc = m0_uint128_cmp(&reserved_range, &tmp_uint128);
    if (rc >= 0) {
      struct m0_uint128 res;
      // ID should be more than M0_CLOVIS_ID_APP
      s3_log(S3_LOG_DEBUG, "",
             "Id from Murmur hash algorithm less than M0_CLOVIS_ID_APP\n");
      m0_uint128_add(&res, &reserved_range, &tmp_uint128);
      tmp_uint128.u_hi = res.u_hi;
      tmp_uint128.u_lo = res.u_lo;
      tmp_uint128.u_hi = tmp_uint128.u_hi & 0x0000000007ffffff;
    }
    *ufid = tmp_uint128;
  } else {
    // Unique OID generation by mero.
    if (s3_clovis_api == NULL) {
      s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
    }
    rc = s3_clovis_api->m0_h_ufid_next(ufid);
    if (rc != 0) {
      s3_log(S3_LOG_ERROR, request_id, "Failed to generate UFID\n");
      // May need to change error code to something better in future -- TODO
      s3_iem(LOG_ALERT, S3_IEM_CLOVIS_CONN_FAIL, S3_IEM_CLOVIS_CONN_FAIL_STR,
             S3_IEM_CLOVIS_CONN_FAIL_JSON);
      return rc;
    }
  }

  if (type == S3ClovisEntityType::index) {
    index_fid = M0_FID_TINIT('x', ufid->u_hi, ufid->u_lo);
    ufid->u_hi = index_fid.f_container;
    ufid->u_lo = index_fid.f_key;
  }

  s3_log(S3_LOG_DEBUG, request_id,
         "uri = %s entity = %s oid = "
         "%" SCNx64 " : %" SCNx64 "\n",
         name, clovis_entity_type_to_string(type).c_str(), ufid->u_hi,
         ufid->u_lo);
  timer.stop();
  LOG_PERF("S3UriToMeroOID_ns", request_id.c_str(),
           timer.elapsed_time_in_nanosec());
  s3_stats_timing("uri_to_mero_oid", timer.elapsed_time_in_millisec());

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return 0;
}
