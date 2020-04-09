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

#pragma once

#ifndef __MERO_LIB_USER_SPACE_GETOPTS_H__
#define __MERO_LIB_USER_SPACE_GETOPTS_H__

#include "lib/types.h"

/**
   @defgroup getopts M0 getopt(3) version from heaven.

   m0_getopts() is a higher-level analogue of a standard getopt(3) function. The
   interface is designed to avoid proliferation of nearly identical code
   fragments, typical for getopt(3) usage (switches nested in loops) and to hide
   global state exposed by getopt(3).

   m0_getopts() interface is especially convenient when used together with
   anonymous functions (see LAMBDA() macro in lib/user_space/thread.h).

   m0_getopts() is implemented on top of getopt(3).

   @note the final 's' in m0_getopts() is to avoid file-name clashes with
   standard library headers.

   @see lib/ut/getopts.c for usage examples.

   @{
 */

/**
   Types of options supported by m0_getopts().
 */
enum m0_getopts_opt_type {
	/**
	    An option without an argument.

	    When this option is encountered, its call-back
	    m0_getopts_opt::go_u::got_void() is executed for its side-effects.
	 */
	GOT_VOID,
	/**
	   An option with a numerical argument. The argument is expected in the
	   format that strtoull(..., 0) can parse. When this option is
	   encountered, its call-back m0_getopts_opt::go_u::got_number() is
	   invoked with the parsed argument as its sole parameter.
	 */
	GOT_NUMBER,
	/**
	   An option with a numerical argument, followed by a optional
	   multiplier suffix.  The argument is expected in the format
	   that m0_bcount_get() can parse.  When this option is encountered,
	   its call-back m0_getopts_opt::go_u::got_scaled() is invoked with
	   the parsed argument as its sole parameter.
	 */
	GOT_SCALED,
	/**
	   An option with a string argument. When this option is encountered,
	   its call-back m0_getopts_opt::go_u::got_string() is invoked with the
	   string argument as its sole parameter.
	 */
	GOT_STRING,
	/**
	   An options with an argument with a format that can be parsed by
	   scanf(3). The argument string is parsed by a call to sscanf(3) with a
	   caller-supplied format string and caller-supplied address to store
	   the result at. No call-back is invoked. The caller is expected to
	   analyse the contents of the address after m0_getopts() returns.
	 */
	GOT_FORMAT,
	/**
	   An option without an argument, serving as a binary flag. When this
	   option is encountered, a user supplied boolean stored at
	   m0_getopts_opt::go_u::got_flag is set to true. If the option wasn't
	   encountered, the flag is set to false. No call-back is invoked. The
	   user is expected to inspect the flag after m0_getopts() returns.
	 */
	GOT_FLAG,
	/** An option without an argument.

	    When this option encountered, program usage is printed to STDERR and
	    program terminates immediately with exit(3).
	 */
	GOT_HELP
};

/**
   A description of an option, recognized by m0_getopts().

   Callers are not supposed to construct these explicitly. M0_*ARG() macros,
   defined below, are used instead.

   @see M0_VOIDARG
   @see M0_NUMBERARG
   @see M0_SCALEDARG
   @see M0_STRINGARG
   @see M0_FORMATARG
   @see M0_FLAGARG
 */
struct m0_getopts_opt {
	enum m0_getopts_opt_type go_type;
	/** Option character. */
	char                     go_opt;
	/** Human-readable option description. Used in error messages. */
	const char              *go_desc;
	/** Option-type specific data. */
	union m0_getopts_union {
		/** Call-back invoked for a GOT_VOID option. */
		void   (*got_void)(void);
		/** Call-back invoked for a GOT_NUMBER option. */
		void   (*got_number)(int64_t num);
		/** Call-back invoked for a GOT_SCALED option. */
		void   (*got_scaled)(m0_bcount_t num);
		/** Call-back invoked for a GOT_STRING option. */
		void   (*got_string)(const char *string);
		struct {
			/** Format string for a GOT_FORMAT option. */
			const char *f_string;
			/** Address to store parsed argument for a GOT_FORMAT
			    option. */
			void       *f_out;
		}        got_fmt;
		/** Address of a boolean flag for a GOT_FLAG option. */
		bool    *got_flag;
	} go_u;
};

