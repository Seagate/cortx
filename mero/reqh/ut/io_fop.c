/* -*- C -*- */
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

#include "lib/misc.h"
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/chan.h"
#include "lib/list.h"

#include "balloc/balloc.h"

#include "net/net.h"
#include "fop/fop.h"
#include "reqh/reqh.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"
#include "stob/stob.h"
#include "stob/ad.h"
#include "rpc/rpc.h"
#include "rpc/rpc_opcodes.h"
#include "fop/fop_item_type.h"
#include "reqh/ut/io_fop.h"
#include "reqh/ut/io_fop_xc.h"
#include "ut/ut.h"

/**
   @defgroup stobio
   @{
 */

extern struct m0_reqh_service_type m0_rpc_service_type;
/**
 * Create, Write and Read fop specific fom execution phases
 */
enum stob_fom_phases {
	M0_FOPH_CREATE_STOB  = M0_FOPH_NR + 1,
	M0_FOPH_READ_STOB_IO = M0_FOPH_NR + 1,
	M0_FOPH_READ_STOB_IO_WAIT,
	M0_FOPH_WRITE_STOB_IO = M0_FOPH_NR + 1,
	M0_FOPH_WRITE_STOB_IO_WAIT
};

struct m0_fop_type m0_stob_io_create_fopt;
struct m0_fop_type m0_stob_io_read_fopt;
struct m0_fop_type m0_stob_io_write_fopt;
struct m0_fop_type m0_stob_io_create_rep_fopt;
struct m0_fop_type m0_stob_io_read_rep_fopt;
struct m0_fop_type m0_stob_io_write_rep_fopt;

struct m0_sm_state_descr stob_create_phases[] = {
	[M0_FOPH_CREATE_STOB] = {
		.sd_name      = "Create stob",
		.sd_allowed   = (1 << M0_FOPH_SUCCESS) |
				(1 << M0_FOPH_FAILURE)
	},
};

struct m0_sm_state_descr stob_read_phases[] = {
	[M0_FOPH_READ_STOB_IO] = {
		.sd_name      = "Read stob",
		.sd_allowed   = (1 << M0_FOPH_READ_STOB_IO_WAIT) |
				(1 << M0_FOPH_FAILURE)
	},
	[M0_FOPH_READ_STOB_IO_WAIT] = {
		.sd_name      = "Read stob wait",
		.sd_allowed   = (1 << M0_FOPH_SUCCESS)
	},
};

const struct m0_sm_conf read_conf = {
	.scf_name      = "Stob read",
	.scf_nr_states = ARRAY_SIZE(stob_read_phases),
	.scf_state     = stob_read_phases,
};

struct m0_sm_state_descr stob_write_phases[] = {
	[M0_FOPH_READ_STOB_IO] = {
		.sd_name      = "Write stob",
		.sd_allowed   = (1 << M0_FOPH_READ_STOB_IO_WAIT) |
				(1 << M0_FOPH_FAILURE)
	},
	[M0_FOPH_READ_STOB_IO_WAIT] = {
		.sd_name      = "Write stob wait",
		.sd_allowed   = (1 << M0_FOPH_SUCCESS)
	},
};

const struct m0_sm_conf write_conf = {
	.scf_name      = "Stob write",
	.scf_nr_states = ARRAY_SIZE(stob_write_phases),
	.scf_state     = stob_write_phases,
};

/**
 * A generic fom structure to hold fom, reply fop, storage object
 * and storage io object, used for fop execution.
 */
struct m0_stob_io_fom {
	/** Generic m0_fom object. */
	struct m0_fom			 sif_fom;
	/** Reply FOP associated with request FOP above. */
	struct m0_fop			*sif_rep_fop;
	/** Stob object on which this FOM is acting. */
	struct m0_stob			*sif_stobj;
	/** Stob IO packet for the operation. */
	struct m0_stob_io		 sif_stio;
	/**
	 * Fol record part representing stob io operations.
	 * It should be pointed by m0_stob_io::si_fol_frag.
	 */
	struct m0_fol_frag		 sif_fol_frag;
};

extern struct m0_stob_domain *reqh_ut_stob_domain_find(void);
static int stob_create_fom_create(struct m0_fop *fop, struct m0_fom **out,
				  struct m0_reqh *reqh);
static int stob_read_fom_create(struct m0_fop *fop, struct m0_fom **out,
				struct m0_reqh *reqh);
