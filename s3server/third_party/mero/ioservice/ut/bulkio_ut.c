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
 * Original author: Madhavrao Vemuri <madhav_vemuri@xyratex.com>
 * Original creation date: 09/29/2011
 */

#include <sys/stat.h>
#include <sys/types.h>

#include "lib/processor.h"
#include "lib/locality.h"
#include "lib/finject.h"
#include "ut/ut.h"
#include "bulkio_common.h"
#include "net/lnet/lnet.h"
#include "rpc/rpclib.h"
#include "ioservice/io_fops.c"  /* To access static APIs. */
#include "ioservice/io_foms.c"  /* To access static APIs. */
#include "mero/setup.h"
#include "mero/setup_internal.h" /* m0_mero_conf_setup */
#include "pool/pool.h"
#include "fop/fom_generic.c"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_IOSERVICE
static struct bulkio_params *bp;

static struct m0_buf payload_buf = M0_BUF_INIT0;
static bool fol_check_enabled = false;
extern void bulkioapi_test(void);
static int io_fop_server_write_fom_create(struct m0_fop  *fop,
					  struct m0_fom **m,
					  struct m0_reqh *reqh);
static int ut_io_fom_cob_rw_create(struct m0_fop *fop, struct m0_fom **m,
				   struct m0_reqh *reqh);
static int io_fop_server_read_fom_create(struct m0_fop *fop, struct m0_fom **m,
					 struct m0_reqh *reqh);
static int io_fop_stob_create_fom_create(struct m0_fop *fop, struct m0_fom **m,
					 struct m0_reqh *reqh);
static int check_write_fom_tick(struct m0_fom *fom);
static int check_read_fom_tick(struct m0_fom *fom);

static const struct m0_fop_type_ops bulkio_stob_create_ops = {
	.fto_fop_replied = io_fop_replied,
	.fto_io_coalesce = io_fop_coalesce,
	.fto_io_desc_get = io_fop_desc_get,
};

static const struct m0_fop_type_ops bulkio_server_write_fop_ut_ops = {
	.fto_fop_replied = io_fop_replied,
	.fto_io_coalesce = io_fop_coalesce,
	.fto_io_desc_get = io_fop_desc_get,
};

static const struct m0_fop_type_ops bulkio_server_read_fop_ut_ops = {
	.fto_fop_replied = io_fop_replied,
	.fto_io_coalesce = io_fop_coalesce,
	.fto_io_desc_get = io_fop_desc_get,
};

static const struct m0_fom_type_ops bulkio_server_write_fomt_ops = {
	.fto_create = io_fop_server_write_fom_create,
};

static const struct m0_fom_type_ops bulkio_server_read_fomt_ops = {
	.fto_create = io_fop_server_read_fom_create,
};

static const struct m0_fom_type_ops bulkio_stob_create_fomt_ops = {
	.fto_create = io_fop_stob_create_fom_create,
};

static const struct m0_fom_type_ops ut_io_fom_cob_rw_type_ops = {
	.fto_create = ut_io_fom_cob_rw_create,
};

static void bulkio_stob_fom_fini(struct m0_fom *fom)
{
	struct m0_io_fom_cob_rw *fom_obj;

	fom_obj = container_of(fom, struct m0_io_fom_cob_rw, fcrw_gen);
	m0_stob_put(fom_obj->fcrw_stob);
	m0_fom_fini(fom);
	m0_free(fom);
}

struct m0_net_buffer_pool * ut_get_buffer_pool(struct m0_fom *fom)
{
	struct m0_reqh_io_service  *serv_obj;
	struct m0_rios_buffer_pool *bpdesc   = NULL;
	struct m0_net_domain       *fop_ndom = NULL;
	struct m0_fop              *fop      = NULL;

	fop = fom->fo_fop;
	serv_obj = container_of(fom->fo_service,
	                        struct m0_reqh_io_service, rios_gen);

	/* Get network buffer pool for network domain */
	fop_ndom = m0_fop_domain_get(fop);

	bpdesc = m0_tl_find(bufferpools, bpdesc, &serv_obj->rios_buffer_pools,
			    bpdesc->rios_ndom == fop_ndom);

	return bpdesc == NULL ? NULL :  &bpdesc->rios_bp;
}


/*
 * - This is positive test case to test m0_io_fom_cob_rw_tick(fom).
 * - This function test next phase after every defined phase for Write FOM.
 * - Validation of next phase is done as per state transition in detail design.
 *   @see DLD-bulk-server-lspec-state
 */
static int bulkio_server_write_fom_tick(struct m0_fom *fom)
{
	int rc;
	int phase0;

	phase0 = m0_fom_phase(fom);
	M0_LOG(M0_DEBUG, "phase=%d", phase0);
	rc = m0_io_fom_cob_rw_tick(fom);
	if (fol_check_enabled) {
		if (phase0 == M0_FOPH_FOL_REC_ADD) {
			int rc;

			if (payload_buf.b_addr != NULL)
				m0_buf_free(&payload_buf);
			rc = m0_buf_copy(&payload_buf,
					 &fom->fo_tx.tx_betx.t_payload);
			M0_UT_ASSERT(rc == 0);
		}
	}
	if (m0_fom_rc(fom) != 0) {
	        M0_UT_ASSERT(m0_fom_phase(fom) == M0_FOPH_FAILURE);
		return rc;
	}
	switch (phase0) {
	case M0_FOPH_IO_FOM_BUFFER_ACQUIRE :
		M0_UT_ASSERT(M0_IN(m0_fom_phase(fom),
				   (M0_FOPH_IO_FOM_BUFFER_WAIT,
				    M0_FOPH_IO_ZERO_COPY_INIT)));
		break;
	case M0_FOPH_IO_ZERO_COPY_INIT:
	        M0_UT_ASSERT(m0_fom_phase(fom) == M0_FOPH_IO_ZERO_COPY_WAIT);
		break;
	case M0_FOPH_IO_ZERO_COPY_WAIT:
	        M0_UT_ASSERT(m0_fom_phase(fom) == M0_FOPH_TXN_INIT);
		break;
	case M0_FOPH_IO_STOB_INIT:
	        M0_UT_ASSERT(m0_fom_phase(fom) == M0_FOPH_IO_STOB_WAIT);
		break;
	case M0_FOPH_IO_STOB_WAIT:
	        M0_UT_ASSERT(m0_fom_phase(fom) == M0_FOPH_IO_BUFFER_RELEASE);
		break;
	case M0_FOPH_IO_BUFFER_RELEASE:
	        M0_UT_ASSERT(M0_IN(m0_fom_phase(fom),
				   (M0_FOPH_IO_FOM_BUFFER_ACQUIRE,
				    M0_FOPH_SUCCESS)));
		break;
	}
	return rc;
}

/*
 * - This is positive test case to test m0_io_fom_cob_rw_tick(fom).
 * - This function test next phase after every defined phase for Read FOM.
 * - Validation of next phase is done as per state transition in detail design.
 *   @see DLD-bulk-server-lspec-state
 */
static int bulkio_server_read_fom_tick(struct m0_fom *fom)
{
	int rc;
	int phase0;

	phase0 = m0_fom_phase(fom);
	rc = m0_io_fom_cob_rw_tick(fom);
	if (m0_fom_rc(fom) != 0) {
	        M0_UT_ASSERT(m0_fom_phase(fom) == M0_FOPH_FAILURE);
		return rc;
	}
	switch (phase0) {
	case M0_FOPH_IO_FOM_BUFFER_ACQUIRE :
	        M0_UT_ASSERT(M0_IN(m0_fom_phase(fom),
				   (M0_FOPH_IO_FOM_BUFFER_WAIT,
				    M0_FOPH_IO_STOB_INIT)));
		break;
	case M0_FOPH_IO_ZERO_COPY_INIT:
	        M0_UT_ASSERT(m0_fom_phase(fom) == M0_FOPH_IO_ZERO_COPY_WAIT);
		break;
	case M0_FOPH_IO_ZERO_COPY_WAIT:
	        M0_UT_ASSERT(m0_fom_phase(fom) == M0_FOPH_IO_BUFFER_RELEASE);
		break;
	case M0_FOPH_IO_STOB_INIT:
	        M0_UT_ASSERT(m0_fom_phase(fom) == M0_FOPH_IO_STOB_WAIT);
		break;
	case M0_FOPH_IO_STOB_WAIT:
	        M0_UT_ASSERT(m0_fom_phase(fom) == M0_FOPH_IO_ZERO_COPY_INIT);
		break;
	case M0_FOPH_IO_BUFFER_RELEASE:
	        M0_UT_ASSERT(M0_IN(m0_fom_phase(fom),
				   (M0_FOPH_IO_FOM_BUFFER_ACQUIRE,
				    M0_FOPH_SUCCESS)));
		break;
	}
	return rc;
}

/*
 * This function intercepts actual I/O FOM state,
 * for state transition testing.
 *
 * This ut FOM work with real fop send by bulk client.
 * - Client first send write fop
 * - Fops at server side are intercepted by this dummy state function and
     checks all possible state transitions.
 * - It simulates failure environment for particular state and restore
 *   it again after each test.
 * - After reply fop is received by client, client sends a read fop to read
 *   data written by previous write fop.
 * - Further it will checks remaining state transitions.
 * - After reply fop is received by client, at client side received data is
 *   compared with the original data used to send it.
 */
