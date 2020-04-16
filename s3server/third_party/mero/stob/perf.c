/* -*- C -*- */
/*
 * COPYRIGHT 2019 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 14-Mar-2019
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_STOB
#include "lib/trace.h"

#include "stob/perf.h"

#include <sys/mount.h>    /* mount */
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>        /* FILE, fopen */
#include <unistd.h>       /* rmdir */

#include "lib/errno.h"
#include "lib/locality.h" /* m0_locality_get */
#include "lib/memory.h"
#include "lib/string.h"   /* m0_strdup */
#include "lib/timer.h"    /* m0_timer */
#include "stob/type.h"
#include "stob/domain.h"
#include "stob/io.h"
#include "stob/stob.h"

/**
 * @addtogroup stobperf
 *
 * <b> Performance STOB. </b>
 *
 * Performance STOB is a special implementation of m0_stob which is created for
 * performance tests. Further, we will call it "perfstob".
 *
 * Perfstob provides predefined I/O latency and bandwidth. It can be used to
 * emulate behaviour of a particular disk roughly or for repeatable tests.
 * Therefore, main purposes are:
 *
 *     - Test code in the absence of hardware;
 *
 *     - Provide environment for baseline tests;
 *
 * Perfstob works in either of the two modes:
 *
 *     1. With storing data. In this mode, stored data can be read back. Maximum
 *        size of such a stob depends on amount of spare RAM. This mode is
 *        intended to be used for metadata;
 *
 *     2. Without storing data. In this mode, write operation goes through the
 *        stob I/O stack, but is not stored anywhere and thus can't be read
 *        back. User can write unlimited amount of data to such a stob.
 *
 * In the both modes, perfstob domains and perfstobs survive process restart,
 * but don't survive node restart.
 *
 * <b> Implementation of the perfstob. </b>
 *
 * Perfstob is implemented on top of linuxstob. Every perfstob domain mounts
 * a tmpfs instance to the location and the linuxstob domain resides there.
 *
 * A linuxstob is created for every perfstob. The linuxstob is represented by
 * either a regular file or symlink to /dev/null depending on the perfstob mode.
 *
 * Perfstob translates all I/O operations to linuxstob.
 *
 * <b> Configuration of the perfstob. </b>
 *
 * All perfstob features must be configured only with the perfstob domain
 * configuration. This is done intentionally. Perfstob replaces other stobs
 * and we want this to be done with minimum changes.
 *
 * All configuration passed to m0_stob interface is ignored here. All necessary
 * configuration is accessible via m0_stob_perf_domain::spd_cfg.
 *
 * <b> Latency emulation. </b>
 *
 * Perfstob emulates latency using a soft timer. Latency pattern is defined by
 * a statefull callback function. Currently, there is only constant latency
 * pattern.
 *
 * <b> Statistics. </b>
 *
 * Statistics is not implemented.
 *
 * @{
 */

struct stob_perf_io;

typedef m0_time_t (*stob_perf_latency_cb_t)(struct stob_perf_io *);

struct stob_perf_domain_cfg {
	size_t                 spc_tmpfs_size;
	bool                   spc_is_null;
	m0_time_t              spc_latency;
	stob_perf_latency_cb_t spc_latency_cb;
};

struct stob_perf_domain {
	struct m0_stob_domain        spd_dom;
	/** Perf stob domain is implemented on top of this linux stob domain. */
	struct m0_stob_domain       *spd_ldom;
	struct stob_perf_domain_cfg  spd_cfg;
	uint64_t                     spd_magic;
};

/* XXX TODO populate with stats and store it to tmpfs periodically or on stob
 * finalisation */
struct stob_perf_stats {
	int unused;
};

struct stob_perf {
	struct m0_stob           sp_stob;
	struct m0_stob          *sp_backstore;
	struct stob_perf_stats   sp_stats;
	struct stob_perf_domain *sp_pdom;
	uint64_t                 sp_magic;

	/* List of I/Os and latency logics */
	struct m0_tl             sp_ios;
	struct m0_mutex          sp_lock;
	struct m0_timer          sp_timer;
	struct m0_sm_ast         sp_ast;
};

