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
 * Original author: Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 04/3/2012
 */

#pragma once

#ifndef __MERO_LNET_DRV_UT_H__
#define __MERO_LNET_DRV_UT_H__

enum {
	UT_TEST_NONE       =   0, /**< no test requested, user program idles */
	UT_TEST_DEV        =   1, /**< device registered */
	UT_TEST_OPEN       =   2, /**< open/close */
	UT_TEST_RDWR       =   3, /**< read/write */
	UT_TEST_BADIOCTL   =   4, /**< invalid ioctl */
	UT_TEST_DOMINIT    =   5, /**< open/dominit/close */
	UT_TEST_TMS        =   6, /**< multi-TM start/stop and no cleanup */
	UT_TEST_DUPTM      =   7, /**< duplicate TM start */
	UT_TEST_TMCLEANUP  =   8, /**< multi-TM start with cleanup */
	UT_TEST_MAX        =   8, /**< final implemented test ID */

	UT_TEST_DONE       = 127, /**< done testing, no user response */

	UT_USER_READY = 'r',	  /**< user program is ready */
	UT_USER_SUCCESS = 'y',	  /**< current test succeeded in user space */
	UT_USER_FAIL = 'n',	  /**< current test failed in user space */

	MULTI_TM_NR = 3,
};

/** The /proc file used to coordinate driver unit test */
#define UT_PROC_NAME "m0_lnet_ut"

#endif /* __MERO_LNET_DRV_UT_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
