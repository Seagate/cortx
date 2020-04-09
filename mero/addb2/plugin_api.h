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

#pragma once

#ifndef __MERO_ADDB2_PLUGIN_API_H__
#define __MERO_ADDB2_PLUGIN_API_H__

/**
 * @addtogroup addb2
 *
 * m0addb2dump plugins API.
 *
 * External plugins allow to add custom addb records interpreters.
 * Custom interpreters should have ids (field ii_id of structure m0_id_intrp)
 * from reserved external ranges (addb2/addb2_internal.h).
 *
 * @{
 */

#include <stdint.h>
#include "addb2/addb2_internal.h"

/**
 * This function is called by the m0addb2dump utility.
 * It should return an array of interpreters in the intrp parameter.
 * The last terminating element of the array must have zero-struct { 0 }.
 */
int m0_addb2_load_interps(uint64_t flags, struct m0_addb2__id_intrp **intrp);

/** @} end of addb2 group */

#endif /* __MERO_ADDB2_PLUGIN_API_H__ */

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
