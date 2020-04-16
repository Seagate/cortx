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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 25-Dec-2011
 */

#include "lib/bob.h"
#include "lib/misc.h"                           /* M0_SET0 */
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/string.h"                         /* m0_streq */
#include "lib/arith.h"                          /* m0_align, max_check */
#include "xcode/xcode.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_XCODE
#include "lib/trace.h"

/**
   @addtogroup xcode

   @{
 */

static bool is_pointer(const struct m0_xcode_type *xt,
		       const struct m0_xcode_field *field)
{
	return xt->xct_aggr == M0_XA_SEQUENCE && field == &xt->xct_child[1];
}

/**
 * Returns "portable" alignment for a field, which would work on any
 * architecture.
 */
static unsigned alignment_mask(const struct m0_xcode_field *field)
{
	unsigned                    x;
	const struct m0_xcode_type *ft = field->xf_type;

	x = ft->xct_aggr == M0_XA_ATOM ? ft->xct_sizeof : 1;
	M0_POST(m0_is_po2(x));
	return x > 0 ? x - 1 : 0;
}

static bool field_invariant(const struct m0_xcode_type *xt,
			    const struct m0_xcode_field *field)
{
	return
		_0C(field->xf_name != NULL) && _0C(field->xf_type != NULL) &&
		_0C(ergo(xt == &M0_XT_OPAQUE, field->xf_opaque != NULL)) &&
		_0C(field->xf_offset +
		    (is_pointer(xt, field) ? sizeof(void *) :
		     field->xf_type->xct_sizeof) <= xt->xct_sizeof) &&
		/* check that alignment is portable. */
		_0C((field->xf_offset & alignment_mask(field)) == 0);
}

bool m0_xcode_type_invariant(const struct m0_xcode_type *xt)
{
	size_t   prev   = 0;
	uint32_t offset = 0;

	static const size_t min[M0_XA_NR] = {
		[M0_XA_RECORD]   = 0,
		[M0_XA_UNION]    = 2,
		[M0_XA_SEQUENCE] = 2,
		[M0_XA_ARRAY]    = 1,
		[M0_XA_TYPEDEF]  = 1,
		[M0_XA_OPAQUE]   = 0,
		[M0_XA_ATOM]     = 0
	};

	static const size_t max[M0_XA_NR] = {
		[M0_XA_RECORD]   = ~0ULL,
		[M0_XA_UNION]    = ~0ULL,
		[M0_XA_SEQUENCE] = 2,
		[M0_XA_ARRAY]    = 1,
		[M0_XA_TYPEDEF]  = 1,
		[M0_XA_OPAQUE]   = 0,
		[M0_XA_ATOM]     = 0
	};

	return
		_0C(0 <= xt->xct_aggr) &&
		_0C(xt->xct_aggr < M0_XA_NR) &&
		_0C(xt->xct_nr >= min[xt->xct_aggr]) &&
		_0C(xt->xct_nr <= max[xt->xct_aggr]) &&
		m0_forall(i, xt->xct_nr, ({
			const struct m0_xcode_field *f = &xt->xct_child[i];

			field_invariant(xt, f) &&
			/* field doesn't overlap with the previous one */
			_0C(i == 0 || offset +
			    xt->xct_child[prev].xf_type->xct_sizeof <=
			    f->xf_offset) &&
			/* field names are unique */
			m0_forall(j, xt->xct_nr,
				  m0_streq(f->xf_name,
					   xt->xct_child[j].xf_name) ==
				  (i == j)) &&
			/* union tags are unique. */
			_0C(ergo(xt->xct_aggr == M0_XA_UNION && i > 0,
			     m0_forall(j, xt->xct_nr,
				ergo(j > 0,
				     (f->xf_tag == xt->xct_child[j].xf_tag) ==
				     (i == j))))) &&
			/* update the previous field offset: for UNION all
			   branches follow the first field. */
			_0C(ergo(i == 0 || xt->xct_aggr != M0_XA_UNION,
				 ({ offset = f->xf_offset;
				    prev = i;
				    true; }) )); }) ) &&
		_0C(ergo(M0_IN(xt->xct_aggr, (M0_XA_UNION, M0_XA_SEQUENCE)),
			 xt->xct_child[0].xf_type->xct_aggr == M0_XA_ATOM)) &&
		_0C(ergo(xt->xct_aggr == M0_XA_OPAQUE,
			 xt == &M0_XT_OPAQUE &&
			 xt->xct_sizeof == sizeof (void *))) &&
		_0C(ergo(xt->xct_aggr == M0_XA_ATOM,
			 0 <= xt->xct_atype && xt->xct_atype < M0_XAT_NR));
}
M0_EXPORTED(m0_xcode_type_invariant);

