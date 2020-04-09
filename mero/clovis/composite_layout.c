/* -*- C -*- */
/*
 * COPYRIGHT 2017 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Authors: Sining Wu       <sining.wu@seagate.com>
 *	    Pratik Shinde   <pratik.shinde@seagate.com>
 *	    Vishwas Bhat    <vishwas.bhat@seagate.com>
 * Original creation date: 05-May-2017
 */
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"
#include "lib/finject.h"
#include "lib/errno.h"
#include "lib/vec.h"
#include "lib/byteorder.h"         /* m0_byteorder_{cpu_to_be64|be64_to_cpu}. */
#include "fid/fid.h"               /* m0_fid */

#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_layout.h"

#include "dix/meta.h"
#include "dix/layout.h"

struct layout_dix_req {
	struct m0_sm_ast            dr_ast;
	/** Attach a clink to underline channel in the dix meta request.*/
	struct m0_clink             dr_clink;
	/** Arguments for clink callback. */
	struct m0_clovis_op_layout *dr_ol;
	/** DIX request to invoke operations on global layout index. */
	struct m0_dix_meta_req      dr_mreq;
};

M0_TL_DESCR_DEFINE(clayer, "composite layout layers",
                   static, struct m0_clovis_composite_layer,
		   ccr_tlink, ccr_tlink_magic,
		   M0_CLOVIS_CLAYER_TL_MAGIC, M0_CLOVIS_CLAYER_TL_MAGIC);
M0_TL_DEFINE(clayer, static, struct m0_clovis_composite_layer);

M0_TL_DESCR_DEFINE(cext, "composite layout extents",
		   static, struct m0_clovis_composite_extent,
		   ce_tlink, ce_tlink_magic,
		   M0_CLOVIS_CEXT_TL_MAGIC, M0_CLOVIS_CEXT_TL_MAGIC);
M0_TL_DEFINE(cext, static, struct m0_clovis_composite_extent);

static int composite_layout_io_build(struct m0_clovis_io_args *args,
				     struct m0_clovis_op **op);
static int
composite_extents_scan_sync(struct m0_clovis_obj *obj,
			    struct m0_clovis_composite_layout *clayout);

static struct layout_dix_req*
clovis_layout_dix_req_alloc(struct m0_sm_group *grp, struct m0_dix_cli *cli,
			    m0_chan_cb_t cb, struct m0_clovis_op_layout *ol)
{
	struct layout_dix_req *req;

	M0_ALLOC_PTR(req);
	if (req == NULL)
		return NULL;

	/* Only support dix for now. */
	m0_dix_meta_req_init(&req->dr_mreq, cli, grp);
	m0_clink_init(&req->dr_clink, cb);
	req->dr_ol = ol;
	return req;
}

static void clovis_layout_dix_req_fini(struct layout_dix_req *req)
{
	m0_clink_fini(&req->dr_clink);
	m0_dix_meta_req_fini(&req->dr_mreq);
}

static void clovis_layout_dix_req_free(struct layout_dix_req *req)
{
	m0_free(req);
}

static void clovis_layout_op_completed(struct m0_clovis_op *op)
{
	struct m0_sm_group *op_grp;

	M0_ENTRY();
	M0_PRE(op != NULL);

	op_grp = &op->op_sm_group;
	m0_sm_group_lock(op_grp);
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_EXECUTED);
	m0_clovis_op_executed(op);
	m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_STABLE);
	m0_clovis_op_stable(op);
	m0_sm_group_unlock(op_grp);

	M0_LEAVE();
}

static void clovis_layout_op_failed(struct m0_clovis_op *op, int rc)
{
	struct m0_sm_group *op_grp;

	M0_ENTRY();
	M0_PRE(rc != 0);
	M0_PRE(op != NULL);

	op_grp = &op->op_sm_group;
	m0_sm_group_lock(op_grp);
	m0_sm_fail(&op->op_sm, M0_CLOVIS_OS_FAILED, rc);
	m0_clovis_op_failed(op);
	m0_sm_group_unlock(op_grp);

	M0_LEAVE();
}

static void clovis_layout_dix_req_ast(struct m0_sm_group *grp,
				      struct m0_sm_ast *ast)
{
	int                         ltype;
	uint64_t                    lid;
	struct m0_clovis_op        *op;
	struct m0_clovis_obj       *obj;
	struct layout_dix_req      *req = ast->sa_datum;
	struct m0_clovis_op_layout *ol;
	int                         rc;

	M0_ENTRY();

	ol  = req->dr_ol;
	M0_ASSERT(ol != NULL && ol->ol_entity != NULL);
	op = &ol->ol_oc.oc_op;
	rc = ol->ol_ar.ar_rc;
	if (rc != 0) {
		clovis_layout_op_failed(op, rc);
		clovis_layout_dix_req_fini(req);
		clovis_layout_dix_req_free(req);
		return;
	}

	/*
 	 * Do we need to change cob's attribute to reflect the
 	 * layout type change?
 	 */
	if (ol->ol_oc.oc_op.op_code == M0_CLOVIS_EO_LAYOUT_SET) {
		ltype = ol->ol_layout->cl_type;
		obj = m0_clovis__obj_entity(ol->ol_entity);
		lid = obj->ob_attr.oa_layout_id;
		if (ltype != M0_CLOVIS_OBJ_LAYOUT_TYPE(lid)) {
			lid = M0_CLOVIS_OBJ_LAYOUT_MAKE_LID(lid, ltype);
			obj->ob_attr.oa_layout_id = lid;
			rc = m0_clovis__obj_layout_send(obj, ol);
			if (rc != 0)
				/* How to undo the changes in layout index? */
				clovis_layout_op_failed(op, rc);
		} else
			clovis_layout_op_completed(op);
	} else
		clovis_layout_op_completed(op);

	clovis_layout_dix_req_fini(req);
	clovis_layout_dix_req_free(req);
	M0_LEAVE();
}

static bool clovis_layout_dix_req_clink_cb(struct m0_clink *cl)
{
	int                         rc;
	struct m0_clovis_op        *op;
	struct m0_clovis_op_layout *ol;
	struct layout_dix_req      *req = M0_AMB(req, cl, dr_clink);
	struct m0_dix_meta_req     *mreq = &req->dr_mreq;
	struct m0_dix_layout        dlayout;

	m0_clink_del(cl);
	ol  = req->dr_ol;
	op = &ol->ol_oc.oc_op;
	M0_ASSERT(M0_IN(op->op_code,
		        (M0_CLOVIS_EO_LAYOUT_GET, M0_CLOVIS_EO_LAYOUT_SET)));
	rc = m0_dix_meta_generic_rc(mreq);
	if (rc != 0)
		goto ast;

	if (op->op_code == M0_CLOVIS_EO_LAYOUT_GET) {
		m0_dix_layout_rep_get(mreq, 0, &dlayout);
		ol->ol_ops->olo_copy_to_app(ol->ol_layout, (void *)&dlayout);
	}
ast:
	/* Post an AST to handle layout op. */
	ol->ol_ar.ar_rc = rc;
	req->dr_ast.sa_cb = clovis_layout_dix_req_ast;
	req->dr_ast.sa_datum = req;
	m0_sm_ast_post(ol->ol_sm_grp, &req->dr_ast);

	return false;
}