static int ut_io_fom_cob_rw_state(struct m0_fom *fom)
{
	return m0_is_read_fop(fom->fo_fop) ?
		check_read_fom_tick(fom) : check_write_fom_tick(fom);
}

enum fom_state_transition_tests {
	TEST00 = M0_FOPH_IO_FOM_PREPARE,
	TEST01,
	TEST02,
	TEST03,
	TEST07,
	TEST10,
	TEST11,
};

static int                        i = 0;
static struct m0_net_buffer      *nb_list[64];
static struct m0_net_buffer_pool *buf_pool;
static int                        next_write_test = TEST00;
static int                        next_read_test  = TEST00;

static void empty_buffers_pool(uint32_t colour)
{
	i--;
	m0_net_buffer_pool_lock(buf_pool);
	do nb_list[++i] = m0_net_buffer_pool_get(buf_pool, colour);
	while (nb_list[i] != NULL);
	m0_net_buffer_pool_unlock(buf_pool);
}

static void release_one_buffer(uint32_t colour)
{
	m0_net_buffer_pool_lock(buf_pool);
	m0_net_buffer_pool_put(buf_pool, nb_list[--i], colour);
	m0_net_buffer_pool_unlock(buf_pool);
}

static void fill_buffers_pool(uint32_t colour)
{
	m0_net_buffer_pool_lock(buf_pool);
	while (i > 0)
		m0_net_buffer_pool_put(buf_pool, nb_list[--i], colour);
	m0_net_buffer_pool_unlock(buf_pool);
}

static void builkio_ut_stob_get(struct m0_io_fom_cob_rw *fom_obj)
{
	struct m0_storage_devs  *devs = m0_cs_storage_devs_get();
	struct m0_storage_dev   *dev;
	struct m0_stob_domain   *dom;

	M0_UT_ASSERT(devs != NULL);
	M0_UT_ASSERT(fom_obj->fcrw_stob != NULL);
	m0_stob_get(fom_obj->fcrw_stob);
	dom = m0_stob_dom_get(fom_obj->fcrw_stob);
	M0_UT_ASSERT(dom != NULL);
	m0_storage_devs_lock(devs);
	dev = m0_storage_devs_find_by_dom(devs, dom);
	M0_UT_ASSERT(dev != NULL);
	M0_LOG(M0_DEBUG, "get: dev=%p, ref=%" PRIi64
	       "state=%d type=%d, %"PRIu64,
	       dev,
	       m0_ref_read(&dev->isd_ref),
	       dev->isd_ha_state,
	       dev->isd_srv_type,
	       dev->isd_cid);
	m0_storage_dev_get(dev);
	m0_storage_devs_unlock(devs);
}

static void fom_phase_set(struct m0_fom *fom, int phase)
{
	if (m0_fom_phase(fom) == M0_FOPH_FAILURE) {
		const struct fom_phase_desc *fpd_phase;

		while (m0_fom_phase(fom) != M0_FOPH_FINISH) {
			fpd_phase = &fpd_table[m0_fom_phase(fom)];
			m0_fom_phase_set(fom, fpd_phase->fpd_nextphase);
		}

		m0_sm_fini(&fom->fo_sm_phase);
		M0_SET0(&fom->fo_sm_phase);
		m0_sm_init(&fom->fo_sm_phase, &fom->fo_type->ft_conf,
			   M0_FOM_PHASE_INIT, &fom->fo_loc->fl_group);

		while (m0_fom_phase(fom) != M0_FOPH_TYPE_SPECIFIC) {
			fpd_phase = &fpd_table[m0_fom_phase(fom)];
			m0_fom_phase_set(fom, fpd_phase->fpd_nextphase);
		}
	}

	while (phase != m0_fom_phase(fom)) {
		struct m0_io_fom_cob_rw_state_transition  st;

		st = m0_is_read_fop(fom->fo_fop) ?
			io_fom_read_st[m0_fom_phase(fom)] :
			io_fom_write_st[m0_fom_phase(fom)];

		if (M0_IN(phase, (st.fcrw_st_next_phase_again,
				  st.fcrw_st_next_phase_wait))) {
			m0_fom_phase_set(fom, phase);
			break;
		}

		m0_fom_phase_set(fom, st.fcrw_st_next_phase_again != 0 ?
				      st.fcrw_st_next_phase_again :
				      st.fcrw_st_next_phase_wait);
	}
}

/*
 * - This function tests next phase after every defined phase for Write FOM.
 * - Validation of next phase is done as per state transition in detail design.
 *   @see DLD-bulk-server-lspec-state
 * - This test covers all positive as well as negative cases.
 * Note : For each test case it does following things,
 *      - simulates the environment,
 *      - run state function for respective I/O FOM,
 *      - check output state & return code,
 *      - restores the FOM to it's clean state by using the saved original data.
 */
