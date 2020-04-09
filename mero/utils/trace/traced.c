/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dima Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 19-Jul-2013
 */


#include <stdio.h>      /* printf */
#include <err.h>        /* err */
#include <errno.h>      /* errno */
#include <string.h>     /* strcpy, basename */
#include <sysexits.h>   /* EX_* exit codes (EX_OSERR, EX_SOFTWARE) */
#include <sys/mman.h>   /* mmap */
#include <sys/stat.h>   /* open */
#include <signal.h>     /* sigaction */
#include <fcntl.h>      /* open */
#include <unistd.h>     /* close, daemon, STDOUT_FILENO */
#include <syslog.h>     /* openlog, vsyslog */
#include <stdarg.h>     /* va_arg */
#include <linux/limits.h> /* PATH_MAX */
#include <sys/sendfile.h> /* sendfile */
#include <sys/utsname.h>  /* uname */

#include "mero/init.h"             /* m0_init */
#include "lib/uuid.h"              /* m0_node_uuid_string_set */
#include "lib/misc.h"
#include "lib/string.h"            /* m0_strdup */
#include "lib/getopts.h"           /* M0_GETOPTS */
#include "lib/thread.h"            /* LAMBDA */
#include "lib/user_space/types.h"  /* bool */
#include "lib/trace.h"             /* M0_THREAD_INIT */
#include "lib/mutex.h"             /* m0_mutex_lock */
#include "lib/cond.h"              /* m0_cond_wait */
#include "lib/trace_internal.h"
#include "module/instance.h"


/**
 * @addtogroup trace
 *
 * @{
 */

#define DEFAULT_IN_FILE_NAME          "/sys/kernel/debug/mero/trace/buffer"
#define DEFAULT_OUT_FILE_NAME         "/var/log/mero/m0trace.bin"
#define M0MERO_KO_CORE_IN_FILE_NAME   "/sys/kernel/debug/mero/core"
#define M0MERO_KO_CORE_OUT_FILE_NAME  "/var/log/mero/m0mero_ko.img"

static const char *progname;
static const char *input_file_name  = DEFAULT_IN_FILE_NAME;
static const char *output_file_name = DEFAULT_OUT_FILE_NAME;
static const char *output_kore_file_name = M0MERO_KO_CORE_OUT_FILE_NAME;

static bool      daemon_mode        = false;
static bool      pipe_mode          = false;
static bool      fsync_enabled      = false;
static bool      use_syslog         = false;
static bool      save_kore          = true;
static int       log_level          = LOG_INFO;
static bool      rotation_enabled   = true;
static uint32_t  log_rotation_dealy = 5;        /* in seconds             */
static uint32_t  max_log_size       = 1024;     /* in MB                  */
static uint32_t  keep_logs          = 6;        /* number of logs to keep */

static struct m0_mutex  write_data_mutex;
static struct m0_mutex  rotator_mutex;
static struct m0_cond   rotator_cond;

struct rotator_ctx {
	int                               log_fd;
	const struct m0_trace_buf_header *log_header;
};

enum lr_action {
	LR_ROTATE,
	LR_STOP,
};

static enum lr_action rotator_action = LR_ROTATE;
static volatile bool  stop_processing = false;

enum {
	MAX_LOG_NAMES = 1024,
};
/* array with the names of log files being rotated */
static char log_names[MAX_LOG_NAMES][PATH_MAX];


static void plog(int level, const char *format, ...)
		__attribute__(( format(printf, 2, 3) ));

#define log_err(fmt, ...)    plog(LOG_ERR, fmt, ## __VA_ARGS__)
#define log_warn(fmt, ...)   plog(LOG_WARNING, fmt, ## __VA_ARGS__)
#define log_info(fmt, ...)   plog(LOG_INFO, fmt, ## __VA_ARGS__)
#define log_debug(fmt, ...)  plog(LOG_DEBUG, fmt, ## __VA_ARGS__)


