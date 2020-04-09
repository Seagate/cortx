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
 * Original author: Rajanikant Chirmade <rajanikant_chirmade@xyratex.com>
 * Original creation date: 06/14/2013
 */

/**
   @page DLD-stats-svc  Stats Service

   - @ref DLD-stats-svc-ovw
   - @ref DLD-stats-svc-def
   - @ref DLD-stats-svc-req
   - @subpage DLD-stats-svc-fspecs
   - @ref DLD-stats-svc-lspecs
      - @ref DLD-stats-svc-lspecs-stats_list
      - @ref DLD-stats-svc-lspec-state
      - @ref DLD-stats-svc-lspec-thread
      - @ref DLD-stats-svc-lspec-service-registration
      - @ref DLD-stats-svc-lspec-numa
      - @ref DLD-stats-svc-lspec-depends
      - @ref DLD-stats-svc-lspec-conformance
   - @ref DLD-stats-svc-ut
   - @ref DLD-stats-svc-it
   - @ref DLD-stats-svc-st
   - @ref DLD-stats-svc-O
   - @ref DLD-stats-svc-ref

   <hr>
   @section DLD-stats-svc-ovw Overview
   Stats service provides the following functionality:
   - Updates of in-memory stats objects on requests from mero nodes
   - Provides stats information from in-memory stats objects to cluster
     administrative utilities/console on their query.

   <hr>
   @section DLD-stats-svc-def Definitions
   - <b>Stats Service</b> Stats service which processes mero statistics
     update/query requests.
   - <b>Stats Object</b> Stats in-memory object which represents a statistics
     matrix.
   - <b>Stats Update</b> Stats update request which updates stats object values.
   - <b>Stats Query</b> Stats query request which returns latest updated
     stats objects value.
   - <b>Stats Clients</b> Mero administrative console/utilities.

   <hr>
   @section DLD-stats-svc-req Requirements
   - <b>r.stats_service.in_memory_objects</b> Maintains in-memory statistic
     objects. It is list of stats object.
   - <b>r.stats_service.update</b> It updates requested stats object values.
   - <b>r.stats_service.query</b> It returns requested stats object values to
     stats clients.

   <hr>
   @section DLD-stats-svc-lspecs Logical Specification

   @subsection DLD-stats-svc-lspecs-stats_list Stats Object List
   In memory stats object list does not contain any object initially. Stats
   FOM update respective stats object. object is created if not found in
   stats list.

   @subsection DLD-stats-svc-lspec-state State Transitions
   State diagram for stats_update FOM:
   @dot
   digraph {
	size = "5,10"
	node [shape=record, fontsize=10]
	S0 [label="Init Update FOM"]
	S1 [label="Update Stats object"]
	S2 [label="Finalise FOM"]
	S0 -> S1 [label="Stats Update fom initiaized"]
	S1 -> S2 [label="Stats object updated"]
   }
   @enddot

   State diagram for stats_query FOM:
   @dot
   digraph {
	size = "5,10"
	node [shape=record, fontsize=10]
	S0 [label="Init Query FOM"]
	S1 [label="Read stats objects and set reply FOP"]
	S2 [label="Finalise FOM"]
	S0 -> S1 [label="Stats object found"]
	S1 -> S2 [label="Set reply FOP"]
   }
   @enddot

   @subsection DLD-stats-svc-lspec-thread Threading and Concurrency Model
   Stats service FOMs(updates/query) are executed by request handler's
   locality thread. Since same locality is assigned to all these FOMs, no
   other serialization is required for stats service.

   @subsection DLD-stats-svc-lspec-service-registration Service Registration
   Stats service type definition :

   struct m0_reqh_service_type m0_stats_svc_type = {
	.rst_name     = "M0_CST_STATS",
	.rst_ops      = &stats_service_type_ops,
	.rst_level    = M0_RS_LEVEL_NORMAL,
	.rst_typecode = M0_CST_STATS,
   };

   Stats service type initialization/finalization :

   Stats service registers/unregisters its service type with request handler
   using interfaces m0_stats_svc_init()/ m0_stats_svc_fini() during
   Mero system initialization and finalization (m0_init()/ m0_fini()).

   @subsection DLD-stats-svc-lspec-numa NUMA optimizations
   Update FOMs and query FOMs are executed in the same locality. Update/Query
   FOMs access in-memory list of statistic objects. Since both type of FOMs
   executed by same localiy threads it gets benifis of locality.

   @subsection DLD-stats-svc-lspec-depends Dependencies
   - <b>r.reqh</b> : Request handler's locality thread execute stats service
   FOMs(update/query).

   @subsection DLD-stats-svc-lspec-conformance Conformance
   - <b>i.stats_service.in_memory_objects</b> It implements list of stats
     object to keep stats run time.
   - <b>i.stats_service.update</b> It implements stats_update FOM.
   - <b>i.stats_service.query</b> It implements stats_query FOM.

   <hr>
   @section DLD-stats-svc-ut Unit Tests
   -# Test update request for stats object not present with stats service
      stats list with single stats parameter
   -# Test update request for stats object present with stats service stats
      list with single stats parameter
   -# Test update request for stats object with multiple stats parameters
   -# Test query for stats object with single stats parameter
   -# Test query for stats object with multiple stats parameters

   <hr>
   @section DLD-stats-svc-it Integration Test
   Mero nodes send stats (different 2-3 types) to stats service,
   verification of stats sent by Mero node with respective stats object.

   <hr>
   @section DLD-stats-svc-st System Test
   This tests whole monitoring infrastructure by running some sample IOs on
   on Mero and verify statistics collected by querying statistic service using
   client/monitoring utility.

   <hr>
   @section DLD-stats-svc-O Analysis
   - Since all the stats objects are in-memory objects, query and update are
     fast.
   - Since stats service is just a book keeping of stats information into
     in-memory object list. Since it does not do any computations of stats
     values, it does not creates CPU usage overhead for mero nodes.

   <hr>
   @section DLD-stats-svc-ref References
   - <a href="https://docs.google.com/a/xyratex.com/document/d/
14uPeE0mNkRu3oF32Ys_EnpvSZtGWbf8hviPHTBTOXso/edit">
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_STATS
#include "lib/trace.h"
#include "lib/tlist.h"
#include "lib/assert.h"
#include "lib/misc.h"
#include "lib/memory.h"
#include "rpc/item.h"
#include "fop/fop_item_type.h"
#include "rpc/rpc_opcodes.h"
#include "stats/stats_srv.h"
#include "fop/fop.h"
#include "fop/fom_generic.h"
#include "reqh/reqh_service.h"
#include "stats/stats_fops.h"
#include "stats/stats_fops_xc.h"

M0_TL_DESCR_DEFINE(stats, "statistic objects", M0_INTERNAL, struct m0_stats,
		   s_linkage, s_magic, M0_STATS_MAGIC, M0_STATS_HEAD_MAGIC);
M0_TL_DEFINE(stats, M0_INTERNAL, struct m0_stats);

static const struct m0_bob_type stats_svc_bob = {
	.bt_name         = "stats svc",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct stats_svc, ss_magic),
	.bt_magix        = M0_STATS_SVC_MAGIC,
	.bt_check        = NULL
};

M0_BOB_DEFINE(static, &stats_svc_bob, stats_svc);

const struct m0_fom_type_ops stats_update_fom_type_ops;
const struct m0_fom_type_ops stats_query_fom_type_ops;
const struct m0_sm_conf stats_update_fom_sm_conf;
const struct m0_sm_conf stats_query_fom_sm_conf;

#define SUM_DATA_SIZE(sum_data) (sum_data->ss_data.se_nr * sizeof(uint64_t))

/*
 * Stats Service
 */