static void clovis_layout_dix_get_ast(struct m0_sm_group *grp,
				      struct m0_sm_ast *ast)
{
	int                         rc;
	struct layout_dix_req      *req = ast->sa_datum;
	struct m0_clovis_op_layout *ol = req->dr_ol;
	struct m0_fid              *layout_fid;

	M0_ENTRY();

	layout_fid = (struct m0_fid *)&ol->ol_entity->en_id;
	m0_clink_add_lock(&req->dr_mreq.dmr_chan, &req->dr_clink);
	rc = m0_dix_layout_get(&req->dr_mreq, layout_fid, 1);
	if (rc != 0) {
		m0_clink_del_lock(&req->dr_clink);
		clovis_layout_op_failed(&ol->ol_oc.oc_op, rc);
		clovis_layout_dix_req_fini(req);
		clovis_layout_dix_req_free(req);
	}

	M0_LEAVE();
	return;
}

static void clovis_layout_dix_put_ast(struct m0_sm_group *grp,
				      struct m0_sm_ast *ast)
{
	int                         rc;
	struct m0_fid              *layout_fid;
	struct layout_dix_req      *req = ast->sa_datum;
	struct m0_clovis_op_layout *ol = req->dr_ol;
	struct m0_dix_layout       *dix_layout;

	M0_ENTRY();
	M0_PRE(ol != NULL);

	/* Allocate and et m0_dix_layout. */
	M0_ALLOC_PTR(dix_layout);
	if (dix_layout == NULL) {
		rc = -ENOMEM;
		goto error;
	}
	rc = ol->ol_ops->olo_copy_from_app(ol->ol_layout, dix_layout);
	if (rc != 0)
		goto error;

	/*
	 * Set callback argument for updating sync record as thi is a UPDATE
	 * op to layout index.
	 */
	req->dr_mreq.dmr_req.dr_sync_datum = &ol->ol_oc.oc_op;

	/* Send request to dix. */
	m0_clink_add_lock(&req->dr_mreq.dmr_chan, &req->dr_clink);
	layout_fid = (struct m0_fid *)&ol->ol_entity->en_id;
	rc = m0_dix_layout_put(&req->dr_mreq, layout_fid, dix_layout, 1,
			       COF_OVERWRITE);
	if (rc != 0) {
		m0_clink_del_lock(&req->dr_clink);
		goto error;
	}
	M0_LEAVE();
	return;

error:
	M0_ASSERT(rc != 0);
	M0_ERR(rc);
	clovis_layout_op_failed(&ol->ol_oc.oc_op, rc);
	clovis_layout_dix_req_fini(req);
	clovis_layout_dix_req_free(req);
	m0_free(dix_layout);
	M0_LEAVE();
	return;
}

static void clovis_layout_dix_req_launch(struct layout_dix_req  *req,
				void (*exec_fn)(struct m0_sm_group *grp,
						struct m0_sm_ast   *ast))
{
	M0_ENTRY();
	req->dr_ast.sa_cb = exec_fn;
	req->dr_ast.sa_datum = req;
	m0_sm_ast_post(req->dr_ol->ol_sm_grp, &req->dr_ast);
	M0_LEAVE();
}

M0_INTERNAL int m0_clovis__layout_op_launch(struct m0_clovis_op_layout *ol)
{
	int                    opcode;
	struct layout_dix_req *req;

	M0_ENTRY();
	M0_PRE(ol != NULL);
	opcode = ol->ol_oc.oc_op.op_code;
	M0_PRE(M0_IN(opcode,
		     (M0_CLOVIS_EO_LAYOUT_GET, M0_CLOVIS_EO_LAYOUT_SET)));

	/* Construct request to dix. */
	req = clovis_layout_dix_req_alloc(ol->ol_sm_grp, ol_dixc(ol),
					  clovis_layout_dix_req_clink_cb, ol);
	if (req == NULL)
		return M0_ERR(-ENOMEM);

	switch(opcode) {
	case M0_CLOVIS_EO_LAYOUT_GET:
		clovis_layout_dix_req_launch(req, clovis_layout_dix_get_ast);
		break;
	case M0_CLOVIS_EO_LAYOUT_SET:
		clovis_layout_dix_req_launch(req, clovis_layout_dix_put_ast);
		break;
	default:
		M0_IMPOSSIBLE("Wrong layout opcode.");
	}

	return M0_RC(0);
}

M0_INTERNAL int m0_clovis__dix_layout_get_sync(struct m0_clovis_obj *obj,
					       struct m0_dix_layout *dlayout)
{
	int                     rc;
	struct m0_fid          *layout_fid;
	struct m0_dix_req      *dreq;
	struct m0_dix_meta_req *dmreq;
	struct layout_dix_req  *req;
	struct m0_sm_group *grp  = m0_locality0_get()->lo_grp;

	M0_ENTRY();
	M0_PRE(obj != NULL && obj->ob_layout != NULL);

	req = clovis_layout_dix_req_alloc(
			grp, ent_dixc(&obj->ob_entity), NULL, NULL);
	if (req == NULL)
		return M0_ERR(-ENOMEM);

	/* Send out the dix meta request. */
	dmreq = &req->dr_mreq;
	dreq  = &dmreq->dmr_req;

        m0_dix_req_lock(dreq);
	layout_fid = (struct m0_fid *)&obj->ob_entity.en_id;
	rc = m0_dix_layout_get(dmreq, layout_fid, 1)? :
	     m0_dix_req_wait(dreq,
		M0_BITS(DIXREQ_FINAL, DIXREQ_FAILURE), M0_TIME_NEVER)?:
	     m0_dix_req_rc(dreq);
	if (rc != 0)
		goto exit;

	/* Parse the layout. */
	m0_dix_layout_rep_get(dmreq, 0, dlayout);

exit:
	/* m0_dix_meta_req_fini() requires lock. */
	clovis_layout_dix_req_fini(req);
        m0_dix_req_unlock(dreq);
	clovis_layout_dix_req_free(req);
	return M0_RC(rc);
}

/**---------------------------------------------------------------------------*
 *                Clovis COMPOSITE LAYOUT OP APIS and routines                *
 *----------------------------------------------------------------------------*/

static int composite_layout_copy_to_app(struct m0_clovis_layout *to, void *from)
{
	int                                rc;
	int                                i = 0;
	struct m0_dix_layout              *dix_layout;
	struct m0_dix_composite_ldesc     *dix_cldesc;
	struct m0_clovis_composite_layout *clayout;
	struct m0_clovis_composite_layer  *layer;

	M0_ENTRY();
	M0_PRE(to != NULL);
	M0_PRE(from != NULL);

	clayout = M0_AMB(clayout, to, ccl_layout);
	dix_layout = (struct m0_dix_layout *)from;
	dix_cldesc = &dix_layout->u.dl_comp_desc;

	for (i = 0; i < dix_cldesc->cld_nr_layers; i++) {
		M0_ALLOC_PTR(layer);
		if (layer == NULL) {
			rc = M0_ERR(-ENOMEM);
			goto error;
		}
		layer->ccr_subobj = dix_cldesc->cld_layers[i].cr_subobj;
		layer->ccr_lid = dix_cldesc->cld_layers[i].cr_lid;
		layer->ccr_priority = dix_cldesc->cld_layers[i].cr_priority;
		cext_tlist_init(&layer->ccr_rd_exts);
		cext_tlist_init(&layer->ccr_wr_exts);
		clayer_tlink_init_at_tail(layer, &clayout->ccl_layers);
	}
	clayout->ccl_nr_layers = dix_cldesc->cld_nr_layers;
	return M0_RC(0);

error:
	m0_tl_teardown(clayer, &clayout->ccl_layers, layer)
		m0_free(layer);
	return M0_RC(rc);
}

