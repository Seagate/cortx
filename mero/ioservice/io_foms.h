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
 * Original author: Anand Vidwansa <Anand_Vidwansa@xyratex.com>
 *                  Anup Barve <Anup_Barve@xyratex.com>
 * Original creation date: 03/21/2011
 * Revision       : Rajanikant Chirmade <Rajanikant_Chirmade@xyratex.com>
 * Revision date  : 09/14/2011
 */

#pragma once

#ifndef __MERO_IOSERVICE_IO_FOMS_H__
#define __MERO_IOSERVICE_IO_FOMS_H__

/**
   @page DLD-bulk-server-fspec Functional Specification

      - @ref DLD-bulk-server-fspec-ds
      - @ref DLD-bulk-server-fspec-if
      - @ref io_foms "FOP State Machines for I/O FOPs"
      - @ref DLD_bulk_server_fspec_ioservice_operations

   @section DLD-bulk-server-fspec Functional Specification
   This section describes the data structure, external interfaces of the
   component and briefly identifies the consumers of these interfaces.

   @subsection DLD-bulk-server-fspec-ds Data structures

   I/O FOMs use the following data structure:

   The Bulk I/O Service is required to maintain run-time context of I/O FOMs.
   The data structure @ref m0_io_fom_cob_rw maintain required context data.

   - Pointer to generic FOM structure<br>
     This holds the information about generic FOM (e.g. its generic states,
     type, locality, reply FOP, file operation log etc.)
   - Total number of descriptor for bulk data transfer requested.
   - Current network buffer descriptor index
   - Current index vector list index
   - Batch size for bulk I/O processing.
   - Actual data transferd.
   - STOB identifier<br>
     Storage object identifier which tells the actual device on which I/O
     operation intended.
   - List of STOB operation vector<br>
     This holds the information required for async STOB I/O (e.g. data segments,
     operations, channel to signal on completion etc.)
   - List acquired network buffers pointer<br>
     zero-copy fills this buffers in bulk I/O case.

   @subsection DLD-bulk-server-fspec-if Interfaces
   Bulk I/O Service will be implemented as read FOM and write FOM. Since
   request handler processes FOM, each FOM needs to define its operations:

   Bulk I/O FOM type operations:

   @verbatim
   m0_io_fom_cob_rw_create()    Request handler uses this interface to
                                create I/O FOM.
   @endverbatim

   Bulk I/O FOM operations :

   @verbatim
   m0_io_fom_cob_rw_locality_get()   Request handler uses this interface to
                                     get the locality for this I/O FOM.
   m0_io_fom_cob_rw_tick()           Request handler uses this interface to
                                     execute next phase of I/O FOM.
   m0_io_fom_cob_rw_fini()           Request handler uses this interface after
                                     I/O FOM finishes its execution.
   @endverbatim

   Bulk I/O Service type operations :

   @verbatim

   m0_io_service_init()       This interface will be called by request handler
                              to create & initiate Bulk I/O Service.

   @endverbatim

   Bulk I/O Service operations :
   @verbatim

   m0_io_service_start()      This interface will be called by request handler
                              to start Buk I/O Service.

   m0_io_service_stop()       This interface will be called by request handler
                              to stop Buk I/O Service.

   m0_io_service_fini()       This interface will be called by request handler
                              to finish & de-allocate Bulk I/O Service.

   @endverbatim

   Bulk I/O FOM creation & initialization

   On receiving of bulk I/O FOP, FOM is created & initialized for respective
   FOP type. Then it is placed into FOM processing queue.
 */

/**
 * @defgroup io_foms Fop State Machines for IO FOPs
 *
 * Fop state machine for IO operations
 * @see fom
 * @see @ref DLD-bulk-server
 *
 * FOP state machines for various IO operations like
 * - Readv
 * - Writev
 *
 * @note Naming convention: For operation xyz, the FOP is named
 * as m0_fop_xyz, its corresponding reply FOP is named as m0_fop_xyz_rep
 * and FOM is named as m0_fom_xyz. For each FOM type, its corresponding
 * create, state and fini methods are named as m0_fom_xyz_create,
 * m0_fom_xyz_state, m0_fom_xyz_fini respectively.
 *
 *  @{
 */

#include "reqh/reqh.h"     /* M0_FOPH_NR */
#include "fop/fop.h"
#include "ioservice/io_fops.h"
#include "stob/stob.h"
#include "net/net.h"
#include "fop/fom.h"
#include "stob/io.h"       /* m0_stob_io */
#include "cob/cob.h"       /* m0_cob */

struct m0_fid;
struct m0_fop_file_fid;
struct m0_io_fom_cob_rw;

/**
 * Since STOB I/O only launch io for single index vec, I/O service need
 * to launch multiple STOB I/O and wait for all to complete. I/O service
 * will put FOM execution into runqueue only after all STOB I/O complete.
 */
struct m0_stob_io_desc {
	/** Magic to verify sanity of struct m0_stob_io_desc */
	uint64_t		 siod_magic;
	/** Stob IO packet for the operation. */
	struct m0_stob_io        siod_stob_io;
	/** Linkage into m0_io_fom_cob_rw::fcrw_stobio_list */
	struct m0_tlink          siod_linkage;
	struct m0_fom_callback   siod_fcb;
	/**
	 * Fol record part representing stob io operations.
	 * It should be pointed by m0_stob_io::si_fol_frag.
	 */
	struct m0_fol_frag       siod_fol_frag;
};

