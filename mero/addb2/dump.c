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
 * Original creation date: 06-Mar-2015
 */


/**
 * @addtogroup addb2
 *
 * @{
 */

#include <dlfcn.h>
#include <err.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sysexits.h>
#include <execinfo.h>
#include <signal.h>
#include <bfd.h>
#include <stdlib.h>                    /* qsort */
#include <unistd.h>                    /* sleep */

#include "lib/memory.h"
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/tlist.h"
#include "lib/varr.h"
#include "lib/getopts.h"
#include "lib/uuid.h"                  /* m0_node_uuid_string_set */

#include "rpc/item.h"                  /* m0_rpc_item_type_lookup */
#include "rpc/rpc_opcodes_xc.h"        /* m0_xc_M0_RPC_OPCODES_enum */
#include "fop/fop.h"
#include "stob/domain.h"
#include "stob/stob.h"
#include "stob/addb2.h"
#include "stob/addb2_xc.h"
#include "mero/init.h"
#include "module/instance.h"
#include "rpc/rpc_opcodes.h"           /* M0_OPCODES_NR */
#include "rpc/bulk_xc.h"
#include "rpc/addb2.h"
#include "rpc/addb2_xc.h"
#include "be/addb2.h"
#include "be/addb2_xc.h"
#include "clovis/clovis.h"
#include "clovis/clovis_internal_xc.h"
#include "clovis/clovis_addb_xc.h"
#include "clovis/clovis_xc.h"
#include "ioservice/io_addb2.h"
#include "ioservice/io_addb2_xc.h"
#include "dix/dix_addb_xc.h"
#include "dix/req_xc.h"
#include "cas/cas_addb2_xc.h"

#include "addb2/identifier.h"
#include "addb2/consumer.h"
#include "addb2/storage.h"
#include "addb2/counter.h"
#include "addb2/histogram.h"

#include "cob/cob_xc.h"
#include "stob/addb2.h"
#include "stob/addb2_xc.h"                /* m0_xc_m0_stio_req_states_enum */
#include "net/addb2.h"
#include "ioservice/io_addb2.h"
#include "cas/cas_addb2.h"
#include "m0t1fs/linux_kernel/m0t1fs_addb2.h"
#include "sns/cm/cm.h"                 /* m0_sns_cm_repair_trigger_fop_init */
#include "clovis/clovis_addb.h"
#include "dix/dix_addb.h"
#include "cas/cas_addb2.h"
#include "xcode/xcode.h"
#include "scripts/systemtap/kem/kem_id.h"
#include "addb2/addb2_internal.h"
#include "lib/trace.h"

enum {
	BUF_SIZE  = 4096,
	PLUGINS_MAX = 64
};

struct fom {
	struct m0_tlink           fo_linkage;
	uint64_t                  fo_addr;
	uint64_t                  fo_tid;
	const struct m0_fom_type *fo_type;
	uint64_t                  fo_magix;
};

struct m0_addb2__context {
	struct fom                    c_fom;
	const struct m0_addb2_record *c_rec;
	const struct m0_addb2_value  *c_val;
};

struct plugin
{
	const char                *p_path;
	void                      *p_handle;
	uint64_t                   p_flag;
	m0_addb2__intrp_load_t     p_intrp_load;
	struct m0_addb2__id_intrp *p_intrp;
};

static struct plugin plugins[PLUGINS_MAX];
static size_t        plugins_nr;

static struct m0_varr value_id;

static void id_init  (void);
static void id_fini  (void);
static void id_set   (struct m0_addb2__id_intrp *intrp);
static void id_set_nr(struct m0_addb2__id_intrp *intrp, int nr);

static struct m0_addb2__id_intrp *id_get(uint64_t id);

static void rec_dump(struct m0_addb2__context *ctx,
                     const struct m0_addb2_record *rec);

static void val_dump(struct m0_addb2__context *ctx, const char *prefix,
                     const struct m0_addb2_value *val, int indent, bool cr);

static void context_fill(struct m0_addb2__context *ctx,
                         const struct m0_addb2_value *val);

static void file_dump(struct m0_stob_domain *dom, const char *fname);
static int  plugin_load(struct plugin *plugin);
static void plugin_unload(struct plugin *plugin);
static int plugins_load(void);
static void plugins_unload(void);

static void libbfd_init(const char *libpath);
static void libbfd_fini(void);
static void libbfd_resolve(uint64_t delta, char *buf);

static void flate(void);
static void deflate(void);

static void misc_init(void);
static void misc_fini(void);

#define DOM "./_addb2-dump"
extern int  optind;
static bool flatten = false;
static bool deflatten = false;
static bool json_output = false;
static const char *json_extra_data = NULL;
static m0_bindex_t offset = 0;
static int delay = 0;

extern void m0_dix_cm_repair_cpx_init(void);
extern void m0_dix_cm_repair_cpx_fini(void);
extern void m0_dix_cm_rebalance_cpx_init(void);
extern void m0_dix_cm_rebalance_cpx_fini(void);


static int plugin_add(const char *path)
{
    struct plugin *plugin;

    M0_ASSERT(plugins_nr < ARRAY_SIZE(plugins));
    M0_PRE(path != NULL);

    plugin = &plugins[plugins_nr];
    plugin->p_path = path;
    ++plugins_nr;
    return M0_RC(0);
}

int main(int argc, char **argv)
{
	struct m0_stob_domain  *dom;
	struct m0               instance = {0};
	int                     result;
	int                     i;
	int                     rc;

	m0_node_uuid_string_set(NULL);
	result = m0_init(&instance);
	if (result != 0)
		err(EX_CONFIG, "Cannot initialise mero: %d", result);

	misc_init();

	result = M0_GETOPTS("m0addb2dump", argc, argv,
			M0_FORMATARG('o', "Starting offset",
				     "%"SCNx64, &offset),
			M0_FORMATARG('c', "Continuous dump interval (sec.)",
				     "%i", &delay),
			M0_STRINGARG('l', "Mero library path",
				     LAMBDA(void, (const char *path) {
						     libbfd_init(path);
					     })),

			M0_STRINGARG('p', "Path to plugin",
				    LAMBDA(void, (const char *path) {
						rc = plugin_add(path);

						if (rc != 0)
							err(EX_OSERR, "Memory allocation failed");
					})),
			M0_FLAGARG('f', "Flatten output", &flatten),
			M0_FLAGARG('d', "De-flatten input", &deflatten),
			M0_FLAGARG('j', "JSON output (see jsonlines.org)", &json_output),
			M0_STRINGARG('J', "Embed extra JSON data into every record",
				    LAMBDA(void, (const char *json_text) {
					    json_extra_data = strdup(json_text);
					}))
			);
	if (result != 0)
		err(EX_USAGE, "Wrong option: %d", result);
	if (deflatten) {
		if (flatten || optind < argc)
			err(EX_USAGE, "De-flattening is exclusive.");
		deflate();
		return EX_OK;
	}
	if (flatten && optind == argc) {
		flate();
		return EX_OK;
	}
	if ((delay != 0 || offset != 0) && optind + 1 < argc)
		err(EX_USAGE,
		    "Staring offset and continuous dump imply single file.");
	result = m0_stob_domain_init("linuxstob:"DOM, "directio=true", &dom);
	if (result == 0)
		m0_stob_domain_destroy(dom);
	else if (result != -ENOENT)
		err(EX_CONFIG, "Cannot destroy domain: %d", result);
	result = m0_stob_domain_create_or_init("linuxstob:"DOM, "directio=true",
					       /* domain key, not important */
					       8, NULL, &dom);
	if (result != 0)
		err(EX_CANTCREAT, "Cannot create domain: %d", result);

	rc = plugins_load();

	if (rc != 0)
		err(EX_CONFIG, "Plugins loading failed");

	id_init();
	for (i = optind; i < argc; ++i)
		file_dump(dom, argv[i]);

	plugins_unload();

	id_fini();
	m0_stob_domain_destroy(dom);
	libbfd_fini();
	misc_fini();
	m0_fini();
	return EX_OK;
}