static int stob_write_fom_create(struct m0_fop *fop, struct m0_fom **out,
				 struct m0_reqh *reqh);

static int stob_create_fom_tick(struct m0_fom *fom);
static int stob_read_fom_tick(struct m0_fom *fom);
static int stob_write_fom_tick(struct m0_fom *fom);

static void stob_io_fom_fini(struct m0_fom *fom);
static size_t stob_find_fom_home_locality(const struct m0_fom *fom);

/**
 * Operation structures for respective foms
 */
static struct m0_fom_ops stob_create_fom_ops = {
	.fo_fini = stob_io_fom_fini,
	.fo_tick = stob_create_fom_tick,
	.fo_home_locality = stob_find_fom_home_locality
};

static struct m0_fom_ops stob_write_fom_ops = {
	.fo_fini = stob_io_fom_fini,
	.fo_tick = stob_write_fom_tick,
	.fo_home_locality = stob_find_fom_home_locality
};

static struct m0_fom_ops stob_read_fom_ops = {
	.fo_fini = stob_io_fom_fini,
	.fo_tick = stob_read_fom_tick,
	.fo_home_locality = stob_find_fom_home_locality
};

/**
 * Fom type operations structures for corresponding foms.
 */
static const struct m0_fom_type_ops stob_create_fom_type_ops = {
	.fto_create = stob_create_fom_create,
};

static const struct m0_fom_type_ops stob_read_fom_type_ops = {
	.fto_create = stob_read_fom_create,
};

static const struct m0_fom_type_ops stob_write_fom_type_ops = {
	.fto_create = stob_write_fom_create,
};

/**
 * Function to locate a storage object.
 */
static struct m0_stob *stob_object_find(const struct stob_io_fop_fid *fid,
					struct m0_fom *fom)
{
	struct m0_stob   *stob;
	int		  rc;
	struct m0_stob_id stob_id;

	m0_stob_id_make(0, fid->f_oid, &reqh_ut_stob_domain_find()->sd_id,
			&stob_id);
	rc = m0_stob_find(&stob_id, &stob);
	M0_ASSERT(rc == 0);
	rc = m0_stob_state_get(stob) == CSS_UNKNOWN ? m0_stob_locate(stob) : 0;
	M0_ASSERT(rc == 0);
	return stob;
}

/**
 * Fom initialization function, invoked from reqh_fop_handle.
 * Invokes m0_fom_init()
 */
static int stob_io_fop_fom_create_helper(struct m0_fop *fop,
		struct m0_fom_ops *fom_ops, struct m0_fop_type *fop_type,
		struct m0_fom **out, struct m0_reqh *reqh)
{
	struct m0_stob_io_fom *fom_obj;

	M0_PRE(fop != NULL);
	M0_PRE(fom_ops != NULL);
	M0_PRE(fop_type != NULL);
	M0_PRE(out != NULL);

	fom_obj = m0_alloc(sizeof *fom_obj);
	if (fom_obj == NULL)
		return -ENOMEM;
	fom_obj->sif_rep_fop = m0_fop_reply_alloc(fop, fop_type);
	if (fom_obj->sif_rep_fop == NULL) {
		m0_free(fom_obj);
		return -ENOMEM;
	}
	fom_obj->sif_stobj = NULL;

	m0_fom_init(&fom_obj->sif_fom, &fop->f_type->ft_fom_type, fom_ops, fop,
		    fom_obj->sif_rep_fop, reqh);

	*out = &fom_obj->sif_fom;
	return 0;
}

/**
 * Creates a fom for create fop.
 */
static int stob_create_fom_create(struct m0_fop *fop, struct m0_fom **out,
				  struct m0_reqh *reqh)
{
	return stob_io_fop_fom_create_helper(fop, &stob_create_fom_ops,
				      &m0_stob_io_create_rep_fopt, out, reqh);
}

/**
 * Creates a fom for write fop.
 */
static int stob_write_fom_create(struct m0_fop *fop, struct m0_fom **out,
				 struct m0_reqh *reqh)
{
	return stob_io_fop_fom_create_helper(fop, &stob_write_fom_ops,
				      &m0_stob_io_write_rep_fopt, out, reqh);
}

/**
 * Creates a fom for read fop.
 */
static int stob_read_fom_create(struct m0_fop *fop, struct m0_fom **out,
				struct m0_reqh *reqh)
{
	return stob_io_fop_fom_create_helper(fop, &stob_read_fom_ops,
				      &m0_stob_io_read_rep_fopt, out, reqh);
}

