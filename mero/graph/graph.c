/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 22-Feb-2014
 */


/**
 * @addtogroup graph
 *
 * @{
 */

#include "lib/assert.h"
#include "lib/misc.h"        /* M0_SET0, ARRAY_SIZE */

#include "graph/graph.h"
#include "graph/graph_xc.h"

static struct m0_garc *arc_get(const struct m0_gvertice *src,
			       const struct m0_garc_type *atype);
static struct m0_gvertice *arc_try(const struct m0_gvertice *vertice,
				   const struct m0_garc_type *atype);

static void graph_add(struct m0_graph *g, struct m0_gvertice *vertice);
static void graph_del(struct m0_graph *g, struct m0_gvertice *vertice);

static struct m0_garc_type GRAPH_PREV;
static struct m0_garc_type GRAPH_NEXT;

static void vertice_init(struct m0_gvertice *vertice,
			 const struct m0_gvertice_type *vt,
			 const struct m0_fid *fid);

static bool has_arc(const struct m0_gvertice *vertice,
		    const struct m0_garc_type *atype);

static const struct m0_gvertice_type *vtype(const struct m0_gvertice *src);

enum { VTYPE_MAX = 256 };

static const struct m0_gvertice_type *vtypes[VTYPE_MAX];

void m0_gvertice_link(struct m0_gvertice *src, struct m0_gvertice *dst,
		      const struct m0_garc_type *atype)
{
	struct m0_garc *arc;

	M0_PRE(m0_gvertice_invariant(src));
	M0_PRE(m0_gvertice_invariant(dst));
	M0_PRE(has_arc(src, atype));
	M0_PRE(has_arc(dst, atype));
	M0_PRE(!m0_gvertice_is_set(src, atype));
	M0_PRE(ergo(atype->at_reverse != NULL,
		    !m0_gvertice_is_set(dst, atype->at_reverse)));

	arc = arc_get(src, atype);
	arc->as_fid = dst->vh_fid;
	m0_cookie_init(&arc->as_local, &dst->vh_gen);

	if (atype->at_reverse != NULL) {
		arc = arc_get(dst, atype->at_reverse);
		arc->as_fid = src->vh_fid;
		m0_cookie_init(&arc->as_local, &src->vh_gen);
	}

	M0_POST(m0_gvertice_invariant(src));
	M0_POST(m0_gvertice_invariant(dst));
	M0_POST(m0_gvertice_linked(src, dst, atype));
	M0_POST(ergo(atype->at_reverse != NULL,
		     m0_gvertice_linked(dst, src, atype->at_reverse)));
}

void m0_gvertice_unlink(struct m0_gvertice *src, struct m0_gvertice *dst,
			const struct m0_garc_type *atype)
{
	M0_PRE(m0_gvertice_invariant(src));
	M0_PRE(m0_gvertice_invariant(dst));
	M0_PRE(has_arc(src, atype));
	M0_PRE(has_arc(dst, atype));
	M0_PRE(m0_gvertice_linked(src, dst, atype));
	M0_PRE(ergo(atype->at_reverse != NULL,
		    m0_gvertice_linked(dst, src, atype->at_reverse)));

	M0_SET0(arc_get(src, atype));

	if (atype->at_reverse != NULL)
		M0_SET0(arc_get(dst, atype->at_reverse));

	M0_POST(m0_gvertice_invariant(src));
	M0_POST(m0_gvertice_invariant(dst));
	M0_POST(!m0_gvertice_is_set(src, atype));
	M0_POST(ergo(atype->at_reverse != NULL,
		     !m0_gvertice_is_set(dst, atype->at_reverse)));
}

bool m0_gvertice_linked(const struct m0_gvertice *src,
			const struct m0_gvertice *dst,
			const struct m0_garc_type *atype)
{
	M0_PRE(m0_gvertice_invariant(src));
	M0_PRE(m0_gvertice_invariant(dst));
	M0_PRE(has_arc(src, atype));
	M0_PRE(has_arc(dst, atype));

	return m0_fid_eq(&arc_get(src, atype)->as_fid, &dst->vh_fid);
}

bool m0_gvertice_is_set(const struct m0_gvertice *vertice,
			const struct m0_garc_type *atype)
{
	M0_PRE(m0_gvertice_invariant(vertice));
	M0_PRE(has_arc(vertice, atype));

	return m0_fid_is_set(&arc_get(vertice, atype)->as_fid);
}

void m0_gvertice_init(struct m0_graph *g, struct m0_gvertice *vertice,
		      const struct m0_gvertice_type *vt,
		      const struct m0_fid *fid)
{
	vertice_init(vertice, vt, fid);
	graph_add(g, vertice);
	M0_POST(m0_gvertice_invariant(vertice));
}

void m0_gvertice_fini(struct m0_graph *g, struct m0_gvertice *vertice)
{
	M0_PRE(m0_gvertice_invariant(vertice));
	graph_del(g, vertice);
	M0_POST(m0_forall(i, vtype(vertice)->vt_arc_nr,
			  !m0_gvertice_is_set(vertice,
					      vtype(vertice)->vt_arc[i])));
}

bool m0_gvertice_invariant(const struct m0_gvertice *vertice)
{
	const struct m0_gvertice_type *vt = vtype(vertice);
	return
		_0C(m0_gvertice_type_invariant(vt)) &&
		_0C(m0_forall(i, vt->vt_arc_nr,
			      m0_garc_type_invariant(vt, vt->vt_arc[i])));
}

bool m0_gvertice_type_invariant(const struct m0_gvertice_type *vt)
{
	return
		_0C(vtypes[vt->vt_id] == vt) &&
		_0C(vt->vt_xt->xct_aggr == M0_XA_RECORD) &&
		m0_forall(i, vt->vt_arc_nr,
			  _0C(m0_garc_type_invariant(vt, vt->vt_arc[i])));

}

