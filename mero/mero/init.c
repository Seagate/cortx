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
 * Original creation date: 06/19/2010
 */

#include "fop/fop.h"
#ifndef __KERNEL__
#  include "desim/sim.h"
#endif
#include "lib/trace.h"   /* m0_trace_init */
#include "lib/thread.h"
#include "stob/type.h"
#include "stob/stob.h"
#include "ut/stob.h"
#include "net/net.h"
#include "net/bulk_emulation/mem_xprt.h"
#include "net/lnet/lnet.h"
#include "rpc/rpc.h"
#include "addb2/addb2.h"
#include "lib/finject.h"
#include "lib/locality.h"
#include "layout/layout.h"
#include "pool/pool.h"
#include "lib/processor.h"
#include "sm/sm.h"
#include "dtm/dtm.h"
#include "fol/fol.h"
#include "dtm/dtm.h"
#include "reqh/reqh.h"
#include "lib/timer.h"
#include "fid/fid.h"
#include "fis/fi_service.h"
#include "fop/fom_simple.h"
#include "fop/fom_generic.h"
#include "graph/graph.h"
#include "mero/init.h"
#include "lib/cookie.h"
#include "conf/fop.h"           /* m0_conf_fops_init, m0_confx_types_init */
#include "conf/obj.h"           /* m0_conf_obj_init */
#include "pool/policy.h"        /* m0_pver_policies_init */
#ifdef __KERNEL__
#  include "m0t1fs/linux_kernel/m0t1fs.h"
#  include "mero/linux_kernel/dummy_init_fini.h"
#  include "net/test/initfini.h" /* m0_net_test_init */
#else
#  include "be/tx_service.h"    /* m0_be_txs_register */
#  include "be/be.h"            /* m0_backend_init */
#  include "conf/confd.h"       /* m0_confd_register */
#  include "mdstore/mdstore.h"  /* m0_mdstore_mod_init */
#endif
#include "cob/cob.h"
#include "ioservice/io_fops.h"
#include "ioservice/io_service.h"
#include "iscservice/isc_service.h"
#include "mdservice/md_fops.h"
#include "mdservice/md_service.h"
#include "rm/rm_service.h"
#include "rm/rm_rwlock.h"
#include "conf/rconfc_link_fom.h" /* m0_rconfc_mod_init, m0_rconfc_mod_fini */
#include "stats/stats_srv.h"
#include "sns/sns.h"
#include "sns/parity_ops.h"
#include "sss/ss_svc.h"
#include "cm/cm.h"
#include "stats/stats_fops.h"
#include "ha/epoch.h"
#include "ha/ha.h"            /* m0_ha_mod_init */
#include "xcode/init.h"
#include "module/instance.h"  /* m0_instance_setup */
#include "clovis/clovis_internal.h"  /* m0_clovis_global_init */
#include "fdmi/fdmi.h"
#include "fdmi/service.h"
#include "fdmi/fol_fdmi_src.h"

M0_INTERNAL int m0_time_init(void);
M0_INTERNAL void m0_time_fini(void);

M0_INTERNAL int m0_memory_init(void);
M0_INTERNAL void m0_memory_fini(void);

M0_INTERNAL int m0_foms_init(void);
M0_INTERNAL void m0_foms_fini(void);

M0_INTERNAL int libm0_init(void);
M0_INTERNAL void libm0_fini(void);

M0_INTERNAL int  m0_addb2_net_module_init(void);
M0_INTERNAL void m0_addb2_net_module_fini(void);

M0_INTERNAL int  m0_addb2_global_init(void);
M0_INTERNAL void m0_addb2_global_fini(void);

M0_INTERNAL int m0_addb2_service_module_init(void);
M0_INTERNAL void m0_addb2_service_module_fini(void);

M0_INTERNAL int  m0_cas_module_init(void);
M0_INTERNAL void m0_cas_module_fini(void);

M0_INTERNAL int  m0_dix_cm_module_init(void);
M0_INTERNAL void m0_dix_cm_module_fini(void);

#ifndef __KERNEL__
M0_INTERNAL int  m0_net_sock_mod_init(void);
M0_INTERNAL void m0_net_sock_mod_fini(void);
#endif

/**
   @addtogroup init
   @{
 */

struct init_fini_call {
	int  (*ifc_init)(void);
	void (*ifc_fini)(void);
	const char *ifc_name;
};

struct init_fini_call quiesce[] = {
	{ &m0_time_init,        &m0_time_fini,        "time" },
	{ &m0_xcode_init,       &m0_xcode_fini,       "xcode" },
	{ &m0_trace_init,       &m0_trace_fini,       "trace" },
	{ &m0_fi_init,          &m0_fi_fini,          "finject" },
};

