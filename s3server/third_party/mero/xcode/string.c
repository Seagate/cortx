/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 07-Sep-2012
 */

#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/arith.h"                          /* min64 */
#include "lib/string.h"                         /* sscanf */

#include "xcode/xcode.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_XCODE
#include "lib/trace.h"

/**
 * @addtogroup xcode
 *
 * @{
 */

/* xcode.c */
M0_EXTERN ssize_t
m0_xcode_alloc_obj(struct m0_xcode_cursor *it,
		   void *(*alloc)(struct m0_xcode_cursor *, size_t));

M0_INTERNAL const char *space_skip(const char *str)
{
	static const char space[] = " \t\v\n\r";
	const char       *s0;

	do {
		s0 = str;
		while (*str != 0 && strchr(space, *str) != NULL)
			str++;

		if (*str == '#') {
			while (*str != 0 && *str != '\n')
				str++;
		}
	} while (s0 != str);
	return str;
}

static int string_literal(const struct m0_xcode_cursor *it,
			  struct m0_xcode_obj *obj, const char *str)
{
	uint64_t                    len;
	const char                 *eol;
	char                       *mem;
	const struct m0_xcode_type *count_type;

	count_type = obj->xo_type->xct_child[0].xf_type;
	if (count_type == &M0_XT_VOID) {
		/* fixed length string */
		len = m0_xcode_tag(obj);
	} else {
		eol = strchr(str, '"');
		if (eol == NULL)
			return M0_ERR(-EPROTO);
		len = eol - str;
		switch (count_type->xct_atype) {
		case M0_XAT_U8:
			*M0_XCODE_VAL(obj, 0, 0, uint8_t) = (uint8_t)len;
			break;
		case M0_XAT_U32:
			*M0_XCODE_VAL(obj, 0, 0, uint32_t) = (uint32_t)len;
			break;
		case M0_XAT_U64:
			*M0_XCODE_VAL(obj, 0, 0, uint64_t) = (uint64_t)len;
			break;
		default:
			M0_IMPOSSIBLE("Invalid counter type.");
		}
	}

	if (len == 0)
		return 1; /* Including closing '"'. */

	*(void **)m0_xcode_addr(obj, 1, ~0ULL) = mem = m0_alloc(len);
	if (mem != NULL) {
		memcpy(mem, str, len);
		return len + 1;
	} else
		return M0_ERR(-ENOMEM);
}

static int (*field_reader(const struct m0_xcode_cursor *it))
				(const struct m0_xcode_cursor *,
				 struct m0_xcode_obj *, const char *)
{
	const struct m0_xcode_field *field = m0_xcode_cursor_field(it);
	return field != NULL ? field->xf_read : NULL;
}

static int char_check(const char **str, char ch)
{
	if (ch != 0) {
		if (**str != ch)
			return M0_ERR_INFO(-EPROTO, "ch='%c' str=`%.80s...'",
					   ch, *str);
		(*str)++;
		*str = space_skip(*str);
	}
	return 0;
}

static const char structure[M0_XA_NR][M0_XCODE_CURSOR_NR] = {
		       /* NONE  PRE   IN POST */
	[M0_XA_RECORD]   = { 0, '(',   0, ')' },
	[M0_XA_UNION]    = { 0, '{',   0, '}' },
	[M0_XA_SEQUENCE] = { 0, '[',   0, ']' },
	[M0_XA_ARRAY]    = { 0, '<',   0, '>' },
	[M0_XA_TYPEDEF]  = { 0,   0,   0,   0 },
	[M0_XA_OPAQUE]   = { 0,   0,   0,   0 },
	[M0_XA_ATOM]     = { 0,   0,   0,   0 }
};

static const char punctuation[M0_XA_NR][3] = {
			/* 1st  2nd later */
	[M0_XA_RECORD]   = { 0, ',', ',' },
	[M0_XA_UNION]    = { 0, '|',  0  },
	[M0_XA_SEQUENCE] = { 0, ':', ',' },
	[M0_XA_ARRAY]    = { 0, ',', ',' },
	[M0_XA_TYPEDEF]  = { 0,  0,   0  },
	[M0_XA_OPAQUE]   = { 0,  0,   0  },
	[M0_XA_ATOM]     = { 0,  0,   0  }
};

static char punctchar(struct m0_xcode_cursor *it)
{
	struct m0_xcode_cursor_frame *pre = m0_xcode_cursor_top(it) - 1;
	enum m0_xcode_aggr            par;
	int                           order;

	if (it->xcu_depth > 0) {
		order = min64(pre->s_datum++, ARRAY_SIZE(punctuation[0]) - 1);
		par   = pre->s_obj.xo_type->xct_aggr;
		return punctuation[par][order];
	} else
		return 0;
}

