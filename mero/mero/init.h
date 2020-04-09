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
 * Original creation date: 06/19/2010
 */

#pragma once

#ifndef __MERO_MERO_INIT_H__
#define __MERO_MERO_INIT_H__

struct m0;

/**
   @defgroup init Mero initialisation calls.

   @{
 */

#if 1 /* XXX OBSOLETE */
/**
   Performs all global initializations of M0 sub-systems. The nomenclature of
   sub-systems to be initialised depends on the build configuration.

   @see m0_fini().
 */
int m0_init(struct m0 *instance);

/**
   Finalizes all sub-systems initialised by m0_init().
 */
void m0_fini(void);

/**
   Performs part global initializations of M0 sub-systems, when stopped before
   reconfigure Mero.

   @see m0_init(), @see m0_fini(), @see m0_quiesce().
 */
int m0_resume(struct m0 *instance);

/**
   Finalizes part global initializations of M0 sub-systems, for starting
   reconfigure Mero. Sub-systems finalize from high level to quiese level.
   @see M0_LEVEL_INST_QUIESCE_SYSTEM.

   @see m0_init(), @see m0_fini(), @see m0_resume().
 */
void m0_quiesce(void);

#endif /* XXX OBSOLETE */

/** @} end of init group */
#endif /* __MERO_MERO_LIST_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