static int plugin_load(struct plugin *plugin)
{
    M0_ENTRY();
    M0_PRE(plugin != NULL);
    M0_PRE(plugin->p_path != NULL);

    plugin->p_handle = dlopen(plugin->p_path, RTLD_LAZY);

    if (plugin->p_handle == NULL)
        return M0_ERR_INFO(-ELIBACC, "%s", dlerror());

    plugin->p_intrp_load = dlsym(plugin->p_handle, M0_ADDB2__PLUGIN_FUNC_NAME);

    if (plugin->p_intrp_load == NULL) {
        dlclose(plugin->p_handle);
        plugin->p_handle = NULL;
        return M0_ERR_INFO(-ELIBBAD, "%s", dlerror());
    }

    return M0_RC(0);
}

static void plugin_unload(struct plugin *plugin)
{
    M0_ENTRY();
    M0_PRE(plugin != NULL);
    M0_PRE(plugin->p_handle != NULL);

    dlclose(plugin->p_handle);
}

static int plugins_load(void)
{
    struct plugin *p;
    int            i;
    int            rc;

    for (i = 0; i < plugins_nr; ++i) {
        p = &plugins[i];
        rc = plugin_load(p) ?: p->p_intrp_load(p->p_flag, &p->p_intrp);

        if (rc != 0)
            return M0_ERR(rc);
    }

    return M0_RC(0);
}

static void plugins_unload(void)
{
    struct plugin *plugin;
    int            i;

    for (i = 0; i < plugins_nr; ++i) {
        plugin = &plugins[i];
        plugin_unload(plugin);
    }

    plugins_nr = 0;
}

static bool intrps_equal(const struct m0_addb2__id_intrp *intrp0,
                         const struct m0_addb2__id_intrp *intrp1)
{
    return memcmp(intrp0, intrp1, sizeof(struct m0_addb2__id_intrp)) == 0;
}

static void file_dump(struct m0_stob_domain *dom, const char *fname)
{
	struct m0_stob         *stob;
	struct m0_addb2_sit    *sit;
	struct stat             buf;
	struct m0_addb2_record *rec;
	int                     result;
	struct m0_stob_id       stob_id;

	m0_stob_id_make(0, 1 /* stob key, any */, &dom->sd_id, &stob_id);
	result = m0_stob_find(&stob_id, &stob);
	if (result != 0)
		err(EX_CANTCREAT, "Cannot find stob: %d", result);
	if (m0_stob_state_get(stob) == CSS_UNKNOWN) {
		result = m0_stob_locate(stob);
		if (result != 0)
			err(EX_CANTCREAT, "Cannot locate stob: %d", result);
	}
	result = m0_stob_create(stob, NULL, fname);
	if (result != 0)
		err(EX_NOINPUT, "Cannot create stob: %d", result);
	M0_ASSERT(m0_stob_state_get(stob) == CSS_EXISTS);
	result = stat(fname, &buf);
	if (result != 0)
		err(EX_NOINPUT, "Cannot stat: %d", result);
	do {
		result = m0_addb2_sit_init(&sit, stob, offset);
		if (delay > 0 && result == -EPROTO) {
			printf("Sleeping for %i seconds (%lx).\n",
			       delay, offset);
			sleep(delay);
			continue;
		}
		if (result != 0)
			err(EX_DATAERR, "Cannot initialise iterator: %d",
			    result);
		while ((result = m0_addb2_sit_next(sit, &rec)) > 0) {
			rec_dump(&(struct m0_addb2__context){}, rec);
			if (rec->ar_val.va_id == M0_AVI_SIT)
				offset = rec->ar_val.va_data[3];
		}
		if (result != 0)
			err(EX_DATAERR, "Iterator error: %d", result);
		m0_addb2_sit_fini(sit);
	} while (delay > 0);
	m0_stob_destroy(stob, NULL);
}

static void dec(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	sprintf(buf, "%"PRId64, v[0]);
}

static void hex(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	if (json_output)
		/* JSON spec supports only decimal format (int and float) */
		sprintf(buf, "%"PRId64, v[0]);
	else
		sprintf(buf, "%"PRIx64, v[0]);
}

static void hex0x(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	if (json_output)
		/* JSON spec supports only decimal format (int and float) */
		sprintf(buf, "%"PRId64, v[0]);
	else
		sprintf(buf, "0x%"PRIx64, v[0]);
}

static void oct(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	if (json_output)
		/* JSON spec supports only decimal format (int and float) */
		sprintf(buf, "%"PRId64, v[0]);
	else
		sprintf(buf, "%"PRIo64, v[0]);
}

static void ptr(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	if (json_output)
		/* JSON spec supports only decimal format (int and float) */
		sprintf(buf, "{\"ptr\":%"PRId64"}", (long)*(void **)v);
	else
		sprintf(buf, "@%p", *(void **)v);
}

static void bol(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	sprintf(buf, "%s", v[0] ? "true" : "false");
}

static void fid(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	if (json_output)
		/* JSON spec supports only decimal format (int and float) */
		sprintf(buf, "{\"container\":%"PRId64","
			     "\"key\":%"PRId64"}",
			     FID_P((struct m0_fid *)v));
	else
		sprintf(buf, FID_F, FID_P((struct m0_fid *)v));
}

static void skip(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	if (json_output)
		/*
		 * This is a placeholder guard that must effect parsing error of
		 * resulting JSON output. Normally, "skip" records should be
		 * removed from the output entirely. This is aimed to protect
		 * against the case where a "skip" record might be printed
		 * partially, e.g. only "key" w/o corresponding "value". This
		 * way we can catch it and fix.
		 */
		sprintf(buf, "[FIXME]");
	else
		buf[0] = 0;
}

static void _clock(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	m0_time_t stamp = v[0];
	time_t    ts    = m0_time_seconds(stamp);
	struct tm tm;

	if (json_output) {
		gmtime_r(&ts, &tm);
		/* ISO8601 formating */
		sprintf(buf, "\"%04d-%02d-%02dT%02d:%02d:%02d.%09luZ\"",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			m0_time_nanoseconds(stamp));
	}else {
		localtime_r(&ts, &tm);
		sprintf(buf, "%04d-%02d-%02d-%02d:%02d:%02d.%09lu",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			m0_time_nanoseconds(stamp));
	}
}

static void duration(struct m0_addb2__context *ctx, const uint64_t *v,
                     char *buf)
{
	m0_time_t elapsed = v[0];

	if (json_output)
		sprintf(buf, "{\"sec\":%"PRId64",\"ns\":%"PRId64"}",
			m0_time_seconds(elapsed), m0_time_nanoseconds(elapsed));
	else
		sprintf(buf, "%"PRId64".%09"PRId64,
			m0_time_seconds(elapsed), m0_time_nanoseconds(elapsed));
}

