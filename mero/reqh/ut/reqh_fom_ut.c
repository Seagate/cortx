/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Mandar Sawant <Mandar_Sawant@xyratex.com>
 * Original creation date: 05/04/2011
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>	/* mkdir */
#include <sys/types.h>	/* mkdir */
#include <err.h>

#include "lib/memory.h"
#include "net/net.h"
#include "fop/fop.h"
#include "reqh/reqh.h"
#include "stob/stob.h"
#include "stob/ad.h"
#include "net/lnet/lnet.h"
#include "rpc/rpc.h"
#include "fop/fop_item_type.h"
#include "rpc/item.h"
#include "rpc/rpc_internal.h"
#include "fop/fom_generic.h"
#include "fop/fom_generic_xc.h"
#include "reqh/ut/io_fop_xc.h"
#include "reqh/ut/io_fop.h"
#include "rpc/rpc_opcodes.h"
#include "rpc/rpclib.h"
#include "balloc/balloc.h"
#include "mdstore/mdstore.h"
#include "stob/ad.h"		/* m0_stob_ad_cfg_make */
#include "fdmi/fdmi.h"

#include "ut/ut.h"
#include "ut/be.h"
/**
   @addtogroup reqh
   @{
 */

/**
 *  Server side structures and objects
 */

#define CLIENT_ENDPOINT_ADDR    "0@lo:12345:34:*"
#define SERVER_ENDPOINT_ADDR    "0@lo:12345:34:1"
#define SERVER_DB_NAME		"reqh_ut_stob/sdb"
#define SERVER_BDOM_LOCATION	"linuxstob:./reqh_fom_ut"
#define SERVER_BDOM_KEY         0xBAC570BD

enum {
	CLIENT_COB_DOM_ID  = 101,
	MAX_RPCS_IN_FLIGHT = 32,
	MAX_RETRIES        = 5,
};

static struct m0_stob_domain   *sdom;
static struct m0_mdstore        srv_mdstore;
static struct m0_cob_domain_id  srv_cob_dom_id;
static struct m0_rpc_machine    srv_rpc_mach;
static struct m0_be_ut_backend  ut_be;
static struct m0_be_ut_seg      ut_seg;
static struct m0_reqh_service  *reqh_ut_service;
static struct m0_reqh_service  *fdmi_ut_service;

/**
 * Global reqh object
 */
static struct m0_reqh  reqh;

/**
 * Helper structures and functions for ad stob.
 * These are used while performing a stob operation.
 */
struct reqh_ut_balloc {
	struct m0_mutex     rb_lock;
	m0_bindex_t         rb_next;
	struct m0_ad_balloc rb_ballroom;
};

static struct reqh_ut_balloc *getballoc(struct m0_ad_balloc *ballroom)
{
	return container_of(ballroom, struct reqh_ut_balloc, rb_ballroom);
}

static int reqh_ut_balloc_init(struct m0_ad_balloc *ballroom,
			       struct m0_be_seg *db,
			       uint32_t bshift,
			       m0_bindex_t container_size,
			       m0_bcount_t groupsize,
			       m0_bcount_t spare_reserve)
{
	struct reqh_ut_balloc *rb = getballoc(ballroom);

	m0_mutex_init(&rb->rb_lock);
	return 0;
}

static void reqh_ut_balloc_fini(struct m0_ad_balloc *ballroom)
{
	struct reqh_ut_balloc *rb = getballoc(ballroom);

	m0_mutex_fini(&rb->rb_lock);
}

static int reqh_ut_balloc_alloc(struct m0_ad_balloc *ballroom,
				struct m0_dtx *tx,
				m0_bcount_t count,
				struct m0_ext *out,
				uint64_t alloc_zone)
{
	struct reqh_ut_balloc	*rb = getballoc(ballroom);

	m0_mutex_lock(&rb->rb_lock);
	out->e_start = rb->rb_next;
	out->e_end   = rb->rb_next + count;
	rb->rb_next += count;
	m0_mutex_unlock(&rb->rb_lock);
	return 0;
}

static int reqh_ut_balloc_free(struct m0_ad_balloc *ballroom, struct m0_dtx *tx,
                            struct m0_ext *ext)
{
	return 0;
}

