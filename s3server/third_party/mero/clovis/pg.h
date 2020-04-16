/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original authors: Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Revision:         Pratik Shinde   <pratik.shinde@seagate.com>
 * Original creation date: 3-Nov-2014
 */

#pragma once

#ifndef __MERO_CLOVIS_PG_H__
#define __MERO_CLOVIS_PG_H__

#include "lib/hash.h"
#include "ioservice/io_fops.h"  /* m0_io_fop_{init,fini,release} */
#include "layout/pdclust.h"     /* struct m0_pdclust_attr */
#include "pool/pool.h"          /* struct m0_pool */

/**
 * Everything in this header file is copied from
 * m0t1fs/linux_kernel/file_internal.h.
 *
 * Changes are summarised here:
 * Change io_request to clovis_op_io, which is the equivalent.
 *
 */

/* Exports */
struct pargrp_iomap;
struct target_ioreq;
struct ioreq_fop;
struct nw_xfer_request;

/**
 * Represents state of IO request call.
 * m0_sm_state_descr structure will be defined for description of all
 * states mentioned below.
 */
enum ioreq_state {
	IRS_UNINITIALIZED,
	IRS_INITIALIZED,
	IRS_READING,
	IRS_WRITING,
	IRS_READ_COMPLETE,
	IRS_WRITE_COMPLETE,
	IRS_TRUNCATE,
	IRS_TRUNCATE_COMPLETE,
	IRS_DEGRADED_READING,
	IRS_DEGRADED_WRITING,
	IRS_REQ_COMPLETE,
	IRS_FAILED,
};

/**
 * Page attributes for all pages spanned by pargrp_iomap::pi_ivec.
 * This enum is also used by data_buf::db_flags.
 */
enum page_attr {
	/** Page not spanned by io vector. */
	PA_NONE                = 0,

	/** Page needs to be read. */
	PA_READ                = (1 << 0),

	/**
	 * Page is completely spanned by incoming io vector, which is why
	 * file data can be modified while read IO is going on.
	 * Such pages need not be read from server.
	 * Mutually exclusive with PA_READ and PA_PARTPAGE_MODIFY.
	 */
	PA_FULLPAGE_MODIFY     = (1 << 1),

	/**
	 * Page is partially spanned by incoming io vector, which is why
	 * it has to wait till the whole page (superset) is read first and
	 * then it can be modified as per user request.
	 * Used only in case of read-modify-write.
	 * Mutually exclusive with PA_FULLPAGE_MODIFY.
	 */
	PA_PARTPAGE_MODIFY     = (1 << 2),

	/** Page needs to be written. */
	PA_WRITE               = (1 << 3),

	/** Page contains file data. */
	PA_DATA                = (1 << 4),

	/** Page contains parity. */
	PA_PARITY              = (1 << 5),

	/**
	 * Data has been copied from user-space into page.
	 * Flag used only when copy_direction == CD_COPY_FROM_USER.
	 * This flag is needed since in case of read-old approach,
	 * even if page/s are fully modified, they have to be read
	 * in order to generate correct parity.
	 * Hence for read-modify-write requests, fully modified pages
	 * from parity groups which have adopted read-old approach
	 * can not be copied before read state finishes.
	 */
	PA_COPY_FRMUSR_DONE    = (1 << 6),

	/** Read IO failed for given page. */
	PA_READ_FAILED         = (1 << 7),

	/** Page needs to be read in degraded mode read IO state. */
	PA_DGMODE_READ         = (1 << 8),

	/** Page needs to be written in degraded mode write IO state. */
	PA_DGMODE_WRITE        = (1 << 9),

	/** The application provided this memory, don't try to free it */
	PA_APP_MEMORY          = (1 << 10),

	/** This page will be truncated, so don't do RW operation. */
	PA_TRUNC               = (1 << 11),

	PA_NR                  = 12,
};

/**
 * Type of read approach used by pargrp_iomap structure
 * in case of rmw IO.
 */
enum pargrp_iomap_rmwtype {
	PIR_NONE,
	PIR_READOLD,
	PIR_READREST,
	PIR_NR,
};

/** State of parity group during IO life-cycle. */
enum pargrp_iomap_state {
	PI_NONE,
	PI_HEALTHY,
	PI_DEGRADED,
	PI_NR,
};