static int check_write_fom_tick(struct m0_fom *fom)
{
	int                        rc;
	uint32_t                   colour;
	int                        acquired_net_bufs;
	int                        saved_ndesc;
	struct m0_fop_cob_rw      *rwfop;
	struct m0_fop             *fop;
	struct m0_io_fom_cob_rw   *fom_obj;
	struct m0_fid              saved_fid;
	struct m0_fid              invalid_fid;
	struct m0_stob_io_desc    *saved_stobio_desc;
	struct m0_stob            *saved_stob;
	int                        saved_count;
	struct m0_net_domain      *netdom;
	struct m0_net_transfer_mc *tm;

	fom_obj = container_of(fom, struct m0_io_fom_cob_rw, fcrw_gen);
	fop = fom->fo_fop;
	rwfop = io_rw_get(fop);

	tm = m0_fop_tm_get(fop);
	colour = m0_net_tm_colour_get(tm);

	if (m0_fom_phase(fom) < M0_FOPH_NR) {
	        /*
	         * No need to test generic phases.
	         */
	        rc = m0_io_fom_cob_rw_tick(fom);
		if (next_write_test <= TEST00)
			next_write_test = m0_fom_phase(fom);
	} else if (next_write_test == TEST00) {
	        rc = m0_io_fom_cob_rw_tick(fom);
		next_write_test = TEST01;
	} else if (next_write_test == TEST01) {
	        /* Acquire all buffer pool buffer test some of cases. */
	        if (fom_obj->fcrw_bp == NULL)
	                buf_pool = ut_get_buffer_pool(fom);
	        else
	                buf_pool = fom_obj->fcrw_bp;
	        M0_UT_ASSERT(buf_pool != NULL);

	        empty_buffers_pool(colour);

	        /*
	         * Case 01: No network buffer is available with the buffer pool.
	         *         Input phase          : M0_FOPH_IO_FOM_BUFFER_ACQUIRE
	         *         Expected Output phase: M0_FOPH_IO_FOM_BUFFER_WAIT
	         */
	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_WAIT &&
	                     m0_fom_phase(fom) == M0_FOPH_IO_FOM_BUFFER_WAIT);

	        /* Cleanup & make clean FOM for next test. */
	        rc = M0_FSO_WAIT;
	        fom->fo_sm_phase.sm_rc = 0;

	        release_one_buffer(colour);
		next_write_test = TEST02;
	} else if (next_write_test == TEST02) {
	        /*
	         * Case 02: No network buffer is available with the buffer pool.
	         *         Even after getting buffer pool not-empty event,
	         *         buffers are not available in pool (which could be
	         *         used by other FOMs in the server).
	         *         Input phase          : M0_FOPH_IO_FOM_BUFFER_WAIT
	         *         Expected Output phase: M0_FOPH_IO_FOM_BUFFER_WAIT
	         */

	        empty_buffers_pool(colour);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_WAIT  &&
	                     m0_fom_phase(fom) ==  M0_FOPH_IO_FOM_BUFFER_WAIT);

	        /* Cleanup & rstore FOM for next test. */
	        rc = M0_FSO_WAIT;

	        release_one_buffer(colour);
		next_write_test = TEST03;
	} else if (next_write_test == TEST03) {
		int cdi = fom_obj->fcrw_curr_desc_index;

	        /*
	         * Case 03 : Network buffer is available with the buffer pool.
	         *         Input phase          : M0_FOPH_IO_FOM_BUFFER_ACQUIRE
	         *         Expected Output phase: M0_FOPH_IO_ZERO_COPY_INIT
	         */
		fom_phase_set(fom, M0_FOPH_IO_FOM_BUFFER_ACQUIRE);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_AGAIN &&
	                     m0_fom_phase(fom) == M0_FOPH_IO_ZERO_COPY_INIT);

	        /*
	         * Cleanup & restore FOM for next test.
	         * Since previous case successfully acquired network buffer
	         * and now buffer pool not having any network buffer, this
	         * buffer need to return back to the buffer pool.
	         */
	        acquired_net_bufs =
	                netbufs_tlist_length(&fom_obj->fcrw_netbuf_list);
	        m0_net_buffer_pool_lock(fom_obj->fcrw_bp);
	        while (acquired_net_bufs > 0) {
	                struct m0_net_buffer *nb;

	                nb = netbufs_tlist_tail(&fom_obj->fcrw_netbuf_list);
	                m0_net_buffer_pool_put(fom_obj->fcrw_bp, nb, colour);
	                netbufs_tlink_del_fini(nb);
	                acquired_net_bufs--;
	        }
	        m0_net_buffer_pool_unlock(fom_obj->fcrw_bp);
	        fom_obj->fcrw_batch_size = 0;

	        /*
	         * Case 04 : Network buffer is available with the buffer pool.
	         *         Input phase          : M0_FOPH_IO_FOM_BUFFER_WAIT
	         *         Expected Output phase: M0_FOPH_IO_ZERO_COPY_INIT
	         */
	        fom_phase_set(fom, M0_FOPH_IO_FOM_BUFFER_WAIT);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_AGAIN &&
	                     m0_fom_phase(fom) == M0_FOPH_IO_ZERO_COPY_INIT);
		/*
		 * No need to cleanup here, since FOM will be  transitioned to
		 * expected phase.
		 */

		/*
		 * Case 05 : Zero-copy failure
		 *         Input phase          : M0_FOPH_IO_ZERO_COPY_INIT
		 *         Expected Output phase: M0_FOPH_FAILURE
		 */

		/*
		 * Modify net buffer used count in fop (value greater than
		 * net domain max), so that zero-copy initialisation fails.
		 */
		saved_count = rwfop->crw_desc.id_descs[cdi].bdd_used;
		netdom = m0_fop_domain_get(fop);
		rwfop->crw_desc.id_descs[cdi].bdd_used =
			m0_net_domain_get_max_buffer_size(netdom) + 4096;

		fom_phase_set(fom, M0_FOPH_IO_ZERO_COPY_INIT);
		builkio_ut_stob_get(fom_obj);

		m0_fi_enable_once("zero_copy_initiate", "keep-net-buffers");
		rc = m0_io_fom_cob_rw_tick(fom);
		M0_UT_ASSERT(m0_fom_rc(fom) != 0 &&
			     rc == M0_FSO_AGAIN &&
			     m0_fom_phase(fom) == M0_FOPH_FAILURE);

		/* Cleanup & restore FOM for next test. */
		rwfop->crw_desc.id_descs[cdi].bdd_used = saved_count;

	        /*
	         * Case 06 : Zero-copy success
	         *         Input phase          : M0_FOPH_IO_ZERO_COPY_INIT
	         *         Expected Output phase: M0_FOPH_IO_ZERO_COPY_WAIT
	         */
	        /*
	         * To bypass request handler need to change FOM callback
	         * function which wakeup FOM from wait.
	         */
	        fom_phase_set(fom, M0_FOPH_IO_ZERO_COPY_INIT);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_WAIT &&
	                     m0_fom_phase(fom) == M0_FOPH_IO_ZERO_COPY_WAIT);
		next_write_test = TEST07;
	} else if (next_write_test == TEST07) {
	        /*
	         * Case 07 : Zero-copy failure
	         *         Input phase          : M0_FOPH_IO_ZERO_COPY_WAIT
	         *         Expected Output phase: M0_FOPH_FAILURE
	         */
	        fom_phase_set(fom, M0_FOPH_IO_ZERO_COPY_WAIT);
	        fom_obj->fcrw_bulk.rb_rc  = -1;
		builkio_ut_stob_get(fom_obj);

		m0_fi_enable_once("zero_copy_finish", "keep-net-buffers");
	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) != 0 &&
	                     rc == M0_FSO_AGAIN &&
	                     m0_fom_phase(fom) == M0_FOPH_FAILURE);

	        /* Cleanup & make clean FOM for next test. */
	        fom_obj->fcrw_bulk.rb_rc  = 0;

	        /*
	         * Case 08 : Zero-copy success from wait state.
	         *         Input phase          : M0_FOPH_IO_ZERO_COPY_WAIT
	         *         Expected Output phase: M0_FOPH_TXN_INIT
	         */
		fom_phase_set(fom, M0_FOPH_IO_ZERO_COPY_WAIT);
	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_AGAIN &&
	                     m0_fom_phase(fom) == M0_FOPH_TXN_INIT);
		next_write_test = TEST10;
	} else if (next_write_test == TEST10) {
	        /*
	         * Case 09 : STOB I/O launch failure
	         *         Input phase          : M0_FOPH_IO_STOB_INIT
	         *         Expected Output phase: M0_FOPH_FAILURE
	         */

	        /* Save original fid and pass invialid fid
	         * to make I/O launch fail. */
	        saved_fid = rwfop->crw_fid;
		saved_stob = fom_obj->fcrw_stob;
	        m0_fid_set(&invalid_fid, 111, 222);
		m0_fid_tassume(&invalid_fid, &m0_cob_fid_type);
		fom_obj->fcrw_stob = NULL;
	        rwfop->crw_fid = invalid_fid;
	        fom_phase_set(fom, M0_FOPH_IO_STOB_INIT);

		m0_fi_enable_once("io_launch", "keep-net-buffers");
	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) != 0 && rc == M0_FSO_AGAIN  &&
	                     m0_fom_phase(fom) == M0_FOPH_FAILURE);

	        /* Cleanup & make clean FOM for next test. */
	        rwfop->crw_fid = saved_fid;
		fom_obj->fcrw_stob = saved_stob;

	        /*
	         * Case 10 : STOB I/O launch success
	         *         Input phase          : M0_FOPH_IO_STOB_INIT
	         *         Expected Output phase: M0_FOPH_IO_STOB_WAIT
	         */
	        /*
	         * To bypass request handler need to change FOM callback
	         * function which wakeup FOM from wait.
	         */
	        fom_phase_set(fom, M0_FOPH_IO_STOB_INIT);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 && rc == M0_FSO_WAIT  &&
	                     m0_fom_phase(fom) == M0_FOPH_IO_STOB_WAIT);

		next_write_test = TEST11;
	} else if (next_write_test == TEST11) {
	        /*
	         * Case 11 : STOB I/O failure from wait state.
	         *         Input phase          : M0_FOPH_IO_STOB_WAIT
	         *         Expected Output phase: M0_FOPH_FAILURE
	         */

	        /*
	         * To test this case there is a need to invalidate stobio
	         * descriptor, since io_finish() removes the stobio descriptor
	         * from list.
	         * There is only one stobio descriptor.
	         * Before returning error this phase will do following phases :
	         * - free and remove stobio descriptors in list,
	         * - put stob object
	         * - leave FOM block
	         */
	        saved_stobio_desc = stobio_tlist_pop(&fom_obj->fcrw_stio_list);
	        M0_UT_ASSERT(saved_stobio_desc != NULL);

		builkio_ut_stob_get(fom_obj);
		fom_phase_set(fom, M0_FOPH_IO_STOB_WAIT);
		m0_fi_enable_once("io_finish", "fake_error");
		m0_fi_enable_once("io_finish", "keep-net-buffers");
	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) != 0 && rc == M0_FSO_AGAIN  &&
	                     m0_fom_phase(fom) == M0_FOPH_FAILURE);

	        /*
	         * Cleanup & make clean FOM for next test.
	         * Restore original fom.
	         */
	        stobio_tlist_add(&fom_obj->fcrw_stio_list, saved_stobio_desc);

	        /*
	         * Case 12 : STOB I/O success
	         *         Input phase          : M0_FOPH_IO_STOB_WAIT
	         *         Expected Output phase: M0_FOPH_IO_BUFFER_RELEASE
	         */
	        fom_phase_set(fom, M0_FOPH_IO_STOB_WAIT);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_AGAIN  &&
	                     m0_fom_phase(fom) == M0_FOPH_IO_BUFFER_RELEASE);

	        /*
	         * Case 13 : Processing of remaining buffer descriptors.
	         *         Input phase          : M0_FOPH_IO_BUFFER_RELEASE
	         *         Expected Output phase: M0_FOPH_IO_FOM_BUFFER_ACQUIRE
	         */
	        fom_phase_set(fom, M0_FOPH_IO_BUFFER_RELEASE);

	        saved_ndesc = fom_obj->fcrw_ndesc;
	        fom_obj->fcrw_ndesc = 2;
	        rwfop->crw_desc.id_nr = 2;
	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_AGAIN  &&
	                     m0_fom_phase(fom) ==
			     M0_FOPH_IO_FOM_BUFFER_ACQUIRE);

	        /* Cleanup & make clean FOM for next test. */
	        fom_obj->fcrw_ndesc = saved_ndesc;
	        rwfop->crw_desc.id_nr = saved_ndesc;

	        /*
	         * Case 14 : All buffer descriptors are processed.
	         *         Input phase          : M0_FOPH_IO_BUFFER_RELEASE
	         *         Expected Output phase: M0_FOPH_SUCCESS
	         */
	        fom_phase_set(fom, M0_FOPH_IO_BUFFER_RELEASE);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_AGAIN &&
	                     m0_fom_phase(fom) == M0_FOPH_SUCCESS);

	        fill_buffers_pool(colour);
	} else {
	        M0_UT_ASSERT(0); /* this should not happen */
	        rc = M0_FSO_WAIT; /* to avoid compiler warning */
	}

	return rc;
}

/*
 * - This function test next phase after every defined phase for Read FOM.
 * - Validation of next phase is done as per state transition in detail design.
 *   @see DLD-bulk-server-lspec-state
 * - This test cover positive as well as negative cases.
 * Note : For each test case it does following things
 *      - simulate environemnt,
 *      - run state function for respective I/O FOM,
 *      - check output state & return code,
 *      - restores the FOM to it's clean state by using the saved original data.
 */
