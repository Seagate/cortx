/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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

#include <string.h>   /* memset, strlen */
#include <errno.h>
#include <err.h>      /* warn */
#include <sysexits.h> /* EX_* exit codes (EX_OSERR, EX_SOFTWARE) */
#include <stdio.h>
#include <stdlib.h>   /* getenv, strtoul */
#include <unistd.h>   /* getpagesize, getpid */
#include <fcntl.h>    /* open, O_RDWR|O_CREAT|O_TRUNC */
#include <sys/mman.h> /* mmap */
#include <sys/stat.h> /* fstat */
#include <limits.h>   /* CHAR_BIT */
#include <stddef.h>   /* ptrdiff_t */
#include <time.h>     /* strftime */

#include "lib/types.h"
#include "lib/arith.h"
#include "lib/memory.h"
#include "lib/string.h" /* m0_strdup */
#include "lib/trace.h"
#include "lib/trace_internal.h"
#include "lib/cookie.h" /* m0_addr_is_sane_and_aligned */

#include "mero/magic.h"

/**
   @addtogroup trace

   <b>User-space m0_trace_parse() implementation.</b>

   @{
 */

pid_t m0_pid_cached;

static int  logfd;
static bool use_mmaped_buffer = true;
static char trace_file_path[PATH_MAX];
static size_t trace_buf_size = M0_TRACE_UBUF_SIZE;

static int logbuf_map()
{
	struct m0_trace_area *trace_area;
	size_t trace_area_size = M0_TRACE_BUF_HEADER_SIZE + trace_buf_size;
	int    rc;

	M0_PRE((trace_area_size % m0_pagesize_get()) == 0);

	if (strlen(trace_file_path) == 0) {
		if (getcwd(trace_file_path, sizeof trace_file_path) == NULL ) {
			warn("failed to read CWD path, using relative path for"
			     " m0trace.PID file");
			memset(trace_file_path, 0, sizeof trace_file_path);
		} else {
			strncat(trace_file_path, "/", sizeof trace_file_path -
				strlen(trace_file_path) - 1);
		}

		int available_bytes = sizeof trace_file_path -
				      strlen(trace_file_path);
		rc = snprintf(trace_file_path + strlen(trace_file_path),
			available_bytes, "m0trace.%u", m0_pid_cached);
		if (rc < 0) {
			warn("failed to construct trace file path");
			return rc;
		}
		if (rc >= available_bytes) {
			warnx("failed to construct trace file path, not enough"
			      " space in the path buffer");
			return -ENOBUFS;
		}
	}

	if ((logfd = open(trace_file_path, O_RDWR|O_CREAT|O_TRUNC|O_CLOEXEC, 0700)) == -1) {
		warn("open(\"%s\")", trace_file_path);
	} else if ((errno = posix_fallocate(logfd, 0, trace_area_size)) != 0) {
		warn("fallocate(\"%s\", %zu)", trace_file_path, trace_area_size);
	} else if ((trace_area = mmap(NULL, trace_area_size, PROT_WRITE,
				      MAP_SHARED, logfd, 0)) == MAP_FAILED)
	{
		warn("mmap(\"%s\")", trace_file_path);
	} else {
		m0_logbuf_header = &trace_area->ta_header;
		m0_logbuf = trace_area->ta_buf;
		memset(trace_area, 0, trace_area_size);
		m0_trace_buf_header_init(&trace_area->ta_header, trace_buf_size);
		m0_trace_logbuf_size_set(trace_buf_size);
	}

	return -errno;
}

M0_INTERNAL const char *m0_trace_file_path_get(void)
{
	return trace_file_path;
}

