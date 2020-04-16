#pragma once

#ifndef __MERO_XCODE_UT_TEST_GCCXML_SIMPLE_H__
#define __MERO_XCODE_UT_TEST_GCCXML_SIMPLE_H__

#ifndef __ENUM_ONLY

#ifndef __KERNEL__
#include <sys/types.h>
#include <stdint.h>
#endif

#include "xcode/xcode.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"

struct optfid {
	uint8_t o_flag;
	union {
		struct m0_fid o_fid   M0_XCA_TAG("1");
		uint32_t      o_short M0_XCA_TAG("3");
	} u;
} M0_XCA_UNION;

struct optfidarray {
	uint64_t ofa_nr;
	struct optfid *ofa_data;
} M0_XCA_SEQUENCE;

enum {
	NR = 9
};

struct fixarray {
	m0_void_t fa_none M0_XCA_TAG("NR");
	struct optfid *fa_data;
} M0_XCA_SEQUENCE;

struct testtypes {
    char          tt_char;
    char         *tt_pchar;
    const char   *tt_cpchar;
    int           tt_int;
    long long     tt_ll;
    unsigned int  tt_ui;
    void         *tt_buf;
} M0_XCA_RECORD;

struct inlinearray {
	/*
	 * M0_XA_ARRAY must contain a single field.
	 *
	 * This field must be an array.
	 */
	struct testtypes ia_bugs[5];
} M0_XCA_ARRAY;

#endif /* __ENUM_ONLY */

enum testenum {
	TE_0,
	TE_1,
	TE_5  = TE_0 + 5,
	TE_33 = 33
} M0_XCA_ENUM;

enum testbitmask {
	BM_ZERO     = 1ULL << 0,
	BM_SIX      = 1ULL << 6,
	BM_FOUR     = 1ULL << 4,
	BM_NINE     = 1ULL << 9,
	BM_FIVE     = 1ULL << 5
} M0_XCA_ENUM;

struct enumfield {
	uint64_t ef_0;
	uint64_t ef_enum M0_XCA_FENUM(testenum);
	uint64_t ef_bitm M0_XCA_FBITMASK(testbitmask);
	uint64_t ef_1;
} M0_XCA_RECORD;

#endif /* __MERO_XCODE_UT_TEST_GCCXML_SIMPLE_H__ */
