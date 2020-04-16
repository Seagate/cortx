#pragma once

#ifndef __MERO_XCODE_UT_TEST_GCCXML_H__
#define __MERO_XCODE_UT_TEST_GCCXML_H__

#include <sys/types.h>
#include <stdint.h>

#include "xcode/xcode.h"
#include "lib/vec.h"
#include "lib/vec_xc.h"

#include "xcode/ut/test_gccxml_simple.h"
#include "xcode/ut/test_gccxml_simple_xc.h"


struct package {
	struct m0_fid   p_fid;
	struct m0_vec   p_vec;
	struct m0_cred *p_cred M0_XCA_OPAQUE("m0_package_cred_get");
	struct package_p_name {
		uint32_t  s_nr;
		uint8_t  *s_data;
	} M0_XCA_SEQUENCE p_name;
} M0_XCA_RECORD;

#endif /* __MERO_XCODE_UT_TEST_GCCXML_H__ */

