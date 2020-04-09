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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 10/04/2010
 */

#include "ut/ut.h"		/* M0_UT_ASSERT */
#include "lib/thread.h"		/* LAMBDA */
#include "lib/getopts.h"	/* m0_bcount_get */
#include "lib/time.h"		/* m0_time_t */
#include "lib/errno.h"          /* ENOENT */

void test_getopts(void)
{
	int	    result;
	int	    argc;
	int	    argc_scaled;
	int	    num;
	bool	    e;
	m0_bcount_t bcount;
	m0_time_t   time;
	char *argv[] = {
		"getopts-ut",
		"-e",
		"-n", "010",
		NULL
	};
	char *argv_scaled[] = {
		"-a", "2b",
		"-b", "30k",
		"-c", "400m",
		"-d", "5000g",
		"-x", "70K",
		"-y", "800M",
		"-z", "9000G",
		"-j", "123456789012345",
		NULL
	};
	struct m0_ut_redirect redir;

	argc		    = ARRAY_SIZE(argv) - 1;
	argc_scaled	    = ARRAY_SIZE(argv_scaled) - 1;

	m0_stream_redirect(stderr, "/dev/null", &redir);
	result = M0_GETOPTS("getopts-ut", argc, argv);
	M0_UT_ASSERT(result == -EINVAL);
	m0_stream_restore(&redir);

	e = false;
	result = M0_GETOPTS("getopts-ut", argc, argv,
			    M0_FORMATARG('n', "Num", "%i", &num),
			    M0_FLAGARG('e', "E", &e));
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(e == true);
	M0_UT_ASSERT(num == 8);

	result = M0_GETOPTS("getopts-ut", argc, argv,
			    M0_FORMATARG('n', "Num", "%d", &num),
			    M0_FLAGARG('e', "E", &e));
	M0_UT_ASSERT(num == 10);

	result = M0_GETOPTS("getopts-ut", argc, argv,
			    M0_STRINGARG('n', "Num",
			 LAMBDA(void, (const char *s){
				 M0_UT_ASSERT(!strcmp(s, "010"));
			 })),
			    M0_FLAGARG('e', "E", &e));
	M0_UT_ASSERT(result == 0);

	/* test for valid "scaled"-type options */
	result = M0_GETOPTS("getopts-ut", argc_scaled, argv_scaled,
			    M0_SCALEDARG('a', "scaled",
			LAMBDA(void, (m0_bcount_t bcount){
				M0_UT_ASSERT(bcount == 2 * 512);})),
			    M0_SCALEDARG('b', "scaled",
			LAMBDA(void, (m0_bcount_t bcount){
				M0_UT_ASSERT(bcount == 30 * 1024);})),
			    M0_SCALEDARG('c', "scaled",
			LAMBDA(void, (m0_bcount_t bcount){
				M0_UT_ASSERT(bcount == 400 * 1024 * 1024);})),
			    M0_SCALEDARG('d', "scaled",
			LAMBDA(void, (m0_bcount_t bcount){
				M0_UT_ASSERT(bcount ==
					5000ULL * 1024 * 1024 * 1024);})),
			    M0_SCALEDARG('x', "scaled",
			LAMBDA(void, (m0_bcount_t bcount){
				M0_UT_ASSERT(bcount == 70 * 1000);})),
			    M0_SCALEDARG('y', "scaled",
			LAMBDA(void, (m0_bcount_t bcount){
				M0_UT_ASSERT(bcount == 800 * 1000000);})),
			    M0_SCALEDARG('z', "scaled",
			LAMBDA(void, (m0_bcount_t bcount){
				M0_UT_ASSERT(bcount ==
					9000 * 1000000000ULL);})),
			    M0_SCALEDARG('j', "scaled",
			LAMBDA(void, (m0_bcount_t bcount){
				M0_UT_ASSERT(bcount ==
					123456789012345ULL);})));
	M0_UT_ASSERT(result == 0);

	argv[--argc] = NULL;
	argv[--argc] = NULL;

	e = false;
	result = M0_GETOPTS("getopts-ut", argc, argv,
			    M0_FLAGARG('e', "E", &e));
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(e == true);
	argv[--argc] = NULL;

	result = M0_GETOPTS("getopts-ut", argc, argv);
	M0_UT_ASSERT(result == 0);

	/* m0_bcount_get() */
	result = m0_bcount_get("123456789012345G", &bcount);
	M0_UT_ASSERT(result == -EOVERFLOW);
	result = m0_bcount_get("1asdf", &bcount);
	M0_UT_ASSERT(result == -EINVAL);

	/* m0_time_get() */
	result = m0_time_get("1", &time);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(m0_time_seconds(time)     == 1);
	M0_UT_ASSERT(m0_time_nanoseconds(time) == 0);

	result = m0_time_get("1.20s", &time);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(m0_time_seconds(time)     == 1);
	M0_UT_ASSERT(m0_time_nanoseconds(time) == 200000000);

	result = m0_time_get("2.300ms", &time);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(m0_time_seconds(time)     == 0);
	M0_UT_ASSERT(m0_time_nanoseconds(time) == 2300000);

	result = m0_time_get("3.4000us", &time);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(m0_time_seconds(time)     == 0);
	M0_UT_ASSERT(m0_time_nanoseconds(time) == 3400);

	result = m0_time_get("5.60000ns", &time);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(m0_time_seconds(time)     == 0);
	M0_UT_ASSERT(m0_time_nanoseconds(time) == 5);

	result = m0_time_get("12345.67890s", &time);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(m0_time_seconds(time)     == 12345);
	M0_UT_ASSERT(m0_time_nanoseconds(time) == 678900000);

	result = m0_time_get(".1s", &time);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(m0_time_seconds(time)     == 0);
	M0_UT_ASSERT(m0_time_nanoseconds(time) == 100000000);

	result = m0_time_get(".01s", &time);
	M0_UT_ASSERT(result == 0);
	M0_UT_ASSERT(m0_time_seconds(time)     == 0);
	M0_UT_ASSERT(m0_time_nanoseconds(time) == 10000000);

	result = m0_time_get("12345.67890sec", &time);
	M0_UT_ASSERT(result == -EINVAL);

	result = m0_time_get("18446744073709551616", &time);
	M0_UT_ASSERT(result == -E2BIG);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
