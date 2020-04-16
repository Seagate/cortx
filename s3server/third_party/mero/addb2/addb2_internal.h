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
 * Original creation date: 5-Nov-2019
 */

#pragma once

#ifndef __MERO_ADDB2_ADDB2_INTERNAL_H__
#define __MERO_ADDB2_ADDB2_INTERNAL_H__

/**
 * @defgroup addb2
 *
 * @{
 */

#include "lib/types.h"

struct m0_addb2__context;

/**
 * Custom addb2 records identifier ranges.
 * These ranges must be used in external projects (S3, NFS)
 * and addb2dump utility plugins
 */
enum {
        M0_ADDB2__EXT_RANGE_1 = 0x0010000,
        M0_ADDB2__EXT_RANGE_2 = 0x0020000,
        M0_ADDB2__EXT_RANGE_3 = 0x0030000,
        M0_ADDB2__EXT_RANGE_4 = 0x0040000
};

enum {
        M0_ADDB2__FIELD_MAX = 15
};

/**
 * Structure of the interpreter of addb2 records
 */
struct m0_addb2__id_intrp {
        uint64_t     ii_id;
        const char  *ii_name;
        void       (*ii_print[M0_ADDB2__FIELD_MAX])(
                        struct m0_addb2__context *ctx,
                        const uint64_t *v, char *buf);
        const char  *ii_field[M0_ADDB2__FIELD_MAX];
        void       (*ii_spec)(struct m0_addb2__context *ctx, char *buf);
        int          ii_repeat;
};

/**
 * addb2dump plugin function name
*/
#define M0_ADDB2__PLUGIN_FUNC_NAME "m0_addb2_load_interps"

typedef int (*m0_addb2__intrp_load_t)(uint64_t flags,
                                      struct m0_addb2__id_intrp **intrp);

/** @} end of addb2 group */
#endif /* __MERO_ADDB2_ADDB2_INTERNAL_H__ */

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
