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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 * Original creation date: 10/19/2010
 * Revision       : Anup Barve <Anup_Barve@xyratex.com>
 * Revision date  : 06/14/2012
 */

#pragma once

#ifndef __MERO_SNS_PARITY_MATH_H__
#define __MERO_SNS_PARITY_MATH_H__

#include "lib/vec.h"
#include "lib/bitmap.h"
#include "lib/tlist.h"
#include "matvec.h"
#include "ls_solve.h"

/**
   @defgroup parity_math Parity Math Component

   A parity math component is a part of Mero core and serving
   several purposes:
   @li Provide algorithms for calculation of SNS parity units (checksums)
       for given data units;
   @li Provide algorithms for quick update of parity units in case of minor
       data changes;
   @li Provide algorithms for SNS repair (recovery) in case of failure.
   @{
*/

/**
 * Parity calculation type indicating various algorithms of parity calculation.
 */
enum m0_parity_cal_algo {
        M0_PARITY_CAL_ALGO_XOR,
        M0_PARITY_CAL_ALGO_REED_SOLOMON,
	M0_PARITY_CAL_ALGO_NR
};

enum m0_sns_ir_block_status {
	M0_SI_BLOCK_ALIVE,
	M0_SI_BLOCK_FAILED,
};

enum m0_sns_ir_block_type {
	M0_SI_BLOCK_LOCAL,
	M0_SI_BLOCK_REMOTE,
};

enum m0_parity_linsys_algo {
	M0_LA_GAUSSIAN,
	M0_LA_INVERSE,
};
/**
 * Every member of a parity-group is called as a block. During incremental
 * recovery m0_sns_ir_block holds that information associated with a block
 * which is relevant for recovery. This involves block-index
 * within a parity-group, a pointer to information held by the block, a pointer
 * to recovery-matrix associated with the block and so on.
 */
struct m0_sns_ir_block {
	/* Index of a block within parity group. */
	uint32_t		     sib_idx;
	/* Data from a block is submitted to incremental-recovery-module using
	 * struct m0_bufvec. sib_addr holds address for the same.
	 */
	struct m0_bufvec            *sib_addr;
	/* Whenever a block fails, sib_bitmap holds block indices of
	 * blocks required for its recovery.
	 */
	struct m0_bitmap	     sib_bitmap;
	/* Column associated with the block within
	 * m0_parity_math::pmi_data_recovery_mat. This field is meaningful
	 * when status of a block is M0_SI_BLOCK_ALIVE.
	 */
	uint32_t		     sib_data_recov_mat_col;
	/* Row associated with the block within its own recovery matrix.
	 * The field is meaningful when status of a block is M0_SI_BLOCK_FAILED.
	 */
	uint32_t		     sib_recov_mat_row;
	/* Indicates whether a block is available, failed or restored. */
	enum m0_sns_ir_block_status  sib_status;
};

/**
   Holds information about system configuration i.e., data and parity units
   data blocks and failure flags.
 */
struct m0_parity_math {
	enum m0_parity_cal_algo	     pmi_parity_algo;

	uint32_t		     pmi_data_count;
	uint32_t		     pmi_parity_count;
	/* structures used for parity calculation and recovery */
	struct m0_matvec	     pmi_data;
	struct m0_matvec	     pmi_parity;
	/* Vandermonde matrix */
	struct m0_matrix	     pmi_vandmat;
	/* Submatrix of Vandermonde matrix used to compute parity. */
	struct m0_matrix	     pmi_vandmat_parity_slice;
	/* structures used for non-incremental recovery */
	struct m0_matrix	     pmi_sys_mat;
	struct m0_matvec	     pmi_sys_vec;
	struct m0_matvec	     pmi_sys_res;
	struct m0_linsys	     pmi_sys;
	/* Data recovery matrix that's inverse of pmi_sys_mat. */
	struct m0_matrix             pmi_recov_mat;
};

/* Holds information essential for incremental recovery. */
struct m0_sns_ir {
	uint32_t		si_data_nr;
	uint32_t		si_parity_nr;
	uint32_t		si_failed_data_nr;
	uint32_t		si_alive_nr;
	/* Number of blocks from a parity group that are available locally
	 * on a node. */
	uint32_t		si_local_nr;
	/* Array holding all blocks */
	struct m0_sns_ir_block *si_blocks;
	/* Vandermonde matrix used during RS encoding */
	struct m0_matrix	si_vandmat;
	/* Recovery matrix for failed data blocks */
	struct m0_matrix	si_data_recovery_mat;
	/* Recovery matrix for failed parity blocks. This is same as
	 * math::pmi_vandmat_parity_slice.
	 */
	struct m0_matrix	si_parity_recovery_mat;
};