static int check_read_fom_tick(struct m0_fom *fom)
{
	int                        rc;
	uint32_t                   colour;
	int                        acquired_net_bufs;
	int                        saved_count;
	int                        saved_ndesc;
	struct m0_fop_cob_rw      *rwfop;
	struct m0_net_domain      *netdom;
	struct m0_fop             *fop;
	struct m0_io_fom_cob_rw   *fom_obj;
	struct m0_fid              saved_fid;
	struct m0_fid              invalid_fid;
	struct m0_stob_io_desc    *saved_stobio_desc;
	struct m0_stob            *saved_stob;
	struct m0_net_transfer_mc *tm;

	fom_obj = container_of(fom, struct m0_io_fom_cob_rw, fcrw_gen);
	fop = fom->fo_fop;
	rwfop = io_rw_get(fop);

	tm = m0_fop_tm_get(fop);
	colour = m0_net_tm_colour_get(tm);

	if (m0_fom_phase(fom) < M0_FOPH_NR) {
	        /*
	         * No need to test generic phases.
	         */
	        rc = m0_io_fom_cob_rw_tick(fom);
		next_read_test = m0_fom_phase(fom);
	} else if (next_read_test == TEST00) {
	        rc = m0_io_fom_cob_rw_tick(fom);
		next_read_test = TEST01;
	} else if (next_read_test == TEST01) {
	        /* Acquire all buffer pool buffer test some of cases. */
	        if (fom_obj->fcrw_bp == NULL)
	                buf_pool = ut_get_buffer_pool(fom);
	        else
	                buf_pool = fom_obj->fcrw_bp;
	        M0_UT_ASSERT(buf_pool != NULL);

	        /* Acquire all buffers from buffer pool to make it empty */
	        empty_buffers_pool(colour);

	        /*
	         * Case 01 : No network buffer is available with buffer pool.
	         *         Input phase          : M0_FOPH_IO_FOM_BUFFER_ACQUIRE
	         *         Expected Output phase: M0_FOPH_IO_FOM_BUFFER_WAIT
	         */
	        fom_phase_set(fom, M0_FOPH_IO_FOM_BUFFER_ACQUIRE);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 && rc == M0_FSO_WAIT &&
	                     m0_fom_phase(fom) ==  M0_FOPH_IO_FOM_BUFFER_WAIT);

	        /* Cleanup & make clean FOM for next test. */
	        rc = M0_FSO_WAIT;

	        release_one_buffer(colour);
	        next_read_test = TEST02;
	} else if (next_read_test == TEST02) {
	        /*
	         * Case 02 : No network buffer is available with buffer pool.
	         *         Even after getting buffer pool not-empty event,
	         *         buffers are not available in pool (which could be
	         *         used by other FOMs in the server).
	         *         Input phase          : M0_FOPH_IO_FOM_BUFFER_WAIT
	         *         Expected Output phase: M0_FOPH_IO_FOM_BUFFER_WAIT
	         */

	        empty_buffers_pool(colour);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_WAIT  &&
	                     m0_fom_phase(fom) ==  M0_FOPH_IO_FOM_BUFFER_WAIT);

	        /* Cleanup & make clean FOM for next test. */
	        rc = M0_FSO_WAIT;

	        release_one_buffer(colour);
		next_read_test = TEST03;
	} else if (next_read_test == TEST03) {
	        /*
	         * Case 03 : Network buffer is available with the buffer pool.
	         *         Input phase          : M0_FOPH_IO_FOM_BUFFER_ACQUIRE
	         *         Expected Output phase: M0_FOPH_IO_STOB_INIT
	         */
	        fom_phase_set(fom, M0_FOPH_IO_FOM_BUFFER_ACQUIRE);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 && rc == M0_FSO_AGAIN  &&
	                     m0_fom_phase(fom) == M0_FOPH_IO_STOB_INIT);

	        /*
	         * Cleanup & make clean FOM for next test.
	         * Since previous this case successfully acquired network buffer
	         * and now buffer pool not having network buffer, this buffer
	         * need to return back to the buffer pool.
	         */
	        acquired_net_bufs =
	                netbufs_tlist_length(&fom_obj->fcrw_netbuf_list);
	        m0_net_buffer_pool_lock(fom_obj->fcrw_bp);
	        while (acquired_net_bufs > 0) {
	                struct m0_net_buffer *nb;

	                nb = netbufs_tlist_tail(&fom_obj->fcrw_netbuf_list);
	                m0_net_buffer_pool_put(fom_obj->fcrw_bp, nb, colour);
	                netbufs_tlink_del_fini(nb);
	                acquired_net_bufs--;
	        }
	        m0_net_buffer_pool_unlock(fom_obj->fcrw_bp);
	        fom_obj->fcrw_batch_size = 0;

	        /*
	         * Case 04 : Network buffer available with buffer pool.
	         *         Input phase          : M0_FOPH_IO_FOM_BUFFER_WAIT
	         *         Expected Output phase: M0_FOPH_IO_STOB_INIT
	         */
	        fom_phase_set(fom, M0_FOPH_IO_FOM_BUFFER_WAIT);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_AGAIN &&
	                     m0_fom_phase(fom) == M0_FOPH_IO_STOB_INIT);

	        /* No need to cleanup here, since FOM will transitioned
	         * to expected phase.
	         */

	        /*
	         * Case 05 : STOB I/O launch failure
	         *         Input phase          : M0_FOPH_IO_STOB_INIT
	         *         Expected Output phase: M0_FOPH_FAILURE
	         */

	        /* Save original fid and pass invalid fid to make I/O launch
	         * fail. */
	        saved_fid = rwfop->crw_fid;
		saved_stob = fom_obj->fcrw_stob;
	        m0_fid_set(&invalid_fid, 111, 222);
		m0_fid_tassume(&invalid_fid, &m0_cob_fid_type);
		fom_obj->fcrw_stob = NULL;
	        rwfop->crw_fid = invalid_fid;

	        fom_phase_set(fom, M0_FOPH_IO_STOB_INIT);

		m0_fi_enable_once("io_launch", "keep-net-buffers");
	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) != 0 &&
	                     rc == M0_FSO_AGAIN &&
	                     m0_fom_phase(fom) == M0_FOPH_FAILURE);

	        /* Cleanup & make clean FOM for next test. */
	        rwfop->crw_fid = saved_fid;
		fom_obj->fcrw_stob = saved_stob;

	        /*
	         * Case 06 : STOB I/O launch success
	         *         Input phase          : M0_FOPH_IO_STOB_INIT
	         *         Expected Output phase: M0_FOPH_IO_STOB_WAIT
	         */
	        /*
	         * To bypass request handler need to change FOM callback
	         * function which wakeup FOM from wait.
	         */
	        fom_phase_set(fom, M0_FOPH_IO_STOB_INIT);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_WAIT &&
	                     m0_fom_phase(fom) == M0_FOPH_IO_STOB_WAIT);
		next_read_test = TEST07;
	} else if (next_read_test == TEST07) {
		int cdi = fom_obj->fcrw_curr_desc_index;

	        /*
	         * Case 07 : STOB I/O failure
	         *         Input phase          : M0_FOPH_IO_STOB_WAIT
	         *         Expected Output phase: M0_FOPH_FAILURE
	         */

	        /*
	         * To test this case there is a need to invalidate stobio
	         * descriptor, since io_finish() remove stobio descriptor
	         * from list.
	         * There is only one stobio descriptor.
	         * Before returning error this phase will do following phases :
	         * - free and remove stobio descriptors in list,
	         * - put stob object
	         * - leave FOM block
	         */
	        saved_stobio_desc = stobio_tlist_pop(&fom_obj->fcrw_stio_list);
	        M0_UT_ASSERT(saved_stobio_desc != NULL);

	        fom_phase_set(fom, M0_FOPH_IO_STOB_WAIT);
		builkio_ut_stob_get(fom_obj);

		m0_fi_enable_once("io_finish", "fake_error");
		m0_fi_enable_once("io_finish", "keep-net-buffers");
	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) != 0 &&
	                     rc == M0_FSO_AGAIN &&
	                     m0_fom_phase(fom) == M0_FOPH_FAILURE);

	        /*
	         * Cleanup & make clean FOM for next test.
	         * Restore original fom.
	         */
	        stobio_tlist_add(&fom_obj->fcrw_stio_list, saved_stobio_desc);

	        /*
	         * Case 08 : STOB I/O success
	         *         Input phase          : M0_FOPH_IO_STOB_WAIT
	         *         Expected Output phase: M0_FOPH_IO_ZERO_COPY_INIT
	         */
	        fom_phase_set(fom, M0_FOPH_IO_STOB_WAIT);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_AGAIN &&
	                     m0_fom_phase(fom) == M0_FOPH_IO_ZERO_COPY_INIT);
		/*
		 * Case 09 : Zero-copy failure
		 *         Input phase          : M0_FOPH_IO_ZERO_COPY_INIT
		 *         Expected Output phase: M0_FOPH_FAILURE
		 */

		/*
		 * Modify net buffer used count in fop (value greater than
		 * net domain max), so that zero-copy initialisation fails.
		 */
		saved_count = rwfop->crw_desc.id_descs[cdi].bdd_used;
		netdom = m0_fop_domain_get(fop);
		rwfop->crw_desc.id_descs[cdi].bdd_used =
			m0_net_domain_get_max_buffer_size(netdom) + 4096;

		fom_phase_set(fom, M0_FOPH_IO_ZERO_COPY_INIT);
		builkio_ut_stob_get(fom_obj);

		m0_fi_enable_once("zero_copy_initiate", "keep-net-buffers");
		rc = m0_io_fom_cob_rw_tick(fom);
		M0_UT_ASSERT(m0_fom_rc(fom) != 0 &&
			     rc == M0_FSO_AGAIN &&
			     m0_fom_phase(fom) == M0_FOPH_FAILURE);

		/* Cleanup & make clean FOM for next test. */
		rwfop->crw_desc.id_descs[cdi].bdd_used = saved_count;
		fom_phase_set(fom, TEST10);

	        /*
	         * Case 10 : Zero-copy success
	         *         Input phase          : M0_FOPH_IO_ZERO_COPY_INIT
	         *         Expected Output phase: M0_FOPH_IO_ZERO_COPY_WAIT
	         */
	        /*
	         * To bypass request handler need to change FOM callback
	         * function which wakeup FOM from wait.
	         */
	        fom_phase_set(fom, M0_FOPH_IO_ZERO_COPY_INIT);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_WAIT &&
	                     m0_fom_phase(fom) == M0_FOPH_IO_ZERO_COPY_WAIT);
		next_read_test = TEST11;
	} else if (next_read_test == TEST11) {
	        /*
	         * Case 11 : Zero-copy failure
	         *         Input phase          : M0_FOPH_IO_ZERO_COPY_WAIT
	         *         Expected Output phase: M0_FOPH_FAILURE
	         */
	        fom_phase_set(fom, M0_FOPH_IO_ZERO_COPY_WAIT);
	        fom_obj->fcrw_bulk.rb_rc  = -1;
		builkio_ut_stob_get(fom_obj);

		m0_fi_enable_once("zero_copy_finish", "keep-net-buffers");
	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) != 0 &&
	                     rc == M0_FSO_AGAIN &&
	                     m0_fom_phase(fom) == M0_FOPH_FAILURE);

	        /* Cleanup & make clean FOM for next test. */
	        fom_obj->fcrw_bulk.rb_rc  = 0;

	        /*
	         * Case 12 : Zero-copy success
	         *         Input phase          : M0_FOPH_IO_ZERO_COPY_WAIT
	         *         Expected Output phase: M0_FOPH_IO_BUFFER_RELEASE
	         */
	        fom_phase_set(fom, M0_FOPH_IO_ZERO_COPY_WAIT);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_AGAIN &&
	                     m0_fom_phase(fom) == M0_FOPH_IO_BUFFER_RELEASE);


	        /*
	         * Case 13 : Processing of remaining buffer descriptors.
	         *         Input phase          : M0_FOPH_IO_BUFFER_RELEASE
	         *         Expected Output phase: M0_FOPH_IO_FOM_BUFFER_ACQUIRE
	         */
	        fom_phase_set(fom, M0_FOPH_IO_BUFFER_RELEASE);

	        saved_ndesc = fom_obj->fcrw_ndesc;
	        fom_obj->fcrw_ndesc = 2;
	        rwfop->crw_desc.id_nr = 2;
	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_AGAIN &&
	                     m0_fom_phase(fom) ==
			     M0_FOPH_IO_FOM_BUFFER_ACQUIRE);

	        /* Cleanup & make clean FOM for next test. */
	        fom_obj->fcrw_ndesc = saved_ndesc;
	        rwfop->crw_desc.id_nr = saved_ndesc;

	        /*
	         * Case 14 : All buffer descriptors are processed.
	         *         Input phase          : M0_FOPH_IO_BUFFER_RELEASE
	         *         Expected Output phase: M0_FOPH_SUCCESS
	         */
	        fom_phase_set(fom, M0_FOPH_IO_BUFFER_RELEASE);

	        rc = m0_io_fom_cob_rw_tick(fom);
	        M0_UT_ASSERT(m0_fom_rc(fom) == 0 &&
	                     rc == M0_FSO_AGAIN &&
	                     m0_fom_phase(fom) == M0_FOPH_SUCCESS);

	        fill_buffers_pool(colour);
	} else {
	        M0_UT_ASSERT(0); /* this should not happen */
	        rc = M0_FSO_WAIT; /* to avoid compiler warning */
	}

	return rc;
}

