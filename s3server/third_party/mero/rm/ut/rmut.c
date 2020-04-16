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
 * Original author: Rajesh Bhalerao <rajesh_bhalerao@xyratex.com>
 * Original creation date: 07/20/2012
 */

#include "lib/memory.h"
#include "lib/cookie.h"
#include "lib/misc.h"
#include "lib/string.h"           /* m0_strdup */
#include "lib/finject.h"          /* m0_fi_enable_once */
#include "rpc/rpclib.h"           /* m0_rpc_client_connect */
#include "ut/ut.h"
#include "lib/ub.h"
#include "fid/fid.h"              /* m0_fid, m0_fid_tset */
#include "conf/obj.h"
#include "conf/obj_ops.h"         /* m0_conf_obj_find */
#include "conf/dir.h"             /* m0_conf_dir_add */
#include "rm/rm.h"
#include "rm/rm_service.h"        /* m0_rms_type */
#include "rm/rm_internal.h"
#include "rm/ut/rings.h"
#include "rm/ut/rmut.h"

extern const struct m0_tl_descr m0_remotes_tl;

const char *serv_addr[] = { "0@lo:12345:34:1",
			    "0@lo:12345:34:2",
			    "0@lo:12345:34:3"
};

const int cob_ids[] = { 20, 30, 40 };
/*
 * Test variable(s)
 */
struct rm_ut_data rm_test_data;
struct rm_ctx     rm_ctxs[SERVER_NR];
struct m0_chan    rm_ut_tests_chan;
struct m0_mutex   rm_ut_tests_chan_mutex;

extern void rm_api_test(void);
extern void local_credits_test(void);
extern void rm_fom_funcs_test(void);
extern void rm_fop_funcs_test(void);
extern void flock_test(void);
extern void rm_group_test(void);
extern bool m0_rm_ur_tlist_is_empty(const struct m0_tl *list);
extern void m0_remotes_tlist_del(struct m0_rm_remote *other);
extern void rmsvc(void);

struct rm_ut_data test_data;

void rm_test_owner_capital_raise(struct m0_rm_owner *owner,
				 struct m0_rm_credit *credit)
{
	m0_rm_credit_init(credit, owner);
	/* Set the initial capital */
	credit->cr_ops->cro_initial_capital(credit);
	m0_rm_owner_selfadd(owner, credit);
	M0_UT_ASSERT(!m0_rm_ur_tlist_is_empty(&owner->ro_borrowed));
	M0_UT_ASSERT(!m0_rm_ur_tlist_is_empty(&owner->ro_owned[OWOS_CACHED]));
}

void rm_utdata_owner_windup_fini(struct rm_ut_data *data)
{
	int rc ;

	m0_rm_owner_windup(rm_test_data.rd_owner);
	rc = m0_rm_owner_timedwait(rm_test_data.rd_owner, M0_BITS(ROS_FINAL),
				   M0_TIME_NEVER);
	M0_ASSERT(rc == 0);
	rm_utdata_fini(&rm_test_data, OBJ_OWNER);
}
/*
 * Recursive call to initialise object hierarchy
 *
 * XXX TODO: Use module/module.h API.
 */
void rm_utdata_init(struct rm_ut_data *data, enum obj_type type)
{
	M0_UT_ASSERT(data != NULL);

	switch (type) {
		case OBJ_DOMAIN:
			/* Initialise test_domain */
			m0_rm_domain_init(&data->rd_dom);
			break;
		case OBJ_RES_TYPE:
			rm_utdata_init(data, OBJ_DOMAIN);
			/* Register test resource type */
			data->rd_ops->rtype_set(data);
			break;
		case OBJ_RES:
			rm_utdata_init(data, OBJ_RES_TYPE);
			data->rd_ops->resource_set(data);
			break;
		case OBJ_OWNER:
			rm_utdata_init(data, OBJ_RES);
			data->rd_ops->owner_set(data);
			break;
		default:
			M0_IMPOSSIBLE("Invalid value of obj_type");
	}
}

/*
 * Recursive call to finalise object hierarchy
 *
 * XXX TODO: Use module/module.h API.
 */
void rm_utdata_fini(struct rm_ut_data *data, enum obj_type type)
{
	struct m0_rm_remote *other;

	M0_UT_ASSERT(data != NULL);

	switch (type) {
		case OBJ_DOMAIN:
			/* Finalise test_domain */
			m0_rm_domain_fini(&data->rd_dom);
			break;
		case OBJ_RES_TYPE:
			/* De-register test resource type */
			data->rd_ops->rtype_unset(data);
			rm_utdata_fini(data, OBJ_DOMAIN);
			break;
		case OBJ_RES:
			m0_tl_teardown(m0_remotes,
				       &data->rd_res->r_remote, other) {
				m0_rm_remote_fini(other);
				m0_free(other);
			}
			data->rd_ops->resource_unset(data);
			rm_utdata_fini(data, OBJ_RES_TYPE);
			break;
		case OBJ_OWNER:
			data->rd_ops->owner_unset(data);
			rm_utdata_fini(data, OBJ_RES);
			break;
		default:
			M0_IMPOSSIBLE("Invalid value of obj_type");
	}
}