/** State of struct nw_xfer_request. */
enum nw_xfer_state {
	NXS_UNINITIALIZED,
	NXS_INITIALIZED,
	NXS_INFLIGHT,
	NXS_COMPLETE,
	NXS_STATE_NR,
};

/** Enum representing direction of data copy in IO. */
enum copy_direction {
	CD_COPY_FROM_APP,
	CD_COPY_TO_APP,
};

/**
 * Represents a simple data buffer wrapper object. The embedded
 * db_buf::b_addr points to a 4K block of memory.
 */
struct data_buf {
	uint64_t             db_magic;

	/** Inline buffer pointing to a kernel page or user memory,(or both). */
	struct m0_buf        db_buf;

	/**
	 * Auxiliary buffer used in case of read-modify-write IO.
	 * Used when page pointed to by ::db_buf::b_addr is partially spanned
	 * by incoming rmw request.
	 */
	struct m0_buf        db_auxbuf;

	/**
	 * Miscellaneous flags.
	 * Can be used later for caching options.
	 */
	enum page_attr       db_flags;

	/**
	 * Link to target-io-request to which the buffer will be mapped, in
	 * case it is part of input IO request.
	 */
	struct target_ioreq *db_tioreq;

	/**
	 * Represents index and value of majority element in case of a
	 * replicated layout. The value of key is unit number in a
	 * parity group.
	 */
	struct m0_key_val    db_maj_ele;

	/**
	 * Holds the CRC value associated with the page in parity-verify mode.
	 */
	uint64_t             db_crc;

	/**
	 * Key associated with the buffer to be used for replicated layout.
	 */
	uint32_t             db_key;
};

/** Operation vector for struct nw_xfer_request. */
struct nw_xfer_ops {
	/**
	 * Distributes file data between target_ioreq objects as needed and
	 * populates target_ioreq::ir_ivec and target_ioreq::ti_bufvec.
	 * @pre   nw_xfer_request_invariant(xfer).
	 * @post  !tioreqs_list_is_empty(xfer->nxr_tioreqs).
	 */
	int  (*nxo_distribute) (struct nw_xfer_request  *xfer);

	/**
	 * Does post processing of a network transfer request.
	 * Primarily all IO fops submitted by this network transfer request
	 * are finalized so that new fops can be created for same request.
	 * @param rmw  Boolean telling if current IO request is rmw or not.
	 * @pre   nw_xfer_request_invariant(xfer).
	 * @post  xfer->nxr_state == NXS_COMPLETE.
	 */
	void (*nxo_complete)   (struct nw_xfer_request  *xfer,
				bool                     rmw);

	/**
	 * Dispatches the IO fops created by all member target_ioreq objects
	 * and sends them to server for processing.
	 * The whole process is done in an asynchronous manner and does not
	 * block the thread during processing.
	 * @pre   nw_xfer_request_invariant(xfer).
	 * @post  xfer->nxr_state == NXS_INFLIGHT.
	 */
	int  (*nxo_dispatch)   (struct nw_xfer_request  *xfer);

	/**
	 * Locates or creates a target_iroeq object which maps to the given
	 * target address.
	 * @param src  Source address comprising of parity group number
	 * and unit number in parity group.
	 * @param tgt  Target address comprising of frame number and
	 * target object number.
	 * @param out  Out parameter containing target_ioreq object.
	 * @pre   nw_xfer_request_invariant(xfer).
	 */
	int  (*nxo_tioreq_map) (struct nw_xfer_request           *xfer,
				const struct m0_pdclust_src_addr *src,
				struct m0_pdclust_tgt_addr       *tgt,
				struct target_ioreq             **out);
};

/**
 * Structure representing the network transfer part for an IO request.
 * This structure keeps track of request IO fops as well as individual
 * completion callbacks and status of whole request.
 * Typically, all IO requests are broken down into multiple fixed-size
 * requests.
 */
struct nw_xfer_request {
	uint64_t                  nxr_magic;

	/** Resultant status code for all IO fops issued by this structure. */
	int                       nxr_rc;

	/** Resultant number of bytes read/written by all IO fops. */
	uint64_t                  nxr_bytes;

