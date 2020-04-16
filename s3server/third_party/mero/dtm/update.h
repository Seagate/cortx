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
 * Original creation date: 01-Feb-2013
 */


#pragma once

#ifndef __MERO_DTM_UPDATE_H__
#define __MERO_DTM_UPDATE_H__


/**
 * @addtogroup dtm
 *
 * @{
 */

#include "dtm/nucleus.h"

/* export */
struct m0_dtm_update;
struct m0_dtm_update_ops;
struct m0_dtm_update_type;
struct m0_dtm_update_descr;

/* import */
struct m0_dtm_history_type;
struct m0_dtm_oper;
struct m0_dtm_history;
struct m0_fop;
#include "lib/cookie.h"
#include "lib/types_xc.h"             /* m0_uint128_xc */
#include "lib/cookie_xc.h"
#include "xcode/xcode.h"              /* struct m0_xcode_obj */

enum m0_dtm_update_comm_state {
	M0_DUX_NEW,
	M0_DUX_INFLIGHT,
	M0_DUX_REPLIED
};

enum m0_dtm_update_comm_flags {
	M0_DUCF_REPLIED_CALLED = (1 << 0)
};

struct m0_dtm_update_comm {
	enum m0_dtm_update_comm_state  uc_state;
	uint64_t                       uc_flags;
	uint64_t                       uc_instance;
	struct m0_fop                 *uc_body;
};

struct m0_dtm_update {
	struct m0_dtm_up                upd_up;
	uint32_t                        upd_label;
	const struct m0_dtm_update_ops *upd_ops;
	struct m0_dtm_update_comm       upd_comm;
};
M0_INTERNAL bool m0_dtm_update_invariant(const struct m0_dtm_update *update);

enum {
	M0_DTM_USER_UPDATE_BASE = 0x100000
};

struct m0_dtm_update_ops {
	int  (*updo_redo)    (struct m0_dtm_update *updt);
	int  (*updo_undo)    (struct m0_dtm_update *updt);
	void (*updto_replied)(struct m0_dtm_update *updt);
	const struct m0_dtm_update_type *updo_type;
};

struct m0_dtm_update_type {
	uint8_t     updtt_id;
	const char *updtt_name;
};

struct m0_dtm_update_data {
	uint64_t da_ver;
	uint64_t da_orig_ver;
	uint32_t da_label;
	uint8_t  da_rule;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#define M0_DTM_UPDATE_DATA(label, rule, ver, orig_ver)	\
(struct m0_dtm_update_data) {				\
	.da_label    = (label),				\
	.da_rule     = (rule),				\
	.da_ver      = (ver),				\
	.da_orig_ver = (orig_ver)			\
}

struct m0_dtm_history_id {
	struct m0_uint128 hid_id;
	struct m0_cookie  hid_sender;
	struct m0_cookie  hid_receiver;
	uint8_t           hid_htype;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_dtm_update_descr {
	struct m0_dtm_update_data udd_data;
	struct m0_dtm_history_id  udd_id;
	uint8_t                   udd_utype;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_INTERNAL void m0_dtm_update_init(struct m0_dtm_update *update,
				    struct m0_dtm_history *history,
				    struct m0_dtm_oper *oper,
				    const struct m0_dtm_update_data *data);
M0_INTERNAL bool m0_dtm_update_is_user(const struct m0_dtm_update *update);
M0_INTERNAL void m0_dtm_update_pack(const struct m0_dtm_update *update,
				    struct m0_dtm_update_descr *updd);
M0_INTERNAL void m0_dtm_update_unpack(struct m0_dtm_update *update,
				      const struct m0_dtm_update_descr *updd);
M0_INTERNAL int m0_dtm_update_build(struct m0_dtm_update *update,
				    struct m0_dtm_oper *oper,
				    const struct m0_dtm_update_descr *updd);
M0_INTERNAL void m0_dtm_update_reint(struct m0_dtm_update *update);
M0_INTERNAL void m0_dtm_comm_set    (struct m0_dtm_update *update,
				     struct m0_fop *fop);
M0_INTERNAL bool
m0_dtm_update_matches_descr(const struct m0_dtm_update *update,
			    const struct m0_dtm_update_descr *updd);
M0_INTERNAL bool
m0_dtm_descr_matches_update(const struct m0_dtm_update *update,
			    const struct m0_dtm_update_descr *updd);
M0_INTERNAL void m0_dtm_update_list_init(struct m0_tl *list);
M0_INTERNAL void m0_dtm_update_list_fini(struct m0_tl *list);
M0_INTERNAL void m0_dtm_update_link(struct m0_tl *list,
				    struct m0_dtm_update *update, uint32_t nr);

/** @} end of dtm group */

#endif /* __MERO_DTM_UPDATE_H__ */


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
