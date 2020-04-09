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
 * Original author: Nikita Danilov  <nikita_danilov@seagate.com>
 * AuthorS:         Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                  James  Morse    <james.s.morse@seagate.com>
 *                  Sining Wu       <sining.wu@seagate.com>
 * Revision:        Pratik Shinde   <pratik.shinde@seagate.com>
 * Original creation date: 14-Oct-2013
 */

#pragma once

#ifndef __MERO_CLOVIS_IO_H__
#define __MERO_CLOVIS_IO_H__

#include "ioservice/fid_convert.h"
#include "clovis/clovis_internal.h"

/*
 * These three are used as macros since they are used as lvalues which is
 * not possible by using static inline functions.
 */
#define INDEX(ivec, i) ((ivec)->iv_index[(i)])

#define COUNT(ivec, i) ((ivec)->iv_vec.v_count[(i)])

#define SEG_NR(vec)    ((vec)->iv_vec.v_nr)

/** This is used to extract the offset within an otherwise aligned block */
#define SHIFT2MASK(x) ~((1ULL<<x) -1)

/* op_io's state configuration */
extern struct m0_sm_conf io_sm_conf;

extern const struct m0_clovis_op_io_ops ioo_ops;
extern const struct m0_clovis_op_io_ops ioo_oostore_ops;

/** Resource Manager group id, copied from m0t1fs */
extern const struct m0_uint128 m0_rm_clovis_group;

/* BOB types */
extern const struct m0_bob_type ioo_bobtype;
M0_BOB_DECLARE(M0_INTERNAL, m0_clovis_op_io);

extern struct m0_bob_type iofop_bobtype;
M0_BOB_DECLARE(M0_INTERNAL, ioreq_fop);

extern const struct m0_bob_type tioreq_bobtype;
M0_BOB_DECLARE(M0_INTERNAL, target_ioreq);

extern const struct m0_bob_type nwxfer_bobtype;
M0_BOB_DECLARE(M0_INTERNAL, nw_xfer_request);

extern const struct m0_bob_type pgiomap_bobtype;
M0_BOB_DECLARE(M0_INTERNAL, pargrp_iomap);

extern const struct m0_bob_type dtbuf_bobtype;
M0_BOB_DECLARE(M0_INTERNAL, data_buf);

/* iofops list */
M0_TL_DESCR_DECLARE(iofops, M0_EXTERN);
M0_TL_DECLARE(iofops, M0_INTERNAL, struct ioreq_fop);

M0_TL_DECLARE(rpcbulk, M0_INTERNAL, struct m0_rpc_bulk_buf);
M0_TL_DESCR_DECLARE(rpcbulk, M0_EXTERN);

/* tioreq hash table*/
M0_TL_DECLARE(tioreqht, M0_EXTERN, struct target_ioreq);
/* a hack to expose hash tl (should fix in hash.h)*/
M0_TL_DESCR_DECLARE(tioreqht, M0_EXTERN);
M0_HT_DESCR_DECLARE(tioreqht, M0_EXTERN);
M0_HT_DECLARE(tioreqht, M0_EXTERN, struct target_ioreq, uint64_t);

/**
 * Determines whether the provided pointer is aligned for use directly
 * by the network code.
 *
 * @param addr The address to check.
 * @return true/false whether the address is aligned.
 */
M0_INTERNAL bool addr_is_network_aligned(void *addr);

/**
 * Determines the IO buffer size of an object.
 *
 * @param obj The object.
 * @return The objects buffer size.
 */
M0_INTERNAL uint64_t obj_buffer_size(const struct m0_clovis_obj *obj);

/**
 * Determines the page size of the object to provided operation is on.
 *
 * @param ioo The operation on an object.
 * @return The objects page size.
 */
M0_INTERNAL uint64_t m0_clovis__page_size(const struct m0_clovis_op_io *ioo);