static bool stats_svc_invariant(const struct stats_svc *svc)
{
	return stats_svc_bob_check(svc);
}

/**
 * The rso_start methods to start stats service.
 */
static int stats_svc_rso_start(struct m0_reqh_service *service)
{
	M0_PRE(m0_reqh_service_state_get(service) == M0_RST_STARTING);
	return 0;
}

/**
 * The rso_stop method to stop Stats service.
 */
static void stats_svc_rso_stop(struct m0_reqh_service *service)
{
        M0_PRE(m0_reqh_service_state_get(service) == M0_RST_STOPPED);
}

/**
 * The rso_fini method to finalise the Stats service.
 */
static void stats_svc_rso_fini(struct m0_reqh_service *service)
{
	struct stats_svc *svc;
	struct m0_stats  *stats_obj;

	M0_PRE(M0_IN(m0_reqh_service_state_get(service), (M0_RST_STOPPED,
							  M0_RST_FAILED)));
	svc = bob_of(service, struct stats_svc, ss_reqhs, &stats_svc_bob);

	m0_tl_for(stats, &svc->ss_stats, stats_obj) {
		M0_ASSERT(stats_obj != NULL);

		stats_tlink_del_fini(stats_obj);
		m0_free(stats_obj->s_sum.ss_data.se_data);
		m0_free(stats_obj);
	} m0_tl_endfor;

	stats_svc_bob_fini(svc);
	m0_free(svc);
}

