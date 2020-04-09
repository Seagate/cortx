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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 *		    Alexey Lyashkov <Alexey_Lyashkov@xyratex.com>
 * Original creation date: 18-Jun-2010
 */

#pragma once

#ifndef __MERO_LIB_MISC_H__
#define __MERO_LIB_MISC_H__

#ifdef __KERNEL__
#  include <linux/string.h>       /* memset, strstr */
#  include <linux/bitops.h>       /* ffs */
#  include "lib/linux_kernel/misc.h"
#else
#  include <string.h>             /* memset, ffs, strstr */
#  include <limits.h>             /* CHAR_BIT */
#  include "lib/user_space/misc.h"
#endif
#include "lib/types.h"
#include "lib/assert.h"           /* M0_CASSERT */
#include "lib/buf.h"              /* m0_buf */

#define _QUOTE(s) #s
#define M0_QUOTE(s) _QUOTE(s)

/**
 * The absolute path to a file in Mero sources directory.
 *
 * M0_SRC_DIR is defined in configure.ac.
 */
#define M0_SRC_PATH(name) M0_QUOTE(M0_SRC_DIR) "/" name

/**
 * Returns rounded up value of @val in chunks of @size.
 * @pre m0_is_po2(size)
 */
M0_INTERNAL uint64_t m0_round_up(uint64_t val, uint64_t size);

/**
 * Returns rounded down value of @val in chunks of @size.
 * @pre m0_is_po2(size)
 */
M0_INTERNAL uint64_t m0_round_down(uint64_t val, uint64_t size);

#define M0_MEMBER_SIZE(type, member) sizeof(((type *)0)->member)

#define M0_SET0(obj)                     \
({                                       \
	M0_CASSERT(!m0_is_array(obj));   \
	memset((obj), 0, sizeof *(obj)); \
})

#define M0_IS0(obj) m0_forall(i, sizeof *(obj), ((char *)obj)[i] == 0)

#define M0_SET_ARR0(arr)                \
({                                      \
	M0_CASSERT(m0_is_array(arr));   \
	memset((arr), 0, sizeof (arr)); \
})

/** Returns the number of array elements that satisfy given criteria. */
#define m0_count(var, nr, ...)                     \
({                                                 \
	unsigned __nr = (nr);                      \
	unsigned var;                              \
	unsigned count;                            \
						   \
	for (count = var = 0; var < __nr; ++var) { \
		if (__VA_ARGS__)                   \
			++count;                   \
	}                                          \
	count;                                     \
})

/**
 * Returns a conjunction (logical AND) of an expression evaluated over a range
 *
 * Declares an unsigned integer variable named "var" in a new scope and
 * evaluates user-supplied expression (the last argument) with "var" iterated
 * over successive elements of [0 .. NR - 1] range, while this expression
 * returns true. Returns true iff the whole range was iterated over.
 *
 * This function is useful for invariant checking.
 *
 * @code
 * bool foo_invariant(const struct foo *f)
 * {
 *        return m0_forall(i, ARRAY_SIZE(f->f_nr_bar), f->f_bar[i].b_count > 0);
 * }
 * @endcode
 *
 * @see m0_tlist_forall(), m0_tl_forall(), m0_list_forall().
 * @see m0_list_entry_forall().
 */
#define m0_forall(var, nr, ...)                                 \
({                                                              \
	unsigned __nr = (nr);                                   \
	unsigned var;                                           \
								\
	for (var = 0; var < __nr && ({ __VA_ARGS__ ; }); ++var) \
		;                                               \
	var == __nr;                                            \
})

/**
 * Returns a disjunction (logical OR) of an expression evaluated over a range.
 *
 * @code
 * bool haystack_contains(int needle)
 * {
 *         return m0_exists(i, ARRAY_SIZE(haystack), haystack[i] == needle);
 * }
 * @endcode
 *
 * @see m0_forall()
 */
#define m0_exists(var, nr, ...) !m0_forall(var, (nr), !(__VA_ARGS__))

/**
 * Reduces ("aggregates") given expression over an interval.
 *
 * @see http://en.wikipedia.org/wiki/Fold_(higher-order_function)
 *
 * Example uses
 *
 * @code
 * sum = m0_reduce(i, ARRAY_SIZE(a), 0, + a[i]);
 * product = m0_reduce(i, ARRAY_SIZE(b), 1, * b[i]);
 * @encode
 *
 * @see m0_fold(), m0_tl_reduce()
 */