static void fom_type(struct m0_addb2__context *ctx, const uint64_t *v,
                     char *buf)
{
	const struct m0_fom_type *ftype = ctx->c_fom.fo_type;
	const struct m0_sm_conf  *conf  = &ftype->ft_conf;
	const char               *fmt;

	if (ftype != NULL) {
		M0_ASSERT(v[2] < conf->scf_nr_states);
		fmt = json_output ?
			"\"type\":\"%s\",\"transitions\":%"PRId64
			",\"phase\":\"%s\""
			: "'%s' transitions: %"PRId64" phase: %s";
		sprintf(buf, fmt, conf->scf_name, v[1],
			conf->scf_state[v[2]].sd_name);
	} else {
		fmt = json_output ?
			"\"type\":\"UNKNOWN-%"PRId64"\",\"transitions\":%"PRId64
			",\"phase\":\"%s\""
			: "?'UNKNOWN-%"PRId64"' transitions: %"PRId64
			  " phase: %"PRId64;
		sprintf(buf, fmt, ctx->c_fom.fo_tid, v[1], v[2]);
	}
}

static void sm_state(const struct m0_sm_conf *conf,
		     struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	const char *fmt;

	/*
	 * v[3] - time stamp
	 * v[2] - transition id
	 * v[1] - state id
	 * v[0] - sm_id
	 */

	if (conf->scf_trans == NULL) {
		M0_ASSERT(v[1] == 0);
		fmt = json_output ?
			"{\"sm_id\":%"PRId64",\"tgt_state\":\"%s\"}" :
			"sm_id: %"PRIu64" --> %s";
		sprintf(buf, fmt, v[0], conf->scf_state[v[2]].sd_name);
	} else {
		/* There's no explicit transition into INIT state */
		if (v[1] == (uint32_t)~0) {
			fmt = json_output ?
				"{\"sm_id\":%"PRId64",\"tgt_state\":\"%s\"}" :
				"sm_id: %"PRIu64" o--> %s";
			sprintf(buf, fmt, v[0], conf->scf_state[v[2]].sd_name);
		} else {
			struct m0_sm_trans_descr *d = &conf->scf_trans[v[1]];

			M0_ASSERT(d->td_tgt == v[2]);
			fmt = json_output ?
				"{\"sm_id\":%"PRId64
				",\"src_state\":\"%s\""
				",\"cause\":\"%s\""
				",\"tgt_state\":\"%s\"}"
			      : "sm_id: %"PRIu64" %s -[%s]-> %s";
			sprintf(buf, fmt, v[0],
				conf->scf_state[d->td_src].sd_name, d->td_cause,
				conf->scf_state[d->td_tgt].sd_name);
		}
	}
}

extern struct m0_sm_conf fom_states_conf;
static void fom_state(struct m0_addb2__context *ctx, const uint64_t *v,
                      char *buf)
{
	sm_state(&fom_states_conf, ctx, v, buf);
}

static void fom_phase(struct m0_addb2__context *ctx, const uint64_t *v,
                      char *buf)
{
	const struct m0_sm_conf        *conf;
	const struct m0_sm_trans_descr *d;
	const struct m0_sm_state_descr *s;
	const char                     *fmt;
	/*
	 * v[0] - sm_id
	 * v[1] - transition id
	 * v[2] - state id
	 */
	if (ctx->c_fom.fo_type != NULL) {
		conf = &ctx->c_fom.fo_type->ft_conf;
		if (conf->scf_trans == NULL) {
			M0_ASSERT(v[1] == 0);
			s = &conf->scf_state[v[2]];
			fmt = json_output ?
				"{\"sm_id\":%"PRId64",\"tgt_state\":\"%s\"}" :
				"sm_id: %"PRIu64" --> %s";
			sprintf(buf, fmt, v[0], s->sd_name);
		} else if (v[1] < conf->scf_trans_nr) {
			d = &conf->scf_trans[v[1]];
			s = &conf->scf_state[d->td_tgt];
			fmt = json_output ?
				"{\"sm_id\":%"PRId64
				",\"src_state\":\"%s\""
				",\"cause\":\"%s\""
				",\"tgt_state\":\"%s\"}"
			      : "sm_id: %"PRIu64" %s -[%s]-> %s";
			sprintf(buf, fmt, v[0],
				conf->scf_state[d->td_src].sd_name,
				d->td_cause, s->sd_name);
		} else {
			fmt = json_output ?
				"{\"sm_id\":%"PRId64
				",\"tgt_state\":\"phase transition %i\"}"
				: "sm_id: %"PRIu64" phase transition %i";
			sprintf(buf, fmt, v[0], (int)v[1]);
		}
	} else {
		fmt = json_output ?
			"{\"sm_id\":%"PRId64
			",\"tgt_state\":\"phase ast transition %i\"}"
			: "sm_id: %"PRIu64" phase ast transition %i";
		sprintf(buf, fmt, v[0], (int)v[1]);
	}
}

static void rpcop(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	struct m0_rpc_item_type *it = m0_rpc_item_type_lookup(v[0]);

	if (it != NULL) {
		char area[64];
		sprintf(buf, json_output ? "\"%s\"" : "%s",
			m0_xcode_enum_print(&m0_xc_M0_RPC_OPCODES_enum,
					    it->rit_opcode, area));
	} else if (v[0] == 0) {
		sprintf(buf, json_output ? "\"none\"" : "none");
	} else
		sprintf(buf, json_output ? "\"rpc#%"PRId64"\"" :
			"?rpc: %"PRId64, v[0]);
}

static void sym(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	const void *addr = m0_ptr_unwrap(v[0]);

	if (v[0] != 0) {
		int     rc;
		Dl_info info;

		libbfd_resolve(v[0], buf);
		buf += strlen(buf);
		rc = dladdr(addr, &info); /* returns non-zero on *success* */
		if (rc != 0 && info.dli_sname != NULL) {
			sprintf(buf, json_output ?
					"\"bfd_symbol\":"
					"{\"name\":\"%s\",\"addr\":%lu},"
					: " [%s+%lx]",
					info.dli_sname, addr - info.dli_saddr);
			buf += strlen(buf);
		}
		if (json_output)
			sprintf(buf, "\"symbol\":{\"addr\":%"PRId64","
				     "\"addr_ptr_wrap\":%"PRId64"}",
				     (long)addr, v[0]);
		else
			sprintf(buf, " @%p/%"PRIx64, addr, v[0]);
	}
}

static void counter(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	struct m0_addb2_counter_data *d = (void *)&v[0];
	char                          sym_buf[BUF_SIZE];
	const char                   *fmt;
	double avg;
	double dev;

	avg = d->cod_nr > 0 ? ((double)d->cod_sum) / d->cod_nr : 0;
	dev = d->cod_nr > 1 ? ((double)d->cod_ssq) / d->cod_nr - avg * avg : 0;

	fmt = json_output ?
		"\"counter\":{"
		"\"nr\":%"PRId64
		",\"min\":%"PRId64
		",\"max\":%"PRId64
		",\"avg\":%g"
		",\"dev\":%g"
		",\"datum\":%"PRId64"}"
	      : " nr: %"PRId64" min: %"PRId64" max: %"PRId64
		" avg: %f dev: %f datum: %"PRIx64" ";

	sprintf(buf + strlen(buf), fmt, d->cod_nr, d->cod_min, d->cod_max,
		avg, dev, d->cod_datum);

	if (json_output) {
		sym_buf[0] = '\0';
		sym(ctx, &d->cod_datum, sym_buf);
		if (strlen(sym_buf) > 0)
			sprintf(buf + strlen(buf), ",%s", sym_buf);
	} else {
		sym(ctx, &d->cod_datum, buf + strlen(buf));
	}
}

static void hbar(char *buf, uint32_t val, uint32_t m)
{
	int len;
	static const char p[] = /* 60 characters. */
		"************************************************************";
	if (!json_output) {
		len = val * strlen(p) / m;
		sprintf(buf + strlen(buf), "%*.*s", len, len, p);
	}
}