static int composite_layout_copy_from_app(struct m0_clovis_layout *from,
					  void *to)
{
	int                                i = 0;
	struct m0_dix_layout              *dix_layout;
	struct m0_dix_composite_layer     *dix_clayers;
	struct m0_dix_composite_ldesc     *dix_cldesc;
	struct m0_clovis_composite_layout *clayout;
	struct m0_clovis_composite_layer  *layer;

	M0_ENTRY();
	M0_PRE(from != NULL);
	M0_PRE(to != NULL);

	clayout = M0_AMB(clayout, from, ccl_layout);
	M0_ALLOC_ARR(dix_clayers, clayout->ccl_nr_layers);
	if (dix_clayers == NULL)
		return M0_ERR(-ENOMEM);

	dix_layout = (struct m0_dix_layout *)to;
	dix_layout->dl_type = DIX_LTYPE_COMPOSITE_DESCR;
	dix_cldesc = &dix_layout->u.dl_comp_desc;
	dix_cldesc->cld_nr_layers = clayout->ccl_nr_layers;
	dix_cldesc->cld_layers = dix_clayers;
	m0_tl_for(clayer, &clayout->ccl_layers, layer) {
		dix_cldesc->cld_layers[i].cr_subobj = layer->ccr_subobj;
		dix_cldesc->cld_layers[i].cr_lid = layer->ccr_lid;
		dix_cldesc->cld_layers[i].cr_priority = layer->ccr_priority;
		i++;
	} m0_tl_endfor;

	return M0_RC(0);
}

const struct m0_clovis_op_layout_ops clovis_op_layout_composite_ops = {
	.olo_launch        = m0_clovis__layout_op_launch,
	.olo_copy_to_app   = composite_layout_copy_to_app,
	.olo_copy_from_app = composite_layout_copy_from_app,
};

static int composite_layout_get_sync(struct m0_clovis_obj *obj)
{
	int                  rc;
	struct m0_dix_layout dlayout;

	M0_ENTRY();
	m0_clovis__dix_layout_get_sync(obj, &dlayout);
	rc = composite_layout_copy_to_app(obj->ob_layout, &dlayout);
	return M0_RC(rc);
}

static int composite_layout_get(struct m0_clovis_layout *layout)
{
	int                                rc = 0;
	struct m0_clovis_obj              *obj;
	struct m0_clovis_composite_layout *comp;

	M0_ENTRY();

	comp = M0_AMB(comp, layout, ccl_layout);
	m0_mutex_init(&comp->ccl_lock);
	clayer_tlist_init(&comp->ccl_layers);

	/*
	 * Retrieve layout and extents for the object this layout
	 * associated with.
	 */
	obj = layout->cl_obj;
	rc = composite_layout_get_sync(obj)?:
	     composite_extents_scan_sync(obj, comp);
	return M0_RC(rc);
}

static void composite_layout_put(struct m0_clovis_layout *layout)
{
	struct m0_clovis_composite_layout *comp;
	struct m0_clovis_composite_layer  *layer;
	struct m0_clovis_composite_extent *ext;

	comp = M0_AMB(comp, layout, ccl_layout);
	m0_mutex_fini(&comp->ccl_lock);

	/* Teardown extent lists and layer list. */
	m0_tl_teardown(clayer, &comp->ccl_layers, layer) {
		m0_tl_teardown(cext, &layer->ccr_rd_exts, ext)
			m0_free(ext);
		m0_tl_teardown(cext, &layer->ccr_wr_exts, ext)
			m0_free(ext);
		m0_free0(&layer);
	}
}

static int composite_layout_alloc(struct m0_clovis_layout **out)
{
	struct m0_clovis_layout           *layout;
	struct m0_clovis_composite_layout *comp;

	M0_ENTRY();

	*out = NULL;
	M0_ALLOC_PTR(comp);
	if (comp == NULL)
		return M0_ERR(-ENOMEM);

	layout = &comp->ccl_layout;
	layout->cl_type = M0_CLOVIS_LT_COMPOSITE;
	layout->cl_ops = &clovis_layout_composite_ops;
	/* Initialise layer list. */
	m0_mutex_init(&comp->ccl_lock);
	clayer_tlist_init(&comp->ccl_layers);
	*out = layout;

	return M0_RC(0);
}

const struct m0_clovis_layout_ops clovis_layout_composite_ops = {
	.lo_alloc     = composite_layout_alloc,
        .lo_get       = composite_layout_get,
        .lo_put       = composite_layout_put,
        .lo_io_build  = composite_layout_io_build,
};

int m0_clovis_composite_layer_add(struct m0_clovis_layout *layout,
				  struct m0_clovis_obj *sub_obj, int priority)
{
	int                                rc;
	struct m0_clovis_composite_layout *clayout;
	struct m0_clovis_composite_layer  *layer;
	struct m0_clovis_composite_layer  *found;
	struct m0_clovis_composite_layer  *anchor;

	M0_ENTRY();
	M0_PRE(layout != NULL);
	clayout = M0_AMB(clayout, layout, ccl_layout);

	/* The same sub-object is not allowed to add twice. */
	m0_mutex_lock(&clayout->ccl_lock);
	found = m0_tl_find(clayer, found, &clayout->ccl_layers,
			   m0_uint128_eq(&found->ccr_subobj,
					 &sub_obj->ob_entity.en_id));
	m0_mutex_unlock(&clayout->ccl_lock);
	if (found != NULL)
		return 0;

	/* Prepare for a new layer. */
	M0_ALLOC_PTR(layer);
	if (layer == NULL)
		return M0_ERR(-ENOMEM);
	cext_tlist_init(&layer->ccr_rd_exts);
	cext_tlist_init(&layer->ccr_wr_exts);
	layer->ccr_priority = priority;

	rc = m0_clovis__obj_attr_get_sync(sub_obj);
	if (rc != 0) {
		m0_free(layer);
		return M0_ERR(rc);
	}
	layer->ccr_subobj = sub_obj->ob_entity.en_id;
	layer->ccr_lid    = m0_clovis__obj_lid(sub_obj);
	clayer_tlink_init(layer);

	/* Sort layers by priority (smaller value, higer priority). */
	m0_mutex_lock(&clayout->ccl_lock);
	anchor = m0_tl_find(clayer, anchor, &clayout->ccl_layers,
			   anchor->ccr_priority > priority);
	if (anchor != NULL)
		clayer_tlist_add_before(anchor, layer);
	else
		clayer_tlist_add_tail(&clayout->ccl_layers, layer);
	clayout->ccl_nr_layers++;
	m0_mutex_unlock(&clayout->ccl_lock);