M0_INTERNAL int m0_trace_set_immediate_mask(const char *mask)
{
	if (mask != NULL) {
		char *endp;

		m0_trace_immediate_mask = strtoul(mask, &endp, 0);

		/*
		 * if mask string fails to convert to a number cleanly, then
		 * assume that mask string contains a comma separated list of
		 * subsystem names, which we use to build a numeric mask
		 */
		if (errno != 0 || *endp != 0) {
			unsigned long  m = 0;
			int            rc;
			char          *s = m0_strdup(mask);

			if (s == NULL)
				return -ENOMEM;

			rc = m0_trace_subsys_list_to_mask(s, &m);
			m0_free(s);

			if (rc != 0)
				return rc;

			m0_trace_immediate_mask = m;
		}
	}

	return 0;
}

M0_INTERNAL void m0_trace_stats_update(uint32_t rec_size)
{
}

M0_INTERNAL void m0_trace_set_mmapped_buffer(bool val)
{
	use_mmaped_buffer = val;
}

M0_INTERNAL bool m0_trace_use_mmapped_buffer(void)
{
	return use_mmaped_buffer;
}

int m0_trace_set_buffer_size(size_t size)
{
	if (m0_pid_cached) {
		m0_error_printf("mero: can't set trace buffer size after it's"
				" been initialized\n");
		return -EBUSY;
	}

	if (size == 0 || !m0_is_po2(size) || size % m0_pagesize_get() != 0) {
		m0_error_printf("mero: incorrect value for trace buffer size"
		       " parameter (%zu), it can't be zero, should be a power of"
		       " 2 and a multiple of PAGE_SIZE(%u)\n",
		       size, m0_pagesize_get());
		return -EINVAL;
	}

	trace_buf_size = size;
	return 0;
}

static int set_trace_dir(const char *path)
{
	int rc;

	if (path == NULL)
		return 0;

	rc = snprintf(trace_file_path, sizeof trace_file_path, "%s/m0trace.%u",
		      path, m0_pid_cached);
	if (rc < 0) {
		warn("ignoring environment variable M0_TRACE_DIR - failed"
		     " to construct trace file path");
		memset(trace_file_path, 0, sizeof trace_file_path);
		return rc;
	}
	if (rc >= sizeof trace_file_path) {
		warnx("ignoring environment variable M0_TRACE_DIR - its content"
		      " is too long (%zu bytes), max allowed lengh is %zu bytes"
		      , strlen(path), (sizeof trace_file_path) - 16);
		memset(trace_file_path, 0, sizeof trace_file_path);
		return -EINVAL;
	}

	return 0;
}

M0_INTERNAL int m0_arch_trace_init()
{
	int         rc;
	const char *var;

	m0_pid_cached = getpid();

	var = getenv("M0_TRACE_IMMEDIATE_MASK");
	rc = m0_trace_set_immediate_mask(var);
	if (rc != 0)
		return rc;

	var = getenv("M0_TRACE_LEVEL");
	rc = m0_trace_set_level(var);
	if (rc != 0)
		return rc;

	var = getenv("M0_TRACE_PRINT_CONTEXT");
	rc = m0_trace_set_print_context(var);
	if (rc != 0)
		return rc;

	var = getenv("M0_TRACE_DIR");
	rc = set_trace_dir(var);
	if (rc != 0 && rc != -EINVAL)
		return rc;

	setlinebuf(stdout);
	return m0_trace_use_mmapped_buffer() ? logbuf_map() : 0;
}

M0_INTERNAL void m0_arch_trace_fini(void)
{
	void     *old_buffer      = m0_logbuf_header;
	uint32_t  old_buffer_size = M0_TRACE_BUF_HEADER_SIZE +
				    m0_trace_logbuf_size_get();

	m0_logbuf_header->tbh_buf_flags &= ~M0_TRACE_BUF_DIRTY;

	m0_trace_switch_to_static_logbuf();

	if (m0_trace_use_mmapped_buffer())
		munmap(old_buffer, old_buffer_size);
	close(logfd);
}