#include "xcode/cursor.c"

enum xcode_op {
	XO_ENC,
	XO_DEC,
	XO_LEN,
	XO_NR
};

static bool at_array(const struct m0_xcode_cursor       *it,
		     const struct m0_xcode_cursor_frame *prev,
		     const struct m0_xcode_obj          *par)
{
	return it->xcu_depth > 0 && par->xo_type->xct_aggr == M0_XA_SEQUENCE &&
	       prev->s_fieldno == 1 && prev->s_elno == 0 &&
	       m0_xcode_tag(par) > 0;
}

static void **allocp(struct m0_xcode_cursor *it, size_t *out)
{
	const struct m0_xcode_cursor_frame *prev;
	const struct m0_xcode_obj          *par;
	const struct m0_xcode_type         *xt;
	const struct m0_xcode_type         *pt;
	struct m0_xcode_cursor_frame       *top;
	struct m0_xcode_obj                *obj;
	size_t                              nob;
	size_t                              size;
	void                              **slot;

	/*
	 * New memory has to be allocated in 3 cases:
	 *
	 * - to decode topmost object (this is different from sunrpc XDR
	 *   interfaces, where topmost object is pre-allocated by the caller);
	 *
	 * - to store an array: a SEQUENCE object has the following in-memory
	 *   structure:
	 *
	 *       struct  {
	 *               scalar_t     count;
	 *               struct elem *data;
	 *
	 *       };
	 *
	 *   This function allocates count * sizeof(struct elem) bytes to hold
	 *   "data";
	 *
	 * - to store an object pointed to by an opaque pointer.
	 *
	 */

	nob  = 0;
	top  = m0_xcode_cursor_top(it);
	prev = top - 1;
	obj  = &top->s_obj;  /* an object being decoded */
	par  = &prev->s_obj; /* obj's parent object */
	xt   = obj->xo_type;
	pt   = par->xo_type;
	size = xt->xct_sizeof;

	if (it->xcu_depth == 0) {
		/* allocate top-most object */
		nob = size;
		slot = &obj->xo_ptr;
	} else {
		if (at_array(it, prev, par))
			/* allocate array */
			nob = m0_xcode_tag(par) * size;
		else if (pt->xct_child[prev->s_fieldno].xf_type == &M0_XT_OPAQUE)
			/*
			 * allocate the object referenced by an opaque
			 * pointer. At this moment "xt" is the type of the
			 * pointed object.
			 */
			nob = size;
		slot = m0_xcode_addr(par, prev->s_fieldno, ~0ULL);
	}
	*out = nob;
	return slot;
}

M0_INTERNAL bool m0_xcode_is_byte_array(const struct m0_xcode_type *xt)
{
	return xt->xct_aggr == M0_XA_SEQUENCE &&
		xt->xct_child[1].xf_type == &M0_XT_U8;
}

M0_INTERNAL ssize_t
m0_xcode_alloc_obj(struct m0_xcode_cursor *it,
		   void *(*alloc)(struct m0_xcode_cursor *, size_t))
{
	struct m0_xcode_obj  *obj;
	size_t                nob = 0;
	void                **slot;

	obj = &m0_xcode_cursor_top(it)->s_obj;  /* an object being decoded */

	slot = allocp(it, &nob);
	if (nob != 0 && *slot == NULL) {
		M0_ASSERT(obj->xo_ptr == NULL);

		obj->xo_ptr = *slot = alloc(it, nob);
		if (obj->xo_ptr == NULL)
			return M0_ERR(-ENOMEM);
	}
	return 0;
}