/**
 * Finds home locality for this type of fom.
 * This function, using a basic hashing method
 * locates a home locality for a particular type
 * of fome, inorder to have same locality of
 * execution for a certain type of fom.
 */
static size_t stob_find_fom_home_locality(const struct m0_fom *fom)
{
	size_t iloc;

	if (fom == NULL)
		return -EINVAL;

	switch (m0_fop_opcode(fom->fo_fop)) {
	case M0_STOB_IO_CREATE_REQ_OPCODE: {
		struct m0_stob_io_create *fop;
		uint64_t oid;
		fop = m0_fop_data(fom->fo_fop);
		oid = fop->fic_object.f_oid;
		iloc = oid;
		break;
	}
	case M0_STOB_IO_WRITE_REQ_OPCODE: {
		struct m0_stob_io_write *fop;
		uint64_t oid;
		fop = m0_fop_data(fom->fo_fop);
		oid = fop->fiw_object.f_oid;
		iloc = oid;
		break;
	}
	case M0_STOB_IO_READ_REQ_OPCODE: {
		struct m0_stob_io_read *fop;
		uint64_t oid;
		fop = m0_fop_data(fom->fo_fop);
		oid = fop->fir_object.f_oid;
		iloc = oid;
		break;
	}
	default:
		return -EINVAL;
	}
	return iloc;
}

/**
 * A simple non blocking create fop specific fom
 * state method implemention.
 */
static int stob_create_fom_tick(struct m0_fom *fom)
{
	struct m0_stob_io_create	*in_fop;
	struct m0_stob_io_create_rep	*out_fop;
	struct m0_stob_io_fom		*fom_obj;
	struct m0_rpc_item              *item;
	struct m0_fop                   *fop;
	int                              result;

	M0_PRE(m0_fop_opcode(fom->fo_fop) == M0_STOB_IO_CREATE_REQ_OPCODE);

	fom_obj = container_of(fom, struct m0_stob_io_fom, sif_fom);
	if (m0_fom_phase(fom) < M0_FOPH_NR) {
		if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN) {
			m0_stob_create_credit(reqh_ut_stob_domain_find(),
					      m0_fom_tx_credit(fom));
		}
		result = m0_fom_tick_generic(fom);
	} else {
		in_fop = m0_fop_data(fom->fo_fop);
		out_fop = m0_fop_data(fom_obj->sif_rep_fop);

		fom_obj->sif_stobj = stob_object_find(&in_fop->fic_object, fom);

		result = m0_stob_create(fom_obj->sif_stobj, &fom->fo_tx, NULL);
		out_fop->ficr_rc = result;
		fop = fom_obj->sif_rep_fop;
		item = m0_fop_to_rpc_item(fop);
		item->ri_type = &fop->f_type->ft_rpc_item_type;
		fom->fo_rep_fop = fom_obj->sif_rep_fop;
		m0_fom_phase_moveif(fom, result, M0_FOPH_SUCCESS,
				    M0_FOPH_FAILURE);
		result = M0_FSO_AGAIN;
	}

	if (m0_fom_phase(fom) == M0_FOPH_FINISH && m0_fom_rc(fom) == 0)
		m0_stob_put(fom_obj->sif_stobj);

	return result;
}

/**
 * A simple non blocking read fop specific fom
 * state method implemention.
 */