/**
 * Determines the number of object:blocks that are needed to hold the
 * specified amount of data.
 *
 * @param size The size of the larger buffer.
 * @param obj The object that will hold this data.
 * @return the number of blocks required.
 */
/** TODO: rename this block_count ? */
M0_INTERNAL uint64_t page_nr(m0_bcount_t size, struct m0_clovis_obj *obj);

/**
 * Retrieves the N (number of data servers), parameter from a parity layout.
 *
 * @param play The parity layout.
 * @return N
 */
M0_INTERNAL uint32_t layout_n(struct m0_pdclust_layout *play);

/**
 * Retrieves the K (number of spare/parity servers), parameter from a parity
 * layout.
 *
 * @param play The parity layout.
 * @return K
 */
M0_INTERNAL uint32_t layout_k(struct m0_pdclust_layout *play);

/**
 * Gets the block number that contains the specified offset.
 *
 * @param offset The offset.
 * @param obj The object.
 * @return the block number in the object.
 */
/** TODO: this code is the same as page_nr -> combine them? */
M0_INTERNAL uint64_t page_id(m0_bindex_t offset, struct m0_clovis_obj *obj);

/**
 * Gets the unit size of the parity calculations, from a parity layout.
 *
 * @param play The parity layout.
 * @return The unit size.
 */
M0_INTERNAL uint64_t layout_unit_size(struct m0_pdclust_layout *play);

/**
 * Determines the number of rows necessary to group the parity layout's
 * unit size, into a number of object:blocks.
 *
 * @param play The Parity layout.
 * @param obj The object.
 * @return The number of rows required.
 */
M0_INTERNAL uint32_t data_row_nr(struct m0_pdclust_layout *play,
				   struct m0_clovis_obj *obj);

/**
 * Determines the number of columns necessary for data in this parity layout.
 * This is the N parameter of the layout.
 *
 * @param play The Parity layout.
 * @return The number of columns required.
 */
M0_INTERNAL uint32_t data_col_nr(struct m0_pdclust_layout *play);

/**
 * Determines the number of columns necessary for parity in this parity layout.
 * This is the K parameter of the layout.
 *
 * @param play The Parity layout.
 * @return The number of columns required.
 */
M0_INTERNAL uint32_t parity_col_nr(struct m0_pdclust_layout *play);

/**
 * Determines the number of parity rows, this should always be the same
 * as the number of data_rows.
 *
 * @param play The Parity layout.
 * @param obj The object.
 * @return The number of rows required.
 */
/** TODO: This is clearly useless */
M0_INTERNAL uint32_t parity_row_nr(struct m0_pdclust_layout *play,
				     struct m0_clovis_obj *obj);

/**
 * Determines the size of data in a row of the parity group.
 *
 * @param play The Parity layout.
 * @return The amount of data this row will contain.
 */
M0_INTERNAL uint64_t data_size(struct m0_pdclust_layout *play);

/**
 * Retrieves a parity-declustered instance from the provided layout instance.
 *
 * @param li the layout instance.
 * @return The pdclust instance.
 */
M0_INTERNAL struct m0_pdclust_instance *
pdlayout_instance(struct m0_layout_instance *li);

/**
 * Gets the parity layout from  the provided IO operation.
 * This is heavily based on m0t1fs/linux_kernel/file.c::pdlayout_get
 *
 * @param ioo The IO Operation.
 * @return The parity layout.
 */
M0_INTERNAL struct m0_pdclust_layout *
pdlayout_get(const struct m0_clovis_op_io *ioo);

/**
 * Retrieves the stashed layout instance from the provided IO operation.
 *
 * @param ioo The IO operation.
 * @return The stashed layout instance.
 */
M0_INTERNAL struct m0_layout_instance *
layout_instance(const struct m0_clovis_op_io *ioo);

/**
 * Retrieves the math representation of parity
 *
 * @param ioo The IO operation
 * @return The parity math representation.
 */