M0_INTERNAL void m0_xcode_free_obj(struct m0_xcode_obj *obj)
{
	struct m0_xcode_ctx ctx;

	M0_SET0(&ctx);
	m0_xcode_ctx_init(&ctx, obj);
	m0_xcode_free(&ctx);
}

/**
   Common xcoding function, implementing encoding, decoding and sizing.
 */
static int ctx_walk(struct m0_xcode_ctx *ctx, enum xcode_op op)
{
	void                   *ptr;
	m0_bcount_t             size;
	int                     length = 0;
	int                     result;
	struct m0_bufvec        area   = M0_BUFVEC_INIT_BUF(&ptr, &size);
	struct m0_bufvec_cursor mem;
	struct m0_xcode_cursor *it     = &ctx->xcx_it;

	M0_PRE(M0_IN(op, (XO_ENC, XO_DEC, XO_LEN)));

	while ((result = m0_xcode_next(it)) > 0) {
		const struct m0_xcode_type     *xt;
		const struct m0_xcode_type_ops *ops;
		struct m0_xcode_obj            *cur;
		struct m0_xcode_cursor_frame   *top;

		top = m0_xcode_cursor_top(it);

		if (top->s_flag != M0_XCODE_CURSOR_PRE)
			continue;

		cur = &top->s_obj;

		if (op == XO_DEC) {
			result = m0_xcode_alloc_obj(it, ctx->xcx_alloc);
			if (result != 0)
				break;
		}

		if (ctx->xcx_iter != NULL) {
			result = ctx->xcx_iter(it);
			if (result != 0)
				break;
		}

		xt  = cur->xo_type;
		ptr = cur->xo_ptr;
		ops = xt->xct_ops;

		if (ops != NULL &&
		    ((op == XO_ENC && ops->xto_encode != NULL) ||
		     (op == XO_DEC && ops->xto_decode != NULL) ||
		     (op == XO_LEN && ops->xto_length != NULL))) {
			switch (op) {
			case XO_ENC:
				result = ops->xto_encode(ctx, ptr);
				break;
			case XO_DEC:
				result = ops->xto_decode(ctx, ptr);
				break;
			case XO_LEN:
				length += ops->xto_length(ctx, ptr);
				break;
			default:
				M0_IMPOSSIBLE("op");
			}
			m0_xcode_skip(it);
		} else if (xt->xct_aggr == M0_XA_ATOM) {
			struct m0_xcode_cursor_frame *prev = top - 1;
			struct m0_xcode_obj          *par  = &prev->s_obj;
			bool array = at_array(it, prev, par) &&
				m0_xcode_is_byte_array(par->xo_type);

			size = xt->xct_sizeof;
			if (array)
				size *= m0_xcode_tag(par);

			if (op == XO_LEN)
				length += size;
			else {
				struct m0_bufvec_cursor *src;
				struct m0_bufvec_cursor *dst;

				m0_bufvec_cursor_init(&mem, &area);
				/* XXX endianness and sharing */
				switch (op) {
				case XO_ENC:
					src = &mem;
					dst = &ctx->xcx_buf;
					break;
				case XO_DEC:
					dst = &mem;
					src = &ctx->xcx_buf;
					break;
				default:
					M0_IMPOSSIBLE("op");
					src = dst = 0;
					break;
				}
				if (m0_bufvec_cursor_copy(dst,
							  src, size) != size)
					result = -EPROTO;
			}
			if (array) {
				it->xcu_depth--;
				m0_xcode_skip(it);
			}
		}
		if (result < 0)
			break;
	}

	if (ctx->xcx_iter_end != NULL)
		ctx->xcx_iter_end(it);

	if (op == XO_LEN)
		result = result ?: length;

	return result;
}

M0_INTERNAL void m0_xcode_ctx_init(struct m0_xcode_ctx *ctx,
				   const struct m0_xcode_obj *obj)
{
	M0_SET0(ctx);
	m0_xcode_cursor_init(&ctx->xcx_it, obj);
}

M0_INTERNAL int m0_xcode_decode(struct m0_xcode_ctx *ctx)
{
	return ctx_walk(ctx, XO_DEC);
}

M0_INTERNAL int m0_xcode_encode(struct m0_xcode_ctx *ctx)
{
	return ctx_walk(ctx, XO_ENC);
}

