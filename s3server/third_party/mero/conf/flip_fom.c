/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Mikhail Antropov <mikhail.v.antropov@xyratex.com>
 *
 * Original creation date: 16-Mar-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SPIEL
#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/string.h"      /* strlen, m0_strdup */
#include "lib/tlist.h"
#include "lib/assert.h"
#include "lib/finject.h"    /* M0_FI_ENABLED */
#include "lib/fs.h"         /* m0_file_read */
#include "conf/confd.h"
#include "conf/confd_stob.h"
#include "conf/flip_fop.h"
#include "conf/flip_fom.h"
#include "conf/obj_ops.h"    /* m0_conf_obj_find */
#include "fop/fop.h"
#include "mero/magic.h"
#ifndef __KERNEL__
  #include "mero/setup.h"
  #include <sys/stat.h>
  #include <unistd.h>
  #include <fcntl.h>
#endif

/**
 * @addtogroup conf_foms
 * @{
 */

M0_TL_DECLARE(m0_conf_cache, M0_INTERNAL, struct m0_conf_obj);

static int conf_flip_fom_tick(struct m0_fom *fom);
static void conf_flip_fom_fini(struct m0_fom *fom);

static int conf_flip_prepare(struct m0_fom *);
static int conf_flip_apply(struct m0_fom *);

static size_t conf_flip_fom_home_locality(const struct m0_fom *fom);

/**
 * Spiel Load FOM operation vector.
 */
const struct m0_fom_ops conf_flip_fom_ops = {
	.fo_fini          = conf_flip_fom_fini,
	.fo_tick          = conf_flip_fom_tick,
	.fo_home_locality = conf_flip_fom_home_locality,
};

/**
 * Spiel Load FOM type operation vector.
 */
const struct m0_fom_type_ops conf_flip_fom_type_ops = {
	.fto_create = m0_conf_flip_fom_create,
};

/**
 * Spiel FOM state transition table.
 */
struct m0_sm_state_descr conf_flip_phases[] = {
	[M0_FOPH_CONF_FLIP_PREPARE] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "Conf_flip_Prepare",
		.sd_allowed   = M0_BITS(M0_FOPH_CONF_APPLY,
					M0_FOPH_FAILURE)
	},
	[M0_FOPH_CONF_APPLY] = {
		.sd_name      = "Conf_Flip_Apply",
		.sd_allowed   = M0_BITS(M0_FOPH_SUCCESS,
					M0_FOPH_FAILURE)
	},
};


struct m0_sm_conf conf_flip_conf = {
	.scf_name      = "Conf_Flip_phases",
	.scf_nr_states = ARRAY_SIZE(conf_flip_phases),
	.scf_state     = conf_flip_phases,
};

/**
 * Compare equals Spiel FOP - Spiel FOM
 */
static bool conf_flip_fom_invariant(const struct m0_conf_flip_fom *fom)
{
	return _0C(fom != NULL);
}

/**
 * Create and initiate Spiel FOM and return generic struct m0_fom
 * Find the corresponding fom_type and associate it with m0_fom.
 * Associate fop with fom type.
 *
 * @param fop file operation packet need to process
 * @param out file operation machine need to allocate and initiate
 *
 * @pre fop != NULL
 * @pre fop is m0_fop_conf_flip
 * @pre out != NULL
 * @pre reqh != NULL
 */
M0_INTERNAL int m0_conf_flip_fom_create(struct m0_fop   *fop,
					struct m0_fom  **out,
					struct m0_reqh  *reqh)
{
	struct m0_fom           *fom;
	struct m0_conf_flip_fom *conf_flip_fom;
	struct m0_fop           *rep_fop;

	M0_PRE(fop != NULL);
	M0_PRE(m0_is_conf_flip_fop(fop));
	M0_PRE(out != NULL);
	M0_PRE(reqh != NULL);

	M0_ENTRY("fop=%p", fop);

	M0_ALLOC_PTR(conf_flip_fom);
	rep_fop = m0_fop_reply_alloc(fop, &m0_fop_conf_flip_rep_fopt);
	if (conf_flip_fom == NULL || rep_fop == NULL) {
		m0_free(conf_flip_fom);
		m0_free(rep_fop);
		return M0_RC(-ENOMEM);
	}