static int stob_read_fom_tick(struct m0_fom *fom)
{
	struct m0_stob_io_read      *in_fop;
	struct m0_stob_io_read_rep  *out_fop;
	struct m0_stob_io_fom       *fom_obj;
	struct m0_stob_io           *stio;
	struct m0_stob              *stobj;
	struct m0_rpc_item          *item;
	struct m0_fop               *fop;
	void                        *addr;
	m0_bcount_t                  count;
	m0_bcount_t                  offset;
	uint32_t                     bshift;
	int                          result = 0;

	M0_PRE(m0_fop_opcode(fom->fo_fop) == M0_STOB_IO_READ_REQ_OPCODE);

	fom_obj = container_of(fom, struct m0_stob_io_fom, sif_fom);
	stio = &fom_obj->sif_stio;
	if (m0_fom_phase(fom) < M0_FOPH_NR) {
		result = m0_fom_tick_generic(fom);
	} else {
		out_fop = m0_fop_data(fom_obj->sif_rep_fop);
		M0_ASSERT(out_fop != NULL);

		if (m0_fom_phase(fom) == M0_FOPH_READ_STOB_IO) {
			uint8_t                  *buf;

			in_fop = m0_fop_data(fom->fo_fop);
			M0_ASSERT(in_fop != NULL);
			fom_obj->sif_stobj = stob_object_find(
				&in_fop->fir_object, fom);

			stobj =  fom_obj->sif_stobj;
			bshift = m0_stob_block_shift(stobj);

			M0_ALLOC_ARR(buf, 1 << BALLOC_DEF_BLOCK_SHIFT);
			M0_ASSERT(buf != NULL);
			out_fop->firr_value.fi_buf   = buf;
			out_fop->firr_value.fi_count =
						    1 << BALLOC_DEF_BLOCK_SHIFT;

			addr = m0_stob_addr_pack(buf, bshift);
			count = out_fop->firr_value.fi_count >> bshift;
			offset = 0;

			m0_stob_io_init(stio);

			stio->si_user = (struct m0_bufvec)
				M0_BUFVEC_INIT_BUF(&addr, &count);

			stio->si_stob.iv_vec.v_nr    = 1;
			stio->si_stob.iv_vec.v_count = &count;
			stio->si_stob.iv_index       = &offset;

			stio->si_opcode = SIO_READ;
			stio->si_flags  = 0;

			m0_mutex_lock(&stio->si_mutex);
			m0_fom_wait_on(fom, &stio->si_wait, &fom->fo_cb);
			m0_mutex_unlock(&stio->si_mutex);
			result = m0_stob_io_prepare_and_launch(stio, stobj,
							       &fom->fo_tx,
							       NULL);

			if (result != 0) {
				m0_mutex_lock(&stio->si_mutex);
				m0_fom_callback_cancel(&fom->fo_cb);
				m0_mutex_unlock(&stio->si_mutex);
				m0_fom_phase_move(fom, result, M0_FOPH_FAILURE);
			} else {
				m0_fom_phase_set(fom,
						 M0_FOPH_READ_STOB_IO_WAIT);
				result = M0_FSO_WAIT;
			}
		} else if (m0_fom_phase(fom) == M0_FOPH_READ_STOB_IO_WAIT) {
			stobj = fom_obj->sif_stobj;
			bshift = m0_stob_block_shift(stobj);
			out_fop->firr_count = stio->si_count << bshift;
			m0_fom_phase_moveif(fom, stio->si_rc, M0_FOPH_SUCCESS,
					    M0_FOPH_FAILURE);
		}

		if (m0_fom_phase(fom) == M0_FOPH_FAILURE ||
		    m0_fom_phase(fom) == M0_FOPH_SUCCESS) {
			out_fop->firr_rc = m0_fom_rc(fom);
			fop = fom_obj->sif_rep_fop;
			item = m0_fop_to_rpc_item(fop);
			item->ri_type = &fop->f_type->ft_rpc_item_type;
			fom->fo_rep_fop = fom_obj->sif_rep_fop;
			result = M0_FSO_AGAIN;
		}

	}

	if (m0_fom_phase(fom) == M0_FOPH_FINISH) {
		/*
		   If we fail in any of the generic phase, stob io
		   is uninitialised, so no need to fini.
		 */
		if (stio->si_state != SIS_ZERO) {
			m0_stob_io_fini(stio);
			m0_stob_put(fom_obj->sif_stobj);
		}
	}

	return result;
}

static void fom_stob_write_credit(struct m0_fom *fom)
{
	struct m0_stob_io_write *in_fop;
	struct m0_stob          *stobj;
	m0_bcount_t              count;
	m0_bindex_t              index;
	struct m0_stob_io        io;

	in_fop = m0_fop_data(fom->fo_fop);
	stobj = stob_object_find(&in_fop->fiw_object, fom);
	index = 0;
	count = in_fop->fiw_value.fi_count >> m0_stob_block_shift(stobj);
	io = (struct m0_stob_io) { .si_opcode = SIO_WRITE };
	io.si_stob = (struct m0_indexvec) {
		.iv_vec   = (struct m0_vec) {.v_nr = 1, .v_count = &count},
		.iv_index = &index};
	m0_stob_io_credit(&io, stobj->so_domain, m0_fom_tx_credit(fom));
	m0_stob_put(stobj);
}

