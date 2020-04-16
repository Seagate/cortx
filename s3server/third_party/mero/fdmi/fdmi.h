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

#ifndef __MERO_FDMI_FDMI_H__
#define __MERO_FDMI_FDMI_H__

#include "lib/types.h"
#include "lib/buf.h"
#include "lib/errno.h"
#include "lib/refs.h"
#include "lib/vec.h" /* m0_bufvec_cursor */

/**
   @page FDMI-DLD-fspec Functional Specification
   - @ref FDMI-DLD-fspec-ds
   - @ref FDMI-DLD-fspec-sub
   - @ref FDMI-DLD-fspec-usecases
   - Detailed functional specifications:
     - @ref fdmi_main
     - @ref fdmi_sd
     - @ref fdmi_sd_int
     - @ref FDMI_DLD_fspec_filter
     - @ref FDMI_DLD_fspec_filter_eval
     - @ref FDMI_DLD_fspec_filterc
     - @ref fdmi_pd
     - @ref fdmi_pd_int
     - @ref fdmi_fol_src

   <hr>
   @section FDMI-DLD-fspec-ds Data Structures

   Below follow FDMI common data types (used by th FDMI service, source dock and
   plugin dock):
   - ::m0_fdmi_dock_type_id
   - ::m0_fdmi_rec_type_id
   - m0_reqh_fdmi_svc_params
   - m0_reqh_fdmi_service

   FDMI Source dock public data types:
   - m0_fdmi_src
   - m0_fdmi_src_rec

   FDMI Source dock internal data types:
   - m0_fdmi_src_dock
   - m0_fdmi_src_ctx
   - fdmi_sd_fom
   - fdmi_rr_fom

   FDMI filter expression public API data types:
   - m0_fdmi_filter
   - m0_fdmi_flt_node
   - m0_fdmi_flt_operand
   - m0_fdmi_flt_var_node
   - m0_fdmi_flt_op_node

   FDMI filter evaluator data types:
   - m0_fdmi_eval_ctx
   - m0_fdmi_flt_operands
   - m0_fdmi_eval_var_info

   FDMI filter client data types:
   - m0_filterc_ctx
   - m0_filterc_iter
   - m0_filterc_ops
   - m0_conf_fdmi_filter
   - ::m0_fdmi_filter_state


   FDMI Plugin dock public data types:
   - m0_fdmi_pcb_if
   - m0_fdmi_pd_if
   - m0_fdmi_filter_desc

   FDMI Plugin dock internal data types:
   - m0_fdmi_filter_reg
   - m0_fdmi_record_reg
   - pdock_fom

   <hr>
   @section FDMI-DLD-fspec-sub Subroutines and Macros
   FDMI service API:
   - m0_fdms_register()
   - m0_fdms_unregister()

   FDMI Source dock public API:
   - m0_fdmi_source_dock_init()
   - m0_fdmi_source_dock_fini()
   - m0_fdmi_source_register()
   - m0_fdmi_source_deregister()
   - M0_FDMI_SOURCE_DOCK_TYPE_DECLARE()
   - M0_FDMI_SOURCE_POST_RECORD()

   FDMI Source dock internal functions:
   - m0_fdmi__record_post()
   - m0_fdmi__rec_id_gen()
   - m0_fdmi__src_dock_fom_init()
   - m0_fdmi__src_dock_fom_start()
   - m0_fdmi__src_dock_fom_stop()
   - m0_fdmi__src_ctx_get()
   - m0_fdmi__fs_get()
   - m0_fdmi__fs_put()
   - m0_fdmi__fs_begin()
   - m0_fdmi__fs_end()
   - m0_fdmi__record_init()
   - m0_fdmi__record_deinit()
   - m0_fdmi__record_is_valid()
   - m0_fdmi__sd_rec_type_id_get()
   - m0_fdmi__handle_reply()
   - m0_fdmi__handle_release()

   FDMI filter expression public API:
   - m0_fdmi_filter_init()
   - m0_fdmi_filter_fini()
   - m0_fdmi_filter_set_root()
   - m0_fdmi_flt_create_op_node()
   - m0_fdmi_flt_create_bool_node()
   - m0_fdmi_flt_create_int_node()
   - m0_fdmi_flt_create_uint_node()
   - m0_fdmi_flt_fill_bool_opnd()
   - m0_fdmi_flt_fill_int_opnd()
   - m0_fdmi_flt_fill_uint_opnd()
   - m0_fdmi_flt_node_to_str()
   - m0_fdmi_flt_node_from_str()

   FDMI filter evaluator public API:
   - m0_fdmi_eval_init()
   - m0_fdmi_eval_flt()
   - m0_fdmi_eval_fini()
   - m0_fdmi_eval_add_op_cb()
   - m0_fdmi_eval_del_op_cb()

   FDMI filter client functions:
   - m0_filterc_ctx_init()
   - m0_filterc_ctx_fini()
   - m0_filterc_ops::fco_start
   - m0_filterc_ops::fco_stop
   - m0_filterc_ops::fco_open
   - m0_filterc_ops::fco_get_next
   - m0_filterc_ops::fco_close

   FDMI Plugin dock public API:
   - m0_fdmi_plugin_dock_api_get

   Fdmi Plugin dock internal functions:
   - m0__fdmi_plugin_dock_init()
   - m0__fdmi_plugin_dock_start()
   - m0__fdmi_plugin_dock_stop()
   - m0__fdmi_plugin_dock_fini()
   - m0__fdmi_plugin_dock_fom_init()
   - m0__fdmi_pdock_fdmi_record_register()
   - m0__fdmi_pdock_filter_reg_find()
   - m0__fdmi_pdock_record_reg_find()
   - m0__fdmi_get_pdock_fom_type_ops()

   FDMI FOL source functions:
   - m0_fol_fdmi_src_init()
   - m0_fol_fdmi_src_fini()
   - m0_fol_fdmi_src_deinit()
   - m0_fol_fdmi_post_record()

   <hr>
   @section FDMI-DLD-fspec-usecases Recipes
   - TBD

*/


