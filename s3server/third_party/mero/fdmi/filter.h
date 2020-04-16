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

#ifndef __MERO_FDMI_FDMI_FILTER_H__
#define __MERO_FDMI_FDMI_FILTER_H__

#include "lib/types.h"
#include "lib/buf.h"
#include "lib/buf_xc.h"
#include "xcode/xcode.h"
#include "xcode/xcode_attr.h"

/**
 * @defgroup FDMI_DLD_fspec_filter FDMI filter expression public API
 * @ingroup fdmi_main
 * @see @ref FDMI-DLD-fspec "FDMI Functional Specification"
 * @see @ref FDMI_DLD_fspec_filter_eval
 *
 *
 * @{
 *
 * In-memory representation of FDMI filter expression.
 *
 * FDMI filter expression has a tree structure. Tree nodes
 * have type @ref m0_fdmi_flt_node.
 *
 * There are three types of nodes:
 * @li @b constant @b operand (@ref m0_fdmi_flt_operand);
 * @li @b variable @b operand (@ref m0_fdmi_flt_var_node).
 *     Its value should be evaluated in run-time using the
 *     information contained in the tree node. Usually it is value of
 *     some field in FDMI record;
 * @li @b operation (@ref m0_fdmi_flt_node). Contains operation code and
 *     links to the operands for this operation.
 *
 * Every operand has a type (@ref m0_fdmi_flt_operand_type). Types of operands
 * should be checked during operation execution.
 *
 * No special functions for tree traversal are provided. User of the filter
 * expression should use @ref m0_fdmi_flt_op_node::opnds field for tree
 * traversal.
 *
 * @todo Insert example of usage here
 */

#define _QUOTE(s)  #s


/** @todo Phase 2: Recursive data definition is not allowed in xcode
 * operations. */

/**
 * Filter operation code
 *
 * Specifies what operation should be applied to operands.
 * There are two types of operations:
 * - common operations
 * - specific to some FDMI record types
 *
 * Common operations are implemented by FDMI filter evaluator
 * (@ref FDMI_DLD_fspec_filter_eval), implementation of specific operations
 * should be provided for FDMI filter evaluator using
 * @ref m0_fdmi_eval_add_op_cb.
 */
enum m0_fdmi_flt_op_code {
	/* Common operations */
	M0_FFO_OR,      /**< logical OR */
	M0_FFO_AND,     /**< logical AND */
	M0_FFO_NOT,     /**< logical NOT */
	M0_FFO_EQUAL,   /**< equality (==) */
	M0_FFO_GT,      /**< greater than (>) */
	M0_FFO_GTE,     /**< greater or equal (>=) */
	M0_FFO_LT,      /**< less then (<) */
	M0_FFO_LTE,     /**< less or equal (<=) */
	M0_FFO_COMMON_OPS_CNT,

	/* Operations specific for some FDMI record types
	 * can be added here */

	M0_FFO_TEST = M0_FFO_COMMON_OPS_CNT, /**< Special operation for UT */

	M0_FFO_TOTAL_OPS_CNT
};

/**
 * FDMI filter expression
 */
struct m0_fdmi_filter {
	struct m0_fdmi_flt_node    *ff_root; /**< Root of the expression tree */
};

/**
 * Possible FDMI filter operand types
 */
enum m0_fdmi_flt_operand_type {
	M0_FF_OPND_INT,          /**< 64-bit signed integer */
	M0_FF_OPND_UINT,         /**< 64-bit unsigned integer */
	M0_FF_OPND_STRING,       /**< string */
	M0_FF_OPND_BOOL,         /**< boolean */
	M0_FF_OPND_TYPES_CNT

	/* Operations specific for some FDMI record types
	 * can be added here */

	/* M0_FF_OPND_FOL_FRAG = M0_FF_OPND_TYPES_CNT,
	 * .... */
};

/**
 * Possible payload types for filter expression tree node
 *
 * @note It is confusing that operand has both operand type
 * and payload type. That is done for easier xcode representation
 * for @ref m0_fdmi_flt_operand.
 */