/**
   Initialization of parity math algorithms.
   Fills '*math' with appropriate values.
   @param data_count - count of SNS data units used in system.
   @param parity_count - count of SNS parity units used in system.
 */
M0_INTERNAL int m0_parity_math_init(struct m0_parity_math *math,
				    uint32_t data_count, uint32_t parity_count);

/**
   Deinitializaton of parity math algorithms.
   Frees all memory blocks allocated by m0_parity_math_init().
 */
M0_INTERNAL void m0_parity_math_fini(struct m0_parity_math *math);

/**
   Calculates parity block data.
   @param[in]  data - data block, treated as uint8_t block with b_nob elements.
   @param[out] parity - parity block, treated as uint8_t block with
                        b_nob elements.
   @pre m0_parity_math_init() succeeded.
 */
M0_INTERNAL void m0_parity_math_calculate(struct m0_parity_math *math,
					  struct m0_buf *data,
					  struct m0_buf *parity);

/**
 * Calculates parity in a differential manner.
 * @pre math != NULL && old_ver != NULL && new_ver != NULL && parity != NULL &&
 *      index < math->pmi_parity_count
 * @param old_ver Old version of data block.
 * @param new_ver New version of data block.
 * @param parity  Parity block.
 * @param index   Index of data unit in parity group for which old and new
 * versions are sent.
 */
M0_INTERNAL void m0_parity_math_diff(struct m0_parity_math *math,
				     struct m0_buf *old_ver,
				     struct m0_buf *new_ver,
				     struct m0_buf *parity, uint32_t index);

/**
   Parity block refinement iff one data word of one data unit had changed.
   @param data[in] - data block, treated as uint8_t block with b_nob elements.
   @param parity[out] - parity block, treated as uint8_t block with
                        b_nob elements.
   @param data_ind_changed[in] - index of data unit recently changed.
   @pre m0_parity_math_init() succeeded.
 */
M0_INTERNAL void m0_parity_math_refine(struct m0_parity_math *math,
				       struct m0_buf *data,
				       struct m0_buf *parity,
				       uint32_t data_ind_changed);


M0_INTERNAL int m0_parity_recov_mat_gen(struct m0_parity_math *math,
					uint8_t *fail);


M0_INTERNAL void m0_parity_recov_mat_destroy(struct m0_parity_math *math);

/**
   Recovers data units' data words from single or multiple errors.
   If parity also needs to be recovered, user of the function needs
   to place a separate call for m0_parity_math_calculate().
   @param data[inout] - data block, treated as uint8_t block with
			b_nob elements.
   @param parity[inout] - parity block, treated as uint8_t block with
                          b_nob elements.
   @param fail[in] - block with flags, treated as uint8_t block with
                     b_nob elements, if element is '1' then data or parity
                     block with given index is treated as broken.
   @param algo[in] - algorithm for recovery of data in case reed solomon
                     encoding is used.
   @pre m0_parity_math_init() succeded.
 */
M0_INTERNAL void m0_parity_math_recover(struct m0_parity_math *math,
					struct m0_buf *data,
					struct m0_buf *parity,
					struct m0_buf *fail,
					enum m0_parity_linsys_algo algo);

/**
 * Recovers data or parity units partially or fully depending on the parity
 * calculation algorithm, given the failure index.
 * @param math - math context.
 * @param data - data block, treated as uint8_t block with b_nob elements.
 * @param parity - parity block, treated as uint8_t block with b_nob elements.
 * @param failure_index - Index of the failed block.
   @pre m0_parity_math_init() succeded.
 */
M0_INTERNAL void m0_parity_math_fail_index_recover(struct m0_parity_math *math,
						   struct m0_buf *data,
						   struct m0_buf *parity,
						   const uint32_t
						   failure_index);

/**
 * XORs the source and destination buffers and stores the output in destination
 * buffer.
 * @param dest - destination buffer, treated as uint8_t block with
 *               b_nob elements, containing the output of src XOR dest.
 * @param src - source buffer, treated as uint8_t block with b_nob elements.
 */
M0_INTERNAL void m0_parity_math_buffer_xor(struct m0_buf *dest,
					   const struct m0_buf *src);

