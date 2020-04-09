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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 8-Apr-2013
 */

#pragma once

#ifndef __MERO_MERO_VERSION_H__
#define __MERO_MERO_VERSION_H__

#include "mero/version_macros.h"
#include "lib/types.h"

struct m0_build_info {
	uint32_t     bi_version;
	const char  *bi_version_string;
	const char  *bi_git_rev_id;
	const char  *bi_git_describe;
	const char  *bi_git_branch;
	const char  *bi_xcode_protocol_checksum;
	const char  *bi_xcode_protocol_be_checksum;
	const char  *bi_xcode_protocol_conf_checksum;
	const char  *bi_xcode_protocol_rpc_checksum;
	const char  *bi_host;
	const char  *bi_user;
	const char  *bi_time;
	const char  *bi_toolchain;
	const char  *bi_kernel;
	const char  *bi_cflags;
	const char  *bi_kcflags;
	const char  *bi_ldflags;
	const char  *bi_configure_opts;
	const char  *bi_build_dir;
	const char  *bi_lustre_src;
	const char  *bi_lustre_version;
};

const struct m0_build_info *m0_build_info_get(void);

void m0_build_info_print(void);

#endif /* __MERO_MERO_VERSION_H__ */

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
