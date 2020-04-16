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
 * Original creation date: 08/12/2010
 */

#pragma once

#ifndef __MERO_LIB_TRACE_H__
#define __MERO_LIB_TRACE_H__

#include <stdarg.h>

#include "lib/types.h"
#include "lib/arith.h"
#include "lib/atomic.h"
#include "lib/time.h"    /* m0_time_t, printf_check */
#include "lib/misc.h"    /* M0_CAT */
#include "mero/magic.h"  /* M0_TRACE_DESCR_MAGIC */

#ifndef __KERNEL__
#include "lib/user_space/trace.h"
#include <sys/user.h>    /* PAGE_SIZE */
#endif

/**
   @defgroup trace Tracing

   See doc/logging-and-tracing.

   <b>Fast and light-weight tracing facility</b>

   The purpose of tracing module is to provide an interface usable for the
   following purposes:

       - temporary tracing to investigate and hunt down bugs and

       - always-on tracing used to postmortem analysis of a Mero
         installation.

   Always-on mode must be non-intrusive and light-weight, otherwise users would
   tend to disable it. On the other hand, the interface should be convenient to
   use (i.e., to place trace points and to analyze a trace) otherwise
   programmers would tend to ignore it. These conflicting requirements lead to
   a implementation subtler than one might expect.

   Specifically, the tracing module should conform to the following
   requirements:

       - minimal synchronization between threads;

       - minimal amount of data-copying and, more generally, minimal processor
         cost of producing a trace record;

       - minimal instruction cache foot-print of tracing calls;

       - printf-like interface.

   <b>Usage</b>

   Users produce trace records by calling M0_LOG() macro like

   @code
   M0_LOG(M0_INFO, "Cached value found: %llx, attempt: %i", foo->f_val, i);
   @endcode

   These records are placed in a shared cyclic buffer. The buffer can be
   "parsed", producing for each record formatted output together with additional
   information:

       - file, line and function (__FILE__, __LINE__ and __func__) for M0_LOG()
         call,

       - processor-dependent time-stamp.

   Parsing occurs in the following situations:

       - synchronously when a record is produced, or

       - asynchronously by a background thread, or

       - after the process (including a kernel) that generated records
         crashed. To this end the cyclic buffer is backed up by a memory mapped
         file (only for user space processes).

   <b>Implementation</b>

   To minimize processor cost of tracing, the implementation avoids run-time
   interpretation of format string. Instead a static (in C language sense)
   record descriptor (m0_trace_descr) is created, which contains all the static
   information about the trace point: file, line, function, format string. The
   record, placed in the cyclic buffer, contains only time-stamp, arguments
   (foo->f_val, i in the example above) and the pointer to the
   descriptor. Substitution of arguments into the format string happens during
   record parsing. This approach poses two problems:

       - how to copy arguments (which are variable in number and size) to the
         cyclic buffer and

       - how to estimate the number of bytes that have to be allocated in the
         buffer for this copying.

   Both problems are solved by means of ugly preprocessor tricks and gcc
   extensions. For a list of arguments A0, A1, ..., An, one of M0_LOG{n}()
   macros defines a C type declaration
@code
       struct t_body { typeof(A0) v0; typeof(A1) v1; ... typeof(An) vn; };
@endcode

   This declaration is used to

       - find how much space in the cyclic buffer is needed to store the
         arguments: sizeof(struct t_body);

       - to copy all the arguments into allocated space:
@code
        *(struct t_body *)space = (struct t_body){ A0, ..., An }
@endcode
         This uses C99 struct literal syntax.

   In addition, M0_LOG{n}() macro produces 2 integer arrays:

@code
       { offsetof(struct t_body, v0), ..., offsetof(struct t_body, vn) };
       { sizeof(a0), ..., sizeof(an) };
@endcode

   These arrays are used during parsing to extract the arguments from the
   buffer.

   @{
 */

/**
   M0_LOG(level, fmt, ...) is the main user interface for the tracing. It
   accepts the arguments in printf(3) format for the numbers, but there are some
   tricks for string arguments.

   String arguments should be specified like this:

   @code
   M0_LOG(M0_DEBUG, "%s", (char *)"foo");
   @endcode

   i.e. explicitly typecast to the pointer. It is because typeof("foo")
   is not the same as typeof((char*)"foo").

   @note The number of arguments after fmt is limited to 9

   M0_LOG() counts the number of arguments and calls correspondent M0_LOGx().
 */