enum m0_fdmi_flt_opnd_pld_type {
	M0_FF_OPND_PLD_INT,      /**< 64-bit signed integer */
	M0_FF_OPND_PLD_UINT,     /**< 64-bit unsigned integer */
	M0_FF_OPND_PLD_BOOL,     /**< boolean */
	M0_FF_OPND_PLD_BUF,      /**< buffer with some data */
};

/**
 * Payload of filter operand node
 */
struct m0_fdmi_flt_opnd_pld {
	uint32_t fpl_type; /**< Payload type (@ref m0_fdmi_flt_opnd_pld_type) */

	union {
		int64_t  fpl_integer  M0_XCA_TAG(_QUOTE(M0_FF_OPND_PLD_INT));
		uint64_t fpl_uinteger M0_XCA_TAG(_QUOTE(M0_FF_OPND_PLD_UINT));
		bool     fpl_boolean  M0_XCA_TAG(_QUOTE(M0_FF_OPND_PLD_BOOL));

                /** Pointer to some data */
		struct m0_buf fpl_buf M0_XCA_TAG( _QUOTE(M0_FF_OPND_PLD_BUF));
	} fpl_pld;
} M0_XCA_UNION;

/**
 * FDMI filter operand
 */
struct m0_fdmi_flt_operand {
	/** Operand type (@ref m0_fdmi_flt_operand_type) */
	uint32_t                    ffo_type;
	/** Operand payload */
	struct m0_fdmi_flt_opnd_pld ffo_data;
} M0_XCA_RECORD;

/**
 * FDMI filter node with "variable" operand
 *
 * "Variable" operand doesn't contain constant value, that
 * can be directly used as operand in operation. "Variable" operand
 * contains information necessary to retrieve this operand instead.
 *
 * In case of FDMI records, "variable" node can describe something
 * like "get field foo from posted record"
 *
 * @note currently format of "variable" node is not defined
 */
struct m0_fdmi_flt_var_node {
	struct m0_buf ffvn_data; /**< Information to retrieve operand value */
} M0_XCA_RECORD;

enum {
	/** Maximum number of operands */
	FDMI_FLT_MAX_OPNDS_NR = 2
};

struct m0_fdmi_flt_node_ptr {
	/* FIXME: Actually type should be m0_fdmi_flt_node, but m0gccxml2xcode
	 * falls into endless loop in this case (Phase 2) */
	void *ffnp_ptr M0_XCA_OPAQUE("m0_fdmi_flt_node_xc_type");
} M0_XCA_RECORD;

/**
 * Operands array for operation node
 */
struct m0_fdmi_flt_op_node_opnds {
	int                           fno_cnt;   /**< operands count */
	struct m0_fdmi_flt_node_ptr  *fno_opnds; /**< operands */
} M0_XCA_SEQUENCE;

/**
 * FDMI filter node describing operation
 */
struct m0_fdmi_flt_op_node {
	/** operational code (@ref m0_fdmi_flt_op_code) */
	uint32_t                         ffon_op_code;
	struct m0_fdmi_flt_op_node_opnds ffon_opnds; /**< Operands */
} M0_XCA_RECORD;

/**
 * Type of the filter tree node
 */
enum m0_fdmi_flt_node_type {
	M0_FLT_VARIABLE_NODE  = 0,
	M0_FLT_OPERAND_NODE   = 1,
	M0_FLT_OPERATION_NODE = 2
};

/**
 * FDMI filter node
 */
struct m0_fdmi_flt_node {
	uint32_t ffn_type; /**< type (@ref m0_fdmi_flt_node_type) */

	union {
		struct m0_fdmi_flt_var_node  ffn_var
		    M0_XCA_TAG( _QUOTE(M0_FLT_VARIABLE_NODE)  );
		struct m0_fdmi_flt_operand   ffn_operand
		    M0_XCA_TAG( _QUOTE(M0_FLT_OPERAND_NODE)   );
		struct m0_fdmi_flt_op_node   ffn_oper
		    M0_XCA_TAG( _QUOTE(M0_FLT_OPERATION_NODE) );
	} ffn_u; /**< node payload */
} M0_XCA_UNION;

M0_INTERNAL int m0_fdmi_flt_node_xc_type(const struct m0_xcode_obj   *par,
					 const struct m0_xcode_type **out);