M0_INTERNAL struct m0_parity_math *parity_math(struct m0_clovis_op_io *ioo);

/**
 * Calculates the offset in the target object based on the global offset.
 *
 * @param frame Frame number of target object.
 * @param play The Parity layout for the global object
 * @param gob_offset Offset in global object.
 * @return The target:object offset.
 */
M0_INTERNAL uint64_t target_offset(uint64_t                 frame,
				     struct m0_pdclust_layout *play,
				     m0_bindex_t               gob_offset);
/**
 * Calculates the group index, given a data index and the block size.
 *
 * @param index The data index.
 * @param dtsize The block size for this data.
 * @return the group index of the data index.
 */
M0_INTERNAL uint64_t group_id(m0_bindex_t index, m0_bcount_t dtsize);

/**
 * Calculates the offset of the last byte in the i'th extent.
 *
 * @param ivec The index of extents.
 * @param i Which extent to inspect.
 * @return The offset of the last byte.
 */
M0_INTERNAL m0_bcount_t seg_endpos(const struct m0_indexvec *ivec, uint32_t i);

/**
 * Counts the/ number of pages in a vector.
 *
 * @param vec The vector to inspect.
 * @param obj The object this corresponds with, used to find the block shift.
 * @return The number of pages.
 */
M0_INTERNAL uint64_t indexvec_page_nr(const struct m0_vec  *vec,
				      struct m0_clovis_obj *obj);

/**
 * Retrieves the number of pages in the provided map.
 *
 * @param map The map to inspect.
 * @return The number of pages.
 */
M0_INTERNAL uint64_t iomap_page_nr(const struct pargrp_iomap *map);

/**
 * Calculates the number of parity pages required for a given layout.
 *
 * @param play The parity layout.
 * @param obj The object this corresponds with, used to find the block shift.
 * @return The number of parity pages.
 */
M0_INTERNAL uint64_t parity_units_page_nr(struct m0_pdclust_layout *play,
					    struct m0_clovis_obj *obj);

#if !defined(round_down)
/**
 * Rounds val down to the nearest multiple of size.
 * This is heavily based on m0t1fs/linux_kernel/file.c::round_down
 *
 * @param val The value to round.
 * @param size The result should be a multiple of this size.
 * @return The rounded version of val.
 */
M0_INTERNAL uint64_t round_down(uint64_t val, uint64_t size);
#endif

#if !defined(round_up)
/**
 * Rounds val up to the nearest multiple of size.
 * This is heavily based on m0t1fs/linux_kernel/file.c::round_up
 *
 * @param val The value to round.
 * @param size The result should be a multiple of this size.
 * @return The rounded version of val.
 */
M0_INTERNAL uint64_t round_up(uint64_t val, uint64_t size);

#endif

/**
 * Calculates additional space for metadata in an io fop, use when adding
 * rpc bulk buffer and data buffers.
 * This is heavily based on m0t1fs/linux_kernel/file.c::io_desc_size
 *
 * @param ndom The network domain the request will be sent on/to/through.
 * @return The additional space requirement.
 */
M0_INTERNAL uint32_t io_desc_size(struct m0_net_domain *ndom);

/**
 * How much space is required for per-segment on-wire metadata.
 * This is heavily based on m0t1fs/linux_kernel/file.c::io_seg_size
 *
 * @return The number of bytes for per-segment on-wire metadata.
 */
M0_INTERNAL uint32_t io_seg_size(void);

/**
 * Calculates the data row/column for the provided index, using
 * the provided parity layout.
 * This is heavily based on m0t1fs/linux_kernel/file.c::page_pos_get
 *
 * @param map The io map that contains the row/columns.
 * @param index The object offset whose position we want.
 * @param grp_size data_size(play) * map->pi_grpid
 * @param[out] row The resulting row in the parity group.
 * @param[out] col The resulting column in the parity group.
 */