	enum nw_xfer_state        nxr_state;

	const struct nw_xfer_ops *nxr_ops;

	/**
	 * Hash of target_ioreq objects. Helps to speed up the lookup
	 * of target_ioreq objects based on a key
	 * (target_ioreq::ti_fid::f_container)
	 */
	struct m0_htable          nxr_tioreqs_hash;

	/** lock to pretect the following two counter */
	struct m0_mutex           nxr_lock;
	/**
	 * Number of IO fops issued by all target_ioreq structures
	 * belonging to this nw_xfer_request object.
	 * This number is updated when bottom halves from ASTs are run.
	 * For WRITE, When it reaches zero, state of io_request::ir_sm changes.
	 * For READ, When it reaches zero and read bulk count reaches zero,
	 * state of io_request::ir_sm changes.
	 */
	struct m0_atomic64       nxr_iofop_nr;

	/**
	 * Number of read IO bulks issued by all target_ioreq structures
	 * belonging to this nw_xfer_request object.
	 * This number is updated in read bulk transfer complete callback.
	 * When it reaches zero, and nxr_iofop_nr reaches zero, state of
	 * io_request::ir_sm changes
	 */
	struct m0_atomic64       nxr_rdbulk_nr;

	/**
	 * Number of cob create fops issued over different targets.
	 * The number is decremented in cc_bottom_half.
	 */
	struct m0_atomic64        nxr_ccfop_nr;
};

/**
 * Represents a map of io extents in a given parity group.
 * Struct m0_clovis_op_io contains as many pargrp_iomap structures
 * as the number of parity groups spanned by m0_clovis_op_io::ioo_ivec.
 * Typically, the segments from pargrp_iomap::pi_ivec are round_{up/down}
 * to nearest page boundary for respective segments from
 * m0_clovis_op_io::ioo_ivec.
 */
struct pargrp_iomap {
	uint64_t                        pi_magic;

	/** Parity group id. */
	uint64_t                        pi_grpid;

	/** State of parity group during IO life-cycle. */
	enum pargrp_iomap_state         pi_state;

	/**
	 * Part of m0_clovis_op_io::ioo_ivec which falls in ::pi_grpid
	 * parity group.
	 * All segments are in increasing order of file offset.
	 * Segment counts in this index vector are multiple of PAGE_SIZE.
	 */
	struct m0_indexvec              pi_ivec;

	/**
	 * Type of read approach used only in case of rmw IO.
	 * Either read-old or read-rest.
	 */
	enum pargrp_iomap_rmwtype       pi_rtype;

	/**
	 * Data units in a parity group.
	 * Unit size should be multiple of PAGE_SIZE.
	 * This is basically a matrix with
	 * - number of rows    = Unit_size / PAGE_SIZE and
	 * - number of columns = N.
	 * Each element of matrix is worth PAGE_SIZE;
	 * A unit size worth of data holds a contiguous chunk of file data.
	 * The file offset grows vertically first and then to the next
	 * data unit.
	 */
	struct data_buf              ***pi_databufs;

	/** The maximum row value to use when accessing pi_databufs */
	uint32_t                        pi_max_row;

	/** The maximum col value to use when accessing pi_databufs */
	uint32_t                        pi_max_col;

	/**
	 * Parity units in a parity group.
	 * Unit size should be multiple of PAGE_SIZE.
	 * This is a matrix with
	 * - number of rows    = Unit_size / PAGE_SIZE and
	 * - number of columns = K.
	 * Each element of matrix is worth PAGE_SIZE;
	 */
	struct data_buf              ***pi_paritybufs;

	/** Operations vector. */
	const struct pargrp_iomap_ops  *pi_ops;

	/** Backlink to m0_clovis_op_io. */
	struct m0_clovis_op_io         *pi_ioo;

	/**
	 * If Truncate spans partially, parity units will be updated and only
	 * corresponding data units are truncated.
	 */
	bool                            pi_trunc_partial;
	/**
	 * In case of a replicated layout this indicates whether
	 * any of the replicas of this group are corrupted.
	 */
	bool                            pi_is_corrupted;
};

