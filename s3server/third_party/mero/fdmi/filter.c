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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/finject.h"
#include "fdmi/filter.h"
#include "fdmi/filter_xc.h"

M0_INTERNAL int m0_fdmi_flt_node_xc_type(const struct m0_xcode_obj   *par,
				         const struct m0_xcode_type **out)
{
	*out = m0_fdmi_flt_node_xc;
	return 0;
}

M0_INTERNAL void m0_fdmi_filter_init(struct m0_fdmi_filter    *flt)
{
	M0_ENTRY();
	M0_PRE(flt != NULL);

	flt->ff_root = NULL;

	M0_LEAVE();
}

M0_INTERNAL void m0_fdmi_filter_root_set(struct m0_fdmi_filter    *flt,
			                 struct m0_fdmi_flt_node  *root)
{
	M0_ENTRY();
	M0_PRE(flt != NULL);
	M0_PRE(root != NULL);
	flt->ff_root = root;
	M0_LEAVE();
}

static void free_operand_node(struct m0_fdmi_flt_node *node)
{
	struct m0_fdmi_flt_operand  *opnd = &node->ffn_u.ffn_operand;

	M0_ENTRY("node %p", node);

	if (opnd->ffo_data.fpl_type == M0_FF_OPND_PLD_BUF) {
		m0_buf_free(&opnd->ffo_data.fpl_pld.fpl_buf);
	}

	m0_free(node);
	M0_LEAVE();
}

static void free_variable_node(struct m0_fdmi_flt_node *node)
{
	struct m0_fdmi_flt_var_node  *var_node = &node->ffn_u.ffn_var;

	M0_ENTRY("node %p", node);
	m0_buf_free(&var_node->ffvn_data);
	m0_free(node);
	M0_LEAVE();
}

static void free_flt_node(struct m0_fdmi_flt_node *node)
{
	int i;

	M0_PRE(node != NULL);

	switch (node->ffn_type) {
	case M0_FLT_OPERATION_NODE:
		for (i = 0; i < node->ffn_u.ffn_oper.ffon_opnds.fno_cnt; i ++) {
			free_flt_node((struct m0_fdmi_flt_node *)
				      node->ffn_u.ffn_oper.ffon_opnds.
				      fno_opnds[i].ffnp_ptr);
		}
		m0_free(node->ffn_u.ffn_oper.ffon_opnds.fno_opnds);
		m0_free(node);
		break;

	case M0_FLT_OPERAND_NODE:
		free_operand_node(node);
		break;

	case M0_FLT_VARIABLE_NODE:
		free_variable_node(node);
		break;

	default:
		M0_ASSERT(false);
	}
}

M0_INTERNAL void m0_fdmi_filter_fini(struct m0_fdmi_filter *flt)
{
	M0_ENTRY("flt=%p", flt);

	if (flt->ff_root != NULL)
		free_flt_node(flt->ff_root);

	M0_LEAVE();
}

M0_INTERNAL void m0_fdmi_flt_bool_opnd_fill(
		struct m0_fdmi_flt_operand *opnd, bool value)
{
	M0_ENTRY("opnd %p, value %d", opnd, value);
	M0_PRE(opnd != NULL);
	opnd->ffo_type = M0_FF_OPND_BOOL;
	opnd->ffo_data.fpl_type = M0_FF_OPND_PLD_BOOL;
	opnd->ffo_data.fpl_pld.fpl_boolean = value;
	M0_LEAVE();
}

struct m0_fdmi_flt_node *m0_fdmi_flt_bool_node_create(bool value)
{
	struct m0_fdmi_flt_node *ret;

	M0_ENTRY("value=%d", value);

	M0_ALLOC_PTR(ret);

	if (ret != NULL) {
		ret->ffn_type = M0_FLT_OPERAND_NODE;
		m0_fdmi_flt_bool_opnd_fill(&ret->ffn_u.ffn_operand, value);
	}

	M0_LEAVE("ret=%p", ret);
	return ret;
}

M0_INTERNAL void m0_fdmi_flt_int_opnd_fill(
		struct m0_fdmi_flt_operand *opnd, int64_t value)
{
	M0_ENTRY("opnd %p, value %"PRId64, opnd, value);
	M0_PRE(opnd != NULL);
	opnd->ffo_type = M0_FF_OPND_INT;
	opnd->ffo_data.fpl_type = M0_FF_OPND_PLD_INT;
	opnd->ffo_data.fpl_pld.fpl_integer = value;
	M0_LEAVE();
}

struct m0_fdmi_flt_node *m0_fdmi_flt_int_node_create(int64_t value)
{
	struct m0_fdmi_flt_node *ret;

	M0_ENTRY("value=%"PRId64, value);