M0_INTERNAL void m0_arch_trace_buf_header_init(struct m0_trace_buf_header *tbh)
{
	int     i;
	int     rc;
	int     cmdline_fd;
	ssize_t read_bytes;

	tbh->tbh_buf_type = M0_TRACE_BUF_USER;

#define CMDLINE "/proc/self/cmdline"
	cmdline_fd = open(CMDLINE, O_RDONLY);
	if (cmdline_fd == -1)
		return;

	read_bytes = read(cmdline_fd, tbh->tbh_cli_args,
			  sizeof(tbh->tbh_cli_args));
	if (read_bytes == -1) {
		warn("mero: failed to read " CMDLINE);
	} else {
		if (read_bytes == sizeof(tbh->tbh_cli_args)) {
			warnx("mero: command line args don't fit into trace "
			      "buffer, truncating after %zu bytes", read_bytes);
			tbh->tbh_cli_args[read_bytes - 1] = '\0';
		} else {
			tbh->tbh_cli_args[read_bytes] = '\0';
		}
                /* cli args are separated with '\0', replace it with spaces */
		for (i = 0; i < read_bytes - 1; ++i)
                        if (tbh->tbh_cli_args[i] == '\0')
                                tbh->tbh_cli_args[i] = ' ';
	}

	rc = close(cmdline_fd);
	if (rc == -1)
		warn("mero: failed to close " CMDLINE ": errno=%d", errno);
#undef CMDLINE

	if (strstr(tbh->tbh_cli_args, "m0mkfs") != NULL)
		tbh->tbh_buf_flags |= M0_TRACE_BUF_MKFS;

	tbh->tbh_buf_flags |= M0_TRACE_BUF_DIRTY;
}

static unsigned align(FILE *file, uint64_t align, uint64_t pos)
{
	M0_ASSERT(m0_is_po2(align));
	while (!feof(file) && (pos & (align - 1))) {
		fgetc(file);
		pos++;
	}
	return pos;
}

static const struct m0_trace_buf_header *read_trace_buf_header(FILE *trace_file)
{
	const struct m0_trace_buf_header *tb_header;

	static char buf[M0_TRACE_BUF_HEADER_SIZE];
	size_t     n;

	n = fread(buf, 1, sizeof buf, trace_file);
	if (n != sizeof buf) {
		warnx("failed to read trace header (got %zu bytes instead of"
		      " %zu bytes)\n", n, sizeof buf);
		return NULL;
	}

	tb_header = (const struct m0_trace_buf_header *)buf;

	if (tb_header->tbh_magic != M0_TRACE_BUF_HEADER_MAGIC) {
		warnx("invalid trace header MAGIC value\n");
		return NULL;
	}

	if (tb_header->tbh_header_size != M0_TRACE_BUF_HEADER_SIZE)
		warnx("trace header has different size: expected=%u, actual=%u",
		      M0_TRACE_BUF_HEADER_SIZE, tb_header->tbh_header_size);

	return tb_header;
}