M0_INTERNAL int m0_xcode_length(struct m0_xcode_ctx *ctx)
{
	return ctx_walk(ctx, XO_LEN);
}

M0_INTERNAL void m0_xcode_type_iterate(struct m0_xcode_type *xt,
				       void (*t)(struct m0_xcode_type *,
						 void *),
				       void (*f)(struct m0_xcode_type *,
						 struct m0_xcode_field *,
						 void *), void *datum)
{
	int i;

	if (t != NULL)
		(*t)(xt, datum);
	for (i = 0; i < xt->xct_nr; ++i) {
		struct m0_xcode_field *field = &xt->xct_child[i];

		if (f != NULL)
			(*f)(xt, field, datum);
		/* Discard const. Not good. */
		m0_xcode_type_iterate((void *)field->xf_type, t, f, datum);
	}
}

M0_INTERNAL int m0_xcode_encdec(struct m0_xcode_obj *obj,
				struct m0_bufvec_cursor *cur,
				enum m0_xcode_what what)
{
	struct m0_xcode_ctx ctx;
	int                 result;

	m0_xcode_ctx_init(&ctx, obj);
	ctx.xcx_buf   = *cur;
	ctx.xcx_alloc = m0_xcode_alloc;

	result = what == M0_XCODE_ENCODE ? m0_xcode_encode(&ctx) :
					   m0_xcode_decode(&ctx);
	if (result == 0) {
		*cur = ctx.xcx_buf;
		if (obj->xo_ptr == NULL)
			obj->xo_ptr = m0_xcode_ctx_top(&ctx);
	}
	return result;
}

M0_INTERNAL int m0_xcode_data_size(struct m0_xcode_ctx *ctx,
				   const struct m0_xcode_obj *obj)
{
	m0_xcode_ctx_init(ctx, obj);
	return m0_xcode_length(ctx);
}

M0_INTERNAL void *m0_xcode_alloc(struct m0_xcode_cursor *it, size_t nob)
{
	return m0_alloc(nob);
}

static void __xcode_free(struct m0_xcode_cursor *it)
{
	struct m0_xcode_cursor_frame *top = m0_xcode_cursor_top(it);
	size_t                        nob = 0;
	void                        **slot;

	slot = allocp(it, &nob);
	if (top->s_datum != 0) {
		m0_free((void *) top->s_datum);
		top->s_datum = 0;
	} else if (nob != 0)
		m0_free(*slot);
}

/**
 * Frees xcode object and its sub-objects.
 *
 * Descends through sub-objects tree, freeing memory allocated during tree
 * construction. The same algorithm (allocp()) is used to determine what to
 * allocate and what to free.
 *
 * Actions are done in post-order: the parent is freed after all children are
 * done with.
 *
 * This is the only xcode function that has to deal with partially constructed
 * objects.
 */
M0_INTERNAL void m0_xcode_free(struct m0_xcode_ctx *ctx)
{
	struct m0_xcode_cursor *it;

	it = &ctx->xcx_it;
	if (ctx->xcx_free == NULL)
		ctx->xcx_free = __xcode_free;
	while (m0_xcode_next(it) > 0) {
		struct m0_xcode_cursor_frame *top    = m0_xcode_cursor_top(it);
		size_t                        nob    = 0;
		struct m0_xcode_cursor_frame *prev   = top -1;
		struct m0_xcode_obj          *par    = &prev->s_obj;
		bool                          arrayp = at_array(it, prev, par);
		void                        **slot;

		if (top->s_flag == M0_XCODE_CURSOR_POST) {
			slot = allocp(it, &nob);
			if (top->s_datum != 0) {
				ctx->xcx_free(it);
				top->s_datum = 0;
			}
			if (arrayp)
				/*
				 * Store the address of allocated array in the
				 * parent stack frame.
				 */
				prev->s_datum = (uint64_t)*slot;
			else if (nob != 0)
				ctx->xcx_free(it);
		} else if (top->s_flag == M0_XCODE_CURSOR_PRE) {
			/*
			 * Deal with partially constructed objects.
			 */
			if (top->s_obj.xo_ptr == NULL) {
				if (arrayp)
					/*
					 * If array allocation failed, skip the
					 * array entirely.
					 */
					--it->xcu_depth;
				m0_xcode_skip(it);
			}
		}
	}
}