#define M0_LOG(level, ...) \
	M0_CAT(M0_LOG, M0_COUNT_PARAMS(__VA_ARGS__))(level, __VA_ARGS__)

#define M0_ENTRY(...) M0_LOG(M0_CALL, "> " __VA_ARGS__)
#define M0_LEAVE(...) M0_LOG(M0_CALL, "< " __VA_ARGS__)

#define M0_RC(rc) ({                        \
	typeof(rc) __rc = (rc);             \
	M0_LOG(M0_CALL, "< rc=%d", __rc);   \
	__rc;                               \
})

#define M0_ERR(rc) ({                        \
	typeof(rc) __rc = (rc);              \
	M0_ASSERT(__rc != 0);                \
	M0_LOG(M0_ERROR, "<! rc=%d", __rc);  \
	__rc;                                \
})

#define M0_RC_INFO(rc, fmt, ...) ({                                \
	typeof(rc) __rc = (rc);                                    \
	M0_LOG(M0_CALL, "< rc=%d " fmt, __rc, ## __VA_ARGS__ );    \
	__rc;                                                      \
})

#define M0_ERR_INFO(rc, fmt, ...) ({                               \
	typeof(rc) __rc = (rc);                                    \
	M0_ASSERT(__rc != 0);                                      \
	M0_LOG(M0_ERROR, "<! rc=%d " fmt, __rc, ## __VA_ARGS__ );  \
	__rc;                                                      \
})

M0_INTERNAL int m0_trace_init(void);
M0_INTERNAL void m0_trace_fini(void);
M0_INTERNAL int m0_trace_set_immediate_mask(const char *mask);
M0_INTERNAL int m0_trace_set_print_context(const char *ctx_name);
M0_INTERNAL int m0_trace_set_level(const char *level);

/**
 * The subsystems definitions.
 *
 * @note: Such kind of definition (via defines) allow to keep enum
 *        and string array in sync.
 *
 * XXX: Until we have first real release, please keep the lower list sorted.
 *      After release, all new subsystems should be added to the end of list in
 *      order to provide compatibility between trace logs of different software
 *      versions.
 */
#define M0_TRACE_SUBSYSTEMS      \
  M0_TRACE_SUBSYS(OTHER,      0) \
  M0_TRACE_SUBSYS(LIB,        1) \
  M0_TRACE_SUBSYS(UT,         2) \
				 \
  M0_TRACE_SUBSYS(ADDB,       3) \
  M0_TRACE_SUBSYS(ADSTOB,     4) \
  M0_TRACE_SUBSYS(BALLOC,     5) \
  M0_TRACE_SUBSYS(BE,         6) \
  M0_TRACE_SUBSYS(BTREE,      7) \
  M0_TRACE_SUBSYS(CAS,        8) \
  M0_TRACE_SUBSYS(CLOVIS,     9) \
  M0_TRACE_SUBSYS(CM,        10) \
  M0_TRACE_SUBSYS(COB,       11) \
  M0_TRACE_SUBSYS(CONF,      12) \
  M0_TRACE_SUBSYS(CONSOLE,   13) \
  M0_TRACE_SUBSYS(DIX,       14) \
  M0_TRACE_SUBSYS(DIXCM,     15) \
  M0_TRACE_SUBSYS(DTM,       16) \
  M0_TRACE_SUBSYS(EXTMAP,    17) \
  M0_TRACE_SUBSYS(FD,        18) \
  M0_TRACE_SUBSYS(FILE,      19) \
  M0_TRACE_SUBSYS(FOL,       20) \
  M0_TRACE_SUBSYS(FOP,       21) \
  M0_TRACE_SUBSYS(FORMATION, 22) \
  M0_TRACE_SUBSYS(HA,        23) \
  M0_TRACE_SUBSYS(IOSERVICE, 24) \
  M0_TRACE_SUBSYS(LAYOUT,    25) \
  M0_TRACE_SUBSYS(LNET,      26) \
  M0_TRACE_SUBSYS(M0D,       27) \
  M0_TRACE_SUBSYS(M0T1FS,    28) \
  M0_TRACE_SUBSYS(MDS,       29) \
  M0_TRACE_SUBSYS(MEMORY,    30) \
  M0_TRACE_SUBSYS(MGMT,      31) \
  M0_TRACE_SUBSYS(NET,       32) \
  M0_TRACE_SUBSYS(POOL,      33) \
  M0_TRACE_SUBSYS(RM,        34) \
  M0_TRACE_SUBSYS(RPC,       35) \
  M0_TRACE_SUBSYS(SM,        36) \
  M0_TRACE_SUBSYS(SNS,       37) \
  M0_TRACE_SUBSYS(SNSCM,     38) \
  M0_TRACE_SUBSYS(SPIEL,     39) \
  M0_TRACE_SUBSYS(SSS,       40) \
  M0_TRACE_SUBSYS(STATS,     41) \
  M0_TRACE_SUBSYS(STOB,      42) \
  M0_TRACE_SUBSYS(XCODE,     43) \
  M0_TRACE_SUBSYS(FDMI,      44) \
  M0_TRACE_SUBSYS(ISCS,      45)