	fom = &conf_flip_fom->clm_gen;
	*out = fom;
	m0_fom_init(fom, &fop->f_type->ft_fom_type, &conf_flip_fom_ops,
		    fop, rep_fop, reqh);

	M0_LOG(M0_DEBUG, "fom=%p", fom);
	return M0_RC(0);
}

static int conf_flip_prepare(struct m0_fom *fom)
{
	M0_ENTRY("fom=%p", fom);
	m0_fom_phase_set(fom, M0_FOPH_CONF_APPLY);
	return M0_RC(M0_FSO_AGAIN);
}

/* Save confd configuration STOB */
static int conf_flip_confd_config_save(char *filename,
				       char *buffer)
{
	int     rc = 0;
#ifndef __KERNEL__
	int    fd;
	size_t length;

	M0_ENTRY();

	if(M0_FI_ENABLED("fcreate_failed"))
		return M0_ERR(-EACCES);
	fd = open(filename, O_TRUNC | O_CREAT | O_WRONLY, 0700);

	if (fd == -1)
		return M0_ERR(-errno);

	length = strlen(buffer);
	rc = write(fd, buffer, length) == length ? 0 : M0_ERR(-EINVAL);
	fsync(fd);
	close(fd);
#endif	/* __KERNEL__ */
	return M0_RC(rc);
}

/**
 * After successful FLIP completion, rconfc appears retaining an outdated conf
 * cache. To the moment, local conf file is already updated by FLIP action. So
 * it is ultimately possible to do a simple conf reload with the newly saved
 * conf to update the conf cache hosted by rconfc internals.
 */
static int conf_after_flip_apply(struct m0_reqh *reqh, const char *filename)
{
	struct m0_rconfc      *rconfc   = &reqh->rh_rconfc;
	struct m0_fid          profile  = rconfc->rc_profile;
	struct m0_sm_group    *sm_grp   = rconfc->rc_sm.sm_grp;
	struct m0_rpc_machine *rmach    = rconfc->rc_rmach;
	m0_rconfc_cb_t         exp_cb   = rconfc->rc_expired_cb;
	m0_rconfc_cb_t         ready_cb = rconfc->rc_ready_cb;
	char                  *local_conf;
	int                    rc;

	M0_ENTRY();
	M0_PRE(m0_rconfc_is_preloaded(rconfc));
	rc = m0_file_read(filename, &local_conf);
	if (rc != 0)
		return M0_ERR(rc);
	/*
	 * Although m0_rconfc_stop_sync() launches an ast internally, there is
	 * nothing to wait for in the pre-loaded rconfc. So hopefully this is to
	 * do no harm to fom tick, but heavily simplify the reload.
	 */
	m0_rconfc_stop_sync(rconfc);
	m0_rconfc_fini(rconfc);
	rc = m0_rconfc_init(rconfc, &profile, sm_grp, rmach, exp_cb, ready_cb);
	if (rc != 0) {
		m0_free(local_conf);
		return M0_ERR(rc);
	}
	rconfc->rc_local_conf = local_conf;
	/*
	 * Despite the name, m0_rconfc_start_sync() does no waiting at all with
	 * local conf pre-loading.
	 */
	return M0_RC(m0_rconfc_start_sync(rconfc));
}

/**
 * Apply Flip command
 * Change Confd configuration
 * 1. Used FOP data read confd configure from IO STOB
 * 2. Try apply data to confd
 * 3. If OK then replace confd configure file and change current version
 *          else restore previous configuration
 *
 * @param fom file operation machine.
 *
 * @pre fom != NULL
 * @pre fop is Flip fop
 * @pre m0_fom_phase(fom) == M0_FOPH_CONF_APPLY
 */
