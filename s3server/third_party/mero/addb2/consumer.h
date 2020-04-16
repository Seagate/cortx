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

#pragma once

#ifndef __MERO_ADDB2_CONSUMER_H__
#define __MERO_ADDB2_CONSUMER_H__

/**
 * @defgroup addb2 ADDB.2
 *
 * Addb2 CONSUMER interface
 * ------------------------
 *
 * CONSUMER interface is used to subscribe to a source of addb2 records
 * (m0_addb2_source) and to get notified when matching records are PRODUCED
 * in the source.
 *
 * There are two types of record sources:
 *
 *     - online consumers use a source embedded in every m0_addb2_mach
 *       (m0_addb2_mach_source()). When a PRODUCER adds a record to the machine,
 *       this record is immediately available through the machine source;
 *
 *     - offline consumers use a source embedded in every storage iterator
 *       (m0_addb2_sit_source()). Depending on the SYSTEM configuration,
 *       produced records end up in one or more stobs (possibly after
 *       transmission over network). In general there are no guarantees about
 *       the stob in which a particular addb2 record will end up. It is up to
 *       the CONSUMER to organise a map-reduce-like search for desirable
 *       records, if necessary.
 *
 * CONSUMER accesses the records by adding one or more "philters"
 * (m0_addb2_philter) to a source and adding call-backs (m0_addb2_callback) to a
 * philter. A philter has a predicate (m0_addb2_philter::ph_matches()) over
 * records. When a record matching the philter predicate appears in the source,
 * all call-backs associated with the philter are invoked
 * (m0_addb2_callback::ca_fire() is called).
 *
 * @{
 */

/* import */
#include "lib/types.h"
#include "lib/tlist.h"

struct m0_addb2_trace;
struct m0_addb2_mach;

/* export */
struct m0_addb2_record;
struct m0_addb2_cursor;
struct m0_addb2_source;
struct m0_addb2_philter;
struct m0_addb2_callback;

void m0_addb2_cursor_init(struct m0_addb2_cursor *cur,
			  const struct m0_addb2_trace *trace);
void m0_addb2_cursor_fini(struct m0_addb2_cursor *cur);
/**
 * Moves the trace cursor to the next record.
 *
 * Returns +ve when cursor was successfully moved to the next record. The record
 * is in m0_addb2_cursor::cu_rec.
 *
 * Returns 0 when the end of the trace has been reached.
 *
 * Returns -EPROTO when the trace is ill-formed.
 */
int  m0_addb2_cursor_next(struct m0_addb2_cursor *cur);

enum {
	/**
	 * Maximal number of labels in addb2 context.
	 */
	M0_ADDB2_LABEL_MAX = 64
};

/**
 * Structure of addb2 measurements and labels: an identifier, a time-stamp and a
 * payload.
 */
struct m0_addb2_value {
	uint64_t        va_id;
	uint64_t        va_time;
	unsigned        va_nr;
	const uint64_t *va_data;
};

/**
 * Addb2 record is a measurement and a set of labels.
 */
struct m0_addb2_record {
	/** Measurement. */
	struct m0_addb2_value ar_val;
	/** Number of labels in the context. */
	unsigned              ar_label_nr;
	/** Labels. */
	struct m0_addb2_value ar_label[M0_ADDB2_LABEL_MAX];
};

/**
 * Source from which records can be CONSUMED.
 */
struct m0_addb2_source {
	/**
	 * List of philters linked through m0_addb2_philter::ph_linkage.
	 */
	struct m0_tl so_philter;
};

void m0_addb2_source_init(struct m0_addb2_source *src);
void m0_addb2_source_fini(struct m0_addb2_source *src);

/**
 * A philter is a predicate and a list of call-backs, which can be attached to a
 * source.
 */
struct m0_addb2_philter {
	/** Predicate function. */
	bool           (*ph_matches)(struct m0_addb2_philter *ph,
				     const struct m0_addb2_record *rec);
	/** Opaque datum, to be used by the predicate. */
	void            *ph_datum;
	/** Linkage into m0_addb2_source::so_philter. */
	struct m0_tlink  ph_linkage;
	/** List of call-backs linked through m0_addb2_callback::ca_linkage. */
	struct m0_tl     ph_callback;
	uint64_t         ph_magix;
};

void m0_addb2_philter_init(struct m0_addb2_philter *philter,
			   bool (*matches)(struct m0_addb2_philter *,
					   const struct m0_addb2_record *),
			   void *datum);

void m0_addb2_philter_fini(struct m0_addb2_philter *philter);

/**
 * Adds a philter to a source.
 */
void m0_addb2_philter_add(struct m0_addb2_source *src,
			  struct m0_addb2_philter *ph);
/**
 * Removes a philter from the source.
 */
void m0_addb2_philter_del(struct m0_addb2_philter *ph);

/**
 * A call-back is attached to a philter and is invoked when a record in the
 * source macthes the philter predicate.
 */
struct m0_addb2_callback {
	/** Call-back function. */
	void           (*ca_fire)(const struct m0_addb2_source   *src,
				  const struct m0_addb2_philter  *ph,
				  const struct m0_addb2_callback *callback,
				  const struct m0_addb2_record   *rec);
	/** Opaque datum. */
	void            *ca_datum;
	/** Linkage into m0_addb2_philter::ph_callback. */
	struct m0_tlink  ca_linkage;
	uint64_t         ca_magix;
};

void m0_addb2_callback_init(struct m0_addb2_callback *callback,
			    void (*fire)(const struct m0_addb2_source   *,
					 const struct m0_addb2_philter  *,
					 const struct m0_addb2_callback *,
					 const struct m0_addb2_record   *),
			    void *datum);

void m0_addb2_callback_fini(struct m0_addb2_callback *callback);

void m0_addb2_callback_add(struct m0_addb2_philter *ph,
			   struct m0_addb2_callback *callback);
void m0_addb2_callback_del(struct m0_addb2_callback *callback);

/**
 * Cursor over trace.
 */
struct m0_addb2_cursor {
	const struct m0_addb2_trace *cu_trace;
	unsigned                     cu_pos;
	struct m0_addb2_source       cu_src;
	struct m0_addb2_record       cu_rec;
};

struct m0_addb2_source *m0_addb2_mach_source(struct m0_addb2_mach *m);
struct m0_addb2_source *m0_addb2_cursor_source(struct m0_addb2_cursor *c);

/**
 * Delivers a record to the CONSUMERS.
 *
 * Scans through the list of philters attached to the source. If a philter
 * matches, its call-backs are invoked.
 */
void m0_addb2_consume(struct m0_addb2_source *src,
		      const struct m0_addb2_record *rec);

/**
 * Sets up a philter which identically true predicate function.
 *
 * This philter matches all records.
 */
void m0_addb2_philter_true_init(struct m0_addb2_philter *ph);

/**
 * Sets up a philter matching records with a given measurement identifier.
 */
void m0_addb2_philter_id_init(struct m0_addb2_philter *ph, uint64_t id);

/**
 * Adds a global philter that will be matched with every records produced by any
 * addb2 machine.
 */
void m0_addb2_philter_global_add(struct m0_addb2_philter *ph);

/**
 * Removes a global philter.
 */
void m0_addb2_philter_global_del(struct m0_addb2_philter *ph);
/** @} end of addb2 group */
#endif /* __MERO_ADDB2_CONSUMER_H__ */

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