static const struct m0_reqh_service_ops stats_svc_ops = {
	.rso_start       = stats_svc_rso_start,
	.rso_start_async = m0_reqh_service_async_start_simple,
	.rso_stop        = stats_svc_rso_stop,
	.rso_fini        = stats_svc_rso_fini
};

static int
stats_svc_rsto_service_allocate(struct m0_reqh_service **srv,
				const struct m0_reqh_service_type *stype)
{
	struct stats_svc *svc;

	M0_PRE(srv != NULL && stype != NULL);

	M0_ALLOC_PTR(svc);
	if (svc == NULL)
		return M0_ERR_INFO(-ENOMEM, "Failed to allocate memory.");

	*srv = &svc->ss_reqhs;
	(*srv)->rs_type = stype;
	(*srv)->rs_ops = &stats_svc_ops;

	stats_svc_bob_init(svc);
	stats_tlist_init(&svc->ss_stats);

	M0_POST(stats_svc_invariant(svc));

	return 0;
}

static const struct m0_reqh_service_type_ops stats_service_type_ops = {
	.rsto_service_allocate = stats_svc_rsto_service_allocate,
};

struct m0_reqh_service_type m0_stats_svc_type = {
	.rst_name     = "M0_CST_STATS",
	.rst_ops      = &stats_service_type_ops,
	.rst_level    = M0_RS_LEVEL_NORMAL,
	.rst_typecode = M0_CST_STATS,
};

/*
 * Public interfaces
 */
M0_INTERNAL int m0_stats_svc_init(void)
{
	return m0_reqh_service_type_register(&m0_stats_svc_type);

}

M0_INTERNAL void m0_stats_svc_fini(void)
{
	m0_reqh_service_type_unregister(&m0_stats_svc_type);
}

/*
 * Stats Update FOM.
 */
static int stats_update_fom_create(struct m0_fop  *fop, struct m0_fom **out,
				   struct m0_reqh *reqh);
static int stats_update_fom_tick(struct m0_fom *fom);
static void stats_update_fom_fini(struct m0_fom *fom);
static size_t stats_fom_home_locality(const struct m0_fom *fom);

/**
 * Stats update FOM operation vector.
 */
struct m0_fom_ops stats_update_fom_ops = {
	.fo_tick          = stats_update_fom_tick,
	.fo_home_locality = stats_fom_home_locality,
	.fo_fini          = stats_update_fom_fini
};

/**
 * Stats update FOP type operation vector.
 */
const struct m0_fom_type_ops stats_update_fom_type_ops = {
	.fto_create = stats_update_fom_create,
};

struct m0_sm_state_descr stats_update_phases[] = {
	[STATS_UPDATE_FOM_INIT] = {
		.sd_flags = M0_SDF_INITIAL,
		.sd_name = "Init",
		.sd_allowed = M0_BITS(STATS_UPDATE_FOM_UPDATE_OBJECT)
	},
	[STATS_UPDATE_FOM_UPDATE_OBJECT] = {
		.sd_name = "Update",
		.sd_allowed = M0_BITS(STATS_UPDATE_FOM_FINISH)
	},
	[STATS_UPDATE_FOM_FINISH] = {
		.sd_flags = M0_SDF_TERMINAL,
		.sd_name = "Finish",
		.sd_allowed = 0
	},
};