/** Operations vector for struct pargrp_iomap. */
struct pargrp_iomap_ops {
	/**
	 * Populates pargrp_iomap::pi_ivec by deciding whether to follow
	 * read-old approach or read-rest approach.
	 * pargrp_iomap::pi_rtype will be set to PIR_READOLD or
	 * PIR_READREST accordingly.
	 * @param ivec   Source index vector from which pargrp_iomap::pi_ivec
	 * will be populated. Typically, this is m0_clovis_op_io::ioo_ivec.
	 * @param cursor Index vector cursor associated with ivec.
	 * @pre iomap != NULL && ivec != NULL &&
	 * m0_vec_count(&ivec->iv_vec) > 0 && cursor != NULL &&
	 * m0_vec_count(&iomap->iv_vec) == 0
	 * @post  m0_vec_count(&iomap->iv_vec) > 0 &&
	 * iomap->pi_databufs != NULL.
	 */
	int (*pi_populate)  (struct pargrp_iomap      *iomap,
			     const struct m0_indexvec *ivec,
			     struct m0_ivec_cursor    *cursor,
			     struct m0_bufvec_cursor  *buf_cursor);

	/**
	 * Returns true if the given segment is spanned by existing segments
	 * in pargrp_iomap::pi_ivec.
	 * @param index Starting index of incoming segment.
	 * @param count Count of incoming segment.
	 * @pre   pargrp_iomap_invariant(iomap).
	 * @ret   true if segment is found in pargrp_iomap::pi_ivec,
	 * false otherwise.
	 */
	bool (*pi_spans_seg) (struct pargrp_iomap *iomap,
			      m0_bindex_t          index,
			      m0_bcount_t          count);

	/**
	 * Changes pargrp_iomap::pi_ivec to suit read-rest approach
	 * for an RMW IO request.
	 * @pre  pargrp_iomap_invariant(iomap).
	 * @post pargrp_iomap_invariant(iomap).
	 */
	int (*pi_readrest)   (struct pargrp_iomap *iomap);

	/**
	 * Finds out the number of pages _completely_ spanned by incoming
	 * io vector. Used only in case of read-modify-write IO.
	 * This is needed in order to decide the type of read approach
	 * {read_old, read_rest} for the given parity group.
	 * @pre pargrp_iomap_invariant(map).
	 * @ret Number of pages _completely_ spanned by pargrp_iomap::pi_ivec.
	 */
	uint64_t (*pi_fullpages_find) (struct pargrp_iomap *map);

	/**
	 * Processes segment pointed to by segid in pargrp_iomap::pi_ivec and
	 * allocate data_buf structures correspondingly.
	 * It also populates data_buf::db_flags for pargrp_iomap::pi_databufs.
	 * @param segid Segment identifier which needs to be processed.
	 * Given seg id should point to last segment in
	 * pargrp_iomap::pi_ivec when invoked.
	 * @param rmw   If given pargrp_iomap structure needs rmw.
	 * @pre   map != NULL.
	 * @post  pargrp_iomap_invariant(map).
	 *
	 */
	int (*pi_seg_process)    (struct pargrp_iomap *map,
				  uint64_t             segid,
				  bool                 rmw,
				  uint64_t             start_buf_index,
				  struct m0_bufvec_cursor *buf_cursor);

	/**
	 * Processes the data buffers in pargrp_iomap::pi_databufs
	 * when read-old approach is chosen.
	 * Auxiliary buffers are allocated here.
	 * @pre pargrp_iomap_invariant(map) && map->pi_rtype == PIR_READOLD.
	 */
	int (*pi_readold_auxbuf_alloc) (struct pargrp_iomap *map);

	int (*pi_databuf_alloc)(struct pargrp_iomap *map,
				uint32_t             row,
				uint32_t             col,
				struct m0_bufvec_cursor *buf_cursor);


	/**
	 * Recalculates parity for given pargrp_iomap.
	 * @pre map != NULL && map->pi_ioreq->ir_type == IRT_WRITE.
	 */
	int (*pi_parity_recalc)(struct pargrp_iomap *map);

	/**
	 * verify parity for given pargrp_iomap for read operation and
	 * in 'parity verify' mode, after data and parity units are all
	 * already read from ioserive.
	 * @pre map != NULL
	 */
	int (*pi_parity_verify)(struct pargrp_iomap *map);