/* It is used to create the stob specified in the fid of each fop. */
static int bulkio_stob_create_fom_tick(struct m0_fom *fom)
{
	struct m0_fop_cob_rw         *rwfop;
	struct m0_stob_domain        *fom_stdom;
	struct m0_stob_id             stob_id;
	int			      rc;
	struct m0_fop_cob_writev_rep *wrep;
	struct m0_io_fom_cob_rw      *fom_obj;
	struct m0_fom_cob_op	      cc;
	struct m0_reqh_io_service    *ios;
	struct m0_cob_attr            attr = { {0, } };
	struct m0_cob_oikey           oikey;
	struct m0_cob                *cob;

	cob_attr_default_fill(&attr);
	fom_obj = container_of(fom, struct m0_io_fom_cob_rw, fcrw_gen);
	ios = container_of(fom->fo_service, struct m0_reqh_io_service,
			   rios_gen);

	rwfop = io_rw_get(fom->fo_fop);
	m0_fid_convert_cob2stob(&rwfop->crw_fid, &stob_id);
	fom_stdom = m0_stob_domain_find_by_stob_id(&stob_id);
	M0_UT_ASSERT(fom_stdom != NULL);

	if (m0_fom_phase(fom) < M0_FOPH_NR) {
		if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN) {
			m0_cob_tx_credit(ios->rios_cdom, M0_COB_OP_CREATE,
					 m0_fom_tx_credit(fom));
			m0_stob_create_credit(fom_stdom, m0_fom_tx_credit(fom));
		}
		return m0_fom_tick_generic(fom);
	}

	cc.fco_stob_id  = stob_id;
	cc.fco_gfid	= rwfop->crw_gfid;
	cc.fco_cfid	= rwfop->crw_fid;
	cc.fco_cob_idx	= (uint32_t) rwfop->crw_gfid.f_key;
	cc.fco_cob_type = M0_COB_IO;

	rc = m0_cc_cob_setup(&cc, ios->rios_cdom, &attr, m0_fom_tx(fom));
	M0_UT_ASSERT(rc == 0);

	rc = m0_stob_find(&stob_id, &fom_obj->fcrw_stob);
	M0_UT_ASSERT(rc == 0);
	rc = m0_stob_locate(fom_obj->fcrw_stob);
	M0_UT_ASSERT(rc == 0);
	rc = m0_stob_create(fom_obj->fcrw_stob, &fom->fo_tx, NULL);
	M0_UT_ASSERT(rc == 0);

	m0_cob_oikey_make(&oikey, &cc.fco_cfid, 0);
	rc = m0_cob_locate(ios->rios_cdom, &oikey, 0, &cob);
	M0_UT_ASSERT(rc == 0);

	wrep = m0_fop_data(fom->fo_rep_fop);
	wrep->c_rep.rwr_rc = 0;
	wrep->c_rep.rwr_count = rwfop->crw_ivec.ci_nr;
	m0_fom_phase_set(fom, M0_FOPH_SUCCESS);
	return M0_FSO_AGAIN;
}

static const struct m0_fom_ops bulkio_stob_create_fom_ops = {
	.fo_fini          = bulkio_stob_fom_fini,
	.fo_tick          = bulkio_stob_create_fom_tick,
	.fo_home_locality = m0_io_fom_cob_rw_locality_get
};

static const struct m0_fom_ops bulkio_server_write_fom_ops = {
	.fo_fini          = m0_io_fom_cob_rw_fini,
	.fo_tick          = bulkio_server_write_fom_tick,
	.fo_home_locality = m0_io_fom_cob_rw_locality_get
};

static const struct m0_fom_ops ut_io_fom_cob_rw_ops = {
	.fo_fini          = m0_io_fom_cob_rw_fini,
	.fo_tick          = ut_io_fom_cob_rw_state,
	.fo_home_locality = m0_io_fom_cob_rw_locality_get
};

static const struct m0_fom_ops bulkio_server_read_fom_ops = {
	.fo_fini          = m0_io_fom_cob_rw_fini,
	.fo_tick          = bulkio_server_read_fom_tick,
	.fo_home_locality = m0_io_fom_cob_rw_locality_get
};

static int io_fop_stob_create_fom_create(struct m0_fop  *fop,
					 struct m0_fom **m,
					 struct m0_reqh *reqh)
{
	int            rc;
	struct m0_fom *fom;

	rc = m0_io_fom_cob_rw_create(fop, &fom, reqh);
	M0_UT_ASSERT(rc == 0);
	fom->fo_ops = &bulkio_stob_create_fom_ops;
	*m = fom;

	return 0;
}

static int io_fop_server_write_fom_create(struct m0_fop  *fop,
					  struct m0_fom **m,
					  struct m0_reqh *reqh)
{
	int            rc;
	struct m0_fom *fom;

	rc = m0_io_fom_cob_rw_create(fop, &fom, reqh);
	M0_UT_ASSERT(rc == 0);
	fom->fo_ops = &bulkio_server_write_fom_ops;
	*m = fom;
	M0_UT_ASSERT(fom->fo_fop != 0);
	return rc;
}

/*
 * This creates FOM for ut.
 */
static int ut_io_fom_cob_rw_create(struct m0_fop *fop, struct m0_fom **m,
				   struct m0_reqh *reqh)
{
	int            rc;
	struct m0_fom *fom;