/** TODO: obj can be retrieved from map->pi_ioo */
M0_INTERNAL void page_pos_get(struct pargrp_iomap  *map,
			      m0_bindex_t           index,
			      m0_bindex_t           grp_size,
			      uint32_t             *row,
			      uint32_t             *col);

/*
 * Returns the starting offset of page given its position in data matrix.
 * Acts as opposite of page_pos_get() API.
 */
M0_INTERNAL m0_bindex_t data_page_offset_get(struct pargrp_iomap *map,
					       uint32_t             row,
					       uint32_t             col);

/**
 * Returns the state-machine:state of the provided io operation.
 * This is heavily based on m0t1fs/linux_kernel/file.c::ioreq_sm_state
 *
 * @param ioo The IO Operation.
 * @return the io sm state.
 */
M0_INTERNAL uint32_t ioreq_sm_state(const struct m0_clovis_op_io *ioo);

/**
 * @todo This code is not required once MERO-899 lands into master.
 * Tolerance for the given level.
 */
M0_INTERNAL uint64_t tolerance_of_level(struct m0_clovis_op_io *ioo,
					uint64_t lv);

/**
 * Gets resource manage domain.
 *
 * @param instance The clovis instance/cluster whose rm domain should be
 *                 returned.
 * @return A pointer to the resource manager domain.
 */
M0_INTERNAL struct m0_rm_domain *
clovis_rm_domain_get(struct m0_clovis *instance);

/**
 * Gets pool state machine from  m0_clovis_op_io.
 *
 * @param ioo The IO operation in question.
 * @return A pointer to the pool machine corresponding to the pool version of
 *         this operation acting on.
 */
M0_INTERNAL struct m0_poolmach*
clovis_ioo_to_poolmach(struct m0_clovis_op_io *ioo);

/**
 * Checks a nw_xfer_request struct is correct.
 *
 * @param xfer The nw_xxfer_request to check.
 * @return true or false.
 */
M0_INTERNAL bool nw_xfer_request_invariant(const struct nw_xfer_request *xfer);

/**
 * Finalises a target_ioreq, freeing any auxillary memory.
 * @param ti The target_ioreq to finalise.
 */
M0_INTERNAL void target_ioreq_fini(struct target_ioreq *ti);

/**
 * Retrieves the cob:fid on a target server for the provided
 * offset. (embedded in tgt).
 *
 * @param ioo The IO operation, contains the layout etc.
 * @param tgt The target parameters, contains the specified offset.
 * @return The cob:fid.
 */
M0_INTERNAL struct m0_fid target_fid(struct m0_clovis_op_io *ioo,
				     struct m0_pdclust_tgt_addr *tgt);

/**
 * Initialises a network transfer request.
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_request_init
 *
 * @param xfer[out] The network transfer request to initialise.
 */
M0_INTERNAL void nw_xfer_request_init(struct nw_xfer_request *xfer);

/**
 * Finalises a network transfer request.
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_request_fini
 *
 * @param xfer[out] The network transfer request to finalise.
 */
M0_INTERNAL void nw_xfer_request_fini(struct nw_xfer_request *xfer);

/**
 * Checks an ioreq_fop struct is correct.
 *
 * @param fop The ioreq_fop to check.
 * @return true or false.
 */
M0_INTERNAL bool ioreq_fop_invariant(const struct ioreq_fop *fop);

/**
 * Posts an io_fop, including updating it with descriptors for the bulk data
 * transfer.
 *
 * @param iofop The io_fop to sent.
 * @param session The target server to send the fop too.
 * @return The response of m0_rpc_post, 0 for success, or -errno.
 */
M0_INTERNAL int ioreq_fop_async_submit(struct m0_io_fop      *iofop,
				       struct m0_rpc_session *session);

/**
 * Initialises an IO fop.
 *
 * @param fop The io fop to initialise.
 * @param ti The target request that this io-fop corresponds to.
 * @param pattr Whether the payload is data or parity.
 * @return 0 for success, -errno otherwise.
 */