/**
 * A simple non blocking write fop specific fom
 * state method implemention.
 */
static int stob_write_fom_tick(struct m0_fom *fom)
{
	struct m0_stob_io_write     *in_fop;
	struct m0_stob_io_write_rep *out_fop;
	struct m0_stob_io_fom       *fom_obj;
	struct m0_stob_io           *stio;
	struct m0_stob              *stobj;
	struct m0_rpc_item          *item;
	struct m0_fop               *fop;
	void                        *addr;
	m0_bcount_t                  count;
	m0_bindex_t                  offset;
	uint32_t                     bshift;
	int                          result = 0;

	M0_PRE(m0_fop_opcode(fom->fo_fop) == M0_STOB_IO_WRITE_REQ_OPCODE);

	fom_obj = container_of(fom, struct m0_stob_io_fom, sif_fom);
	stio = &fom_obj->sif_stio;

	if (m0_fom_phase(fom) < M0_FOPH_NR) {
		if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN)
			fom_stob_write_credit(fom);
		result = m0_fom_tick_generic(fom);
	} else {
		out_fop = m0_fop_data(fom_obj->sif_rep_fop);
		M0_ASSERT(out_fop != NULL);

		if (m0_fom_phase(fom) == M0_FOPH_WRITE_STOB_IO) {
			in_fop = m0_fop_data(fom->fo_fop);
			M0_ASSERT(in_fop != NULL);

			fom_obj->sif_stobj = stob_object_find(
				&in_fop->fiw_object, fom);

			stobj = fom_obj->sif_stobj;
			bshift = m0_stob_block_shift(stobj);

			addr = m0_stob_addr_pack(in_fop->fiw_value.fi_buf,
						 bshift);
			count = in_fop->fiw_value.fi_count >> bshift;
			offset = 0;

			m0_stob_io_init(stio);

			stio->si_user = (struct m0_bufvec)
				M0_BUFVEC_INIT_BUF(&addr, &count);

			stio->si_stob.iv_vec.v_nr    = 1;
			stio->si_stob.iv_vec.v_count = &count;
			stio->si_stob.iv_index       = &offset;
			stio->si_opcode = SIO_WRITE;
			stio->si_fol_frag = &fom_obj->sif_fol_frag;
			stio->si_flags  = 0;
			result = m0_stob_io_private_setup(stio, stobj);
			M0_UT_ASSERT(result == 0);
			m0_stob_ad_balloc_set(stio, M0_BALLOC_NORMAL_ZONE);
			m0_mutex_lock(&stio->si_mutex);
			m0_fom_wait_on(fom, &stio->si_wait, &fom->fo_cb);
			m0_mutex_unlock(&stio->si_mutex);
			result = m0_stob_io_prepare_and_launch(stio, stobj,
							       &fom->fo_tx,
							       NULL);

			if (result != 0) {
				m0_mutex_lock(&stio->si_mutex);
				m0_fom_callback_cancel(&fom->fo_cb);
				m0_mutex_unlock(&stio->si_mutex);
				m0_fom_phase_move(fom, result, M0_FOPH_FAILURE);
			} else {
				m0_fom_phase_set(fom,
						 M0_FOPH_WRITE_STOB_IO_WAIT);
				result = M0_FSO_WAIT;
			}
		} else if (m0_fom_phase(fom) == M0_FOPH_WRITE_STOB_IO_WAIT) {
			stobj = fom_obj->sif_stobj;
			bshift = m0_stob_block_shift(stobj);
			out_fop->fiwr_count = stio->si_count << bshift;
			m0_fom_phase_moveif(fom, stio->si_rc, M0_FOPH_SUCCESS,
					    M0_FOPH_FAILURE);
		}

		if (m0_fom_phase(fom) == M0_FOPH_FAILURE ||
		    m0_fom_phase(fom) == M0_FOPH_SUCCESS) {
			out_fop->fiwr_rc = m0_fom_rc(fom);
			fop = fom_obj->sif_rep_fop;
			item = m0_fop_to_rpc_item(fop);
			item->ri_type = &fop->f_type->ft_rpc_item_type;
			fom->fo_rep_fop = fom_obj->sif_rep_fop;
			result = M0_FSO_AGAIN;
		}
	}

	if (m0_fom_phase(fom) == M0_FOPH_FINISH) {
		/*
		   If we fail in any of the generic phase, stob io
		   is uninitialised, so no need to fini.
		 */
		if (stio->si_state != SIS_ZERO) {
			m0_stob_io_fini(stio);
			m0_stob_put(fom_obj->sif_stobj);
		}
	}
	return result;
}

