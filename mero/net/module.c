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
 * Original creation date: 18-Jan-2014
 */

#include "net/module.h"
#include "module/instance.h"
#include "net/lnet/lnet.h"    /* m0_net_lnet_xprt */
#include "net/bulk_mem.h"     /* m0_net_bulk_mem_xprt */
#include "lib/memory.h"       /* M0_ALLOC_PTR */

static struct m0_module *net_module_create(struct m0 *instance);
static int  level_net_enter(struct m0_module *module);
static void level_net_leave(struct m0_module *module);
static int  level_net_xprt_enter(struct m0_module *module);
static void level_net_xprt_leave(struct m0_module *module);

const struct m0_module_type m0_net_module_type = {
	.mt_name   = "m0_net_module",
	.mt_create = net_module_create
};

static const struct m0_modlev levels_net[] = {
	[M0_LEVEL_NET] = {
		.ml_name  = "M0_LEVEL_NET",
		.ml_enter = level_net_enter,
		.ml_leave = level_net_leave
	}
};

static const struct m0_modlev levels_net_xprt[] = {
	[M0_LEVEL_NET_DOMAIN] = {
		.ml_name  = "M0_LEVEL_NET_DOMAIN",
		.ml_enter = level_net_xprt_enter,
		.ml_leave = level_net_xprt_leave
	}
};

static struct {
	const char         *name;
	struct m0_net_xprt *xprt;
} net_xprt_mods[] = {
	[M0_NET_XPRT_LNET] = {
		.name = "\"lnet\" m0_net_xprt_module",
		.xprt = &m0_net_lnet_xprt
	},
	[M0_NET_XPRT_BULKMEM] = {
		.name = "\"bulk-mem\" m0_net_xprt_module",
		.xprt = &m0_net_bulk_mem_xprt
	}
};
M0_BASSERT(ARRAY_SIZE(net_xprt_mods) ==
	   ARRAY_SIZE(M0_FIELD_VALUE(struct m0_net_module, n_xprts)));

static struct m0_module *net_module_create(struct m0 *instance)
{
	struct m0_net_module *net;
	struct m0_module     *m;
	unsigned              i;

	M0_ALLOC_PTR(net);
	if (net == NULL)
		return NULL;
	m0_module_setup(&net->n_module, m0_net_module_type.mt_name,
			levels_net, ARRAY_SIZE(levels_net), instance);
	for (i = 0; i < ARRAY_SIZE(net->n_xprts); ++i) {
		m = &net->n_xprts[i].nx_module;
		m0_module_setup(m, net_xprt_mods[i].name, levels_net_xprt,
				ARRAY_SIZE(levels_net_xprt), instance);
		m0_module_dep_add(m, M0_LEVEL_NET_DOMAIN,
				  &net->n_module, M0_LEVEL_NET);
	}
	instance->i_moddata[M0_MODULE_NET] = net;
	return &net->n_module;
}

static int level_net_enter(struct m0_module *module)
{
	struct m0_net_module *m = M0_AMB(m, module, n_module);
	int                   i;

	M0_PRE(module->m_cur + 1 == M0_LEVEL_NET);
	/*
	 * We could have introduced a dedicated level for assigning
	 * m0_net_xprt_module::nx_xprt pointers, but assigning them
	 * this way is good enough.
	 */
	for (i = 0; i < ARRAY_SIZE(net_xprt_mods); ++i)
		m->n_xprts[i].nx_xprt = net_xprt_mods[i].xprt;
#if 0 /* XXX TODO
       * Rename current m0_net_init() to m0_net__init(), exclude it
       * from subsystem[] of mero/init.c, and ENABLEME.
       */
	return m0_net__init();
#else
	return 0;
#endif
}

static void level_net_leave(struct m0_module *module)
{
	M0_PRE(module->m_cur == M0_LEVEL_NET);
#if 0 /* XXX TODO
       * Rename current m0_net_fini() to m0_net__fini(), exclude it
       * from subsystem[] of mero/init.c, and ENABLEME.
       */
	m0_net__fini();
#endif
}

static int level_net_xprt_enter(struct m0_module *module)
{
	struct m0_net_xprt_module *m = M0_AMB(m, module, nx_module);

	M0_PRE(module->m_cur + 1 == M0_LEVEL_NET_DOMAIN);
	return m0_net_domain_init(&m->nx_domain, m->nx_xprt);
}

static void level_net_xprt_leave(struct m0_module *module)
{
	M0_PRE(module->m_cur == M0_LEVEL_NET_DOMAIN);
	m0_net_domain_fini(&container_of(module, struct m0_net_xprt_module,
					 nx_module)->nx_domain);
}