	M0_ALLOC_PTR(ret);

	if (ret != NULL) {
		ret->ffn_type = M0_FLT_OPERAND_NODE;
		m0_fdmi_flt_int_opnd_fill(&ret->ffn_u.ffn_operand, value);
	}

	M0_LEAVE("ret=%p", ret);
	return ret;
}

M0_INTERNAL void m0_fdmi_flt_uint_opnd_fill(
		struct m0_fdmi_flt_operand *opnd, uint64_t value)
{
	M0_ENTRY("opnd %p, value %"PRIu64, opnd, value);
	M0_PRE(opnd != NULL);
	opnd->ffo_type = M0_FF_OPND_UINT;
	opnd->ffo_data.fpl_type = M0_FF_OPND_PLD_UINT;
	opnd->ffo_data.fpl_pld.fpl_uinteger = value;
	M0_LEAVE();
}

struct m0_fdmi_flt_node *m0_fdmi_flt_uint_node_create(uint64_t value)
{
	struct m0_fdmi_flt_node *ret;

	M0_ENTRY("value=%"PRIu64, value);

	M0_ALLOC_PTR(ret);

	if (ret != NULL) {
		ret->ffn_type = M0_FLT_OPERAND_NODE;
		m0_fdmi_flt_uint_opnd_fill(&ret->ffn_u.ffn_operand, value);
	}

	M0_LEAVE("ret=%p", ret);
	return ret;
}

struct m0_fdmi_flt_node *m0_fdmi_flt_var_node_create(struct m0_buf *data)
{
	struct m0_fdmi_flt_node *ret;

	M0_ENTRY("data="BUF_F, BUF_P(data));

	M0_ALLOC_PTR(ret);

	if (ret != NULL) {
		ret->ffn_type = M0_FLT_VARIABLE_NODE;
		ret->ffn_u.ffn_var.ffvn_data = *data;
	}

	M0_LEAVE("ret=%p", ret);
	return ret;
}

struct
m0_fdmi_flt_node *m0_fdmi_flt_op_node_create(enum m0_fdmi_flt_op_code  op_code,
					     struct m0_fdmi_flt_node  *left,
					     struct m0_fdmi_flt_node  *right)
{
	struct m0_fdmi_flt_node *ret;

	M0_ENTRY("op_code=%d", op_code);

	/**
	 * @todo Per-operation code check for number of operands and its types
	 *       (phase 2)*/
	M0_ALLOC_PTR(ret);
	M0_ALLOC_ARR(ret->ffn_u.ffn_oper.ffon_opnds.fno_opnds,
		     FDMI_FLT_MAX_OPNDS_NR);

	ret->ffn_type = M0_FLT_OPERATION_NODE;
	ret->ffn_u.ffn_oper.ffon_op_code = op_code;
	ret->ffn_u.ffn_oper.ffon_opnds.fno_cnt = !!left + !!right;
	ret->ffn_u.ffn_oper.ffon_opnds.fno_opnds[0].ffnp_ptr = left;
	ret->ffn_u.ffn_oper.ffon_opnds.fno_opnds[1].ffnp_ptr = right;

	M0_LEAVE("ret=%p", ret);
	return ret;
}

M0_INTERNAL int m0_fdmi_flt_node_print(struct m0_fdmi_flt_node *node,
				       char                   **out)
{
	enum {
		FIRST_SIZE_GUESS = 256,
	};

	int   rc;
	char *str;

	M0_ENTRY();

	M0_ALLOC_ARR(str, FIRST_SIZE_GUESS);

	if (str == NULL) {
		rc = -ENOMEM;
		goto fail;
	}

	rc = m0_xcode_print(&M0_XCODE_OBJ(m0_fdmi_flt_node_xc, node),
			    str, FIRST_SIZE_GUESS);

	if (rc >= FIRST_SIZE_GUESS || M0_FI_ENABLED("rc_bigger_than_size_guess")) {
		m0_free(str);
		M0_ALLOC_ARR(str, rc + 1);

		if (str == NULL) {
			rc = -ENOMEM;
		} else {
			rc = m0_xcode_print(
				&M0_XCODE_OBJ(m0_fdmi_flt_node_xc, node),
				str, rc);
		}
	}

	if (rc > 0)
		rc = 0;
	else
		m0_free0(&str);

fail:
	*out = str;

	return M0_RC(rc);
}

M0_INTERNAL int m0_fdmi_flt_node_parse(const char              *str,
				       struct m0_fdmi_flt_node *node)
{
	return M0_RC(
		m0_xcode_read(&M0_XCODE_OBJ(m0_fdmi_flt_node_xc, node), str));
}

#undef M0_TRACE_SUBSYSTEM

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