static void hist(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	struct m0_addb2_hist_data *hd = (void *)&v[M0_ADDB2_COUNTER_VALS];
	int                        i;
	int64_t                    start;
	uint64_t                   step;
	uint32_t                   m = 1; /* avoid division by 0. */
	char                       cr = flatten ? ' ' : '\n';

	counter(ctx, v, buf);
	if (json_output)
		/* TODO: enable histogram support in JSON format */
		return;
	start = hd->hd_min;
	step  = (hd->hd_max - hd->hd_min) / (M0_ADDB2_HIST_BUCKETS - 2);
	sprintf(buf + strlen(buf), " %"PRId32, hd->hd_bucket[0]);
	for (i = 1; i < ARRAY_SIZE(hd->hd_bucket); ++i) {
		sprintf(buf + strlen(buf), " %"PRId64": %"PRId32,
			start, hd->hd_bucket[i]);
		start += step;
		m = max32(m, hd->hd_bucket[i]);
	}
	sprintf(buf + strlen(buf), "%c|           : %9"PRId32" | ",
		cr, hd->hd_bucket[0]);
	hbar(buf, hd->hd_bucket[0], m);
	for (i = 1, start = hd->hd_min; i < ARRAY_SIZE(hd->hd_bucket); ++i) {
		sprintf(buf + strlen(buf), "%c| %9"PRId64" : %9"PRId32" | ",
			cr, start, hd->hd_bucket[i]);
		hbar(buf, hd->hd_bucket[i], m);
		start += step;
	}
}

static void sm_trans(const struct m0_sm_conf *conf, const char *name,
		     struct m0_addb2__context *ctx, char *buf)
{
	int nob;
	const char *fmt;
	int idx = ctx->c_val->va_id - conf->scf_addb2_counter;
	const struct m0_sm_trans_descr *trans = &conf->scf_trans[idx];

	M0_PRE(conf->scf_addb2_key > 0);
	M0_PRE(0 <= idx && idx < 200);

	fmt = json_output ?
		"{\"name\":\"%s\""
		",\"sm_name\":\"%s\""
		",\"src_state\":\"%s\""
		",\"cause\":\"%s\""
		",\"tgt_state\":\"%s\"},"
	      : "%s/%s: %s -[%s]-> %s ";
	nob = sprintf(buf, fmt, name, conf->scf_name,
		      conf->scf_state[trans->td_src].sd_name,
		      trans->td_cause, conf->scf_state[trans->td_tgt].sd_name);
	hist(ctx, &ctx->c_val->va_data[0], buf + nob);
}

static void fom_state_counter(struct m0_addb2__context *ctx, char *buf)
{
	sm_trans(&fom_states_conf, "", ctx, buf);
}

static void fop_counter(struct m0_addb2__context *ctx, char *buf)
{
	uint64_t mask = ctx->c_val->va_id - M0_AVI_FOP_TYPES_RANGE_START;
	struct m0_fop_type *fopt = m0_fop_type_find(mask >> 12);
	const struct m0_sm_conf *conf;

	if (fopt != NULL) {
		switch ((mask >> 8) & 0xf) {
		case M0_AFC_PHASE:
			conf = &fopt->ft_fom_type.ft_conf;
			break;
		case M0_AFC_STATE:
		conf = &fopt->ft_fom_type.ft_state_conf;
		break;
		case M0_AFC_RPC_OUT:
			conf = &fopt->ft_rpc_item_type.rit_outgoing_conf;
			break;
		case M0_AFC_RPC_IN:
			conf = &fopt->ft_rpc_item_type.rit_incoming_conf;
			break;
		default:
			M0_IMPOSSIBLE("Wrong mask.");
		}
		sm_trans(conf, fopt->ft_name, ctx, buf);
	} else {
		if (json_output)
			sprintf(buf + strlen(buf),
				"{\"error\":\"unknown-fop-mask\""
				",\"msg\":\"Unknown FOP mask 0x%"PRIx64"\""
				",\"fop_mask\":%"PRId64"},",
				mask, mask);
		else
			sprintf(buf + strlen(buf),
				" unknown-fop-mask: %"PRIx64, mask);
	}
}

static void rpc_in(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	extern const struct m0_sm_conf incoming_item_sm_conf;
	sm_state(&incoming_item_sm_conf, ctx, v, buf);
}

static void rpc_out(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	extern const struct m0_sm_conf outgoing_item_sm_conf;
	sm_state(&outgoing_item_sm_conf, ctx, v, buf);
}

extern struct m0_sm_conf be_tx_sm_conf;
static void tx_state(struct m0_addb2__context *ctx, const uint64_t *v,
                     char *buf)
{
	sm_state(&be_tx_sm_conf, ctx, v, buf);
}

static void tx_state_counter(struct m0_addb2__context *ctx, char *buf)
{
	sm_trans(&be_tx_sm_conf, "tx", ctx, buf);
}

extern struct m0_sm_conf op_states_conf;
static void beop_state_counter(struct m0_addb2__context *ctx, char *buf)
{
	sm_trans(&op_states_conf, "be-op", ctx, buf);
}

extern struct m0_sm_conf clovis_op_conf;
static void clovis_op_state(struct m0_addb2__context *ctx, const uint64_t *v,
                            char *buf)
{
	sm_state(&clovis_op_conf, ctx, v, buf);
}

static void clovis_state_counter(struct m0_addb2__context *ctx, char *buf)
{
	sm_trans(&clovis_op_conf, "clovis_op", ctx, buf);
}

extern struct m0_sm_conf dix_req_sm_conf;
static void dix_op_state(struct m0_addb2__context *ctx, const uint64_t *v,
                         char *buf)
{
	sm_state(&dix_req_sm_conf, ctx, v, buf);
}

static void dix_state_counter(struct m0_addb2__context *ctx, char *buf)
{
	sm_trans(&dix_req_sm_conf, "dix_req", ctx, buf);
}

extern struct m0_sm_conf cas_req_sm_conf;
static void cas_op_state(struct m0_addb2__context *ctx, const uint64_t *v,
                         char *buf)
{
	sm_state(&cas_req_sm_conf, ctx, v, buf);
}

static void cas_state_counter(struct m0_addb2__context *ctx, char *buf)
{
	sm_trans(&cas_req_sm_conf, "cas_req", ctx, buf);
}

extern struct m0_sm_conf io_sm_conf;
static void ioo_req_state(struct m0_addb2__context *ctx, const uint64_t *v,
                          char *buf)
{
	sm_state(&io_sm_conf, ctx, v, buf);
}

static void ioo_state_counter(struct m0_addb2__context *ctx, char *buf)
{
	sm_trans(&io_sm_conf, "ioo_req", ctx, buf);
}

static void cob_req_state(struct m0_addb2__context *ctx, const uint64_t *v,
                          char *buf)
{
	uint64_t id = v[0];
	const struct m0_xcode_enum_val *xev =
		&m0_xc_m0_clovis_cob_req_states_enum.xe_val[id];
	M0_ASSERT(xev->xev_idx == id);

	sprintf(buf, json_output ? "\"%s\"" : "%s", xev->xev_name);
}

static void bulk_op_state(struct m0_addb2__context *ctx, const uint64_t *v,
			  char *buf)
{
	uint64_t id = v[0];

	sprintf(buf, json_output ? "\"%s\"" : "%s",
		m0_xcode_enum_print(&m0_xc_m0_rpc_bulk_op_states_enum,
				    id, buf));
}

static void stob_io_req_state(struct m0_addb2__context *ctx,
			      const uint64_t *v, char *buf)
{
	uint64_t id = v[0];
	sprintf(buf, json_output ? "\"%s\"" : "%s",
		m0_xcode_enum_print(&m0_xc_m0_addb2_stio_req_labels_enum,
				    id, buf));
}