M0_INTERNAL int m0_xcode_dup(struct m0_xcode_ctx *dest,
			     struct m0_xcode_ctx *src)
{
	struct m0_xcode_cursor  *dit;
	struct m0_xcode_cursor  *sit;
	int                      result;

	dit = &dest->xcx_it;
	sit = &src->xcx_it;
	M0_ASSERT(m0_xcode_cursor_top(dit)->s_obj.xo_type ==
		  m0_xcode_cursor_top(sit)->s_obj.xo_type);

	while ((result = m0_xcode_next(sit)) > 0) {
		struct m0_xcode_cursor_frame *sf;
		struct m0_xcode_cursor_frame *df;
		struct m0_xcode_obj          *sobj;
		struct m0_xcode_obj          *dobj;
		const struct m0_xcode_type   *xt;

		result = m0_xcode_next(dit);
		M0_ASSERT(result > 0);

		sf = m0_xcode_cursor_top(sit);
		df = m0_xcode_cursor_top(dit);
		M0_ASSERT(sf->s_flag == df->s_flag);
		sobj = &sf->s_obj;
		dobj = &df->s_obj;
		xt = sobj->xo_type;
		M0_ASSERT(xt == dobj->xo_type);

		if (sf->s_flag != M0_XCODE_CURSOR_PRE)
			continue;

		result = m0_xcode_alloc_obj(dit, dest->xcx_alloc == NULL ?
						 m0_xcode_alloc :
						 dest->xcx_alloc);
		if (result != 0)
			return result;

		if (xt->xct_aggr == M0_XA_ATOM) {
			M0_ASSERT(dobj->xo_ptr != NULL);
			memcpy(dobj->xo_ptr, sobj->xo_ptr, xt->xct_sizeof);
		}
	}

	M0_POST(ergo(result == 0, m0_xcode_cmp(&dit->xcu_stack[0].s_obj,
					       &sit->xcu_stack[0].s_obj) == 0));
	return result;
}

M0_INTERNAL int m0_xcode_cmp(const struct m0_xcode_obj *o0,
			     const struct m0_xcode_obj *o1)
{
	int                    result;
	struct m0_xcode_cursor it0;
	struct m0_xcode_cursor it1;

	M0_PRE(o0->xo_type == o1->xo_type);

	m0_xcode_cursor_init(&it0, o0);
	m0_xcode_cursor_init(&it1, o1);

	while ((result = m0_xcode_next(&it0)) > 0) {
		struct m0_xcode_cursor_frame *t0;
		struct m0_xcode_cursor_frame *t1;
		struct m0_xcode_obj          *s0;
		struct m0_xcode_obj          *s1;
		const struct m0_xcode_type   *xt;

		result = m0_xcode_next(&it1);
		M0_ASSERT(result > 0);

		t0 = m0_xcode_cursor_top(&it0);
		t1 = m0_xcode_cursor_top(&it1);
		M0_ASSERT(t0->s_flag == t1->s_flag);
		s0 = &t0->s_obj;
		s1 = &t1->s_obj;
		xt = s0->xo_type;
		M0_ASSERT(xt == s1->xo_type);

		if (t0->s_flag == M0_XCODE_CURSOR_PRE &&
		    xt->xct_aggr == M0_XA_ATOM) {
			result = memcmp(s0->xo_ptr, s1->xo_ptr, xt->xct_sizeof);
			if (result != 0)
				return result;
		}
	}
	return 0;
}

M0_INTERNAL void *m0_xcode_addr(const struct m0_xcode_obj *obj, int fileno,
				uint64_t elno)
{
	char                        *addr = (char *)obj->xo_ptr;
	const struct m0_xcode_type  *xt   = obj->xo_type;
	const struct m0_xcode_field *f    = &xt->xct_child[fileno];
	const struct m0_xcode_type  *ct   = f->xf_type;

	M0_ASSERT(fileno < xt->xct_nr);
	addr += f->xf_offset;
	if (xt->xct_aggr == M0_XA_SEQUENCE && fileno == 1 && elno != ~0ULL)
		addr = *((char **)addr) + elno * ct->xct_sizeof;
	else if (xt->xct_aggr == M0_XA_ARRAY && fileno == 0)
		addr += elno * ct->xct_sizeof;
	else if (ct == &M0_XT_OPAQUE && elno != ~0ULL)
		addr = *((char **)addr);
	return addr;
}