struct stob_perf_io {
	struct m0_stob_io *spi_io;
	struct m0_stob_io  spi_lio;
	struct stob_perf  *spi_pstob;
	struct m0_clink    spi_clink;
	struct m0_tlink    spi_link;
	uint64_t           spi_magic;
};

static struct m0_stob_domain_ops stob_perf_domain_ops;
static struct m0_stob_ops stob_perf_ops;
static const struct m0_stob_io_op stob_perf_io_ops;

static void stob_perf_io_completed(struct stob_perf_io *pio);

enum {
	STOB_TYPE_PERF = 0xFE,
	/* Default size of the tmpfs in MB. */
	STOB_PERF_TMPFS_SIZE_DEFAULT = 128,
};

#define STOB_PERF_DOM_CONFIG_PATH "/config"
#define STOB_PERF_DOM_LDOM_PATH "/backstore"

M0_TL_DESCR_DEFINE(stob_perf_ios, "List of perfstob I/O",
		   static, struct stob_perf_io, spi_link, spi_magic,
		   M0_STOB_PERF_IO_MAGIC, M0_STOB_PERF_IO_HEAD_MAGIC);
M0_TL_DEFINE(stob_perf_ios, static, struct stob_perf_io);

static void stob_perf_type_register(struct m0_stob_type *type)
{
}

static void stob_perf_type_deregister(struct m0_stob_type *type)
{
}

static struct stob_perf_domain *
stob_perf_domain_container(struct m0_stob_domain *dom)
{
	struct stob_perf_domain *pdom =
			container_of(dom, struct stob_perf_domain, spd_dom);
	M0_POST(pdom->spd_magic == M0_STOB_DOM_PERF_MAGIC);
	return pdom;
}

static struct stob_perf *stob_perf_container(struct m0_stob *stob)
{
	struct stob_perf *pstob = container_of(stob, struct stob_perf, sp_stob);
	M0_POST(pstob->sp_magic == M0_STOB_PERF_MAGIC);
	return pstob;
}

static m0_time_t stob_perf_latency_const(struct stob_perf_io *pio)
{
	return pio->spi_pstob->sp_pdom->spd_cfg.spc_latency;
}

static int stob_perf_domain_cfg_init_parse(const char  *str_cfg_init,
					   void       **cfg_init)
{
	return 0;
}

static void stob_perf_domain_cfg_init_free(void *cfg_init)
{
}

static int stob_perf_domain_cfg_create_parse(const char  *str_cfg_create,
					     void       **cfg_create)
{
	int rc = 0;

	M0_PRE(cfg_create != NULL);

	*cfg_create = NULL;
	if (str_cfg_create != NULL) {
		*cfg_create = m0_strdup(str_cfg_create);
		rc = *cfg_create == NULL ? M0_ERR(-ENOMEM) : 0;
	}
	return M0_RC(rc);
}

static void stob_perf_domain_cfg_create_free(void *cfg_create)
{
	if (cfg_create != NULL)
		m0_free(cfg_create);
}

static char *stob_perf_domain_config(const char *location_data)
{
	char *config_path;

	config_path = m0_alloc(strlen(location_data) +
			       strlen(STOB_PERF_DOM_CONFIG_PATH) + 1);
	if (config_path != NULL) {
		strcpy(config_path, location_data);
		strcat(config_path, STOB_PERF_DOM_CONFIG_PATH);
	}
	return config_path;
}

static char *stob_perf_domain_ldom_location(const char *location_data)
{
	char *ldom_location;

#define __LINUXSTOB "linuxstob:"
	ldom_location = m0_alloc(strlen(__LINUXSTOB) + strlen(location_data) +
				 strlen(STOB_PERF_DOM_LDOM_PATH) + 1);
	if (ldom_location != NULL) {
		strcpy(ldom_location, __LINUXSTOB);
		strcat(ldom_location, location_data);
		strcat(ldom_location, STOB_PERF_DOM_LDOM_PATH);
	}
#undef __LINUXSTOB
	return ldom_location;
}