/**
 * @defgroup incremental_recovery Incremental recovery APIs
 * @{
 *  @section incremental_recovery-highlights Highlights
 *  - Algorithm is distributed.
 *  - Algorithm is not symmetric for data and parity blocks.
 *  - Vandermonde matrix is used as an encoding matrix for Reed-Solomon
 *    encoding.
 *  @section incremental_recovery-discussion Discussion
 *  After encountering a failure, SNS client initializes the incremental
 *  recovery module by calling m0_sns_ir_init(). Followed by that it reports
 *  failed blocks with m0_sns_ir_failure_register().
 *  Once all failed blocks are registered, a call to m0_sns_ir_mat_compute()
 *  is placed. During this initialization the recovery module constructs
 *  a recovery matrix associated with failed blocks.
 *
 *  Once initialization is over, client passes available blocks to recovery
 *  module using the function m0_sns_ir_recover(). This function uses the fact
 *  that a lost block can be represented as a linear combination of alive
 *  blocks.  It keeps accumulating data from input alive blocks, as SNS client
 *  passes it.  Various parameters to m0_sns_ir_recover() tell the recovery
 *  routine how to use an incoming alive block for recovery. We will see how
 *  m0_sns_ir_recover() works using a simple example. Suppose we have four
 *  blocks in a parity group, b0 to b3, of which two have failed
 *  (say b0 and b1). Assume that these blocks satisfy following equations:
 *
 *  @code
 *    b0 = u0.b2 + u1.b3
 *    b1 = u2.b2 + u3.b3
 *  @endcode
 *  Note that each u_{i} is a  constant coefficient, which gets computed during
 *  m0_sns_ir_compute_mat(). When block b2 becomes available to SNS client, it
 *  submits it using m0_sns_ir_recover(). This routine then updates in-memory
 *  copies of b0 and b1 as below:
 *  @code
 *    b0 = u0.b2
 *    b1 = u2.b2
 *  @endcode
 *
 *  Next, when block b3 gets submitted, m0_sns_ir_recover() adds its
 *  contribution cumulatively.
 *
 *  @code
 *  b0 += u1.b3
 *  b1 += u3.b3
 *  @endcode
 *
 *  We call this mode of recovery as incremental recovery, where failed blocks
 *  get recovered incrementally for each input alive block.
 *  In all, there are three use-cases as below
 *	 -# All failed blocks are data blocks.
 *	 -# All failed blocks are parity blocks.
 *	 -# Failed blocks are mixture of data and parity blocks.
 *
 *  It is worth noting that recovery of a lost parity block requires all data
 *  blocks. Hence in the third case above, we first require to recover all lost
 *  data blocks and then use recovered data blocks for restoring lost parity
 *  blocks. SNS client being unaware of dependencies between lost and
 *  available blocks, submits all available blocks to recovery module. When
 *  contribution by all available blocks is received at a destination node for
 *  a lost block, then SNS client marks recovery of that lost block as
 *  complete.
 *
 *  This algorithm works in three phases listed below:
 *  -# Phase1: transform alive blocks present locally on each node.
 *  -# Phase2: transform partially recovered data-blocks (from phase1 above)
 *             for failed parity-blocks.
 *  -# Phase3: add partially recovered blocks from last two phases at
 *             nodes where respective spare resides.
 *  In the first phase each node updates in-memory buffers for lost-blocks
 *  using in-memory buffers for alive blocks, local to the node. If the set of
 *  lost blocks consists of both data as well as parity blocks, then in the
 *  second phase, contribution of the partially recovered data blocks towards
 *  lost parity blocks gets computed. SNS client achieves this by calling
 *  m0_sns_ir_local_xform(). At this stage, if we add in-memory buffers of a
 *  lost block residing over all the nodes, then we would get a recovered
 *  block. In order to achieve this, each node sends an in-memory buffer of a
 *  lost block, to a node at which the spare for that block resides. Node at
 *  the receiving end then passes incoming blocks to m0_sns_ir_recover().
 *  This completes the third phase. Nodes that do not have any spare block,
 *  do not have any computations in the last phase. Next section illustrates
 *  entire recovery process using an example.
 *
 *  @section incremental_recovery-use-case Use-case
 *  Consider a case of six data blocks, two parity blocks, and two spare
 *  blocks, distributed over four nodes. Assume following mapping between
 *  data/parity/spare blocks and nodes. di represents the i^th data block,
 *  pi represents the i^th parity block, si represents i^th spare block,
 *  and Ni represents the i^th node.
 *
    @verbatim
			 ________________________
			*			 *
			*  d0, p0, s0     --> N0 *
			*  d2, p1         --> N1 *
			*  d1, d3, d5     --> N2 *
			*  d4, s1         --> N3 *
			*________________________*
    @endverbatim
 *
 * Assume that blocks d2 and p1 are lost. Let us assume that d2 gets restored
 * at s0 and that of p1 gets restored at s1. Assume that d2 and p1 obey
 * following equations:
 * @code
 * d2 = (w0.d0 + w5.p0)           + (w1.d1 + w2.d3 + w4.d5) + (w3.d4)
 * p1 = (v0.d0)         + (v2.d2) + (v1.d1 + v3.d3 + v5.d5) + (v4.d4)
 * @endcode
 * In the equations above, vi and wi are constants, for each i. Note
 * that we have clubbed those terms which can be evaluated at a same node.
 * As discussed earlier, recovery of failed blocks is carried out in three
 * phases. Following table illustrates all three phases in the context of our
 * example. In the following table, we represent an in-memory buffer of a lost
 * block di, on node Nj as di_Nj.
 * @verbatim
	Configuration:
	 N = 6, K = 2
	 Lost blocks: d2 and p1
          _____________________________________________________________________
         |Phase 1                      |Phase 2          |Phase 3		|
   ______|_____________________________|_________________|______________________|
   |  N0 |d2_N0 = w0.d0 + w5.p0        |p1_N0 += v2.d2_N0|send p1_N0 to N3      |
   |     |p1_N0 = v0.d0                |                 |d2_N0 += d2_N2 + d2_N3|
   |     |                             |                 |s0 = d2_N0            |
   |_____|_____________________________|_________________|______________________|
   |  N1 |			       |		 |		        |
   |     |			       |                 |		        |
   |_____|_____________________________|_________________|______________________|
   |  N2 |d2_N2 = w1.d1 + w2.d3 + w4.d5|p1_N2 += v2.d2_N2|send d2_N2 to N0      |
   |     |p1_N2 = v1.d1 + v3.d3 + v5.d5|                 |send p1_N2 to N3      |
   |_____|_____________________________|_________________|______________________|
   |  N3 |d2_N3 = w3.d4                |p1_N3 += v2.d2_N3|send d2_N3 to N0      |
   |     |p1_N3 = v4.d4                |                 |p1_N3 += p1_N0 + p1_N2|
   |     |                             |                 |s1 = p1_N3            |
   |_____|_____________________________|_________________|______________________|
 * @endverbatim
 * We will now see how incremental-recovery APIs enable execution of various
 * phases described above.
 * Following code will show sequence of APIs that are executed at the node N0.
 *
 * @code
 * struct m0_sns_ir	 ir;
 * struct m0_parity_math math;
 * :
 * :
 * ret = m0_sns_ir_init(&math, &ir);
 * if (ret != 0)
 *	 goto handle_error;
 * // Register the failure of block d2 and provide in-memory location for its
 * // recovery.
 * ret = m0_sns_ir_failure_register(d2_N0, index_of(d2), &ir);
 * if (ret != 0)
 *	goto handle_error;
 * // Register the failure of block p1 and provide in-memory location for its
 * // recovery.
 * ret = m0_sns_ir_failure_register(p1_N0, index_of(p1), &ir);
 * if (ret != 0)
 *	goto handle_error;
 * // Construct recovery matrix for lost blocks.
 * ret = m0_sns_ir_mat_compute(&ir);
 * if (ret != 0)
 *	goto handle_error;
 * @endcode
 *
 * Once initialization is over, nodes start the compute phases described
 * earlier. Following code illustrates this for the node N0.
 * @code
 * // Phase 1
 * // Incrementally recovering d2 and p1 using d0.
 * m0_bitmap_set(bitmap, index_of(d0), true);
 * m0_sns_ir_recover(&ir, d0_N0, bitmap_d0, do_not_care);
 * // SNS client updates bitmaps associated with accumulator buffers for d2 and
 * // p1.
 * m0_bitmap_set(bitmap_d2, index_of(d0), true);
 * m0_bitmap_set(bitmap_p1, index_of(d0), true);
 * //Unset a bit from the bitmap that represents d0.
 * m0_bitmap_set(bitmap, index_of(d0), false);
 * // Incrementally recovering d2 and p1 using p0.
 * m0_bitmap_set(bitmap, index_of(p0), true);
 * m0_sns_ir_recover(&ir, p0_N0, bitmap_p0, do_not_care);
 * // SNS client updates bitmap associated with accumulator buffers for d2 and
 * // p1.
 * m0_bitmap_set(bitmap_d2, index_of(p0), true);
 * m0_bitmap_set(bitmap_p1, index_of(p0), true);
 * @endcode
 *
 * Once SNS client runs out of local blocks at N0, Phase 1 gets over. Second
 * phase gets executed using m0_sns_ir_local_xform(), as shown below:
 * @code
 * //Phase 2
 * m0_sns_ir_local_xform(&ir);
 * @endcode
 * Note that whenever a set of failed blocks does not contain both data and
 * parity blocks, the only operation m0_sns_ir_local_xform() performs is
 * changing the phase associated with recovery routine from M0_SI_BARE to
 * M0_SI_XFORM.
 * In the last phase, a node sends an in-memory buffer for a lost block to a
 * node where respective spare unit resides. Receiving node passes incoming
 * buffers to recovery routine.
 *
 * @code
 * //Phase 3 at N0
 * send_block_to_spare(p1_N0, N3);
 *
 * for (i = 2; i < 4; ++i) {
 *	m0_sns_ir_recover(&ir, d2_Ni, bitmap_d2_Ni, index_of(d0));
 * }
 * @endcode
 *
 * With every buffer that SNS client submits to m0_sns_ir_recover(), a bitmap
 * is submitted. Based upon indices of bits set in the bitmap, recovery module
 * figures out the members from a parity group that have contributed to the
 * incoming buffer. Hence, when a local block d0 gets submitted at node N0, a
 * bitmap accompanying has only that bit set which represents d0. On the
 * other hand, during third phase, a bitmap that accompanies d2_Ni has those
 * bits set which represent blocks local to node Ni.
 * When recovery gets over, node places a call to m0_sns_ir_fini().
 * @code
 * m0_sns_ir_fini(&ir);
 * @endcode
 **/