M0_INTERNAL int m0_xcode_subobj(struct m0_xcode_obj *subobj,
				const struct m0_xcode_obj *obj, int fieldno,
				uint64_t elno)
{
	const struct m0_xcode_field *f;
	int                          result;

	M0_PRE(0 <= fieldno && fieldno < obj->xo_type->xct_nr);

	f = &obj->xo_type->xct_child[fieldno];

	subobj->xo_ptr = m0_xcode_addr(obj, fieldno, elno);
	if (f->xf_type == &M0_XT_OPAQUE) {
		result = f->xf_opaque(obj, &subobj->xo_type);
	} else {
		subobj->xo_type = f->xf_type;
		result = 0;
	}
	return result;
}

M0_INTERNAL uint64_t m0_xcode_atom(const struct m0_xcode_obj *obj)
{
	const struct m0_xcode_type  *xt  = obj->xo_type;
	void                        *ptr = obj->xo_ptr;
	uint64_t                     val;

	M0_PRE(xt->xct_aggr == M0_XA_ATOM);

	switch (xt->xct_atype) {
	case M0_XAT_U8:
		val = *(uint8_t *)ptr;
		break;
	case M0_XAT_U32:
		val = *(uint32_t *)ptr;
		break;
	case M0_XAT_U64:
		val = *(uint64_t *)ptr;
		break;
	case M0_XAT_VOID:
	default:
		M0_IMPOSSIBLE("value of void");
		val = 0;
		break;
	}
	return val;
}

M0_INTERNAL uint64_t m0_xcode_tag(const struct m0_xcode_obj *obj)
{
	const struct m0_xcode_type  *xt = obj->xo_type;
	const struct m0_xcode_field *f  = &xt->xct_child[0];
	uint64_t                     tag;

	M0_PRE(M0_IN(xt->xct_aggr, (M0_XA_SEQUENCE, M0_XA_ARRAY, M0_XA_UNION)));

	if (xt->xct_aggr != M0_XA_ARRAY) {
		M0_PRE(f->xf_type->xct_aggr == M0_XA_ATOM);

		switch (f->xf_type->xct_atype) {
		case M0_XAT_VOID:
			tag = f->xf_tag;
			break;
		case M0_XAT_U8:
		case M0_XAT_U32:
		case M0_XAT_U64: {
			struct m0_xcode_obj subobj;

			m0_xcode_subobj(&subobj, obj, 0, 0);
			tag = m0_xcode_atom(&subobj);
			break;
		}
		default:
			M0_IMPOSSIBLE("atype");
			tag = 0;
			break;
		}
	} else
		tag = f->xf_tag;
	return tag;
}

M0_INTERNAL int m0_xcode_find(struct m0_xcode_obj *obj,
			      const struct m0_xcode_type *xt, void **place)
{
	struct m0_xcode_cursor it;
	int                    result;

	m0_xcode_cursor_init(&it, obj);
	while ((result = m0_xcode_next(&it)) > 0) {
		struct m0_xcode_obj *cur = &m0_xcode_cursor_top(&it)->s_obj;

		if (cur->xo_type == xt) {
			*place = cur->xo_ptr;
			return 0;
		}
	}
	return result ?: -ENOENT;
}

M0_INTERNAL void m0_xcode_bob_type_init(struct m0_bob_type *bt,
					const struct m0_xcode_type *xt,
					size_t magix_field, uint64_t magix)
{
	const struct m0_xcode_field *mf = &xt->xct_child[magix_field];

	M0_PRE(magix_field < xt->xct_nr);
	M0_PRE(xt->xct_aggr == M0_XA_RECORD);
	M0_PRE(mf->xf_type == &M0_XT_U64);

	bt->bt_name         = xt->xct_name;
	bt->bt_magix        = magix;
	bt->bt_magix_offset = mf->xf_offset;
}