	/*
	 * Case : This tests the I/O FOM create api.
	 *        It use real I/O FOP
	 */
	rc = m0_io_fom_cob_rw_create(fop, &fom, reqh);
	M0_UT_ASSERT(rc == 0 &&
	             fom != NULL &&
	             fom->fo_rep_fop != NULL &&
	             fom->fo_fop != NULL &&
	             fom->fo_type != NULL &&
	             fom->fo_ops != NULL);

	fom->fo_ops = &ut_io_fom_cob_rw_ops;
	*m = fom;
	M0_UT_ASSERT(fom->fo_fop != 0);
	return rc;
}

static int io_fop_server_read_fom_create(struct m0_fop  *fop,
					 struct m0_fom **m,
					 struct m0_reqh *reqh)
{
	int            rc;
	struct m0_fom *fom;

	rc = m0_io_fom_cob_rw_create(fop, &fom, reqh);
	M0_UT_ASSERT(rc == 0);
	fom->fo_ops = &bulkio_server_read_fom_ops;
	*m = fom;
	M0_UT_ASSERT(fom->fo_fop != 0);
	return rc;
}

static void bulkio_stob_create(void)
{
	struct m0_fop_cob_rw *rw;
	enum M0_RPC_OPCODES   op;
	struct thrd_arg       targ[IO_FIDS_NR];
	int		      i;
	int		      rc;

	op = M0_IOSERVICE_WRITEV_OPCODE;
	M0_ALLOC_ARR(bp->bp_wfops, IO_FIDS_NR);
	bp->bp_wfops_nr = IO_FIDS_NR;
	for (i = 0; i < IO_FIDS_NR; ++i) {
		M0_ALLOC_PTR(bp->bp_wfops[i]);
		rc = m0_io_fop_init(bp->bp_wfops[i], &bp->bp_fids[i],
				    &m0_fop_cob_writev_fopt,
				    NULL);
		M0_UT_ASSERT(rc == 0);
		/*
		 * We replace the original ->ft_ops and ->ft_fom_type for
		 * regular io_fops. This is reset later.
		 */
		bp->bp_wfops[i]->if_fop.f_type->ft_fom_type.ft_ops =
			&bulkio_stob_create_fomt_ops;
		bp->bp_wfops[i]->if_fop.f_type->ft_fom_type.ft_conf =
			m0_generic_conf;
		rw = io_rw_get(&bp->bp_wfops[i]->if_fop);
		bp->bp_wfops[i]->if_fop.f_type->ft_ops =
			&bulkio_stob_create_ops;
		m0_file_init(&bp->bp_file[i], &bp->bp_fids[i], &bp->bp_rdom,
			     M0_DI_CRC32_4K);
		rw->crw_fid = bp->bp_fids[i];
		targ[i].ta_index = i;
		targ[i].ta_op = op;
		targ[i].ta_bp = bp;
		io_fops_rpc_submit(&targ[i]);
	}
	io_fops_destroy(bp);
}

static void io_fops_submit(uint32_t index, enum M0_RPC_OPCODES op)
{
	struct thrd_arg targ = {};

	targ.ta_index = index;
	targ.ta_op = op;
	targ.ta_bp = bp;
	io_fops_rpc_submit(&targ);
}

static void io_single_fop_submit(enum M0_RPC_OPCODES op)
{
	struct m0_fop *fop;

	io_fops_create(bp, op, 1, 1, IO_SEGS_NR);
	if (op == M0_IOSERVICE_WRITEV_OPCODE) {
		fop = &bp->bp_wfops[0]->if_fop;
		fop->f_type->ft_fom_type.ft_ops = &bulkio_server_write_fomt_ops;
	} else {
		fop = &bp->bp_rfops[0]->if_fop;
		fop->f_type->ft_fom_type.ft_ops = &io_fom_type_ops;
	}
	/*
	 * Here we replace the original ->ft_ops and ->ft_fom_type as they were
	 * changed during bulkio_stob_create test.
	 */
	fop->f_type->ft_ops = &io_fop_rwv_ops;
	fop->f_type->ft_fom_type.ft_conf = io_conf;
	io_fops_submit(0, op);
}

static void bulkio_server_single_read_write(void)
{
	int		  j;
	struct m0_bufvec *buf;

	buf = &bp->bp_iobuf[0]->nb_buffer;
	for (j = 0; j < IO_SEGS_NR; ++j) {
		memset(buf->ov_buf[j], 'b', IO_SEG_SIZE);
	}
	fol_check_enabled = true;
	io_single_fop_submit(M0_IOSERVICE_WRITEV_OPCODE);

	buf = &bp->bp_iobuf[0]->nb_buffer;
	for (j = 0; j < IO_SEGS_NR; ++j) {
		memset(buf->ov_buf[j], 'a', IO_SEG_SIZE);
	}
	io_single_fop_submit(M0_IOSERVICE_READV_OPCODE);
	fol_check_enabled = false;
}

#define WRITE_FOP_DATA(fop) M0_XCODE_OBJ(m0_fop_cob_writev_xc, fop)

static void bulkio_server_write_fol_rec_verify(void)
{
	struct m0_reqh		 *reqh;
	struct m0_fol_rec	  dec_rec;
	int			  result;
	struct m0_fol_frag	 *dec_frag;
	struct m0_fop		 *fop;
	struct m0_fop_cob_writev *wfop;

	fop = &bp->bp_wfops[0]->if_fop;
	wfop = (struct m0_fop_cob_writev *)m0_fop_data(fop);

	reqh = m0_cs_reqh_get(&bp->bp_sctx->rsx_mero_ctx);
	M0_LOG(M0_DEBUG, "payload_buf=" BUF_F, BUF_P(&payload_buf));
	m0_fol_rec_init(&dec_rec, &reqh->rh_fol);
	result = m0_fol_rec_decode(&dec_rec, &payload_buf);
	M0_UT_ASSERT(result == 0);

	/* FOL record frags are 2 for AD stob type and 1 for LINUX stob type. */
	M0_UT_ASSERT(dec_rec.fr_header.rh_frags_nr == 1 ||
		     dec_rec.fr_header.rh_frags_nr == 2);
	m0_tl_for(m0_rec_frag, &dec_rec.fr_frags, dec_frag) {
		struct m0_fop_fol_frag *fp_frag = dec_frag->rp_data;

		if (dec_frag->rp_ops->rpo_type->rpt_index ==
		    m0_fop_fol_frag_type.rpt_index &&
		    fp_frag->ffrp_fop_code == M0_IOSERVICE_WRITEV_OPCODE) {
			struct m0_fop_cob_writev_rep *wfop_rep;

			M0_UT_ASSERT(m0_xcode_cmp(
			     &WRITE_FOP_DATA(fp_frag->ffrp_fop),
			     &WRITE_FOP_DATA(wfop)) == 0);
			wfop_rep = fp_frag->ffrp_rep;
			M0_UT_ASSERT(wfop_rep->c_rep.rwr_rc == 0);
			M0_UT_ASSERT(wfop_rep->c_rep.rwr_count > 0);
		}
	} m0_tl_endfor;

	m0_fol_rec_fini(&dec_rec);
	io_fops_destroy(bp);
}

static void bulkio_server_write_fol_rec_undo_verify(void)
{
	int			j;
	struct m0_bufvec       *buf;
	struct m0_reqh	       *reqh;
	struct m0_fol_rec	dec_rec;
	struct m0_dtx           dtx;
	struct m0_sm_group     *grp = m0_locality0_get()->lo_grp;
	int			result;
	struct m0_fol_frag     *dec_frag;
	struct m0_buf           save_buf = M0_BUF_INIT0;
	/* m0_get()->i_reqh_uses_ad_stob */
	bool                    stob_ad = m0_cs_storage_devs_get() != NULL;

	if (!stob_ad)
		return;

	buf = &bp->bp_iobuf[0]->nb_buffer;
	/* Write data "b" in the file. */
	for (j = 0; j < IO_SEGS_NR; ++j) {
		memset(buf->ov_buf[j], 'b', IO_SEG_SIZE);
	}
	io_single_fop_submit(M0_IOSERVICE_WRITEV_OPCODE);
	io_fops_destroy(bp);
	result = m0_buf_copy(&save_buf, &payload_buf);
	M0_UT_ASSERT(result == 0);
	io_single_fop_submit(M0_IOSERVICE_READV_OPCODE);
	io_fops_destroy(bp);

	/* Write data "a" in the file. */
	for (j = 0; j < IO_SEGS_NR; ++j) {
		memset(buf->ov_buf[j], 'a', IO_SEG_SIZE);
	}
	io_single_fop_submit(M0_IOSERVICE_WRITEV_OPCODE);
	io_fops_destroy(bp);

	/* Undo the last write, so that file contains data "b". */
	reqh = m0_cs_reqh_get(&bp->bp_sctx->rsx_mero_ctx);
	M0_UT_ASSERT(reqh != NULL);
	m0_reqh_idle_wait(reqh);

	m0_fol_rec_init(&dec_rec, &reqh->rh_fol);
	result = m0_fol_rec_decode(&dec_rec, &save_buf);
	M0_UT_ASSERT(result == 0);
	m0_buf_free(&save_buf);

	M0_UT_ASSERT(dec_rec.fr_header.rh_frags_nr == 1 ||
		     dec_rec.fr_header.rh_frags_nr == 2);
	m0_tl_for(m0_rec_frag, &dec_rec.fr_frags, dec_frag) {
		if (dec_frag->rp_ops->rpo_type->rpt_index ==
		    m0_fop_fol_frag_type.rpt_index) {
			struct m0_fop_fol_frag *fp_frag;
			struct m0_fop_type     *ftype;

			fp_frag = dec_frag->rp_data;
			M0_UT_ASSERT(fp_frag->ffrp_fop_code ==
				     M0_IOSERVICE_WRITEV_OPCODE);

			ftype = m0_fop_type_find(fp_frag->ffrp_fop_code);
			M0_UT_ASSERT(ftype != NULL);
			M0_UT_ASSERT(ftype->ft_ops->fto_undo != NULL &&
				     ftype->ft_ops->fto_redo != NULL);
			result = ftype->ft_ops->fto_undo(fp_frag,
							 &reqh->rh_fol);
		} else {
			m0_sm_group_lock(grp);
			M0_SET0(&dtx);
			m0_dtx_init(&dtx, reqh->rh_beseg->bs_domain, grp);
			dec_frag->rp_ops->rpo_undo_credit(dec_frag,
				&dtx.tx_betx_cred);
			m0_dtx_open_sync(&dtx);
			result = dec_frag->rp_ops->rpo_undo(dec_frag,
							    &dtx.tx_betx);
			m0_dtx_done_sync(&dtx);
			m0_dtx_fini(&dtx);
			m0_sm_group_unlock(grp);
		}
		M0_UT_ASSERT(result == 0);
	} m0_tl_endfor;
	m0_fol_rec_fini(&dec_rec);

	/* Read that data from file and compare it with data "b". */
	io_single_fop_submit(M0_IOSERVICE_READV_OPCODE);
	io_fops_destroy(bp);
	m0_reqh_idle_wait(reqh);
}

