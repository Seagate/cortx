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
 * Original creation date: 08/19/2010
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2 /* for getopt */
#endif
#include <unistd.h>     /* getopt */
#include <stdio.h>      /* fprintf, sscanf */
#include <stdlib.h>     /* strtoll */
#include <string.h>     /* strchr */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

/* getopt(3) interface */
extern char *optarg;
extern int   optind;
extern int   optopt;
extern int   opterr;
extern int   optreset;

#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/getopts.h"

/**
   @addtogroup getopts
   @{
 */

static void usage(const char *progname,
		  const struct m0_getopts_opt *opts, unsigned nr)
{
	int i;

	fprintf(stderr, "Usage: %s options...\n\nwhere valid options are\n\n",
			progname);

	for (i = 0; i < nr; ++i) {
		const struct m0_getopts_opt *o;

		o = &opts[i];
		fprintf(stderr, "\t -%c %10s: %s\n", o->go_opt,
			o->go_type == GOT_VOID ? "" :
			o->go_type == GOT_HELP ? "" :
			o->go_type == GOT_FLAG ? "" :
			o->go_type == GOT_FORMAT ? o->go_u.got_fmt.f_string :
			o->go_type == GOT_SCALED ? "scaled" :
			o->go_type == GOT_NUMBER ? "number" : "string",
			o->go_desc);
	}
}

static int getnum(const char *arg, const char *desc, int64_t *out)
{
	char *end;

	*out = strtoll(arg, &end, 0);
	if (*end != 0) {
		fprintf(stderr, "Failed conversion of \"%s\" to %s\n",
			arg, desc);
		return M0_ERR(-EINVAL);
	} else
		return 0;
}

static int getscaled(const char *arg, const char *desc, m0_bcount_t *out)
{
	int rc;

	rc = m0_bcount_get(arg, out);
	if (rc != 0)
		fprintf(stderr, "Failed conversion of \"%s\" to %s\n",
			arg, desc);
	return M0_RC(rc);
}

int m0_getopts(const char *progname, int argc, char *const *argv,
	       const struct m0_getopts_opt *opts, unsigned nr)
{
	char *optstring;
	int   i;
	int   scan;
	int   ch;
	int   result;

	M0_ALLOC_ARR(optstring, 2 * nr + 1);
	if (optstring == NULL)
		return M0_ERR(-ENOMEM);

	for (scan = i = 0; i < nr; ++i) {
		/* -W is reserved by POSIX.2 and used by GNU getopts as a long
		   option escape. */
		M0_ASSERT(opts[i].go_opt != 'W');
		optstring[scan++] = opts[i].go_opt;
		if (!M0_IN(opts[i].go_type, (GOT_VOID, GOT_FLAG, GOT_HELP)))
			optstring[scan++] = ':';
		if (opts[i].go_type == GOT_FLAG)
			*opts[i].go_u.got_flag = false;
	}

	result = 0;

	/*
	 * Re-set global getopt(3) state before calling it.
	 */
	optind = 1;
	opterr = 1;

	while (result == 0 && (ch = getopt(argc, argv, optstring)) != -1) {
		for (i = 0; i < nr; ++i) {
			const struct m0_getopts_opt  *o;
			const union m0_getopts_union *u;

			o = &opts[i];
			if (ch != o->go_opt)
				continue;

			u = &o->go_u;
			switch (o->go_type) {
			case GOT_VOID:
				u->got_void();
				break;
			case GOT_NUMBER: {
				int64_t num;

				result = getnum(optarg, o->go_desc, &num);
				if (result == 0)
					u->got_number(num);
				break;
			}
			case GOT_SCALED: {
				m0_bcount_t num;

				result = getscaled(optarg, o->go_desc, &num);
				if (result == 0)
					u->got_scaled(num);
				break;
			}
			case GOT_STRING:
				u->got_string(optarg);
				break;
			case GOT_FORMAT:
				result = sscanf(optarg, u->got_fmt.f_string,
						u->got_fmt.f_out);
				result = result == 1 ? 0 : M0_ERR(-EINVAL);
				if (result != 0) {
					fprintf(stderr, "Cannot scan \"%s\" "
						"as \"%s\" in \"%s\"\n",
						optarg, u->got_fmt.f_string,
						o->go_desc);
				}
				break;
			case GOT_FLAG:
				*u->got_flag = true;
				break;
			case GOT_HELP:
				usage(progname, opts, nr);
				exit(EXIT_FAILURE);
				break;
			default:
				M0_IMPOSSIBLE("Wrong option type.");
			}
			break;
		}
		if (i == nr)  {
			M0_ASSERT(ch == '?');
			fprintf(stderr, "Unknown option '%c'\n", optopt);
			usage(progname, opts, nr);
			result = M0_ERR(-EINVAL);
		}
	}

	m0_free(optstring);
	return result;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of getopts group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
