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
 * Original author: Valery V. Vorotyntsev <valery.vorotyntsev@seagate.com>
 * Original creation date: 25-Nov-2014
 */

#include "module/param.h"
#include "module/instance.h"  /* m0_get */
#include "mero/magic.h"       /* M0_PARAM_SOURCE_MAGIC */

/**
 * @addtogroup module
 *
 * @{
 */

M0_TL_DESCR_DEFINE(m0_param_sources, "m0_param_sources", static,
		   struct m0_param_source, ps_link, ps_magic,
		   M0_PARAM_SOURCE_MAGIC, M0_PARAM_SOURCES_MAGIC);
M0_TL_DEFINE(m0_param_sources, M0_INTERNAL, struct m0_param_source);

static struct m0_tl *param_sources(void)
{
	struct m0_tl *list = &m0_get()->i_param_sources;
	M0_ASSERT(list->t_head.l_head != NULL); /* the list is initialised */
	return list;
}

M0_INTERNAL void *m0_param_get(const char *key)
{
	struct m0_param_source *src;

	m0_tl_for(m0_param_sources, param_sources(), src) {
		void *p = src->ps_param_get(src, key);
		if (p != NULL)
			return p;
	} m0_tl_endfor;
	return NULL;
}

M0_INTERNAL void m0_param_source_add(struct m0_param_source *src)
{
	M0_PRE(src->ps_param_get != NULL);
	m0_param_sources_tlink_init_at_tail(src, param_sources());
}

M0_INTERNAL void m0_param_source_del(struct m0_param_source *src)
{
	m0_param_sources_tlink_del_fini(src);
}

/** @} module */