static const struct m0_ad_balloc_ops reqh_ut_balloc_ops = {
	.bo_init  = reqh_ut_balloc_init,
	.bo_fini  = reqh_ut_balloc_fini,
	.bo_alloc = reqh_ut_balloc_alloc,
	.bo_free  = reqh_ut_balloc_free,
};

/* static */ struct reqh_ut_balloc rb = {
	.rb_next = 0,
	.rb_ballroom = {
		.ab_ops = &reqh_ut_balloc_ops
	}
};

/* Buffer pool for TM receive queue. */
static struct m0_net_buffer_pool app_pool;

struct m0_stob_domain *reqh_ut_stob_domain_find(void)
{
	return sdom;
}

static int server_init(const char             *stob_path,
		       const char             *srv_db_name,
		       struct m0_net_domain   *net_dom,
		       uint64_t		       back_key,
		       struct m0_stob_domain **bdom)
{
	int                          rc;
	struct m0_sm_group          *grp;
	struct m0_rpc_machine       *rpc_machine = &srv_rpc_mach;
	struct m0_be_tx              tx;
	struct m0_be_tx_credit       cred = {};
	struct m0_be_seg            *seg;
	struct m0_stob		    *bstore;
	uint32_t		     bufs_nr;
	uint32_t		     tms_nr;
	char			    *sdom_cfg;
	char			    *sdom_init_cfg;
	char			    *sdom_location;
	struct m0_stob_id            stob_id;

        srv_cob_dom_id.id = 102;

	rc = M0_REQH_INIT(&reqh,
			  .rhia_dtm       = NULL,
			  .rhia_db        = NULL,
			  .rhia_mdstore   = &srv_mdstore,
			  .rhia_fid       = &g_process_fid,
		);
	M0_UT_ASSERT(rc == 0);

	m0_be_ut_backend_init(&ut_be);
	m0_be_ut_seg_init(&ut_seg, &ut_be, 1 << 20 /* 1 MB */);

	seg = ut_seg.bus_seg;
	grp = m0_be_ut_backend_sm_group_lookup(&ut_be);

	rc = m0_reqh_be_init(&reqh, seg);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Locate and create (if necessary) the backing store object.
	 */
	rc = m0_stob_domain_create_or_init(SERVER_BDOM_LOCATION, NULL,
					   SERVER_BDOM_KEY, NULL,
					   bdom);
	M0_UT_ASSERT(rc == 0);
	m0_stob_id_make(0, back_key, &(*bdom)->sd_id, &stob_id);
	rc = m0_stob_find(&stob_id, &bstore);
	M0_UT_ASSERT(rc == 0);
	rc = m0_stob_locate(bstore);
	M0_UT_ASSERT(rc == 0);
	rc = m0_stob_create(bstore, NULL, NULL);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Create AD domain over backing store object.
	 */
	m0_stob_ad_cfg_make(&sdom_cfg, seg, m0_stob_id_get(bstore), 0);
	sdom_location = m0_alloc(0x1000);
	snprintf(sdom_location, 0x1000, "adstob:seg=%p,1234", seg);
	m0_stob_ad_init_cfg_make(&sdom_init_cfg, &ut_be.but_dom);
	rc = m0_stob_domain_create(sdom_location, sdom_init_cfg, 2,
				   sdom_cfg, &sdom);
	M0_UT_ASSERT(rc == 0);

	m0_free(sdom_cfg);
	m0_free(sdom_location);
	m0_stob_put(bstore);

	/* Init mdstore without reading root cob. */
	rc = m0_mdstore_init(&srv_mdstore, seg, false);
	M0_UT_ASSERT(rc == -ENOENT);
	rc = m0_mdstore_create(&srv_mdstore, grp, &srv_cob_dom_id,
			       &ut_be.but_dom, seg);
	M0_UT_ASSERT(rc == 0);

	/* Create root session cob and other structures */
	m0_cob_tx_credit(srv_mdstore.md_dom, M0_COB_OP_DOMAIN_MKFS, &cred);
	m0_ut_be_tx_begin(&tx, &ut_be, &cred);
	rc = m0_cob_domain_mkfs(srv_mdstore.md_dom, &M0_MDSERVICE_SLASH_FID, &tx);
	m0_ut_be_tx_end(&tx);
	M0_UT_ASSERT(rc == 0);