#define m0_reduce(var, nr, init, exp)		\
({						\
	unsigned __nr = (nr);			\
	unsigned var;				\
	typeof(init) __accum = (init);		\
						\
	for (var = 0; var < __nr; ++var) {	\
		__accum = __accum exp;		\
	}					\
	__accum;				\
})

/**
 * Folds given expression over an interval.
 *
 * This is a generalised version of m0_reduce().
 *
 * @see http://en.wikipedia.org/wiki/Fold_(higher-order_function)
 *
 * Example uses
 *
 * @code
 * sum = m0_fold(i, s, ARRAY_SIZE(a), 0, s + a[i]);
 * max = m0_fold(i, m, ARRAY_SIZE(b), INT_MIN, max_t(int, m, a[i]));
 * @encode
 *
 * @see m0_reduce(), m0_tl_fold()
 */
#define m0_fold(var, accum, nr, init, exp)	\
({						\
	unsigned __nr = (nr);			\
	unsigned var;				\
	typeof(init) accum = (init);		\
						\
	for (var = 0; var < __nr; ++var) {	\
		accum = exp;			\
	}					\
	accum;					\
})

/**
   Evaluates to true iff x is present in set.

   e.g. M0_IN(session->s_state, (M0_RPC_SESSION_IDLE,
				 M0_RPC_SESSION_BUSY,
				 M0_RPC_SESSION_TERMINATING))

   Parentheses around "set" members are mandatory.
 */
#define M0_IN(x, set)						\
	({ typeof (x) __x = (x);				\
		M0_IN0(__x, M0_UNPACK set); })

#define M0_UNPACK(...) __VA_ARGS__

#define M0_IN0(...) \
	M0_CAT(M0_IN_, M0_COUNT_PARAMS(__VA_ARGS__))(__VA_ARGS__)

#define M0_IN_1(x, v) ((x) == (v))
#define M0_IN_2(x, v, ...) ((x) == (v) || M0_IN_1(x, __VA_ARGS__))
#define M0_IN_3(x, v, ...) ((x) == (v) || M0_IN_2(x, __VA_ARGS__))
#define M0_IN_4(x, v, ...) ((x) == (v) || M0_IN_3(x, __VA_ARGS__))
#define M0_IN_5(x, v, ...) ((x) == (v) || M0_IN_4(x, __VA_ARGS__))
#define M0_IN_6(x, v, ...) ((x) == (v) || M0_IN_5(x, __VA_ARGS__))
#define M0_IN_7(x, v, ...) ((x) == (v) || M0_IN_6(x, __VA_ARGS__))
#define M0_IN_8(x, v, ...) ((x) == (v) || M0_IN_7(x, __VA_ARGS__))
#define M0_IN_9(x, v, ...) ((x) == (v) || M0_IN_8(x, __VA_ARGS__))

/**
   M0_BITS(...) returns bitmask of passed states.
   e.g.
@code
   enum foo_states {
	FOO_UNINITIALISED,
	FOO_INITIALISED,
	FOO_ACTIVE,
	FOO_FAILED,
	FOO_NR,
   };
@endcode

   then @code M0_BITS(FOO_ACTIVE, FOO_FAILED) @endcode returns
   @code (1ULL << FOO_ACTIVE) | (1ULL << FOO_FAILED) @endcode

   @note M0_BITS() macro with no parameters causes compilation failure.
 */
#define M0_BITS(...) \
	M0_CAT(__M0_BITS_, M0_COUNT_PARAMS(__VA_ARGS__))(__VA_ARGS__)

#define __M0_BITS_0(i)        (1ULL << (i))
#define __M0_BITS_1(i, ...)  ((1ULL << (i)) | __M0_BITS_0(__VA_ARGS__))
#define __M0_BITS_2(i, ...)  ((1ULL << (i)) | __M0_BITS_1(__VA_ARGS__))
#define __M0_BITS_3(i, ...)  ((1ULL << (i)) | __M0_BITS_2(__VA_ARGS__))
#define __M0_BITS_4(i, ...)  ((1ULL << (i)) | __M0_BITS_3(__VA_ARGS__))
#define __M0_BITS_5(i, ...)  ((1ULL << (i)) | __M0_BITS_4(__VA_ARGS__))
#define __M0_BITS_6(i, ...)  ((1ULL << (i)) | __M0_BITS_5(__VA_ARGS__))
#define __M0_BITS_7(i, ...)  ((1ULL << (i)) | __M0_BITS_6(__VA_ARGS__))
#define __M0_BITS_8(i, ...)  ((1ULL << (i)) | __M0_BITS_7(__VA_ARGS__))