static void stob_perf_domain_cfg_parse(struct stob_perf_domain_cfg *cfg,
				       const char                  *cfg_str)
{
	/* XXX TODO Define config format and parse it properly. */

	cfg->spc_is_null = cfg_str != NULL &&
			   (strstr(cfg_str, "null=true") != NULL ||
			    strstr(cfg_str, "null=1") != NULL);
	M0_LOG(M0_DEBUG, "spc_is_null=%d", !!cfg->spc_is_null);

	cfg->spc_tmpfs_size = cfg->spc_is_null ? 64 : 256; /* XXX */
	cfg->spc_latency = M0_TIME_ONE_MSEC;
	cfg->spc_latency_cb = &stob_perf_latency_const;

	/* Don't allow latency longer than 2 seconds, it's converted to long. */
	M0_POST(cfg->spc_latency < 2 * M0_TIME_ONE_SECOND);
}

static int stob_perf_domain_read_config(struct stob_perf_domain *pdom,
					const char              *location_data)
{
	struct stat  st;
	char        *config_path;
	char        *cfg_str;
	bool         cfg_allocated = false;
	FILE        *f;
	size_t       len;
	size_t       read;
	int          rc;

	config_path = stob_perf_domain_config(location_data);
	if (config_path == NULL)
		return M0_ERR(-ENOMEM);

	rc = stat(config_path, &st);
	rc = rc != 0 ? M0_ERR(-errno) : 0;
	if (rc == 0) {
		len = st.st_size + 1;
		cfg_str = m0_alloc(len);
		rc = cfg_str == NULL ? M0_ERR(-ENOMEM) : 0;
	}
	if (rc == 0) {
		cfg_allocated = true;
		f = fopen(config_path, "r");
		rc = f == NULL ? M0_ERR(-errno) : 0;
	}
	if (rc == 0) {
		read = fread(cfg_str, 1, len - 1, f);
		rc = ferror(f) != 0 ? M0_ERR(-errno) : 0;
		if (rc == 0) {
			M0_ASSERT(read < len);
			cfg_str[read] = '\0';
		}
		(void)fclose(f);
	}
	if (rc == 0)
		stob_perf_domain_cfg_parse(&pdom->spd_cfg, cfg_str);

	if (cfg_allocated)
		m0_free(cfg_str);
	m0_free(config_path);

	return M0_RC(rc);
}

static int stob_perf_domain_write_config(const char *cfg_str,
					 uint64_t    dom_key,
					 const char *location_data)
{
	char   *config_path;
	size_t  written;
	FILE   *f;
	int     rc;
	int     rc2;

	config_path = stob_perf_domain_config(location_data);
	if (config_path == NULL)
		return M0_ERR(-ENOMEM);

	f = fopen(config_path, "w");
	rc = f == NULL ? M0_ERR(-errno) : 0;
	if (rc == 0 && cfg_str != NULL) {
		written = fwrite(cfg_str, 1, strlen(cfg_str), f);
		if (written != strlen(cfg_str))
			rc = ferror(f) != 0 ? M0_ERR(-errno) : M0_ERR(-EINVAL);
	}
	if (f != NULL) {
		rc2 = fclose(f);
		rc2 = rc2 == EOF ? M0_ERR(-errno) : 0;
		rc = rc ?: rc2;
	}
	m0_free(config_path);

	return M0_RC(rc);
}

static int stob_perf_domain_init(struct m0_stob_type    *type,
				 const char             *location_data,
				 void                   *cfg_init,
				 struct m0_stob_domain **out)
{
	struct stob_perf_domain *pdom;
	struct m0_fid            dom_id;
	uint64_t                 dom_key;
	uint8_t                  type_id;
	char                    *ldom_location;
	int                      rc;

	M0_PRE(out != NULL);

	M0_ALLOC_PTR(pdom);
	ldom_location = stob_perf_domain_ldom_location(location_data);

	if (pdom == NULL || ldom_location == NULL) {
		m0_free(pdom);
		m0_free(ldom_location);
		return M0_ERR(-ENOMEM);
	}