static int conf_flip_apply(struct m0_fom *fom)
{
	int                      rc;
	struct m0_conf_flip_fom *conf_fom;
	struct m0_fop_conf_flip *conf_fop;
	struct m0_stob          *stob = NULL;
	char                    *location;
	char                    *conf_filename = NULL;
	struct m0_confd         *confd;
	char                    *confd_buffer = NULL;
	struct m0_conf_cache    *new_cache;

	M0_ENTRY("fom=%p", fom);

	M0_PRE(fom != NULL);
	M0_PRE(m0_is_conf_flip_fop(fom->fo_fop));
	M0_PRE(m0_fom_phase(fom) == M0_FOPH_CONF_APPLY);

	conf_fom = container_of(fom, struct m0_conf_flip_fom, clm_gen);
	M0_ASSERT(conf_flip_fom_invariant(conf_fom));

	conf_fop = m0_conf_fop_to_flip_fop(fom->fo_fop);

	/* Reads data of previous "Load command" aka stob */
	rc = m0_conf_stob_location_generate(fom, &location) ?:
	     m0_confd_stob_init(&stob, location,
				&M0_CONFD_FID(conf_fop->cff_prev_version,
					      conf_fop->cff_next_version,
					      conf_fop->cff_tx_id)) ?:
	     m0_confd_stob_read(stob, &confd_buffer);
	if (rc != 0)
		goto done;

	confd = bob_of(fom->fo_service, struct m0_confd, d_reqh, &m0_confd_bob);

	/*
	 * Create new cache and use it in confd instead of a current cache.
	 * Save new cache in a file used by confd on startup, so confd will use
	 * new configuration db after restart.
	 * If something fails, then don't touch current cache at all.
	 */
	rc = m0_confd_service_to_filename(fom->fo_service, &conf_filename) ?:
	     m0_confd_cache_create(&new_cache, &confd->d_cache_lock,
				   confd_buffer) ?:
		conf_flip_confd_config_save(conf_filename, confd_buffer) ?:
		conf_after_flip_apply(m0_fom2reqh(fom), conf_filename);
	if (rc == 0) {
		m0_confd_cache_destroy(confd->d_cache);
		confd->d_cache = new_cache;
	} else if (new_cache != NULL)
		m0_confd_cache_destroy(new_cache);
done:
	if (confd_buffer != NULL)
		m0_free_aligned(confd_buffer, strlen(confd_buffer) + 1,
			        m0_stob_block_shift(stob));
	m0_confd_stob_fini(stob);
	m0_free(location);
	m0_free(conf_filename);
	m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
	return M0_RC(M0_FSO_AGAIN);
}

/**
 * Phase transition for the Spiel FLIP operation.
 *
 * @param fom instance file operation machine under execution
 *
 * @pre fom != NULL
 * @pre fop from fom is Flip fop
 */
static int conf_flip_fom_tick(struct m0_fom *fom)
{
	int                          rc = 0;
	struct m0_conf_flip_fom     *conf_fom;
	struct m0_fop_conf_flip_rep *rep;

	M0_ENTRY("fom %p", fom);

	M0_PRE(fom != NULL);
	M0_PRE(m0_is_conf_flip_fop(fom->fo_fop));

	if (m0_fom_phase(fom) < M0_FOPH_NR)
		return m0_fom_tick_generic(fom);

	conf_fom = container_of(fom, struct m0_conf_flip_fom, clm_gen);
	M0_ASSERT(conf_flip_fom_invariant(conf_fom));

	switch (m0_fom_phase(fom)) {
	case M0_FOPH_CONF_FLIP_PREPARE:
		rc = conf_flip_prepare(fom);
		break;
	case M0_FOPH_CONF_APPLY:
		rc = conf_flip_apply(fom);
		break;
	default:
		M0_ASSERT(0);
		break;
	}

	if (M0_IN(m0_fom_phase(fom), (M0_FOPH_SUCCESS, M0_FOPH_FAILURE))) {
		rep = m0_conf_fop_to_flip_fop_rep(fom->fo_rep_fop);
		rep->cffr_rc = m0_fom_rc(fom);
	}

	return M0_RC(rc);
}

/**
 * Finalise of Spiel file operation machine.
 * This is the right place to free all resources acquired by FOM
 *
 * @param fom instance file operation machine under execution
 *
 * @pre fom != NULL
 */
static void conf_flip_fom_fini(struct m0_fom *fom)
{
	struct m0_conf_flip_fom *conf_fom;

	M0_PRE(fom != NULL);

	conf_fom = container_of(fom, struct m0_conf_flip_fom, clm_gen);
	M0_ASSERT(conf_flip_fom_invariant(conf_fom));

	m0_fom_fini(fom);
	m0_free(conf_fom);
}

static size_t conf_flip_fom_home_locality(const struct m0_fom *fom)
{
	M0_ENTRY();
	M0_PRE(fom != NULL);
	M0_LEAVE();
	return 1;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of io_foms */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
