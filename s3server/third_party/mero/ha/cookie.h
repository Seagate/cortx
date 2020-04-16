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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 1-Nov-2019
 */

#pragma once

#ifndef __MERO_HA_COOKIE_H__
#define __MERO_HA_COOKIE_H__

#include "lib/types.h"  /* bool */
#include "lib/time.h"   /* m0_time_t */

#include "lib/types_xc.h"       /* m0_uint128_xc for ha/cookie_xc.h */ /* XXX */

/**
 * @defgroup ha
 *
 * @{
 */


/*
 * m0_ha_cookie is used to check if a process at the other end of the connection
 * is still the same. A combination of PID, start time and OS uptime is used to
 * determine the process change.
 *
 * Reasons why all the fields from the structure are needed:
 * - hc_pid: it might be 2 processes started simultaneously on different CPU
 *   cores. They may get 2 different PIDs but they still may have the same start
 *   time and OS uptime;
 * - hc_time_start: after OS reboot a process may get the same PID and it can
 *   start at the same OS uptime. Start time of the process can't simply be the
 *   same after OS restart in this case;
 * - hc_uptime: if someone wants to play with system time it would be possible
 *   to get the same start time and the same PID (when the PID is reused)
 *   without OS restart. hc_uptime ensures that in this case such processes will
 *   get different m0_ha_cookie.
 * - hc_uuid: it's still possible to restart a system while playing with system
 *   time to get a process with the same m0_ha_cookie after restart. It's highly
 *   unlikely, but it's still possible. In this case UUID generator should
 *   decrease probability of getting the same m0_ha_cookie even more.
 *
 * Anyway, if we still get into the virtually impossible situation when
 * m0_ha_cookie is the same for different processes, it would either result in a
 * Mero process panic or it will become stuck. In either case HA is able to
 * handle such situation.
 *
 * TODO implement a function to get OS uptime.
 */
struct m0_ha_cookie {
	uint64_t          hc_pid;
	m0_time_t         hc_time_start;
	m0_time_t         hc_uptime;
	struct m0_uint128 hc_uuid;
};

struct m0_ha_cookie_xc {
	uint64_t          hcx_pid;
	uint64_t          hcx_time_start;
	uint64_t          hcx_uptime;
	struct m0_uint128 hcx_uuid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

extern const struct m0_ha_cookie m0_ha_cookie_no_record;

/* XXX M0_BASSERT() doesn't work for _xc.h files, need to investigate why */
/*
M0_BASSERT(sizeof(((struct m0_ha_cookie_xc *)NULL)->hcx_time_start) ==
           sizeof(((struct m0_ha_cookie *)NULL)->hc_time_start));
M0_BASSERT(sizeof(((struct m0_ha_cookie_xc *)NULL)->hcx_uptime) ==
           sizeof(((struct m0_ha_cookie *)NULL)->hc_uptime));
           */

M0_INTERNAL void m0_ha_cookie_init(struct m0_ha_cookie *hc);
M0_INTERNAL void m0_ha_cookie_fini(struct m0_ha_cookie *hc);

M0_INTERNAL void m0_ha_cookie_record(struct m0_ha_cookie *hc);
M0_INTERNAL bool m0_ha_cookie_is_eq(const struct m0_ha_cookie *a,
                                    const struct m0_ha_cookie *b);

M0_INTERNAL void m0_ha_cookie_from_xc(struct m0_ha_cookie          *hc,
                                      const struct m0_ha_cookie_xc *hc_xc);
M0_INTERNAL void m0_ha_cookie_to_xc(const struct m0_ha_cookie *hc,
                                    struct m0_ha_cookie_xc    *hc_xc);

/** @} end of ha group */
#endif /* __MERO_HA_COOKIE_H__ */

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