static void plog(int level, const char *format, ...)
{
	static char  buf[8 * 1024];  /* 8 KB */
	va_list      args;

	if (level > log_level)
		return;

	va_start(args, format);

	if (use_syslog) {
		vsyslog(level, format, args);
	} else {
		snprintf(buf, sizeof buf, "%s: %s", progname, format);
		buf[sizeof buf - 1] = '\0';
		vfprintf(stderr, buf, args);
	}

	va_end(args);
}

void sig_term_quit_int_handler(int signum, siginfo_t *siginfo, void *uctx)
{
	log_debug("%s: signum %d\n", __func__, signum);
	stop_processing = true;
}

static const struct m0_trace_buf_header *read_trace_buf_header(int ifd)
{
	const struct m0_trace_buf_header *tb_header;

	static char buf[M0_TRACE_BUF_HEADER_SIZE];
	ssize_t     n;

	n = read(ifd, buf, sizeof buf);
	if (n != sizeof buf) {
		log_err("failed to read trace header from '%s' file (got %zd"
			" bytes instead of %zu bytes)\n",
			input_file_name, n, sizeof buf);
		return NULL;
	}

	tb_header = (const struct m0_trace_buf_header *)buf;

	if (tb_header->tbh_magic != M0_TRACE_BUF_HEADER_MAGIC) {
		log_err("invalid trace header MAGIC value\n");
		return NULL;
	}

	log_info("Trace buffer header:  [%s]\n", input_file_name);
	log_info("  header size: %u\n", tb_header->tbh_header_size);
	log_info("  buffer size: %lu\n", tb_header->tbh_buf_size);
	log_info("  buffer type: %s\n",
		tb_header->tbh_buf_type == M0_TRACE_BUF_KERNEL ? "kernel" :
		tb_header->tbh_buf_type == M0_TRACE_BUF_USER   ? "user"   :
								 "unknown"
	);
	log_info("  mero_version: %s\n", tb_header->tbh_mero_version);
	log_info("  mero_git_describe: %s\n", tb_header->tbh_mero_git_describe);

	return tb_header;
}

static int write_trace_header(const struct m0_trace_buf_header *header,
			      int *ofd, const char *ofname)
{
	int     fd;
	ssize_t n;
	int     rc = 0;

	m0_mutex_lock(&write_data_mutex);
	fd = *ofd;
	n = write(fd, header, header->tbh_header_size);
	if (n != header->tbh_header_size) {
		log_err("failed to write trace header to output file '%s': %s\n",
			ofname, strerror(errno));
		rc = EX_IOERR;
		goto out;
	}

	if (fsync_enabled) {
		rc = fsync(fd);
		if (rc != 0)
			log_err("fsync(2) failed on output file '%s': %s",
				ofname, strerror(errno));
	}
out:
	m0_mutex_unlock(&write_data_mutex);
	return rc;
}


static int write_trace_data(int *fd_ptr, const void *buf, size_t size)
{
	ssize_t n;
	int     fd;
	int     rc = 0;

	m0_mutex_lock(&write_data_mutex);
	fd = *fd_ptr;
	n = write(fd, buf, size);
	if (n != size) {
		log_err("failed to write trace data to output file"
			" '%s': %s\n", output_file_name,
			strerror(errno));
		rc = EX_IOERR;
		goto out;
	}

	if (fsync_enabled) {
		rc = fsync(fd);
		if (rc != 0)
			log_err("fsync(2) failed on output file '%s': %s",
				output_file_name, strerror(errno));
	}
out:
	m0_mutex_unlock(&write_data_mutex);
	return rc;
}

static const char *get_cur_pos(const struct m0_trace_buf_header *logheader,
			       const char *logbuf)
{
	const char *curpos = logbuf + m0_atomic64_get(&logheader->tbh_cur_pos) %
				      logheader->tbh_buf_size;

	if (curpos >= logbuf + logheader->tbh_buf_size) {
		log_err("invalid trace buffer pointer (out of range)\n");
		return NULL;
	}

	return curpos;
}