M0_INTERNAL void *m0_xcode_ctx_top(const struct m0_xcode_ctx *ctx)
{
	return ctx->xcx_it.xcu_stack[0].s_obj.xo_ptr;
}

M0_INTERNAL void m0_xcode_union_init(struct m0_xcode_type *un, const char *name,
				     const char *discriminator,
				     size_t maxbranches)
{
	*un = (typeof(*un)) {
		.xct_aggr  = M0_XA_UNION,
		.xct_name  = name,
		.xct_nr    = maxbranches + 1
	};
	/* Cannot put this in the initialiser above, because
	   m0_xcode_type::xct_child[] is declared to have 0 elements. */
	un->xct_child[0] = (struct m0_xcode_field) {
		.xf_name = discriminator,
		.xf_type = &M0_XT_U64
	};
}

M0_INTERNAL void m0_xcode_union_fini(struct m0_xcode_type *un)
{
	int i;
	for (i = 0; i < un->xct_nr; ++i)
		un->xct_child[i].xf_name = NULL;
	*un = (typeof(*un)) {
		.xct_aggr  = 0,
		.xct_name  = NULL,
		.xct_nr    = 0
	};
}

M0_INTERNAL void m0_xcode_union_add(struct m0_xcode_type *un, const char *name,
				    const struct m0_xcode_type *xt,
				    uint64_t tag)
{
	int                    i;
	struct m0_xcode_field *f = NULL; /* "may be used uninitialized" */

	M0_PRE(un->xct_aggr == M0_XA_UNION);
	M0_PRE(un->xct_child[0].xf_name != NULL);
	M0_PRE(un->xct_child[0].xf_type != NULL);

	for (i = 1; i < un->xct_nr; ++i) {
		f = &un->xct_child[i];
		if (f->xf_name == NULL)
			break;
	}
	M0_ASSERT(i < un->xct_nr);
	*f = (typeof(*f)) {
		.xf_name   = name,
		.xf_type   = xt,
		.xf_tag    = tag,
		.xf_offset = un->xct_child[0].xf_type->xct_sizeof
	};
}

M0_INTERNAL void m0_xcode_union_close(struct m0_xcode_type *un)
{
	int                    i;
	struct m0_xcode_field *f;
	size_t                 maxsize = 0;

	M0_PRE(un->xct_aggr == M0_XA_UNION);

	for (i = 1; i < un->xct_nr; ++i) {
		f = &un->xct_child[i];
		if (f->xf_name != NULL)
			/*
			 * If we ever maintain ->xct_alignof, it can be used to
			 * calculate padding between the discriminator and union
			 * branches.
			 */
			maxsize = max_check(maxsize, f->xf_type->xct_sizeof);
		else
			break;
	}
	M0_ASSERT(i < un->xct_nr);
	un->xct_nr = i;
	un->xct_sizeof = m0_align(un->xct_child[0].xf_type->xct_sizeof +
				  maxsize, 16);
	M0_POST(m0_xcode_type_invariant(un));
}

M0_INTERNAL int m0_xcode_obj_enc_to_buf(struct m0_xcode_obj  *obj,
					void                **buf,
					m0_bcount_t          *len)
{
	struct m0_xcode_ctx     ctx;
	struct m0_bufvec        val;
	struct m0_bufvec_cursor cur;

	M0_PRE(obj != NULL);
	*len = m0_xcode_data_size(&ctx, obj);
	*buf = m0_alloc(*len);
	if (*buf == NULL)
		return M0_ERR(-ENOMEM);
	val  = M0_BUFVEC_INIT_BUF(buf, len);
	m0_bufvec_cursor_init(&cur, &val);
	return m0_xcode_encdec(obj, &cur, M0_XCODE_ENCODE);
}

M0_INTERNAL int m0_xcode_obj_dec_from_buf(struct m0_xcode_obj  *obj,
					  void                 *buf,
					  m0_bcount_t           len)
{
	struct m0_bufvec        val;
	struct m0_bufvec_cursor cur;

	M0_PRE(obj != NULL);
	val = M0_BUFVEC_INIT_BUF(&buf, &len);
	m0_bufvec_cursor_init(&cur, &val);
	return m0_xcode_encdec(obj, &cur, M0_XCODE_DECODE);
}