	rc = stob_perf_domain_read_config(pdom, location_data);
	if (rc == 0)
		rc = m0_stob_domain_init(ldom_location, NULL, &pdom->spd_ldom);
	M0_ASSERT(ergo(rc == 0, pdom->spd_ldom != NULL));

	if (rc == 0) {
		dom_key = m0_stob_domain__dom_key(
					m0_stob_domain_id_get(pdom->spd_ldom));
		M0_ASSERT(m0_stob_domain__dom_key_is_valid(dom_key));
		type_id = m0_stob_type_id_get(type);
		m0_stob_domain__dom_id_make(&dom_id, type_id, 0, dom_key);
		m0_stob_domain__id_set(&pdom->spd_dom, &dom_id);

		pdom->spd_magic = M0_STOB_DOM_PERF_MAGIC;
		pdom->spd_dom.sd_ops = &stob_perf_domain_ops;
		*out = &pdom->spd_dom;
	}
	m0_free(ldom_location);

	return M0_RC(rc);
}

static void stob_perf_domain_fini(struct m0_stob_domain *dom)
{
	struct stob_perf_domain *pdom = stob_perf_domain_container(dom);

	m0_stob_domain_fini(pdom->spd_ldom);
	m0_free(pdom);
}

static int stob_perf_domain_dir_clean(const char *location_data)
{
	int rc;

	(void)umount(location_data);
	rc = rmdir(location_data);
	rc = rc != 0 ? M0_ERR(-errno) : 0;

	return M0_RC(rc);
}

static int stob_perf_domain_tmpfs_opts(char                        *opts,
				       size_t                       opts_len,
				       struct stob_perf_domain_cfg *cfg)
{
	size_t size;
	int    i = 0;
	int    rc;

	static char sfx[] = { 'M', 'G', 'T' };

	size = cfg->spc_tmpfs_size == 0 ? STOB_PERF_TMPFS_SIZE_DEFAULT :
					  cfg->spc_tmpfs_size;

	if (size >= 1024 * 1024 * 1024)
		return M0_ERR(-EOVERFLOW);

	for (i = 0; size >= 1024; ++i)
		size /= 1024;
	rc = snprintf(opts, opts_len, "size=%zu%c", size, sfx[i]);
	if (rc >= opts_len)
		return M0_ERR(-ERANGE);

	return 0;
}

static int stob_perf_domain_create(struct m0_stob_type *type,
				   const char          *location_data,
				   uint64_t             dom_key,
				   void                *cfg_create)
{
	struct stob_perf_domain_cfg  cfg = {};
	struct m0_stob_domain       *ldom = NULL;
	const char                  *cfg_str = cfg_create;
	char                         tmpfs_opts[16];
	char                        *ldom_location;
	bool                         dir_created = false;
	int                          rc;

	stob_perf_domain_cfg_parse(&cfg, cfg_str);
	rc = stob_perf_domain_tmpfs_opts(tmpfs_opts, sizeof tmpfs_opts, &cfg);
	if (rc != 0)
		return rc;

	ldom_location = stob_perf_domain_ldom_location(location_data);
	if (ldom_location == NULL)
		return M0_ERR(-ENOMEM);

	rc = mkdir(location_data, 0700);
	rc = rc != 0 ? M0_ERR(-errno) : 0;
#ifdef STOB_PERF_DOMAIN_CREATE_FORCE
	/* XXX This section is disabled to satisfy stob/ut/domain.c checks. */
	if (rc == -EEXIST) {
		rc = stob_perf_domain_dir_clean(location_data);
		if (rc == 0) {
			rc = mkdir(location_data, 0700);
			rc = rc != 0 ? M0_ERR(-errno) : 0;
		}
	}
#endif
	if (rc == 0) {
		dir_created = true;
		rc = mount("none", location_data, "tmpfs", MS_NODEV | MS_NOEXEC,
			   tmpfs_opts);
		rc = rc != 0 ? M0_ERR(-errno) : 0;
	}
	if (rc == 0)
		rc = stob_perf_domain_write_config(cfg_str, dom_key,
						   location_data);
	if (rc == 0)
		rc = m0_stob_domain_create(ldom_location, NULL, dom_key,
					   NULL, &ldom);
	M0_ASSERT(ergo(rc == 0, ldom != NULL));
	if (rc == 0)
		m0_stob_domain_fini(ldom);

	m0_free(ldom_location);

	if (rc != 0 && dir_created)
		(void)stob_perf_domain_dir_clean(location_data);

	return M0_RC(rc);
}

