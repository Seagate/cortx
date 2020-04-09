/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 17-Jan-2014
 */
#pragma once
#ifndef __MERO_NET_MODULE_H__
#define __MERO_NET_MODULE_H__

#include "module/module.h"  /* m0_module */
#include "net/net.h"        /* m0_net_domain */

/**
 * @addtogroup net
 *
 * @{
 */

/** Identifiers of network transports. */
enum m0_net_xprt_id {
	M0_NET_XPRT_LNET,
	M0_NET_XPRT_BULKMEM,
	M0_NET_XPRT_NR
};

/** Network transport module. */
struct m0_net_xprt_module {
	struct m0_module     nx_module;
	struct m0_net_xprt  *nx_xprt;
	struct m0_net_domain nx_domain;
};

/** Network module. */
struct m0_net_module {
	struct m0_module          n_module;
	struct m0_net_xprt_module n_xprts[M0_NET_XPRT_NR];
};

/** Levels of m0_net_module::n_module. */
enum {
	M0_LEVEL_NET
};

/** Levels of m0_net_xprt_module::nx_module. */
enum {
	/** m0_net_xprt_module::nx_domain has been initialised. */
	M0_LEVEL_NET_DOMAIN
};

/*
 *  m0_net_module         m0_net_xprt_module
 * +--------------+      +---------------------+
 * | M0_LEVEL_NET |<-----| M0_LEVEL_NET_DOMAIN |
 * +--------------+      +---------------------+
 */
extern const struct m0_module_type m0_net_module_type;

/** @} net */
#endif /* __MERO_NET_MODULE_H__ */
