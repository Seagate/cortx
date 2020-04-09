/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include "ut/ut.h"
#include "fdmi/fdmi.h"
#include "fdmi/source_dock_internal.h"

#include "fdmi/ut/sd_common.h"

void fdmi_sd_post_record(void);
void fdmi_sd_apply_filter(void);
void fdmi_sd_release_fom(void);
void fdmi_sd_send_notif(void);

struct m0_ut_suite fdmi_sd_ut = {
	.ts_name = "fdmi-sd-ut",
	.ts_tests = {
		{ "fdmi-sd-post-record", fdmi_sd_post_record},
		{ "fdmi-sd-apply-filter", fdmi_sd_apply_filter},
		{ "fdmi-sd-release-fom", fdmi_sd_release_fom},
		{ "fdmi-sd-send-notif", fdmi_sd_send_notif},

		{ NULL, NULL },
	},
};

#undef M0_TRACE_SUBSYSTEM

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