static int stob_perf_domain_destroy(struct m0_stob_type *type,
				    const char          *location_data)
{
	char *ldom_location = stob_perf_domain_ldom_location(location_data);
	int   rc;

	if (ldom_location == NULL)
		return M0_ERR(-ENOMEM);

	/* XXX TODO dump stats somewhere */

	rc = m0_stob_domain_destroy_location(ldom_location);
	if (rc != 0)
		M0_LOG(M0_ERROR, "Couldn't destroy linux domain: rc=%d", rc);
	m0_free(ldom_location);
	/*
	 * Destroy the domain regardless of the previous errors. Destroy()
	 * has to work even for partially created domains.
	 */
	rc = stob_perf_domain_dir_clean(location_data);

	return M0_RC(rc);
}

static struct m0_stob *stob_perf_alloc(struct m0_stob_domain *dom,
				       const struct m0_fid   *stob_fid)
{
	struct stob_perf *pstob;

	M0_ALLOC_PTR(pstob);
	if (pstob != NULL)
		pstob->sp_magic = M0_STOB_PERF_MAGIC;
	return pstob == NULL ? NULL : &pstob->sp_stob;
}

static void stob_perf_free(struct m0_stob_domain *dom,
			   struct m0_stob        *stob)
{
	struct stob_perf *pstob = stob_perf_container(stob);

	m0_free(pstob);
}

static int stob_perf_cfg_parse(const char  *str_cfg_create,
			       void       **cfg_create)
{
       return 0;
}

static void stob_perf_cfg_free(void *cfg_create)
{
}

static void stob_perf_linux_fid_set(struct m0_fid           *lstob_fid,
				    const struct m0_fid     *stob_fid,
				    struct stob_perf_domain *pdom)
{
	*lstob_fid = *stob_fid;
	m0_fid_tchange(lstob_fid, m0_stob_domain__type_id(
				m0_stob_domain_id_get(pdom->spd_ldom)));
}

static int stob_perf_linux_init_create(struct stob_perf_domain *pdom,
				       const struct m0_fid     *stob_fid,
				       struct m0_dtx           *dtx,
				       bool                     create,
				       struct m0_stob         **out)
{
	struct m0_stob *lstob;
	struct m0_fid   lstob_fid;
	const char     *create_cfg;
	int             rc;

	stob_perf_linux_fid_set(&lstob_fid, stob_fid, pdom);
	rc = m0_stob_find_by_key(pdom->spd_ldom, &lstob_fid, &lstob);
	if (rc != 0)
		return M0_RC(rc);

	M0_ASSERT(lstob != NULL);
	if (m0_stob_state_get(lstob) == CSS_UNKNOWN)
		rc = m0_stob_locate(lstob);
	if (rc == 0 && create && m0_stob_state_get(lstob) != CSS_NOENT)
		rc = M0_ERR(-EEXIST);
	if (rc == 0 && !create && m0_stob_state_get(lstob) != CSS_EXISTS)
		rc = M0_ERR(-ENOENT);
	if (rc == 0 && create) {
		create_cfg = pdom->spd_cfg.spc_is_null ? "/dev/null" : NULL;
		rc = m0_stob_create(lstob, dtx, create_cfg);
	}
	if (rc != 0)
		m0_stob_put(lstob);
	if (rc == 0)
		*out = lstob;
	return M0_RC(rc);
}

static void stob_perf_timer_start(struct stob_perf    *pstob,
				  struct stob_perf_io *pio)
{
	m0_time_t latency;

