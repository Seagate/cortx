/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 05-Feb-2015
 */

#pragma once

#ifndef __MERO_ADDB2_UT_COMMON_H__
#define __MERO_ADDB2_UT_COMMON_H__

/**
 * @defgroup addb2
 *
 * @{
 */

#include "lib/types.h"
#include "addb2/addb2.h"
#include "addb2/consumer.h"
#include "addb2/storage.h"

extern int submitted;

extern int (*submit)(const struct m0_addb2_mach *mach,
		     struct m0_addb2_trace *trace);
extern void (*idle)(const struct m0_addb2_mach *mach);
struct m0_addb2_mach *mach_set(int (*s)(const struct m0_addb2_mach  *,
					struct m0_addb2_trace *));
extern const struct m0_addb2_sensor_ops sensor_ops;
extern const uint64_t SENSOR_MARKER;
extern uint64_t seq;
extern bool sensor_finalised;

void mach_fini(struct m0_addb2_mach *m);
void mach_put(struct m0_addb2_mach *m);
int  fill_one(struct m0_addb2_mach *m);

/* define a smaller record type to fit into kernel stack frame. */
struct small_record {
	struct m0_addb2_value ar_val;
	unsigned              ar_label_nr;
	struct m0_addb2_value ar_label[10];
};

#define VAL(id, ...) {							\
	.va_id   = (id),						\
	.va_nr   = ARRAY_SIZE(((const uint64_t[]) { __VA_ARGS__ })),	\
	.va_data = ((const uint64_t[]) { __VA_ARGS__ } )		\
}

bool valeq(const struct m0_addb2_value *v0, const struct m0_addb2_value *v1);
bool receq(const struct m0_addb2_record *r0, const struct small_record *r1);

/** @} end of addb2 group */
#endif /* __MERO_ADDB2_UT_COMMON_H__ */

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