/**
 * Prints filter node into string (@ref m0_xcode_print is used).
 *
 * @param node  Filter node to be printed
 * @param out   Out parameter, pointer to memalloc'ed string with representation
 *              of node. Should be freed by the caller.
 * @return 0 if filter node is successfully printed to the string, @n
 *         error code otherwise
 */
M0_INTERNAL int m0_fdmi_flt_node_print(struct m0_fdmi_flt_node *node,
				       char                   **out);

/**
 * Fills filter node from provided string (@ref m0_xcode_read is used).
 *
 * @param str   String with textual x-code representation of the node
 * @param node  Filter node to fill
 * @return 0 if filter node is successfully read from the string, @n
 *         error code otherwise
 */

M0_INTERNAL int m0_fdmi_flt_node_parse(const char              *str,
				       struct m0_fdmi_flt_node *node);

/**
 * Fills boolean operand node with given value
 *
 * @param value boolean operand value
 * @param opnd  filter tree operand
 */
M0_INTERNAL void m0_fdmi_flt_bool_opnd_fill(
		struct m0_fdmi_flt_operand *opnd, bool value);

/**
 * Fills integer operand node with given value
 *
 * @param value  operand value
 * @param opnd   filter tree operand
 */
M0_INTERNAL void m0_fdmi_flt_int_opnd_fill(
		struct m0_fdmi_flt_operand *opnd, int64_t value);

/**
 * Fills unsigned integer operand node with given value
 *
 * @param value  operand value
 * @param opnd   filter tree operand
 */
M0_INTERNAL void m0_fdmi_flt_uint_opnd_fill(
		struct m0_fdmi_flt_operand *opnd, uint64_t value);

/**
 * Allocates new operation filter node and fills it with provided values
 *
 * @param op_code  operational code
 * @param left     left operand
 * @param right    right operand
 *
 * @return         pointer to the allocated node
 */
struct m0_fdmi_flt_node *m0_fdmi_flt_op_node_create(
		enum m0_fdmi_flt_op_code  op_code,
		struct m0_fdmi_flt_node  *left,
		struct m0_fdmi_flt_node  *right);

/**
 * Allocates new filter "variable" node and fills its payload
 * m0_buf structure should be freed by caller, but not payload
 * of buffer
 *
 * @param data  "variable" node payload
 *
 * @return      pointer to the allocated node
 */
struct m0_fdmi_flt_node *m0_fdmi_flt_var_node_create(struct m0_buf *data);

/**
 * Allocates new filter node with boolean operand
 *
 * @param value boolean operand value
 * @return    pointer to the allocated node
 */
struct m0_fdmi_flt_node *m0_fdmi_flt_bool_node_create(bool value);

/**
 * Allocates new filter node with int operand
 *
 * @param value int operand value
 * @return    pointer to the allocated node
 */
struct m0_fdmi_flt_node *m0_fdmi_flt_int_node_create(int64_t value);

/**
 * Allocates new filter node with unsigned int operand
 *
 * @param value unsigned int operand value
 * @return    pointer to the allocated node
 */
struct m0_fdmi_flt_node *m0_fdmi_flt_uint_node_create(uint64_t value);

/**
 * Initialize FDMI filter expression
 *
 * @param flt   Filter expression to be initialized
 */
M0_INTERNAL void m0_fdmi_filter_init(struct m0_fdmi_filter *flt);

/**
 * Finalize FDMI filter expression
 *
 * It also deallocates all the memory occupied by filter tree nodes.
 *
 * @param flt Filter expression to be finalized
 */
M0_INTERNAL void m0_fdmi_filter_fini(struct m0_fdmi_filter *flt);

/**
 * Set root node of the filter
 *
 * @param flt   Filter expression to be initialized
 * @param root  Root of the tree
 */
M0_INTERNAL void m0_fdmi_filter_root_set(struct m0_fdmi_filter    *flt,
                                         struct m0_fdmi_flt_node  *root);

/** @} end of FDMI_DLD_fspec_filter */

#endif /* __MERO_FDMI_FDMI_FILTER_H__ */

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