struct m0_reqh_service *rmservice[SERVER_NR];

void rm_ctx_init(struct rm_ctx *rmctx)
{
	enum rm_server id;
	int            rc;

	/* Determine `id'. */
	for (id = 0; id < SERVER_NR && rmctx != &rm_ctxs[id]; ++id)
		;
	M0_PRE(id != SERVER_NR);

	*rmctx = (struct rm_ctx){
		.rc_id        = id,
		.rc_rmach_ctx = {
			.rmc_cob_id.id = cob_ids[id],
			.rmc_ep_addr   = serv_addr[id]
		}
	};
	m0_ut_rpc_mach_init_and_add(&rmctx->rc_rmach_ctx);
	m0_mutex_init(&rmctx->rc_mutex);
	rc = m0_reqh_service_setup(&rmservice[rmctx->rc_id],
				   &m0_rms_type, &rmctx->rc_rmach_ctx.rmc_reqh,
				   NULL, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_chan_init(&rmctx->rc_chan, &rmctx->rc_mutex);
	m0_clink_init(&rmctx->rc_clink, NULL);
	m0_clink_add_lock(&rmctx->rc_chan, &rmctx->rc_clink);
}

void rm_ctx_fini(struct rm_ctx *rmctx)
{
	m0_clink_del_lock(&rmctx->rc_clink);
	m0_clink_fini(&rmctx->rc_clink);
	m0_chan_fini_lock(&rmctx->rc_chan);
	m0_mutex_fini(&rmctx->rc_mutex);
	m0_ut_rpc_mach_fini(&rmctx->rc_rmach_ctx);
}

void rm_ctx_connect(struct rm_ctx *src, const struct rm_ctx *dest)
{
	enum { MAX_RPCS_IN_FLIGHT = 15 };
	int rc;

	rc = m0_rpc_client_connect(&src->rc_conn[dest->rc_id],
				   &src->rc_sess[dest->rc_id],
				   &src->rc_rmach_ctx.rmc_rpc,
				   dest->rc_rmach_ctx.rmc_ep_addr,
				   NULL, MAX_RPCS_IN_FLIGHT,
				   M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
}

void rm_ctx_disconnect(struct rm_ctx *src, const struct rm_ctx *dest)
{
	int rc;

	rc = m0_rpc_session_destroy(&src->rc_sess[dest->rc_id], M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);

	rc = m0_rpc_conn_destroy(&src->rc_conn[dest->rc_id], M0_TIME_NEVER);
	M0_UT_ASSERT(rc == 0);
}

void rm_ctx_server_start(enum rm_server srv_id)
{
	struct m0_rm_remote *creditor;
	struct m0_rm_owner  *owner;
	struct rm_ut_data   *data = &rm_ctxs[srv_id].rc_test_data;
	enum rm_server       cred_id = rm_ctxs[srv_id].creditor_id;
	enum rm_server       debtr_id;
	uint32_t             debtors_nr = rm_ctxs[srv_id].rc_debtors_nr;
	uint32_t             i;

	rm_utdata_init(data, OBJ_OWNER);
	owner = data->rd_owner;

	/*
	 * If creditor id is valid, do creditor setup.
	 * If there is no creditor, this server is original owner.
	 * For original owner, raise capital.
	 */
	if (cred_id != SERVER_INVALID) {
		rm_ctx_connect(&rm_ctxs[srv_id], &rm_ctxs[cred_id]);
		M0_ALLOC_PTR(creditor);
		M0_UT_ASSERT(creditor != NULL);
		m0_rm_remote_init(creditor, owner->ro_resource);
		creditor->rem_state = REM_SERVICE_LOCATED;
		creditor->rem_session = &rm_ctxs[srv_id].rc_sess[cred_id];
		owner->ro_creditor = creditor;
	} else
		rm_test_owner_capital_raise(owner, &data->rd_credit);

	for (i = 0; i < debtors_nr; ++i) {
		debtr_id = rm_ctxs[srv_id].debtor_id[i];
		if (debtr_id != SERVER_INVALID)
			rm_ctx_connect(&rm_ctxs[srv_id], &rm_ctxs[debtr_id]);
	}

}

void rm_ctx_server_owner_windup(enum rm_server srv_id)
{
	struct m0_rm_owner *owner = rm_ctxs[srv_id].rc_test_data.rd_owner;
	int                 rc;

	if (rm_ctxs[srv_id].rc_is_dead)
		m0_fi_enable_once("owner_finalisation_check", "drop_loans");
	m0_rm_owner_windup(owner);
	rc = m0_rm_owner_timedwait(owner, M0_BITS(ROS_FINAL),
				   M0_TIME_NEVER);
	M0_ASSERT(rc == 0);
	rm_ctx_server_windup(srv_id);
}

void rm_ctx_server_windup(enum rm_server srv_id)
{
	struct m0_rm_owner *owner = rm_ctxs[srv_id].rc_test_data.rd_owner;
	enum rm_server      cred_id = rm_ctxs[srv_id].creditor_id;
	struct m0_reqh     *reqh = &rm_ctxs[srv_id].rc_rmach_ctx.rmc_reqh;

	m0_chan_lock(&reqh->rh_conf_cache_exp);
	if (cred_id != SERVER_INVALID) {
		M0_UT_ASSERT(owner->ro_creditor != NULL);
		m0_rm_remote_fini(owner->ro_creditor);
		m0_free0(&owner->ro_creditor);
	}
	rm_utdata_fini(&rm_ctxs[srv_id].rc_test_data, OBJ_OWNER);
	m0_chan_unlock(&reqh->rh_conf_cache_exp);
}

void rm_ctx_server_stop(enum rm_server srv_id)
{
	enum rm_server cred_id = rm_ctxs[srv_id].creditor_id;
	enum rm_server debtr_id;
	uint32_t       debtors_nr = rm_ctxs[srv_id].rc_debtors_nr;
	uint32_t       i;

	if (cred_id != SERVER_INVALID)
		rm_ctx_disconnect(&rm_ctxs[srv_id], &rm_ctxs[cred_id]);
	for (i = 0; i < debtors_nr; ++i) {
		debtr_id = rm_ctxs[srv_id].debtor_id[i];
		if (debtr_id != SERVER_INVALID)
			rm_ctx_disconnect(&rm_ctxs[srv_id], &rm_ctxs[debtr_id]);
	}
}

void rm_ctxs_rmsvc_conf_add(struct m0_confc *confc, struct rm_ctx *rmctx)
{
	struct m0_fid           svc_fid;
	struct m0_conf_service *service;
	struct m0_conf_obj     *svc_obj;
	struct m0_fid           proc_fid;
	struct m0_conf_obj     *proc_obj;
	struct m0_conf_process *process;
	struct m0_conf_cache   *cache = &confc->cc_cache;
	int                     rc;

	m0_conf_cache_lock(cache);

	m0_fid_tset(&proc_fid, M0_CONF_PROCESS_TYPE.cot_ftype.ft_id, 1, 3);
	proc_obj = m0_conf_cache_lookup(cache, &proc_fid);
	M0_ASSERT(proc_obj != NULL);
	process = M0_CONF_CAST(proc_obj, m0_conf_process);

	m0_fid_tset(&svc_fid, M0_CONF_SERVICE_TYPE.cot_ftype.ft_id,
			0, rmctx->rc_id);
	rc = m0_conf_obj_find(cache, &svc_fid, &svc_obj);
	M0_ASSERT(rc == 0);
	M0_ASSERT(m0_conf_obj_is_stub(svc_obj));

	service = M0_CONF_CAST(svc_obj, m0_conf_service);
	service->cs_type = M0_CST_RMS;
	M0_ALLOC_ARR(service->cs_endpoints, 2);
	M0_ASSERT(service->cs_endpoints != NULL);
	service->cs_endpoints[0] = m0_strdup(rmctx->rc_rmach_ctx.rmc_ep_addr);
	M0_ASSERT(service->cs_endpoints[0] != NULL);
	service->cs_endpoints[1] = NULL;

	m0_conf_dir_add(process->pc_services, svc_obj);
	svc_obj->co_status = M0_CS_READY;

	m0_conf_cache_unlock(cache);
}

void rm_ctxs_conf_init(struct rm_ctx *rm_ctxs, int ctxs_nr)
{
	struct rm_ctx   *rmctx;
	struct m0_confc *confc;
	int              i, j;
	int              rc;
	char minimal_conf_str[] = "[5:\
	   {0x74| ((^t|1:0), 1, (11, 22), ^o|1:23, ^v|1:24, 41212,\
		   [3: \"param-0\", \"param-1\", \"param-2\"],\
		   [1: ^n|1:2], [0], [1: ^o|1:23], [1:^p|1:0], [0])},\
	   {0x70| ((^p|1:0), [1: ^o|1:23])},\
	   {0x6e| ((^n|1:2), 16000, 2, 3, 2, [1: ^r|1:3])},\
	   {0x72| ((^r|1:3), [1:3], 0, 0, 0, 0, \"addr-0\", [0])},\
	   {0x6f| ((^o|1:23), 0, [0])}]";

	for (i = 0; i < ctxs_nr; i++) {
		rmctx = &rm_ctxs[i];
		confc = m0_reqh2confc(&rmctx->rc_rmach_ctx.rmc_reqh);
		/*
		 * Initialise confc instance in reqh. Confc is necessary to
		 * track death of creditors/debtors.
		 */
		m0_fid_tset(m0_reqh2profile(&rmctx->rc_rmach_ctx.rmc_reqh),
			    M0_CONF_PROFILE_TYPE.cot_ftype.ft_id, 1, 0);
		rc = m0_confc_init(confc,
				   m0_locality0_get()->lo_grp,
				   NULL,
				   &rmctx->rc_rmach_ctx.rmc_rpc,
				   minimal_conf_str);
		M0_ASSERT(rc == 0);
		for (j = 0; j < ctxs_nr; j++) {
			rm_ctxs_rmsvc_conf_add(confc, &rm_ctxs[j]);
		}
		m0_ha_client_add(confc);
	}
}

void rm_ctxs_conf_fini(struct rm_ctx *rm_ctxs, int ctxs_nr)
{
	struct rm_ctx   *rmctx;
	struct m0_confc *confc;
	int              i;

	for (i = 0; i < ctxs_nr; i++) {
		rmctx = &rm_ctxs[i];
		confc = m0_reqh2confc(&rmctx->rc_rmach_ctx.rmc_reqh);
		m0_ha_client_del(confc);
		m0_conf_cache_lock(&confc->cc_cache);
		m0_conf_cache_clean(&confc->cc_cache, &M0_CONF_DIR_TYPE);
		m0_conf_cache_unlock(&confc->cc_cache);
		m0_confc_fini(confc);
	}
}

void loan_session_set(enum rm_server csrv_id,
		      enum rm_server dsrv_id)
{
	struct m0_rm_owner  *owner = rm_ctxs[csrv_id].rc_test_data.rd_owner;
	struct m0_rm_loan   *loan;
	struct m0_rm_credit *credit;
	struct m0_rm_remote *remote;
	struct m0_cookie     dcookie;

	M0_UT_ASSERT(!m0_rm_ur_tlist_is_empty(&owner->ro_sublet));
	m0_tl_for(m0_rm_ur, &owner->ro_sublet, credit) {
		loan = bob_of(credit, struct m0_rm_loan, rl_credit, &loan_bob);
		M0_UT_ASSERT(loan != NULL);
		m0_cookie_init(&dcookie,
			       &rm_ctxs[dsrv_id].rc_test_data.rd_owner->ro_id);
		remote = loan->rl_other;
		if (m0_cookie_is_eq(&dcookie, &remote->rem_cookie))
			remote->rem_session = &rm_ctxs[csrv_id].rc_sess[dsrv_id];
	} m0_tl_endfor;
}

void creditor_cookie_setup(enum rm_server dsrv_id,
			   enum rm_server csrv_id)
{
	struct m0_rm_owner *creditor = rm_ctxs[csrv_id].rc_test_data.rd_owner;
	struct m0_rm_owner *owner = rm_ctxs[dsrv_id].rc_test_data.rd_owner;

	m0_cookie_init(&owner->ro_creditor->rem_cookie, &creditor->ro_id);
}

void credits_are_equal(enum rm_server          srv_id,
		       enum rm_ut_credits_list list_id,
		       uint64_t                value)
{
	struct m0_rm_owner *owner = rm_ctxs[srv_id].rc_test_data.rd_owner;
	uint64_t            sum;
	struct m0_tl       *list;

	switch (list_id) {
	case RCL_CACHED:
		list = &owner->ro_owned[OWOS_CACHED];
		break;
	case RCL_HELD:
		list = &owner->ro_owned[OWOS_HELD];
		break;
	case RCL_BORROWED:
		list = &owner->ro_borrowed;
		break;
	case RCL_SUBLET:
		list = &owner->ro_sublet;
		break;
	default:
		M0_IMPOSSIBLE("Invalid credits list");
	}

	sum = m0_tl_reduce(m0_rm_ur, credit, list, 0, + credit->cr_datum);
	M0_UT_ASSERT(sum == value);
}

struct m0_ut_suite rm_ut = {
	.ts_name = "rm-ut",
	.ts_tests = {
		{ "api", rm_api_test },
		{ "lcredits", local_credits_test },
		{ "fop-funcs", rm_fop_funcs_test },
#ifndef __KERNEL__
		{ "fom-funcs", rm_fom_funcs_test },
		{ "rmsvc", rmsvc },
		{ "flock", flock_test },
		{ "group", rm_group_test },
#endif
		{ NULL, NULL }
	}
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