	/**
	 * In case of a replicated layout this compares the members of
	 * a parity group for equality.
	 */
	int (*pi_parity_replica_verify)(struct pargrp_iomap *map);

	/**
	 * In case of a replicated layout this method replicates
	 * data into parity buffers.
	 */
	int (*pi_data_replicate)(struct pargrp_iomap *map);

	/**
	 * Allocates data_buf structures for pargrp_iomap::pi_paritybufs
	 * and populate db_flags accordingly.
	 * @pre   map->pi_paritybufs == NULL.
	 * @post  map->pi_paritybufs != NULL && pargrp_iomap_invariant(map).
	 */
	int (*pi_paritybufs_alloc)(struct pargrp_iomap *map);

	/**
	 * Does necessary processing for degraded mode read IO.
	 * Marks pages belonging to failed targets with a flag PA_READ_FAILED.
	 * @param index Array of target indices that fall in given parity
	 *              group. These are converted to global file offsets.
	 * @param count Number of segments provided.
	 * @pre   map->pi_state == PI_HEALTHY.
	 * @post  map->pi_state == PI_DEGRADED.
	 */
	int (*pi_dgmode_process)(struct pargrp_iomap *map,
				 struct target_ioreq *tio,
				 m0_bindex_t         *index,
				 uint32_t             count);

	/**
	 * Marks all but the failed pages with flag PA_DGMODE_READ in
	 * data matrix and parity matrix.
	 */
	int (*pi_dgmode_postprocess)(struct pargrp_iomap *map);

	/**
	 * Recovers lost unit/s by using data recover APIs from
	 * underlying parity algorithm.
	 * @pre  map->pi_state == PI_DEGRADED.
	 * @post map->pi_state == PI_HEALTHY.
	 */
	int (*pi_dgmode_recover)(struct pargrp_iomap *map);

	/**
	 * In case of degraded mode picks the replica with majority.
	 */
	int (*pi_replica_recover)(struct pargrp_iomap *map);
};

/** Operation vector for struct io_request. */
struct m0_clovis_op_io_ops {
	/**
	 * Prepares pargrp_iomap structures for the parity groups spanned
	 * by io_request::ir_ivec.
	 * @pre   req->ir_iomaps == NULL && req->ir_iomap_nr == 0.
	 * @post  req->ir_iomaps != NULL && req->ir_iomap_nr > 0.
	 */
	int (*iro_iomaps_prepare) (struct m0_clovis_op_io *ioo);

	/**
	 * Finalizes and deallocates pargrp_iomap structures.
	 * @pre  req != NULL && req->ir_iomaps != NULL.
	 * @post req->ir_iomaps == NULL && req->ir_iomap_nr == 0.
	 */
	void (*iro_iomaps_destroy)(struct m0_clovis_op_io *ioo);

	/**
	 * Copies data from/to application buffers according
	 * to given direction and page filter.
	 * @param dir    Direction of copy.
	 * @param filter Only copy pages that match the filter.
	 * @pre   io_request_invariant(req).
	 */
	int (*iro_application_data_copy) (struct m0_clovis_op_io *ioo,
					  enum copy_direction     dir,
					  enum page_attr          filter);

	/**
	 * Recalculates parity for all pargrp_iomap structures in
	 * given io_request.
	 * Basically, invokes parity_recalc() routine for every
	 * pargrp_iomap in io_request::ir_iomaps.
	 * @pre  io_request_invariant(req) && req->ir_type == IRT_WRITE.
	 * @post io_request_invariant(req).
	 */
	int (*iro_parity_recalc)  (struct m0_clovis_op_io *ioo);

	/**
	 * Verifies parity for all pargrp_iomap structures in
	 * given io_request in 'parity verify' mode and for READ request.
	 * Basically, invokes parity_verify() routine for every
	 * pargrp_iomap in io_request::ir_iomaps.
	 * @pre  io_request_invariant(req) && req->ir_type == IRT_READ.
	 * @post io_request_invariant(req).
	 */
	int (*iro_parity_verify)  (struct m0_clovis_op_io  *ioo);

	/**
	 * Handles the state transition, status of request and the
	 * intermediate copy_{from/to}_user for the launch of the initial
	 * request, and setup of subsequent callbacks.
	 * @pre io_request_invariant(req).
	 */
	void (*iro_iosm_handle_launch)(struct m0_sm_group *grp,
				       struct m0_sm_ast *ast);

