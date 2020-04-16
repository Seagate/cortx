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
 * Original creation date: 19-Mar-2013
 */


#pragma once

#ifndef __MERO_DTM_DTM_INTERNAL_H__
#define __MERO_DTM_DTM_INTERNAL_H__

/**
 * @addtogroup dtm
 *
 * @{
 */

/* import */
#include "lib/tlist.h"
#include "dtm/nucleus.h"
struct m0_dtm_oper;
struct m0_dtm_update;
struct m0_dtm_history;
struct m0_dtm_remote;

M0_TL_DESCR_DECLARE(hi, M0_EXTERN);
M0_TL_DECLARE(hi, M0_EXTERN, struct m0_dtm_up);
M0_TL_DESCR_DECLARE(op, M0_EXTERN);
M0_TL_DECLARE(op, M0_EXTERN, struct m0_dtm_up);

#define up_for(o, up)				\
do {						\
	struct m0_dtm_up *up;			\
						\
	m0_tl_for(op, &(o)->op_ups, up)

#define up_endfor				\
	m0_tl_endfor;				\
} while (0)

#define hi_for(h, up)				\
do {						\
	struct m0_dtm_up *up;			\
						\
	m0_tl_for(hi, &(h)->hi_ups, up)

#define hi_endfor				\
	m0_tl_endfor;				\
} while (0)

M0_TL_DESCR_DECLARE(history, M0_EXTERN);
M0_TL_DECLARE(history, M0_EXTERN, struct m0_dtm_update);
M0_TL_DESCR_DECLARE(oper, M0_EXTERN);
M0_TL_DECLARE(oper, M0_EXTERN, struct m0_dtm_update);
M0_TL_DESCR_DECLARE(cat, M0_EXTERN);
M0_TL_DECLARE(cat, M0_EXTERN, struct m0_dtm_history);
M0_TL_DESCR_DECLARE(exc, M0_EXTERN);
M0_TL_DECLARE(exc, M0_EXTERN, struct m0_dtm_history);

#define oper_for(o, update)				\
do {							\
	struct m0_dtm_update *update;			\
							\
	m0_tl_for(oper, &(o)->oprt_op.op_ups, update)

#define oper_endfor				\
	m0_tl_endfor;				\
} while (0)

#define history_for(h, update)				\
do {							\
	struct m0_dtm_update *update;			\
							\
	m0_tl_for(history, &(h)->h_hi.hi_ups, update)

#define history_endfor				\
	m0_tl_endfor;				\
} while (0)

#define UPDATE_UP(update)				\
({							\
	typeof(update) __update = (update);		\
	__update != NULL ? &__update->upd_up : NULL;	\
})

#define UP_HISTORY(up) hi_history((up)->up_hi)
#define UPDATE_HISTORY(update) UP_HISTORY(&(update)->upd_up)
#define UPDATE_REM(update) (UPDATE_HISTORY(update)->h_rem)
#define HISTORY_DTM(history) (nu_dtm((history)->h_hi.hi_nu))

M0_INTERNAL struct m0_dtm *nu_dtm(struct m0_dtm_nu *nu);
M0_INTERNAL struct m0_dtm_history *hi_history(struct m0_dtm_hi *hi);
M0_INTERNAL struct m0_dtm_update *up_update(struct m0_dtm_up *up);
M0_INTERNAL m0_dtm_ver_t up_ver(const struct m0_dtm_up *up);
M0_INTERNAL bool op_state(const struct m0_dtm_op *op, enum m0_dtm_state state);
M0_INTERNAL void advance_try(const struct m0_dtm_op *op);
M0_INTERNAL void up_prepared(struct m0_dtm_up *up);
M0_INTERNAL void history_close(struct m0_dtm_history *history);
M0_INTERNAL void nu_lock(struct m0_dtm_nu *nu);
M0_INTERNAL void nu_unlock(struct m0_dtm_nu *nu);
M0_INTERNAL void dtm_lock(struct m0_dtm *dtm);
M0_INTERNAL void dtm_unlock(struct m0_dtm *dtm);
M0_INTERNAL void oper_lock(const struct m0_dtm_oper *oper);
M0_INTERNAL void oper_unlock(const struct m0_dtm_oper *oper);
M0_INTERNAL void history_lock(const struct m0_dtm_history *history);
M0_INTERNAL void history_unlock(const struct m0_dtm_history *history);
M0_INTERNAL void update_reint(struct m0_dtm_update *update);

M0_INTERNAL struct m0_dtm_up *hi_latest(struct m0_dtm_hi *hi);
M0_INTERNAL struct m0_dtm_up *hi_earliest(struct m0_dtm_hi *hi);
M0_INTERNAL struct m0_dtm_up *hi_find(struct m0_dtm_hi *hi, m0_dtm_ver_t ver);
M0_INTERNAL struct m0_dtm_up *history_latest(struct m0_dtm_history *history);
M0_INTERNAL struct m0_dtm_up *history_earliest(struct m0_dtm_history *history);

M0_INTERNAL m0_dtm_ver_t update_ver(const struct m0_dtm_update *update);
M0_INTERNAL m0_dtm_ver_t up_ver(const struct m0_dtm_up *up);
M0_INTERNAL bool up_is_earlier(struct m0_dtm_up *up0, struct m0_dtm_up *up1);
M0_INTERNAL bool update_is_earlier(struct m0_dtm_update *update0,
				   struct m0_dtm_update *update1);

M0_INTERNAL int m0_dtm_remote_global_init(void);
M0_INTERNAL void m0_dtm_remote_global_fini(void);

M0_INTERNAL void oper_print(const struct m0_dtm_oper *oper);
M0_INTERNAL void update_print(const struct m0_dtm_update *update);
M0_INTERNAL void history_print(const struct m0_dtm_history *history);
M0_INTERNAL void history_print_header(const struct m0_dtm_history *history,
				      char *buf);
M0_INTERNAL void update_print_internal(const struct m0_dtm_update *update,
				       bool history);

M0_INTERNAL bool oper_update_unique(const struct m0_dtm_oper *oper,
				    const struct m0_dtm_update *update);

/** @} end of dtm group */

#endif /* __MERO_DTM_DTM_INTERNAL_H__ */


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