struct flags_data {
	uint32_t fd_on;
	uint32_t fd_off;
	uint64_t fd_aggr_umask;
	bool     fd_ok;
};

static void xcode_flags_check(struct m0_xcode_type *xt, struct flags_data *fd)
{
	if ((M0_BITS(xt->xct_aggr) & fd->fd_aggr_umask) == 0) {
		fd->fd_ok &= (xt->xct_flags & fd->fd_on)  == fd->fd_on;
		fd->fd_ok &= (xt->xct_flags & fd->fd_off) == 0;
	}
}

M0_INTERNAL bool m0_xcode_type_flags(struct m0_xcode_type *xt,
				     uint32_t on, uint32_t off,
				     uint64_t aggr_umask)
{
	struct flags_data fd = {
		.fd_on         = on,
		.fd_off        = off,
		.fd_aggr_umask = aggr_umask,
		.fd_ok         = true
	};
	m0_xcode_type_iterate(xt, (void *)&xcode_flags_check, NULL, &fd);
	return fd.fd_ok;
}

void m0_xc_u8_init(void)
{
}

void m0_xc_u16_init(void)
{
}

void m0_xc_u32_init(void)
{
}

void m0_xc_u64_init(void)
{
}

void m0_xc_void_init(void)
{
}

void m0_xc_opaque_init(void)
{
}

const struct m0_xcode_type M0_XT_VOID = {
	.xct_aggr   = M0_XA_ATOM,
	.xct_name   = "void",
	.xct_atype  = M0_XAT_VOID,
	.xct_sizeof = 0,
	.xct_nr     = 0
};

const struct m0_xcode_type M0_XT_U8 = {
	.xct_aggr   = M0_XA_ATOM,
	.xct_name   = "u8",
	.xct_atype  = M0_XAT_U8,
	.xct_sizeof = sizeof(uint8_t),
	.xct_nr     = 0
};

const struct m0_xcode_type M0_XT_U32 = {
	.xct_aggr   = M0_XA_ATOM,
	.xct_name   = "u32",
	.xct_atype  = M0_XAT_U32,
	.xct_sizeof = sizeof(uint32_t),
	.xct_nr     = 0
};

const struct m0_xcode_type M0_XT_U64 = {
	.xct_aggr   = M0_XA_ATOM,
	.xct_name   = "u64",
	.xct_atype  = M0_XAT_U64,
	.xct_sizeof = sizeof(uint64_t),
	.xct_nr     = 0
};

const struct m0_xcode_type M0_XT_OPAQUE = {
	.xct_aggr   = M0_XA_OPAQUE,
	.xct_name   = "opaque",
	.xct_sizeof = sizeof (void *),
	.xct_nr     = 0
};

const char *m0_xcode_aggr_name[M0_XA_NR] = {
	[M0_XA_RECORD]   = "record",
	[M0_XA_UNION]    = "union",
	[M0_XA_SEQUENCE] = "sequence",
	[M0_XA_ARRAY]    = "array",
	[M0_XA_TYPEDEF]  = "typedef",
	[M0_XA_OPAQUE]   = "opaque",
	[M0_XA_ATOM]     = "atom"
};

const char *m0_xcode_atom_type_name[M0_XAT_NR] = {
	[M0_XAT_VOID] = "void",
	[M0_XAT_U8]   = "u8",
	[M0_XAT_U32]  = "u32",
	[M0_XAT_U64]  = "u64",
};

const char *m0_xcode_endianness_name[M0_XEND_NR] = {
	[M0_XEND_LE] = "le",
	[M0_XEND_BE] = "be"
};

const char *m0_xcode_cursor_flag_name[M0_XCODE_CURSOR_NR] = {
	[M0_XCODE_CURSOR_NONE] = "none",
	[M0_XCODE_CURSOR_PRE]  = "pre",
	[M0_XCODE_CURSOR_IN]   = "in",
	[M0_XCODE_CURSOR_POST] = "post"
};

#undef M0_TRACE_SUBSYSTEM

/** @} end of xcode group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