/**
   @addtogroup  FDMI_DLD_fspec_filter
   @{
 */

M0_EXTERN struct m0_reqh_service_type m0_fdmi_service_type;

/**
  FDMI filter state flags

  Note: Currently only INACTIVE and DEAD states are envisioned,
        though later there may be some more additions to that
*/
enum m0_fdmi_filter_state {
	M0_FDMI_FILTER_INACTIVE = 0,
	M0_FDMI_FILTER_DEAD,

	/* add flags above */
	M0_FDMI_FILTER_STATE_NR
};

/** @} addtogroup FDMI_DLD_fspec_filter */

/**
   @defgroup fdmi_main File Data Manipulation Interface API
   @{
*/

/**
  FDMI dock type

  Indicates what role FDMI service is to initiate. Initiation results in dock
  envirinment setup, including specific FOM, RPC, queues, etc.

  Note: Mero node is allowed to have both roles initialized simultaneously.
 */
enum m0_fdmi_dock_type_id {
	M0_FDMI_DOCK_TYPE_SOURCE = 0,
	M0_FDMI_DOCK_TYPE_PLUGIN,
	M0_FDMI_DOCK_TYPE_NR
};

/**
   FDMI record type enumeration
 */
enum m0_fdmi_rec_type_id {
	/** This rec type is only used for tests. */
	M0_FDMI_REC_TYPE_TEST = 0x100,
	/** FDMI record provided by FOL subsystem */
	M0_FDMI_REC_TYPE_FOL = 0x1000,
	/** FDMI record provided by ADDB subsystem */
	M0_FDMI_REC_TYPE_ADDB
};

/** Initializes FDMI subsystem */
M0_INTERNAL int m0_fdmi_init(void);

/** Deinitializes FDMI subsystem */
M0_INTERNAL void m0_fdmi_fini(void);

/** Return pointer to source dock context */
M0_INTERNAL struct m0_fdmi_src_dock *m0_fdmi_src_dock_get(void);

/** @} end of fdmi_main group */

#endif /* __MERO_FDMI_FDMI_H__ */

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
