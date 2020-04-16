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
 * Original author: Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original creation date: 08/17/2011
 * Revision              : Manish Honap <Manish_Honap@xyratex.com>
 * Revision date         : 07/31/2012
 */

#include "lib/memory.h"		  /* M0_ALLOC_ARR */
#include "fop/fop.h"		  /* m0_fop */

#include "console/console.h"	  /* m0_console_verbose */
#include "console/console_it.h"
#include "console/console_yaml.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONSOLE
#include "lib/trace.h"

/**
   @addtogroup console_it
   @{
 */

bool m0_console_verbose;

static void depth_print(int depth)
{
	static const char ruler[] = "\t\t\t\t\t\t\t\t\t\t";
	if (m0_console_verbose)
		printf("%*.*s", depth, depth, ruler);
}

static void default_show(const struct m0_xcode_type *xct,
			 const char *name, void *data)
{
	printf("%s:%s\n", name, xct->xct_name);
}

static void void_get(const struct m0_xcode_type *xct,
		     const char *name, void *data)
{
}

static void void_set(const struct m0_xcode_type *xct,
		     const char *name, void *data)
{
}

static void byte_get(const struct m0_xcode_type *xct,
		     const char *name, void *data)
{
	if (m0_console_verbose)
		printf("%s(%s) = %s\n", name, xct->xct_name, (char *)data);
}

static void *cons_yaml_get_unsafe(const char *name)
{
	void *result = m0_cons_yaml_get_value(name);
	M0_ASSERT_INFO(result != NULL, "name=`%s'", name);
	return result;
}

static void byte_set(const struct m0_xcode_type *xct,
		     const char *name, void *data)
{
	void *tmp_value;
	char  value;

	if (yaml_support) {
		tmp_value = cons_yaml_get_unsafe(name);
		strncpy(data, tmp_value, strlen(tmp_value));
		if (m0_console_verbose)
			printf("%s(%s) = %s\n", name, xct->xct_name,
			       (char *)data);
	} else {
		printf("%s(%s) = ", name, xct->xct_name);
		if (scanf("\r%c", &value) != EOF)
			*(char *)data = value;
	}
}

static void u32_get(const struct m0_xcode_type *xct,
		    const char *name, void *data)
{
	if (m0_console_verbose)
		printf("%s(%s) = %d\n", name, xct->xct_name, *(uint32_t *)data);
}

static void u32_set(const struct m0_xcode_type *xct,
		    const char *name, void *data)
{
	uint32_t value;
	void    *tmp_value;

	if (yaml_support) {
		tmp_value = cons_yaml_get_unsafe(name);
		*(uint32_t *)data = atoi((const char *)tmp_value);
		if (m0_console_verbose)
			printf("%s(%s) = %u\n", name, xct->xct_name,
			       *(uint32_t *)data);
	} else {
		printf("%s(%s) = ", name, xct->xct_name);
		if (scanf("%u", &value) != EOF)
			*(uint32_t *)data = value;
	}
}

static void u64_get(const struct m0_xcode_type *xct,
		    const char *name, void *data)
{
	if (m0_console_verbose)
		printf("%s(%s) = %ld\n", name, xct->xct_name,
		       *(uint64_t *)data);
}

static void u64_set(const struct m0_xcode_type *xct,
		    const char *name, void *data)
{
	void    *tmp_value;
	uint64_t value;

	if (yaml_support) {
		tmp_value = cons_yaml_get_unsafe(name);
		*(uint64_t *)data = atol((const char *)tmp_value);
		if (m0_console_verbose)
			printf("%s(%s) = %ld\n", name, xct->xct_name,
			       *(uint64_t *)data);
	} else {
		printf("%s(%s) = ", name, xct->xct_name);
		if (scanf("%lu", &value) != EOF)
			*(uint64_t *)data = value;
	}
}

/**
 * @brief Methods to handle U64, U32 etc.
 */
static struct m0_cons_atom_ops atom_ops[M0_XAT_NR] = {
	[M0_XAT_VOID] = { void_get, void_set, default_show },
	[M0_XAT_U8]   = { byte_get, byte_set, default_show },
	[M0_XAT_U32]  = { u32_get, u32_set, default_show },
	[M0_XAT_U64]  = { u64_get, u64_set, default_show }
};

