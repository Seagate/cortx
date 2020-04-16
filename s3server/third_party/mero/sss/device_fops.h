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
 * Original author: Mikhail Antropov <mikhail.v.antropov@xyratex.com>
 * Original creation date: 21-Apr-2015
 */

#pragma once

#ifndef __MERO_SSS_DEVICE_FOPS_H__
#define __MERO_SSS_DEVICE_FOPS_H__

#include "lib/types_xc.h"
#include "lib/buf_xc.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"
#include "rpc/rpc_machine.h"

/**
 * @page DLD_sss_device DLD Device command
 *
 * - @ref DLD-sss_device-ds
 * - @ref DLD-sss_device-usecases
 * - @ref DLDGRP_sss_device "Device command internal" <!-- Note link -->
 *
 * Device commands derived disk (see m0_conf_drive) on Mero. Each device
 * must preregister on Pool machine, and has record in confc subsystem. All
 * additional information about disk and its storage device must present in
 * confc on server side.
 *
 * Server side - all know Mero instances with ioservices service.
 *
 * Commands
 *
 * Attach device - create AD stob domain and backing store stob. Also create
 * and run Pool event for set status "Device online" in Pool machine.
 * Its mechanism like create each disk duration start Mero instance.
 *
 * Detach device - finalize AD stob domain and backing store stob in memory.
 * Its not destroy file on storage device - remove information about storage
 * device file for Mero only. Also create and run Pool event for set status
 * "Device offline".
 *
 * Format device - formatting storage device. Not implement on server side.

 * @section DLD-sss_device-ds Data Structures
 *
 * Device command interface data structures include fop m0_sss_device_fop,
 * command enumerated m0_sss_device_req_cmd.
 *
 * Example:
 *
@code
struct m0_fop  *fop;

fop  = m0_sss_device_fop_create(rmachine, cmd, dev_fid);
if (fop == NULL)
	return M0_RC(-ENOMEM);
@endcode

 * cmd - device command id - see m0_sss_device_req_cmd
 * dev_fid - Disk FID.
 *
 * Reply fop - m0_sss_device_fop_rep contain error status of command.
 *
 * @section DLD-sss_device-usecases Unit tests
 *
 * spiel-ci-ut:device-cmds
 *
 * Contain 4 steps:
 *
 * - detach - detach device. Device was connect on standard start UT and
 * description in file ut/conf.xc.
 * Expected return value - OK (0)
 *
 * - attach - attach device. Attach device which was detach on previous step.
 * Expected return value - OK (0)
 *
 * - format - format device. Client send command and receive answer.
 * No activity on server side except receive command and send answer.
 * Expected return value - OK (0)
 *
 * - detach with invalid fid - detach device with invalid fid. Command not send.
 * Expected return value - (-ENOENT)
 *
 *
 * sss-ut:device-fom-fail
 *
 * Test some error when create fom for this commands. See create command
 * m0_sss_device_fop_create and custom fom struct m0_sss_device_fom.
 */

/**
 * @defgroup DLDGRP_sss_device Device command
 * @brief Detailed functional Device command
 *
 * All Device commands use one type of fop m0_sss_device_fop for send
 * command to sss service and one type of fop m0_sss_device_fop_rep
 * for reply.
 * Command different command ID m0_sss_device_req_cmd only.
 *
 * @{
 */

extern struct m0_fop_type m0_sss_fop_device_fopt;
extern struct m0_fop_type m0_sss_fop_device_rep_fopt;

/**
 * Device commands enumerated
 *
 * Value of custom fop field @ref ssd_cmd, determines device operation.
 */
enum m0_sss_device_req_cmd {
	/**
	 * Attach command.
	 * Create AD stob domain, stob and change device status to online in
	 * Pool machine.
	 */
	M0_DEVICE_ATTACH,
	/**
	 * Detach command.
	 * Finalization AD stob domain, stob and change device status to offline
	 * in Pool machine.
	 */
	M0_DEVICE_DETACH,
	/**
	 * Format command.
	 * Format select device.
	 */
	M0_DEVICE_FORMAT,
	/**
	 * Number of device commands.
	 */
	M0_DEVICE_CMDS_NR
};

/** Request to command a device.
 *
 * Request fop contain ID command and device fid.
 * All needs to execute command: index in Pool machine, device cid,
 * etc. fom @ref m0_sss_device_fom - reads form confc uses ssd_fid as
 * disk fid.
 */
struct m0_sss_device_fop {
	/**
	 * Command to execute.
	 * @see enum m0_sss_device_req_cmd
	 */
	uint32_t      ssd_cmd;
	/**
	 * Disk fid.
	 */
	struct m0_fid ssd_fid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Response to m0_sss_device_fop. */
struct m0_sss_device_fop_rep {
	/**
	 * Result of device operation
	 * (-Exxx = failure, 0 = success).
	 * @see enum m0_reqh_process_state
	 */
	int32_t  ssdp_rc;
	/**
	 * Device HA state found on the called SSS side. The field is valid in
	 * case of M0_DEVICE_ATTACH command only.
	 */
	uint32_t ssdp_ha_state;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);


M0_INTERNAL struct m0_fop *m0_sss_device_fop_create(struct m0_rpc_machine *mach,
						    uint32_t               cmd,
						    const struct m0_fid   *fid);

M0_INTERNAL bool m0_sss_fop_is_dev_req(const struct m0_fop *fop);

M0_INTERNAL struct m0_sss_device_fop *m0_sss_fop_to_dev_req(struct m0_fop *fop);

M0_INTERNAL bool m0_sss_fop_is_dev_rep(const struct m0_fop *fop);

M0_INTERNAL
struct m0_sss_device_fop_rep *m0_sss_fop_to_dev_rep(struct m0_fop *fop);

M0_INTERNAL int m0_sss_device_fops_init(void);

M0_INTERNAL void m0_sss_device_fops_fini(void);

/** @} end group  DLDGRP_sss_device */

#endif /* __MERO_SSS_DEVICE_FOPS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
