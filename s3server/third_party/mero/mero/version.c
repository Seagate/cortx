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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 8-Apr-2013
 */

#include "mero/version.h"
#include "xcode/protocol_checksum.h"
#ifdef __KERNEL__
#  include <linux/kernel.h>  /* KERN_INFO */
#endif
#include "lib/trace.h"       /* m0_console_printf */

#ifdef __KERNEL__
#  define BI_PREFIX  KERN_INFO "mero: "
#else
#  define BI_PREFIX
#endif

static const struct m0_build_info build_info = {
	.bi_version        = M0_VERSION_CODE,
	.bi_version_string = M0_VERSION_STRING,
	.bi_git_rev_id     = M0_VERSION_GIT_REV_ID,
	.bi_git_describe   = M0_VERSION_GIT_DESCRIBE,
	.bi_git_branch     = M0_VERSION_GIT_BRANCH,
	.bi_host           = M0_VERSION_BUILD_HOST,
	.bi_user           = M0_VERSION_BUILD_USER,
	.bi_time           = M0_VERSION_BUILD_TIME,
	.bi_toolchain      = M0_VERSION_BUILD_GCC,
	.bi_kernel         = M0_VERSION_BUILD_KERNEL,
	.bi_cflags         = M0_VERSION_BUILD_CFLAGS,
	.bi_kcflags        = M0_VERSION_BUILD_KCFLAGS,
	.bi_ldflags        = M0_VERSION_BUILD_LDFLAGS,
	.bi_configure_opts = M0_VERSION_BUILD_CONFIGURE_OPTS,
	.bi_build_dir      = M0_VERSION_BUILD_DIR,
	.bi_lustre_src     = M0_VERSION_LUSTRE_SRC,
	.bi_lustre_version = M0_VERSION_LUSTRE_VER,

	.bi_xcode_protocol_checksum      = M0_XCODE_PROTOCOL_CHECKSUM_MD5,
	.bi_xcode_protocol_be_checksum   = M0_XCODE_PROTOCOL_BE_CHECKSUM_MD5,
	.bi_xcode_protocol_conf_checksum = M0_XCODE_PROTOCOL_CONF_CHECKSUM_MD5,
	.bi_xcode_protocol_rpc_checksum  = M0_XCODE_PROTOCOL_RPC_CHECKSUM_MD5,
};

const struct m0_build_info *m0_build_info_get(void)
{
	return &build_info;
}

void m0_build_info_print(void)
{
	const struct m0_build_info *bi = m0_build_info_get();

	m0_console_printf(BI_PREFIX
			  "Mero %s built on %s  by %s@%s at %s for %s kernel\n",
			  bi->bi_version_string,
			  bi->bi_time,
			  bi->bi_user, bi->bi_host, bi->bi_build_dir,
			  bi->bi_kernel);

	m0_console_printf(BI_PREFIX "\n");

	m0_console_printf(BI_PREFIX "Git branch   : %s\n", bi->bi_git_branch);
	m0_console_printf(BI_PREFIX "Git describe : %s\n", bi->bi_git_describe);
	m0_console_printf(BI_PREFIX "Git revision : %s\n", bi->bi_git_rev_id);

	m0_console_printf(BI_PREFIX "\n");

	m0_console_printf(BI_PREFIX "xcode      protocol checksum: %s\n",
			  bi->bi_xcode_protocol_checksum);
	m0_console_printf(BI_PREFIX "xcode BE   protocol checksum: %s\n",
			  bi->bi_xcode_protocol_be_checksum);
	m0_console_printf(BI_PREFIX "xcode CONF protocol checksum: %s\n",
			  bi->bi_xcode_protocol_conf_checksum);
	m0_console_printf(BI_PREFIX "xcode RPC  protocol checksum: %s\n",
			  bi->bi_xcode_protocol_rpc_checksum);

	m0_console_printf(BI_PREFIX "\n");

	m0_console_printf(BI_PREFIX
			  "Copyright (C) 2019 Xyratex Technology Limited\n");

	m0_console_printf(BI_PREFIX "\n");

	m0_console_printf(BI_PREFIX "toolchain: %s\n", bi->bi_toolchain);
	m0_console_printf(BI_PREFIX "CONFIGURE OPTS: %s\n", bi->bi_configure_opts);
#ifdef __KERNEL__
	m0_console_printf(BI_PREFIX "KCFLAGS: %s\n", bi->bi_kcflags);
#else
	m0_console_printf(BI_PREFIX "CFLAGS: %s\n", bi->bi_cflags);
	m0_console_printf(BI_PREFIX "LDFLAGS: %s\n", bi->bi_ldflags);
#endif

	m0_console_printf(BI_PREFIX "\n");

	m0_console_printf(BI_PREFIX "Lustre: %s\n", bi->bi_lustre_src);
	m0_console_printf(BI_PREFIX "Lustre version: %s\n", bi->bi_lustre_version);
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
