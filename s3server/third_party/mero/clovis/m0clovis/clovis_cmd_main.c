/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 22-Apr-2016
 */


/**
 * @addtogroup clovis
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"

#include <stdio.h>              /* vprintf */
#include <stdlib.h>             /* exit */
#include <stdarg.h>             /* va_list */
#include "lib/errno.h"
#include "clovis_index.h"
#include "clovis_common.h"
#include "lib/getopts.h"	/* M0_GETOPTS */
#include "lib/thread.h"		/* LAMBDA */
#include "module/instance.h"	/* m0 */

struct c_subsystem {
	const char  *s_name;
	int        (*s_init)(struct clovis_params *par);
	void       (*s_fini)(void);
	void       (*s_usage)(void);
	int        (*s_execute)(int argc, char** argv);
};

static struct c_subsystem subsystems[] = {
	{ "index", index_init, index_fini, index_usage, index_execute }
};

enum {
	LOCAL,
	HA,
	CONFD,
	PROF,
	HELP
};

static struct m0 instance;

static int subsystem_id(char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(subsystems); ++i) {
		if (!strcmp(name, subsystems[i].s_name))
			return i;
	}
	return M0_ERR(-EPROTONOSUPPORT);
}

static void usage(void)
{
	int i;

	m0_console_printf(
		"Clovis Command Line tool: m0clovis\n"
		"Usage: ./m0clovis "
		"-l local_addr -h ha_addr -p profile -f proc_fid "
		"[subsystem] [subsystem commands]\n"
		"\n"
		"Use -? for more verbose help on common arguments.\n"
		"Usage example for common arguments: \n"
		"./m0clovis -l 10.0.2.15@tcp:12345:33:100 "
		"-h 10.0.2.15@tcp:12345:34:1 "
		"-p '<0x7000000000000001:0>' -f '<0x7200000000000000:0>'"
		"[subsystem] [subsystem commands]\n"
		"\n"
		"Available subsystems and subsystem-specific commands are "
		"listed below.\n");
	for (i = 0; i < ARRAY_SIZE(subsystems); i++)
		subsystems[i].s_usage();
}

static int opts_get(struct clovis_params *par, int *argc, char ***argv)
{
	int    rc = 0;
	char **arg = *(argv);

	par->cp_local_addr = NULL;
	par->cp_ha_addr    = NULL;
	par->cp_prof       = NULL;
	par->cp_proc_fid   = NULL;

	rc = M0_GETOPTS("m0clovis", *argc, *argv,
			M0_HELPARG('?'),
			M0_VOIDARG('i', "more verbose help",
					LAMBDA(void, (void) {
						usage();
						exit(0);
					})),
			M0_STRINGARG('l', "Local endpoint address",
					LAMBDA(void, (const char *string) {
					par->cp_local_addr = (char*)string;
					})),
			M0_STRINGARG('h', "HA address",
					LAMBDA(void, (const char *str) {
						par->cp_ha_addr = (char*)str;
					})),
			M0_STRINGARG('f', "Process FID",
					LAMBDA(void, (const char *str) {
						par->cp_proc_fid = (char*)str;
					})),
			M0_STRINGARG('p', "Profile options for Clovis",
					LAMBDA(void, (const char *str) {
						par->cp_prof = (char*)str;
					})));
	if (rc != 0)
		return M0_ERR(rc);
	/* All mandatory params must be defined. */
	if (rc == 0 &&
	    (par->cp_local_addr == NULL || par->cp_ha_addr == NULL ||
	     par->cp_prof == NULL || par->cp_proc_fid == NULL)) {
		usage();
		rc = M0_ERR(-EINVAL);
	}
	*argc -= 9;
	*(argv) = arg + 9;
	return rc;
}

int main(int argc, char **argv)
{
	int                  rc;
	int                  subid;
	struct clovis_params params;

	m0_instance_setup(&instance);
	rc = m0_module_init(&instance.i_self, M0_LEVEL_INST_ONCE);
	if (rc != 0) {
		m0_console_printf("Cannot init module %i\n", rc);
		return M0_ERR(rc);
	}
	rc = opts_get(&params, &argc, &argv);

	if (rc == 0)
		rc = subsystem_id(argv[0]);
	if (rc == 0) {
		subid = rc;
		rc = subsystems[subid].s_init(&params);
		if (rc == 0) {
			rc = subsystems[subid].s_execute(argc + 1, argv + 1);
			if (rc != 0)
				m0_console_printf("Execution result %i\n", rc);
			else
				rc = 0;
			subsystems[subid].s_fini();
		}
		else
			m0_console_printf("Initialization error %i\n", rc);
	}
	if (rc < 0) {
		m0_console_printf("Got error %i\n", rc);
		/* main() should return positive values. */
		rc = -rc;
	}
	m0_console_printf("Done, rc:  %i\n", rc);
	return rc;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of clovis group */

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