/*
 * Sends regular write and read io fops, although replaces the original FOM
 * types in each io fop type with UT specific FOM types.
 */
static void bulkio_server_read_write_state_test(void)
{
	int		    j;
	enum M0_RPC_OPCODES op;
	struct m0_bufvec   *buf;
	struct m0_reqh     *reqh;

	buf = &bp->bp_iobuf[0]->nb_buffer;
	for (j = 0; j < IO_SEGS_NR; ++j) {
		memset(buf->ov_buf[j], 'b', IO_SEG_SIZE);
	}
	op = M0_IOSERVICE_WRITEV_OPCODE;
	io_fops_create(bp, op, 1, 1, IO_SEGS_NR);
	bp->bp_wfops[0]->if_fop.f_type->ft_fom_type.ft_ops =
		&bulkio_server_write_fomt_ops;
	bp->bp_wfops[0]->if_fop.f_type->ft_ops =
		&bulkio_server_write_fop_ut_ops;
	io_fops_submit(0, op);
	io_fops_destroy(bp);
	reqh = m0_cs_reqh_get(&bp->bp_sctx->rsx_mero_ctx);
	m0_reqh_idle_wait(reqh);

	buf = &bp->bp_iobuf[0]->nb_buffer;
	for (j = 0; j < IO_SEGS_NR; ++j) {
		memset(buf->ov_buf[j], 'a', IO_SEG_SIZE);
	}
	op = M0_IOSERVICE_READV_OPCODE;
	io_fops_create(bp, op, 1, 1, IO_SEGS_NR);
	bp->bp_rfops[0]->if_fop.f_type->ft_fom_type.ft_ops =
		&bulkio_server_read_fomt_ops;
	bp->bp_rfops[0]->if_fop.f_type->ft_ops =
		&bulkio_server_read_fop_ut_ops;
	io_fops_submit(0, op);
	io_fops_destroy(bp);
	m0_reqh_idle_wait(reqh);
}

/*
 * Sends regular write and read fops although replaces the original FOM types
 * in each io fop type with UT specific FOM types to check state transition for
 * I/O FOM.
 */
static void bulkio_server_rw_state_transition_test(void)
{
	int		    j;
	enum M0_RPC_OPCODES op;
	struct m0_bufvec   *buf;
	struct m0_reqh     *reqh;

	buf = &bp->bp_iobuf[0]->nb_buffer;
	for (j = 0; j < IO_SEGS_NR; ++j) {
		memset(buf->ov_buf[j], 'b', IO_SEG_SIZE);
	}
	op = M0_IOSERVICE_WRITEV_OPCODE;
	io_fops_create(bp, op, 1, 1, IO_SEGS_NR);
	bp->bp_wfops[0]->if_fop.f_type->ft_fom_type.ft_ops =
		&ut_io_fom_cob_rw_type_ops;
	bp->bp_wfops[0]->if_fop.f_type->ft_ops =
		&bulkio_server_write_fop_ut_ops;
	io_fops_submit(0, op);
	io_fops_destroy(bp);
	reqh = m0_cs_reqh_get(&bp->bp_sctx->rsx_mero_ctx);
	m0_reqh_idle_wait(reqh);

	buf = &bp->bp_iobuf[0]->nb_buffer;
	for (j = 0; j < IO_SEGS_NR; ++j) {
		memset(buf->ov_buf[j], 'a', IO_SEG_SIZE);
	}
	op = M0_IOSERVICE_READV_OPCODE;
	io_fops_create(bp, op, 1, 1, IO_SEGS_NR);
	bp->bp_rfops[0]->if_fop.f_type->ft_fom_type.ft_ops =
		&ut_io_fom_cob_rw_type_ops;
	bp->bp_rfops[0]->if_fop.f_type->ft_ops =
		&bulkio_server_read_fop_ut_ops;
	io_fops_submit(0, op);
	io_fops_destroy(bp);
	m0_reqh_idle_wait(reqh);
}

/**
 * Sends several read/write fops to the ioservice and keeps the ID of the
 * last transaction generated by them. The received ID is then used to send an
 * fsync fop request that commits transactions that are lingering in the
 * ioservice.
 */
static void bulkio_server_fsync_multiple_read_write(void)
{
	int                     i;
	int                     j;
	enum M0_RPC_OPCODES     op;
	struct thrd_arg         targ[IO_FOPS_NR];
	struct m0_bufvec       *buf;
	struct m0_be_tx_remid   remid;
	int                     rc;
	struct m0_reqh         *reqh;

	for (i = 0; i < IO_FOPS_NR; ++i) {
		buf = &bp->bp_iobuf[i]->nb_buffer;
		for (j = 0; j < IO_SEGS_NR; ++j) {
			memset(buf->ov_buf[j], 'b', IO_SEG_SIZE);
		}
	}
	for (op = M0_IOSERVICE_WRITEV_OPCODE; op >= M0_IOSERVICE_READV_OPCODE;
	     --op) {
		/*
		 * IO fops are deallocated by an rpc item type op on receiving
		 * the reply fop. See io_item_free().
		 */
		io_fops_create(bp, op, IO_FIDS_NR, IO_FOPS_NR, IO_SEGS_NR);
		for (i = 0; i < IO_FOPS_NR; ++i) {
			targ[i].ta_index = i;
			targ[i].ta_op = op;
			targ[i].ta_bp = bp;
			io_fops_rpc_submit(&targ[i]);
			remid = targ[i].ta_bp->bp_remid;
		}
		io_fops_destroy(bp);
	}

	/*
	 * Send the fsync fop request.
	 * targ[0] suffices, since it contains the session.
	 */
	rc = io_fsync_send_fop(&remid, &targ[0]);
	M0_UT_ASSERT(rc == 0);
	reqh = m0_cs_reqh_get(&bp->bp_sctx->rsx_mero_ctx);
	m0_reqh_idle_wait(reqh);
}

static void bulkio_server_multiple_read_write(void)
{
	int		     rc;
	int		     i;
	int		     j;
	enum M0_RPC_OPCODES  op;
	struct thrd_arg      targ[IO_FOPS_NR];
	struct m0_bufvec    *buf;
	struct m0_reqh      *reqh;

	for (i = 0; i < IO_FOPS_NR; ++i) {
		buf = &bp->bp_iobuf[i]->nb_buffer;
		for (j = 0; j < IO_SEGS_NR; ++j) {
			memset(buf->ov_buf[j], 'b', IO_SEG_SIZE);
		}
	}
	for (op = M0_IOSERVICE_WRITEV_OPCODE; op >= M0_IOSERVICE_READV_OPCODE;
	  --op) {
		/*
		 * IO fops are deallocated by an rpc item type op on receiving
		 * the reply fop. See io_item_free().
		 */
		io_fops_create(bp, op, IO_FIDS_NR, IO_FOPS_NR, IO_SEGS_NR);
		for (i = 0; i < IO_FOPS_NR; ++i) {
			targ[i].ta_index = i;
			targ[i].ta_op = op;
			targ[i].ta_bp = bp;
			M0_SET0(bp->bp_threads[i]);
			rc = M0_THREAD_INIT(bp->bp_threads[i],
					    struct thrd_arg *,
					    NULL, &io_fops_rpc_submit,
					    &targ[i], "io_thrd");
			M0_UT_ASSERT(rc == 0);
		}
		/* Waits till all threads finish their job. */
		for (i = 0; i < IO_FOPS_NR; ++i) {
			m0_thread_join(bp->bp_threads[i]);
			buf = &bp->bp_iobuf[i]->nb_buffer;
			for (j = 0; j < IO_SEGS_NR; ++j) {
				memset(buf->ov_buf[j], 'a', IO_SEG_SIZE);
			}
		}
	        io_fops_destroy(bp);
	}
	reqh = m0_cs_reqh_get(&bp->bp_sctx->rsx_mero_ctx);
	m0_reqh_idle_wait(reqh);
}