static void print_trace_buf_header(FILE *ofile,
				   const struct m0_trace_buf_header *tbh)
{
	int        rc;
	struct tm  tm;
	struct tm *ptm;
	char       time_str[512];
	size_t     time_str_len;
	time_t     time;
	bool       need_comma = false;

	fprintf(ofile, "header:\n");

	fprintf(ofile, "  mero_version:       %s\n", tbh->tbh_mero_version);
	fprintf(ofile, "  mero_git_describe:  %s\n", tbh->tbh_mero_git_describe);
	fprintf(ofile, "  mero_kernel:        %s\n", tbh->tbh_mero_kernel_ver);

	rc = putenv("TZ=UTC0");
	if (rc != 0)
		warn("failed to set timezone to UTC\n");

	time = m0_time_seconds(tbh->tbh_log_time);
	ptm = localtime_r(&time, &tm);
	if (ptm != NULL) {
		time_str_len = strftime(time_str, sizeof time_str,
					"%FT%T%z", &tm);
					/*"%b %e %H:%M:%S %Z %Y", &tm);*/
		if (time_str_len == 0)
			fprintf(stderr, "  failed to format trace file timestamp\n");
		fprintf(ofile, "  trace_time:         %s\n", time_str);
	} else {
		fprintf(stderr, "incorrect timestamp value in trace header\n");
		fprintf(ofile, "  trace_time:         ''\n");
	}

	fprintf(ofile, "  buffer_type:        %s\n",
		tbh->tbh_buf_type == M0_TRACE_BUF_KERNEL ? "KERNEL" :
		tbh->tbh_buf_type == M0_TRACE_BUF_USER   ? "USER"   :
							   "UNKNOWN"
	);
	fprintf(ofile, "  flags:              [ ");
	if (tbh->tbh_buf_flags & M0_TRACE_BUF_DIRTY) {
		need_comma = true;
		fprintf(ofile, "DIRTY");
	}
	if (tbh->tbh_buf_flags & M0_TRACE_BUF_MKFS) {
		if (need_comma)
			fprintf(ofile, ", ");
		fprintf(ofile, "MKFS");
	}
	fprintf(ofile, " ]\n");

	fprintf(ofile, "  header_addr:        %p\n", tbh->tbh_header_addr);
	fprintf(ofile, "  header_size:        %u\t\t# bytes\n", tbh->tbh_header_size);
	fprintf(ofile, "  buffer_addr:        %p\n", tbh->tbh_buf_addr);
	fprintf(ofile, "  buffer_size:        %lu\t\t# bytes\n", tbh->tbh_buf_size);

	if (tbh->tbh_buf_type == M0_TRACE_BUF_KERNEL) {
		fprintf(ofile, "  mod_struct_addr:    %p\n",
				tbh->tbh_module_struct);
		fprintf(ofile, "  mod_core_addr:      %p\n",
				tbh->tbh_module_core_addr);
		fprintf(ofile, "  mod_core_size:      %u\t\t# bytes\n",
				tbh->tbh_module_core_size);
	}

	fprintf(ofile, "  cli_args:           '%s'\n", tbh->tbh_cli_args);
}

static int mmap_m0mero_ko(const char *m0mero_ko_path, void **ko_addr)
{
	int         rc;
	int         kofd;
	struct stat ko_stat;

	kofd = open(m0mero_ko_path, O_RDONLY);
	if (kofd == -1) {
		warn("failed to open '%s' file", m0mero_ko_path);
		return EX_NOINPUT;
	}

	rc = fstat(kofd, &ko_stat);
	if (rc != 0) {
		warn("failed to get stat info for '%s' file",
		     m0mero_ko_path);
		return EX_OSERR;
	}

	*ko_addr = mmap(NULL, ko_stat.st_size, PROT_READ, MAP_PRIVATE,
		        kofd, 0);
	if (*ko_addr == MAP_FAILED) {
		warn("failed to mmap '%s' file", m0mero_ko_path);
		return EX_OSERR;
	}

	return 0;
}

static int calc_trace_descr_offset(const struct m0_trace_buf_header *tbh,
				   const char *m0mero_ko_path,
				   ptrdiff_t *td_offset)
{
	if (tbh->tbh_buf_type == M0_TRACE_BUF_USER) {
		*td_offset = (char*)m0_trace_magic_sym_addr_get() -
			     (char*)tbh->tbh_magic_sym_addr;
	} else if (tbh->tbh_buf_type == M0_TRACE_BUF_KERNEL) {
		int       rc;
		void     *ko_addr;
		off_t     msym_file_offset;
		uint64_t *msym;

		msym_file_offset = (char*)tbh->tbh_magic_sym_addr -
				   (char*)tbh->tbh_module_core_addr;

		rc = mmap_m0mero_ko(m0mero_ko_path, &ko_addr);
		if (rc != 0)
			return rc;

		msym = (uint64_t*)((char*)ko_addr + msym_file_offset);
		if (*msym != M0_TRACE_MAGIC) {
			warnx("invalid trace magic symbol value in '%s' file at"
			      " offset 0x%lx: 0x%lx (expected 0x%lx)",
			      m0mero_ko_path, msym_file_offset, *msym,
			      M0_TRACE_MAGIC);
			return EX_DATAERR;
		}

		*td_offset = (char*)msym - (char*)tbh->tbh_magic_sym_addr;
	} else {
		return EX_DATAERR;
	}

	return 0;
}

