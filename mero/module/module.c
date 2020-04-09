/* -*- C -*- */
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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>,
 *                  Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 8-Jan-2014
 */

#include "module/instance.h"
#include "lib/misc.h"         /* m0_forall */
#include "lib/errno.h"        /* EAGAIN */
#include "lib/arith.h"        /* M0_CNT_INC */

/**
 * @addtogroup module
 *
 * @{
 */

/** Returns true if the first `n' entries of `arr' are unique. */
static bool moddeps_are_unique(const struct m0_moddep *arr, unsigned n)
{
	M0_PRE(n <= M0_MODDEP_MAX);
	return m0_elems_are_unique(arr, n, sizeof *arr);
}

static bool module_invariant(const struct m0_module *mod)
{
	const struct m0_moddep *md;

	return  _0C(mod->m_m0 != NULL) &&
		_0C(0 < mod->m_level_nr && mod->m_level_nr < M0_MODLEV_MAX) &&
		_0C(mod->m_cur >= M0_MODLEV_NONE &&
		    mod->m_cur < mod->m_level_nr) &&
		_0C(m0_forall(i, mod->m_level_nr,
			      ergo(mod->m_level_nrefs[i] > 0,
				   mod->m_cur >= i) &&
			      mod->m_level_nrefs[i] == m0_count(
				      j, mod->m_inv_nr,
				      mod->m_inv[j].md_dst == i &&
				      mod->m_inv[j].md_other->m_cur >=
					mod->m_inv[j].md_src))) &&
		_0C(mod->m_dep_nr <= ARRAY_SIZE(mod->m_dep)) &&
		_0C(mod->m_inv_nr <= ARRAY_SIZE(mod->m_inv)) &&
		_0C(moddeps_are_unique(mod->m_dep, mod->m_dep_nr)) &&
		_0C(moddeps_are_unique(mod->m_inv, mod->m_inv_nr)) &&
		m0_forall(i, mod->m_dep_nr,
			  (md = &mod->m_dep[i]) &&
			  _0C(md->md_other != NULL) &&
			  _0C(md->md_other != mod) &&
			  _0C(md->md_src > M0_MODLEV_NONE &&
			      md->md_src < mod->m_level_nr) &&
			  _0C(md->md_dst > M0_MODLEV_NONE &&
			      md->md_dst < md->md_other->m_level_nr) &&
			  /* Check that dependencies are satisfied. */
			  _0C(ergo(mod->m_cur >= md->md_src,
				   md->md_other->m_cur >= md->md_dst)) &&
			  /* Check that there is a matching inverse
			   * dependency. */
			  _0C(m0_exists(j, md->md_other->m_inv_nr,
					({
						const struct m0_moddep *md1 =
							&md->md_other->m_inv[j];

						md1->md_other == mod &&
						md1->md_src == md->md_src &&
						md1->md_dst == md->md_dst;
					}))));
}

static int module_up(struct m0_module *module, int level)
{
	int            result = 0;
	const uint64_t gen = module->m_m0->i_dep_gen;

	M0_PRE(level < module->m_level_nr);
	M0_PRE(module_invariant(module));

	while (module->m_cur < level && result == 0) {
		int               next = module->m_cur + 1;
		unsigned          i;
		struct m0_moddep *md;

		for (i = 0; i < module->m_dep_nr && result == 0; ++i) {
			md = &module->m_dep[i];
			if (md->md_src == next) {
				result = module_up(md->md_other, md->md_dst);
				if (result == 0 &&
				    module->m_m0->i_dep_gen != gen)
					/*
					 * If generation changed, restart the
					 * initialisation.
					 */
					result = -EAGAIN;
				if (result == 0)
					M0_CNT_INC(md->md_other->m_level_nrefs[
							   md->md_dst]);
				/*
				 * If dependencies failed, don't attempt any
				 * form of cleanup, because it is impossible:
				 * the original levels of the modules we
				 * (transitively) depend on are no longer known.
				 */
			}
		}
		if (result == 0 && module->m_level[next].ml_enter != NULL) {
			result = module->m_level[next].ml_enter(module);
			M0_ASSERT(result != -EAGAIN);
		}
		if (result == 0)
			module->m_cur = next;
	}
	M0_POST(module_invariant(module));
	return result;
}

M0_INTERNAL int m0_module_init(struct m0_module *module, int level)
{
	int result;
	M0_PRE(level > M0_MODLEV_NONE);

	/* Repeat initialisation if a new module or dependency were added. */
	while ((result = module_up(module, level)) == -EAGAIN)
		;
	return result;
}

M0_INTERNAL void m0_module_fini(struct m0_module *module, int level)
{
	M0_PRE(M0_MODLEV_NONE <= level && level < module->m_level_nr);
	M0_PRE(module_invariant(module));

	while (level < module->m_cur &&
	       module->m_level_nrefs[module->m_cur] == 0) {
		int               cur = module->m_cur;
		int               i;
		struct m0_moddep *md;

		if (module->m_level[cur].ml_leave != NULL)
			module->m_level[cur].ml_leave(module);
		--module->m_cur;
		for (i = module->m_dep_nr - 1; i >= 0; --i) {
			md = &module->m_dep[i];
			if (md->md_src == cur) {
				M0_CNT_DEC(md->md_other->m_level_nrefs[
						   md->md_dst]);
				m0_module_fini(md->md_other, M0_MODLEV_NONE);
			}
		}
	}
	M0_POST(module_invariant(module));
}

M0_INTERNAL void m0_module_dep_add(struct m0_module *m0, int l0,
				   struct m0_module *m1, int l1)
{
	M0_PRE(m0 != m1);
	M0_PRE(m0->m_m0 == m1->m_m0);
	M0_PRE(module_invariant(m0));
	M0_PRE(module_invariant(m1));
	M0_PRE(l0 > M0_MODLEV_NONE && l1 > M0_MODLEV_NONE);
	M0_PRE(m0->m_cur < l0); /* Otherwise it is too late to enforce the
				 * dependency. */

	M0_ASSERT(m0->m_dep_nr < ARRAY_SIZE(m0->m_dep));
	m0->m_dep[m0->m_dep_nr++] =
		(struct m0_moddep)M0_MODDEP_INIT(m1, l0, l1);

	M0_ASSERT(m1->m_inv_nr < ARRAY_SIZE(m1->m_inv));
	m1->m_inv[m1->m_inv_nr++] =
		(struct m0_moddep)M0_MODDEP_INIT(m0, l0, l1);

	M0_CNT_INC(m0->m_m0->i_dep_gen);

	M0_POST(module_invariant(m0));
	M0_POST(module_invariant(m1));
}

M0_INTERNAL void m0_module_setup(struct m0_module *module, const char *name,
				 const struct m0_modlev *level, int level_nr,
				 struct m0 *instance)
{
	*module = (struct m0_module){
		.m_name     = name,
		.m_m0       = instance,
		.m_cur      = M0_MODLEV_NONE,
		.m_level    = level,
		.m_level_nr = level_nr
	};
	M0_POST(module_invariant(module));
}

/** @} module */

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