const struct m0_sm_conf stats_update_fom_sm_conf = {
	.scf_name      = "stats-update-fom-sm",
	.scf_nr_states = ARRAY_SIZE(stats_update_phases),
	.scf_state     = stats_update_phases
};

static bool stats_invariant(const struct m0_stats *stats)
{
	return stats->s_magic == M0_STATS_MAGIC;
}

M0_INTERNAL struct m0_stats *m0_stats_get(struct m0_tl *stats_list, uint64_t id)
{
	M0_PRE(stats_list != NULL);

	return m0_tl_find(stats, stats_obj, stats_list,
			  (M0_ASSERT(stats_invariant(stats_obj)),
			   stats_obj->s_sum.ss_id == id));
}

static int stats_sum_copy(struct m0_stats_sum *s, struct m0_stats_sum *d)
{
	M0_PRE(s != NULL && d != NULL);
	if (d->ss_data.se_data == 0) {
		M0_ALLOC_ARR(d->ss_data.se_data, s->ss_data.se_nr);
		if (d->ss_data.se_data == NULL)
			return M0_ERR(-ENOMEM);
	}
	d->ss_id = s->ss_id;
	d->ss_data.se_data = s->ss_data.se_data;
	memcpy(d->ss_data.se_data, s->ss_data.se_data, SUM_DATA_SIZE(d));
	return 0;
}

static int stats_add(struct m0_tl *stats_list, struct m0_stats_sum *sum)
{
	struct m0_stats *new_stats;
	int              rc;

	M0_PRE(stats_list != NULL);
	M0_PRE(sum != NULL);

	/**
          * @todo
          * Is it required to check valid m0_stats_sum::ss_id from some
          * some list? How ? need some global stats id list.
          */

	M0_ALLOC_PTR(new_stats);
	if (new_stats == NULL)
		return M0_ERR_INFO(-ENOMEM, "Failed to allocate memory.");

	new_stats->s_magic = M0_STATS_MAGIC;
	rc = stats_sum_copy(sum, &new_stats->s_sum);
	if (rc != 0) {
		m0_free(new_stats);
		return M0_RC(rc);
	}

	stats_tlink_init(new_stats);
	stats_tlist_add_tail(stats_list, new_stats);

	return 0;
}

static int stats_update(struct m0_fom *fom)
{
	struct m0_stats_update_fop *ufop;
	struct stats_svc           *svc;
	int			    i;

	M0_PRE(fom != NULL);

	ufop = m0_stats_update_fop_get(fom->fo_fop);

	svc = container_of(fom->fo_service, struct stats_svc, ss_reqhs);
	stats_svc_invariant(svc);

	for (i = 0; i < ufop->suf_stats.sf_nr; ++i) {
		struct m0_stats_sum *sum = &(ufop->suf_stats.sf_stats[i]);
		struct m0_stats     *stats_obj = m0_stats_get(&svc->ss_stats,
							      sum->ss_id);

		if (stats_obj != NULL) {
			stats_sum_copy(sum, &stats_obj->s_sum);
		} else {
			int rc = stats_add(&svc->ss_stats, sum);
			if (rc != 0)
				return M0_RC(rc);
		}
	}

	return 0;
}

/**
 * Create and initiate stats update FOM.
 */
static int stats_update_fom_create(struct m0_fop  *fop, struct m0_fom **out,
				   struct m0_reqh *reqh)
{
	struct stats_update_fom *stats_ufom;
	struct m0_fom           *fom;

	M0_PRE(fop != NULL);
	M0_PRE(out != NULL);

	M0_ALLOC_PTR(stats_ufom);
	if (stats_ufom == NULL)
		return M0_ERR_INFO(-ENOMEM, "Failed to allocate memory.");

