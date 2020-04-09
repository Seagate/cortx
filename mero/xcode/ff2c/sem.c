/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 01-Jan-2012
 */

/**
   @addtogroup xcode

   @{
 */

#include <err.h>
#include <sysexits.h>
#include <stdio.h>                        /* asprintf */
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>                       /* malloc */
#include <string.h>                       /* memset, strrchr */

#include "xcode/ff2c/parser.h"
#include "xcode/ff2c/sem.h"

static void *alloc(size_t nr)
{
	void *data;

	data = malloc(nr);
	if (data == NULL)
		err(EX_TEMPFAIL, "cannot allocate %zu bytes.", nr);
	memset(data, 0, nr);
	return data;
}

__attribute__((format(printf, 1, 2)))
char *fmt(const char *format, ...)
{
	va_list  args;
	char    *out;

	va_start(args, format);
	if (vasprintf(&out, format, args) == -1)
		err(EX_TEMPFAIL, "cannot allocate string.");
	va_end(args);
	return out;
}

static void *add(struct ff2c_list *list, void *obj)
{
	return list->l_tail = *(list->l_head == NULL ?
				&list->l_head : (void **)list->l_tail) = obj;
}

static void *add_new(struct ff2c_list *list, size_t size)
{
	return add(list, alloc(size));
}

#define TOK(tok) (int)(tok)->ft_len, (int)(tok)->ft_len, (tok)->ft_val
#define T(term) TOK(&(term)->fn_tok)

void require_init(struct ff2c_ff *ff,
		  struct ff2c_require *r, const struct ff2c_term *term)
{
	char *buf = fmt("%*.*s    ", T(term));

	*strrchr(buf, '"') = 0;
	r->r_path = fmt("%s.h\"", buf);
	free(buf);
}

void type_init(struct ff2c_ff *ff, struct ff2c_type *t,
	       const struct ff2c_term *term);

static const char *name[][3] = {
	[FTT_VOID]   = { "void", "&M0_XT_VOID",   "m0_void_t" },
	[FTT_U8]     = { "u8",   "&M0_XT_U8",     "uint8_t"   },
	[FTT_U32]    = { "u32",  "&M0_XT_U32",    "uint32_t"  },
	[FTT_U64]    = { "u64",  "&M0_XT_U64",    "uint64_t"  },
	[FTT_OPAQUE] = { NULL,   "&M0_XT_OPAQUE", NULL        }
};

void field_init(struct ff2c_ff *ff,
		struct ff2c_type *t, struct ff2c_field *f, int i,
		const struct ff2c_term *term)
{
	const struct ff2c_term  *inner;
	const struct ff2c_term  *sub;
	const struct ff2c_token *tok;
	struct ff2c_type        *comp;
	const char              *ptr;

	inner = term->fn_head;
	assert(inner != NULL);
	tok = &inner->fn_tok;

	ptr = t->t_sequence && i > 0 ? "*" : "";
	t->t_nr++;
	f->f_parent = t;
	f->f_name   = fmt("%*.*s", T(term));
	f->f_c_name = fmt("%s%*.*s", t->t_union && i > 0 ? "u." : "", T(term));
	for (sub = term->fn_head; sub != NULL; sub = sub->fn_next) {
		if (sub->fn_type == FNT_TAG) {
			assert(f->f_tag == NULL);
			f->f_tag = fmt("%*.*s", T(sub));
		} else if (sub->fn_type == FNT_ESCAPE) {
			struct ff2c_escape *esc;

			assert(f->f_escape == NULL);
			f->f_escape = fmt("%*.*s", T(sub));
			esc = alloc(sizeof *esc);
			esc->e_escape = f->f_escape;
			add(&ff->ff_escape, esc);
		}
	}
	switch (inner->fn_type) {
	case FNT_ATOMIC:
		if (tok->ft_type == FTT_OPAQUE) {
			assert(f->f_escape != NULL);
			f->f_decl = fmt("struct %*.*s *%s%s",
					T(inner), ptr, f->f_name);
		} else {
			f->f_decl = fmt("%s %s%s",
					name[tok->ft_type][2], ptr, f->f_name);
		}
		break;
	case FNT_TYPENAME:
		f->f_decl = fmt("struct %*.*s %s%s", T(inner), ptr, f->f_name);
		break;
	case FNT_COMPOUND:
		for (comp = ff->ff_type.l_head;
		     comp != NULL; comp = comp->t_next) {
			if (comp->t_term == term) {
				f->f_type = comp;
				break;
			}
		}
		assert(f->f_type != NULL);
		break;
	default:
		assert("impossible" == NULL);
	}
	if (inner->fn_type == FNT_ATOMIC)
		f->f_xc_type = fmt("%s", name[tok->ft_type][1]);
	else if (inner->fn_type == FNT_COMPOUND)
		f->f_xc_type = fmt("%s", f->f_type->t_xc_name);
	else
		f->f_xc_type = fmt("%*.*s_xc", T(inner));
}

