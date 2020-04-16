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

#include "lib/types.h"
#include "lib/errno.h"

#include "fdmi/filter.h"
#include "fdmi/flt_eval.h"

static int eval_or(struct m0_fdmi_flt_operands *opnds,
                   struct m0_fdmi_flt_operand  *res)
{
	int rc = 0;

	M0_ENTRY();

	if (opnds->ffp_count != 2 ||
	    opnds->ffp_operands[0].ffo_type != M0_FF_OPND_BOOL ||
	    opnds->ffp_operands[1].ffo_type != M0_FF_OPND_BOOL) {
		rc = -EINVAL;
	} else {
		m0_fdmi_flt_bool_opnd_fill(res,
			opnds->ffp_operands[0].ffo_data.fpl_pld.fpl_boolean ||
			opnds->ffp_operands[1].ffo_data.fpl_pld.fpl_boolean);
	}
	return M0_RC(rc);
}

static int eval_gt(struct m0_fdmi_flt_operands *opnds,
                   struct m0_fdmi_flt_operand  *res)
{
	int                            rc = 0;
	enum m0_fdmi_flt_operand_type  opnd_type;

	M0_ENTRY();

	if (opnds->ffp_count != 2) {
		return M0_RC(-EINVAL);
	}

	opnd_type = opnds->ffp_operands[0].ffo_type;

	/* Both operands should be int or both operands shoulb be uint */
	if (opnd_type == M0_FF_OPND_UINT) {
		if (opnds->ffp_operands[1].ffo_type != M0_FF_OPND_UINT) {
			rc = -EINVAL;
		} else {
			m0_fdmi_flt_bool_opnd_fill(res,
			    !!(opnds->ffp_operands[0].ffo_data.
			       fpl_pld.fpl_uinteger >
			    opnds->ffp_operands[1].ffo_data.
			       fpl_pld.fpl_uinteger));
		}
	} else if (opnd_type == M0_FF_OPND_INT) {
		if (opnds->ffp_operands[1].ffo_type != M0_FF_OPND_INT) {
			rc = -EINVAL;
		} else {
			m0_fdmi_flt_bool_opnd_fill(res,
			    !!(opnds->ffp_operands[0].ffo_data.
			       fpl_pld.fpl_integer >
			    opnds->ffp_operands[1].ffo_data.
			       fpl_pld.fpl_integer));
		}
	} else {
		rc = -EINVAL;
	}

	return M0_RC(rc);
}

static void init_std_operation_handlers(m0_fdmi_flt_op_cb_t *handlers)
{
	handlers[M0_FFO_OR] = eval_or;
	handlers[M0_FFO_GT] = eval_gt;
}

M0_INTERNAL int m0_fdmi_eval_add_op_cb(struct m0_fdmi_eval_ctx *ctx,
                                       enum m0_fdmi_flt_op_code op,
                                       m0_fdmi_flt_op_cb_t      cb)
{
	int rc = 0;

	M0_ENTRY("ctx=%p, op=%d", ctx, op);
	M0_PRE(op < M0_FFO_TOTAL_OPS_CNT);

	if (ctx->opers[op] != NULL) {
		rc = -EEXIST;
	} else {
		ctx->opers[op] = cb;
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_fdmi_eval_del_op_cb(struct m0_fdmi_eval_ctx *ctx,
                                        enum m0_fdmi_flt_op_code op)
{
	M0_ENTRY("ctx=%p, op=%d", ctx, op);
	M0_PRE(op < M0_FFO_TOTAL_OPS_CNT);

	ctx->opers[op] = NULL;

	M0_LEAVE();
}

M0_INTERNAL void m0_fdmi_eval_init(struct m0_fdmi_eval_ctx *ctx)
{
	M0_ENTRY("ctx=%p", ctx);
	M0_SET_ARR0(ctx->opers);
	init_std_operation_handlers(ctx->opers);
	M0_LEAVE();
}

static int eval_flt_node(struct m0_fdmi_eval_ctx    *ctx,
                         struct m0_fdmi_flt_node    *node,
                         struct m0_fdmi_flt_operand *res,
			 struct m0_fdmi_eval_var_info *var_info)
{
	struct m0_fdmi_flt_op_node *on = &node->ffn_u.ffn_oper;
	struct m0_fdmi_flt_operands operands = {0};
	int                         rc = 0;
	int                         i;

	M0_ENTRY();

	switch (node->ffn_type) {
	case M0_FLT_OPERATION_NODE:
		/* Gather operands and execute operation. */
		for (i = 0; i < on->ffon_opnds.fno_cnt; i ++) {
			rc = eval_flt_node(ctx, (struct m0_fdmi_flt_node *)
					   on->ffon_opnds.fno_opnds[i].ffnp_ptr,
					   &operands.ffp_operands[i], var_info);
			if (rc != 0)
				break;
			operands.ffp_count++;
		}

		if (rc == 0)
			rc = ctx->opers[on->ffon_op_code](&operands, res);
		/** @todo Free resources from operands? */
		break;
	case M0_FLT_OPERAND_NODE:
		*res = node->ffn_u.ffn_operand;
		rc = 0;
		break;
	case M0_FLT_VARIABLE_NODE:
		if (var_info->get_value_cb != NULL)
			rc = var_info->get_value_cb(var_info->user_data,
						    &node->ffn_u.ffn_var, res);
		else
			rc = -EINVAL;
		break;
	default:
		M0_ASSERT(false);
	}

	return M0_RC(rc);
}

M0_INTERNAL int m0_fdmi_eval_flt(struct m0_fdmi_eval_ctx *ctx,
                                 struct m0_fdmi_filter   *flt,
                                 struct m0_fdmi_eval_var_info *var_info)
{
	int                        rc;
	struct m0_fdmi_flt_operand res;

	M0_ENTRY();

	rc = eval_flt_node(ctx, flt->ff_root, &res, var_info);

	if (rc == 0) {
		M0_ASSERT(res.ffo_type == M0_FF_OPND_BOOL);
		M0_ASSERT(res.ffo_data.fpl_type == M0_FF_OPND_PLD_BOOL);
		rc = res.ffo_data.fpl_pld.fpl_boolean;
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_fdmi_eval_fini(struct m0_fdmi_eval_ctx *ctx)
{
	M0_ENTRY("ctx=%p", ctx);
	M0_LEAVE();
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
