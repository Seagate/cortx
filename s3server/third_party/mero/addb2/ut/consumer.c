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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT

#include "lib/trace.h"
#include "lib/misc.h"              /* ARRAY_SIZE */
#include "ut/ut.h"
#include "addb2/addb2.h"
#include "addb2/consumer.h"
#include "addb2/ut/common.h"

static int noop_submit(const struct m0_addb2_mach  *m,
		       struct m0_addb2_trace *t)
{
	++submitted;
	return 0;
}

static struct m0_addb2_mach        *m;
static struct m0_addb2_source      *s;
static struct m0_addb2_philter      p;
static struct m0_addb2_callback     c;
static struct m0_addb2_sensor       sen;

static void (*fire)(const struct m0_addb2_source   *,
		    const struct m0_addb2_philter  *,
		    const struct m0_addb2_callback *,
		    const struct m0_addb2_record   *) = NULL;

static void test_fire(const struct m0_addb2_source   *s,
		      const struct m0_addb2_philter  *p,
		      const struct m0_addb2_callback *c,
		      const struct m0_addb2_record   *r)
{
	(*fire)(s, p, c, r);
}

static void source_get(void)
{
	m = mach_set(&noop_submit);
	s = m0_addb2_mach_source(m);
	m0_addb2_philter_true_init(&p);
	m0_addb2_callback_init(&c, &test_fire, NULL);
	m0_addb2_callback_add(&p, &c);
	m0_addb2_philter_add(s, &p);
}

static void source_put(void)
{
	m0_addb2_philter_del(&p);
	m0_addb2_callback_del(&c);
	m0_addb2_callback_fini(&c);
	m0_addb2_philter_fini(&p);
	mach_put(m);
	fire = NULL;
}

static struct stream {
	unsigned                nr;
	unsigned                seen;
	struct small_record    *rec;
} *shouldbe;

static uint64_t fired;

static void cmp_fire(const struct m0_addb2_source   *s,
		     const struct m0_addb2_philter  *p,
		     const struct m0_addb2_callback *c,
		     const struct m0_addb2_record   *r)
{
	if (shouldbe != NULL) {
		M0_UT_ASSERT(shouldbe->seen < shouldbe->nr);
		M0_UT_ASSERT(receq(r, &shouldbe->rec[shouldbe->seen]));
		++ shouldbe->seen;
	}
	++ fired;
}

/**
 * "empty" test: check that empty trace is empty.
 */
static void empty(void)
{
	fired = 0;
	fire = &cmp_fire;
	source_get();
	source_put();
	M0_UT_ASSERT(fired == 0);
}

enum {
	LABEL_ID = 0xffff5aa4a947 /* A most important constant. */
};

static uint64_t payload[1024];

/**
 * "empty-push-pop" test: check that trace with context manipulations only
 * contains no records.
 */
static void empty_push_pop(void)
{
	int i;

	fired = 0;
	fire = &cmp_fire;
	source_get();
	for (i = 0; i < M0_ADDB2_LABEL_MAX / 2; ++i) {
		m0_addb2_push(LABEL_ID + i, i/8, payload + i);
		m0_addb2_push(LABEL_ID, 0, NULL);
		m0_addb2_pop(LABEL_ID);
	}
	while (--i >= 0)
		m0_addb2_pop(LABEL_ID + i);
	source_put();
	M0_UT_ASSERT(fired == 0);
}

/**
 * "data" test: add a record, check that it is returned to the consumer.
 */
static void data(void)
{
	fire = &cmp_fire;
	shouldbe = &(struct stream) {
		.nr = 1,
		.seen = 0,
		.rec = (struct small_record[]) {
			[0] = {
				.ar_val      = VAL(LABEL_ID, 2, 3, 5, 7, 11),
				.ar_label_nr = 0
			}
		}
	};

	source_get();
	M0_ADDB2_ADD(LABEL_ID, 2, 3, 5, 7, 11);
	source_put();
	M0_UT_ASSERT(shouldbe->seen == shouldbe->nr);
}

/**
 * "data-label" test: create a context, add records; check that records are
 * returned with correct labels.
 */
