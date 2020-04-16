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
 * Original author: Mikhail Antropov <mikhail.v.antropov@xyratex.com>
 * Original creation date: 23-Mar-2015
 */

#pragma once
#ifndef __MERO_SSS_PROCESS_FOMS_H__
#define __MERO_SSS_PROCESS_FOMS_H__

#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "fop/fom_generic.h"

/**
 * @defgroup ss_process Process command
 * @{
 */

/**
   Process Reconfig command contains the series of steps:

   1. Save core mask and memory limits to instance data (@ref m0).
      Core mask and memory limits applied in between of finalisation and
      initialisation of modules.

   2. Send signal for finalisation of current Mero instance.
      During finalisation of current Mero instance the system correctly
      finalises all modules and all Mero entities (services, REQH, localities,
      FOPs, FOMs, etc).

   3. Apply core mask and memory limits (see setup.c and instance.c).

   4. Restart Mero instance.

      @note For future development, common possible problem in processing
      Reconfig command is missing of a Mero entity finalisation or cleanup.
 */


/** @} end group ss_process */

#endif /* __MERO_SSS_PROCESS_FOMS_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