M0_INTERNAL int m0_xcode_read(struct m0_xcode_obj *obj, const char *str)
{
	struct m0_xcode_cursor it;
	int                    result;

	static const char *fmt[M0_XAT_NR] = {
		[M0_XAT_VOID] = " %0c %n",
		[M0_XAT_U8]   = " %i %n",
		[M0_XAT_U32]  = " %i %n",
		[M0_XAT_U64]  = " %li %n"
	};

	/* check that formats above are valid. */
	M0_CASSERT(sizeof(uint64_t) == sizeof(unsigned long));
	M0_CASSERT(sizeof(uint32_t) == sizeof(unsigned));

	m0_xcode_cursor_init(&it, obj);

	while ((result = m0_xcode_next(&it)) > 0) {
		struct m0_xcode_cursor_frame *top  = m0_xcode_cursor_top(&it);
		struct m0_xcode_obj          *cur  = &top->s_obj;
		enum m0_xcode_cursor_flag     flag = top->s_flag;
		const struct m0_xcode_type   *xt   = cur->xo_type;
		enum m0_xcode_aggr            aggr = xt->xct_aggr;

		str = space_skip(str);
		if (flag == M0_XCODE_CURSOR_PRE) {
			int (*custom)(const struct m0_xcode_cursor *,
				      struct m0_xcode_obj *, const char *);

			result = m0_xcode_alloc_obj(&it, m0_xcode_alloc);
			if (result != 0)
				return result;
			result = char_check(&str, punctchar(&it));
			if (result != 0)
				return result;
			custom = *str == '"' && m0_xcode_is_byte_array(xt) ?
				&string_literal :
				*str == '^' && xt->xct_ops != NULL ?
				xt->xct_ops->xto_read :
				*str == '@' ? field_reader(&it) : NULL;
			if (custom != NULL) {
				/*
				 * A string literal (skip opening '"'), a custom
				 * type reader (skip opening '^') or a custom
				 * field reader (skip opening '@').
				 */
				++str;
				result = custom(&it, cur, str);
				if (result < 0)
					return result;
				str += result;
				m0_xcode_skip(&it);
				continue;
			}
		}
		result = char_check(&str, structure[aggr][flag]);
		if (result != 0)
			return result;
		if (flag == M0_XCODE_CURSOR_PRE && aggr == M0_XA_ATOM) {
			int      nob;
			int      nr;
			unsigned bval = 0;
			void    *pval;

			/*
			 * according to format, a byte goes to 4-byte bval, but
			 * not directly to allocated buffer (i.e. cur->xo_ptr)
			 */
			pval = xt->xct_atype == M0_XAT_U8 ? &bval : cur->xo_ptr;
			nr = sscanf(str, fmt[xt->xct_atype], pval, &nob);
			if (xt->xct_atype == M0_XAT_U8) {
				if (bval < 0x100)
					*(uint8_t *)cur->xo_ptr = (uint8_t)bval;
				else
					return M0_ERR(-EOVERFLOW);
			}
			/*
			 * WARNING
			 *
			 *     The C standard says: "Execution of a %n directive
			 *     does not increment the assignment count returned
			 *     at the completion of execution" but the
			 *     Corrigendum seems to contradict this.  Probably
			 *     it is wise not to make any assumptions on the
			 *     effect of %n conversions on the return value.
			 *
			 *                          -- man 3 sscanf
			 *
			 * glibc-2.11.2 and Linux kernel do not increment. See
			 * NO_BUG_IN_ISO_C_CORRIGENDUM_1 in
			 * glibc/stdio-common/vfscanf.c.
			 */
			if (nr != 1)
				return M0_ERR(-EPROTO);
			str += nob;
		}
	}
	return *str == 0 ? 0 : M0_ERR(-EINVAL);
}

static bool quoted_string(const struct m0_xcode_type *xt,
			  const struct m0_xcode_obj  *obj,
			  struct m0_fop_str          *qstr)
{
	if (m0_xcode_is_byte_array(xt)) {
		qstr->s_len = m0_xcode_tag(obj);
		qstr->s_buf = m0_xcode_addr(obj, 1, 0);

		/* A crude printability check. */
		return m0_forall(i, qstr->s_len,
				qstr->s_buf[i] >= ' ' && qstr->s_buf[i] <= '~');
	} else
		return false;
}

M0_INTERNAL int m0_xcode_print(const struct m0_xcode_obj *obj,
			       char *str, int nr)
{
	struct m0_xcode_cursor it;
	int                    result;
	int                    nob = 0;

	m0_xcode_cursor_init(&it, obj);

#define P(...)								\
	({ nob += snprintf(str + nob, max64(nr - nob, 0), __VA_ARGS__); })
#define PCHAR(ch) ({ char _ch = (ch); if (_ch != 0) P("%c", _ch); })

	while ((result = m0_xcode_next(&it)) > 0) {
		struct m0_xcode_cursor_frame *top  = m0_xcode_cursor_top(&it);
		struct m0_xcode_obj          *cur  = &top->s_obj;
		enum m0_xcode_cursor_flag     flag = top->s_flag;
		const struct m0_xcode_type   *xt   = cur->xo_type;
		enum m0_xcode_aggr            aggr = xt->xct_aggr;
		struct m0_fop_str             qstr;

		if (flag == M0_XCODE_CURSOR_PRE)
			PCHAR(punctchar(&it));

		if (quoted_string(xt, cur, &qstr)) {
			P("\"%.*s\"", qstr.s_len, qstr.s_buf);
			m0_xcode_skip(&it);
			continue;
		}
		PCHAR(structure[aggr][flag]);

		if (flag == M0_XCODE_CURSOR_PRE && aggr == M0_XA_ATOM)
			P("%#lx", (unsigned long)m0_xcode_atom(cur));
	}
	return result ?: nob;
#undef PCHAR
#undef P
}

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