/**
 * Object encompassing FOM for cob I/O
 * operation and necessary context data
 */
struct m0_io_fom_cob_rw {
	/** Generic m0_fom object. */
	struct m0_fom                    fcrw_gen;
	/** Pool version for this io request. */
	struct m0_pool_version          *fcrw_pver;
	/** Number of desc io_fop desc list */
	uint32_t                         fcrw_ndesc;
	/** index of net buffer descriptor under process */
	int                              fcrw_curr_desc_index;
	/** Total IO requested from m0_io_indexvec */
	m0_bcount_t                      fcrw_total_ioivec_cnt;
	/** Current position in bytes */
	m0_bcount_t                      fcrw_curr_size;
	/** no. of descriptor going to process */
	uint32_t                         fcrw_batch_size;
	/** Number of bytes requested to transfer. */
	m0_bcount_t                      fcrw_req_count;
	/** Number of bytes successfully transferred. */
	m0_bcount_t                      fcrw_count;
	/** Stob block shift */
	uint32_t                         fcrw_bshift;
	/** New cob size in case of update */
	uint64_t                         fcrw_cob_size;

	/**
	 * Summary index vector representing the extent information
	 * for the IO request. Used for BE-credit calculation.
	 */
	struct m0_stob_io                fcrw_io;

	/** Number of STOB I/O launched */
	uint32_t                         fcrw_num_stobio_launched;
	/** Pointer to buffer pool refered by FOM */
	struct m0_net_buffer_pool       *fcrw_bp;
	/** Stob object on which this FOM is acting. */
	struct m0_stob		        *fcrw_stob;
	/** Cob object corresponding to the stob. */
	struct m0_cob                   *fcrw_cob;
	/** Array of all stob IOs we are going to fire (fcrw_ndesc in size). */
	struct m0_stob_io_desc          *fcrw_stio;
	/** The list of fired stob IOs. */
	struct m0_tl                     fcrw_stio_list;
	/** Completed stob IOs, used as holders for fol records. */
	struct m0_tl                     fcrw_done_list;
	/** rpc bulk load data. */
	struct m0_rpc_bulk               fcrw_bulk;
	/** Start time for FOM. */
	m0_time_t                        fcrw_fom_start_time;
	/** Start time for FOM specific phase. */
	m0_time_t                        fcrw_phase_start_time;
	/** network buffer list currently acquired by io service*/
	struct m0_tl                     fcrw_netbuf_list;
	/** Used to store error when any of the stob io fails while
	 *  waiting for stob io to finish(i.e. all stobio call backs
	 *  are returned successfully).
	 */
	int				 fcrw_rc;
	/** fol record fragment representing operations in io sub-system. */
	struct m0_fol_frag		 fcrw_fol_frag;
	/** Time stamp when stob io request was launched */
	m0_time_t                        fcrw_io_launch_time;
	/** The flags from m0_fop_cob_rw::crw_flags */
	uint64_t                         fcrw_flags;
};

/**
 * The various phases for I/O FOM.
 * complete FOM and reqh infrastructure is in place.
 */
enum m0_io_fom_cob_rw_phases {
	M0_FOPH_IO_FOM_PREPARE = M0_FOPH_TYPE_SPECIFIC,
	M0_FOPH_IO_FOM_BUFFER_ACQUIRE,
	M0_FOPH_IO_FOM_BUFFER_WAIT,
	M0_FOPH_IO_STOB_INIT,
	M0_FOPH_IO_STOB_WAIT,
	M0_FOPH_IO_ZERO_COPY_INIT,
	M0_FOPH_IO_ZERO_COPY_WAIT,
	M0_FOPH_IO_BUFFER_RELEASE,
};

/**
 * State transition information.
 */
struct m0_io_fom_cob_rw_state_transition {
	/** Current phase of I/O FOM */
	int         fcrw_st_current_phase;
	/** Function which executes current phase */
	int         (*fcrw_st_state_function)(struct m0_fom *);
	/** Next phase in which FOM is going to execute */
	int         fcrw_st_next_phase_again;
	/** Next phase in which FOM is going to wait */
	int         fcrw_st_next_phase_wait;
	/** Description of phase */
	const char *fcrw_st_desc;
};

/**
 * Returns string representing ioservice name given a fom.
 */
M0_INTERNAL const char *m0_io_fom_cob_rw_service_name(struct m0_fom *fom);

M0_INTERNAL int m0_io_cob_create(struct m0_cob_domain *cdom,
				 struct m0_fid *fid,
				 struct m0_fid *pver,
				 uint64_t lid,
				 struct m0_be_tx *tx);
/**
 * If cob of different version exists, it will delete it and recreate the
 * cob with pool version pver.
 * If crow is true, it will create the cob if it does not exist.
 */
M0_INTERNAL int m0_io_cob_stob_create(struct m0_fom *fom,
				      struct m0_cob_domain *ios_cdom,
				      struct m0_fid *fid,
				      struct m0_fid *pver,
				      uint64_t lid,
				      bool crow,
				      struct m0_cob **out);

M0_INTERNAL uint64_t m0_io_size(struct m0_stob_io *sio, uint32_t bshift);

/** @} end of io_foms */

#endif /* __MERO_IOSERVICE_IO_FOMS_H__ */
 /*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