static void data_label(void)
{
	fire = &cmp_fire;
	shouldbe = &(struct stream) {
		.nr = 2,
		.seen = 0,
		.rec = (struct small_record[]) {
			[0] = {
				.ar_val      = VAL(LABEL_ID, 2, 3, 5, 7, 11),
				.ar_label_nr = 1,
				.ar_label    = {
					[0] = VAL(LABEL_ID + 1, 1, 1, 2, 3, 5)
				}
			},
			[1] = {
				.ar_val      = VAL(LABEL_ID + 4, 1, 6, 21, 107),
				.ar_label_nr = 2,
				.ar_label    = {
					[0] = VAL(LABEL_ID + 1, 1, 1, 2, 3, 5),
					[1] = VAL(LABEL_ID + 3, 1, 2, 4, 8, 16,
						  32, 64, 128, 256, 512)
				}
			}
		}
	};

	source_get();
	M0_ADDB2_PUSH(LABEL_ID + 1, 1, 1, 2, 3, 5);
	M0_ADDB2_ADD(LABEL_ID, 2, 3, 5, 7, 11);
	M0_ADDB2_PUSH(LABEL_ID + 2, 1, 4, 9, 16, 25, 36, 49, 64, 81, 100);
	m0_addb2_pop(LABEL_ID + 2);
	M0_ADDB2_PUSH(LABEL_ID + 3, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512);
	M0_ADDB2_ADD(LABEL_ID + 4, 1, 6, 21, 107); /* A060843 */
	m0_addb2_pop(LABEL_ID + 3);
	m0_addb2_pop(LABEL_ID + 1);
	source_put();
	M0_UT_ASSERT(shouldbe->seen == shouldbe->nr);
}

#if 0
#include <stdio.h>

static void indent(unsigned x)
{
	static const char ruler[] = "                                         "
		"                                       ";
	printf("%*.*s", x, x, ruler);
}

static void val_print(const struct m0_addb2_value *v, int offset)
{
	int i;

	indent(offset);
	printf("[%8.8"PRIx64" |", v->va_id);
	for (i = 0; i < v->va_nr; ++i)
		printf(" %8.8"PRIx64, v->va_data[i]);
	printf("]\n");
}

static void print_fire(const struct m0_addb2_source   *s,
		       const struct m0_addb2_philter  *p,
		       const struct m0_addb2_callback *c,
		       const struct m0_addb2_record   *r)
{
	int i;

	val_print(&r->ar_val, 0);
	for (i = 0; i < r->ar_label_nr; ++i)
		val_print(&r->ar_label[i], 8);
	++ fired;
}
#endif

/**
 * "sensor" test: create a context, add a sensor; check that the sensor readings
 * are returned correctly.
 */
static void sensor(void)
{
	fire = &cmp_fire;
	shouldbe = NULL;
	fired = 0;
	seq = 0;
	sensor_finalised = false;

	source_get();
	M0_ADDB2_PUSH(LABEL_ID + 0, 1, 1, 2, 3, 5);
	M0_ADDB2_PUSH(LABEL_ID + 1, 1, 6, 21, 107);
	m0_addb2_sensor_add(&sen, LABEL_ID + 2, 2, -1, &sensor_ops);
	M0_ADDB2_ADD(LABEL_ID + 4, 1, 6, 21, 107);
	m0_addb2_pop(LABEL_ID + 1);
	m0_addb2_pop(LABEL_ID + 0);
	source_put();
	M0_UT_ASSERT(seq + 1 == fired);
	M0_UT_ASSERT(sensor_finalised);
}

/**
 * "sensor-N" test: create a context, add a sensor, force the trace buffer to
 * completion; check that the sensor readings are reproduced.
 */
static void sensor_N(void)
{
	int issued = 0;

	fire = &cmp_fire;
	shouldbe = NULL;
	fired = 0;
	seq = 0;
	sensor_finalised = false;
	M0_SET0(&sen);

	source_get();
	M0_ADDB2_PUSH(LABEL_ID + 0, 1, 1, 2, 3, 5);
	M0_ADDB2_PUSH(LABEL_ID + 1, 1, 6, 21, 107);
	m0_addb2_sensor_add(&sen, LABEL_ID + 2, 2, -1, &sensor_ops);
	M0_ADDB2_ADD(LABEL_ID + 4, 1, 6, 21, 107);
	++ issued;
	issued += fill_one(m);
	M0_ADDB2_ADD(LABEL_ID + 5, 127, 0, 0, 1);
	++ issued;
	m0_addb2_pop(LABEL_ID + 1);
	m0_addb2_pop(LABEL_ID + 0);
	source_put();
	M0_UT_ASSERT(issued + seq == fired);
	M0_UT_ASSERT(sensor_finalised);
}