struct init_fini_call once[] = {
	{ &m0_threads_once_init, &m0_threads_once_fini, "threads" }
};

/*
  XXX dummy_init_fini.c defines dummy init() and fini() routines for
  subsystems, that are not yet ported to kernel mode.
 */
struct init_fini_call subsystem[] = {
	{ &m0_memory_init,      &m0_memory_fini,      "memory" },
	{ &m0_addb2_module_init, &m0_addb2_module_fini, "addb2" },
	{ &m0_addb2_global_init, &m0_addb2_global_fini, "addb2-global" },
	{ &libm0_init,          &libm0_fini,          "libm0" },
	{ &m0_ha_global_init ,  &m0_ha_global_fini,   "ha" },
	{ &m0_fid_init,         &m0_fid_fini,         "fid" },
	{ &m0_file_mod_init,    &m0_file_mod_fini,     "file" },
	{ &m0_cookie_global_init, &m0_cookie_global_fini, "cookie" },
	{ &m0_timers_init,      &m0_timers_fini,      "timer" },
	{ &m0_processors_init,  &m0_processors_fini,  "processors" },
	/* localities must be initialised after lib/processor.h */
	{ &m0_localities_init,  &m0_localities_fini,  "locality" },
	{ &m0_fols_init,        &m0_fols_fini,        "fol" },
	{ &m0_layouts_init,     &m0_layouts_fini,     "layout" },
	/* fops must be initialised before network, because network build fop
	   type for network descriptors. */
	{ &m0_fops_init,        &m0_fops_fini,        "fop" },
	{ &m0_foms_init,        &m0_foms_fini,        "fom" },
	{ &m0_net_init,         &m0_net_fini,         "net" },
#ifdef __KERNEL__
	{ &m0_net_test_init,    &m0_net_test_fini,    "net-test" },
#endif
	{ &m0_reqhs_init,       &m0_reqhs_fini,       "reqhs" },
	/* fom-simple must go after reqh init, because it registers a service
	   type. */
	{ &m0_fom_simples_init, &m0_fom_simples_fini, "fom-simple" },
	/* addb2-service must go after reqh init, because it registers a service
	   type. */
	{ &m0_addb2_service_module_init,
	  &m0_addb2_service_module_fini, "addb2-service" },
	{ &m0_rpc_init,         &m0_rpc_fini,         "rpc" },
	/* fom generic must be after rpc, because it initialises rpc item
	   type for generic error reply. */
	{ &m0_fom_generic_init, &m0_fom_generic_fini, "fom-generic" },
	/* addb2-net must be after rpc, because it initialises a fop type. */
	{ &m0_addb2_net_module_init, &m0_addb2_net_module_fini, "addb2-net" },
#ifndef __KERNEL__
	{ &m0_net_sock_mod_init, &m0_net_sock_mod_fini, "net/sock" },
#endif
	{ &m0_mem_xprt_init,    &m0_mem_xprt_fini,    "bulk/mem" },
	{ &m0_net_lnet_init,    &m0_net_lnet_fini,    "net/lnet" },
	{ &m0_cob_mod_init,     &m0_cob_mod_fini,     "cob" },
	{ &m0_stob_mod_init,    &m0_stob_mod_fini,    "stob" },
#ifndef __KERNEL__
	{ &m0_stob_types_init,  &m0_stob_types_fini,  "stob-types" },
	{ &m0_ut_stob_init,	&m0_ut_stob_fini,     "ut-stob" },
	{ &sim_global_init,     &sim_global_fini,     "desim" },
#endif
	{ &m0_graph_mod_init,   &m0_graph_mod_fini,   "graph" },
	{ &m0_conf_obj_init,    &m0_conf_obj_fini,    "conf-obj" },
	{ &m0_confx_types_init, &m0_confx_types_fini, "conf-xtypes" },
	{ &m0_conf_fops_init,   &m0_conf_fops_fini,   "conf-fops" },
	{ &m0_stats_fops_init,  &m0_stats_fops_fini,  "stats_fops"},
	{ &m0_rms_register,     &m0_rms_unregister,   "rmservice"},
#ifdef __KERNEL__
	{ &m0t1fs_init,         &m0t1fs_fini,         "m0t1fs" },
#else
	{ &m0_backend_init,     &m0_backend_fini,     "be" },
	{ &m0_be_txs_register,  &m0_be_txs_unregister, "be-tx-service" },
	{ &m0_confd_register,   &m0_confd_unregister, "confd" },
	/*
	 * mds should go before ios because the latter uses
	 * fsync fop registration stuff from the former.
	 */
	{ &m0_mds_register,     &m0_mds_unregister,   "mdservice"},
	{ &m0_ios_register,     &m0_ios_unregister,   "ioservice" },
	{ &m0_isc_mod_init,     &m0_isc_mod_fini,     "in-storage-compute"},
	{ &m0_iscs_register,    &m0_iscs_unregister,  "iscervice" },
	{ &m0_pools_init,       &m0_pools_fini,       "pool" },
	{ &m0_cm_module_init,   &m0_cm_module_fini,   "copy machine" },
	{ &m0_sns_init,         &m0_sns_fini,         "sns" },
	{ &m0_mdstore_mod_init, &m0_mdstore_mod_fini, "mdstore" },
	{ &m0_stats_svc_init,   &m0_stats_svc_fini,   "stats-service" },
	{ &m0_ss_svc_init,      &m0_ss_svc_fini,      "sss" },
	{ &m0_dix_cm_module_init, &m0_dix_cm_module_fini, "dix-cm" },
	{ &m0_fdms_register,    &m0_fdms_unregister,  "fdmi-service" },
#endif /* __KERNEL__ */
	{ &m0_cas_module_init,  &m0_cas_module_fini,  "cas" },
	{ &m0_parity_init,      &m0_parity_fini,      "parity_math" },
	{ &m0_dtm_global_init,  &m0_dtm_global_fini,  "dtm" },
	{ &m0_ha_mod_init,      &m0_ha_mod_fini,      "ha" },
	{ &m0_clovis_global_init, &m0_clovis_global_fini, "clovis" },
	{ &m0_rconfc_mod_init,  &m0_rconfc_mod_fini,  "rconfc" },
	{ &m0_pver_policies_init, &m0_pver_policies_fini, "pver-policies" },
	{ &m0_fis_register,     &m0_fis_unregister,   FI_SERVICE_NAME },
#ifndef __KERNEL__
	{ &m0_fdmi_init,         &m0_fdmi_fini,         "fdmi" },
	{ &m0_fol_fdmi_src_init, &m0_fol_fdmi_src_fini, "fol_fdmi_source" }
#endif
};

