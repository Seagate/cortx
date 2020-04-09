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
 * Original creation date: 30-Jan-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT

#include "ut/ut.h"
#include "lib/finject.h"
#include "lib/thread.h"
#include "addb2/addb2.h"
#include "addb2/ut/common.h"

int submitted = 0;

int (*submit)(const struct m0_addb2_mach *mach,
	      struct m0_addb2_trace *trace);
void (*idle)(const struct m0_addb2_mach *mach);

static int test_submit(struct m0_addb2_mach *mach,
		       struct m0_addb2_trace_obj *obj)
{
	return (*submit)(mach, &obj->o_tr);
}

static void test_idle(struct m0_addb2_mach *mach)
{
	if (idle != NULL)
		(*idle)(mach);
}

static const struct m0_addb2_mach_ops test_mach_ops = {
	.apo_submit = &test_submit,
	.apo_idle   = &test_idle
};

extern struct m0_addb2_mach *(*m0_addb2__mach)(void);
static struct m0_thread     *main_thread;
static struct m0_addb2_mach *__mach;

static struct m0_addb2_mach *getmach(void)
{
	if (m0_thread_self() == main_thread)
		return __mach;
	else
		return m0_thread_tls()->tls_addb2_mach;
}

struct m0_addb2_mach *mach_set(int (*s)(const struct m0_addb2_mach  *,
					struct m0_addb2_trace *))
{
	submitted = 0;
	submit = s;
	__mach = m0_addb2_mach_init(&test_mach_ops, NULL);
	M0_UT_ASSERT(__mach != NULL);
	main_thread = m0_thread_self();
	m0_addb2__mach = &getmach;
	m0_fi_enable("mach", "surrogate-mach");
	return __mach;
}

void mach_fini(struct m0_addb2_mach *m)
{
	m0_fi_disable("mach", "surrogate-mach");
	__mach = NULL;
	m0_addb2__mach = NULL;
	m0_addb2_mach_fini(m);
	submit = NULL;
	idle = NULL;
	submitted = 0;
}

void mach_put(struct m0_addb2_mach *m)
{
	m0_addb2_mach_stop(m);
	mach_fini(m);
}


const uint64_t SENSOR_MARKER = 0x5555555555555555;
uint64_t seq = 0;
bool sensor_finalised;

static void snapshot(struct m0_addb2_sensor *s, uint64_t *area)
{
	area[0] = SENSOR_MARKER;
	area[1] = ++seq;
}

static void sensor_fini(struct m0_addb2_sensor *s)
{
	M0_UT_ASSERT(!sensor_finalised);
	sensor_finalised = true;
}

const struct m0_addb2_sensor_ops sensor_ops = {
	.so_snapshot = &snapshot,
	.so_fini     = &sensor_fini
};

int fill_one(struct m0_addb2_mach *m)
{
	unsigned sofar = submitted;
	int issued;

	for (issued = 0; submitted == sofar; ++ issued)
		M0_ADDB2_ADD(42, 1, 2, 3, 4, 5, 6);
	return issued;
}

bool valeq(const struct m0_addb2_value *v0, const struct m0_addb2_value *v1)
{
	return v0->va_id == v1->va_id && v0->va_nr == v1->va_nr &&
		memcmp(v0->va_data, v1->va_data,
		       v0->va_nr * sizeof v0->va_data[0]) == 0;
}

bool receq(const struct m0_addb2_record *r0, const struct small_record *r1)
{
	return valeq(&r0->ar_val, &r1->ar_val) &&
		r0->ar_label_nr == r1->ar_label_nr &&
		m0_forall(i, r0->ar_label_nr,
			  valeq(&r0->ar_label[i], &r1->ar_label[i]));
}

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