static void fop_create_populate(int index, enum M0_RPC_OPCODES op, int buf_nr)
{
	struct m0_io_fop       **io_fops;
	struct m0_rpc_bulk_buf	*rbuf;
	struct m0_rpc_bulk	*rbulk;
	struct m0_io_fop	*iofop;
	struct m0_fop_cob_rw	*rw;
	int                      i;
	int			 j;
	int			 rc;

	if (op == M0_IOSERVICE_WRITEV_OPCODE) {
		M0_ALLOC_ARR(bp->bp_wfops, IO_FOPS_NR);
		bp->bp_wfops_nr = IO_FOPS_NR;
	} else {
		M0_ALLOC_ARR(bp->bp_rfops, IO_FOPS_NR);
		bp->bp_rfops_nr = IO_FOPS_NR;
	}

	io_fops = (op == M0_IOSERVICE_WRITEV_OPCODE) ? bp->bp_wfops :
						       bp->bp_rfops;
	M0_ALLOC_PTR(io_fops[index]);

	if (op == M0_IOSERVICE_WRITEV_OPCODE)
		rc = m0_io_fop_init(io_fops[index], &bp->bp_fids[0],
				    &m0_fop_cob_writev_fopt, NULL);
	else
		rc = m0_io_fop_init(io_fops[index], &bp->bp_fids[0],
				    &m0_fop_cob_readv_fopt, NULL);
	iofop = io_fops[index];
	rbulk = &iofop->if_rbulk;
	rw = io_rw_get(&io_fops[index]->if_fop);

	rw->crw_fid = bp->bp_fids[0];
	rw->crw_index = m0_fid_cob_device_id(&bp->bp_fids[0]);
	rw->crw_pver = CONF_PVER_FID;
	bp->bp_offsets[0] = IO_SEG_START_OFFSET;

	void add_buffer_bulk(int j)
	{
		/*
		 * Adds a m0_rpc_bulk_buf structure to list of such structures
		 * in m0_rpc_bulk.
		 */
		rc = m0_rpc_bulk_buf_add(rbulk, IO_SEGS_NR, 0, &bp->bp_cnetdom,
					 NULL, &rbuf);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(rbuf != NULL);

		/* Adds io buffers to m0_rpc_bulk_buf structure. */
		for (i = 0; i < IO_SEGS_NR; ++i) {
			rc = m0_rpc_bulk_buf_databuf_add(rbuf,
				 bp->bp_iobuf[j]->nb_buffer.ov_buf[i],
				 bp->bp_iobuf[j]->nb_buffer.ov_vec.v_count[i],
				 bp->bp_offsets[0], &bp->bp_cnetdom);
			M0_UT_ASSERT(rc == 0);
			bp->bp_offsets[0] +=
				bp->bp_iobuf[j]->nb_buffer.ov_vec.v_count[i];
		}
		bp->bp_offsets[0] += IO_SEG_SIZE;

		rbuf->bb_nbuf->nb_qtype = (op == M0_IOSERVICE_WRITEV_OPCODE) ?
			M0_NET_QT_PASSIVE_BULK_SEND :
			M0_NET_QT_PASSIVE_BULK_RECV;
	}

	for (j = 0; j < buf_nr; ++j)
		add_buffer_bulk(j);

	/*
	 * Allocates memory for array of net buf descriptors and array of
	 * index vectors from io fop.
	 */
	rc = m0_io_fop_prepare(&iofop->if_fop);
	M0_UT_ASSERT(rc == 0);

	/*
	 * Stores the net buf desc/s after adding the corresponding
	 * net buffers to transfer machine to io fop wire format.
	 */
	rc = m0_rpc_bulk_store(rbulk, &bp->bp_cctx->rcx_connection,
			       rw->crw_desc.id_descs, &m0_rpc__buf_bulk_cb);
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < IO_FIDS_NR; ++i)
		bp->bp_offsets[i] = IO_SEG_START_OFFSET;
}

static void bulkio_server_read_write_multiple_nb(void)
{
	int		    i;
	int		    j;
	int		    buf_nr;
	enum M0_RPC_OPCODES op;
	struct m0_bufvec   *buf;
	struct m0_reqh     *reqh;

	buf_nr = IO_FOPS_NR / 4;
	for (i = 0; i < buf_nr; ++i) {
		buf = &bp->bp_iobuf[i]->nb_buffer;
		for (j = 0; j < IO_SEGS_NR; ++j) {
			memset(buf->ov_buf[j], 'b', IO_SEG_SIZE);
		}
	}
	op = M0_IOSERVICE_WRITEV_OPCODE;
	fop_create_populate(0, op, buf_nr);
	bp->bp_wfops[0]->if_fop.f_type->ft_ops = &io_fop_rwv_ops;
	io_fops_submit(0, op);
	io_fops_destroy(bp);
	reqh = m0_cs_reqh_get(&bp->bp_sctx->rsx_mero_ctx);
	m0_reqh_idle_wait(reqh);

	for (i = 0; i < buf_nr; ++i) {
		buf = &bp->bp_iobuf[i]->nb_buffer;
		for (j = 0; j < IO_SEGS_NR; ++j) {
			memset(buf->ov_buf[j], 'a', IO_SEG_SIZE);
		}
	}
	op = M0_IOSERVICE_READV_OPCODE;
	fop_create_populate(0, op, buf_nr);
	bp->bp_rfops[0]->if_fop.f_type->ft_ops = &io_fop_rwv_ops;
	io_fops_submit(0, op);
	io_fops_destroy(bp);
	m0_reqh_idle_wait(reqh);
}

static void bulkio_init(void)
{
	int         rc;
	const char *caddr = "0@lo:12345:34:*";
	const char *saddr = "0@lo:12345:34:1";

	/*
	 * Current set of tests work with standalone io_fops, but
	 * io_fop_di_prepare() relies on the fact that an io_fop is embedded
	 * into the io_req_fop structure. Therefore, we have to skip DI prepare
	 * for the tests in order to avoid crashes.
	 */
	m0_fi_enable("io_fop_di_prepare", "skip_di_for_ut");

	M0_ALLOC_PTR(bp);
	M0_ASSERT(bp != NULL);
	bulkio_params_init(bp);

	rc = bulkio_server_start(bp, saddr);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(bp->bp_sctx != NULL);
	rc = bulkio_client_start(bp, caddr, saddr);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(bp->bp_cctx != NULL);

	bulkio_stob_create();
}

static void bulkio_fini(void)
{
	struct m0_reqh *reqh;
	for (i = 0; i < IO_FIDS_NR; ++i)
		m0_file_fini(&bp->bp_file[i]);
	reqh = m0_cs_reqh_get(&bp->bp_sctx->rsx_mero_ctx);
	m0_reqh_idle_wait(reqh);
	bulkio_client_stop(bp->bp_cctx);
	bulkio_server_stop(bp->bp_sctx);
	bulkio_params_fini(bp);
	m0_free(bp);

	m0_fi_disable("io_fop_di_prepare", "skip_di_for_ut");
}

/*
 * Only used for user-space UT.
 */
struct m0_ut_suite bulkio_server_ut = {
	.ts_name = "bulk-server-ut",
	.ts_tests = {
		/*
		 * Intentionally kept as first test case. It initializes
		 * all necessary data for sending IO fops. Keeping
		 * bulkio_init() as .ts_init requires changing all
		 * M0_UT_ASSERTS to M0_ASSERTS.
		 */
		{ "bulkio_init", bulkio_init},
		{ "bulkio_server_single_read_write",
		   bulkio_server_single_read_write},
		{ "bulkio_server_write_fol_rec_verify",
		   bulkio_server_write_fol_rec_verify},
		{ "bulkio_server_write_fol_rec_undo_verify",
		   bulkio_server_write_fol_rec_undo_verify},
		{ "bulkio_server_read_write_state_test",
		   bulkio_server_read_write_state_test},
		{ "bulkio_server_vectored_read_write",
		   bulkio_server_multiple_read_write},
		/*
		 * Keep this test close to bulkio_server_multiple_read_write.
		 * Otherwise something breaks :)
		 */
		{ "bulkio_server_fsync_multiple_read_write",
		   bulkio_server_fsync_multiple_read_write},
		{ "bulkio_server_rw_multiple_nb_server",
		   bulkio_server_read_write_multiple_nb},
		{ "bulkio_server_rw_state_transition_test",
		   bulkio_server_rw_state_transition_test},
/** @todo: MERO-1502: When HA will be in place we no longer require
 *       VERSION_MISMATCH error as server/client will
 *       fetch latestpool machine states from HA at every crash/reboot.
 *       Please visit the Jira page for more details.
 *       Commenting this now as a cleaner approach to ignore it is being
 *       handeled as part of MERO-1502.
 */
#if 0

		{ "bulkio_server_read_write_fv_mismatch",
		   bulkio_server_read_write_fv_mismatch},
#endif
		{ "bulkio_fini", bulkio_fini},
		{ NULL, NULL }
	}
};
M0_EXPORTED(bulkio_server_ut);
#undef M0_TRACE_SUBSYSTEM
