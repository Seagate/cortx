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
 * Original creation date: 22-Jan-2013
 */


#pragma once

#ifndef __MERO_DTM_NUCLEUS_H__
#define __MERO_DTM_NUCLEUS_H__


/**
 * @addtogroup dtm
 *
 * @{
 */

/* import */
#include "lib/mutex.h"
#include "lib/tlist.h"
#include "lib/types.h"

/* export */
struct m0_dtm_nu;
struct m0_dtm_hi;
struct m0_dtm_op;
struct m0_dtm_up;

struct m0_dtm_hi_ops;
struct m0_dtm_op_ops;

typedef uint64_t m0_dtm_ver_t;

enum m0_dtm_state {
	M0_DOS_LIMBO,
	M0_DOS_FUTURE,
	M0_DOS_PREPARE,
	M0_DOS_INPROGRESS,
	M0_DOS_VOLATILE,
	M0_DOS_PERSISTENT,
	M0_DOS_STABLE,
	M0_DOS_NR
};

struct m0_dtm_hi {
	struct m0_dtm_nu           *hi_nu;
	uint64_t                    hi_flags;
	m0_dtm_ver_t                hi_ver;
	const struct m0_dtm_hi_ops *hi_ops;
	struct m0_tl                hi_ups;
};
M0_INTERNAL bool m0_dtm_hi_invariant(const struct m0_dtm_hi *hi);

enum m0_dtm_hi_flags {
	M0_DHF_SHARED = 1ULL << 0,
	M0_DHF_FULL   = 1ULL << 1,
	M0_DHF_OWNED  = 1ULL << 2,
	M0_DHF_BUSY   = 1ULL << 3,
	M0_DHF_LAST   = 1ULL << 4
};

struct m0_dtm_hi_ops {
	void (*dho_release)   (struct m0_dtm_hi *hi);
};

enum m0_dtm_up_rule {
	M0_DUR_INC,
	M0_DUR_SET,
	M0_DUR_NOT,
	M0_DUR_APP,
	M0_DUR_NR
};

struct m0_dtm_up {
	uint64_t             up_magix;
	enum m0_dtm_state    up_state;
	enum m0_dtm_up_rule  up_rule;
	m0_dtm_ver_t         up_ver;
	m0_dtm_ver_t         up_orig_ver;
	struct m0_dtm_hi    *up_hi;
	struct m0_tlink      up_hi_linkage;
	struct m0_dtm_op    *up_op;
	struct m0_tlink      up_op_linkage;
};
M0_INTERNAL bool m0_dtm_up_invariant(const struct m0_dtm_up *up);

struct m0_dtm_op {
	struct m0_dtm_nu           *op_nu;
	struct m0_tl                op_ups;
	const struct m0_dtm_op_ops *op_ops;
};
M0_INTERNAL bool m0_dtm_op_invariant(const struct m0_dtm_op *op);

struct m0_dtm_op_ops {
	void (*doo_ready)(struct m0_dtm_op *op);
	void (*doo_late) (struct m0_dtm_op *op);
	void (*doo_miser)(struct m0_dtm_op *op);
};

struct m0_dtm_nu {
	struct m0_mutex nu_lock;
};

M0_INTERNAL void m0_dtm_op_init    (struct m0_dtm_op *op,
				    struct m0_dtm_nu *nu);
M0_INTERNAL void m0_dtm_op_prepared(const struct m0_dtm_op *op);
M0_INTERNAL void m0_dtm_op_done    (const struct m0_dtm_op *op);
M0_INTERNAL void m0_dtm_op_close   (const struct m0_dtm_op *op);
M0_INTERNAL void m0_dtm_op_del     (struct m0_dtm_op *op);
M0_INTERNAL void m0_dtm_op_fini    (struct m0_dtm_op *op);

M0_INTERNAL void m0_dtm_hi_init    (struct m0_dtm_hi *hi,
				    struct m0_dtm_nu *nu);
M0_INTERNAL void m0_dtm_hi_fini    (struct m0_dtm_hi *hi);

M0_INTERNAL void m0_dtm_up_init    (struct m0_dtm_up *up,
				    struct m0_dtm_hi *hi,
				    struct m0_dtm_op *op,
				    enum m0_dtm_up_rule rule,
				    m0_dtm_ver_t ver, m0_dtm_ver_t orig_ver);
M0_INTERNAL void m0_dtm_up_ver_set (struct m0_dtm_up *up,
				    m0_dtm_ver_t ver, m0_dtm_ver_t orig_ver);
M0_INTERNAL void m0_dtm_nu_init    (struct m0_dtm_nu *nu);
M0_INTERNAL void m0_dtm_nu_fini    (struct m0_dtm_nu *nu);

M0_INTERNAL struct m0_dtm_up *m0_dtm_up_prior(struct m0_dtm_up *up);
M0_INTERNAL struct m0_dtm_up *m0_dtm_up_later(struct m0_dtm_up *up);

M0_INTERNAL void m0_dtm_nuclei_init(void);
M0_INTERNAL void m0_dtm_nuclei_fini(void);

/** @} end of dtm group */

#endif /* __MERO_DTM_NUCLEUS_H__ */


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
