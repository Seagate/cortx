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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0D

#include <stdio.h>
#include <stdarg.h>

#include "mero/iem.h"
#include "lib/trace.h"

// Reference : https://docs.google.com/presentation/d/1cZ2ugLD6Eg7Yx6VJ0tDNj5ULeBpYskSVK-QlFhNuF3Q/edit#slide=id.g55f68daf15_0_3
// Slide 4
const char M0_MERO_IEM_SOURCE_ID = 'S';

// Slide 12
const int M0_MERO_IEM_COMPONENT_ID_MERO = 2;

const char *m0_mero_iem_severity = "TAXEWNCIDB";

void m0_mero_iem(const char* file, const char* function, int line,
		const enum m0_mero_iem_severity sev_id,
		const enum m0_mero_iem_module mod_id,
		const enum m0_mero_iem_event evt_id,
		const char* msg, ...)
{
	char     description[512] = {0x0};
	va_list  aptr;

	if (msg != NULL && (strlen(msg) != 0)) {
		va_start(aptr, msg);
		vsnprintf(description, sizeof(description)-1, msg, aptr);
		va_end(aptr);
	}
	printf("IEC: %c%c%03x%03x%04x:%s\n",
		m0_mero_iem_severity[sev_id],
		M0_MERO_IEM_SOURCE_ID,
		M0_MERO_IEM_COMPONENT_ID_MERO,
		mod_id, evt_id, description);
	fflush(stdout);

	M0_LOG(M0_INFO, "IEC: %c%c%3x%3x%4x:%s",
		m0_mero_iem_severity[sev_id],
		M0_MERO_IEM_SOURCE_ID,
		M0_MERO_IEM_COMPONENT_ID_MERO,
		mod_id, evt_id, (const char*)description);
	M0_LOG(M0_INFO, "from %s:%s:%d",
		(const char*)file, (const char*)function, line);

}
#undef M0_TRACE_SUBSYSTEM
