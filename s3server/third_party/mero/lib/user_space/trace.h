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
 * Original author: Andriy Tkachuk <Andriy_Tkachuk@xyratex.com>
 * Original creation date: 01/30/2012
 */

#pragma once

#ifndef __MERO_LIB_USERSP_TRACE_H__
#define __MERO_LIB_USERSP_TRACE_H__

#include <stdio.h>  /* FILE */

#include "lib/types.h"  /* pid_t */

/**
   @defgroup trace Tracing.

   User-space specific declarations.

 */

extern pid_t m0_pid_cached;

enum m0_trace_parse_flags {
	M0_TRACE_PARSE_HEADER_ONLY             = 1 << 0,
	M0_TRACE_PARSE_YAML_SINGLE_DOC_OUTPUT  = 1 << 1,

	M0_TRACE_PARSE_DEFAULT_FLAGS           = 0 /* all flags off */
};

M0_INTERNAL int m0_trace_parse(FILE *trace_file, FILE *output_file,
			       const char *m0mero_ko_path,
			       enum m0_trace_parse_flags flags,
			       const void *magic_symbols[],
			       unsigned int magic_symbols_nr);

M0_INTERNAL void m0_trace_set_mmapped_buffer(bool val);

int m0_trace_set_buffer_size(size_t size);

/** @} end of trace group */
#endif /* __MERO_LIB_USERSP_TRACE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
