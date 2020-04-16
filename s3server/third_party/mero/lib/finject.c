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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 02/22/2012
 */

#ifdef ENABLE_FAULT_INJECTION

#ifdef __KERNEL__
#include <linux/kernel.h>  /* snprintf */
#else
#include <stdio.h>         /* snprintf */
#endif

#include "lib/errno.h"     /* ENOMEM */
#include "lib/memory.h"    /* M0_ALLOC_ARR */
#include "lib/mutex.h"     /* m0_mutex */
#include "lib/misc.h"      /* <linux/string.h> <string.h> for strcmp */
#include "lib/assert.h"    /* M0_ASSERT */
#include "lib/tlist.h"
#include "lib/finject.h"
#include "lib/finject_internal.h"
#include "mero/magic.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

enum {
	FI_STATES_ARRAY_SIZE = 64 * 1024,
};

struct m0_fi_fpoint_state fi_states[FI_STATES_ARRAY_SIZE];
uint32_t                  fi_states_free_idx;
struct m0_mutex           fi_states_mutex;

struct fi_dynamic_id {
	struct m0_tlink  fdi_tlink;
	uint64_t         fdi_magic;
	char            *fdi_str;
};

M0_TL_DESCR_DEFINE(fi_dynamic_ids, "finject_dynamic_id", static,
		   struct fi_dynamic_id, fdi_tlink, fdi_magic,
		   M0_FI_DYNAMIC_ID_MAGIC, M0_FI_DYNAMIC_ID_HEAD_MAGIC);
M0_TL_DEFINE(fi_dynamic_ids, static, struct fi_dynamic_id);

/**
 * A storage for fault point ID strings, which are allocated dynamically in
 * runtime.
 *
 * Almost always ID string is a C string-literal with a
 * static storage duration. But in some rare cases ID strings need to be
 * allocated dynamically (for example when enabling FP via debugfs). To prevent
 * memleaks in such cases, all dynamically allocated ID strings are stored in
 * this linked list, using m0_fi_add_dyn_id(), which is cleaned in
 * fi_states_fini().
 */
static struct m0_tl fi_dynamic_ids;

/* keep these long strings on a single line for easier editing */
const char *m0_fi_states_headline[] = {
" Idx | Enb |TotHits|TotTrig|Hits|Trig|   Type   |   Data   | Module |              File name                 | Line |             Func name             |   Tag\n",
"-----+-----+-------+-------+----+----+----------+----------+--------+----------------------------------------+------+-----------------------------------+----------\n",
};
M0_EXPORTED(m0_fi_states_headline);

const char m0_fi_states_print_format[] =
" %-3u    %c   %-7u %-7u %-4u %-4u %-10s %-10s %-8s %-40s  %-4u  %-35s  %s\n";
M0_EXPORTED(m0_fi_states_print_format);


M0_INTERNAL const struct m0_fi_fpoint_state *m0_fi_states_get(void)
{
	return fi_states;
}
M0_EXPORTED(m0_fi_states_get);

M0_INTERNAL uint32_t m0_fi_states_get_free_idx(void)
{
	return fi_states_free_idx;
}
M0_EXPORTED(m0_fi_states_get_free_idx);

static inline uint32_t fi_state_idx(const struct m0_fi_fpoint_state *s)
{
	return s - fi_states;
}

static void fi_state_info_init(struct m0_fi_fpoint_state_info *si)
{
	si->si_idx               = 0;
	si->si_enb               = 'n';
	si->si_total_hit_cnt     = 0;
	si->si_total_trigger_cnt = 0;
	si->si_hit_cnt           = 0;
	si->si_trigger_cnt       = 0;
	si->si_type              = "";
	si->si_module            = "";
	si->si_file              = "";
	si->si_func              = "";
	si->si_tag               = "";
	si->si_line_num          = 0;

	M0_SET_ARR0(si->si_data);
}

M0_INTERNAL void m0_fi_states_get_state_info(const struct m0_fi_fpoint_state *s,
					     struct m0_fi_fpoint_state_info *si)
{
	const struct m0_fi_fault_point  *fp;

	fi_state_info_init(si);

