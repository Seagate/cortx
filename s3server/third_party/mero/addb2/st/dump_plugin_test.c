/* -*- C -*- */
/*
 * COPYRIGHT 2019 XYRATEX TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF XYRATEX TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF XYRATEX TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF XYRATEX LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF XYRATEX'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A XYRATEX REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author: Alexander Sukhachev <alexander.sukhachev@seagate.com>
 * Original creation date: 14-Oct-2019
 */

/**
 * This is simple test plugin for m0addb2dump utility.
 * It's using in system test addb2/st/addb2dump_plugin.sh
 *
 */

#include "addb2/plugin_api.h"
#include <stdio.h>

static void param1(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
        sprintf(buf, "param1: 0x%lu", (long unsigned int)v[0]);
}

static void param2(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
        sprintf(buf, "param2: 0x%lu", (long unsigned int)v[0]);
}


struct m0_addb2__id_intrp ext_intrp[] = {
        { M0_ADDB2__EXT_RANGE_1, "measurement_1",   { &param1, &param2 } },
        { M0_ADDB2__EXT_RANGE_2, "measurement_2",   { &param1, &param2 } },
        { M0_ADDB2__EXT_RANGE_3, "measurement_3",   { &param1, &param2 } },
        { M0_ADDB2__EXT_RANGE_4, "measurement_4",   { &param1, &param2 } },
        { 0 }
};

int m0_addb2_load_interps(uint64_t flags, struct m0_addb2__id_intrp **intrp)
{
        *intrp = ext_intrp;
        return 0;
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