	/**
	 * Handles the state transition, status of request and the
	 * intermediate copy_{from/to}_user for the initial reply of a
	 * request.
	 * @pre io_request_invariant(req).
	 */
	void (*iro_iosm_handle_executed)(struct m0_sm_group *grp,
					  struct m0_sm_ast *ast);

	/**
	 * Handles degraded mode read IO. Issues read IO for pages
	 * in all parity groups which need to be read in order to
	 * recover lost data.
	 * @param rmw Tells whether current io_request is rmw or not.
	 * @pre   req->ir_state == IRS_READ_COMPLETE.
	 * @pre   io_request_invariant(req).
	 * @post  req->ir_state == IRS_DEGRADED_READING.
	 */
	int (*iro_dgmode_read)    (struct m0_clovis_op_io *ioo, bool rmw);

	/**
	 * Recovers lost unit/s by calculating parity over remaining
	 * units.
	 * @pre  req->ir_state == IRS_READ_COMPLETE &&
	 *       io_request_invariant(req).
	 * @post io_request_invariant(req).
	 */
	int (*iro_dgmode_recover) (struct m0_clovis_op_io *ioo);

	/**
	 * Handles degraded mode write IO. Finds out whether SNS repair
	 * has finished on given global fid or is still due.
	 * This is done in context of a distributed lock on the given global
	 * file.
	 * @param rmw Tells whether current io request is rmw or not.
	 * @pre  req->ir_state == IRS_WRITE_COMPLETE.
	 * @pre  io_request_invariant(req).
	 * @post req->ir_state == IRS_DEGRADED_READING.
	 */
	int (*iro_dgmode_write)   (struct m0_clovis_op_io *ioo, bool rmw);

	/**
	 * This method fixes the affected replicas from the corrupted parity
	 * groups.
	 */
	int (*iro_replica_rectify) (struct m0_clovis_op_io *ioo, bool rmw);
};

/** Operations vector for struct target_ioreq. */
struct target_ioreq_ops {
	/**
	 * Adds an io segment to index vector and buffer vector in
	 * target_ioreq structure.
	 */
	void (*tio_seg_add)     (struct target_ioreq              *ti,
				 const struct m0_pdclust_src_addr *src,
				 const struct m0_pdclust_tgt_addr *tgt,
				 m0_bindex_t                       gob_offset,
				 m0_bcount_t                       count,
				 struct pargrp_iomap              *map);

	/**
	 * Prepares io fops from index vector and buffer vector.
	 * This API uses rpc bulk API to store net buffer descriptors
	 * in IO fops.
	 */
	int  (*tio_iofops_prepare) (struct target_ioreq *ti,
				    enum page_attr       filter);

	/**
	 * Prepares cob create/truncate fops for the given target.
	 */
	int (*tio_cc_fops_prepare) (struct target_ioreq *ti);
};

/**
 * IO vector for degraded mode read or write.
 * This is not used when pool state is healthy.
 */
struct dgmode_rwvec {
	/**
	 * Index vector to hold page indices during degraded mode
	 * read/write IO.
	 */
	struct m0_indexvec   dr_ivec;

	/**
	 * Buffer vector to hold page addresses during degraded mode
	 * read/write IO.
	 */
	struct m0_bufvec     dr_bufvec;
	struct m0_bufvec     dr_auxbufvec;

	/** Represents attributes for pages from ::ti_dgvec. */
	enum page_attr      *dr_pageattrs;

	/** Backlink to parent target_ioreq structure. */
	struct target_ioreq *dr_tioreq;
};

/**
 * Cob create/trucate request fop along with the respective ast that gets posted
 * in respective call back. The call back does not do anything other than
 * posting the ast which then takes a lock over nw_xfer and conducts the
 * operation further.
 */
struct cc_req_fop {
	struct m0_fop        crf_fop;

	struct m0_sm_ast     crf_ast;

	struct target_ioreq *crf_tioreq;
};

/**
 * A request for a target can be of two types, either for read/write IO or for
 * cob creation for the target on remote ioservice.
 */