	si->si_idx = fi_state_idx(s);
	si->si_func = s->fps_id.fpi_func;
	si->si_tag = s->fps_id.fpi_tag;
	si->si_total_hit_cnt = s->fps_total_hit_cnt;
	si->si_total_trigger_cnt = s->fps_total_trigger_cnt;
	fp = s->fps_fp;

	/*
	 * fp can be NULL if fault point was enabled but had not been registered
	 * yet
	 */
	if (fp != NULL) {
		si->si_module = fp->fp_module;
		si->si_file = m0_short_file_name(fp->fp_file);
		si->si_line_num = fp->fp_line_num;
	}

	if (fi_state_enabled(s)) {
		si->si_enb = 'y';
		si->si_type = m0_fi_fpoint_type_name(s->fps_data.fpd_type);
		switch (s->fps_data.fpd_type) {
		case M0_FI_OFF_N_ON_M:
			snprintf(si->si_data, sizeof si->si_data, "n=%u,m=%u",
					s->fps_data.u.s1.fpd_n,
					s->fps_data.u.s1.fpd_m);
			break;
		case M0_FI_RANDOM:
			snprintf(si->si_data, sizeof si->si_data, "p=%u",
					s->fps_data.u.fpd_p);
			break;
		default:
			break; /* leave data string empty */
		}
		si->si_hit_cnt = s->fps_data.fpd_hit_cnt;
		si->si_trigger_cnt = s->fps_data.fpd_trigger_cnt;
	}

	return;
}
M0_EXPORTED(m0_fi_states_get_state_info);

M0_INTERNAL int m0_fi_add_dyn_id(char *str)
{
	struct fi_dynamic_id *fdi;

	M0_ALLOC_PTR(fdi);
	if (fdi == NULL)
		return M0_ERR(-ENOMEM);

	m0_tlink_init(&fi_dynamic_ids_tl, &fdi->fdi_tlink);
	fdi->fdi_str = str;
	m0_tlist_add(&fi_dynamic_ids_tl, &fi_dynamic_ids, &fdi->fdi_tlink);

	return 0;
}
M0_EXPORTED(m0_fi_add_dyn_id);

static void fi_dynamic_ids_fini(void)
{
	struct fi_dynamic_id *entry;

	m0_tl_teardown(fi_dynamic_ids, &fi_dynamic_ids, entry) {
		m0_free(entry->fdi_str);
		m0_free(entry);
	}

	m0_tlist_fini(&fi_dynamic_ids_tl, &fi_dynamic_ids);
}

M0_INTERNAL void fi_states_init(void)
{
	m0_tlist_init(&fi_dynamic_ids_tl, &fi_dynamic_ids);
}

M0_INTERNAL void fi_states_fini(void)
{
	int i;

	for (i = 0; i < fi_states_free_idx; ++i)
		m0_mutex_fini(&fi_states[i].fps_mutex);

	fi_dynamic_ids_fini();
}

/**
 * Checks equality of two fault point ids.
 *
 * @param id1 Pointer to first ID
 * @param id2 Pointer to second ID
 *
 * @return    true, if provided IDs are equal
 * @return    false otherwise
 */
static inline bool fi_fpoint_id_eq(const struct m0_fi_fpoint_id *id1,
				   const struct m0_fi_fpoint_id *id2)
{
	return strcmp(id1->fpi_func, id2->fpi_func) == 0 &&
			strcmp(id1->fpi_tag, id2->fpi_tag) == 0;
}

/**
 * Searches for m0_fi_fpoint_state structure in global fi_states array by fault
 * point ID.
 *
 * @param fp_id Pointer to fault point ID
 *
 * @return      Pointer to m0_fi_fpoint_state object, which has fps_id equal
 *              to the provided fp_id, if any
 * @return      NULL, if no such state object exists
 */
static
struct m0_fi_fpoint_state *__fi_state_find(const struct m0_fi_fpoint_id *fp_id)
{
	int i;

	for (i = 0; i < fi_states_free_idx; ++i)
		if (fi_fpoint_id_eq(&fi_states[i].fps_id, fp_id))
			return &fi_states[i];