	latency = pstob->sp_pdom->spd_cfg.spc_latency_cb(pio);
	m0_timer_start(&pstob->sp_timer, m0_time_from_now(0, latency));
}

static void stob_perf_ast_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct stob_perf    *pstob =
				container_of(ast, struct stob_perf, sp_ast);
	struct stob_perf_io *pio;
	struct stob_perf_io *pio2;

	M0_PRE(pstob == ast->sa_datum);
	M0_PRE(pstob->sp_magic == M0_STOB_PERF_MAGIC);

	m0_timer_stop(&pstob->sp_timer);

	m0_mutex_lock(&pstob->sp_lock);
	pio = stob_perf_ios_tlist_pop(&pstob->sp_ios);
	M0_ASSERT(pio != NULL);
	if (!stob_perf_ios_tlist_is_empty(&pstob->sp_ios)) {
		pio2 = stob_perf_ios_tlist_head(&pstob->sp_ios);
		M0_ASSERT(pio2 != NULL);
		stob_perf_timer_start(pstob, pio2);
	}
	m0_mutex_unlock(&pstob->sp_lock);
	stob_perf_io_completed(pio);
}

static unsigned long stob_perf_timer_cb(unsigned long data)
{
	struct stob_perf   *pstob = (struct stob_perf *)data;
	struct m0_locality *loc;

	M0_PRE(pstob->sp_magic == M0_STOB_PERF_MAGIC);

	loc = m0_locality_get(data);
	M0_ASSERT(loc != NULL);
	pstob->sp_ast.sa_cb = &stob_perf_ast_cb;
	pstob->sp_ast.sa_datum = pstob;
	m0_sm_ast_post(loc->lo_grp, &pstob->sp_ast);

	return 0; /* XXX what to return here? */
}

static int stob_perf_init(struct m0_stob        *stob,
			  struct m0_stob_domain *dom,
			  const struct m0_fid   *stob_fid)
{
	struct stob_perf_domain *pdom = stob_perf_domain_container(dom);
	struct stob_perf        *pstob = stob_perf_container(stob);
	struct m0_stob          *lstob;
	int                      rc;

	M0_ENTRY();

	rc = m0_timer_init(&pstob->sp_timer, M0_TIMER_SOFT, NULL,
			   &stob_perf_timer_cb, (unsigned long)pstob);
	if (rc == 0) {
		rc = stob_perf_linux_init_create(pdom, stob_fid, NULL,
						 false, &lstob);
		if (rc != 0)
			m0_timer_fini(&pstob->sp_timer);
	}
	if (rc == 0) {
		stob->so_ops = &stob_perf_ops;
		pstob->sp_pdom = pdom;
		pstob->sp_backstore = lstob;
		stob_perf_ios_tlist_init(&pstob->sp_ios);
		m0_mutex_init(&pstob->sp_lock);
		M0_SET0(&pstob->sp_ast);
		/* XXX TODO init stats */
	}
	return M0_RC(rc);
}

static void stob_perf_fini(struct m0_stob *stob)
{
	struct stob_perf *pstob = stob_perf_container(stob);

	M0_ENTRY();

	/* XXX TODO dump stats? */

	m0_timer_fini(&pstob->sp_timer);
	m0_mutex_fini(&pstob->sp_lock);
	stob_perf_ios_tlist_fini(&pstob->sp_ios);

	/* Assume backstore is NULL when the stob is being destroyed. */
	if (pstob->sp_backstore != NULL) {
		m0_stob_put(pstob->sp_backstore);
		pstob->sp_backstore = NULL;
	}
	M0_LEAVE();
}

static void stob_perf_create_credit(struct m0_stob_domain  *dom,
				    struct m0_be_tx_credit *accum)
{
	struct stob_perf_domain *pdom = stob_perf_domain_container(dom);

	m0_stob_create_credit(pdom->spd_ldom, accum);
}