M0_INTERNAL int ioreq_fop_init(struct ioreq_fop    *fop,
				struct target_ioreq *ti,
				enum page_attr       pattr);
/**
 * Finalises an io fop.
 *
 * @param fop The fop to finalise.
 */
M0_INTERNAL void ioreq_fop_fini(struct ioreq_fop *fop);

/**
 * Initialises cob create fop. It's required for those targets that
 * are not part of io request, but host the members of at least one
 * parity group that's spanned by io request.
 */
M0_INTERNAL int ioreq_cc_fop_init(struct target_ioreq *ti);

/**
 * Finds out parity groups for which read IO failed and marks them as
 * DEGRADED.
 *
 * @param irfop The object IO fop in question.
 * @return 0 for success, -errno otherwise.
 */
M0_INTERNAL int ioreq_fop_dgmode_read(struct ioreq_fop *irfop);

/**
 * Sets the state-machine:state of the provided io operation.
 * This function should be called with the sm_grp lock held.
 *
 * @param ioo The IO Operation.
 * @param state The state to move the state machine into.
 */
M0_INTERNAL void ioreq_sm_state_set_locked(struct m0_clovis_op_io *ioo,
					   int state);

/**
 * Moves the state-machine into the failed state, with the provided rc.
 * This function should be called with the sm_grp lock held.
 *
 * @param ioo The IO Operation.
 * @param rc The rc to set in the state machine.
 */
M0_INTERNAL void ioreq_sm_failed_locked(struct m0_clovis_op_io *ioo, int rc);

/**
 * Checks a data_buf struct is correct.
 *
 * @param db The data_buf to check.
 * @return true or false.
 */
M0_INTERNAL bool data_buf_invariant(const struct data_buf *db);

/**
 * Calculates the parity group identifier this 'index' belongs to.
 *
 * @param index The data index.
 * @param ioo The IO operation whose iomaps should be checked.
 * @param ir_fop The io fop serving the data 'index'.
 * @return true or false.
 */
M0_INTERNAL uint64_t pargrp_id_find(m0_bindex_t                   index,
				    const struct m0_clovis_op_io *ioo,
				    const struct ioreq_fop       *ir_fop);

/**
 * Checks a pargrp_iomap struct is correct.
 *
 * @param map The pargrp_iomap to check.
 * @return true or false.
 */
M0_INTERNAL bool pargrp_iomap_invariant(const struct pargrp_iomap *map);

/**
 * Checks all the pagrp_iomaps pass the invariant check.
 *
 * @param ioo The IO operation whose iomaps should be checked.
 * @return true or false.
 */
M0_INTERNAL bool pargrp_iomap_invariant_nr(const struct m0_clovis_op_io *ioo);

/**
 * Initialises a parity group iomap. Populates the databuf and paritybuf arrays,
 * but doesn't allocate any data_bufs for them.
 *
 * @param map[out] The map to initialise.
 * @param ioo The IO operation responsible for this IO.
 * @param grpid Which group in the file the map should start at.
 * @return 0 for success, -errno otherwise.
 */
M0_INTERNAL int pargrp_iomap_init(struct pargrp_iomap    *map,
				  struct m0_clovis_op_io *ioo,
				  uint64_t                grpid);
/**
 * Finalises a parity group io map.
 *
 * @param map The map to finalise.
 * @param obj The object this map correpsonds with, used to find the block size.
 */
/** TODO: arguments here can be reduced, map+obj are in ioo */
M0_INTERNAL void pargrp_iomap_fini(struct pargrp_iomap *map,
				   struct m0_clovis_obj *obj);

M0_INTERNAL struct m0_clovis_obj_attr *
m0_clovis_io_attr(struct m0_clovis_op_io *ioo);
#endif /* __MERO_CLOVIS_IO_H__ */

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
