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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 01-Jan-2012
 */

#pragma once

#ifndef __MERO_XCODE_FF2C_SEM_H__
#define __MERO_XCODE_FF2C_SEM_H__

/**
   @addtogroup xcode
 */
/** @{ */

#include <stdbool.h>

/* import */
struct ff2c_term;

/* export */
struct ff2c_require;
struct ff2c_type;
struct ff2c_field;
struct ff2c_escape;
struct ff2c_list;

struct ff2c_list {
	void *l_head;
	void *l_tail;
};

struct ff2c_require {
	struct ff2c_require *r_next;
	const char          *r_path;
};

struct ff2c_type {
	struct ff2c_type       *t_next;
	const struct ff2c_term *t_term;
	const char             *t_name;
	const char             *t_xc_name;
	const char             *t_c_name;
	bool                    t_compound;
	bool                    t_atomic;
	bool                    t_opaque;
	bool                    t_sequence;
	bool                    t_array;
	bool                    t_union;
	bool                    t_record;
	bool                    t_public;
	int                     t_nr;
	struct ff2c_list        t_field;
};

struct ff2c_field {
	struct ff2c_field *f_next;
	struct ff2c_type  *f_parent;
	struct ff2c_type  *f_type;
	const char        *f_name;
	const char        *f_c_name;
	const char        *f_decl;
	const char        *f_xc_type;
	const char        *f_tag;
	const char        *f_escape;
};

struct ff2c_escape {
	struct ff2c_escape *e_next;
	const char         *e_escape;
};

struct ff2c_ff {
	struct ff2c_list ff_require;
	struct ff2c_list ff_type;
	struct ff2c_list ff_escape;
};

void ff2c_sem_init(struct ff2c_ff *ff, struct ff2c_term *top);
void ff2c_sem_fini(struct ff2c_ff *ff);

char *fmt(const char *format, ...) __attribute__((format(printf, 1, 2)));

/** @} end of xcode group */

/* __MERO_XCODE_FF2C_SEM_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
