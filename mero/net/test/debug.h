/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 01/12/2013
 */

#include "lib/trace.h"	/* M0_LOG */

/**
   @defgroup NetTestDebugDFS Debugging tools
   @ingroup NetTestDFS

   Usage recipe
   0. in .c file
   1. #define NET_TEST_MODULE_NAME name_of_net_test_module
   2. #include "net/test/debug.h"
   3. Use LOGD() macro for regular debug output.
   4. #undef NET_TEST_MODULE_NAME
      in the end of file (or scope in which LOGD() is needed)
      (because of altogether build mode).
   5. Enable/disable debug output from any point using
      LOGD_VAR_NAME(some_module_name) variable
      (declared using LOGD_VAR_DECLARE(some_module_name))
      Debug output is disabled by default.

   Macro names used: LOGD, NET_TEST_MODULE_NAME, LOGD_VAR_DECLARE, LOGD_VAR_NAME
   @note Include guards are not needed in this file because
   LOGD_VAR_NAME(NET_TEST_MODULE_NAME) variable should be defined
   for each module that includes this file.

   @{
 */

#ifndef NET_TEST_MODULE_NAME
M0_BASSERT("NET_TEST_MODULE_NAME should be defined "
	   "before including debug.h" == NULL);
#endif

#ifndef LOGD_VAR_NAME
#define LOGD_VAR_NAME(module_name)					\
	M0_CAT(m0_net_test_logd_, module_name)
#endif

/**
   There is one variable per inclusion of this file.
   It is useful to enable/disable module debug messages from any point in code.
 */
bool LOGD_VAR_NAME(NET_TEST_MODULE_NAME) = false;

#ifndef LOGD_VAR_DECLARE
#define LOGD_VAR_DECLARE(module_name)					\
	extern bool LOGD_VAR_NAME(module_name);
#endif

#undef LOGD
#define LOGD(...)							\
	do {								\
		if (LOGD_VAR_NAME(NET_TEST_MODULE_NAME))		\
			M0_LOG(M0_DEBUG, __VA_ARGS__);			\
	} while (0)


/**
   @} end of NetTestDebugDFS group
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