	return M0_RC(0);
}
M0_EXPORTED(m0_clovis_composite_layer_add);

void m0_clovis_composite_layer_del(struct m0_clovis_layout *layout,
				   struct m0_uint128 subobj_id)
{
	struct m0_clovis_composite_layout *clayout;
	struct m0_clovis_composite_layer  *layer;
	struct m0_clovis_composite_extent *ext;

	M0_ENTRY();
	M0_PRE(layout != NULL);

	clayout = M0_AMB(clayout, layout, ccl_layout);
	m0_mutex_lock(&clayout->ccl_lock);
	layer = m0_tl_find(clayer, layer, &clayout->ccl_layers,
			   m0_uint128_eq(&layer->ccr_subobj, &subobj_id));
	if (layer == NULL) {
		M0_LEAVE();
		return;
	}
	m0_tl_teardown(cext, &layer->ccr_rd_exts, ext)
		m0_free(ext);
	m0_tl_teardown(cext, &layer->ccr_wr_exts, ext)
		m0_free(ext);
	clayer_tlist_del(layer);
	clayout->ccl_nr_layers--;
	m0_mutex_unlock(&clayout->ccl_lock);

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_composite_layer_del);

/**---------------------------------------------------------------------------*
 *                   Clovis COMPOSITE LAYOUT IO routines                      *
 *----------------------------------------------------------------------------*/

struct composite_sub_io_ext {
	m0_bindex_t      sie_off;
	m0_bcount_t      sie_len;
	void            *sie_buf;

	uint64_t         sie_tlink_magic;
	struct m0_tlink  sie_tlink;
};

struct composite_sub_io {
	struct m0_uint128 si_id;
	uint64_t          si_lid;
	int               si_nr_exts;
	struct m0_tl      si_exts;
};

M0_TL_DESCR_DEFINE(sio_ext, "composite layout subobj extent list",
                   static, struct composite_sub_io_ext,
		   sie_tlink, sie_tlink_magic,
		   M0_CLOVIS_CIO_EXT_MAGIC, M0_CLOVIS_CIO_EXT_MAGIC);
M0_TL_DEFINE(sio_ext, static, struct composite_sub_io_ext);

static const struct m0_bob_type oci_bobtype;
M0_BOB_DEFINE(static, &oci_bobtype,  m0_clovis_op_composite_io);
static const struct m0_bob_type oci_bobtype = {
	.bt_name         = "oci_bobtype",
	.bt_magix_offset = offsetof(struct m0_clovis_op_composite_io, oci_magic),
	.bt_magix        = M0_CLOVIS_OCI_MAGIC,
	.bt_check        = NULL,
};

static bool
clovis_op_composite_io_invariant(const struct m0_clovis_op_composite_io *oci)
{
	return M0_RC(oci != NULL &&
		     m0_clovis_op_composite_io_bob_check(oci) &&
		     oci->oci_oo.oo_oc.oc_op.op_size < sizeof *oci &&
		     M0_IN(oci->oci_oo.oo_oc.oc_op.op_code,
			   (M0_CLOVIS_OC_READ, M0_CLOVIS_OC_WRITE)) &&
		     m0_fid_is_valid(&oci->oci_oo.oo_fid));
}

static void composite_sub_io_destroy(struct composite_sub_io *sio_arr,
				     int nr_subobjs)
{
	int                          i;
	struct composite_sub_io     *io;
	struct composite_sub_io_ext *io_ext;

	for (i = 0; i < nr_subobjs; i++) {
		io = &sio_arr[i];
		m0_tl_teardown(sio_ext, &io->si_exts, io_ext)
			m0_free(io_ext);
	}
	m0_free(sio_arr);
}

/*
 * Divide original IO index vector and buffers according to sub-objects.
 */
static int composite_io_divide(struct m0_clovis_composite_layout *clayout,
			       enum m0_clovis_obj_opcode opcode,
			       struct m0_indexvec *ext, struct m0_bufvec *data,
			       struct composite_sub_io **out,
			       int *out_nr_sios)
{
	int                                 rc;
	int                                 i;
	int                                 j;
	int                                 k;
	int                                 nr_subobjs;
	int                                 valid_subobj_cnt = 0;
	m0_bindex_t                         off;
	m0_bcount_t                         len = 0;
	m0_bindex_t                         next_off;
	struct m0_ivec_cursor               icursor;
	struct m0_bufvec_cursor             bcursor;
	struct composite_sub_io            *sio_arr;
	struct composite_sub_io_ext        *sio_ext;
	struct m0_clovis_composite_layer   *layer = NULL;
	struct m0_clovis_composite_extent **cexts;
	struct m0_tl                      **cext_tlists;
	struct m0_tl                       *tl;

	nr_subobjs = clayout->ccl_nr_layers;
	M0_ASSERT(nr_subobjs != 0);
	M0_ASSERT(!clayer_tlist_is_empty(&clayout->ccl_layers));
	M0_ALLOC_ARR(sio_arr, nr_subobjs);
	M0_ALLOC_ARR(cext_tlists, nr_subobjs);
	M0_ALLOC_ARR(cexts, nr_subobjs);
	if (sio_arr == NULL || cext_tlists == NULL || cexts == NULL) {
		m0_free(sio_arr);
		m0_free(cext_tlists);
		m0_free(cexts);
		return M0_ERR(-ENOMEM);
	}

	for (i = 0; i < nr_subobjs; i++) {
		if (i == 0)
			layer = clayer_tlist_head(&clayout->ccl_layers);
		else
			layer = clayer_tlist_next(&clayout->ccl_layers, layer);
		tl = (opcode == M0_CLOVIS_OC_READ)?
		     &layer->ccr_rd_exts: &layer->ccr_wr_exts;

		/* Only those layers with extents are considered valid. */
		if (cext_tlist_is_empty(tl))
			continue;

		/* Initialise subobj IO. */
		cext_tlists[valid_subobj_cnt] = tl;
		cexts[valid_subobj_cnt] = cext_tlist_head(tl);
		sio_ext_tlist_init(&sio_arr[valid_subobj_cnt].si_exts);
		sio_arr[valid_subobj_cnt].si_id = layer->ccr_subobj;
		sio_arr[valid_subobj_cnt].si_lid = layer->ccr_lid;
		valid_subobj_cnt++;
	}
	M0_ASSERT(valid_subobj_cnt != 0);

	m0_ivec_cursor_init(&icursor, ext);
	m0_bufvec_cursor_init(&bcursor, data);
	for (i = 0; !m0_ivec_cursor_move(&icursor, len) &&
		    !m0_bufvec_cursor_move(&bcursor, len); i++) {
		off = m0_ivec_cursor_index(&icursor);
		len = m0_ivec_cursor_step(&icursor);

		/*
		 * Naive search for an extent covering the offset. Note that
		 * layers are sorted in priority order and extents are sorted
		 * in offset order in a layer. It is considered an assert if
		 * there is no sub-object covering the offset.
		 */
		for (j = 0; j < valid_subobj_cnt; j++)
			if (cexts[j] != NULL && off >= cexts[j]->ce_off)
				break;
		M0_ASSERT(j != valid_subobj_cnt);

		/* Compute the step to advance. */
		next_off = cexts[j]->ce_off + cexts[j]->ce_len;
		for (k = j - 1; k >= 0; k--) {
			if (cexts[k] == NULL)
				continue;
			if (cexts[k]->ce_off <= next_off )
				break;
		}
		if (k >= 0)
			next_off = cexts[k]->ce_off;

		if (next_off > off + len)
			next_off = off + len;
		len = next_off - off;

		/* Create a new IO extent. */
		M0_ALLOC_PTR(sio_ext);
		if (sio_ext == NULL) {
			rc = M0_ERR(-ENOMEM);
			goto error;
		}
		sio_ext->sie_off = off;
		sio_ext->sie_len = len;
		sio_ext->sie_buf = m0_bufvec_cursor_addr(&bcursor);
		sio_arr[j].si_nr_exts++;
		sio_ext_tlink_init_at(sio_ext, &sio_arr[j].si_exts);

		/* Advance each layer's extent cursor. */
		for (j = 0; j < valid_subobj_cnt; j++) {
			if (cexts[j] == NULL ||
			    cexts[j]->ce_off + cexts[j]->ce_len > next_off)
				continue;
			cexts[j] = cext_tlist_next(cext_tlists[j], cexts[j]);
			while (cexts[j] != NULL &&
			       cexts[j]->ce_off + cexts[j]->ce_len <= next_off)
				cexts[j] = cext_tlist_next(
						cext_tlists[j], cexts[j]);
		}
	}
	*out = sio_arr;
	*out_nr_sios = valid_subobj_cnt;
	return M0_RC(0);

error:
	composite_sub_io_destroy(sio_arr, nr_subobjs);
	return M0_RC(rc);
}