#define M0_TRACE_SUBSYS(name, value) M0_TRACE_SUBSYS_ ## name = (1UL << value),
/** The subsystem bitmask definitions */
enum m0_trace_subsystem {
	M0_TRACE_SUBSYSTEMS
};

/**
 * The subsystems bitmask of what should be printed immediately
 * to the console.
 */
extern unsigned long m0_trace_immediate_mask;

/**
 * Controls whether to display additional trace point info, like
 * timestamp, subsystem, file, func, etc.
 *
 * Acceptable values are elements of enum m0_trace_print_context.
 */
extern unsigned int m0_trace_print_context;

/**
 * Controls which M0_LOG() messages are displayed on console when
 * M0_TRACE_IMMEDIATE_DEBUG is enabled.
 *
 * If log level of trace point is less or equal to m0_trace_level, then it's
 * displayed, otherwise not.
 *
 * By default m0_trace_level is set to M0_WARN. Also, @see documentation of
 * M0_CALL log level, it has special meaning.
 */
extern unsigned int m0_trace_level;

/*
 * Below is the internal implementation stuff.
 */

enum {
	/** Size, reserved for trace buffer header */
	M0_TRACE_BUF_HEADER_SIZE = PAGE_SIZE,
	/** Alignment for trace records in trace buffer */
	M0_TRACE_REC_ALIGN = 8, /* word size on x86_64 */
};
M0_BASSERT(M0_TRACE_BUF_HEADER_SIZE % PAGE_SIZE == 0);

extern struct m0_trace_buf_header *m0_logbuf_header; /**< Trace buffer header pointer */
extern void      *m0_logbuf;        /**< Trace buffer pointer */

enum m0_trace_buf_type {
	M0_TRACE_BUF_KERNEL = 1,
	M0_TRACE_BUF_USER   = 2,
};

enum m0_trace_buf_flags {
	M0_TRACE_BUF_MKFS  = 1 << 0,
	M0_TRACE_BUF_DIRTY = 1 << 1,

	M0_TRACE_BUF_FLAGS_MAX
};
M0_BASSERT(M0_TRACE_BUF_FLAGS_MAX < UINT16_MAX);

/**
 * Trace buffer header structure
 *
 * It's placed at the beginning of a trace buffer in a reserved area of
 * M0_TRACE_BUF_HEADER_SIZE size.
 */
struct m0_trace_buf_header {
	union {
		struct {
			/* XXX: don't change fields order */
			/* XXX: new fields should be added at the end */

			uint64_t                tbh_magic;
			/** Trace header address */
			const void             *tbh_header_addr;
			/**
			 * Size, reserved for trace buffer header (NOTE: not a
			 * size of this structure). Effectively, this is an
			 * offset within trace file, starting from which actual
			 * trace records begin.
			 */
			uint32_t                tbh_header_size;
			/** Trace buffer address */
			const void             *tbh_buf_addr;
			/** Trace buffer size */
			uint64_t                tbh_buf_size;
			/** enum m0_trace_buf_type */
			uint16_t                tbh_buf_type;
			/** enum m0_trace_buf_flags */
			uint16_t                tbh_buf_flags;
			/**
			 * Current position in trace buffer, @see comments in
			 * m0_trace_allot()
			 */
			struct m0_atomic64      tbh_cur_pos;
			/** Record counter */
			struct m0_atomic64      tbh_rec_cnt;
			/**
			 * Address of special trace magic symbol (used for trace
			 * buffer decoding)
			 */
			const void             *tbh_magic_sym_addr;
			/** Mero version string */
			char                    tbh_mero_version[16];
			/** Git describe revision ID */
			char                    tbh_mero_git_describe[64];
			/** Kernel version, for which Mero is built */
			char                    tbh_mero_kernel_ver[128];
			/** Trace file creation time and date */
			m0_time_t               tbh_log_time;
			/** m0mero.ko module struct (used only for kernel trace) */
			const void             *tbh_module_struct;
			/**
			 * Address of loaded m0mero.ko module (used only for
			 * kernel trace)
			 */
			const void             *tbh_module_core_addr;
			/** Size of loaded m0mero.ko module (used only for kernel trace) */
			unsigned int            tbh_module_core_size;
			/** Command line arguments string (only for user trace) */
			char                    tbh_cli_args[1024];
			/**
			 * Actual number of magic symbol addresses stored in
			 * tbh_magic_sym_addresses array, see
			 * m0_trace_magic_sym_extra_addr_add()
			 */
			uint16_t                tbh_magic_sym_addresses_nr;
			/** Additional magic symbols for external libraries */
			const void             *tbh_magic_sym_addresses[128];

