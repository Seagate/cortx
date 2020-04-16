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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 * Original creation date: 12/02/2010
 */

#include "lib/arith.h"  /* min_type */
#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/misc.h"
#include "lib/buf.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

/**
   @addtogroup buf Basic buffer type
   @{
*/

M0_INTERNAL void m0_buf_init(struct m0_buf *buf, void *data, uint32_t nob)
{
	buf->b_addr = data;
	buf->b_nob  = nob;
}

M0_INTERNAL int m0_buf_alloc(struct m0_buf *buf, size_t size)
{
	M0_PRE(!m0_buf_is_set(buf));

	buf->b_addr = m0_alloc(size);
	if (buf->b_addr == NULL)
		return -ENOMEM;

	buf->b_nob = size;
	return 0;
}

M0_INTERNAL void m0_buf_free(struct m0_buf *buf)
{
	m0_free0(&buf->b_addr);
	buf->b_nob = 0;
}

M0_INTERNAL int m0_buf_cmp(const struct m0_buf *x, const struct m0_buf *y)
{
	int rc;

	rc = memcmp(x->b_addr, y->b_addr,
		    min_type(m0_bcount_t, x->b_nob, y->b_nob));
	/*
	 * Special case when one buffer is prefix for the second. We can't
	 * compare the first byte of the suffix with 0, because m0_buf may
	 * contain '\0' and in this situation 0 would return for not equal
	 * buffers.
	 */
	if (rc == 0 && x->b_nob != y->b_nob)
		rc = x->b_nob > y->b_nob ? 1 : -1;
	return rc;
}

M0_INTERNAL bool m0_buf_eq(const struct m0_buf *x, const struct m0_buf *y)
{
	return x->b_nob == y->b_nob &&
		memcmp(x->b_addr, y->b_addr, x->b_nob) == 0;
}

M0_INTERNAL int m0_buf_copy(struct m0_buf *dest, const struct m0_buf *src)
{
	M0_PRE(dest->b_nob == 0 && dest->b_addr == NULL);

	if (src->b_nob != 0) {
		dest->b_addr = m0_alloc(src->b_nob);
		if (dest->b_addr == NULL)
			return M0_ERR(-ENOMEM);
		dest->b_nob = src->b_nob;
		memcpy(dest->b_addr, src->b_addr, src->b_nob);
	}
	M0_POST(m0_buf_eq(dest, src));
	return 0;
}

M0_INTERNAL int m0_buf_copy_aligned(struct m0_buf *dst,
				    struct m0_buf *src,
				    unsigned       shift)
{
	M0_PRE(dst->b_nob == 0 && dst->b_addr == NULL);

	M0_ALLOC_ARR_ALIGNED(dst->b_addr, src->b_nob, shift);
	if (dst->b_addr == NULL)
		return M0_ERR(-ENOMEM);
	dst->b_nob = src->b_nob;
	memcpy(dst->b_addr, src->b_addr, src->b_nob);
	return 0;
}

M0_INTERNAL bool m0_buf_is_set(const struct m0_buf *buf)
{
	return buf->b_nob > 0 && buf->b_addr != NULL;
}

M0_INTERNAL bool m0_buf_streq(const struct m0_buf *buf, const char *str)
{
	M0_PRE(str != NULL);

	return strlen(str) == buf->b_nob &&
		memcmp(str, buf->b_addr, buf->b_nob) == 0;
}

M0_INTERNAL char *m0_buf_strdup(const struct m0_buf *buf)
{
	size_t len;
	char  *s;

	/* Measure the size of payload. */
	s = memchr(buf->b_addr, 0, buf->b_nob);
	len = s == NULL ? buf->b_nob : s - (char *)buf->b_addr;

	M0_ALLOC_ARR(s, len + 1);
	if (s != NULL) {
		memcpy(s, buf->b_addr, len);
		s[len] = 0;
	}
	return s;
}

M0_INTERNAL int m0_bufs_from_strings(struct m0_bufs *dest, const char **src)
{
	size_t i;
	int    rc;

	M0_SET0(dest);

	if (src == NULL)
		return 0;

	while (src[dest->ab_count] != NULL)
		++dest->ab_count;
	if (dest->ab_count == 0)
		return 0;

	M0_ALLOC_ARR(dest->ab_elems, dest->ab_count);
	if (dest->ab_elems == NULL)
		return M0_ERR(-ENOMEM);

	for (i = 0; i < dest->ab_count; ++i) {
		rc = m0_buf_copy(&dest->ab_elems[i],
				 &M0_BUF_INITS((char *)src[i]));
		if (rc != 0) {
			m0_bufs_free(dest);
			return M0_ERR(-ENOMEM);
		}
	}
	return 0;
}

M0_INTERNAL int
m0_bufs_to_strings(const char ***dest, const struct m0_bufs *src)
{
	uint32_t i;

	M0_PRE(*dest == NULL);
	M0_PRE((src->ab_count == 0) == (src->ab_elems == NULL));

	if (src->ab_count == 0)
		return 0; /* there is nothing to copy */

	M0_ALLOC_ARR(*dest, src->ab_count + 1);
	if (*dest == NULL)
		return M0_ERR(-ENOMEM);

	for (i = 0; i < src->ab_count; ++i) {
		(*dest)[i] = m0_buf_strdup(&src->ab_elems[i]);
		if ((*dest)[i] == NULL)
			goto fail;
	}
	(*dest)[i] = NULL; /* end of list */

	return 0;
fail:
	for (; i != 0; --i)
		m0_free((void *)(*dest)[i]);
	m0_free(*dest);
	return M0_ERR(-ENOMEM);
}

M0_INTERNAL bool m0_bufs_streq(const struct m0_bufs *bufs, const char **strs)
{
	uint32_t i;

	for (i = 0; strs[i] != NULL; ++i) {
		if (i >= bufs->ab_count || !m0_buf_streq(&bufs->ab_elems[i],
							 strs[i]))
			return false;
	}
	return i == bufs->ab_count;
}

M0_INTERNAL void m0_bufs_free(struct m0_bufs *bufs)
{
	while (bufs->ab_count > 0)
		m0_buf_free(&bufs->ab_elems[--bufs->ab_count]);
	m0_free0(&bufs->ab_elems);
	M0_POST(bufs->ab_count == 0);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of buf group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