static void composite_io_op_done(struct m0_clovis_op_composite_io *oci)
{
	int                  i;
	struct m0_clovis_op *child_op = NULL;
	struct m0_clovis_op *op;
	struct m0_sm_group  *op_grp;

	M0_ENTRY();
	M0_PRE(oci != NULL);
	M0_PRE(oci->oci_nr_sub_ops > 0);

	op = &oci->oci_oo.oo_oc.oc_op;
	op_grp = &op->op_sm_group;

	for (i = 0; i < oci->oci_nr_sub_ops; i++) {
		child_op = oci->oci_sub_ops[i];
		if (child_op->op_sm.sm_rc != 0)
			break;
	}

	m0_sm_group_lock(op_grp);
	if (i == oci->oci_nr_sub_ops) {
		/* IO is completed successfully. */
		m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_EXECUTED);
		m0_clovis_op_executed(op);
		m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_STABLE);
		m0_clovis_op_stable(op);
	} else {
		/* IO fails. */
		m0_sm_move(&op->op_sm, 0, M0_CLOVIS_OS_EXECUTED);
		m0_clovis_op_executed(op);
		m0_sm_fail(&op->op_sm, M0_CLOVIS_OS_FAILED,
			   child_op->op_sm.sm_rc);
		m0_clovis_op_failed(op);
	}
	m0_sm_group_unlock(op_grp);
	M0_LEAVE();
}

static void composite_io_op_ast(struct m0_sm_group *grp,
				struct m0_sm_ast *ast)
{
	struct m0_clovis_op              *child_op;
	struct m0_clovis_op              *op;
	struct m0_clovis_op_common       *oc;
	struct m0_clovis_op_obj          *oo;
	struct m0_clovis_op_composite_io *oci;

	M0_ENTRY();

	M0_PRE(grp != NULL);
	M0_PRE(m0_sm_group_is_locked(grp));
	M0_PRE(ast != NULL);

	child_op =
		bob_of(ast,  struct m0_clovis_op, op_parent_ast, &op_bobtype);
	op = child_op->op_parent;
	oc = bob_of(op, struct m0_clovis_op_common, oc_op, &oc_bobtype);
	oo  = bob_of(oc, struct m0_clovis_op_obj, oo_oc, &oo_bobtype);
	oci = bob_of(oo, struct m0_clovis_op_composite_io,
		     oci_oo, &oci_bobtype);
	clovis_op_composite_io_invariant(oci);
	oci->oci_nr_replied++;
	if (oci->oci_nr_replied == oci->oci_nr_sub_ops)
		composite_io_op_done(oci);

	M0_LEAVE();
}

static int
composite_sub_io_op_build(struct m0_clovis_obj *cobj,
			  struct m0_clovis_op  *cop,
			  struct composite_sub_io *sio,
			  struct m0_clovis_op **out)
{
	int                          i = 0;
	int                          rc;
	struct m0_clovis_obj        *obj = NULL;
	struct m0_clovis_op         *op = NULL;
	struct m0_indexvec          *ext = NULL;
	struct m0_bufvec            *data = NULL;
	struct m0_bufvec            *attr = NULL;
	struct composite_sub_io_ext *sio_ext;

	M0_ALLOC_PTR(ext);
	M0_ALLOC_PTR(data);
	M0_ALLOC_PTR(attr);
	if (ext == NULL || data == NULL || attr == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto error;
	}

	if (m0_indexvec_alloc(ext, sio->si_nr_exts) ||
	    m0_bufvec_empty_alloc(data, sio->si_nr_exts) ||
	    m0_bufvec_alloc(attr, 1, 1)) {
		rc = M0_ERR(-ENOMEM);
		goto error;
	}
	m0_tl_for(sio_ext, &sio->si_exts, sio_ext) {
		ext->iv_vec.v_count[i] = sio_ext->sie_len;
		ext->iv_index[i] = sio_ext->sie_off;
		data->ov_buf[i] = sio_ext->sie_buf;
		data->ov_vec.v_count[i] = sio_ext->sie_len;
		i++;
	} m0_tl_endfor;

	/* Initial the object for IO. */
	M0_ALLOC_PTR(obj);
	if (obj == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto error;
	}
	m0_clovis_obj_init(obj, cobj->ob_entity.en_realm,
			   &sio->si_id, sio->si_lid);
	rc = m0_clovis__obj_attr_get_sync(obj);
	if (rc != 0)
		goto error;

	/* Create an IO op for the sub object. */
	m0_clovis_obj_op(obj, cop->op_code, ext, data, attr, 0, &op);
	if (op == NULL)
		goto error;
	op->op_parent = cop;
	op->op_parent_ast.sa_cb = &composite_io_op_ast;
	*out = op;
	return M0_RC(0);

error:
	m0_indexvec_free(ext);
	m0_bufvec_free(data);
	m0_bufvec_free(attr);
	m0_free(ext);
	m0_free(data);
	m0_free(attr);
	m0_free(obj);
	m0_free(op);
	return M0_RC(rc);
}