/**
 * Marks a failed block and populates the m0_parity_math_block structure
 * associated with the failed block accordingly.
 * @pre  block_idx < total_blocks
 * @pre  total failures are less than maximum supported failures.
 * @param[in] recov_addr    address at which failed block is to be restored.
 * @param[in] failed_index  index of the failed block in a parity group.
 * @param     ir	    holds recovery_matrix and other data for recovery.
 * @retval    0		    on success.
 * @retval    -EDQUOT       recovery is not possible, as number failed units
 *			    exceed number of parity units.
 */
M0_INTERNAL int m0_sns_ir_failure_register(struct m0_bufvec *recov_addr,
					   uint32_t failed_index,
					   struct m0_sns_ir *ir);

/**
 * Populates the structure m0_sns_ir with fields relevant for recovery.
 * @param[in]  math
 * @param[in]  local_nr Number of blocks from a parity group, that are available
 *			locally on a node.
 * @param[out] ir
 * @retval     0       on success
 * @retval     -ENOMEM when it fails to allocate array of m0_sns_ir_block or
 *		       when initialization of bitmap fails.
 */
M0_INTERNAL int m0_sns_ir_init(const struct m0_parity_math *math,
			       uint32_t local_nr, struct m0_sns_ir *ir);
/**
 * Computes data-recovery matrix. Populates dependency bitmaps for failed
 * blocks.
 * @retval     0      on success.
 * @retval    -ENOMEM on failure to acquire memory.
 * @retval    -EDOM   when input matrix is singular.
 */
