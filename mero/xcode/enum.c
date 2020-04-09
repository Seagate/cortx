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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 11-Apr-2017
 */

/**
 * @addtogroup xcode
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_XCODE
#include "lib/trace.h"
#include "lib/errno.h"                       /* EPROTO */
#include "lib/misc.h"                        /* M0_BITS, m0_strtou64 */

#include "xcode/xcode.h"
#include "xcode/enum.h"

static const struct m0_xcode_enum_val *valget(const struct m0_xcode_enum *en,
					      uint64_t val);
static const struct m0_xcode_enum_val *nameget(const struct m0_xcode_enum *en,
					       const char *name, int nr);
static int enum_getnum(const char *buf, uint64_t *out);
static const char *enum_id   (const char *buf);
static const char *bitmask_id(const char *buf);

bool m0_xcode_enum_is_valid(const struct m0_xcode_enum *en, uint64_t val)
{
	return valget(en, val)->xev_idx >= 0;
}
M0_EXPORTED(m0_xcode_enum_is_valid);

const char *m0_xcode_enum_print(const struct m0_xcode_enum *en, uint64_t val,
				char *buf)
{
	const struct m0_xcode_enum_val *v = valget(en, val);

	if (v->xev_idx >= 0 || buf == NULL)
		return v->xev_name;
	else {
		sprintf(buf, "%"PRIx64, val);
		return buf;
	}
}
M0_EXPORTED(m0_xcode_enum_print);

int m0_xcode_enum_read(const struct m0_xcode_enum *en,
		       const char *buf, int nr, uint64_t *val)
{
	const struct m0_xcode_enum_val *v = nameget(en, buf, nr);

	if (v->xev_idx >= 0) {
		*val = v->xev_val;
		return 0;
	} else
		return enum_getnum(buf, val) == nr ? 0 : M0_ERR(-EPROTO);
}
M0_EXPORTED(m0_xcode_enum_read);

bool m0_xcode_bitmask_is_valid(const struct m0_xcode_enum *en, uint64_t val)
{
	return m0_forall(i, 64, ergo(val & M0_BITS(i),
				     m0_xcode_enum_is_valid(en, M0_BITS(i))));
}
M0_EXPORTED(m0_xcode_bitmask_is_valid);

int m0_xcode_bitmask_print(const struct m0_xcode_enum *en,
			   uint64_t val, char *buf, int nr)
{
	int i;
	int nob;
#define P(fmt, val)					\
({							\
	nob += snprintf(buf + nob, nr - nob, "%s" fmt,	\
			nob == 0 ? "" : "|", val);	\
})

	for (i = 0, nob = 0; i < en->xe_nr; ++i) {
		const struct m0_xcode_enum_val *v = &en->xe_val[i];

		if ((val & v->xev_val) != 0) {
			P("%s", v->xev_name);
			val &= ~v->xev_val;
		}
	}
	if (val != 0)
		/* Print remaining bits in sexadecimal. */
		P("%"PRIx64, val);
	return nob;
#undef P
}
M0_EXPORTED(m0_xcode_bitmask_print);

int m0_xcode_bitmask_read(const struct m0_xcode_enum *en,
			  const char *buf, int nr, uint64_t *val)
{
	*val = 0ULL;
	while (nr > 0) {
		int                             nob;
		const struct m0_xcode_enum_val *v;

		for (nob = 0; nob < nr && buf[nob] != '|'; ++nob)
			;
		v = nameget(en, buf, nob);
		if (v->xev_idx < 0) {
			uint64_t num;

			nob = enum_getnum(buf, &num);
			if (nob > 0)
				*val |= num;
			else
				return M0_ERR(-EPROTO);
		} else
			*val |= v->xev_val;
		if (buf[nob] == '|')
			nob++;
		buf += nob;
		nr  -= nob;
	}
	return 0;
}
M0_EXPORTED(m0_xcode_bitmask_read);

M0_INTERNAL int m0_xcode_enum_field_read(const struct m0_xcode_cursor *it,
					 struct m0_xcode_obj *obj,
					 const char *str)
{
	const struct m0_xcode_field *f  = m0_xcode_cursor_field(it);
	const struct m0_xcode_enum  *en = f->xf_decor[M0_XCODE_DECOR_READ];
	int                          nr = enum_id(str) - str;

	M0_PRE(M0_IN(f->xf_type, (&M0_XT_U8, &M0_XT_U32, &M0_XT_U64)));
	M0_PRE(en != NULL);
	return m0_xcode_enum_read(en, str, nr, obj->xo_ptr) ?: nr;
}

M0_INTERNAL int m0_xcode_bitmask_field_read(const struct m0_xcode_cursor *it,
					    struct m0_xcode_obj *obj,
					    const char *str)
{
	const struct m0_xcode_field *f  = m0_xcode_cursor_field(it);
	const struct m0_xcode_enum  *en = f->xf_decor[M0_XCODE_DECOR_READ];
	int                          nr = bitmask_id(str) - str;

	M0_PRE(M0_IN(f->xf_type, (&M0_XT_U8, &M0_XT_U32, &M0_XT_U64)));
	M0_PRE(en != NULL);
	return m0_xcode_bitmask_read(en, str, nr, obj->xo_ptr) ?: nr;
}

static const struct m0_xcode_enum_val *valget(const struct m0_xcode_enum *en,
					      uint64_t val)
{
	int i;

	for (i = 0; i < en->xe_nr; ++i) {
		if (en->xe_val[i].xev_val == val)
			break;
	}
	return &en->xe_val[i];
}

static const struct m0_xcode_enum_val *nameget(const struct m0_xcode_enum *en,
					       const char *name, int nr)
{
	int i;

	for (i = 0; i < en->xe_nr; ++i) {
		if (strncmp(en->xe_val[i].xev_name, name, nr) == 0 &&
		    nr == strlen(en->xe_val[i].xev_name))
			break;
	}
	return &en->xe_val[i];
}

static int enum_getnum(const char *buf, uint64_t *out)
{
	char *endp;

	*out = m0_strtou64(buf, &endp, 0x10);
	return endp - buf;
}

static bool enum_char(char ch)
{
#define BETWEEN(ch, l, h) ((l) <= (ch) && (ch) <= (h))
	return  BETWEEN(ch, 'a', 'z') || BETWEEN(ch, 'A', 'Z') ||
		BETWEEN(ch, '0', '9') || ch == '_';
#undef BETWEEN
}

static const char *enum_id(const char *b)
{
	while (*b != 0 && enum_char(*b))
		b++;
	return b;
}

static const char *bitmask_id(const char *b)
{
	while (*b != 0 && (enum_char(*b) || *b == '|'))
		b++;
	return b;
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