static bool is_log_rotation_needed(int *fd_ptr)
{
	static m0_time_t  time_of_last_check;
	m0_time_t         time_diff;
	struct stat       log_stat;
	int               rc;

	if (!rotation_enabled || max_log_size == 0)
		return false;

	if (time_of_last_check == 0)
		time_of_last_check = m0_time_now();

	time_diff = m0_time_sub(m0_time_now(), time_of_last_check);

	if (m0_time_seconds(time_diff) > log_rotation_dealy) {
		time_of_last_check = m0_time_now();

		m0_mutex_lock(&write_data_mutex);
		rc = fstat(*fd_ptr, &log_stat);
		m0_mutex_unlock(&write_data_mutex);
		if (rc != 0) {
			log_err("failed to get stat info for '%s' file: %s\n",
					output_file_name, strerror(errno));
			return false;
		}

		if (log_stat.st_size / 1024 / 1024 >= max_log_size) {
			log_debug("log size is %.2fMB, log rotation is needed\n",
				  (float)log_stat.st_size / 1024 / 1024);
			return true;
		}
	}

	return false;
}

static void wake_up_rotator_thread(enum lr_action a)
{
	m0_mutex_lock(&rotator_mutex);
	rotator_action = a;
	m0_cond_signal(&rotator_cond);
	m0_mutex_unlock(&rotator_mutex);
}