	fom = &stats_ufom->suf_fom;
	m0_fom_init(fom, &(fop->f_type->ft_fom_type), &stats_update_fom_ops,
		    fop, NULL, reqh);

	stats_ufom->suf_magic = M0_STATS_UPDATE_FOM_MAGIC;

	*out = fom;

	return 0;
}

/**
 * State transition function for stats update FOM.
 */
static int stats_update_fom_tick(struct m0_fom *fom)
{
	int rc = 0;

	M0_PRE(fom != NULL);

	switch (m0_fom_phase(fom)) {
	case STATS_UPDATE_FOM_INIT:
		m0_fom_phase_set(fom, STATS_UPDATE_FOM_UPDATE_OBJECT);
		rc = M0_FSO_AGAIN;
		break;
	case STATS_UPDATE_FOM_UPDATE_OBJECT:
		rc = stats_update(fom);
		m0_fom_phase_set(fom, STATS_UPDATE_FOM_FINISH);
		/* No need to execute generic phases. */
		rc = M0_FSO_WAIT;
		break;
	default:
		M0_IMPOSSIBLE("Bad phase.");
	}

	return M0_RC(rc);
}

/**
 * Finalise stats update FOM.
 */
static void stats_update_fom_fini(struct m0_fom *fom)
{
	struct stats_update_fom *ufom;

	M0_PRE(fom != NULL);
	ufom = container_of(fom, struct stats_update_fom, suf_fom);

	m0_fom_fini(fom);
	m0_free(ufom);
}

/**
 * Get stats update FOM locality.
 */
static size_t stats_fom_home_locality(const struct m0_fom *fom)
{
	return 1; /* all stats update FOM run in same locality */
}

/*
 * Stats Query FOM
 */
static int    stats_query_fom_create(struct m0_fop  *fop, struct m0_fom **out,
				     struct m0_reqh *reqh);
static int    stats_query_fom_tick(struct m0_fom *fom);
static void   stats_query_fom_fini(struct m0_fom *fom);

/**
 * Stats update FOM operation vector.
 */
static const struct m0_fom_ops stats_query_fom_ops = {
	.fo_tick          = stats_query_fom_tick,
	.fo_home_locality = stats_fom_home_locality,
	.fo_fini          = stats_query_fom_fini
};

/**
 * Stats update FOP type operation vector.
 */
const struct m0_fom_type_ops stats_query_fom_type_ops = {
	.fto_create = stats_query_fom_create,
};

struct m0_sm_state_descr stats_query_phases[] = {
	[STATS_QUERY_FOM_READ_OBJECT] = {
		.sd_name    = "Read",
		.sd_allowed = M0_BITS(M0_FOPH_SUCCESS, M0_FOPH_FAILURE)
	}
};

const struct m0_sm_conf stats_query_fom_sm_conf = {
	.scf_name      = "stats-query-fom-sm",
	.scf_nr_states = ARRAY_SIZE(stats_query_phases),
	.scf_state     = stats_query_phases
};

static int read_stats(struct m0_fom *fom)
{
	struct m0_stats_query_fop     *qfop;
	struct stats_svc              *svc;
	struct m0_stats_query_rep_fop *rep_fop;
	int			       i;
	int			       rc = 0;

	M0_PRE(fom != NULL);

	qfop = m0_stats_query_fop_get(fom->fo_fop);
	M0_ASSERT(qfop->sqf_ids.se_nr != 0);

	svc = container_of(fom->fo_service, struct stats_svc, ss_reqhs);
	stats_svc_invariant(svc);

	rep_fop = m0_stats_query_rep_fop_get(fom->fo_rep_fop);
	rep_fop->sqrf_stats.sf_nr = qfop->sqf_ids.se_nr;

	for (i = 0; i < qfop->sqf_ids.se_nr; ++i) {
		struct m0_stats           *stats_obj =
			m0_stats_get(&svc->ss_stats, qfop->sqf_ids.se_data[i]);

		/* Continue getting stats for next id */
		if (stats_obj == NULL) {
			rep_fop->sqrf_stats.sf_stats[i].ss_data.se_nr = 0;
			continue;
		}

		rc = stats_sum_copy(&stats_obj->s_sum,
				    &rep_fop->sqrf_stats.sf_stats[i]);
		if (rc != 0) {
#undef REP_STATS_SUM_DATA
#define REP_STATS_SUM_DATA(rep_fop, i) \
	(rep_fop->sqrf_stats.sf_stats[i].ss_data.se_data)

			for (;i >= 0; --i)
				m0_free(REP_STATS_SUM_DATA(rep_fop, i));
			m0_free(rep_fop->sqrf_stats.sf_stats);
			break;
		}
	}

	return M0_RC(rc);
}

