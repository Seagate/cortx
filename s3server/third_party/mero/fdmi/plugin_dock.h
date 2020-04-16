/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */


#pragma once

#ifndef __MERO_FDMI_FDMI_PLUGIN_DOCK_H__
#define __MERO_FDMI_FDMI_PLUGIN_DOCK_H__

#include "fdmi/fdmi.h"
#include "lib/types.h"
#include "lib/types_xc.h"
#include "lib/tlist.h"
#include "rpc/session.h"
#include "fid/fid.h"

/**
   @defgroup fdmi_pd FDMI Plugin Dock public interface
   @ingroup fdmi_main

   @see @ref FDMI-DLD-fspec "FDMI Functional Specification"
   @{
*/


/**
  FDMI plugin callback interface - The callback is registered along with filter
  definition, and this way it may be filter specific, or common for entire
  plugin filter set.
 */
struct m0_fdmi_plugin_ops {
	/**
	   Plugin callback to feed FDMI record over.

	   Returns zero in case plugin accepts fdmi record for internal
	   processing. This as well indicates that plugin is going to explicitly
	   inform plugin dock about getting to the end of processing by calling
	   m0_fdmi_pd_if::fpi_release_fdmi_rec(). Otherwise, plugin must return
	   non-zero code.

	   @see struct m0_fdmi_filter_reg
	 */
	int (*po_fdmi_rec) (struct m0_uint128   *rec_id,
			    struct m0_buf        fdmi_rec,
			    struct m0_fid        filter_id);
};

/**
  FDMI filter description

  @todo Phase 2: Internals will be understood during further development
 */
struct m0_fdmi_filter_desc {
};

/**
  Filter registration list item
 */
struct m0_fdmi_filter_reg {
        /** filter identifier unique across the whole Mero cluster */
	struct m0_fid               ffr_ffid;

	struct m0_fdmi_filter_desc *ffr_desc;   /**< filter description */

	struct m0_fdmi_plugin_ops  *ffr_pcb;    /**< plugin callback to be
						 * called with matched record */

	char                       *ffr_ep;     /**< local endpoint address
						 * source dock to use for
						 * posting matched records */
        /** filter state mask: see m0_fdmi_filter_state */
	uint32_t                    ffr_flags;

	/* tl specifics */
	struct m0_tlink             ffr_link;

	uint64_t                    ffr_magic;
};

/**
  FDMI record registration list item
 */
struct m0_fdmi_record_reg {
	struct m0_fop_fdmi_record      *frr_rec;    /**< FDMI record */

	char                           *frr_ep_addr;/**< backward communication
						     * rpc endpoint to source */
/**
   rpc session the release request was sent over
 */
	struct m0_rpc_session          *frr_sess;

	struct m0_ref                   frr_ref;    /**< reference counter */
/** save pointer to initial fop */
	struct m0_fop                  *frr_fop;
	/* tl specifics */
	struct m0_tlink                 frr_link;

	uint64_t                        frr_magic;
};

/**
  FDMI private plugin dock api interface
 */
struct m0_fdmi_pd_ops {
	/**
	  Filter description registration callback
	  - WARNING: It's not enough to just register filter, as it
	  remains inactive until being activated explicitly (see
	  m0_fdmi_pd_if::fpi_enable_filters() callback)
	 */
	int (*fpo_register_filter) (const struct m0_fid              *fid,
				    const struct m0_fdmi_filter_desc *desc,
				    const struct m0_fdmi_plugin_ops  *pcb);

	/**
	  Filter bulk activation/deactivation callback.
	 */
	void (*fpo_enable_filters) (bool           enable,
				    struct m0_fid *filter_ids,
				    uint32_t       filter_count);

	/**
	  FDMI record release callback
	  - Intended for indication the fact that the record may be released
	    by source. Actual release command is sent when there is no plugin
	    still holding the record due to some lengthy processing
	 */
	void (*fpo_release_fdmi_rec) (struct m0_uint128 *rec_id,
				      struct m0_fid     *filter_id);

	/**
	  Plugin deregistration callback
	  - Plugin deregistration results in announcing the plugin dead
	    across the whole Mero cluster, implying corresponding filters
	    to be announced dead as well
	 */
	void (*fpo_deregister_plugin) (struct m0_fid *filter_ids,
				       uint64_t       filter_count);
};

/**
   Accessing FDMI plugin dock api

   @return Returns private plugin dock interface.
   @see struct m0_fdmi_pd_if
 */

const struct m0_fdmi_pd_ops *m0_fdmi_plugin_dock_api_get(void);

#if 0  /* cut off until future development */
/**
 * FDMI filters enable request body
 */
struct m0_fdmi_filters_enable {
	/* @todo Fill this in with filter definitions */
	int foo;
} M0_XCA_RECORD;

/**
 * FDMI filters enable reply body
 */
struct m0_fdmi_filters_enable_reply {
	int ffer_rc;                  /**< enable request result */
} M0_XCA_RECORD;
#endif

/** @} end of fdmi_pd group */

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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
