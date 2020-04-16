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
 * Original author: Igor Vartanov <igor.vartanov@seagate.com>
 * Original creation date: 10-Feb-2017
 */

#pragma once

#ifndef __MERO_FIS_FI_COMMAND_H__
#define __MERO_FIS_FI_COMMAND_H__

#include "xcode/xcode.h"
#include "lib/buf_xc.h"
#include "rpc/session.h"

/**
 * @page fis-fspec-command Fault Injection Command.
 *
 * Fault Injection Command is represented by @ref m0_fi_command_req
 * structure. The command requires m0_fi_command_req::fcr_func,
 * m0_fi_command_req::fcr_tag and m0_fi_command_req::fcr_disp be specified to
 * control FI behavior on remote side. None of the fields is expected to be
 * empty.
 *
 * Numeric fields are used when three or four operands implied by disposition
 * (e.g. m0_fi_enable_random() makes use of num1 while m0_fi_enable_off_n_on_m()
 * makes use of both num1 and num2). Otherwise, unused field is ignored.
 *
 * FIS replies with @ref m0_fi_command_rep reporting command application
 * success.
 *
 * A FI command request FOP may be posted regular way as any other FOP in Mero,
 * or a helper function m0_fi_command_post_sync() may be used to the purpose.
 */

/**
 * @addtogroup fis-dfspec
 *
 * @{
 */

/** FI command disposition types supported by FIS. */
enum m0_fi_disp {
	M0_FI_DISP_DISABLE,        /**< Invokes m0_fi_disable() */
	M0_FI_DISP_ENABLE,         /**< Invokes m0_fi_enable() */
	M0_FI_DISP_ENABLE_ONCE,    /**< Invokes m0_fi_enable_once() */
	M0_FI_DISP_RANDOMIZE,      /**< Invokes m0_fi_enable_random() */
	M0_FI_DISP_DO_OFF_N_ON_M,  /**< Invokes m0_fi_enable_off_n_on_m() */
};

/** FI command request. */
struct m0_fi_command_req {
	struct m0_buf fcr_func;     /**< function to fail */
	struct m0_buf fcr_tag;      /**< tag to fail at */
	uint8_t       fcr_disp;     /**< fault disposition, @ref m0_fi_disp */
	uint32_t      fcr_num1;     /**< 1st numeric, optional */
	uint32_t      fcr_num2;     /**< 2nd numeric, optional */
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** FI command reply. */
struct m0_fi_command_rep {
	int32_t fcp_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * Posts fault injection command over already existing rpc session. Remote side
 * to execute fault injection command in accordance with function name, tag and
 * disposition. The command is executed synchronously providing
 * m0_fi_command_rep::fcp_rc as a return code.
 *
 * @param sess - a valid rpc session
 * @param func - function name to fail on remote side
 * @param tag  - tag the function to fail at
 * @param disp - fault disposition
 * @param num1 - 1st numerical (e.g. p in m0_fi_enable_random())
 * @param num2 - 2nd numerical (e.g. m in m0_fi_enable_off_n_on_m())
 */
M0_INTERNAL int m0_fi_command_post_sync(struct m0_rpc_session *sess,
					const char            *func,
					const char            *tag,
					enum m0_fi_disp        disp,
					uint32_t               num1,
					uint32_t               num2);

/** @} end fis-dfspec */
#endif /* __MERO_FIS_FI_COMMAND_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
