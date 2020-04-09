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
 * Original creation date: 29-Jan-2015
 */


/**
 * @addtogroup addb2 ADDB.2
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ADDB

#include "lib/assert.h"
#include "lib/misc.h"                  /* M0_IS0 */
#include "mero/magic.h"

#include "addb2/consumer.h"
#include "addb2/addb2.h"
#include "addb2/internal.h"

M0_TL_DESCR_DEFINE(philter, "addb2 source philters",
		   static, struct m0_addb2_philter, ph_linkage, ph_magix,
		   M0_ADDB2_PHILTER_MAGIC, M0_ADDB2_PHILTER_HEAD_MAGIC);
M0_TL_DEFINE(philter, static, struct m0_addb2_philter);

M0_TL_DESCR_DEFINE(callback, "addb2 philter callbacks",
		   static, struct m0_addb2_callback, ca_linkage, ca_magix,
		   M0_ADDB2_CALLBACK_MAGIC, M0_ADDB2_CALLBACK_HEAD_MAGIC);
M0_TL_DEFINE(callback, static, struct m0_addb2_callback);

static bool true_matches(struct m0_addb2_philter *philter,
			 const struct m0_addb2_record *rec);
static bool id_matches(struct m0_addb2_philter *philter,
		       const struct m0_addb2_record *rec);

void m0_addb2_source_init(struct m0_addb2_source *src)
{
	philter_tlist_init(&src->so_philter);
}

void m0_addb2_source_fini(struct m0_addb2_source *src)
{
	while (philter_tlist_pop(&src->so_philter) != NULL)
		;
	philter_tlist_fini(&src->so_philter);
}

void m0_addb2_philter_init(struct m0_addb2_philter *philter,
			   bool (*matches)(struct m0_addb2_philter *,
					   const struct m0_addb2_record *),
			   void *datum)
{
	philter->ph_matches = matches;
	philter->ph_datum   = datum;
	philter_tlink_init(philter);
	callback_tlist_init(&philter->ph_callback);
}

void m0_addb2_philter_fini(struct m0_addb2_philter *philter)
{
	while (callback_tlist_pop(&philter->ph_callback) != NULL)
		;
	callback_tlist_fini(&philter->ph_callback);
	philter_tlink_fini(philter);
}

void m0_addb2_philter_add(struct m0_addb2_source *src,
			  struct m0_addb2_philter *ph)
{
	philter_tlink_init_at_tail(ph, &src->so_philter);
}

void m0_addb2_philter_del(struct m0_addb2_philter *ph)
{
	philter_tlink_del_fini(ph);
}

void m0_addb2_callback_init(struct m0_addb2_callback *callback,
			    void (*fire)(const struct m0_addb2_source   *,
					 const struct m0_addb2_philter  *,
					 const struct m0_addb2_callback *,
					 const struct m0_addb2_record   *),
			    void *datum)
{
	callback->ca_fire  = fire;
	callback->ca_datum = datum;
	callback_tlink_init(callback);
}

void m0_addb2_callback_fini(struct m0_addb2_callback *callback)
{
	callback_tlink_fini(callback);
}

void m0_addb2_callback_add(struct m0_addb2_philter *ph,
			   struct m0_addb2_callback *callback)
{
	callback_tlink_init_at_tail(callback, &ph->ph_callback);
}

void m0_addb2_callback_del(struct m0_addb2_callback *callback)
{
	callback_tlink_del_fini(callback);
}

struct m0_addb2_source *m0_addb2_cursor_source(struct m0_addb2_cursor *c)
{
	return &c->cu_src;
}

static void philter_consume(struct m0_addb2_source *src,
			    struct m0_addb2_philter *ph,
			    const struct m0_addb2_record *rec)
{
	struct m0_addb2_callback *callback;

	if (ph->ph_matches(ph, rec)) {
		m0_tl_for(callback, &ph->ph_callback, callback) {
			callback->ca_fire(src, ph, callback, rec);
		} m0_tl_endfor;
	}
}

void m0_addb2_consume(struct m0_addb2_source *src,
		      const struct m0_addb2_record *rec)
{
	struct m0_addb2_philter  *ph;
	struct m0_addb2_module   *am = m0_addb2_module_get();
	int                       i;

	m0_tl_for(philter, &src->so_philter, ph) {
		philter_consume(src, ph, rec);
	} m0_tl_endfor;

	for (i = 0; i < ARRAY_SIZE(am->am_philter); ++i) {
		if (am->am_philter[i] != NULL)
			philter_consume(src, am->am_philter[i], rec);
	}
}

void m0_addb2_philter_true_init(struct m0_addb2_philter *ph)
{
	m0_addb2_philter_init(ph, true_matches, NULL);
}

void m0_addb2_philter_id_init(struct m0_addb2_philter *ph, uint64_t id)
{
	m0_addb2_philter_init(ph, id_matches, (void *)id);
}

static bool true_matches(struct m0_addb2_philter *philter,
			 const struct m0_addb2_record *rec)
{
	return true;
}

static bool id_matches(struct m0_addb2_philter *philter,
		       const struct m0_addb2_record *rec)
{
	return rec->ar_val.va_id == (uint64_t)philter->ph_datum;
}

void m0_addb2_philter_global_add(struct m0_addb2_philter *ph)
{
	struct m0_addb2_module *am = m0_addb2_module_get();
	int                     i;

	for (i = 0; i < ARRAY_SIZE(am->am_philter); ++i) {
		if (am->am_philter[i] == NULL) {
			am->am_philter[i] = ph;
			return;
		}
	}
	M0_IMPOSSIBLE("Too many global philters.");
}

void m0_addb2_philter_global_del(struct m0_addb2_philter *ph)
{
	struct m0_addb2_module *am = m0_addb2_module_get();
	int                     i;

	for (i = 0; i < ARRAY_SIZE(am->am_philter); ++i) {
		if (am->am_philter[i] == ph) {
			am->am_philter[i] = NULL;
			return;
		}
	}
	M0_IMPOSSIBLE("Unknown global philter.");
}

#undef M0_TRACE_SUBSYSTEM

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
