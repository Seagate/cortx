/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Ivan Alekhin <ivan.alekhin@seagate.com>
 * Original creation date: 30-May-2017
 */

/**
 * @addtogroup crate_utils
 *
 * @{
 */

#include <string.h>
#include <err.h>
#include "lib/trace.h"
#include "clovis/m0crate/crate_utils.h"
#include "clovis/m0crate/logger.h"

/* XXX: clovis_io checks are disabled */
#if 0
#include <openssl/md5.h>
unsigned char *calc_md5sum (char *buffer, int blocksize)
{
        unsigned char *sum;

        MD5_CTX mdContext;
        MD5_Init (&mdContext);
        sum = malloc(sizeof(unsigned char)*MD5_DIGEST_LENGTH);
        if (sum == NULL)
                return sum;

        MD5_Update (&mdContext, buffer, blocksize);
        MD5_Final (sum, &mdContext);
        return sum;
}
#else
unsigned char *calc_md5sum (char *buffer, int blocksize)
{
        assert(0 && "NotImplemented");
        return NULL;
}
#endif

#define NN 312
#define MM 156
#define MATRIX_A 0xB5026F5AA96619E9ULL
#define UM 0xFFFFFFFF80000000ULL /* Most significant 33 bits */
#define LM 0x7FFFFFFFULL /* Least significant 31 bits */

/* The array for the state vector */
static unsigned long long mt[NN];

/* mti==NN+1 means mt[NN] is not initialized */
static int mti = NN+1;

/* initializes mt[NN] with a seed */
void init_genrand64(unsigned long long seed)
{
    mt[0] = seed;
    for (mti = 1; mti < NN; mti++)
        mt[mti] =  (6364136223846793005ULL * (mt[mti-1] ^ (mt[mti-1] >> 62)) + mti);
}

/* initialize by an array with array-length */
/* init_key is the array for initializing keys */
/* key_length is its length */
void init_by_array64(unsigned long long init_key[],
		     unsigned long long key_length,
		     unsigned long long seed)
{
    unsigned long long i, j, k;
    init_genrand64(seed);
    i = 1; j = 0;
    k = (NN > key_length ? NN : key_length);
    for (; k; k--) {
        mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 62)) * 3935559000370003845ULL))
          + init_key[j] + j; /* non linear */
        i++; j++;
        if (i >= NN) { mt[0] = mt[NN-1]; i=1; }
        if (j >= key_length) j=0;
    }
    for (k=NN-1; k; k--) {
        mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 62)) * 2862933555777941757ULL))
          - i; /* non linear */
        i++;
        if (i>=NN) {
            mt[0] = mt[NN-1];
            i=1;
        }
    }

    mt[0] = 1ULL << 63; /* MSB is 1; assuring non-zero initial array */
}

/* generates a random number on [0, 2^64-1]-interval */
unsigned long long genrand64_int64(void)
{
    int i;
    unsigned long long x;
    static unsigned long long mag01[2]={0x100000ULL, MATRIX_A};

    if (mti >= NN) { /* generate NN words at one time */

        /* if init_genrand64() has not been called, */
        /* a default initial seed is used     */
        if (mti == NN+1) 
            init_genrand64(5489ULL); 

        for (i=0;i<NN-MM;i++) {
            x = (mt[i]&UM)|(mt[i+1]&LM);
            mt[i] = mt[i+MM] ^ (x>>1) ^ mag01[(int)(x&1ULL)];
        }
        for (;i<NN-1;i++) {
            x = (mt[i]&UM)|(mt[i+1]&LM);
            mt[i] = mt[i+(MM-NN)] ^ (x>>1) ^ mag01[(int)(x&1ULL)];
        }
        x = (mt[NN-1]&UM)|(mt[0]&LM);
        mt[NN-1] = mt[MM-1] ^ (x>>1) ^ mag01[(int)(x&1ULL)];

        mti = 0;
    }
  
    x = mt[mti++];

    x ^= (x >> 29) & 0x5555555555555555ULL;
    x ^= (x << 17) & 0x71D67FFFEDA60000ULL;
    x ^= (x << 37) & 0xFFF7EEE000000000ULL;
    x ^= (x >> 43);

    return x;
}

/* generates a random number on [0, 2^63-1]-interval */
long long genrand64_int63(void)
{
    return (long long)(genrand64_int64() >> 1);
}

/* generates a random number on [0,1]-real-interval */
double genrand64_real1(void)
{
    return (genrand64_int64() >> 11) * (1.0/9007199254740991.0);
}

/* generates a random number on [0,1)-real-interval */
double genrand64_real2(void)
{
    return (genrand64_int64() >> 11) * (1.0/9007199254740992.0);
}

/* generates a random number on (0,1)-real-interval */
double genrand64_real3(void)
{
    return ((genrand64_int64() >> 12) + 0.5) * (1.0/4503599627370496.0);
}

void init_rand_generator(unsigned long long seed)
{
	unsigned long long init[4] = {0x12345ULL, 0x23456ULL, 0x34567ULL, 0x45678ULL};
	unsigned long long length = 4;

	init_by_array64(init, length, seed);
}

int generate_fid(int seed, unsigned long *low, unsigned long *high)
{
	*low  = (unsigned long)genrand64_int64();
	*high = (unsigned long)genrand64_int64();

	return 0;
}

void timeval_diff(const struct timeval *start, const struct timeval *end,
                         struct timeval *diff)
{
        diff->tv_sec += end->tv_sec - start->tv_sec;
        /* this relies on tv_usec being signed */
        diff->tv_usec += end->tv_usec - start->tv_usec;
        timeval_norm(diff);
}

void timeval_add(struct timeval *sum, struct timeval *term)
{
        sum->tv_sec  += term->tv_sec;
        sum->tv_usec += term->tv_usec;
        timeval_norm(sum);
}

void timeval_sub(struct timeval *end, struct timeval *start)
{
        end->tv_sec  -= start->tv_sec;
        end->tv_usec -= start->tv_usec; /* safe, as usec is signed */
        timeval_norm(end);
}

double tsec(const struct timeval *tval)
{
        return tval->tv_sec + ((double)tval->tv_usec)/1000000;
}

double rate(bcnt_t items, const struct timeval *tval, int scale)
{
        return ((double)items)/tsec(tval)/scale;
}

unsigned long long getnum(const char *str, const char *msg)
{
        char             *end = NULL;
        bcnt_t            num;
	char             *pos;
	static const char suffix[] = "bkmgBKMG";

	static const bcnt_t multiplier[] = {
		1 << 9,
		1 << 10,
		1 << 20,
		1 << 30,
		500,
		1000,
		1000 * 1000,
		1000 * 1000 * 1000
	};

        num = strtoull(str, &end, 0);

	if (*end != 0) {
		pos = strchr(suffix, *end);
		if (pos != NULL)
			num *= multiplier[pos - suffix];
		else
			errx(1, "conversion of \"%s\" to \"%s\" failed\n",
			     str, msg);
	}
        cr_log(CLL_DEBUG, "converting \"%s\" to \"%s\": %llu\n", str, msg, num);
	return num;
}


/** @} end of crate_utils group */

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