bool m0_garc_type_invariant(const struct m0_gvertice_type *vt,
			    const struct m0_garc_type *atype)
{
	const struct m0_xcode_type *xt = vt->vt_xt;

	return
		_0C(atype->at_field < xt->xct_nr) &&
		_0C(xt->xct_child[atype->at_field].xf_type == m0_garc_xc);
}

bool m0_graph_invariant(const struct m0_graph *graph)
{
	return true;
}

struct m0_gvertice *m0_garc_try(const struct m0_gvertice *vertice,
				const struct m0_garc_type *atype)
{
	M0_PRE(m0_gvertice_invariant(vertice));
	return arc_try(vertice, atype);
}

void m0_gvertice_type_register(struct m0_gvertice_type *vt)
{
	M0_PRE(IS_IN_ARRAY(vt->vt_id, vtypes));
	M0_PRE(vtypes[vt->vt_id] == NULL);
	M0_PRE(vt->vt_arc_nr < ARRAY_SIZE(vt->vt_arc) - 1);
	vtypes[vt->vt_id] = vt;

	m0_garc_type_add(vt, &GRAPH_NEXT);
	m0_garc_type_add(vt, &GRAPH_PREV);

	M0_POST(m0_gvertice_type_invariant(vt));
}

void m0_garc_type_register(const struct m0_garc_type *atype)
{
}

void m0_garc_type_pair_register(struct m0_garc_type *direct,
				struct m0_garc_type *reverse)
{
	direct->at_reverse = reverse;
	reverse->at_reverse = direct;
}

void m0_garc_type_add(struct m0_gvertice_type *vt,
		      const struct m0_garc_type *atype)
{
	M0_PRE(vt->vt_arc_nr < ARRAY_SIZE(vt->vt_arc) - 1);
	vt->vt_arc[vt->vt_arc_nr++] = atype;
	M0_POST(m0_garc_type_invariant(vt, atype));
}

static struct m0_gvertice *arc_try(const struct m0_gvertice *vertice,
				   const struct m0_garc_type *atype)
{
	return m0_cookie_of(&arc_get(vertice, atype)->as_local,
			    struct m0_gvertice, vh_gen);
}

int m0_garc_follow(const struct m0_gvertice *vertice,
		   const struct m0_garc_type *atype, ...)
{
	M0_IMPOSSIBLE("Not implemented.");
	return 0;
}

static struct m0_garc *arc_get(const struct m0_gvertice *src,
			       const struct m0_garc_type *atype)
{
	return m0_xcode_addr(&M0_XCODE_OBJ(vtype(src)->vt_xt,
					   /* suppress const */
					   (struct m0_gvertice *)src),
			     atype->at_field, 0);
}

static void vertice_init(struct m0_gvertice *vertice,
			 const struct m0_gvertice_type *vt,
			 const struct m0_fid *fid)
{
	vertice->vh_typeid = vt->vt_id;
	vertice->vh_fid    = *fid;
	m0_cookie_new(&vertice->vh_gen);
}

static void graph_add(struct m0_graph *g, struct m0_gvertice *vertice)
{
	struct m0_gvertice *tail = m0_garc_try(&g->g_anchor, &GRAPH_PREV);

	M0_PRE(m0_graph_invariant(g));
	M0_PRE(tail != NULL);
	m0_gvertice_unlink(tail, &g->g_anchor, &GRAPH_NEXT);
	m0_gvertice_link(tail, vertice, &GRAPH_NEXT);
	m0_gvertice_link(vertice, &g->g_anchor, &GRAPH_NEXT);

	M0_POST(m0_garc_try(&g->g_anchor, &GRAPH_PREV) == vertice);
	M0_POST(m0_graph_invariant(g));
}

static void graph_del(struct m0_graph *g, struct m0_gvertice *vertice)
{
	struct m0_gvertice *prev = m0_garc_try(vertice, &GRAPH_PREV);
	struct m0_gvertice *next = m0_garc_try(vertice, &GRAPH_NEXT);

	M0_PRE(m0_graph_invariant(g));
	M0_PRE(prev != NULL);
	M0_PRE(next != NULL);
	m0_gvertice_unlink(vertice, prev, &GRAPH_PREV);
	m0_gvertice_unlink(vertice, next, &GRAPH_NEXT);
	m0_gvertice_link(prev, next, &GRAPH_NEXT);
	M0_POST(m0_graph_invariant(g));
}

static const struct m0_gvertice_type *vtype(const struct m0_gvertice *vertice)
{
	M0_PRE(IS_IN_ARRAY(vertice->vh_typeid, vtypes));
	return vtypes[vertice->vh_typeid];
}

static bool has_arc(const struct m0_gvertice *vertice,
		    const struct m0_garc_type *arc)
{
	const struct m0_gvertice_type *vt = vtype(vertice);
	return m0_exists(i, vt->vt_arc_nr, vt->vt_arc[i] == arc);
}

static struct m0_garc_type GRAPH_NEXT = {
	.at_name = "next in graph",
	.at_field = 1
};

static struct m0_garc_type GRAPH_PREV = {
	.at_name = "prev in graph",
	.at_field = 2
};

M0_INTERNAL int m0_graph_mod_init(void)
{
	m0_garc_type_register(&GRAPH_NEXT);
	m0_garc_type_register(&GRAPH_PREV);
	m0_garc_type_pair_register(&GRAPH_NEXT, &GRAPH_PREV);
	return 0;
}

M0_INTERNAL void m0_graph_mod_fini(void)
{;}

/** @} end of graph group */

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
