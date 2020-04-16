/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 30-May-2016
 */

/**
 * @addtogroup hash_fnc
 *
 * @{
 */
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"
#include "lib/memory.h"
#include "hash_fnc.h"

static const uint64_t k0 = 0xc3a5c85c97cb3127ULL;
static const uint64_t k1 = 0xb492b66fbe98f273ULL;
static const uint64_t k2 = 0x9ae16a3b2f90404fULL;
static const uint64_t def_mul = 0x9ddfea08eb382d69ULL;

static inline uint32_t swap_32(uint32_t x)
{
	x = ((x << 8) & 0xFF00FF00) | ((x>> 8) & 0x00FF00FF);
	x = (x >> 16) | (x << 16);
	return x;
}

static inline uint64_t swap_64(uint64_t x)
{
	union {
		uint64_t ll;
		uint32_t l[2];
	} w, r;

	w.ll = x;
	r.l[0] = swap_32(w.l[1]);
	r.l[1] = swap_32(w.l[0]);
	return r.ll;
}

static inline uint64_t fetch32(const unsigned char *val)
{
	uint32_t res;

	memcpy(&res, val, sizeof(res));
	return res;
}

static inline uint64_t fetch64(const unsigned char *val)
{
	uint64_t res;

	memcpy(&res, val, sizeof(res));
	return res;
}

static inline uint64_t rotate(uint64_t val, int shift)
{
	return shift == 0 ? val : ((val >> shift) | (val << (64 - shift)));
}

static inline uint32_t rotate32(uint32_t val, int shift)
{
	return shift == 0 ? val : ((val >> shift) | (val << (32 - shift)));
}

M0_INTERNAL uint64_t m0_hash_fnc_fnv1(const void *buffer, m0_bcount_t len)
{
	const unsigned char *ptr = (const unsigned char*)buffer;
	uint64_t             val = 14695981039346656037UL;
	m0_bcount_t          i;

	if (buffer == NULL || len == 0)
		return 0;
	for (i = 0; i < len; i++)
		val = (val * 1099511628211) ^ ptr[i];
	return val;
}

static inline uint64_t hash_len16(uint64_t u, uint64_t v, uint64_t mul)
{
	uint64_t a = (u ^ v) * mul;
	uint64_t b;

	a ^= (a >> 47);
	b = (v ^ a) * mul;
	b ^= (b >> 47);
	b *= mul;
	return b;
}

static inline struct m0_uint128 weak_hash32_seeds_6(uint64_t w, uint64_t x,
						    uint64_t y, uint64_t z,
						    uint64_t a, uint64_t b)
{
	uint64_t    c;

	a += w;
	b = rotate(b + a + z, 21);
	c = a;
	a += x;
	a += y;
	b += rotate(a, 44);
	return M0_UINT128((uint64_t) (a + z), (uint64_t) (b + c));
}

static inline struct m0_uint128 weak_hash32_seeds(const unsigned char* s,
						  uint64_t a, uint64_t b)
{
	return weak_hash32_seeds_6(fetch64(s),
				   fetch64(s + 8),
				   fetch64(s + 16),
				   fetch64(s + 24),
				   a, b);
}

static inline uint64_t shift_mix(uint64_t val)
{
	return val ^ (val >> 47);
}

static uint64_t hash_0to16(const unsigned char *buffer, m0_bcount_t len)
{
	if (len >= 8) {
		uint64_t mul = k2 + len * 2;
		uint64_t a = fetch64(buffer) + k2;
		uint64_t b = fetch64(buffer + len - 8);
		uint64_t c = rotate(b, 37) * mul + a;
		uint64_t d = (rotate(a, 25) + b) * mul;

		return hash_len16(c, d, mul);
	}
	if (len >= 4) {
		uint64_t mul = k2 + len * 2;
		uint64_t a = fetch32(buffer);

		return hash_len16(len + (a << 3),
				  fetch32(buffer + len - 4),
				  mul);
	}
	if (len > 0) {
		uint8_t a = buffer[0];
		uint8_t b = buffer[len >> 1];
		uint8_t c = buffer[len - 1];
		uint32_t y = a + (b << 8);
		uint32_t z = len + (c << 2);

		return shift_mix(y * k2 ^ z * k0) * k2;
	}
	return k2;
}