static int stob_perf_create(struct m0_stob        *stob,
			    struct m0_stob_domain *dom,
			    struct m0_dtx         *dtx,
			    const struct m0_fid   *stob_fid,
			    void                  *cfg)
{
	struct stob_perf_domain *pdom = stob_perf_domain_container(dom);
	struct m0_stob          *lstob;
	int                      rc;

	rc = stob_perf_linux_init_create(pdom, stob_fid, dtx, true, &lstob);
	if (rc == 0) {
		m0_stob_put(lstob);
		rc = stob_perf_init(stob, dom, stob_fid);
	}
	return M0_RC(rc);
}

static void stob_perf_destroy_credit(struct m0_stob *stob,
				     struct m0_be_tx_credit *accum)
{
	struct stob_perf *pstob = stob_perf_container(stob);

	m0_stob_destroy_credit(pstob->sp_backstore, accum);
}

static int stob_perf_destroy(struct m0_stob *stob, struct m0_dtx *dtx)
{
	struct stob_perf *pstob = stob_perf_container(stob);
	int               rc;

	rc = m0_stob_destroy(pstob->sp_backstore, dtx);
	if (rc == 0)
		pstob->sp_backstore = NULL;

	return M0_RC(rc);
}

static int stob_perf_punch_credit(struct m0_stob         *stob,
				  struct m0_indexvec     *want,
				  struct m0_indexvec     *got,
				  struct m0_be_tx_credit *accum)
{
	struct stob_perf *pstob = stob_perf_container(stob);

	got->iv_index[0] = 0;
	got->iv_vec.v_count[0] = M0_BINDEX_MAX + 1;
	if (pstob->sp_pdom->spd_cfg.spc_is_null)
		return 0;
	else
		return m0_stob_punch_credit(pstob->sp_backstore, want, got,
					    accum);
}

static int stob_perf_punch(struct m0_stob     *stob,
			   struct m0_indexvec *range,
			   struct m0_dtx      *dtx)
{
	struct stob_perf *pstob = stob_perf_container(stob);

	if (pstob->sp_pdom->spd_cfg.spc_is_null)
		return 0;
	else
		return m0_stob_punch(pstob->sp_backstore, range, dtx);
}

static void stob_perf_write_credit(const struct m0_stob_domain *dom,
				   const struct m0_stob_io     *io,
				   struct m0_be_tx_credit      *accum)
{
	struct stob_perf_domain *pdom =
		stob_perf_domain_container((struct m0_stob_domain *)dom);

	m0_stob_io_credit(io, pdom->spd_ldom, accum);
}

static uint32_t stob_perf_block_shift(struct m0_stob *stob)
{
	struct stob_perf *pstob = stob_perf_container(stob);

	return m0_stob_block_shift(pstob->sp_backstore);
}

static int stob_perf_fd(struct m0_stob *stob)
{
	struct stob_perf *pstob = stob_perf_container(stob);

	return m0_stob_fd(pstob->sp_backstore);
}

static void stob_perf_consume_io(struct stob_perf_io *pio)
{
	struct stob_perf *pstob = pio->spi_pstob;

	m0_mutex_lock(&pstob->sp_lock);
	if (stob_perf_ios_tlist_is_empty(&pstob->sp_ios))
		stob_perf_timer_start(pstob, pio);
	stob_perf_ios_tlist_add_tail(&pstob->sp_ios, pio);
	m0_mutex_unlock(&pstob->sp_lock);
}

static void stob_perf_io_completed(struct stob_perf_io *pio)
{
	struct m0_stob_io *io = pio->spi_io;
	struct m0_stob_io *lio = &pio->spi_lio;

	io->si_rc = lio->si_rc;
	io->si_count = lio->si_count;
	io->si_state = SIS_IDLE;
	m0_chan_broadcast_lock(&io->si_wait);
}

static bool stob_perf_io_lio_completed(struct m0_clink *clink)
{
	struct stob_perf_io *pio =
			container_of(clink, struct stob_perf_io, spi_clink);

	M0_PRE(pio->spi_pstob->sp_magic == M0_STOB_PERF_MAGIC);

	m0_clink_del(&pio->spi_clink);
	stob_perf_consume_io(pio);

	return true;
}

static int stob_perf_io_init(struct m0_stob    *stob,
			     struct m0_stob_io *io)
{
	struct stob_perf    *pstob = stob_perf_container(stob);
	struct stob_perf_io *pio;