enum target_ioreq_type {
	TI_NONE,
	TI_READ_WRITE,
	TI_COB_CREATE,
	TI_COB_TRUNCATE,
};

/**
 * Collection of IO extents and buffers, directed towards particular
 * target objects (data_unit / parity_unit) in a parity group.
 * These structures are created by struct m0_clovis_op_io dividing the incoming
 * struct iovec into members of a parity group.
 */
struct target_ioreq {
	uint64_t                       ti_magic;

	/** Fid of component object. */
	struct m0_fid                  ti_fid;

	/** Target-object id. */
	uint64_t                       ti_obj;
	/**
	 * Time of launch for target IO request
	 * Used for ADDB posting.
	 */
	m0_time_t                      ti_start_time;

	/** Status code for io operation done for this target_ioreq. */
	int                            ti_rc;

	/** Number of parity bytes read/written for this target_ioreq. */
	uint64_t                       ti_parbytes;

	/** Number of file data bytes read/written for this object. */
	uint64_t                       ti_databytes;

	/** List of ioreq_fop structures issued on this target object. */
	struct m0_tl                   ti_iofops;
	/** Fop when the ti_req_type == TI_COB_CREATE|TI_COB_TRUNCATE. */
	struct cc_req_fop              ti_cc_fop;
	/** Resulting IO fops are sent on this rpc session. */
	struct m0_rpc_session         *ti_session;

	/** Linkage to link in to nw_xfer_request::nxr_tioreqs_hash table. */
	struct m0_hlink                ti_link;

	/**
	 * Index vector containing IO segments with cob offsets and
	 * their length.
	 * Each segment in this vector is worth PAGE_SIZE except
	 * the very last one.
	 */
	struct m0_indexvec             ti_ivec;

	/**
	 * Index vector containing IO segments with cob offset and
	 * their length which will be truncated.
	 */
	struct m0_indexvec             ti_trunc_ivec;

	/**
	 * Buffer vector corresponding to index vector above.
	 * This buffer is in sync with ::ti_ivec.
	 */
	struct m0_bufvec               ti_bufvec;
	struct m0_bufvec               ti_auxbufvec;

	/**
	 * Degraded mode read/write IO vector.
	 * This is intentionally kept as a pointer so that it
	 * won't consume memory when pool state is healthy.
	 */
	struct dgmode_rwvec           *ti_dgvec;

	/**
	 * Array of page attributes.
	 * Represents attributes for pages from ::ti_ivec and ::ti_bufvec.
	 */
	enum page_attr                *ti_pageattrs;

	/** target_ioreq operation vector. */
	const struct target_ioreq_ops *ti_ops;

	/** Backlink to parent structure nw_xfer_request. */
	struct nw_xfer_request        *ti_nwxfer;

	/** State of target device in the storage pool. */
	enum m0_pool_nd_state          ti_state;

	/** Whether cob create request for spare or read/write request. */
	enum target_ioreq_type         ti_req_type;
};

/**
 * Represents a wrapper over generic IO fop and its callback
 * to keep track of such IO fops issued by the same target_ioreq structure.
 *
 * When bottom halves for m0_sm_ast structures are run, it updates
 * target_ioreq::ti_rc and target_ioreq::ti_bytes with data from
 * IO reply fop.
 * Then it decrements nw_xfer_request::nxr_iofop_nr, number of IO fops.
 * When this count reaches zero, m0_clovis_op_io::ioo_oo::oo_sm changes its
 * state.
 */
struct ioreq_fop {
	uint64_t                     irf_magic;

	/** Status of IO reply fop. */
	int                          irf_reply_rc;

	/** In-memory handle for IO fop. */
	struct m0_io_fop             irf_iofop;

	/** Type of pages {PA_DATA, PA_PARITY} carried by io fop. */
	enum page_attr               irf_pattr;

	/** Callback per IO fop. */
	struct m0_sm_ast             irf_ast;

	/** Linkage to link in to target_ioreq::ti_iofops list. */
	struct m0_tlink              irf_link;

	/**
	 * Backlink to target_ioreq object where rc and number of bytes
	 * are updated.
	 */
	struct target_ioreq         *irf_tioreq;
};


#endif /* __MERO_CLOVIS_PG_H__ */

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