M0_INTERNAL int m0_sns_ir_mat_compute(struct m0_sns_ir *ir);

/**
 * Computes the lost data incrementally using an incoming block.
 * @pre ergo(m0_bitmap_set_nr >  1, failed_index < total_blocks)
 * @param     ir	    holds recovery matrix, and destination address for
 *			    a failed block(s) to be recovered.
 * @param[in] bufvec        input bufvec to be used for recovery. This can be
 *			    one of the available data/parity blocks or
 *			    a linear combination of a subset of them.
 * @param[in] bitmap        indicates which of the available data/parity
 *			    blocks have contributed towards the input
 *			    bufvec.
 * @param[in] failed_index  index of a failed block for which the input bufvec
 *			    is to be used.
 * @param[in] block_type    indicates whether incoming block is placed locally
 *			    on a node or coming from a remote node. All remote
 *			    blocks are assumed to be transformed.
 */
M0_INTERNAL void m0_sns_ir_recover(struct m0_sns_ir *ir,
				   struct m0_bufvec *bufvec,
				   const struct m0_bitmap *bitmap,
				   uint32_t failed_indexi,
				   enum m0_sns_ir_block_type block_type);
/**
 * When failures include both data and parity blocks, this function uses
 * local copy of recovered (partially or fully) data-block for recovering
 * failed parity block. On any node, this function can be triggered only once
 * per parity group. Subsequent triggers will result into a no-operaiton.
 * @param ir holds information relevant for recovery
 */
//M0_INTERNAL void m0_sns_ir_local_xform(struct m0_sns_ir *ir);
M0_INTERNAL void m0_sns_ir_fini(struct m0_sns_ir *ir);
/** @} end group Incremental recovery APIs */

/** @} end group parity_math */

/* __MERO_SNS_PARITY_MATH_H__  */
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
