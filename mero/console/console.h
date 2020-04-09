/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original creation date: 08/03/2011
 */

#pragma once

#ifndef __MERO_CONSOLE_CONSOLE_H__
#define __MERO_CONSOLE_CONSOLE_H__

#include "lib/types.h"

/**
   @defgroup console Console

   Build a standalone utility that

   - connects to a specified service.
   - constructs a fop of a specified fop type and with specified
     values of fields and sends it to the service.
   - waits fop reply.
   - outputs fop reply to the user.

   The console utility can send a DEVICE_FAILURE fop to a server. Server-side
   processing for fops of this type consists of calling a single stub function.
   Real implementation will be supplied by the middleware.cm-setup task.

   @{
*/

extern bool m0_console_verbose;

/** @} end of console group */
#endif /* __MERO_CONSOLE_CONSOLE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