	M0_ALLOC_PTR(pio);
	if (pio == NULL)
		return M0_ERR(-ENOMEM);

	m0_clink_init(&pio->spi_clink, &stob_perf_io_lio_completed);
	m0_stob_io_init(&pio->spi_lio);
	stob_perf_ios_tlink_init(pio);
	io->si_op = &stob_perf_io_ops;
	io->si_stob_private = pio;
	pio->spi_io = io;
	pio->spi_pstob = pstob;

	return M0_RC(0);
}

static int stob_perf_io_launch(struct m0_stob_io *io)
{
	struct stob_perf_io *pio = io->si_stob_private;
	struct stob_perf    *pstob = pio->spi_pstob;
	struct m0_stob_io   *lio = &pio->spi_lio;
	int                  rc;

	lio->si_flags  = io->si_flags;
	lio->si_user   = io->si_user;
	lio->si_stob   = io->si_stob;
	lio->si_opcode = io->si_opcode;

	m0_clink_add_lock(&lio->si_wait, &pio->spi_clink);

	rc = m0_stob_io_prepare_and_launch(lio, pstob->sp_backstore, io->si_tx,
					   io->si_scope);
	return M0_RC(rc);
}

static void stob_perf_io_fini(struct m0_stob_io *io)
{
	struct stob_perf_io *pio = io->si_stob_private;

	stob_perf_ios_tlink_fini(pio);
	m0_stob_io_fini(&pio->spi_lio);
	m0_clink_fini(&pio->spi_clink);
	m0_free(pio);
}

static struct m0_stob_type_ops stob_perf_type_ops = {
	.sto_register                = &stob_perf_type_register,
	.sto_deregister              = &stob_perf_type_deregister,
	.sto_domain_cfg_init_parse   = &stob_perf_domain_cfg_init_parse,
	.sto_domain_cfg_init_free    = &stob_perf_domain_cfg_init_free,
	.sto_domain_cfg_create_parse = &stob_perf_domain_cfg_create_parse,
	.sto_domain_cfg_create_free  = &stob_perf_domain_cfg_create_free,
	.sto_domain_init             = &stob_perf_domain_init,
	.sto_domain_create           = &stob_perf_domain_create,
	.sto_domain_destroy          = &stob_perf_domain_destroy,
};

static struct m0_stob_domain_ops stob_perf_domain_ops = {
	.sdo_fini               = &stob_perf_domain_fini,
	.sdo_stob_alloc         = &stob_perf_alloc,
	.sdo_stob_free          = &stob_perf_free,
	.sdo_stob_cfg_parse     = &stob_perf_cfg_parse,
	.sdo_stob_cfg_free      = &stob_perf_cfg_free,
	.sdo_stob_init          = &stob_perf_init,
	.sdo_stob_create_credit = &stob_perf_create_credit,
	.sdo_stob_create        = &stob_perf_create,
	.sdo_stob_write_credit  = &stob_perf_write_credit,
};

static struct m0_stob_ops stob_perf_ops = {
	.sop_fini	    = &stob_perf_fini,
	.sop_destroy_credit = &stob_perf_destroy_credit,
	.sop_destroy	    = &stob_perf_destroy,
	.sop_punch_credit   = &stob_perf_punch_credit,
	.sop_punch          = &stob_perf_punch,
	.sop_io_init        = &stob_perf_io_init,
	.sop_block_shift    = &stob_perf_block_shift,
	.sop_fd             = &stob_perf_fd,
};

static const struct m0_stob_io_op stob_perf_io_ops = {
	.sio_launch  = stob_perf_io_launch,
	.sio_fini    = stob_perf_io_fini
};

const struct m0_stob_type m0_stob_perf_type = {
	.st_ops  = &stob_perf_type_ops,
	.st_fidt = {
		.ft_id   = STOB_TYPE_PERF,
		.ft_name = "perfstob",
	},
};

/** @} end group stobperf */

#undef M0_TRACE_SUBSYSTEM

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