static int
composite_sub_io_ops_build(struct m0_clovis_obj *cobj,
			   struct m0_clovis_op *cop,
			   struct composite_sub_io *sio_arr,
			   int nr_subobjs)
{
	int                               i;
	int                               rc = 0;
	struct m0_clovis_op              *op = NULL;
	struct m0_clovis_op_obj          *oo;
	struct m0_clovis_op_common       *oc;
	struct m0_clovis_op_composite_io *oci;
	struct composite_sub_io          *sio;

	M0_ENTRY();
	oc = M0_AMB(oc, cop, oc_op);
	oo = M0_AMB(oo, oc, oo_oc);
	oci = M0_AMB(oci, oo, oci_oo);
	M0_ALLOC_ARR(oci->oci_sub_ops, nr_subobjs);
	if (oci->oci_sub_ops == NULL)
		return M0_ERR(-ENOMEM);

	for (i = 0; i < nr_subobjs; i++) {
		sio = &sio_arr[i];
		if (sio->si_nr_exts == 0)
			continue;
		rc = composite_sub_io_op_build(cobj, cop, sio, &op);
		if (rc != 0)
			goto error;
		oci->oci_sub_ops[oci->oci_nr_sub_ops] = op;
		oci->oci_nr_sub_ops++;
	}
	M0_ASSERT(rc == 0);
	return M0_RC(rc);
error:
	m0_free(oci->oci_sub_ops);
	return M0_ERR(rc);
}

static void composite_io_op_cb_fini(struct m0_clovis_op_common *oc)
{
	int                               i;
	struct m0_clovis_op              *sop;
	struct m0_clovis_op_obj          *oo;
	struct m0_clovis_op_composite_io *oci;

	M0_ENTRY();

	M0_PRE(oc != NULL);
	M0_PRE(M0_IN(oc->oc_op.op_code,
		     (M0_CLOVIS_OC_WRITE, M0_CLOVIS_OC_READ)));
	M0_PRE(M0_IN(oc->oc_op.op_sm.sm_state,
		     (M0_CLOVIS_OS_STABLE, M0_CLOVIS_OS_FAILED,
		      M0_CLOVIS_OS_INITIALISED)));
	M0_PRE(oc->oc_op.op_size >= sizeof *oci);

	/* Finialise each sub IO op. */
	oo  = bob_of(oc, struct m0_clovis_op_obj, oo_oc, &oo_bobtype);
	oci = bob_of(oo, struct m0_clovis_op_composite_io,
		     oci_oo, &oci_bobtype);
	clovis_op_composite_io_invariant(oci);
	for (i = 0; i < oci->oci_nr_sub_ops; i++) {
		sop = oci->oci_sub_ops[i];
		if (M0_FI_ENABLED("skip_fini_sub_io_op"))
			continue;

		M0_ASSERT(sop != NULL);
		m0_clovis_op_fini(sop);
		m0_clovis_entity_fini(sop->op_entity);
	}

	/* Finalise the bob type */
	m0_clovis_op_obj_bob_fini(oo);
	m0_clovis_op_composite_io_bob_fini(oci);
	M0_LEAVE();
}

static void composite_io_op_cb_free(struct m0_clovis_op_common *oc)
{
	int                               i;
	struct m0_clovis_op              *sop;
	struct m0_clovis_op_common       *soc;
	struct m0_clovis_op_obj          *soo;
	struct m0_clovis_op_io           *sioo;
	struct m0_clovis_op_obj          *oo;
	struct m0_clovis_op_composite_io *oci;

	M0_ENTRY();
	M0_PRE(oc != NULL);
	M0_PRE((oc->oc_op.op_size >= sizeof *oci));

	/* Can't use bob_of here */
	oo = M0_AMB(oo, oc, oo_oc);
	oci = M0_AMB(oci, oo, oci_oo);
	for (i = 0; i < oci->oci_nr_sub_ops; i++) {
		sop = oci->oci_sub_ops[i];
		if (M0_FI_ENABLED("skip_free_sub_io_op"))
			continue;

		soc = M0_AMB(soc, sop, oc_op);
		soo = M0_AMB(soo, soc, oo_oc);
		sioo = M0_AMB(sioo, soo, ioo_oo);
		M0_ASSERT(sioo != NULL);
		m0_indexvec_free(&sioo->ioo_ext);
		m0_bufvec_free2(&sioo->ioo_data);
		m0_bufvec_free2(&sioo->ioo_attr);
		m0_clovis_op_free(&soc->oc_op);
	}

	m0_free(oci->oci_sub_ops);
	M0_LEAVE();
}

static void composite_io_op_cb_launch(struct m0_clovis_op_common *oc)
{
	int                               i;
	struct m0_clovis_op              *sop;
	struct m0_clovis_op              *ops[1] = {NULL};
	struct m0_clovis_op_obj          *oo;
	struct m0_clovis_op_composite_io *oci;

	M0_ENTRY();
	M0_PRE(oc != NULL);
	M0_PRE(oc->oc_op.op_entity != NULL);
	M0_PRE(m0_uint128_cmp(&M0_CLOVIS_ID_APP,
			      &oc->oc_op.op_entity->en_id) < 0);
	M0_PRE(M0_IN(oc->oc_op.op_code,
		     (M0_CLOVIS_OC_WRITE, M0_CLOVIS_OC_READ)));
	M0_PRE(oc->oc_op.op_size >= sizeof *oci);

	oo = bob_of(oc, struct m0_clovis_op_obj, oo_oc, &oo_bobtype);
	oci = bob_of(oo, struct m0_clovis_op_composite_io,
		     oci_oo, &oci_bobtype);
	clovis_op_composite_io_invariant(oci);
	for (i = 0; i < oci->oci_nr_sub_ops; i++) {
		if (M0_FI_ENABLED("no_subobj_ops_launched"))
			continue;
		sop = oci->oci_sub_ops[i];
		M0_ASSERT(sop != NULL);
		ops[0] = sop;
		m0_clovis_op_launch(ops, 1);
	}
	m0_sm_move(&oc->oc_op.op_sm, 0, M0_CLOVIS_OS_LAUNCHED);

	M0_LEAVE();
}

static int composite_io_op_init(struct m0_clovis_obj *obj,
			        int opcode, struct m0_clovis_op *op)
{
	int                               rc;
	struct m0_clovis                 *cinst;
	struct m0_clovis_entity          *entity;
	struct m0_locality               *locality;
	struct m0_clovis_op_obj          *oo;
	struct m0_clovis_op_common       *oc;
	struct m0_clovis_op_composite_io *oci;

	M0_ENTRY();

	M0_PRE(obj != NULL);
	entity = &obj->ob_entity;
	cinst = m0_clovis__entity_instance(entity);

	M0_ASSERT(op->op_size >= sizeof(struct m0_clovis_op_composite_io));
	oc  = M0_AMB(oc, op, oc_op);
	oo  = M0_AMB(oo, oc, oo_oc);
	oci = M0_AMB(oci, oo, oci_oo);

	/* Initialise the operation */
	op->op_code = opcode;
	rc = m0_clovis_op_init(op, &clovis_op_conf, entity);
	if (rc != 0)
		return M0_ERR(rc);

	/* Initalise the vtable */
	oc->oc_cb_launch = composite_io_op_cb_launch;
	oc->oc_cb_fini = composite_io_op_cb_fini;
	oc->oc_cb_free = composite_io_op_cb_free;

	/* Set locality for this op. */
	locality = m0_clovis__locality_pick(cinst);
	M0_ASSERT(locality != NULL);
	oo->oo_sm_grp = locality->lo_grp;

	/* Initialise op data structures as 'bob's. */
	m0_clovis_op_common_bob_init(oc);
	m0_clovis_op_obj_bob_init(oo);
	m0_clovis_op_composite_io_bob_init(oci);

	return M0_RC(0);
}