static void console_xc_atom_process(struct m0_xcode_cursor_frame *top,
                                    enum m0_cons_data_process_type type)
{
	const char                         *name;
	struct m0_xcode_obj                *cur   = &top->s_obj;
	const struct m0_xcode_type         *xt    = cur->xo_type;
	enum m0_xode_atom_type              atype = xt->xct_atype;
	const struct m0_xcode_type         *pt;
	const struct m0_xcode_obj          *par;
	const struct m0_xcode_cursor_frame *prev;

	prev = top - 1;
	par  = &prev->s_obj;
	pt   = par->xo_type;
	name = pt->xct_child[prev->s_fieldno].xf_name;

	switch (type) {
	case CONS_IT_INPUT:
		atom_ops[atype].catom_val_set(xt, name, cur->xo_ptr);
		break;
	case CONS_IT_OUTPUT:
		atom_ops[atype].catom_val_get(xt, name, cur->xo_ptr);
		break;
	case CONS_IT_SHOW:
		/*
		 * If it's a sequence element, set count field to 1.
		 * If count field is 0, rest of the sequence element
		 * will be skipped, and we won't be able to display it.
		 */
		if (pt->xct_aggr == M0_XA_SEQUENCE) {
			void **ptr;

			ptr = m0_xcode_addr(par, 1, ~0ULL);
			*(uint32_t *)ptr = 1;
		}
	default:
		atom_ops[atype].catom_val_show(xt, name, cur->xo_ptr);
	}
}

static int
cons_fop_iterate(struct m0_fop *fop, enum m0_cons_data_process_type type)
{
	int                     fop_depth = 0;
	int                     result;
	bool                    skip_next = false;
	struct m0_xcode_ctx     ctx;
	struct m0_xcode_cursor *it;

	M0_PRE(fop != NULL);

	m0_xcode_ctx_init(&ctx, &M0_FOP_XCODE_OBJ(fop));
	it = &ctx.xcx_it;

	printf("\n");

        while((result = m0_xcode_next(it)) > 0) {
		int                                 rc;
		struct m0_xcode_cursor_frame       *top;
		struct m0_xcode_obj                *cur;
		const struct m0_xcode_type         *xt;
		enum m0_xcode_aggr                  gtype;
		const struct m0_xcode_obj          *par;
		const struct m0_xcode_cursor_frame *prev;

		top   = m0_xcode_cursor_top(it);
		cur   = &top->s_obj;
		xt    = cur->xo_type;
		prev  = top - 1;
		par   = &prev->s_obj;
		gtype = xt->xct_aggr;

		switch (top->s_flag) {
		case M0_XCODE_CURSOR_PRE:
			rc = m0_xcode_alloc_obj(it, m0_xcode_alloc);
			if (rc != 0)
				return M0_RC(rc);
			++fop_depth;
			depth_print(fop_depth);

			if (gtype != M0_XA_ATOM && m0_console_verbose)
				printf("%s\n", xt->xct_name);
			else if (gtype == M0_XA_ATOM)
				console_xc_atom_process(top, type);
			break;
		case M0_XCODE_CURSOR_IN:
			if (gtype == M0_XA_SEQUENCE && skip_next) {
				m0_xcode_skip(it);
				skip_next = false;
				--fop_depth;
			}
			break;
		case M0_XCODE_CURSOR_POST:
			if (prev->s_fieldno == 1 &&
			    par->xo_type->xct_aggr == M0_XA_SEQUENCE) {
				if (m0_xcode_tag(par) == 1)
					skip_next = true;
				else if (xt->xct_atype == M0_XAT_U8)
					skip_next = true;
			}
			--fop_depth;
		default:
			break;
		}
	}
	return 0;
}

M0_INTERNAL int m0_cons_fop_obj_input(struct m0_fop *fop)
{
	return cons_fop_iterate(fop, CONS_IT_INPUT);
}

M0_INTERNAL int m0_cons_fop_obj_output(struct m0_fop *fop)
{
	return cons_fop_iterate(fop, CONS_IT_OUTPUT);
}

M0_INTERNAL int m0_cons_fop_fields_show(struct m0_fop *fop)
{
	bool vo;
	int  rc;

	vo = m0_console_verbose;
	m0_console_verbose = true;
	rc = cons_fop_iterate(fop, CONS_IT_SHOW);
	m0_console_verbose = vo;

	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of console_it group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
