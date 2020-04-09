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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 17-Nov-2019
 */

/**
 * @addtogroup netsock
 *
 * @{
 */

#pragma once

#ifndef __MERO_NET_SOCK_XCODE_H__
#define __MERO_NET_SOCK_XCODE_H__

#include "xcode/xcode_attr.h"

#ifndef __KERNEL__
#include <sys/types.h>
#include <netinet/in.h>                    /* INET_ADDRSTRLEN */
#include <netinet/ip.h>
#include <arpa/inet.h>                     /* inet_pton, htons */
#else
#include <linux/in.h>
#include <linux/in6.h>
#endif

#include "lib/assert.h"
#include "lib/types.h"
#include "lib/cookie.h"
#include "format/format.h"
#include "xcode/xcode_attr.h"

#include "lib/cookie_xc.h"
#include "format/format_xc.h"

/**
 * sock module binary structures
 * -----------------------------
 */


enum {
	WADDR_LEN = sizeof(struct in6_addr)
};

M0_BASSERT(sizeof(struct in_addr) <= sizeof(struct in6_addr));

/** Protocol-family specific part of addr. */
struct addrdata {
	char v_data[WADDR_LEN];
} M0_XCA_ARRAY M0_XCA_DOMAIN(rpc);

/** Peer address. */
struct addr {
	uint32_t        a_family;
	uint32_t        a_socktype;
	uint32_t        a_protocol;
	uint32_t        a_port;
	struct addrdata a_data;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * Buffer descriptor.
 *
 * This is put inside of generic m0_net_buf_desc. The descriptor uniquely
 * identifies a buffer within a cluster.
 *
 * @see bdesc_encode(), bdesc_decode()
 */
struct bdesc {
	struct addr      bd_addr;
	struct m0_cookie bd_cookie;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Packet header. */
struct packet {
	/** Format header, contains opcode (GET vs. PUT). */
	struct m0_format_header p_header;
	/** Source buffer. */
	struct bdesc            p_src;
	/** Target buffer. */
	struct bdesc            p_dst;
	/** Packet index within the source buffer. */
	uint32_t                p_idx;
	/**
	 * Total number of packets that will be sent to transfer the source
	 * buffer.
	 */
	uint32_t                p_nr;
	/** Number of bytes in the packet payload. */
	m0_bcount_t             p_size;
	/** Starting offset in the source buffer. */
	m0_bindex_t             p_offset;
	/** Size of the source buffer. */
	m0_bcount_t             p_totalsize;
	/** Format footer, contains checksum. */
	struct m0_format_footer p_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** @} end of netsock group */
#endif /* __MERO_NET_SOCK_XCODE_H__ */

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
