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

#include "module/instance.h"
#include "module/param.h"     /* m0_param_sources_tlist_init */
#include "lib/thread.h"       /* m0_thread_tls */

/**
 * @addtogroup module
 *
 * @{
 */

M0_INTERNAL int m0_init_once(struct m0 *instance);
M0_INTERNAL void m0_fini_once(void);
M0_INTERNAL int m0_subsystems_init(void);
M0_INTERNAL void m0_subsystems_fini(void);
M0_INTERNAL int m0_quiesce_init(void);
M0_INTERNAL void m0_quiesce_fini(void);

M0_LOCKERS__DEFINE(M0_INTERNAL, m0_inst, m0, i_lockers);

M0_INTERNAL struct m0 *m0_get(void)
{
	struct m0 *result = m0_thread_tls()->tls_m0_instance;
	M0_POST(result != NULL);
	return result;
}

M0_INTERNAL void m0_set(struct m0 *instance)
{
	M0_PRE(instance != NULL);
	m0_thread_tls()->tls_m0_instance = instance;
}

static int level_inst_enter(struct m0_module *module)
{
	switch (module->m_cur + 1) {
	case M0_LEVEL_INST_PREPARE: {
		struct m0 *inst = M0_AMB(inst, module, i_self);

		m0_param_sources_tlist_init(&inst->i_param_sources);
		m0_inst_lockers_init(inst);
		return 0;
	}
	case M0_LEVEL_INST_QUIESCE_SYSTEM:
		return m0_quiesce_init();
	case M0_LEVEL_INST_ONCE:
		return m0_init_once(container_of(module, struct m0, i_self));
	case M0_LEVEL_INST_SUBSYSTEMS:
		return m0_subsystems_init();
	}
	M0_IMPOSSIBLE("Unexpected level: %d", module->m_cur + 1);
}

static void level_inst_leave(struct m0_module *module)
{
	struct m0 *inst = M0_AMB(inst, module, i_self);

	M0_PRE(module->m_cur == M0_LEVEL_INST_PREPARE);

	m0_inst_lockers_fini(inst);
	m0_param_sources_tlist_fini(&inst->i_param_sources);
}

static const struct m0_modlev levels_inst[] = {
	[M0_LEVEL_INST_PREPARE] = {
		.ml_name  = "M0_LEVEL_INST_PREPARE",
		.ml_enter = level_inst_enter,
		.ml_leave = level_inst_leave
	},
	[M0_LEVEL_INST_QUIESCE_SYSTEM] = {
		.ml_name  = "M0_LEVEL_INST_QUIESCE_SYSTEM",
		.ml_enter = level_inst_enter,
		.ml_leave = (void *)m0_quiesce_fini
	},
	[M0_LEVEL_INST_ONCE] = {
		.ml_name  = "M0_LEVEL_INST_ONCE",
		.ml_enter = level_inst_enter,
		.ml_leave = (void *)m0_fini_once
	},
	[M0_LEVEL_INST_SUBSYSTEMS] = {
		.ml_name  = "M0_LEVEL_INST_SUBSYSTEMS",
		.ml_enter = level_inst_enter,
		.ml_leave = (void *)m0_subsystems_fini
	},
	[M0_LEVEL_INST_READY] = {
		.ml_name = "M0_LEVEL_INST_READY"
	}
};

M0_INTERNAL void m0_instance_setup(struct m0 *instance)
{
	m0_module_setup(&instance->i_self, "m0 instance",
			levels_inst, ARRAY_SIZE(levels_inst), instance);
}

/** @} module */
