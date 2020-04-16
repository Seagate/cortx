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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 25-Mar-2015
 */


/**
 * @addtogroup XXX
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_XCODE

#include <stdio.h>
#include <string.h>                   /* strcmp, basename */
#include <stdlib.h>                   /* qsort */
#include <err.h>
#include <sys/stat.h>
#include <sysexits.h>

#include "lib/uuid.h"                 /* m0_node_uuid_string_set */
#include "lib/misc.h"                 /* ARRAY_SIZE */
#include "lib/user_space/types.h"     /* bool */
#include "mero/init.h"
#include "mero/version.h"             /* m0_build_info_print */
#include "module/instance.h"
#include "sm/sm.h"                    /* m0_sm_conf_print */
#include "lib/user_space/trace.h"     /* m0_trace_set_mmapped_buffer */
#include "xcode/xcode.h"
#include "xcode/init.h"               /* m0_xcode_init */

#undef __MERO_XCODE_XLIST_H__

#define _TI(x)
#define _EN(x)
#define _FI(x)
#define _FF(x)

#define _XT(x) M0_EXTERN struct m0_xcode_type *x;
#include "xcode/xlist.h"
#undef _XT

/* Let it be included second time. */
#undef __MERO_XCODE_XLIST_H__

static struct m0_xcode_type **xt[] = {
#define _XT(x) &x,
#include "xcode/xlist.h"
#undef _XT
};

#undef _TI
#undef _EN
#undef _FI
#undef _FF

static void field_print(const struct m0_xcode_field *f, int i);
static void type_print(const struct m0_xcode_type *xt);
static void protocol_print(int dom);
static int  cmp(const void *p0, const void *p1);

static void type_print(const struct m0_xcode_type *xt)
{
	int i;

	printf("\n%-30s %8.8s %6zi",
	       xt->xct_name,
	       xt->xct_aggr == M0_XA_ATOM ?
	       m0_xcode_atom_type_name[xt->xct_atype] :
	       m0_xcode_aggr_name[xt->xct_aggr],
	       xt->xct_sizeof);
	for (i = 0; i < xt->xct_nr; ++i)
		field_print(&xt->xct_child[i], i);
	printf("\nend %s", xt->xct_name);
}

static const char *xcode_domain_name = "all";

static void protocol_print(int dom)
{
	const struct m0_xcode_type *xtype;
	size_t total_structs = ARRAY_SIZE(xt);
	int i;

	qsort(xt, ARRAY_SIZE(xt), sizeof xt[0], &cmp);
	if (dom != 0)
		for (i = 0, total_structs = 0; i < ARRAY_SIZE(xt); ++i) {
			xtype = *(xt[i]);
			if (xtype->xct_flags & dom)
				total_structs++;
		}

	printf("Mero binary protocol\n");
	printf("Domains: %s\n", xcode_domain_name);
	printf("Total structures: %zu\n", total_structs);

	for (i = 0; i < ARRAY_SIZE(xt); ++i) {
		xtype = *(xt[i]);
		if (dom == 0 /* any domain */ || xtype->xct_flags & dom)
			type_print(xtype);
	}
	printf("\n");
}

static int cmp(const void *p0, const void *p1)
{
	const struct m0_xcode_type *xt0 = **(void ***)p0;
	const struct m0_xcode_type *xt1 = **(void ***)p1;

	return strcmp(xt0->xct_name, xt1->xct_name);
}

static void field_print(const struct m0_xcode_field *f, int i)
{
	printf("\n    %4i %-20s %-20s %4"PRIi32" [%#"PRIx64"]",
	       i, f->xf_name, f->xf_type->xct_name, f->xf_offset, f->xf_tag);
}

void (*m0_sm__conf_init)(const struct m0_sm_conf *conf);

void usage_print(const char* progname) {
	fprintf(stderr,
		"Usage: %s [options]\n"
		"  -p|--only-proto    print only protocol, skip"
					" any other useful info\n"
		"  -d|--domain DOM    print only protocol for a particular"
					" xcode domain, available domains are:"
					" 'be', 'conf' and 'rpc'\n"
		"\n"
		"  -v|--version       print version information\n"
		"  -h|--help          print usage information\n",
		progname);
}

int main(int argc, char **argv)
{
	struct m0 instance = {};
	bool      only_protocol = false;
	int       xcode_domain  = 0;  /* any domain */
	int       i;
	int       result;

	/* prevent creation of trace file for ourselves */
	m0_trace_set_mmapped_buffer(false);

	/*
	 * break dependency on m0mero.ko module and make ADDB happy,
	 * as we don't need a real node uuid for normal operation of this utility
	 */
	m0_node_uuid_string_set(NULL);

	/*
	 * using "manual" option processing because M0_GETOPTS expects m0_init()
	 * to be already done (due to m0_alloc() which uses fault injection
	 * which in turn uses m0_mutex)
	 */
	for (i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-p") == 0 ||
		    strcmp(argv[i], "--only-proto") == 0)
		{
			only_protocol = true;
			continue;
		}
		if (strcmp(argv[i], "-d") == 0 ||
		    strcmp(argv[i], "--domain") == 0)
		{
			if (i + 1 >= argc) {
				warnx("Error: missing argument for"
				      " '-d|--domain' option\n");
				usage_print(basename(argv[0]));
				exit(EX_USAGE);
			}

			if (strcmp(argv[i + 1], "be") == 0) {
				xcode_domain = M0_XCODE_TYPE_FLAG_DOM_BE;
			} else if (strcmp(argv[i + 1], "conf") == 0) {
				xcode_domain = M0_XCODE_TYPE_FLAG_DOM_CONF;
			} else if (strcmp(argv[i + 1], "rpc") == 0) {
				xcode_domain = M0_XCODE_TYPE_FLAG_DOM_RPC;
			} else {
				warnx("Error: invaid domain '%s' specified for"
				      " '-d|--domain' option\n", argv[i + 1]);
				usage_print(basename(argv[0]));
				exit(EX_USAGE);
			}

			xcode_domain_name = argv[++i];
			only_protocol = true;
			continue;
		}
		if (strcmp(argv[i], "-v") == 0 ||
		    strcmp(argv[i], "--version") == 0)
		{
			m0_build_info_print();
			exit(EXIT_SUCCESS);
		}

		/* show usage if requested or for an unknown option */
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0)
		{
			usage_print(basename(argv[0]));
			exit(EXIT_SUCCESS);
		} else {
			warnx("Error: unknown option '%s'\n", argv[i]);
			usage_print(basename(argv[0]));
			exit(EX_USAGE);
		}
	}

	if (only_protocol) {
		result = m0_xcode_init();
	} else {
		m0_sm__conf_init = &m0_sm_conf_print;
		result = m0_init(&instance);
	}

	if (result != 0)
		err(EX_CONFIG, "Cannot initialise mero: %d", result);
	protocol_print(xcode_domain);

	if (only_protocol)
		m0_xcode_fini();
	else
		m0_fini();
	return EX_OK;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of XXX group */

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
