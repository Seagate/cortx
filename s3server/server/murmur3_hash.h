//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#pragma once

#ifndef __S3_SERVER_MURMURHASH3_H__
#define __S3_SERVER_MURMURHASH3_H__

//-----------------------------------------------------------------------------
// Platform-specific functions and macros

// Microsoft Visual Studio

#if defined(_MSC_VER) && (_MSC_VER < 1600)

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;

// Other compilers

#else   // defined(_MSC_VER)

#include <stdint.h>

#endif // !defined(_MSC_VER)

//-----------------------------------------------------------------------------

void MurmurHash3_x86_32  ( const void * key, int len, uint32_t seed, void * out );

void MurmurHash3_x86_128 ( const void * key, int len, uint32_t seed, void * out );

void MurmurHash3_x64_128 ( const void * key, int len, uint32_t seed, void * out );

//-----------------------------------------------------------------------------

#endif  // __S3_SERVER_MURMURHASH3_H__