void type_init(struct ff2c_ff *ff, struct ff2c_type *t,
	       const struct ff2c_term *term)
{
	const struct ff2c_term *inner;
	const struct ff2c_term *grand;
	enum ff2c_token_type    itype;

	t->t_term = term;

	inner = term->fn_head;
	assert(term->fn_parent != NULL);
	assert(inner != NULL);
	itype = inner->fn_tok.ft_type;

	if (inner->fn_type == FNT_ATOMIC) {
		if (itype == FTT_OPAQUE) {
			t->t_name    = fmt("[@MUST NOT HAPPEN@]");
			t->t_xc_name = fmt("[@MUST NOT HAPPEN@]");
			t->t_c_name  = fmt("%*.*s", T(inner));
			t->t_opaque  = true;
		} else {
			t->t_name    = fmt("%s", name[itype][0]);
			t->t_xc_name = fmt("%s", name[itype][1]);
			t->t_c_name  = fmt("%s", name[itype][2]);
			t->t_atomic  = true;
		}
	} else {
		grand = term->fn_parent->fn_parent;
		t->t_public = grand == NULL;
		if (inner->fn_type == FNT_COMPOUND && grand != NULL)
			t->t_name = fmt("%*.*s_%*.*s", T(grand), T(term));
		else
			t->t_name = fmt("%*.*s", T(term));
		t->t_xc_name  = fmt("%s_xc", t->t_name);
		t->t_c_name   = fmt("struct %s", t->t_name);
		t->t_compound = true;
		switch (itype) {
		case FTT_SEQUENCE:
			t->t_sequence = true;
			break;
		case FTT_ARRAY:
			t->t_array = true;
			break;
		case FTT_UNION:
			t->t_union = true;
			break;
		case FTT_RECORD:
			t->t_record = true;
			break;
		case FTT_IDENTIFIER:
			break;
		default:
			assert("impossible" == NULL);
		}
	}
}

void tree_walk(struct ff2c_ff *ff, const struct ff2c_term *top)
{
	const struct ff2c_term *child;

	switch (top->fn_type) {
	case FNT_REQUIRE:
		require_init(ff, add_new(&ff->ff_require,
					 sizeof(struct ff2c_require)), top);
		break;
	case FNT_DECLARATION:
		if (top->fn_head->fn_type == FNT_COMPOUND) {
			type_init(ff, add_new(&ff->ff_type,
					      sizeof(struct ff2c_type)),
				  top);
		}
		break;
	default:
		break;
	}
	for (child = top->fn_head; child != NULL; child = child->fn_next)
		tree_walk(ff, child);
}

void ff2c_sem_init(struct ff2c_ff *ff, struct ff2c_term *top)
{
	struct ff2c_type *t;

	tree_walk(ff, top);

	for (t = ff->ff_type.l_head; t != NULL; t = t->t_next) {
		const struct ff2c_term *inner;
		const struct ff2c_term *f;
		int                     i;

		inner = t->t_term->fn_head;
		for(i = 0, f = inner->fn_head; f != NULL; f = f->fn_next, ++i) {
			field_init(ff, t, add_new(&t->t_field,
						  sizeof(struct ff2c_field)),
				   i, f);
		}
	}
}

void ff2c_sem_fini(struct ff2c_ff *ff)
{
}

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