static uint64_t hash_17to32(const unsigned char *buffer, size_t len)
{
	uint64_t mul = k2 + len * 2;
	uint64_t a = fetch64(buffer) * k1;
	uint64_t b = fetch64(buffer + 8);
	uint64_t c = fetch64(buffer + len - 8) * mul;
	uint64_t d = fetch64(buffer + len - 16) * k2;

	return hash_len16(rotate(a + b, 43) + rotate(c, 30) + d,
		a + rotate(b + k2, 18) + c, mul);
}

static uint64_t hash_33to64(const unsigned char *buffer, size_t len)
{
	uint64_t mul = k2 + len * 2;
	uint64_t a = fetch64(buffer) * k2;
	uint64_t b = fetch64(buffer + 8);
	uint64_t c = fetch64(buffer + len - 24);
	uint64_t d = fetch64(buffer + len - 32);
	uint64_t e = fetch64(buffer + 16) * k2;
	uint64_t f = fetch64(buffer + 24) * 9;
	uint64_t g = fetch64(buffer + len - 8);
	uint64_t h = fetch64(buffer + len - 16) * mul;
	uint64_t u = rotate(a + g, 43) + (rotate(b, 30) + c) * 9;
	uint64_t v = ((a + g) ^ d) + f + 1;
	uint64_t w = swap_64((u + v) * mul) + h;
	uint64_t x = rotate(e + f, 42) + c;
	uint64_t y = (swap_64((v + w) * mul) + g) * mul;
	uint64_t z = e + f + c;

	a = swap_64((x + z) * mul + y) + b;
	b = shift_mix((z + a) * mul + d + h) * mul;
	return b + x;
}

M0_INTERNAL uint64_t m0_hash_fnc_city(const void *buffer, m0_bcount_t len)
{
	const unsigned char *ptr = (const unsigned char*)buffer;
	uint64_t             x;
	uint64_t             y;
	uint64_t             z;
	uint64_t             temp;
	struct m0_uint128    v;
	struct m0_uint128    w;

	if (buffer == NULL || len == 0)
		return 0;
	if (len <= 32) {
		if (len <= 16)
			return hash_0to16(buffer, len);
		else
			return hash_17to32(buffer, len);
	}
	else
		if (len <= 64)
			return hash_33to64(buffer, len);
	/* Large buffer processing. */
	x = fetch64(ptr + len - 40);
	y = fetch64(ptr + len - 16) + fetch64(ptr + len - 56);
	z = hash_len16(fetch64(ptr + len - 48) + len,
				fetch64(ptr + len - 24), def_mul);

	v = weak_hash32_seeds(ptr + len - 64, len, z);
	w = weak_hash32_seeds(ptr + len - 32, y + k1, x);
	x = x * k1 + fetch64(ptr);

	len = (len - 1) & ~(m0_bcount_t)(63);
	do {
		x = rotate(x + y + v.u_hi + fetch64(ptr + 8), 37) * k1;
		y = rotate(y + v.u_lo + fetch64(ptr + 48), 42) * k1;
		x ^= w.u_lo;
		y += v.u_hi + fetch64(ptr + 40);
		z = rotate(z + w.u_hi, 33) * k1;
		v = weak_hash32_seeds(ptr, v.u_lo * k1, x + w.u_hi);
		w = weak_hash32_seeds(ptr + 32, z + w.u_lo,
				      y + fetch64(ptr + 16));
		temp = z;
		z = x;
		x = temp;
		ptr += 64;
		len -= 64;
	} while (len != 0);
	return hash_len16(hash_len16(v.u_hi, w.u_hi, def_mul) +
			  shift_mix(y) * k1 + z,
			  hash_len16(v.u_lo, w.u_lo, def_mul) + x, def_mul);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of hash_fnc group */

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
