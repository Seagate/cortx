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
* Original author: Sourish Banerjee <sourish.banerjee@seagate.com>
* Original creation date: 22-March-2020
*/

#pragma once

#ifndef __MERO_IEM_H__
#define __MERO_IEM_H__

enum m0_mero_iem_severity {
	M0_MERO_IEM_SEVERITY_TEST = 0,
	M0_MERO_IEM_SEVERITY_A_ALERT,
	M0_MERO_IEM_SEVERITY_X_CRITICAL,
	M0_MERO_IEM_SEVERITY_E_ERROR,
	M0_MERO_IEM_SEVERITY_W_WARNING,
	M0_MERO_IEM_SEVERITY_N_NOTICE,
	M0_MERO_IEM_SEVERITY_C_CONFIGURATION,
	M0_MERO_IEM_SEVERITY_I_INFORMATIONAL,
	M0_MERO_IEM_SEVERITY_D_DETAIL,
	M0_MERO_IEM_SEVERITY_B_DEBUG,
};

/**
 * The members of enum m0_mero_iem_module and enum m0_mero_iem_event are mapped
 * against the file [low-level/files/iec_mapping/mero] in the sspl repo
 * http://gitlab.mero.colo.seagate.com/eos/sspl
 *
 * Field description of the mpping file is available in slide 11 of the  "EES
 * RAS IEM Alerts Userstories" document
 *
 * 0020010001,TestIEM,EOS Core test IEM
 * 002 is component id, 001 is module id and 0001 is event id
 *
 * As per "EES RAS IEM Alerts Userstories" document,
 *     Other teams inform RAS team about a new IEMs.
 *     RAS team will add new IEMs to this file.
 *     File will be local to SSPL and may be a part of repo.
 *
 * Any new entry to these enums must also be updated to the mapping file as
 * well.
 */

enum m0_mero_iem_module {
	M0_MERO_IEM_MODULE_TEST = 1,
	M0_MERO_IEM_MODULE_IO,
	M0_MERO_IEM_MODULE_OS,
};

enum m0_mero_iem_event {
	M0_MERO_IEM_EVENT_TEST = 1,
	M0_MERO_IEM_EVENT_IOQ,
	M0_MERO_IEM_EVENT_FREE_SPACE,
};

/**
 * The function must be called with appropriate parameters using
 * the macros M0_MERO_IEM() & M0_MERO_IEM_DESC() to send an IEM alert.
 *
 * @param file from where m0_mero_iem is called, use __FILE__
 * @param function from where m0_mero_iem is called, use __FUNCTION__
 * @param line from where m0_mero_iem is called, use __LINE__
 * @param sev_id a valid value from enum m0_mero_iem_severity
 * @param mod_id a valid value from enum m0_mero_iem_module
 * @param evt_id a valid value from enum m0_mero_iem_event
 * @param desc a string description with variable args. Can be NULL,
 *             max (512-1) bytes in length.
 */
void m0_mero_iem(const char* file, const char* function, int line,
		const enum m0_mero_iem_severity sev_id,
		const enum m0_mero_iem_module mod_id,
		const enum m0_mero_iem_event evt_id,
		const char* desc, ...);

#define M0_MERO_IEM(_sev_id, _mod_id, _evt_id) \
	m0_mero_iem(__FILE__, __FUNCTION__, __LINE__, \
	_sev_id, _mod_id, _evt_id, NULL)

#define M0_MERO_IEM_DESC(_sev_id, _mod_id, _evt_id, _desc, ...) \
	m0_mero_iem(__FILE__, __FUNCTION__, __LINE__, \
	_sev_id, _mod_id, _evt_id, _desc, __VA_ARGS__)

#endif  // __MERO_IEM_H__