M0_INTERNAL const char *m0_bool_to_str(bool b);

/**
 * Extracts the file name, relative to a mero sources directory, from a
 * full-path file name. A mero source directory is detected by a name
 * "mero/".
 *
 * For example, given the following full-path file name:
 *
 *     /path/to/mero/lib/ut/finject.c
 *
 * A short file name, relative to the "mero/" directory, is:
 *
 *     lib/ut/finject.c
 *
 * @bug {
 *     This function doesn't search for the rightmost occurrence of "mero/"
 *     in a file path, if "mero/" encounters several times in the path the first
 *     one will be picked up:
 *
 *       /path/to/mero/fs/mero/lib/misc.h => fs/mero/lib/misc.h
 * }
 *
 * @param   fname  full path
 *
 * @return  short file name - a pointer inside fname string to the remaining
 *          file path, after mero source directory;
 *          if short file name cannot be found, then full fname is returned.
 */
M0_INTERNAL const char *m0_short_file_name(const char *fname);

/* strtoull for user- and kernel-space */
uint64_t m0_strtou64(const char *str, char **endptr, int base);

/* strtoul for user- and kernel-space */
uint32_t m0_strtou32(const char *str, char **endptr, int base);

/*
 * Helper macros for implication and equivalence.
 *
 * Unfortunately, name clashes are possible and m0_ prefix is too awkward. See
 * M0_BASSERT() checks in lib/misc.c
 */
#ifndef ergo
#define ergo(a, b) (!(a) || (b))
#endif

#ifndef equi
#define equi(a, b) (!(a) == !(b))
#endif

void __dummy_function(void);

/**
 * A macro used with if-statements without `else' clause to assure proper
 * coverage analysis.
 */
#define AND_NOTHING_ELSE else __dummy_function();

#define m0_is_array(x) \
	(!__builtin_types_compatible_p(typeof(&(x)[0]), typeof(x)))

#define IS_IN_ARRAY(idx, array)                     \
({                                                  \
	M0_CASSERT(m0_is_array(array));             \
	((unsigned long)(idx)) < ARRAY_SIZE(array); \
})

M0_INTERNAL bool m0_elems_are_unique(const void *array, unsigned nr_elems,
				     size_t elem_size);

#define M0_AMB(obj, ptr, field)				\
	(container_of((ptr), typeof(*(obj)), field))

#define M0_MEMBER_PTR(ptr, member)		\
({						\
	typeof(ptr) __ptr = (ptr);		\
	__ptr == NULL ? NULL : &__ptr->member;	\
})

#define M0_MEMBER(ptr, member)			\
({						\
	typeof(ptr) __ptr = (ptr);		\
	__ptr == NULL ? NULL : __ptr->member;	\
})

/**
 * Produces an expression having the same type as a given field in a given
 * struct or union. Suitable to be used as an argument to sizeof() or typeof().
 */
#define M0_FIELD_VALUE(type, field) (((type *)0)->field)

/**
 * True if an expression has a given type.
 */
#define M0_HAS_TYPE(expr, type) __builtin_types_compatible_p(typeof(expr), type)

/**
 * True iff type::field is of type "ftype".
 */
#define M0_FIELD_IS(type, field, ftype) \
	M0_HAS_TYPE(M0_FIELD_VALUE(type, field), ftype)

/**
 * Computes offset of "magix" field, iff magix field is of type uint64_t.
 * Otherwise causes compilation failure.
 */
#define M0_MAGIX_OFFSET(type, field) \
M0_FIELD_IS(type, field, uint64_t) ? \
	offsetof(type, field) :      \
	sizeof(char [M0_FIELD_IS(type, field, uint64_t) - 1])

/**
 * Returns the number of parameters given to this variadic macro (up to 9
 * parameters are supported)
 * @note M0_COUNT_PARAMS() returns max(number_of_parameters - 1, 0)
 *     e.g. M0_COUNT_PARAMS()        -> 0
 *          M0_COUNT_PARAMS(x)       -> 0
 *          M0_COUNT_PARAMS(x, y)    -> 1
 *          M0_COUNT_PARAMS(x, y, z) -> 2
 */