static int rotate_original_log(struct rotator_ctx *rctx)
{
	int ofd;
	int rc;

	log_debug("rotating original log..\n");

	rc = rename(output_file_name, log_names[0]);
	if (rc != 0) {
		log_err("failed to rename log file '%s' => '%s': %s\n",
			output_file_name, log_names[0], strerror(errno));
		return rc;
	}

	/* open new output file */
	ofd = open(output_file_name, O_RDWR|O_CREAT|O_TRUNC,
		   S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (ofd == -1) {
		log_err("failed to open output file '%s': %s\n",
			output_file_name, strerror(errno));
		return EX_CANTCREAT;
	}

	log_debug("new output log fd %d\n", ofd);

	rc = write_trace_header(rctx->log_header, &ofd, output_file_name);
	if (rc != 0)
		return rc;

	m0_mutex_lock(&write_data_mutex);
	if (rctx->log_fd > 0)
		close(rctx->log_fd);
	rctx->log_fd = ofd;
	m0_mutex_unlock(&write_data_mutex);

	log_debug("original log has been rotated\n");
	return rc;
}

static void log_rotator_thread(struct rotator_ctx *rctx)
{
	const char  *last_log;
	int          i;
	int          rc;

	log_debug("log rotation thread started\n");

	while (true) {
		/* waiting for some work */
		m0_mutex_lock(&rotator_mutex);

		if (rotator_action == LR_STOP) {
			m0_mutex_unlock(&rotator_mutex);
			break;
		}

		m0_cond_wait(&rotator_cond);

		if (rotator_action == LR_STOP) {
			m0_mutex_unlock(&rotator_mutex);
			break;
		}

		m0_mutex_unlock(&rotator_mutex);

		log_debug("perform logs rotation\n");

		/* drop oldest log file if it exists */
		last_log = log_names[keep_logs - 1];
		if (access(last_log, F_OK) == 0) {
			rc = unlink(last_log);
			if (rc != 0) {
				log_err("failed to remove oldest log file '%s':"
					" %s\n", last_log, strerror(errno));
				continue;
			}
		}

		/* rotate log files with digit suffix */
		for (i = keep_logs - 2; i >= 0; --i)
			if (access(log_names[i], F_OK) == 0) {
				rc = rename(log_names[i], log_names[i + 1]);
				if (rc != 0) {
					log_err("failed to rename log file"
						" '%s' => '%s': %s\n",
						log_names[i], log_names[i + 1],
						strerror(errno));
					break;
				}
			}

		rotate_original_log(rctx);
	}

	log_debug("log rotation thread stopped\n");
}

static int process_trace_buffer(int *ofd_ptr,
				const struct m0_trace_buf_header *logheader,
				const char *logbuf)
{
	int              rc;
	const char      *curpos;
	const char      *oldpos;
	const char      *logbuf_end = logbuf + logheader->tbh_buf_size;
	const m0_time_t  idle_timeo = 100 * M0_TIME_ONE_MSEC;
	m0_time_t        timeo = idle_timeo;

	log_info("Fsync enabled: %s\n", fsync_enabled ? "yes" : "no");

	rc = write_trace_header(logheader, ofd_ptr, output_file_name);
	if (rc != 0)
		return rc;

	curpos = get_cur_pos(logheader, logbuf);
	if (curpos == NULL)
		return -EINVAL;

        if (!pipe_mode) {
                /*
                 * write first chunk of trace data, we always start from the
                 * beginning of buffer till current position
                 */
                rc = write_trace_data(ofd_ptr, logbuf, curpos - logbuf);
                if (rc != 0)
                        return rc;
        }

	oldpos = curpos;

	/* write rest of trace data as it appears in the buffer */
	while (!stop_processing) {
		curpos = get_cur_pos(logheader, logbuf);
		if (curpos == NULL)
			return -EINVAL;

		if (curpos == oldpos) {
			timeo += M0_TIME_ONE_MSEC;
			if (timeo > idle_timeo)
				timeo = idle_timeo;

			rc = m0_nanosleep(timeo, NULL);
			if (rc != 0) {
				log_warn("trace data processing interrupted by"
					 " a signal, exiting\n");
				return 0;
			}

			continue;
		}

		if (curpos > oldpos) {
			rc = write_trace_data(ofd_ptr, oldpos, curpos - oldpos);
			if (rc != 0)
				return rc;
		} else {
			rc = write_trace_data(ofd_ptr, curpos, logbuf_end - curpos);
			if (rc != 0)
				return rc;
			rc = write_trace_data(ofd_ptr, logbuf, curpos - logbuf);
			if (rc != 0)
				return rc;
		}

		oldpos = curpos;
		timeo /= 2;

		if (is_log_rotation_needed(ofd_ptr))
			wake_up_rotator_thread(LR_ROTATE);
	}

	return 0;
}

static int init_log_names(void)
{
	int i;

	if (keep_logs > MAX_LOG_NAMES) {
		log_err("invalid valude for -k option, it should be <= %u\n",
			MAX_LOG_NAMES);
		return EX_USAGE;
	}

	for (i = 0; i < keep_logs; ++i)
		snprintf(log_names[i], sizeof log_names[i], "%s.%u",
			 output_file_name, i + 1);

	return 0;
}

int save_kore_file(void)
{
	int  ifd;
	int  ofd;
	int  nr = 0;
	int  nw;
	int  rc = 0;

	static char buf[4 * 1024 * 1024]; /* 4MB */

	/* open input kore file */
	ifd = open(M0MERO_KO_CORE_IN_FILE_NAME, O_RDONLY);
	if (ifd == -1) {
		log_err("failed to open input file '%s': %s\n",
			M0MERO_KO_CORE_IN_FILE_NAME, strerror(errno));
		return EX_NOINPUT;
	}

	/* open output kore file */
	ofd = open(output_kore_file_name, O_RDWR|O_CREAT|O_TRUNC,
		   S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (ofd == -1) {
		log_err("failed to open output file '%s': %s\n",
			output_kore_file_name, strerror(errno));
		return EX_CANTCREAT;
	}

	while ((nr = read(ifd, buf, sizeof buf)) > 0) {
		nw = write(ofd, buf, nr);
		if (nw == -1) {
			log_err("failed to write data to '%s' file: %s\n",
				output_kore_file_name, strerror(errno));
			rc = -errno;
			goto close;
		}
	}

	if (nr == -1) {
		log_err("failed to read data from '%s' file: %s\n",
			M0MERO_KO_CORE_IN_FILE_NAME, strerror(errno));
		rc = -errno;
		goto close;
	}
close:
	close(ifd);
	close(ofd);

	if (rc == 0)
		log_debug("m0mero.ko core image saved into '%s'\n",
			  output_kore_file_name);
	return rc;
}

int main(int argc, char *argv[])
{
	int  rc;
	int  ifd;
	int  ofd;

	static struct m0                  instance = { 0 };
	const struct m0_trace_buf_header *logheader;
	void                             *logbuf;
	struct m0_thread                  rotator_tid = { 0 };
	struct rotator_ctx                rotator_data = { 0 };
	int32_t                           monitor_cycles = 0;

	struct sigaction old_sa;
	struct sigaction sa = {
		.sa_sigaction = sig_term_quit_int_handler,
		.sa_flags     = SA_SIGINFO,
	};

	progname = basename(argv[0]);

	/* prevent creation of trace file for ourselves */
	m0_trace_set_mmapped_buffer(false);

	/* we don't need a real node uuid, so we force a default one to be useed
	 * instead */
	m0_node_uuid_string_set(NULL);

	rc = m0_init(&instance);
	if (rc != 0) {
		log_err("failed to initialize libmero\n");
		return EX_SOFTWARE;
	}

	/* process CLI options */
	rc = M0_GETOPTS(progname, argc, argv,
	  M0_HELPARG('h'),
	  M0_STRINGARG('i',
		"input file name, if none is provided, then "
		DEFAULT_IN_FILE_NAME " is used by default",
		LAMBDA(void, (const char *str) {
			input_file_name = m0_strdup(str);
		})
	  ),
	  M0_STRINGARG('o',
		"output file name, if none is provided, then "
		DEFAULT_OUT_FILE_NAME " is used by default",
		LAMBDA(void, (const char *str) {
			output_file_name = m0_strdup(str);
		})
	  ),
	  M0_STRINGARG('O',
		"output file name for m0mero.ko core image, if none is provided"
		", then " M0MERO_KO_CORE_OUT_FILE_NAME " is used by default;"
		" it has effect only if -K option is also specified",
		LAMBDA(void, (const char *str) {
			output_kore_file_name = m0_strdup(str);
		})
	  ),
	  M0_FLAGARG('d',
		  "daemon mode - run in the background, implies -L (log errors"
		  " into syslog)",
		  &daemon_mode
	  ),
	  M0_VOIDARG('K',
		  "save m0mero.ko core image into a file, specified by -O option"
		  " (this option has no effect as it's 'on' by default, it has"
		  " been kept for backward compatibility, it's superseded by"
		  " '-S' option)",
		  LAMBDA(void, (void) { })
	  ),
	  M0_VOIDARG('S',
		  "discards action of '-K' option, i.e. don't save m0mero.ko"
		  " core image into a file",
		  LAMBDA(void, (void) {
			  save_kore = false;
		  })
	  ),
	  M0_NUMBERARG('l', "log level number (from syslog.h)",
		LAMBDA(void, (int64_t lvl) {
			log_level = lvl;
		})
	  ),
	  M0_FLAGARG('L',
		  "log information into syslog instead of STDERR (by default"
		  " is off)",
		  &use_syslog
	  ),
	  M0_NUMBERARG('s',
		"output trace file size in MB, when it's reached, log rotation"
		" is performed; if set to 0 then rotation is disabled, default"
		" is 1024MB",
		LAMBDA(void, (int64_t size) {
			max_log_size = size;
		})
	  ),
	  M0_NUMBERARG('k',
		"number of logs to keep in log rotation, default is 6",
		LAMBDA(void, (int64_t k) {
			keep_logs = k;
		})
	  ),
	  M0_NUMBERARG('m',
		"monitor trace buffer file creation each second during"
		" specified period of time (in seconds)",
		LAMBDA(void, (int64_t k) {
			monitor_cycles = k;
		})
	  ),
	  M0_VOIDARG('p',
		"pipe mode - run in foreground, output to STDOUT and disable"
		" log rotation",
		LAMBDA(void, (void) {
			pipe_mode        = true;
			fsync_enabled    = false;
			rotation_enabled = false;
			daemon_mode      = false;
			save_kore        = false;
			output_file_name = "-";
		})
	  ),
	  M0_FLAGARG('f', "do fsync after writing each chunk of trace data",
		&fsync_enabled
	  ),
	);

	if (rc != 0)
		return EX_USAGE;

	if (daemon_mode) {
		/* configure syslog(3) */
		openlog(progname, LOG_NOWAIT | LOG_CONS, LOG_DAEMON);
		use_syslog = true;
		/*
		 * become a daemon (fork background process,
		 * close STD{IN,OUT}, etc.)
		 */
		rc = daemon(0, 0);
		if (rc != 0)
			err(EX_OSERR, "failed to switch to daemon mode");
	}

	rc  = sigaction(SIGTERM, &sa, &old_sa);
	rc |= sigaction(SIGQUIT, &sa, NULL);
	rc |= sigaction(SIGINT,  &sa, NULL);
	if (rc != 0) {
		log_err("failed to set SIGTERM/SIGQUIT/SIGINT handler\n");
		return EX_OSERR;
	}

	if (rotation_enabled) {
		rc = init_log_names();
		if (rc != 0)
			return rc;
	}

	if (save_kore) {
		rc = save_kore_file();
		if (rc != 0)
			return rc;
	}

	while (monitor_cycles-- > 0) {
		rc = access(input_file_name, F_OK);
		if (rc == 0)
			break;
		sleep(1);
	}

	/* open input file */
	ifd = open(input_file_name, O_RDONLY);
	if (ifd == -1) {
		log_err("failed to open input file '%s': %s\n",
			input_file_name, strerror(errno));
		return EX_NOINPUT;
	}

	/* open output file */
	if (strcmp(output_file_name, "-") == 0) {
		ofd = STDOUT_FILENO;
		output_file_name = "*STDOUT*";
	} else {
		ofd = open(output_file_name, O_RDWR|O_CREAT|O_TRUNC,
			   S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		if (ofd == -1) {
			log_err("failed to open output file '%s': %s\n",
				output_file_name, strerror(errno));
			return EX_CANTCREAT;
		}
	}
	log_debug("output log fd %d\n", ofd);

	logheader = read_trace_buf_header(ifd);
	if (logheader == NULL)
		return EX_DATAERR;

	logheader = mmap(NULL, logheader->tbh_header_size, PROT_READ,
			 MAP_SHARED, ifd, 0);
	if (logheader == MAP_FAILED) {
		log_err("failed to mmap trace buffer header from '%s': %s\n",
			input_file_name, strerror(errno));
		return EX_OSERR;
	}

	logbuf = mmap(NULL, logheader->tbh_buf_size, PROT_READ, MAP_SHARED,
		      ifd, logheader->tbh_header_size);
	if (logbuf == MAP_FAILED) {
		log_err("failed to mmap trace buffer from '%s': %s\n",
			input_file_name, strerror(errno));
		return EX_OSERR;
	}

	m0_mutex_init(&write_data_mutex);
	m0_mutex_init(&rotator_mutex);
	m0_cond_init(&rotator_cond, &rotator_mutex);

	rotator_data.log_fd = ofd;
	rotator_data.log_header = logheader;

	if (rotation_enabled) {
		/* start log rotation thread */
		rc = M0_THREAD_INIT(&rotator_tid, struct rotator_ctx *, NULL,
				    &log_rotator_thread, &rotator_data,
				    "m0traced_logrt");
		if (rc != 0) {
			log_err("failed to start log rotation thread\n");
			return EX_SOFTWARE;
		}
	}

	/* do main work */
	rc = process_trace_buffer(&rotator_data.log_fd, logheader, logbuf);

	/* stop log rotation thread */
	if (rotation_enabled) {
		wake_up_rotator_thread(LR_STOP);
		m0_thread_join(&rotator_tid);
		m0_thread_fini(&rotator_tid);
	}

	m0_cond_fini(&rotator_cond);
	m0_mutex_fini(&rotator_mutex);
	m0_mutex_fini(&write_data_mutex);

	m0_fini();
	munmap(logbuf, logheader->tbh_buf_size);
	munmap((void*)logheader, logheader->tbh_header_size);
	close(ofd);
	close(ifd);

	return rc;
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
