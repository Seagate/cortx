/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 20-Jun-2016
 */

#pragma once

#ifndef __MERO_CONF_HA_H__
#define __MERO_CONF_HA_H__

/**
 * @defgroup conf-ha
 *
 * @{
 */
#include "lib/types.h"          /* uint64_t */
#include "xcode/xcode_attr.h"   /* M0_XCA_RECORD */
#include "conf/schema.h"        /* m0_conf_service_type */

struct m0_fid;
struct m0_ha;
struct m0_ha_link;

/**
 * Process event.
 *
 * - it's sent from a process and it's about the process;
 * - it's a reliable information about the process state;
 * - if process fails these notifications are not sent.
 *
 */
enum m0_conf_ha_process_event {
	/**
	 * The process is about to start. Usually this notification is sent
	 * after connection to HA is established, but it may not be the first
	 * m0_ha_msg sent from the process.
	 */
	M0_CONF_HA_PROCESS_STARTING,
	/**
	 * The process is fully started and its services can handle requests.
	 */
	M0_CONF_HA_PROCESS_STARTED,
	/**
	 * The process is about to stop. New connections to the services from
	 * this process shouldn't be made after this notification is sent
	 * (exception: if connections are required during the "stopping" phase).
	 */
	M0_CONF_HA_PROCESS_STOPPING,
	/**
	 * Process is stopped. No new connections should be made after this
	 * point. Usually this notification is sent just before process
	 * disconnects from HA, but it may not be the last m0_ha_msg sent
	 * from the process.
	 */
	M0_CONF_HA_PROCESS_STOPPED,
};

/** Defines the source of the process event */
enum m0_conf_ha_process_type {
	/** Source is not defined. Example: the source is a debugging tool. */
	M0_CONF_HA_PROCESS_OTHER,
	/** The event is sent from kernel (only m0t1fs can send this atm). */
	M0_CONF_HA_PROCESS_KERNEL,
	/** The event is sent from m0mkfs */
	M0_CONF_HA_PROCESS_M0MKFS,
	/** The event is sent from m0d */
	M0_CONF_HA_PROCESS_M0D,
};

struct m0_conf_ha_process {
	/** @see m0_conf_ha_process_event for values */
	uint64_t chp_event;
	/** @see m0_conf_ha_process_type for values */
	uint64_t chp_type;
	/**
	 * PID of the current process.
	 * 0 if it's a kernel mode "process" (m0t1fs mount, for example).
	 * @see m0_conf_ha_service:chs_pid, m0_process().
	 */
	uint64_t chp_pid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * Service event.
 *
 * - it's sent from a process with the service and it's about the service;
 * - it's a reliable information about the service state;
 * - if process fails these notifications are not sent;
 * - if service fails but process is alive these notifications are sent.
 *
 */
enum m0_conf_ha_service_event {
	/**
	 * Service is about to start. There is no point in connecting to the
	 * service before this notification is sent.
	 */
	M0_CONF_HA_SERVICE_STARTING,
	/** Service is started and it can handle requests. */
	M0_CONF_HA_SERVICE_STARTED,
	/**
	 * Service is about to stop. New connections to the service shouldn't
	 * be made after this notification is sent if the connections are not
	 * a part of "stopping" phase.
	 */
	M0_CONF_HA_SERVICE_STOPPING,
	/**
	 * Service is stopped. There is no point in connecting to the service
	 * after this notification is sent.
	 */
	M0_CONF_HA_SERVICE_STOPPED,
	/**
	 * Service failed during the starting phase. There is no point in
	 * connecting to the service if it's failed.
	 */
	M0_CONF_HA_SERVICE_FAILED,
};

struct m0_conf_ha_service {
	/** @see m0_conf_ha_service_event for values */
	uint64_t chs_event;
	/** @see m0_conf_service_type for values */
	uint64_t chs_type;
	/**
	 * PID of the current process.
	 * 0 if it's a kernel mode "process" (m0t1fs mount, for example).
	 * @see m0_conf_ha_process:chp_pid, m0_process().
	 */
	uint64_t chs_pid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Sends notification about process state to HA */
M0_INTERNAL void
m0_conf_ha_process_event_post(struct m0_ha                  *ha,
                              struct m0_ha_link             *hl,
                              const struct m0_fid           *process_fid,
                              uint64_t                       pid,
                              enum m0_conf_ha_process_event  event,
                              enum m0_conf_ha_process_type   type);


/** Sends notification about service state to HA */
M0_INTERNAL void
m0_conf_ha_service_event_post(struct m0_ha                  *ha,
                              struct m0_ha_link             *hl,
                              const struct m0_fid           *source_process_fid,
                              const struct m0_fid           *source_service_fid,
                              const struct m0_fid           *service_fid,
                              uint64_t                       pid,
                              enum m0_conf_ha_service_event  event,
                              enum m0_conf_service_type      service_type);


/** @} end of conf-ha group */
#endif /* __MERO_CONF_HA_H__ */

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