static void attr(struct m0_addb2__context *ctx, const uint64_t *v, char *buf)
{
	/**
	 * v[0] - attr name
	 * v[1] - attr val
	 */
	static const struct range_xen {
		int                   start;
		int                   end;
		struct m0_xcode_enum *xen;
	} names[] = {
		{ M0_AVI_CLOVIS_RANGE_START, M0_AVI_DIX_RANGE_START,
		  &m0_xc_m0_avi_clovis_labels_enum },
		{ M0_AVI_DIX_RANGE_START, M0_AVI_KEM_RANGE_START,
		  &m0_xc_m0_avi_dix_labels_enum },
		{ M0_AVI_CAS_RANGE_START, M0_AVI_CLOVIS_RANGE_START,
		  &m0_xc_m0_avi_cas_labels_enum },
		{ M0_AVI_RPC_RANGE_START, M0_AVI_ADDB2_RANGE_START,
		  &m0_xc_m0_avi_rpc_labels_enum },
		{ M0_AVI_IOS_RANGE_START, M0_AVI_STOB_RANGE_START,
                  &m0_xc_m0_avi_ios_io_labels_enum },
                { M0_AVI_STOB_RANGE_START, M0_AVI_RPC_RANGE_START,
                  &m0_xc_m0_avi_stob_io_labels_enum },
                { M0_AVI_BE_RANGE_START, M0_AVI_NET_RANGE_START,
                  &m0_xc_m0_avi_be_labels_enum },
	}, clovis_opcode_xc[] = {
                { M0_CLOVIS_EO_INVALID, M0_CLOVIS_EO_NR,
                  &m0_xc_m0_clovis_entity_opcode_enum },
                { M0_CLOVIS_OC_READ, M0_CLOVIS_OC_NR,
                  &m0_xc_m0_clovis_obj_opcode_enum },
                { M0_CLOVIS_IC_GET, M0_CLOVIS_IC_NR,
                  &m0_xc_m0_clovis_idx_opcode_enum },
	};
	static const struct attr_name_val {
		int                   attr_name;
		struct m0_xcode_enum *val_xen;
	} clovis_values[] = {
		{ M0_AVI_CLOVIS_OP_ATTR_ENTITY_ID,
		  &m0_xc_m0_clovis_entity_type_enum },
	}, ios_values[] = {
		{ M0_AVI_IOS_IO_ATTR_FOMCOB_FOP_TYPE, &m0_xc_m0_cob_op_enum },
	}, dix_values[] = {
		{ M0_AVI_DIX_REQ_ATTR_REQ_TYPE, &m0_xc_dix_req_type_enum },
	}, rpc_values[] = {
		{ M0_AVI_RPC_ATTR_OPCODE, &m0_xc_M0_RPC_OPCODES_enum },
		{ M0_AVI_RPC_BULK_ATTR_OP, &m0_xc_m0_rpc_bulk_op_type_enum },
	};
	static struct {
		struct m0_xcode_enum       *attr_name_xen;
		const struct attr_name_val *name_val;
		int                         name_val_nr;
	} vnmap[] = {
		{ &m0_xc_m0_avi_clovis_labels_enum, clovis_values,
		  ARRAY_SIZE(clovis_values) },
		{ &m0_xc_m0_avi_ios_io_labels_enum, ios_values,
		  ARRAY_SIZE(ios_values) },
		{ &m0_xc_m0_avi_dix_labels_enum, dix_values,
		  ARRAY_SIZE(dix_values) },
		{ &m0_xc_m0_avi_rpc_labels_enum, rpc_values,
		  ARRAY_SIZE(rpc_values) },
	}, *vn = NULL;
	uint64_t attr_name                  = v[0];
	uint64_t attr_val                   = v[1];
	struct m0_xcode_enum *attr_name_xen = NULL;
	struct m0_xcode_enum *attr_val_xen  = NULL;
	int i;
	char *buf_ptr;

	buf_ptr = buf;

	for (i = 0; i < ARRAY_SIZE(names); ++i) {
		if (attr_name >= names[i].start && attr_name < names[i].end) {
			attr_name_xen = names[i].xen;
			break;
		}
        }
	M0_ASSERT_INFO(i != ARRAY_SIZE(names), "Unsupported subsystem: %"PRIu64,
		       attr_name);

	sprintf(buf_ptr, json_output ? "\"%s\":" : "%s: ",
		m0_xcode_enum_print(attr_name_xen,
				    attr_name, NULL));
	buf_ptr += strlen(buf);

	/* Check whether we can log attr value as a string. */
	for (i = 0; i < ARRAY_SIZE(vnmap); i++)
		if (attr_name_xen == vnmap[i].attr_name_xen) {
			vn = &vnmap[i];
			break;
		}
	for (i = 0; vn && i < vn->name_val_nr; i++)
		if (attr_name == vn->name_val[i].attr_name) {
			attr_val_xen = vn->name_val[i].val_xen;
			break;
		}

	/*
	 * A very specific case for Clovis opcode which is presented as
	 * several enums.
	 */
	if (attr_name == M0_AVI_CLOVIS_OP_ATTR_CODE) {
		M0_ASSERT(attr_val_xen == NULL);
		for (i = 0; i < ARRAY_SIZE(clovis_opcode_xc); ++i) {
			if (attr_val >= clovis_opcode_xc[i].start &&
			    attr_val < clovis_opcode_xc[i].end) {
				attr_val_xen = clovis_opcode_xc[i].xen;
				break;
			}
		}
	}

	if (attr_val_xen)
		sprintf(buf_ptr, json_output ? "\"%s\"" : "%s",
			m0_xcode_enum_print(attr_val_xen, attr_val, NULL));
	else
		sprintf(buf_ptr, "%"PRId64, attr_val);
}

#define COUNTER  &counter, &skip, &skip, &skip, &skip, &skip, &skip
#define FID &fid, &skip
#define TIMED &duration, &sym
#define HIST &hist, &skip, &skip, &skip, &skip, &skip, &skip, &skip, &skip, \
		&skip, &skip, &skip, &skip, &skip, &skip
#define SKIP2 &skip, &skip

