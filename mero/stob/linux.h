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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 04/28/2010
 */

#pragma once

#ifndef __MERO_STOB_LINUX_H__
#define __MERO_STOB_LINUX_H__

#include "sm/sm.h"              /* m0_sm_ast */
#include "stob/stob.h"		/* m0_stob_type */
#include "stob/domain.h"	/* m0_stob_domain */
#include "stob/ioq.h"		/* m0_stob_ioq */

/**
   @defgroup stoblinux Storage object based on Linux specific file system
   and block device interfaces.

   @see stob
   @{
 */

struct m0_stob_linux_domain_cfg {
	mode_t sldc_file_mode;
	int    sldc_file_flags;
	bool   sldc_use_directio;
};

struct m0_stob_linux_domain {
	struct m0_stob_domain		 sld_dom;
	struct m0_stob_ioq		 sld_ioq;
	/** parent directory to hold the objects  */
	char				*sld_path;
	/** @see m0_stob_type_ops::sto_domain_cfg_init_parse() */
	struct m0_stob_linux_domain_cfg	 sld_cfg;
};

struct m0_stob_linux {
	struct m0_stob		     sl_stob;
	struct m0_stob_linux_domain *sl_dom;
	/** fd from returned open(2) */
	int			     sl_fd;
	/** file mode as returned by stat(2) */
	mode_t			     sl_mode;
	/** fid of the corresponding m0_conf_sdev object */
	struct m0_fid                sl_conf_sdev;
};

M0_INTERNAL struct m0_stob_linux *m0_stob_linux_container(struct m0_stob *stob);
M0_INTERNAL struct m0_stob_linux_domain *
m0_stob_linux_domain_container(struct m0_stob_domain *dom);

/**
 * Reopen the stob to update it's file descriptor.
 * Find the stob from the provided stob_id and destroy it to get rid
 * of the stale fd. Create the stob with provided path to reopen the
 * underlying device, create will also update the stob with new fd.
 */
M0_INTERNAL int m0_stob_linux_reopen(struct m0_stob_id *stob_id,
				     const char *f_path);

/**
 * Associates linux stob with fid of a m0_conf_sdev object.
 * This fid is sent to HA when stob I/O error is reported.
 */
M0_INTERNAL void
m0_stob_linux_conf_sdev_associate(struct m0_stob      *stob,
                                  const struct m0_fid *conf_sdev);

/**
 * Obtains file descriptor of a file which is stored on the same local
 * filesystem where objects are.
 */
M0_INTERNAL int
m0_stob_linux_domain_fd_get(struct m0_stob_domain *dom, int *fd);

/** Closes previously obtained file descriptor. */
M0_INTERNAL int m0_stob_linux_domain_fd_put(struct m0_stob_domain *dom, int fd);

M0_INTERNAL bool m0_stob_linux_domain_directio(struct m0_stob_domain *dom);

extern const struct m0_stob_type m0_stob_linux_type;

/** @} end group stoblinux */
#endif /* __MERO_STOB_LINUX_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
