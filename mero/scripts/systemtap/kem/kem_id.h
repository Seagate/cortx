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
 * Original author: Maxim Malezhin <maxim.malezhin@seagate.com>
 * Original creation date: 30-Aug-2019
 */

#pragma once

#ifndef __MERO_SCRIPTS_SYSTEMTAP_KEM_KEM_ID_H__
#define __MERO_SCRIPTS_SYSTEMTAP_KEM_KEM_ID_H__

#include "addb2/identifier.h"

/**
 * @defgroup kem Kernel Event Message ADDB2 definition
 *
 *
 * @{
 */

enum {
	M0_AVI_KEM_CPU = M0_AVI_KEM_RANGE_START + 1,
	M0_AVI_KEM_PAGE_FAULT,
	M0_AVI_KEM_CONTEXT_SWITCH,
};

/** @} end of kem group */

#endif /* __MERO_SCRIPTS_SYSTEMTAP_KEM_KEM_ID_H__ */

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
