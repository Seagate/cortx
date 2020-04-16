/* -*- C -*- */
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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 11-Jun-2013
 */

/**
 * @addtogroup fom
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FOP

#include "lib/misc.h"                   /* M0_IN, M0_BITS */
#include "lib/errno.h"                  /* ENOMEM */
#include "lib/memory.h"
#include "lib/locality.h"
#include "lib/trace.h"                  /* M0_ERR */
#include "lib/finject.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "fop/fom.h"
#include "fop/fom_simple.h"

static const struct m0_fom_type_ops fom_simple_ft_ops;
static struct m0_reqh_service_type fom_simple_rstype;
static const struct m0_fom_ops fom_simple_ops;
static struct m0_sm_conf fom_simple_conf;

M0_EXTERN struct m0_sm_conf fom_states_conf;

M0_INTERNAL void m0_fom_simple_post(struct m0_fom_simple *simpleton,
				    struct m0_reqh *reqh,
				    struct m0_sm_conf *conf,
				    int (*tick)(struct m0_fom *, void *, int *),
				    void (*free)(struct m0_fom_simple *sfom),
				    void *data, size_t locality)
{
	struct m0_fom_type *fomt;

	if (conf != NULL) {
		if (conf->scf_trans_nr > 0)
			m0_sm_conf_init(conf);
	} else
		conf = &fom_simple_conf;
	fomt = &simpleton->si_type;
	*fomt = (typeof(*fomt)) {
		.ft_ops        = &fom_simple_ft_ops,
		.ft_conf       = *conf,
		.ft_state_conf = fom_states_conf,
		.ft_rstype     = &fom_simple_rstype
	};
	m0_fom_init(&simpleton->si_fom, fomt, &fom_simple_ops,
		    NULL, NULL, reqh);
	simpleton->si_data = data;
	simpleton->si_tick = tick;
	simpleton->si_free = free;
	if (locality == M0_FOM_SIMPLE_HERE)
		locality = m0_locality_here()->lo_idx;
	simpleton->si_locality = locality;
	m0_fom_queue(&simpleton->si_fom);
}

M0_INTERNAL void m0_fom_simple_hoard(struct m0_fom_simple *cat, size_t nr,
				     struct m0_reqh *reqh,
				     struct m0_sm_conf *conf,
				     int (*tick)(struct m0_fom *, void *,
						 int *),
				     void (*free)(struct m0_fom_simple *sfom),
				     void *data)
{
	size_t i;

	for (i = 0; i < nr; ++i)
		m0_fom_simple_post(&cat[i], reqh, conf, tick, free, data, i);
}

M0_INTERNAL int m0_fom_simples_init(void)
{
	return m0_reqh_service_type_register(&fom_simple_rstype);
}

M0_INTERNAL void m0_fom_simples_fini(void)
{
	m0_reqh_service_type_unregister(&fom_simple_rstype);
}

/* fom ops */

#define FOM_SIMPLE(fom) (container_of(fom, struct m0_fom_simple, si_fom))

static int fom_simple_tick(struct m0_fom *fom)
{
	struct m0_fom_simple *simpleton = FOM_SIMPLE(fom);
	int                   result;
	int                   phase;
	bool                  simple;

	simple = fom->fo_type->ft_conf.scf_name == fom_simple_conf.scf_name;
	phase = m0_fom_phase(fom);
	M0_ASSERT(ergo(simple, phase == M0_FOM_PHASE_INIT));
	if (m0_reqh_state_get(m0_fom_reqh(fom)) <= M0_REQH_ST_NORMAL)
		result = simpleton->si_tick(fom, simpleton->si_data, &phase);
	else
		result = -ENOENT;
	M0_ASSERT(ergo(simple, phase == M0_FOM_PHASE_INIT));
	if (result < 0) {
		phase = M0_FOM_PHASE_FINISH;
		result = M0_FSO_WAIT;
	}
	m0_fom_phase_set(fom, phase);
	M0_ASSERT(M0_IN(result, (M0_FSO_WAIT, M0_FSO_AGAIN)));
	return result;
}

static size_t fom_simple_locality_get(const struct m0_fom *fom)
{
	return FOM_SIMPLE(fom)->si_locality;
}

static void fom_simple_fini(struct m0_fom *fom)
{
	struct m0_fom_simple *simpleton = FOM_SIMPLE(fom);

	m0_fom_fini(fom);
	if (simpleton->si_free != NULL)
		simpleton->si_free(simpleton);
}

static const struct m0_fom_ops fom_simple_ops = {
	.fo_fini          = &fom_simple_fini,
	.fo_tick          = &fom_simple_tick,
	.fo_home_locality = &fom_simple_locality_get
};

/* fom state machine. */

static const struct m0_fom_type_ops fom_simple_ft_ops = {
};

static struct m0_sm_state_descr fom_simple_phases[] = {
	[M0_FOM_PHASE_INIT] = {
		.sd_name      = "working",
		.sd_allowed   = M0_BITS(M0_FOM_PHASE_INIT, M0_FOM_PHASE_FINISH),
		.sd_flags     = M0_SDF_INITIAL
	},
	[M0_FOM_PHASE_FINISH] = {
		.sd_name      = "done",
		.sd_flags     = M0_SDF_TERMINAL
	}
};

static struct m0_sm_conf fom_simple_conf = {
	.scf_name      = "simple fom",
	.scf_nr_states = ARRAY_SIZE(fom_simple_phases),
	.scf_state     = fom_simple_phases
};


/* reqh service. */

static int fom_simple_service_start(struct m0_reqh_service *service)
{
	return 0;
}

static void fom_simple_service_prepare_to_stop(struct m0_reqh_service *service)
{
}

static void fom_simple_service_stop(struct m0_reqh_service *service)
{
}

static void fom_simple_service_fini(struct m0_reqh_service *service)
{
	m0_free(service);
}

static const struct m0_reqh_service_ops fom_simple_service_ops = {
	.rso_start           = &fom_simple_service_start,
	.rso_prepare_to_stop = &fom_simple_service_prepare_to_stop,
	.rso_stop            = &fom_simple_service_stop,
	.rso_fini            = &fom_simple_service_fini
};

static int fom_simple_service_allocate(struct m0_reqh_service **out,
				       const struct m0_reqh_service_type *stype)
{
	struct m0_reqh_service *service;

	M0_ALLOC_PTR(service);
	if (service != NULL)
		service->rs_ops = &fom_simple_service_ops;
	*out = service;
	return service != NULL ? 0 : M0_ERR(-ENOMEM);
}

static const struct m0_reqh_service_type_ops fom_simple_rsops = {
	.rsto_service_allocate = &fom_simple_service_allocate
};

static struct m0_reqh_service_type fom_simple_rstype = {
	.rst_name    = "simple-fom-service",
	.rst_ops     = &fom_simple_rsops,
	.rst_level   = M0_RS_LEVEL_NORMAL
};

#undef M0_TRACE_SUBSYSTEM

/** @} end of fom group */

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