static int calc_extra_trace_descr_offsets(const struct m0_trace_buf_header *tbh,
					  const void *magic_symbols[],
					  size_t nr, ptrdiff_t td_offsets[])
{
	int i;

	if (tbh->tbh_buf_type == M0_TRACE_BUF_USER) {
		if (tbh->tbh_magic_sym_addresses_nr < nr) {
			warnx("There are only %u additional magic symbols"
			      " stored in trace log, but %zu were provided to"
			      " m0_trace_parse()",
			      tbh->tbh_magic_sym_addresses_nr, nr);
			nr = tbh->tbh_magic_sym_addresses_nr;
		}
		if (nr == 0)
			return -ENODATA;
		for (i = 0; i < nr; ++i)
			td_offsets[i] = (char*)magic_symbols[i] -
					(char*)tbh->tbh_magic_sym_addresses[i];
	} else {
		warnx("Cannot use additional magic symbols information with"
		      " kernel mode trace log");
		return -EOPNOTSUPP;
	}

	return 0;
}

static void patch_trace_descr(struct m0_trace_descr *td, ptrdiff_t offset)
{
	td->td_fmt    = (char*)td->td_fmt + offset;
	td->td_func   = (char*)td->td_func + offset;
	td->td_file   = (char*)td->td_file + offset;
	td->td_offset = (typeof (td->td_offset))((char*)td->td_offset + offset);
	td->td_sizeof = (typeof (td->td_sizeof))((char*)td->td_sizeof + offset);
	td->td_isstr  = (typeof (td->td_isstr))((char*)td->td_isstr + offset);
}

enum {
	MAGIC_SYM_OFFSETS_MAX = ARRAY_SIZE(
		((struct m0_trace_buf_header *)0)->tbh_magic_sym_addresses)
};

/**
 * Parse log buffer from a trace file.
 *
 * Normally a trace file would be called "m0trace.12345" or something like that,
 * where number represents a PID of the process which created that trace file.
 *
 * Returns sysexits.h error codes.
 */