	return NULL;
}

/**
 * A wrapper around __fi_state_find(), which uses fi_states_mutex mutex to
 * prevent potential changes of fi_states array from other threads.
 *
 * @see __fi_state_find()
 */
static inline
struct m0_fi_fpoint_state *fi_state_find(struct m0_fi_fpoint_id *fp_id)
{
	struct m0_fi_fpoint_state *state;

	m0_mutex_lock(&fi_states_mutex);
	state = __fi_state_find(fp_id);
	m0_mutex_unlock(&fi_states_mutex);

	return state;
}

/**
 * "Allocates" and initializes state structure in fi_states array.
 */
static struct m0_fi_fpoint_state *fi_state_alloc(struct m0_fi_fpoint_id *id)
{
	struct m0_fi_fpoint_state *state;

	state = &fi_states[fi_states_free_idx];
	fi_states_free_idx++;
	M0_ASSERT(fi_states_free_idx < ARRAY_SIZE(fi_states));
	m0_mutex_init(&state->fps_mutex);
	state->fps_id = *id;

	return state;
}

/**
 * Triggering algorithm for M0_FI_ALWAYS type
 */
static bool fi_state_always(struct m0_fi_fpoint_state *fps)
{
	return true;
}

static void fi_disable_state(struct m0_fi_fpoint_state *fps);

/**
 * Triggering algorithm for M0_FI_ONESHOT type
 */
static bool fi_state_oneshot(struct m0_fi_fpoint_state *fps)
{
	fi_disable_state(fps);
	return true;
}

M0_INTERNAL uint32_t fi_random(void);

/**
 * Triggering algorithm for M0_FI_RANDOM type
 */
static bool fi_state_random(struct m0_fi_fpoint_state *fps)
{
	return fps->fps_data.u.fpd_p >= fi_random();
}

/**
 * Triggering algorithm for M0_FI_OFF_N_ON_M type
 */
static bool fi_state_off_n_on_m(struct m0_fi_fpoint_state *fps)
{
	struct m0_fi_fpoint_data *data = &fps->fps_data;
	bool enabled = false;

	m0_mutex_lock(&fps->fps_mutex);

	data->u.s1.fpd___n_cnt++;
	if (data->u.s1.fpd___n_cnt > data->u.s1.fpd_n) {
		enabled = true;
		data->u.s1.fpd___m_cnt++;
		if (data->u.s1.fpd___m_cnt >= data->u.s1.fpd_m) {
			data->u.s1.fpd___n_cnt = 0;
			data->u.s1.fpd___m_cnt = 0;
		}
	}

	m0_mutex_unlock(&fps->fps_mutex);

	return enabled;
}

/**
 * Triggering algorithm for M0_FI_FUNC type
 */
static bool fi_state_user_func(struct m0_fi_fpoint_state *fps)
{
	return fps->fps_data.u.s2.fpd_trigger_func(fps->fps_data.u.s2.fpd_private);
}

static const char *fi_type_names[M0_FI_TYPES_NR] = {
	[M0_FI_ALWAYS]       = "always",
	[M0_FI_ONESHOT]      = "oneshot",
	[M0_FI_RANDOM]       = "random",
	[M0_FI_OFF_N_ON_M]   = "off_n_on_m",
	[M0_FI_FUNC]         = "user_func",
	[M0_FI_INVALID_TYPE] = "",
};

M0_INTERNAL const char *m0_fi_fpoint_type_name(enum m0_fi_fpoint_type type)
{
	M0_PRE(IS_IN_ARRAY(type, fi_type_names));
	return fi_type_names[type];
}
M0_EXPORTED(m0_fi_fpoint_type_name);

M0_INTERNAL enum m0_fi_fpoint_type m0_fi_fpoint_type_from_str(const char
							      *type_name)
{
	int i;

	for (i = 0; i < M0_FI_TYPES_NR; i++)
		if (strcmp(fi_type_names[i], type_name) == 0)
			return i;

	return M0_FI_INVALID_TYPE;
}
M0_EXPORTED(m0_fi_fpoint_type_from_str);