static void sensor_check_fire(const struct m0_addb2_source   *s,
			      const struct m0_addb2_philter  *p,
			      const struct m0_addb2_callback *c,
			      const struct m0_addb2_record   *r)
{
	M0_UT_ASSERT(receq(r, &(struct small_record) {
		.ar_val = VAL(LABEL_ID + 2, SENSOR_MARKER, seq),
		.ar_label_nr = 2,
		.ar_label = {
			[0] = VAL(LABEL_ID + 0, 1, 1, 2, 3, 5),
			[1] = VAL(LABEL_ID + 1, 1, 6, 21, 107)
		}
	}));
	++ *(int *)c->ca_datum;
}

/**
 * "id-philter" test: create a context, add a sensor, add a number of
 * records. Create an id-philter for the sensor, check that the philter sees all
 * sensor read outs.
 */
static void id_philter(void)
{
	struct m0_addb2_philter  idph;
	struct m0_addb2_callback sensor_check;
	int sensor_consumed = 0;

	fire = &cmp_fire;
	shouldbe = NULL;
	fired = 0;
	seq = 0;
	sensor_finalised = false;
	M0_SET0(&sen);

	source_get();

	m0_addb2_philter_id_init(&idph, LABEL_ID + 2);
	m0_addb2_callback_init(&sensor_check,
			       &sensor_check_fire, &sensor_consumed);
	m0_addb2_callback_add(&idph, &sensor_check);
	m0_addb2_philter_add(s, &idph);

	M0_ADDB2_PUSH(LABEL_ID + 0, 1, 1, 2, 3, 5);
	M0_ADDB2_PUSH(LABEL_ID + 1, 1, 6, 21, 107);
	m0_addb2_sensor_add(&sen, LABEL_ID + 2, 2, -1, &sensor_ops);
	fill_one(m);
	fill_one(m);
	fill_one(m);
	fill_one(m);
	m0_addb2_pop(LABEL_ID + 1);
	m0_addb2_pop(LABEL_ID + 0);

	m0_addb2_philter_del(&idph);
	m0_addb2_callback_del(&sensor_check);
	m0_addb2_callback_fini(&sensor_check);
	m0_addb2_philter_fini(&idph);
	source_put();
	M0_UT_ASSERT(sensor_consumed == seq);
	M0_UT_ASSERT(sensor_finalised);
}

/**
 * "global-philter" test: check that m0_addb2_philter_global_{add,del}()
 * manipulate global philter list.
 */
static void global_philter(void)
{
	fire = &cmp_fire;
	fired = 0;
	m0_addb2_philter_true_init(&p);
	m0_addb2_callback_init(&c, &test_fire, NULL);
	m0_addb2_callback_add(&p, &c);
	m0_addb2_philter_global_add(&p);
	M0_ADDB2_ADD(LABEL_ID, 2, 3, 5, 7, 11);
	M0_UT_ASSERT(fired == 1);
	m0_addb2_philter_global_del(&p);
	m0_addb2_callback_del(&c);
	m0_addb2_callback_fini(&c);
	m0_addb2_philter_fini(&p);
	fired = 0;
	M0_ADDB2_ADD(LABEL_ID, 2, 3, 5, 7, 11);
	M0_UT_ASSERT(fired == 0);
	m0_addb2_philter_true_init(&p);
	m0_addb2_callback_init(&c, &test_fire, NULL);
	m0_addb2_callback_add(&p, &c);
	m0_addb2_philter_global_add(&p);
	m0_addb2_philter_global_add(&p);
	M0_ADDB2_ADD(LABEL_ID, 2, 3, 5, 7, 11);
	M0_UT_ASSERT(fired == 2);
	m0_addb2_philter_global_del(&p);
	m0_addb2_philter_global_del(&p);
	m0_addb2_callback_del(&c);
	m0_addb2_callback_fini(&c);
	m0_addb2_philter_fini(&p);
}

struct m0_ut_suite addb2_consumer_ut = {
	.ts_name = "addb2-consumer",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
		{ "empty",                &empty },
		{ "empty-push-pop",       &empty_push_pop },
		{ "data",                 &data },
		{ "data-label",           &data_label },
		{ "sensor",               &sensor },
		{ "sensor-N",             &sensor_N },
		{ "id-philter",           &id_philter },
		{ "global-philter",       &global_philter },
		{ NULL, NULL }
	}
};

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