struct m0_addb2__id_intrp ids[] = {
	{ M0_AVI_NULL,            "null" },
	{ M0_AVI_NODE,            "node",            { FID } },
	{ M0_AVI_LOCALITY,        "locality",        { &dec } },
	{ M0_AVI_PID,             "pid",             { &dec } },
	{ M0_AVI_THREAD,          "thread",          { &hex, &hex } },
	{ M0_AVI_SERVICE,         "service",         { FID } },
	{ M0_AVI_FOM,             "fom",             { &ptr, &fom_type,
						       &skip, &skip },
						     { "addr" } },
	{ M0_AVI_CLOCK,           "clock",           { } },
	{ M0_AVI_PHASE,           "fom-phase",       { &fom_phase, SKIP2 } },
	{ M0_AVI_STATE,           "fom-state",       { &fom_state, SKIP2 } },
	{ M0_AVI_STATE_COUNTER,   "",
	  .ii_repeat = M0_AVI_STATE_COUNTER_END - M0_AVI_STATE_COUNTER,
	  .ii_spec   = &fom_state_counter },
	{ M0_AVI_ALLOC,           "alloc",           { &dec, &ptr },
	  { "size", "addr" } },
	{ M0_AVI_FOM_DESCR,       "fom-descr",       { FID, &hex0x, &rpcop,
						       &rpcop, &bol, &dec,
						       &dec, &dec },
	  { "service", NULL, "sender",
	    "req-opcode", "rep-opcode", "local", "rpc_sm_id",
	    "fom_sm_id", "fom_state_sm_id" } },
	{ M0_AVI_FOM_ACTIVE,      "fom-active",      { HIST } },
	{ M0_AVI_RUNQ,            "runq",            { HIST } },
	{ M0_AVI_WAIL,            "wail",            { HIST } },
	{ M0_AVI_AST,             "ast" },
	{ M0_AVI_LOCALITY_FORQ_DURATION, "loc-forq-duration", { TIMED },
	  { "duration" } },
	{ M0_AVI_LOCALITY_FORQ,      "loc-forq-hist",  { HIST } },
	{ M0_AVI_LOCALITY_CHAN_WAIT, "loc-wait-hist",  { HIST } },
	{ M0_AVI_LOCALITY_CHAN_CB,   "loc-cb-hist",    { HIST } },
	{ M0_AVI_LOCALITY_CHAN_QUEUE,"loc-queue-hist", { HIST } },
	{ M0_AVI_IOS_IO_DESCR,    "ios-io-descr",    { FID, FID,
						       &hex, &hex, &dec, &dec,
						       &dec, &dec, &dec },
	  { "file", NULL, "cob", NULL,
	    "seg-nr", "count", "offset", "descr-nr", "colour" }},
	{ M0_AVI_FS_OPEN,         "m0t1fs-open",     { FID, &oct },
	  { NULL, NULL, "flags" }},
	{ M0_AVI_FS_LOOKUP,       "m0t1fs-lookup",   { FID } },
	{ M0_AVI_FS_CREATE,       "m0t1fs-create",   { FID, &oct, &dec },
	  { NULL, NULL, "mode", "rc" } },
	{ M0_AVI_FS_READ,         "m0t1fs-read",     { FID } },
	{ M0_AVI_FS_WRITE,        "m0t1fs-write",    { FID } },
	{ M0_AVI_FS_IO_DESCR,     "m0t1fs-io-descr", { &dec, &dec },
	  { "offset", "rc" }},
	{ M0_AVI_FS_IO_MAP,       "m0t1fs-io-map",     { &dec, FID, &dec,
							 &dec, &dec,&dec,
							 &dec,&dec },
	  { "req_state", NULL, NULL, "unit_type", "device_state",
	    "frame", "target", "group", "unit" }},
	{ M0_AVI_STOB_IO_LAUNCH,  "stob-io-launch",  { FID, &dec,
						       &dec, &dec, &dec, &dec },
	  { "fid", NULL, "count", "bvec-nr", "ivec-nr", "offset" } },
	{ M0_AVI_STOB_IO_END,     "stob-io-end",     { FID, &duration,
						       &dec, &dec, &dec },
	  { "fid", NULL, "duration", "rc", "count", "frag" } },
	{ M0_AVI_STOB_IOQ,        "stob-ioq-thread", { &dec } },
	{ M0_AVI_STOB_IOQ_INFLIGHT, "stob-ioq-inflight", { HIST } },
	{ M0_AVI_STOB_IOQ_QUEUED, "stob-ioq-queued", { HIST } },
	{ M0_AVI_STOB_IOQ_GOT,    "stob-ioq-got",    { HIST } },

	{ M0_AVI_RPC_LOCK,        "rpc-machine-lock", { &ptr } },
	{ M0_AVI_RPC_REPLIED,     "rpc-replied",      { &ptr, &rpcop } },
	{ M0_AVI_RPC_OUT_PHASE,   "rpc-out-phase",    { &rpc_out, SKIP2 } },
	{ M0_AVI_RPC_IN_PHASE,    "rpc-in-phase",    { &rpc_in, SKIP2 } },
	{ M0_AVI_RPC_ITEM_ID_ASSIGN, "rpc-item-id-assign",
	  { &dec, &dec, &dec, &dec }, { "id", "opcode", "xid", "session_id" } },
	{ M0_AVI_RPC_ITEM_ID_FETCH, "rpc-item-id-fetch",
	  { &dec, &dec, &dec, &dec }, { "id", "opcode", "xid", "session_id" } },
	{ M0_AVI_BE_TX_STATE,     "tx-state",        { &tx_state, SKIP2  } },
	{ M0_AVI_BE_TX_COUNTER,   "",
	  .ii_repeat = M0_AVI_BE_TX_COUNTER_END - M0_AVI_BE_TX_COUNTER,
	  .ii_spec   = &tx_state_counter },
	{ M0_AVI_BE_OP_COUNTER,   "",
	  .ii_repeat = M0_AVI_BE_OP_COUNTER_END - M0_AVI_BE_OP_COUNTER,
	  .ii_spec   = &beop_state_counter },
	{ M0_AVI_BE_TX_TO_GROUP,  "tx-to-gr", { &dec, &dec, &dec },
	  { "tx_id", "gr_id", "inout" } },
	{ M0_AVI_NET_BUF,         "net-buf",         { &ptr, &dec, &_clock,
						       &duration, &dec, &dec },
	  { "buf", "qtype", "time", "duration", "status", "len" } },
	{ M0_AVI_FOP_TYPES_RANGE_START,   "",
	  .ii_repeat = M0_AVI_FOP_TYPES_RANGE_END-M0_AVI_FOP_TYPES_RANGE_START,
	  .ii_spec   = &fop_counter },
	{ M0_AVI_SIT,             "sit",  { &hex, &hex, &hex, &hex, &hex,
					    &dec, &dec, FID },
	  { "seq", "offset", "prev", "next", "size", "idx", "nr", "fid" } },
	{ M0_AVI_LONG_LOCK,       "long-lock", { &ptr, &duration, &duration },
	  { "fom", "wait", "hold"} },
	{ M0_AVI_CAS_KV_SIZES,    "cas-kv-sizes",  { FID, &dec, &dec },
	  { "ifid", NULL, "ksize", "vsize"} },

	/* clovis -> md|io-path */
	{ M0_AVI_CLOVIS_SM_OP,         "clovis-op-state", { &clovis_op_state, SKIP2 } },
	{ M0_AVI_CLOVIS_SM_OP_COUNTER, "",
	  .ii_repeat = M0_AVI_CLOVIS_SM_OP_COUNTER_END - M0_AVI_CLOVIS_SM_OP_COUNTER,
	  .ii_spec   = &clovis_state_counter },

	/* md path dumplings */
	{ M0_AVI_DIX_SM_REQ,         "dix-req-state", { &dix_op_state, SKIP2 } },
	{ M0_AVI_DIX_SM_REQ_COUNTER, "",
	  .ii_repeat = M0_AVI_DIX_SM_REQ_COUNTER_END - M0_AVI_DIX_SM_REQ_COUNTER,
	  .ii_spec   = &dix_state_counter },

	{ M0_AVI_CAS_SM_REQ,         "cas-req-state", { &cas_op_state, SKIP2 } },
	{ M0_AVI_CAS_SM_REQ_COUNTER, "",
	  .ii_repeat = M0_AVI_CAS_SM_REQ_COUNTER_END - M0_AVI_CAS_SM_REQ_COUNTER,
	  .ii_spec   = &cas_state_counter },

	{ M0_AVI_CLOVIS_TO_DIX,      "clovis-to-dix", { &dec, &dec },
	  { "clovis_id", "dix_id" } },
	{ M0_AVI_DIX_TO_MDIX,      "dix-to-mdix", { &dec, &dec },
	  { "dix_id", "mdix_id" } },
	{ M0_AVI_DIX_TO_CAS,      "dix-to-cas", { &dec, &dec },
	  { "dix_id", "cas_id" } },
	{ M0_AVI_CAS_TO_RPC,      "cas-to-rpc", { &dec, &dec },
	  { "cas_id", "rpc_id" } },
	{ M0_AVI_FOM_TO_TX,      "fom-to-tx", { &dec, &dec },
	  { "fom_id", "tx_id" } },
	{ M0_AVI_FOM_TO_STIO, "fom-to-stio", { &dec, &dec },
	  {"fom_id", "stio_id" } },
	{ M0_AVI_CAS_FOM_TO_CROW_FOM,   "cas-fom-to-crow-fom",    { &dec, &dec },
	  { "fom_id", "crow_fom_id" } },