#define M0_COUNT_PARAMS(...) \
	M0_COUNT_PARAMS2(__VA_ARGS__, 9,8,7,6,5,4,3,2,1,0)
#define M0_COUNT_PARAMS2(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_, ...) _

/**
 * Concatenates two arguments to produce a single token.
 */
#define M0_CAT(A, B) M0_CAT2(A, B)
#define M0_CAT2(A, B) A ## B

#define M0_UNUSED __attribute__((unused))

M0_INTERNAL uint32_t m0_no_of_bits_set(uint64_t val);

M0_INTERNAL unsigned int
m0_full_name_hash(const unsigned char *name, unsigned int len);

/**
 * Converts Mero function pointer in a form that can be stored somewhere (e.g.,
 * in a trace log or addb2 record) and later decoded back into original pointer.
 *
 * Such transformation is needed, because function pointers depend on the
 * address at which Mero library is loaded.
 *
 * @pre "p" must be a pointer to Mero executable code or NULL.
 */
M0_INTERNAL uint64_t m0_ptr_wrap(const void *p);

M0_INTERNAL const void *m0_ptr_unwrap(uint64_t val);

/**
 * Val should be of unsigned type.
 */
#define M0_CIRCULAR_SHIFT_LEFT(val, bits)                       \
({                                                              \
	typeof(val) __v = (val);                                \
	typeof(bits) __b = (bits);                              \
								\
	(__v << __b) | (__v >> (sizeof(__v) * CHAR_BIT - __b)); \
})

enum {
	UINT32_STR_LEN = 64
};


/** An object representing a key value pair. */
struct m0_key_val {
	struct m0_buf kv_key;
	struct m0_buf kv_val;
};

/**
 * Apply a permutation given by its Lehmer code in k[] to a set s[] of n
 * elements and build inverse permutation in r[].
 *
 * @param n - number of elements in k[], s[] and r[]
 * @param k - Lehmer code of the permutation
 * @param s - an array to permute
 * @param r - an array to build inverse permutation in
 *
 * @pre  m0_forall(i, n, k[i] + i < n)
 * @pre  m0_forall(i, n, s[i] < n && ergo(s[i] == s[j], i == j))
 * @post m0_forall(i, n, s[i] < n && ergo(s[i] == s[j], i == j))
 * @post m0_forall(i, n, s[r[i]] == i && r[s[i]] == i)
 */
M0_INTERNAL void m0_permute(uint64_t n, uint64_t *k, uint64_t *s, uint64_t *r);

/** Sorts an array of integers in ascending order. */
M0_INTERNAL void m0_array_sort(uint64_t *arr, uint64_t arr_len);

/** Get number of complete bytes from provided bits number. */
#define M0_BYTES(bits_nr) ((bits_nr + 7) / 8)

/** Get i-th bit value from the buffer. */
M0_INTERNAL bool m0_bit_get(void *buffer, m0_bcount_t i);

/**Set i-th bit value in the buffer. */
M0_INTERNAL void m0_bit_set(void *buffer, m0_bcount_t i, bool val);

/** Initialises a key value pair. */
M0_INTERNAL void m0_key_val_init(struct m0_key_val *kv, const struct m0_buf *key,
				 const struct m0_buf *val);

/**
 * This API implements Boyer-Moore Voting Algorithm.
 * Returns the majority element present in an input array. The majority element
 * of an array, if present, is the element that occurs more than n/2 times in
 * the array of length n. It has been assumed that members of the array can be
 * compared for equality and method for the same needs to be provided by the
 * user of the API.
 * Returns null if there is no majority element.
 */
M0_INTERNAL void *m0_vote_majority_get(struct m0_key_val *arr, uint32_t len,
				       bool (*cmp)(const struct m0_buf *,
					           const struct m0_buf *),
				       uint32_t *vote_nr);

/**
 * Returns true iff it's M0_KEY_VAL_NULL.
 */
M0_INTERNAL bool m0_key_val_is_null(struct m0_key_val *kv);

/**
 * Initialises a key to M0_KEY_VAL_NULL.
 */
M0_INTERNAL void m0_key_val_null_set(struct m0_key_val *kv);

M0_EXTERN const struct m0_key_val M0_KEY_VAL_NULL;

/**
 * Generates process-uinque identifier. WARN: Atomic based.
 */
M0_INTERNAL uint64_t m0_dummy_id_generate(void);

#endif /* __MERO_LIB_MISC_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