			/* XXX: add new field right above this line */
		};
		char    tbh_header_area[M0_TRACE_BUF_HEADER_SIZE];
	};
};
M0_BASSERT(sizeof (struct m0_trace_buf_header) == M0_TRACE_BUF_HEADER_SIZE);

/**
 * Record header structure
 *
 * - magic number to locate the record in buffer
 * - stack pointer - useful to distinguish between threads
 * - global record number
 * - timestamp
 * - pointer to record description in the program file
 */
struct m0_trace_rec_header {
	/* XXX: don't change fields order */
	/* XXX: new fields should be added to the end */
	uint64_t                     trh_magic;
	uint64_t                     trh_sp; /**< stack pointer */
	uint64_t                     trh_no; /**< record # */
	uint64_t                     trh_pos; /**< abs record pos in logbuf */
	uint64_t                     trh_timestamp;
	const struct m0_trace_descr *trh_descr;
	uint32_t                     trh_string_data_size;
	uint32_t                     trh_record_size; /**< total record size */
	pid_t                        trh_pid; /**< current PID */
};

/**
 * Trace levels. To be used as a first argument of M0_LOG().
 */
enum m0_trace_level {
	/**
	 * special level, which represents an invalid trace level,
	 * should _not_ be used directly with M0_LOG()
	 */
	M0_NONE   = 0,

	/**
	 * special level, it's always displayed on console, intended for
	 * printf-style debugging, should be removed from code before committing
	 */
	M0_ALWAYS = 1 << 0,

	/** system is unusable and not able to perform it's basic function */
	M0_FATAL  = 1 << 1,
	/**
	 * local error condition, i.e.: resource unavailable, no connectivity,
	 * incorrect value, etc.
	 */
	M0_ERROR  = 1 << 2,
	/**
	 * warning condition, i.e.: condition which requires some special
	 * treatment, something which not happens normally, corner case, etc.
	 */
	M0_WARN   = 1 << 3,
	/** normal but significant condition */
	M0_NOTICE = 1 << 4,
	/** some useful information about current system's state */
	M0_INFO   = 1 << 5,
	/**
	 * lower-level, detailed information to aid debugging and analysis of
	 * incorrect behavior
	 */
	M0_DEBUG  = 1 << 6,

	/**
	 * special level, used only with M0_ENTRY() and M0_LEAVE() to trace
	 * function calls, should _not_ be used directly with M0_LOG();
	 */
	M0_CALL   = 1 << 7,
};

enum m0_trace_print_context {
	M0_TRACE_PCTX_NONE  = 0,
	M0_TRACE_PCTX_FUNC  = 1,
	M0_TRACE_PCTX_SHORT = 2,
	M0_TRACE_PCTX_FULL  = 3,

	M0_TRACE_PCTX_INVALID
};

struct m0_trace_descr {
	uint64_t             td_magic;
	const char          *td_fmt;
	const char          *td_func;
	const char          *td_file;
	uint64_t             td_subsys;
	int                  td_line;
	int                  td_size;
	int                  td_nr;
	const int           *td_offset;
	const int           *td_sizeof;
	const bool          *td_isstr;
	enum m0_trace_level  td_level;
	bool                 td_hasstr;
};

M0_INTERNAL void m0_trace_allot(const struct m0_trace_descr *td,
				const void *data);
M0_INTERNAL void m0_trace_record_print(const struct m0_trace_rec_header *trh,
				       const void *buf);

M0_INTERNAL void m0_trace_print_subsystems(void);

__attribute__ ((format (printf, 1, 2)))
M0_INTERNAL void m0_console_printf(const char *fmt, ...);

__attribute__ ((format (printf, 1, 2)))
M0_INTERNAL void m0_error_printf(const char *fmt, ...);