/*
 * Note: current version simplifies constructing layer's IOs by retrieving
 * all layout details back from global layout index and extent indices. The
 * layout information is used to calculate which part of IO goes to which
 * layer(sub-object). This method may not be good in the case where there are
 * large number of extents and IO op only covers a small range of the object.
 * An alternative method is on-demand extent retrival. But this method may
 * have to re-retrieve extent information for every op even for back to back
 * ops if extents for a layer are not continuous.
 */
static int composite_layout_io_build(struct m0_clovis_io_args *args,
				     struct m0_clovis_op **op)
{
	int                                rc;
	int                                nr_sios = 0;
	struct m0_clovis_obj              *obj;
	struct m0_clovis_composite_layout *clayout;
	struct composite_sub_io           *sio_arr = NULL;

	M0_ENTRY();
	M0_PRE(args != NULL);
	M0_PRE(args->ia_obj != NULL);

	obj = args->ia_obj;
	rc = m0_clovis_op_get(op, sizeof(struct m0_clovis_op_composite_io)) ?:
		composite_io_op_init(obj, args->ia_opcode, *op);
	if (rc != 0)
		return M0_ERR(rc);

	/* Construct child IO ops. */
	clayout = M0_AMB(clayout, obj->ob_layout, ccl_layout);
	rc = composite_io_divide(
		clayout, args->ia_opcode, args->ia_ext,
		args->ia_data, &sio_arr, &nr_sios)?:
	     composite_sub_io_ops_build(obj, *op, sio_arr, nr_sios);
	composite_sub_io_destroy(sio_arr, nr_sios);
	return (rc == 0)?M0_RC(0):M0_ERR(rc);
}

/**---------------------------------------------------------------------------*
 *           Clovis Composite Layout Extent Index API and routines            *
 *----------------------------------------------------------------------------*/

/**
 * 2 global extent indices are created for composite objects' extents:
 * one is for read extents and another is for write extents. Clovis uses
 * two reserved FIDs for these 2 global extent indices.
 * The format for key/value pairs is defined as:
 *    key   = {layer_id, extent offset} and
 *    value =  extent length
 * To ensure that the key/value pairs are stored in key's lexicographical,
 * order (so that extents for a layer is stored in increasing offset order),
 * clovis transforms the keys input by the application in big-endian format.
 *
 * To create these internal extent indices, clovis also has to maintain
 * an container for it.
 */
struct m0_fid composite_extent_rd_idx_fid = M0_FID_TINIT('x', 0, 0x10);
struct m0_fid composite_extent_wr_idx_fid = M0_FID_TINIT('x', 0, 0x11);

/* Container used by Clovis internally. */
static struct m0_clovis_container composite_container;

enum {
	M0_CLOVIS_COMPOSITE_EXTENT_SCAN_BATCH = 5
};

int m0_clovis_composite_layer_idx_key_to_buf(
			struct m0_clovis_composite_layer_idx_key *key,
			void **out_kbuf, m0_bcount_t *out_klen)
{
	struct m0_clovis_composite_layer_idx_key *be_key;

	M0_ENTRY();
	M0_PRE(out_kbuf != NULL);
	M0_PRE(out_klen != NULL);

	M0_ALLOC_PTR(be_key);
	if (be_key == NULL)
		return M0_ERR(-ENOMEM);
	be_key->cek_layer_id.u_hi =
		m0_byteorder_cpu_to_be64(key->cek_layer_id.u_hi);
	be_key->cek_layer_id.u_lo =
		m0_byteorder_cpu_to_be64(key->cek_layer_id.u_lo);
	be_key->cek_off =
		m0_byteorder_cpu_to_be64(key->cek_off);
	*out_kbuf = be_key;
	*out_klen = sizeof *be_key;
	return M0_RC(0);
}
M0_EXPORTED(m0_clovis_composite_layer_idx_key_to_buf);

void m0_clovis_composite_layer_idx_key_from_buf(
			struct m0_clovis_composite_layer_idx_key *key,
			void *kbuf)
{
	struct m0_clovis_composite_layer_idx_key *be_key;

	M0_PRE(key != NULL);
	M0_PRE(kbuf != NULL);
	be_key = (struct m0_clovis_composite_layer_idx_key *)kbuf;
	key->cek_layer_id.u_hi =
		m0_byteorder_be64_to_cpu(be_key->cek_layer_id.u_hi);
	key->cek_layer_id.u_lo =
		m0_byteorder_be64_to_cpu(be_key->cek_layer_id.u_lo);
	key->cek_off =
		m0_byteorder_be64_to_cpu(be_key->cek_off);
}
M0_EXPORTED(m0_clovis_composite_layer_idx_key_from_buf);

int m0_clovis_composite_layer_idx_val_to_buf(
			struct m0_clovis_composite_layer_idx_val *val,
			void **out_vbuf, m0_bcount_t *out_vlen)
{
	struct m0_clovis_composite_layer_idx_val *be_val;

	M0_ENTRY();
	M0_PRE(out_vbuf != NULL);
	M0_PRE(out_vlen != NULL);

	M0_ALLOC_PTR(be_val);
	if (be_val == NULL)
		return M0_ERR(-ENOMEM);
	be_val->cev_len = val->cev_len;
	*out_vbuf = be_val;
	*out_vlen = sizeof *be_val;
	return M0_RC(0);
}
M0_EXPORTED(m0_clovis_composite_layer_idx_val_to_buf);

void m0_clovis_composite_layer_idx_val_from_buf(
			struct m0_clovis_composite_layer_idx_val *val,
			void *vbuf)
{
	struct m0_clovis_composite_layer_idx_val *be_val;

	M0_PRE(val != NULL);
	M0_PRE(vbuf != NULL);

	be_val = (struct m0_clovis_composite_layer_idx_val *)vbuf;
	val->cev_len = be_val->cev_len;
}
M0_EXPORTED(m0_clovis_composite_layer_idx_val_from_buf);

int m0_clovis_composite_layer_idx(struct m0_uint128 layer_id,
				  bool write, struct m0_clovis_idx *idx)
{
	struct m0_fid fid;

	M0_ENTRY();
	M0_PRE(idx != NULL);

	/* Initialise in-memory index data structure. */
	fid = (write == true)?
	      composite_extent_wr_idx_fid:composite_extent_rd_idx_fid;
	m0_clovis_idx_init(idx, &composite_container.co_realm,
			   (struct m0_uint128 *)&fid);

	return M0_RC(0);
}
M0_EXPORTED(m0_clovis_composite_layer_idx);

