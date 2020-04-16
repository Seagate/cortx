/* -*- c -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 *                  Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 * Original creation date: 29-Aug-2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/preload.h"
#include "conf/onwire.h"     /* m0_confx */
#include "conf/onwire_xc.h"  /* m0_confx_xc */
#include "xcode/xcode.h"
#include "lib/memory.h"      /* M0_ALLOC_PTR */
#include "lib/errno.h"       /* ENOMEM */

M0_INTERNAL void m0_confx_free(struct m0_confx *enc)
{
	M0_ENTRY();
	if (enc != NULL)
		m0_xcode_free_obj(&M0_XCODE_OBJ(m0_confx_xc, enc));
	M0_LEAVE();
}

M0_INTERNAL int m0_confstr_parse(const char *str, struct m0_confx **out)
{
	int rc;

	M0_ENTRY();

	M0_ALLOC_PTR(*out);
	if (*out == NULL)
		return M0_ERR(-ENOMEM);

	rc = m0_xcode_read(&M0_XCODE_OBJ(m0_confx_xc, *out), str);
	if (rc != 0) {
		M0_LOG(M0_WARN, "Cannot parse configuration string:\n%s", str);
		m0_confx_free(*out);
		*out = NULL;
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_confx_to_string(struct m0_confx *confx, char **out)
{
	m0_bcount_t size;
	int         rc;

	M0_ENTRY();

	size = m0_xcode_print(&M0_XCODE_OBJ(m0_confx_xc, confx), NULL, 0) + 1;
	/*
	 * Spiel sends conf string over bulk transport. Bulk buffers
	 * have to be aligned.
	 */
	*out = m0_alloc_aligned(size, PAGE_SHIFT);
	if (*out == NULL)
		return M0_ERR_INFO(-ENOMEM, "failed to allocate internal buffer"
				   " for encoded Spiel conf data");
	/* Convert */
	rc = m0_xcode_print(&M0_XCODE_OBJ(m0_confx_xc, confx),
			    *out, size) <= size ? 0 : M0_ERR(-ENOMEM);
	if (rc != 0) {
		m0_free_aligned(*out, size, PAGE_SHIFT);
		*out = NULL;
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_confx_string_free(char *str)
{
	M0_PRE(m0_addr_is_aligned(str, PAGE_SHIFT));
	m0_free_aligned(str, strlen(str)+1, PAGE_SHIFT);
}

#undef M0_TRACE_SUBSYSTEM