M0_INTERNAL
int  m0_trace_record_print_yaml(char *outbuf, size_t outbuf_size,
				const struct m0_trace_rec_header *trh,
				const void *tr_body, bool yaml_stream_mode);

/*
 * The code below abuses C preprocessor badly. Looking at it might be damaging
 * to your eyes and sanity.
 */

/**
 * This is a low-level entry point into tracing sub-system.
 *
 * Don't call this directly, use M0_LOG() macros instead.
 *
 * Add a fixed-size trace entry into the trace buffer.
 *
 * @param LEVEL a log level
 * @param NR the number of arguments
 * @param DECL C definition of a trace entry format
 * @param OFFSET the set of offsets of each argument
 * @param SIZEOF the set of sizes of each argument
 * @param ISSTR  the set of bool flags, which indicate whether corresponding
 *               argument is a string
 * @param HASSTR bool flag, which is set true iff there is at least one string
 *               argument present
 * @param FMT the printf-like format string
 * @note The variadic arguments must match the number
 *       and types of fields in the format.
 */
#define M0_TRACE_POINT(LEVEL, NR, DECL, OFFSET, SIZEOF, ISSTR, HASSTR, FMT, ...)\
({									\
	struct t_body DECL;						\
	static const int  _offset[NR] = OFFSET;				\
	static const int  _sizeof[NR] = SIZEOF;				\
	static const bool _isstr[NR]  = ISSTR;				\
	static const struct m0_trace_descr __trace_descr = {		\
		.td_magic  = M0_TRACE_DESCR_MAGIC,			\
		.td_level  = (LEVEL),					\
		.td_fmt    = (FMT),					\
		.td_func   = __func__,					\
		.td_file   = __FILE__,					\
		.td_line   = __LINE__,					\
		.td_subsys = M0_TRACE_SUBSYSTEM,			\
		.td_size   = sizeof(struct t_body),			\
		.td_nr     = (NR),					\
		.td_offset = _offset,					\
		.td_sizeof = _sizeof,					\
		.td_isstr  = _isstr,					\
		.td_hasstr = (HASSTR),					\
	};								\
	printf_check(FMT , ## __VA_ARGS__);				\
	m0_trace_allot(&__trace_descr, &(const struct t_body){ __VA_ARGS__ });\
})

#ifndef M0_TRACE_SUBSYSTEM
#  define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER
#endif

enum {
	M0_TRACE_ARGC_MAX = 9
};

/*
 *  Helpers for M0_LOG{n}().
 */
#define LOG_TYPEOF(a, v) typeof(a) v
#define LOG_OFFSETOF(v) offsetof(struct t_body, v)
#define LOG_SIZEOF(a) sizeof(a)
#define LOG_IS_STR_ARG(a)			    \
		M0_HAS_TYPE((a), char*)        ?:   \
		M0_HAS_TYPE((a), const char*)  ?:   \
		M0_HAS_TYPE((a), char[])       ?:   \
		M0_HAS_TYPE((a), const char[]) ?: false


#define LOG_CHECK(a)							\
M0_CASSERT(!M0_HAS_TYPE(a, const char []) &&				\
	   (sizeof(a) == 1 || sizeof(a) == 2 || sizeof(a) == 4 ||	\
	    sizeof(a) == 8))

/**
 * LOG_GROUP() is used to pass { x0, ..., xn } as a single argument to
 * M0_TRACE_POINT().
 */
#define LOG_GROUP(...) __VA_ARGS__

#define M0_LOG0(level, fmt) \
	M0_TRACE_POINT(level, 0, { ; }, {}, {}, {}, false, fmt)

#define M0_LOG1(level, fmt, a0)						\
({ M0_TRACE_POINT(level, 1,						\
   { LOG_TYPEOF(a0, v0); },						\
   { LOG_OFFSETOF(v0) },						\
   { LOG_SIZEOF(a0) },							\
   { LOG_IS_STR_ARG(a0) },						\
   LOG_IS_STR_ARG(a0),							\
   fmt, a0);								\
   LOG_CHECK(a0); })

#define M0_LOG2(level, fmt, a0, a1)					\
({ M0_TRACE_POINT(level, 2,						\
   { LOG_TYPEOF(a0, v0); LOG_TYPEOF(a1, v1); },				\
   LOG_GROUP({ LOG_OFFSETOF(v0), LOG_OFFSETOF(v1) }),			\
   LOG_GROUP({ LOG_SIZEOF(a0), LOG_SIZEOF(a1) }),			\
   LOG_GROUP({ LOG_IS_STR_ARG(a0), LOG_IS_STR_ARG(a1) }),		\
   LOG_IS_STR_ARG(a0) || LOG_IS_STR_ARG(a1),				\
   fmt, a0, a1);							\
   LOG_CHECK(a0); LOG_CHECK(a1); })

#define M0_LOG3(level, fmt, a0, a1, a2)					\
({ M0_TRACE_POINT(level, 3,						\
   { LOG_TYPEOF(a0, v0); LOG_TYPEOF(a1, v1); LOG_TYPEOF(a2, v2); },	\
   LOG_GROUP({ LOG_OFFSETOF(v0), LOG_OFFSETOF(v1), LOG_OFFSETOF(v2) }),	\
   LOG_GROUP({ LOG_SIZEOF(a0), LOG_SIZEOF(a1), LOG_SIZEOF(a2) }),	\
   LOG_GROUP({ LOG_IS_STR_ARG(a0), LOG_IS_STR_ARG(a1),			\
               LOG_IS_STR_ARG(a2) }),					\
   LOG_IS_STR_ARG(a0) || LOG_IS_STR_ARG(a1) || LOG_IS_STR_ARG(a2),	\
   fmt, a0, a1, a2);							\
   LOG_CHECK(a0); LOG_CHECK(a1); LOG_CHECK(a2); })

#define M0_LOG4(level, fmt, a0, a1, a2, a3)				\
({ M0_TRACE_POINT(level, 4,						\
   { LOG_TYPEOF(a0, v0); LOG_TYPEOF(a1, v1); LOG_TYPEOF(a2, v2);	\
     LOG_TYPEOF(a3, v3); },						\
   LOG_GROUP({ LOG_OFFSETOF(v0), LOG_OFFSETOF(v1), LOG_OFFSETOF(v2),	\
               LOG_OFFSETOF(v3) }),					\
   LOG_GROUP({ LOG_SIZEOF(a0), LOG_SIZEOF(a1), LOG_SIZEOF(a2),		\
               LOG_SIZEOF(a3) }),					\
   LOG_GROUP({ LOG_IS_STR_ARG(a0), LOG_IS_STR_ARG(a1),			\
               LOG_IS_STR_ARG(a2), LOG_IS_STR_ARG(a3)  }),		\
   LOG_IS_STR_ARG(a0) || LOG_IS_STR_ARG(a1) || LOG_IS_STR_ARG(a2) ||	\
	LOG_IS_STR_ARG(a3),							\
   fmt, a0, a1, a2, a3);						\
   LOG_CHECK(a0); LOG_CHECK(a1); LOG_CHECK(a2); LOG_CHECK(a3); })

#define M0_LOG5(level, fmt, a0, a1, a2, a3, a4)				\
({ M0_TRACE_POINT(level, 5,						\
   { LOG_TYPEOF(a0, v0); LOG_TYPEOF(a1, v1); LOG_TYPEOF(a2, v2);	\
     LOG_TYPEOF(a3, v3); LOG_TYPEOF(a4, v4); },				\
   LOG_GROUP({ LOG_OFFSETOF(v0), LOG_OFFSETOF(v1), LOG_OFFSETOF(v2),	\
               LOG_OFFSETOF(v3), LOG_OFFSETOF(v4) }),			\
   LOG_GROUP({ LOG_SIZEOF(a0), LOG_SIZEOF(a1), LOG_SIZEOF(a2),		\
               LOG_SIZEOF(a3), LOG_SIZEOF(a4) }),			\
   LOG_GROUP({ LOG_IS_STR_ARG(a0), LOG_IS_STR_ARG(a1),			\
               LOG_IS_STR_ARG(a2), LOG_IS_STR_ARG(a3),			\
               LOG_IS_STR_ARG(a4) }),					\
   LOG_IS_STR_ARG(a0) || LOG_IS_STR_ARG(a1) || LOG_IS_STR_ARG(a2) ||	\
	LOG_IS_STR_ARG(a3) || LOG_IS_STR_ARG(a4),			\
   fmt, a0, a1, a2, a3, a4);						\
   LOG_CHECK(a0); LOG_CHECK(a1); LOG_CHECK(a2); LOG_CHECK(a3);		\
   LOG_CHECK(a4); })

#define M0_LOG6(level, fmt, a0, a1, a2, a3, a4, a5)			\
({ M0_TRACE_POINT(level, 6,						\
   { LOG_TYPEOF(a0, v0); LOG_TYPEOF(a1, v1); LOG_TYPEOF(a2, v2);	\
     LOG_TYPEOF(a3, v3); LOG_TYPEOF(a4, v4); LOG_TYPEOF(a5, v5); },	\
   LOG_GROUP({ LOG_OFFSETOF(v0), LOG_OFFSETOF(v1), LOG_OFFSETOF(v2),	\
               LOG_OFFSETOF(v3), LOG_OFFSETOF(v4), LOG_OFFSETOF(v5) }),	\
   LOG_GROUP({ LOG_SIZEOF(a0), LOG_SIZEOF(a1), LOG_SIZEOF(a2),		\
               LOG_SIZEOF(a3), LOG_SIZEOF(a4), LOG_SIZEOF(a5) }),	\
   LOG_GROUP({ LOG_IS_STR_ARG(a0), LOG_IS_STR_ARG(a1),			\
               LOG_IS_STR_ARG(a2), LOG_IS_STR_ARG(a3),			\
               LOG_IS_STR_ARG(a4), LOG_IS_STR_ARG(a5) }),		\
   LOG_IS_STR_ARG(a0) || LOG_IS_STR_ARG(a1) || LOG_IS_STR_ARG(a2) ||	\
	LOG_IS_STR_ARG(a3) || LOG_IS_STR_ARG(a4) || LOG_IS_STR_ARG(a5),	\
   fmt, a0, a1, a2, a3, a4, a5);					\
   LOG_CHECK(a0); LOG_CHECK(a1); LOG_CHECK(a2); LOG_CHECK(a3);		\
   LOG_CHECK(a4); LOG_CHECK(a5); })

#define M0_LOG7(level, fmt, a0, a1, a2, a3, a4, a5, a6)			\
({ M0_TRACE_POINT(level, 7,						\
   { LOG_TYPEOF(a0, v0); LOG_TYPEOF(a1, v1); LOG_TYPEOF(a2, v2);	\
     LOG_TYPEOF(a3, v3); LOG_TYPEOF(a4, v4); LOG_TYPEOF(a5, v5);	\
     LOG_TYPEOF(a6, v6); },						\
   LOG_GROUP({ LOG_OFFSETOF(v0), LOG_OFFSETOF(v1), LOG_OFFSETOF(v2),	\
               LOG_OFFSETOF(v3), LOG_OFFSETOF(v4),			\
               LOG_OFFSETOF(v5), LOG_OFFSETOF(v6) }),			\
   LOG_GROUP({ LOG_SIZEOF(a0), LOG_SIZEOF(a1), LOG_SIZEOF(a2),		\
               LOG_SIZEOF(a3), LOG_SIZEOF(a4),				\
               LOG_SIZEOF(a5), LOG_SIZEOF(a6) }),			\
   LOG_GROUP({ LOG_IS_STR_ARG(a0), LOG_IS_STR_ARG(a1),			\
               LOG_IS_STR_ARG(a2), LOG_IS_STR_ARG(a3),			\
               LOG_IS_STR_ARG(a4), LOG_IS_STR_ARG(a5),			\
               LOG_IS_STR_ARG(a6) }),					\
   LOG_IS_STR_ARG(a0) || LOG_IS_STR_ARG(a1) || LOG_IS_STR_ARG(a2) ||	\
	LOG_IS_STR_ARG(a3) || LOG_IS_STR_ARG(a4) ||			\
	LOG_IS_STR_ARG(a5) || LOG_IS_STR_ARG(a6),			\
   fmt, a0, a1, a2, a3, a4, a5, a6);					\
   LOG_CHECK(a0); LOG_CHECK(a1); LOG_CHECK(a2); LOG_CHECK(a3);		\
   LOG_CHECK(a4); LOG_CHECK(a5); LOG_CHECK(a6); })

#define M0_LOG8(level, fmt, a0, a1, a2, a3, a4, a5, a6, a7)		\
({ M0_TRACE_POINT(level, 8,						\
   { LOG_TYPEOF(a0, v0); LOG_TYPEOF(a1, v1); LOG_TYPEOF(a2, v2);	\
     LOG_TYPEOF(a3, v3); LOG_TYPEOF(a4, v4); LOG_TYPEOF(a5, v5);	\
     LOG_TYPEOF(a6, v6); LOG_TYPEOF(a7, v7); },				\
   LOG_GROUP({ LOG_OFFSETOF(v0), LOG_OFFSETOF(v1), LOG_OFFSETOF(v2),	\
               LOG_OFFSETOF(v3), LOG_OFFSETOF(v4), LOG_OFFSETOF(v5),	\
               LOG_OFFSETOF(v6), LOG_OFFSETOF(v7) }),			\
   LOG_GROUP({ LOG_SIZEOF(a0), LOG_SIZEOF(a1), LOG_SIZEOF(a2),		\
               LOG_SIZEOF(a3), LOG_SIZEOF(a4), LOG_SIZEOF(a5),		\
               LOG_SIZEOF(a6), LOG_SIZEOF(a7) }),			\
   LOG_GROUP({ LOG_IS_STR_ARG(a0), LOG_IS_STR_ARG(a1),			\
               LOG_IS_STR_ARG(a2), LOG_IS_STR_ARG(a3),			\
               LOG_IS_STR_ARG(a4), LOG_IS_STR_ARG(a5),			\
               LOG_IS_STR_ARG(a6), LOG_IS_STR_ARG(a7) }),		\
   LOG_IS_STR_ARG(a0) || LOG_IS_STR_ARG(a1) || LOG_IS_STR_ARG(a2) ||	\
	LOG_IS_STR_ARG(a3) || LOG_IS_STR_ARG(a4) ||			\
	LOG_IS_STR_ARG(a5) || LOG_IS_STR_ARG(a6) ||			\
	LOG_IS_STR_ARG(a7),						\
   fmt, a0, a1, a2, a3, a4, a5, a6, a7);				\
   LOG_CHECK(a0); LOG_CHECK(a1); LOG_CHECK(a2); LOG_CHECK(a3);		\
   LOG_CHECK(a4); LOG_CHECK(a5); LOG_CHECK(a6); LOG_CHECK(a7); })

#define M0_LOG9(level, fmt, a0, a1, a2, a3, a4, a5, a6, a7, a8)		\
({ M0_TRACE_POINT(level, 9,						\
   { LOG_TYPEOF(a0, v0); LOG_TYPEOF(a1, v1); LOG_TYPEOF(a2, v2);	\
     LOG_TYPEOF(a3, v3); LOG_TYPEOF(a4, v4); LOG_TYPEOF(a5, v5);	\
     LOG_TYPEOF(a6, v6); LOG_TYPEOF(a7, v7); LOG_TYPEOF(a8, v8); },	\
   LOG_GROUP({ LOG_OFFSETOF(v0), LOG_OFFSETOF(v1), LOG_OFFSETOF(v2),	\
               LOG_OFFSETOF(v3), LOG_OFFSETOF(v4), LOG_OFFSETOF(v5),	\
               LOG_OFFSETOF(v6), LOG_OFFSETOF(v7), LOG_OFFSETOF(v8) }),	\
   LOG_GROUP({ LOG_SIZEOF(a0), LOG_SIZEOF(a1), LOG_SIZEOF(a2),		\
               LOG_SIZEOF(a3), LOG_SIZEOF(a4), LOG_SIZEOF(a5),		\
               LOG_SIZEOF(a6), LOG_SIZEOF(a7), LOG_SIZEOF(a8) }),	\
   LOG_GROUP({ LOG_IS_STR_ARG(a0), LOG_IS_STR_ARG(a1),			\
               LOG_IS_STR_ARG(a2), LOG_IS_STR_ARG(a3),			\
               LOG_IS_STR_ARG(a4), LOG_IS_STR_ARG(a5),			\
               LOG_IS_STR_ARG(a6), LOG_IS_STR_ARG(a7),			\
               LOG_IS_STR_ARG(a8) }),					\
   LOG_IS_STR_ARG(a0) || LOG_IS_STR_ARG(a1) || LOG_IS_STR_ARG(a2) ||	\
	LOG_IS_STR_ARG(a3) || LOG_IS_STR_ARG(a4) ||			\
	LOG_IS_STR_ARG(a5) || LOG_IS_STR_ARG(a6) ||			\
	LOG_IS_STR_ARG(a7) || LOG_IS_STR_ARG(a8),			\
   fmt, a0, a1, a2, a3, a4, a5, a6, a7, a8);				\
   LOG_CHECK(a0); LOG_CHECK(a1); LOG_CHECK(a2); LOG_CHECK(a3);		\
   LOG_CHECK(a4); LOG_CHECK(a5); LOG_CHECK(a6); LOG_CHECK(a7); LOG_CHECK(a8); })

/** @} end of trace group */
#endif /* __MERO_LIB_TRACE_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