M0_INTERNAL int m0_clovis__composite_container_init(struct m0_clovis *cinst)
{
	int rc = 0;

	m0_clovis_container_init(&composite_container,
				 NULL, &M0_CLOVIS_UBER_REALM, cinst);
	rc = composite_container.co_realm.re_entity.en_sm.sm_rc;
	if (rc != 0)
		return M0_ERR(rc);
	else
		return M0_RC(rc);
}

static int composite_layer_idx_next_query(struct m0_clovis_idx *idx,
					  struct m0_bufvec *keys,
					  struct m0_bufvec *vals,
					  int *rcs, uint32_t flags)
{
	int                  rc;
	struct m0_clovis_op *ops[1] = {NULL};

	m0_clovis_idx_op(idx, M0_CLOVIS_IC_NEXT, keys, vals, rcs, flags,
			 &ops[0]);
	m0_clovis_op_launch(ops, 1);
	rc = m0_clovis_op_wait(ops[0],
			       M0_BITS(M0_CLOVIS_OS_FAILED,
				       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER)?:
	     ops[0]->op_sm.sm_rc;

	/* fini and release */
	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
	return M0_RC(rc);
}

static int
composite_layer_idx_extents_extract(
			struct m0_clovis_composite_layer *layer,
			struct m0_bufvec *keys,
			struct m0_bufvec *vals, int *rcs,
			struct m0_tl *ext_list,
			struct m0_clovis_composite_layer_idx_key *max_key)
{
	int                                       i;
	struct m0_clovis_composite_extent        *ext;
	struct m0_clovis_composite_layer_idx_key  key;

	for (i = 0; i < keys->ov_vec.v_nr; i++) {
		/* Reach the end of index. */
		if (keys->ov_buf[i] == NULL ||
		    vals->ov_buf[i] == NULL || rcs[i] != 0)
			break;

		/* Have retrieved all kv pairs for an layer. */
		 m0_clovis_composite_layer_idx_key_from_buf(
						&key, keys->ov_buf[i]);
		if (!m0_uint128_eq(&key.cek_layer_id, &layer->ccr_subobj))
			break;

		/* Add a new extent. */
		M0_ALLOC_PTR(ext);
		if (ext == NULL)
			return M0_ERR(-ENOMEM);
		ext->ce_id = key.cek_layer_id;
		ext->ce_off = key.cek_off;
		ext->ce_len = *(m0_bcount_t *)vals->ov_buf[i];

		/* The extents are in increasing order for `offset`. */
		cext_tlink_init_at_tail(ext, ext_list);
	}

	ext = cext_tlist_tail(ext_list);
	if (ext != NULL) {
		max_key->cek_layer_id = ext->ce_id;
		max_key->cek_off = ext->ce_off;
	}
	return M0_RC(i);
}

static int composite_layer_idx_scan(struct m0_clovis_composite_layer *layer,
				    bool is_wr_list)
{
	int                                        i;
	int                                       rc;
	int                                       nr_kvp;
	int                                       nr_exts;
	int                                      *rcs = NULL;
	uint32_t                                  flags = 0;
	struct m0_bufvec                         *keys;
	struct m0_bufvec                         *vals;
	struct m0_clovis_idx                      idx;
	struct m0_clovis_composite_extent        *ext;
	struct m0_tl                             *ext_list = NULL;
	struct m0_clovis_composite_layer_idx_key  start_key;
	struct m0_clovis_composite_layer_idx_key  max_key;

	M0_ENTRY();

	M0_ALLOC_PTR(keys);
	M0_ALLOC_PTR(vals);
	if (keys == NULL || vals == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto exit;
	}

	/* Allocate bufvec's for keys and vals. */
	nr_kvp = M0_CLOVIS_COMPOSITE_EXTENT_SCAN_BATCH;
	rc = m0_bufvec_empty_alloc(keys, nr_kvp)?:
	     m0_bufvec_empty_alloc(vals, nr_kvp);
	if (rc != 0) {
		rc = M0_ERR(-ENOMEM);
		goto exit;
	}
	M0_ALLOC_ARR(rcs, nr_kvp);
	if (rcs == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto exit;
	}

	/* Initialise the layer index to be queried. */
	rc = m0_clovis_composite_layer_idx(layer->ccr_subobj, is_wr_list, &idx);
	if (rc != 0)
		goto exit;
	ext_list = (is_wr_list == true)?&layer->ccr_wr_exts:&layer->ccr_rd_exts;

	/* Use NEXT op to scan the layer index. */
	start_key.cek_layer_id = layer->ccr_subobj;
	start_key.cek_off = 0;
	while(true) {
		/* Set key and then launch NEXT query. */
		rc = m0_clovis_composite_layer_idx_key_to_buf(
			&start_key, &keys->ov_buf[0],
			&keys->ov_vec.v_count[0])?:
		     composite_layer_idx_next_query(
			&idx, keys, vals, rcs, flags);
		if (rc != 0) {
			M0_ERR(rc);
			goto exit;
		}

		/* Extract extents and add them to the list. */
		nr_exts = composite_layer_idx_extents_extract(
				layer, keys, vals, rcs, ext_list, &max_key);
		if (nr_exts < 0)
			break;
		else if (nr_exts == 0)
			/*
 			 * This can happen when we just reach the
 			 * end of an index. Rest the keys and vals
 			 * to avoid m0_bufvec_free() to free `start_key`
 			 * variable.
 			 */
			break;
		else  if (nr_exts < nr_kvp)
			break;

		/* Reset keys and vals. */
		for (i = 0; i < nr_kvp; i++) {
			m0_free(keys->ov_buf[i]);
			keys->ov_buf[i] = NULL;
			keys->ov_vec.v_count[i] = 0;
			m0_free(vals->ov_buf[i]);
			vals->ov_buf[i] = NULL;
			vals->ov_vec.v_count[i] = 0;
		}

		/* Next round. */
		start_key = max_key;
		flags = M0_OIF_EXCLUDE_START_KEY;
	}

exit:
	m0_clovis_idx_fini(&idx);
	m0_bufvec_free(keys);
	m0_bufvec_free(vals);
	m0_free0(&rcs);
	m0_free(keys);
	m0_free(vals);
	if (rc != 0) {
		/* Cleanup the list if an error happens. */
		if (ext_list != NULL) {
			m0_tl_teardown(cext, ext_list, ext)
				m0_free(ext);
		}
	}
	return M0_RC(rc);
}

static int
composite_extents_scan_sync(struct m0_clovis_obj *obj,
			    struct m0_clovis_composite_layout *clayout)
{
	int                               rc = 0;
	struct m0_clovis_composite_layer *layer;

	m0_tl_for(clayer, &clayout->ccl_layers, layer) {
		/* Query and update write extent list. */
		rc = composite_layer_idx_scan(layer, true);
		if (rc != 0) {
			composite_layout_put(&clayout->ccl_layout);
			return M0_ERR(rc);
		}

		/* Query and update read extent list. */
		rc = composite_layer_idx_scan(layer, false);
		if (rc != 0) {
			composite_layout_put(&clayout->ccl_layout);
			return M0_ERR(rc);
		}
	} m0_tl_endfor;

	M0_ASSERT(rc == 0);
	return M0_RC(rc);
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