/**
 * Create and initialize stats query FOM.
 */
static int stats_query_fom_create(struct m0_fop  *fop, struct m0_fom **out,
				  struct m0_reqh *reqh)
{
	struct stats_query_fom	      *stats_qfom;
	struct m0_fom		      *fom;
	struct m0_fop		      *reply_fop;
	struct m0_stats_query_rep_fop *qrep_fop;
	struct m0_stats_query_fop     *q_fop;
	int			       rc = 0;

	M0_PRE(fop != NULL);
	M0_PRE(out != NULL);

	M0_ALLOC_PTR(stats_qfom);
	if (stats_qfom == NULL)
		return M0_ERR_INFO(-ENOMEM, "Failed to allocate memory.");

	M0_ALLOC_PTR(qrep_fop);
	if (qrep_fop == NULL) {
		rc = -ENOMEM;
		goto free_qfom;
	}

	q_fop = m0_stats_query_fop_get(fop);
	qrep_fop->sqrf_stats.sf_nr = q_fop->sqf_ids.se_nr;

	M0_ALLOC_ARR(qrep_fop->sqrf_stats.sf_stats, q_fop->sqf_ids.se_nr);
	if (qrep_fop->sqrf_stats.sf_stats == NULL) {
		rc = -ENOMEM;
		goto free_qrep_fop;
	}

	reply_fop = m0_fop_alloc(&m0_fop_stats_query_rep_fopt, qrep_fop,
				 m0_fop_rpc_machine(fop));
	if (reply_fop == NULL) {
		rc = -ENOMEM;
		goto free_qrep_fop_stats;
	}

	fom = &stats_qfom->sqf_fom;
	m0_fom_init(fom, &fop->f_type->ft_fom_type, &stats_query_fom_ops, fop,
		    reply_fop, reqh);

	stats_qfom->sqf_magic = M0_STATS_QUERY_FOM_MAGIC;

	*out = fom;

	return 0;

free_qrep_fop_stats:
	m0_free(qrep_fop->sqrf_stats.sf_stats);
free_qrep_fop:
	m0_free(qrep_fop);
free_qfom:
	m0_free(stats_qfom);

	return M0_ERR_INFO(rc, "Failed to create query FOM");
}

/**
 * State transition function for stats query FOM.
 */
static int stats_query_fom_tick(struct m0_fom *fom)
{
	int rc = 0;

	M0_PRE(fom != NULL);

	/**
	 * @note only M0_FOPH_QUEUE_REPLY & M0_FOPH_QUEUE_REPLY_WAIT are
	 * required.
	 */
	if (m0_fom_phase(fom) < M0_FOPH_NR)
		return m0_fom_tick_generic(fom);

	switch (m0_fom_phase(fom)) {
	case STATS_QUERY_FOM_READ_OBJECT:
		rc = read_stats(fom);
		m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
		rc = M0_FSO_AGAIN;
		break;
	default:
		M0_IMPOSSIBLE("Phase not defined.");
	}

	return M0_RC(rc);
}

/**
 * Finalise stats query FOM.
 */
static void stats_query_fom_fini(struct m0_fom *fom)
{
	struct stats_query_fom *qfom;

	M0_PRE(fom != NULL);
	qfom = container_of(fom, struct stats_query_fom, sqf_fom);

	m0_fom_fini(fom);
	m0_free(qfom);
}

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