	/* Finalize old mdstore. */
        m0_mdstore_fini(&srv_mdstore);

        /* Init new mdstore with open root flag. */
        rc = m0_mdstore_init(&srv_mdstore, seg, true);
        M0_UT_ASSERT(rc == 0);

	m0_reqh_start(&reqh);

	tms_nr   = 1;
	bufs_nr  = m0_rpc_bufs_nr(M0_NET_TM_RECV_QUEUE_DEF_LEN, tms_nr);

	rc = m0_rpc_net_buffer_pool_setup(net_dom, &app_pool, bufs_nr, tms_nr);
	M0_UT_ASSERT(rc == 0);

	/* Init the rpcmachine */
        rc = m0_rpc_machine_init(rpc_machine, net_dom,
				 SERVER_ENDPOINT_ADDR, &reqh, &app_pool,
				 M0_BUFFER_ANY_COLOUR, 0,
				 M0_NET_TM_RECV_QUEUE_DEF_LEN);
        M0_UT_ASSERT(rc == 0);
	reqh_ut_service = reqh.rh_rpc_service;
	M0_UT_ASSERT(reqh_ut_service != NULL);

	/* Init fdmi service */
	rc = m0_reqh_service_allocate(&fdmi_ut_service, &m0_fdmi_service_type, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_service_init(fdmi_ut_service, &reqh, NULL);
	rc = m0_reqh_service_start(fdmi_ut_service);
	M0_UT_ASSERT(rc == 0);

	return rc;
}

/* Fini the server */
static void server_fini(struct m0_stob_domain *bdom,
			uint64_t	       back_key)
{
	struct m0_sm_group *grp;
	struct m0_stob	   *bstore;
	int		    rc;
	struct m0_stob_id   stob_id;

	m0_reqh_service_prepare_to_stop(fdmi_ut_service);
	m0_reqh_idle_wait_for(&reqh, fdmi_ut_service);
	m0_reqh_service_stop(fdmi_ut_service);
	m0_reqh_service_fini(fdmi_ut_service);

	if (m0_reqh_state_get(&reqh) == M0_REQH_ST_NORMAL)
		m0_reqh_shutdown_wait(&reqh);

	M0_UT_ASSERT(m0_fom_domain_is_idle_for(reqh_ut_service));
	/* reqh_ut_service is finalised by m0_reqh_services_terminate(). */
	m0_reqh_services_terminate(&reqh);
	M0_UT_ASSERT(m0_reqh_state_get(&reqh) == M0_REQH_ST_STOPPED);

	grp = m0_be_ut_backend_sm_group_lookup(&ut_be);
	rc = m0_mdstore_destroy(&srv_mdstore, grp, &ut_be.but_dom);
	M0_UT_ASSERT(rc == 0);

	/* XXX domain can't be destroyed because of credit calculations bug */
	/* rc = m0_stob_domain_destroy(sdom); */
	/* M0_UT_ASSERT(rc == 0); */
	m0_stob_domain_fini(sdom);

        /* Fini the rpc_machine */
        m0_rpc_machine_fini(&srv_rpc_mach);
	m0_rpc_net_buffer_pool_cleanup(&app_pool);

	m0_stob_id_make(0, back_key, &bdom->sd_id, &stob_id);
	rc = m0_stob_find(&stob_id, &bstore);
	M0_ASSERT(rc == 0);
	rc = m0_stob_destroy(bstore, NULL);
	M0_ASSERT(rc == 0);

	rc = m0_stob_domain_destroy(bdom);
	M0_UT_ASSERT(rc == 0);

	m0_be_ut_seg_fini(&ut_seg);
	m0_be_ut_backend_fini(&ut_be);

	m0_reqh_fini(&reqh);
}

static void fop_send(struct m0_fop *fop, struct m0_rpc_session *session)
{
	int rc;

	rc = m0_rpc_post_sync(fop, session, NULL, 0 /* deadline */);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(fop->f_item.ri_error == 0);
	M0_UT_ASSERT(fop->f_item.ri_reply != 0);
	m0_fop_put_lock(fop);
}

/** Sends create fop request. */
static void create_send(struct m0_rpc_session *session)
{
	uint32_t                  i;
	struct m0_fop            *fop;
	struct m0_stob_io_create *rh_io_fop;

	for (i = 0; i < 10; ++i) {
		fop = m0_fop_alloc_at(session, &m0_stob_io_create_fopt);
		rh_io_fop = m0_fop_data(fop);
		rh_io_fop->fic_object.f_seq = i;
		rh_io_fop->fic_object.f_oid = i;
		fop_send(fop, session);
	}
}

/** Sends read fop request. */
static void read_send(struct m0_rpc_session *session)
{
	uint32_t                i;
	struct m0_fop          *fop;
	struct m0_stob_io_read *rh_io_fop;

	for (i = 0; i < 10; ++i) {
		fop = m0_fop_alloc_at(session, &m0_stob_io_read_fopt);
		rh_io_fop = m0_fop_data(fop);
		rh_io_fop->fir_object.f_seq = i;
		rh_io_fop->fir_object.f_oid = i;
		fop_send(fop, session);
	}
}

/** Sends write fop request. */
static void write_send(struct m0_rpc_session *session)
{
	uint32_t                 i;
	struct m0_fop           *fop;
	struct m0_stob_io_write *rh_io_fop;
	uint8_t                 *buf;

	for (i = 0; i < 10; ++i) {
		fop = m0_fop_alloc_at(session, &m0_stob_io_write_fopt);
		rh_io_fop = m0_fop_data(fop);
		rh_io_fop->fiw_object.f_seq = i;
		rh_io_fop->fiw_object.f_oid = i;
		M0_ALLOC_ARR(buf, 1 << BALLOC_DEF_BLOCK_SHIFT);
		M0_ASSERT(buf != NULL);
		rh_io_fop->fiw_value.fi_buf   = buf;
		rh_io_fop->fiw_value.fi_count = 1 << BALLOC_DEF_BLOCK_SHIFT;

		fop_send(fop, session);
	}
}

/**
 * Test function for reqh ut
 */
void test_reqh(void)
{
	int                    result;
	char                   opath[64];
	const char            *path;
	struct m0_net_xprt    *xprt        = &m0_net_lnet_xprt;
	struct m0_net_domain   net_dom     = { };
	struct m0_net_domain   srv_net_dom = { };
	struct m0_stob_domain *bdom;
	uint64_t	       back_key = 0xdf11e;
	struct m0_rpc_client_ctx cctx = {
		.rcx_net_dom            = &net_dom,
		.rcx_local_addr         = CLIENT_ENDPOINT_ADDR,
		.rcx_remote_addr        = SERVER_ENDPOINT_ADDR,
		.rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT,
		.rcx_fid                = &g_process_fid,
	};

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	path = "reqh_ut_stob";

	m0_stob_io_fop_init();

	M0_UT_ASSERT(strlen(path) < ARRAY_SIZE(opath) - 8);

	result = m0_net_domain_init(&net_dom, xprt);
	M0_UT_ASSERT(result == 0);
	result = m0_net_domain_init(&srv_net_dom, xprt);
	M0_UT_ASSERT(result == 0);

	server_init(path, SERVER_DB_NAME, &srv_net_dom, back_key, &bdom);

	result = m0_rpc_client_start(&cctx);
	M0_UT_ASSERT(result == 0);

	/* send fops */
	create_send(&cctx.rcx_session);
	write_send(&cctx.rcx_session);
	read_send(&cctx.rcx_session);

	result = m0_rpc_client_stop(&cctx);
	M0_UT_ASSERT(result == 0);

	server_fini(bdom, back_key);

	m0_net_domain_fini(&net_dom);
	m0_net_domain_fini(&srv_net_dom);
	m0_stob_io_fop_fini();
}

struct m0_ut_suite reqh_ut = {
	.ts_name = "reqh-ut",
	.ts_tests = {
		{ "reqh", test_reqh },
		{ NULL, NULL }
	}
};

/** @} end group reqh */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
