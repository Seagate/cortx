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
 * Original creation date: 5-Aug-2019
 */

#pragma once

#ifndef __MERO_SCRIPTS_SYSTEMTAP_KEM_KEM_H__
#define __MERO_SCRIPTS_SYSTEMTAP_KEM_KEM_H__

#include <linux/types.h>

/**
 * @defgroup kem Kernel Event Message entity definition
 *
 *
 * @{
 */

#define KEMD_DEV_NAME "kemd"

enum ke_type {
	KE_PAGE_FAULT,
	KE_CONTEXT_SWITCH,
};

struct pf_event {
	pid_t              pfe_pid;
	pid_t              pfe_tgid;
	unsigned long long pfe_rdtsc_call;
	unsigned long long pfe_rdtsc_ret;
	unsigned long      pfe_address;
	unsigned int       pfe_write_access;
	int                pfe_fault;
};

struct cs_event {
	pid_t              cse_prev_pid;
	pid_t              cse_prev_tgid;
	pid_t              cse_next_pid;
	pid_t              cse_next_tgid;
	unsigned long long cse_rdtsc;
};

struct ke_data {
	unsigned int ked_type;
	union {
		struct pf_event ked_pf;
		struct cs_event ked_cs;
	} u;
};

struct ke_msg {
	struct timeval kem_timestamp;
	struct ke_data kem_data;
};

/** @} end of kem group */

#endif /* __MERO_SCRIPTS_SYSTEMTAP_KEM_KEM_H__ */

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
