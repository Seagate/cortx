/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
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
 * Original author:  Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 20-Oct-2014
 */

#pragma once

#ifndef __MERO_CLOVIS_ST_ASSERT_H__
#define __MERO_CLOVIS_ST_ASSERT_H__

#include "clovis/clovis.h"

/* XXX juan: file defining assertimpl should be #included */
#define CLOVIS_ST_ASSERT_FATAL(a) \
	do { \
		if (!clovis_st_assertimpl((a),#a,__FILE__,__LINE__,__func__))\
			return; \
	} while(0);


/* XXX juan: we need doxygen doc for all these functions. */
/* Functions for cleaner*/
int clovis_st_cleaner_init(void);
void clovis_st_cleaner_fini(void);
bool clovis_st_is_cleaner_up(void);
void clovis_st_cleaner_enable(void);
void clovis_st_cleaner_disable(void);
void clovis_st_cleaner_empty_bin(void);

void clovis_st_mark_op(struct m0_clovis_op *op);
void clovis_st_unmark_op(struct m0_clovis_op *op);
void clovis_st_mark_entity(struct m0_clovis_entity *entity);
void clovis_st_unmark_entity(struct m0_clovis_entity *entity);
void clovis_st_mark_ptr(void *ptr);
void clovis_st_unmark_ptr(void *ptr);

#endif /* __MERO_CLOVIS_ST_ASSERT_H__ */
