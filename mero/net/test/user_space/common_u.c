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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 09/15/2012
 */

#include <string.h>			/* strlen */
#include <limits.h>			/* ULONG_MAX */

#include "lib/misc.h"			/* M0_SET0 */
#include "lib/assert.h"			/* M0_ASSERT */
#include "lib/memory.h"			/* m0_alloc */
#include "net/lnet/lnet.h"		/* m0_net_lnet_ifaces_get */
#include "net/net.h"			/* m0_net_domain */

#include "net/test/user_space/common_u.h"

/**
   @defgroup NetTestUCommonInternals Test Node user-space program
   @ingroup NetTestInternals

   @see @ref net-test

   @{
 */

bool m0_net_test_u_printf_verbose = false;

char *m0_net_test_u_str_copy(const char *str)
{
	size_t  len = strlen(str) + 1;
	char   *copy = m0_alloc(len);

	M0_ASSERT(copy != NULL);
	return strncpy(copy, str, len);
}

void m0_net_test_u_str_free(char *str)
{
	m0_free(str);
}

void m0_net_test_u_print_error(const char *s, int code)
{
	if (code != 0)
		fprintf(stderr, "%s, error %d: %s\n", s, code, strerror(-code));
}

void m0_net_test_u_print_s(const char *fmt, const char *str)
{
	m0_net_test_u_printf_v(fmt, str == NULL ? "NULL" : str);
}

void m0_net_test_u_print_time(char *name, m0_time_t time)
{
	uint64_t ns = m0_time_nanoseconds(time);

	m0_net_test_u_printf_v("%s\t= %lus", name, m0_time_seconds(time));
	if (ns != 0)
		m0_net_test_u_printf_v(" %luns", ns);
	m0_net_test_u_printf_v("\n");
}

void m0_net_test_u_print_bsize(double bsize)
{
	/*
	 * 0123456
	 *    1B
	 * 1023B
	 * 1.00KiB
	 * 9.99KiB
	 * 10.0KiB
	 * 99.9KiB
	 *  100KiB
	 * 1023KiB
	 * 1.00MiB
	 */
	char *fmt;
	int   i;
	static const struct {
		unsigned long  max;
		char	      *name;
	}     suffix[] = {
		{ .max = 1ul << 20, .name = "KiB" },
		{ .max = 1ul << 30, .name = "MiB" },
		{ .max = 1ul << 40, .name = "GiB" },
		{ .max = 1ul << 50, .name = "TiB" },
		{ .max = 1ul << 60, .name = "PiB" },
		{ .max = ULONG_MAX, .name = "EiB" },
	};

	if (bsize < 1023.5) {
		m0_net_test_u_printf("%4dB  ", (int) bsize);
	} else {
		for (i = 0; i < ARRAY_SIZE(suffix) - 1; ++i) {
			if (bsize < suffix[i].max - .5)
				break;
		}
		bsize /= suffix[i].max / (1 << 10);
		fmt = bsize < 9.995 ? "%4.2f%s" :
		      bsize < 99.95 ? "%4.1f%s" : "%4.0f%s";
		m0_net_test_u_printf(fmt, bsize, suffix[i].name);
	};
}

void m0_net_test_u_lnet_info(void)
{
	struct m0_net_domain  dom;
	int		      rc;
	char * const	     *nidstrs;
	int		      i;

	M0_SET0(&dom);
	rc = m0_net_domain_init(&dom, &m0_net_lnet_xprt);
	M0_ASSERT(rc == 0);

	m0_net_test_u_printf(
	      "m0_net_domain_get_max_buffer_size()         = %lu\n",
	      m0_net_domain_get_max_buffer_size(&dom));
	m0_net_test_u_printf(
	      "m0_net_domain_get_max_buffer_segments()     = %i\n",
	      m0_net_domain_get_max_buffer_segments(&dom));
	m0_net_test_u_printf(
	      "m0_net_domain_get_max_buffer_segment_size() = %lu\n",
	      m0_net_domain_get_max_buffer_segment_size(&dom));
	rc = m0_net_lnet_ifaces_get(&dom, &nidstrs);
	if (rc != 0) {
		m0_net_test_u_printf("m0_net_lnet_ifaces_get() failed.\n");
	} else {
		m0_net_test_u_printf("List of LNET interfaces:\n");
		for (i = 0; nidstrs != NULL && nidstrs[i] != NULL; ++i)
			m0_net_test_u_printf("NID %d:\t%s\n", i, nidstrs[i]);
		m0_net_test_u_printf("End of list.\n");
		m0_net_lnet_ifaces_put(&dom, &nidstrs);
	}

	m0_net_domain_fini(&dom);
}

static int net_test_u_printf(bool _verbose, const char *fmt, va_list argp)
{
	if (m0_net_test_u_printf_verbose || !_verbose)
		return vprintf(fmt, argp);
	return 0;
}

int m0_net_test_u_printf(const char *fmt, ...)
{
	va_list argp;
	int	rc = 0;

	va_start(argp, fmt);
	rc = net_test_u_printf(false, fmt, argp);
	va_end(argp);
	return rc;
}

int m0_net_test_u_printf_v(const char *fmt, ...)
{
	va_list argp;
	int	rc = 0;

	va_start(argp, fmt);
	rc = net_test_u_printf(true, fmt, argp);
	va_end(argp);
	return rc;
}

/**
   @} end of NetTestUCommonInternals group
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