M0_INTERNAL int m0_trace_parse(FILE *trace_file, FILE *output_file,
			       const char *m0mero_ko_path,
			       enum m0_trace_parse_flags flags,
			       const void *magic_symbols[],
			       unsigned int magic_symbols_nr)
{
	const struct m0_trace_buf_header *tbh;
	struct m0_trace_rec_header        trh;
	struct m0_trace_descr            *td;
	struct m0_trace_descr             patched_td;

	int        rc;
	int        i;
	size_t     pos = 0;
	size_t     nr;
	size_t     n2r;
	size_t     size;
	bool       td_is_sane;
	char      *buf;

	static char  yaml_buf[256 * 1024]; /* 256 KB */
	ptrdiff_t   *td_offset;
	ptrdiff_t    td_offsets[MAGIC_SYM_OFFSETS_MAX + 1] = { 0 };
	size_t       td_offsets_nr =
			(magic_symbols_nr < MAGIC_SYM_OFFSETS_MAX ?
			 magic_symbols_nr : MAGIC_SYM_OFFSETS_MAX) + 1;

	tbh = read_trace_buf_header(trace_file);
	if (tbh == NULL)
		return EX_DATAERR;

	print_trace_buf_header(output_file, tbh);
	if (flags & M0_TRACE_PARSE_HEADER_ONLY)
		return 0;

	rc = calc_trace_descr_offset(tbh, m0mero_ko_path, &td_offsets[0]);
	if (rc != 0)
		return rc;

	if (magic_symbols != NULL) {
		rc = calc_extra_trace_descr_offsets(tbh, magic_symbols,
				td_offsets_nr - 1, &td_offsets[1]);
		if (rc != 0)
			/* something's not good with the additional magic
			 * symbols information, using only internal (libmero)
			 * magic symbol */
			td_offsets_nr = 1;
		else if (magic_symbols_nr > MAGIC_SYM_OFFSETS_MAX)
			warnx("Using only first %u magic symbol offsets"
			      " out of %u provided to m0_trace_parse()",
			      MAGIC_SYM_OFFSETS_MAX, magic_symbols_nr);
	} else if (magic_symbols_nr > 0) {
		warnx("Not using additional magic symbol addresses"
		      " - inconsistent parameters");
	}

	if (flags & M0_TRACE_PARSE_YAML_SINGLE_DOC_OUTPUT)
		fprintf(output_file, "trace_records:\n");

	while (!feof(trace_file)) {

		/* At the beginning of a record */
		pos = align(trace_file, M0_TRACE_REC_ALIGN, pos);

		/* Find the complete record */
		do {
			nr = fread(&trh.trh_magic, 1,
				   sizeof trh.trh_magic, trace_file);

			if (nr != sizeof trh.trh_magic) {
				if (!feof(trace_file)) {
					warnx("Got %zu bytes of magic instead"
					      " of %zu", nr,
					      sizeof trh.trh_magic);
					return EX_DATAERR;
				}
				return EX_OK;
			}

			pos += nr;
		} while (trh.trh_magic != M0_TRACE_MAGIC);

		/* Now we might have complete record */
		n2r = sizeof trh - sizeof trh.trh_magic;
		nr  = fread(&trh.trh_sp, 1, n2r, trace_file);
		if (nr != n2r) {
			warnx("Got %zu bytes of record (need %zu)", nr, n2r);
			return EX_DATAERR;
		}
		pos += nr;

		for (i = 0; i < td_offsets_nr; ++i) {
			td_offset = &td_offsets[i];
			td = (struct m0_trace_descr*)((char*)trh.trh_descr +
						      *td_offset);
			td_is_sane =
			     m0_addr_is_sane_and_aligned((const uint64_t *)td);
			if (td_is_sane && td->td_magic == M0_TRACE_DESCR_MAGIC)
					break;

		}

		if (!td_is_sane) {
			warnx("Skipping non-existing trace descriptor %p",
			      trh.trh_descr);
			continue;
		}

		if (td->td_magic != M0_TRACE_DESCR_MAGIC) {
			warnx("Invalid trace descriptor - most probably the"
			      " trace file was produced by a different version"
			      " of Mero");
			return EX_TEMPFAIL;
		}

		patched_td = *td;
		if (tbh->tbh_buf_type == M0_TRACE_BUF_KERNEL)
			patch_trace_descr(&patched_td, *td_offset);
		trh.trh_descr = &patched_td;
		size = m0_align(td->td_size + trh.trh_string_data_size,
				M0_TRACE_REC_ALIGN);

		buf = m0_alloc(size);
		if (buf == NULL) {
			warn("Failed to allocate %zu bytes of memory, looks like"
			     " a corrupted trace descriptor, skipping...", size);
			continue;
		}

		nr = fread(buf, 1, size, trace_file);
		if (nr != size) {
			warnx("Got %zu bytes of data (need %zu)", nr, size);
			return EX_DATAERR;
		}
		pos += nr;

		rc = m0_trace_record_print_yaml(yaml_buf, sizeof yaml_buf, &trh,
			buf, !(flags & M0_TRACE_PARSE_YAML_SINGLE_DOC_OUTPUT));
		if (rc == 0)
			fprintf(output_file, "%s", yaml_buf);
		else if (rc == -ENOBUFS)
			warnx("Internal buffer is too small to hold trace record");
		else
			warnx("Failed to process trace record data for %p"
			      " descriptor", trh.trh_descr);
		m0_free(buf);
	}
	return EX_OK;
}

M0_INTERNAL void m0_console_vprintf(const char *fmt, va_list args)
{
	vprintf(fmt, args);
}

M0_INTERNAL void m0_error_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

/** @} end of trace group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