static const fp_state_func_t fi_trigger_funcs[M0_FI_TYPES_NR] = {
	[M0_FI_ALWAYS]     = fi_state_always,
	[M0_FI_ONESHOT]    = fi_state_oneshot,
	[M0_FI_RANDOM]     = fi_state_random,
	[M0_FI_OFF_N_ON_M] = fi_state_off_n_on_m,
	[M0_FI_FUNC]       = fi_state_user_func,
};

/**
 * Helper function for m0_fi_enable_generic()
 */
static void fi_enable_state(struct m0_fi_fpoint_state *fp_state,
			    const struct m0_fi_fpoint_data *fp_data)
{
	M0_PRE(IS_IN_ARRAY(fp_data->fpd_type, fi_trigger_funcs));

	m0_mutex_lock(&fp_state->fps_mutex);

	if (fp_data != NULL)
		fp_state->fps_data = *fp_data;

	if (fp_state->fps_data.fpd_type == M0_FI_OFF_N_ON_M) {
		fp_state->fps_data.u.s1.fpd___n_cnt = 0;
		fp_state->fps_data.u.s1.fpd___m_cnt = 0;
	}

	fp_state->fps_trigger_func = fi_trigger_funcs[fp_data->fpd_type];

	m0_mutex_unlock(&fp_state->fps_mutex);
}

/**
 * Helper function for m0_fi_disable()
 */
static void fi_disable_state(struct m0_fi_fpoint_state *fps)
{
	static const struct m0_fi_fpoint_data zero_data;

	m0_mutex_lock(&fps->fps_mutex);

	fps->fps_trigger_func = NULL;
	fps->fps_data = zero_data;

	m0_mutex_unlock(&fps->fps_mutex);
}

void m0_fi_register(struct m0_fi_fault_point *fp)
{
	struct m0_fi_fpoint_state *state;
	struct m0_fi_fpoint_id    id = {
		.fpi_func = fp->fp_func,
		.fpi_tag  = fp->fp_tag,
	};

	m0_mutex_lock(&fi_states_mutex);

	state = __fi_state_find(&id);
	if (state == NULL)
		state = fi_state_alloc(&id);

	/* Link state and fault point structures to each other */
	state->fps_fp = fp;
	fp->fp_state = state;

	m0_mutex_unlock(&fi_states_mutex);
}
M0_EXPORTED(m0_fi_register);

bool m0_fi_enabled(struct m0_fi_fpoint_state *fps)
{
	bool enabled;

	enabled = fi_state_enabled(fps) ? fps->fps_trigger_func(fps) : false;
	if (enabled) {
		fps->fps_total_trigger_cnt++;
		fps->fps_data.fpd_trigger_cnt++;
	}
	if (fi_state_enabled(fps))
		fps->fps_data.fpd_hit_cnt++;
	fps->fps_total_hit_cnt++;

	return enabled;
}
M0_EXPORTED(m0_fi_enabled);

M0_INTERNAL void m0_fi_enable_generic(const char *fp_func, const char *fp_tag,
				      const struct m0_fi_fpoint_data *fp_data)
{
	struct m0_fi_fpoint_state *state;
	struct m0_fi_fpoint_id    id = {
		.fpi_func = fp_func,
		.fpi_tag  = fp_tag,
	};

	M0_PRE(fp_func != NULL && fp_tag != NULL);

	m0_mutex_lock(&fi_states_mutex);

	state = __fi_state_find(&id);
	if (state == NULL)
		state = fi_state_alloc(&id);

	fi_enable_state(state, fp_data);

	m0_mutex_unlock(&fi_states_mutex);
}
M0_EXPORTED(m0_fi_enable_generic);

M0_INTERNAL void m0_fi_disable(const char *fp_func, const char *fp_tag)
{
	struct m0_fi_fpoint_state *state;
	struct m0_fi_fpoint_id    id = {
		.fpi_func = fp_func,
		.fpi_tag  = fp_tag
	};

	state = fi_state_find(&id);
	M0_ASSERT(state != NULL);

	fi_disable_state(state);
}
M0_EXPORTED(m0_fi_disable);

#endif /* ENABLE_FAULT_INJECTION */

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