/**
 * Fom specific clean up function, invokes m0_fom_fini()
 */
static void stob_io_fom_fini(struct m0_fom *fom)
{
	struct m0_stob_io_fom *fom_obj;

	fom_obj = container_of(fom, struct m0_stob_io_fom, sif_fom);
	m0_fom_fini(fom);
	m0_free(fom_obj);
}

void m0_stob_io_fop_fini(void);

/**
 * Function to intialise stob io fops.
 */
void m0_stob_io_fop_init(void)
{
	m0_sm_conf_extend(m0_generic_conf.scf_state, stob_read_phases,
			  m0_generic_conf.scf_nr_states);
	m0_sm_conf_extend(m0_generic_conf.scf_state, stob_write_phases,
			  m0_generic_conf.scf_nr_states);
	m0_xc_reqh_ut_io_fop_init();

	m0_stob_io_create_xc->xct_flags     = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_stob_io_read_xc->xct_flags       = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_stob_io_write_xc->xct_flags      = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_stob_io_create_rep_xc->xct_flags = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_stob_io_read_rep_xc->xct_flags   = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_stob_io_write_rep_xc->xct_flags  = M0_XCODE_TYPE_FLAG_DOM_RPC;
	stob_io_fop_fid_xc->xct_flags       = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_fi_value_xc->xct_flags           = M0_XCODE_TYPE_FLAG_DOM_RPC;

	M0_FOP_TYPE_INIT(&m0_stob_io_create_fopt,
			 .name      = "Stob create",
			 .opcode    = M0_STOB_IO_CREATE_REQ_OPCODE,
			 .xt        = m0_stob_io_create_xc,
			 .fom_ops   = &stob_create_fom_type_ops,
			 .sm        = &m0_generic_conf,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .svc_type  = &m0_rpc_service_type);
	M0_FOP_TYPE_INIT(&m0_stob_io_read_fopt,
			 .name      = "Stob read",
			 .opcode    = M0_STOB_IO_READ_REQ_OPCODE,
			 .xt        = m0_stob_io_read_xc,
			 .fom_ops   = &stob_read_fom_type_ops,
			 .sm        = &read_conf,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .svc_type  = &m0_rpc_service_type);
	M0_FOP_TYPE_INIT(&m0_stob_io_write_fopt,
			 .name      = "Stob write",
			 .opcode    = M0_STOB_IO_WRITE_REQ_OPCODE,
			 .xt        = m0_stob_io_write_xc,
			 .fom_ops   = &stob_write_fom_type_ops,
			 .sm        = &write_conf,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .svc_type  = &m0_rpc_service_type);
	M0_FOP_TYPE_INIT(&m0_stob_io_create_rep_fopt,
			 .name      = "Stob create reply",
			 .opcode    = M0_STOB_IO_CREATE_REPLY_OPCODE,
			 .xt        = m0_stob_io_create_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .svc_type  = &m0_rpc_service_type);
	M0_FOP_TYPE_INIT(&m0_stob_io_read_rep_fopt,
			 .name      = "Stob read reply",
			 .opcode    = M0_STOB_IO_READ_REPLY_OPCODE,
			 .xt        = m0_stob_io_read_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .svc_type  = &m0_rpc_service_type);
	M0_FOP_TYPE_INIT(&m0_stob_io_write_rep_fopt,
			 .name      = "Stob write reply",
			 .opcode    = M0_STOB_IO_WRITE_REPLY_OPCODE,
			 .xt        = m0_stob_io_write_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .svc_type  = &m0_rpc_service_type);
}

/**
 * Function to clean stob io fops
 */
void m0_stob_io_fop_fini(void)
{
	m0_fop_type_fini(&m0_stob_io_write_rep_fopt);
	m0_fop_type_fini(&m0_stob_io_read_rep_fopt);
	m0_fop_type_fini(&m0_stob_io_create_rep_fopt);
	m0_fop_type_fini(&m0_stob_io_write_fopt);
	m0_fop_type_fini(&m0_stob_io_read_fopt);
	m0_fop_type_fini(&m0_stob_io_create_fopt);
	m0_xc_reqh_ut_io_fop_fini();
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