/**
   Parses command line stored in argv[] array with argc elements according to a
   traditional UNIX/POSIX fashion.

   Recognized options are supplied in opts[] array with nr elements.

   When a parsing error occurs (unrecognized option, invalid argument format,
   etc.), a error message is printed on stderr, followed by a usage summary. The
   summary enumerates all the recognized options, their argument requirements
   and human-readable descriptions. Caller-supplied progname is used as a prefix
   of error messages.

   @note -W option is reserved by POSIX.2. GNU getopt() uses -W as a long option
   escape. Do not use it.
 */
int m0_getopts(const char *progname, int argc, char *const *argv,
	       const struct m0_getopts_opt *opts, unsigned nr);

/**
   A wrapper around m0_getopts(), calculating the size of options array.
 */
#define M0_GETOPTS(progname, argc, argv, ...)				\
	m0_getopts((progname), (argc), (argv),				\
		(const struct m0_getopts_opt []){ __VA_ARGS__ },	\
	   ARRAY_SIZE(((const struct m0_getopts_opt []){ __VA_ARGS__ })))

/**
   Defines a GOT_VOID option, with a given description and a call-back.
 */
#define M0_VOIDARG(ch, desc, func) {		\
	.go_type = GOT_VOID,			\
	.go_opt  = (ch),			\
	.go_desc = (desc),			\
	.go_u    = { .got_void = (func) }	\
}

/**
   Defines a GOT_NUMBER option, with a given description and a call-back.
 */
#define M0_NUMBERARG(ch, desc, func) {		\
	.go_type = GOT_NUMBER,			\
	.go_opt  = (ch),			\
	.go_desc = (desc),			\
	.go_u    = { .got_number = (func) }	\
}

/**
   Defines a GOT_SCALED option, with a given description and a call-back.
 */
#define M0_SCALEDARG(ch, desc, func) {		\
	.go_type = GOT_SCALED,			\
	.go_opt  = (ch),			\
	.go_desc = (desc),			\
	.go_u    = { .got_scaled = (func) }	\
}

/**
   Defines a GOT_STRING option, with a given description and a call-back.
 */
#define M0_STRINGARG(ch, desc, func) {		\
	.go_type = GOT_STRING,			\
	.go_opt  = (ch),			\
	.go_desc = (desc),			\
	.go_u    = { .got_string = (func) }	\
}

/**
   Defines a GOT_FORMAT option, with a given description, argument format and
   address.
 */
#define M0_FORMATARG(ch, desc, fmt, ptr) {			\
	.go_type = GOT_FORMAT,					\
	.go_opt  = (ch),					\
	.go_desc = (desc),					\
	.go_u    = {						\
		.got_fmt = {					\
			.f_string = (fmt), .f_out = (ptr)	\
		}						\
	}							\
}

/**
   Defines a GOT_FLAG option, with a given description and a flag address.
 */
#define M0_FLAGARG(ch, desc, ptr) {		\
	.go_type = GOT_FLAG,			\
	.go_opt  = (ch),			\
	.go_desc = (desc),			\
	.go_u    = { .got_flag = (ptr) }	\
}

/**
   Defines a GOT_HELP option.
 */
#define M0_HELPARG(ch) {			\
	.go_type = GOT_HELP,			\
	.go_opt  = (ch),			\
	.go_desc = "display this help and exit",\
	.go_u    = { .got_void = NULL }		\
}

/** @} end of getopts group */
#endif /* __MERO_LIB_USER_SPACE_GETOPTS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