static void fini_nr(struct init_fini_call *arr, int nr)
{
	while (--nr >= 0) {
		if (arr[nr].ifc_fini != NULL)
			arr[nr].ifc_fini();
	}
}

static int init_nr(struct init_fini_call *arr, int nr)
{
	int i;
	int rc;

	for (i = 0; i < nr; ++i) {
		rc = arr[i].ifc_init();
		if (rc != 0) {
			m0_error_printf("subsystem %s init failed: rc = %d\n",
					arr[i].ifc_name, rc);
			fini_nr(arr, i);
			return rc;
		}
	}
	return 0;
}

/**
 * Flag protecting initialisations to be done only once per process address
 * space (or kernel).
 */
static bool initialised_once = false;

M0_INTERNAL int m0_init_once(struct m0 *instance)
{
	int rc;

	if (!initialised_once) {
		/*
		 * Bravely ignore all issues of concurrency and memory
		 * consistency models, which occupy weaker minds.
		 */
		rc = init_nr(once, ARRAY_SIZE(once));
		if (rc != 0)
			return rc;
		initialised_once = true;
	}

	rc = m0_threads_init(instance);
	if (rc != 0) {
		fini_nr(once, ARRAY_SIZE(once));
		initialised_once = false;
	}
	return rc;
}

M0_INTERNAL void m0_fini_once(void)
{
	m0_threads_fini();
	fini_nr(once, ARRAY_SIZE(once));
	initialised_once = false;
}

#if 1 /* XXX OBSOLETE */
int m0_init(struct m0 *instance)
{
	M0_PRE(M0_IS0(instance));

	m0_instance_setup(instance);
	return m0_module_init(&instance->i_self, M0_LEVEL_INST_READY);
}

void m0_fini(void)
{
	m0_module_fini(&m0_get()->i_self, M0_MODLEV_NONE);
}

int m0_resume(struct m0 *instance)
{
	return m0_module_init(&instance->i_self, M0_LEVEL_INST_READY);
}

void m0_quiesce(void)
{
	m0_module_fini(&m0_get()->i_self, M0_LEVEL_INST_QUIESCE_SYSTEM);
}

M0_INTERNAL int m0_subsystems_init(void)
{
	return init_nr(subsystem, ARRAY_SIZE(subsystem));
}

M0_INTERNAL void m0_subsystems_fini(void)
{
	fini_nr(subsystem, ARRAY_SIZE(subsystem));
}

M0_INTERNAL int m0_quiesce_init(void)
{
	return init_nr(quiesce, ARRAY_SIZE(quiesce));
}

M0_INTERNAL void m0_quiesce_fini(void)
{
	fini_nr(quiesce, ARRAY_SIZE(quiesce));
}

#endif /* XXX OBSOLETE */

/** @} end of init group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