	/* io path dumplings */
	{ M0_AVI_CLOVIS_COB_REQ,      "cob-req-state", { &dec, &cob_req_state },
	  { "cob_id", "cob_state" } },
	{ M0_AVI_CLOVIS_TO_COB_REQ,   "clovis-to-cob", { &dec, &dec },
	  { "clovis_id", "cob_id" } },
	{ M0_AVI_CLOVIS_COB_REQ_TO_RPC, "cob-to-rpc",  { &dec, &dec },
	  { "cob_id", "rpc_id" } },
	{ M0_AVI_CLOVIS_TO_IOO,       "clovis-to-ioo", { &dec, &dec },
	  { "clovis_id", "ioo_id" } },
	{ M0_AVI_CLOVIS_IOO_TO_RPC,   "ioo-to-rpc",    { &dec, &dec },
	  { "ioo_id", "rpc_id" } },

	{ M0_AVI_CLOVIS_IOO_REQ,         "ioo-req-state", { &ioo_req_state, SKIP2 } },
	{ M0_AVI_CLOVIS_IOO_REQ_COUNTER, "",
	  .ii_repeat = M0_AVI_CLOVIS_IOO_REQ_COUNTER_END - M0_AVI_CLOVIS_IOO_REQ_COUNTER,
	  .ii_spec   = &ioo_state_counter },
	{ M0_AVI_STOB_IO_REQ,    "stio-req-state", { &dec, &stob_io_req_state},
	  { "stio_id", "stio_state" } },

	{ M0_AVI_KEM_CPU, "kem-cpu", { &dec },
	  { "cpu" } },
	{ M0_AVI_KEM_PAGE_FAULT, "pagefault", { &dec, &dec, &hex,
						&dec, &hex, &hex },
	  { "tgid", "pid", "addr", "wr", "fault", "delta_t" } },
	{ M0_AVI_KEM_CONTEXT_SWITCH, "ctx_switch", { &dec, &dec,
						     &dec, &dec, &hex },
	  { "prev_tgid", "prev_pid", "next_tgid", "next_pid", "delta_t" } },
	{ M0_AVI_CLOVIS_BULK_TO_RPC,   "bulk-to-rpc",    { &dec, &dec },
	  { "bulk_id", "rpc_id" } },
	{ M0_AVI_FOM_TO_BULK,   "fom-to-bulk",    { &dec, &dec },
	  { "fom_sm_id", "bulk_id" } },
	{ M0_AVI_RPC_BULK_OP, "rpc-bulk-op", { &dec, &bulk_op_state },
	  { "bulk_id", "state" } },
	{ M0_AVI_ATTR, "attr", { &dec, &attr, &skip },
	  { "entity_id", NULL, NULL } },
	{ M0_AVI_NODATA,          "nodata" },
};

static void id_init(void)
{
	int                             result;
	int                             i;
	const struct m0_addb2__id_intrp z_intrp = {};

	result = m0_varr_init(&value_id, M0_AVI_LAST, sizeof(char *), 4096);
	if (result != 0)
		err(EX_CONFIG, "Cannot initialise array: %d", result);
	id_set_nr(ids, ARRAY_SIZE(ids));

	for (i = 0; i < plugins_nr; ++i) {
		struct plugin *p = &plugins[i];
		uint32_t       e_id_nr = 0;

		/* Calculate array size without last termination item */
		for(; !intrps_equal(&p->p_intrp[e_id_nr], &z_intrp); e_id_nr++);

		id_set_nr(p->p_intrp, e_id_nr);
	}
}

static void id_fini(void)
{
	m0_varr_fini(&value_id);
}

static void id_set(struct m0_addb2__id_intrp *intrp)
{
	struct m0_addb2__id_intrp **addr;

	if (intrp->ii_id < m0_varr_size(&value_id)) {
		addr = m0_varr_ele_get(&value_id, intrp->ii_id);
		M0_ASSERT(addr != NULL);
		M0_ASSERT(*addr == NULL);
		*addr = intrp;
	}
}

static void id_set_nr(struct m0_addb2__id_intrp *batch, int nr)
{
	while (nr-- > 0) {
		struct m0_addb2__id_intrp *intrp = &batch[nr];
		int                        i;

		id_set(intrp);
		for (i = 0; i < intrp->ii_repeat; ++i) {
			++(intrp->ii_id);
			id_set(intrp);
		}
	}
}

static struct m0_addb2__id_intrp *id_get(uint64_t id)
{
	struct m0_addb2__id_intrp **addr;
	struct m0_addb2__id_intrp  *intr = NULL;

	if (id < m0_varr_size(&value_id)) {
		addr = m0_varr_ele_get(&value_id, id);
		if (addr != NULL)
			intr = *addr;
	}
	return intr;
}

#define U64 "%16"PRIx64

static void rec_dump(struct m0_addb2__context *ctx,
                     const struct m0_addb2_record *rec)
{
	int i;

	ctx->c_rec = rec;
	context_fill(ctx, &rec->ar_val);
	for (i = 0; i < rec->ar_label_nr; ++i)
		context_fill(ctx, &rec->ar_label[i]);
	if (json_output)
		printf("{");
	val_dump(ctx, "* ", &rec->ar_val, 0, !flatten);
	if (json_output && rec->ar_label_nr > 0)
		printf(",");
	for (i = 0; i < rec->ar_label_nr; ++i) {
		val_dump(ctx, "| ", &rec->ar_label[i], 8, !flatten);
		if (json_output && i < rec->ar_label_nr - 1)
			printf(",");
	}
	if (json_output) {
		if (json_extra_data != NULL)
			printf(",%s}\n", json_extra_data);
		else
			puts("}");
	} else if (flatten) {
		puts("");
	}
}

static int pad(int indent)
{
	return indent > 0 ? printf("%*.*s", indent, indent,
		   "                                                    ") : 0;
}

static unsigned int count_nonempty_vals(const struct m0_addb2_value *val)
{
	struct m0_addb2__id_intrp *intrp = id_get(val->va_id);
	int i, c;

	for (i = 0, c = 0; intrp != NULL && i < val->va_nr; ++i)
		if (intrp->ii_print[i] != NULL && intrp->ii_print[i] != &skip)
			c++;
	return c;
}

