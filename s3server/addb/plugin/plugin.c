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
 * Original author:  Ivan Tishchenko   <ivan.tishchenko@seagate.com>
 * Original creation date: 23-Oct-2019
 */

#include <stdio.h>
#include <addb2/plugin_api.h>

#include "s3_addb_plugin_auto.h"
#include "s3_addb_map.h"

/* Borrowed from addb2/dump.c, hope Mero will publish it as API in future */

static void dec(struct m0_addb2__context *ctx, const uint64_t *v, char *buf) {
  sprintf(buf, "%" PRId64, v[0]);
}

static void hex(struct m0_addb2__context *ctx, const uint64_t *v, char *buf) {
  sprintf(buf, "%" PRIx64, v[0]);
}

static void hex0x(struct m0_addb2__context *ctx, const uint64_t *v, char *buf) {
  sprintf(buf, "0x%" PRIx64, v[0]);
}

static void oct(struct m0_addb2__context *ctx, const uint64_t *v, char *buf) {
  sprintf(buf, "%" PRIo64, v[0]);
}

static void ptr(struct m0_addb2__context *ctx, const uint64_t *v, char *buf) {
  sprintf(buf, "@%p", *(void **)v);
}

static void bol(struct m0_addb2__context *ctx, const uint64_t *v, char *buf) {
  sprintf(buf, "%s", v[0] ? "true" : "false");
}

/* end of clip from dump.c */

static void idx_to_state(struct m0_addb2__context *ctx, const uint64_t *v,
                         char *buf) {
  const char *state_name = addb_idx_to_s3_state(*v);
  sprintf(buf, "%s", state_name);
}

static struct m0_addb2__id_intrp gs_curr_ids[] = {
    {S3_ADDB_REQUEST_ID,
     "s3-request-uid",
     {&dec, &hex0x, &hex0x},
     {"s3_request_id", "uid_first_64_bits", "uid_last_64_bits"}, },
    {S3_ADDB_REQUEST_TO_CLOVIS_ID,
     "s3-request-to-clovis",
     {&dec, &dec},
     {"s3_request_id", "clovis_id"}},
    {S3_ADDB_FIRST_REQUEST_ID,
     "s3-request-state",
     {&dec, &idx_to_state},
     {"s3_request_id", "state"},
     .ii_repeat = (S3_ADDB_LAST_REQUEST_ID - S3_ADDB_FIRST_REQUEST_ID)},
    {0}};

int m0_addb2_load_interps(uint64_t flags,
                          struct m0_addb2__id_intrp **intrps_array) {
  /* suppres "unused" warnings */
  (void)dec;
  (void)hex0x;
  (void)oct;
  (void)hex;
  (void)bol;
  (void)ptr;

  *intrps_array = gs_curr_ids;
  return 0;
}