static void val_dump_json(struct m0_addb2__context *ctx,
		     const struct m0_addb2_value *val, bool output_timestamp)
{
	struct m0_addb2__id_intrp *intrp = id_get(val->va_id);
	int                        i;
	char                       buf[BUF_SIZE];
	bool			   need_braces = false;
	bool			   need_comma;

#define BEND (buf + strlen(buf))

	ctx->c_val = val;
	if (output_timestamp && val->va_time != 0) {
		_clock(ctx, &val->va_time, buf);
		printf("\"timestamp\":%s,", buf);
	}
	if (intrp != NULL && intrp->ii_spec != NULL) {
		intrp->ii_spec(ctx, buf);
		// FIXME: rename "spec" to something meaningful
		printf("\"spec\":%s", buf);
		return;
	}
	if (intrp != NULL) {
		need_braces = count_nonempty_vals(val) > 1;
		printf("\"%s\":%s", intrp->ii_name, need_braces ? "{" : "");
		 /* boolean attributes (flags) */
		if (val->va_nr == 0)
			printf("true");
		else if (intrp->ii_print != NULL &&
			 intrp->ii_print[0] == &hist)
			printf("true,");
	}
	else {
		printf("\"m0addb2dump[%s:%u]:%"PRIu64"\"",
			__FILE__, __LINE__, val->va_id);
	}
	for (i = 0; i < val->va_nr; ++i) {
		buf[0] = 0;
		if (intrp == NULL)
			sprintf(buf, "\"m0addb2dump[%s:%u]:%"PRIu64"\"",
				__FILE__, __LINE__, val->va_data[i]);
		else {
			if (intrp->ii_field[i] != NULL)
				sprintf(buf, "\"%s\":", intrp->ii_field[i]);

			need_comma = (i < val->va_nr - 2 &&
				      intrp->ii_print[i] == &fid) ||
				     (i < val->va_nr - 1 &&
				      (intrp->ii_field[i + 1] != NULL ||
				       intrp->ii_print[i + 1] == &attr));

			if (intrp->ii_print[i] == NULL)
				sprintf(BEND, "%"PRId64"%s", val->va_data[i],
					need_comma ? "," : "");
			else {
				if (intrp->ii_print[i] == &skip)
					continue;
				intrp->ii_print[i](ctx, &val->va_data[i], BEND);
				if (intrp->ii_print[i] == &ptr ||
				    intrp->ii_print[i] == &duration)
					need_comma = i < val->va_nr - 1;
				printf("%s%s", buf, need_comma ? "," : "");
			}
		}
	}
	if (need_braces)
		printf("}");
#undef BEND
}

static void val_dump_plaintext(struct m0_addb2__context *ctx, const char *prefix,
			       const struct m0_addb2_value *val, int indent, bool cr)
{
	struct m0_addb2__id_intrp *intrp = id_get(val->va_id);
	int                        i;
	char                       buf[BUF_SIZE];
	enum { WIDTH = 12 };

#define BEND (buf + strlen(buf))

	ctx->c_val = val;
	printf("%s", prefix);
	pad(indent);
	if (indent == 0 && val->va_time != 0) {
		_clock(ctx, &val->va_time, buf);
		printf("%s ", buf);
	}
	if (intrp != NULL && intrp->ii_spec != NULL) {
		intrp->ii_spec(ctx, buf);
		printf("%s%s", buf, cr ? "\n" : " ");
		return;
	}
	if (intrp != NULL)
		printf("%-16s ", intrp->ii_name);
	else
		printf(U64" ", val->va_id);
	for (i = 0, indent = 0; i < val->va_nr; ++i) {
		buf[0] = 0;
		if (intrp == NULL)
			sprintf(buf, "?"U64"?", val->va_data[i]);
		else {
			if (intrp->ii_field[i] != NULL)
				sprintf(buf, "%s: ", intrp->ii_field[i]);
			if (intrp->ii_print[i] == NULL)
				sprintf(BEND, "?"U64"?", val->va_data[i]);
			else {
				if (intrp->ii_print[i] == &skip)
					continue;
				intrp->ii_print[i](ctx, &val->va_data[i], BEND);
			}
		}
		if (i > 0)
			indent += printf(", ");
		indent += pad(WIDTH * i - indent);
		indent += printf("%s", buf);
	}
	printf("%s", cr ? "\n" : " ");
#undef BEND
}

static void val_dump(struct m0_addb2__context *ctx, const char *prefix,
		     const struct m0_addb2_value *val, int indent, bool cr)
{
	if (json_output)
		val_dump_json(ctx, val, indent == 0 /* print timestamp? */);
	else
		val_dump_plaintext(ctx, prefix, val, indent, cr);
}

extern struct m0_fom_type *m0_fom__types[M0_OPCODES_NR];
static void context_fill(struct m0_addb2__context *ctx,
                         const struct m0_addb2_value *val)
{
	switch (val->va_id) {
	case M0_AVI_FOM:
		ctx->c_fom.fo_addr = val->va_data[0];
		ctx->c_fom.fo_tid  = val->va_data[1];
		M0_ASSERT(val->va_data[1] < ARRAY_SIZE(m0_fom__types));
		ctx->c_fom.fo_type = m0_fom__types[val->va_data[1]];
		break;
	}
}

static bfd      *abfd;
static asymbol **syms;
static uint64_t  base;
static size_t    nr;

static int asymbol_cmp(const void *a0, const void *a1)
{
	const asymbol *const* s0 = a0;
	const asymbol *const* s1 = a1;

	return ((int)bfd_asymbol_value(*s0)) - ((int)bfd_asymbol_value(*s1));
}

/**
 * Initialises bfd.
 */
static void libbfd_init(const char *libpath)
{
	unsigned symtab_size;
	size_t   i;

	bfd_init();
	abfd = bfd_openr(libpath, 0);
	if (abfd == NULL)
		err(EX_OSERR, "bfd_openr(): %d.", errno);
	bfd_check_format(abfd, bfd_object); /* cargo-cult call. */
	symtab_size = bfd_get_symtab_upper_bound(abfd);
	syms = (asymbol **) m0_alloc(symtab_size);
	if (syms == NULL)
		err(EX_UNAVAILABLE, "Cannot allocate symtab.");
	nr = bfd_canonicalize_symtab(abfd, syms);
	for (i = 0; i < nr; ++i) {
		if (strcmp(syms[i]->name, "m0_ptr_wrap") == 0) {
			base = bfd_asymbol_value(syms[i]);
			break;
		}
	}
	if (base == 0)
		err(EX_CONFIG, "No base symbol found.");
	qsort(syms, nr, sizeof syms[0], &asymbol_cmp);
}

static void libbfd_fini(void)
{
	m0_free(syms);
}

static void libbfd_resolve(uint64_t delta, char *buf)
{
	static uint64_t    cached = 0;
	static const char *name   = NULL;

	if (abfd == NULL)
		;
	else if (delta == cached)
		sprintf(buf, " %s", name);
	else {
		size_t mid;
		size_t left = 0;
		size_t right = nr;

		cached = delta;
		delta += base;
		while (left + 1 < right) {
			asymbol *sym;

			mid = (left + right) / 2;
			sym = syms[mid];

			if (bfd_asymbol_value(sym) > delta)
				right = mid;
			else if (bfd_asymbol_value(sym) < delta)
				left = mid;
			else {
				left = mid;
				break;
			}
		}
		name = syms[left]->name;
		sprintf(buf, " %s", name);
	}
}

static void deflate(void)
{
	int ch;

	while ((ch = getchar()) != EOF) {
		if (ch == '|')
			putchar('\n');
		putchar(ch);
	}
}

static void flate(void)
{
	int ch;
	int prev;

	for (prev = 0; (ch = getchar()) != EOF; prev = ch) {
		if (prev == '\n' && ch == '|')
			prev = ' ';
		if (prev != 0)
			putchar(prev);
	}
	if (prev != 0)
		putchar(prev);
}

static void misc_init(void)
{
	m0_sns_cm_repair_trigger_fop_init();
	m0_sns_cm_rebalance_trigger_fop_init();
	m0_sns_cm_repair_sw_onwire_fop_init();
	m0_sns_cm_rebalance_sw_onwire_fop_init();
	m0_dix_cm_repair_cpx_init();
	m0_dix_cm_rebalance_cpx_init();
}

static void misc_fini(void)
{
	m0_dix_cm_rebalance_cpx_fini();
	m0_dix_cm_repair_cpx_fini();
	m0_sns_cm_repair_trigger_fop_fini();
	m0_sns_cm_rebalance_trigger_fop_fini();
	m0_sns_cm_repair_sw_onwire_fop_fini();
	m0_sns_cm_rebalance_sw_onwire_fop_fini();
}

/** @} end of addb2 group */

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
