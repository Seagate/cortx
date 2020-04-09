/*
 * GF-Complete: A Comprehensive Open Source Library for Galois Field Arithmetic
 * James S. Plank, Ethan L. Miller, Kevin M. Greenan,
 * Benjamin A. Arnold, John A. Burnum, Adam W. Disney, Allen C. McBride.
 *
 * Copyright (c) 2014: Janne Grunau <j@jannau.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  - Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 *  - Neither the name of the University of Tennessee nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * gf_w16_neon.c
 *
 * Neon routines for 16-bit Galois fields
 *
 */

#include "gf_int.h"
#include <stdio.h>
#include <stdlib.h>
#include "gf_w16.h"

#ifdef ARCH_AARCH64
static
inline
void
neon_w16_split_4_multiply_region(gf_t *gf, uint16_t *src, uint16_t *dst,
                                 uint16_t *d_end, uint8_t *tbl,
                                 gf_val_32_t val, int xor)
{
  unsigned i;
  uint8_t *high = tbl + 4 * 16;
  uint16x8_t va0, va1, r0, r1;
  uint8x16_t loset, rl, rh;
  uint8x16x2_t va;

  uint8x16_t tbl_h[4], tbl_l[4];
  for (i = 0; i < 4; i++) {
      tbl_l[i] = vld1q_u8(tbl + i*16);
      tbl_h[i] = vld1q_u8(high + i*16);
  }

  loset = vdupq_n_u8(0xf);

  while (dst < d_end) {
      va0 = vld1q_u16(src);
      va1 = vld1q_u16(src + 8);

      va = vtrnq_u8(vreinterpretq_u8_u16(va0), vreinterpretq_u8_u16(va1));

      rl = vqtbl1q_u8(tbl_l[0], vandq_u8(va.val[0], loset));
      rh = vqtbl1q_u8(tbl_h[0], vandq_u8(va.val[0], loset));
      rl = veorq_u8(rl, vqtbl1q_u8(tbl_l[2], vandq_u8(va.val[1], loset)));
      rh = veorq_u8(rh, vqtbl1q_u8(tbl_h[2], vandq_u8(va.val[1], loset)));

      va.val[0] = vshrq_n_u8(va.val[0], 4);
      va.val[1] = vshrq_n_u8(va.val[1], 4);

      rl = veorq_u8(rl, vqtbl1q_u8(tbl_l[1], va.val[0]));
      rh = veorq_u8(rh, vqtbl1q_u8(tbl_h[1], va.val[0]));
      rl = veorq_u8(rl, vqtbl1q_u8(tbl_l[3], va.val[1]));
      rh = veorq_u8(rh, vqtbl1q_u8(tbl_h[3], va.val[1]));

      va = vtrnq_u8(rl, rh);
      r0 = vreinterpretq_u16_u8(va.val[0]);
      r1 = vreinterpretq_u16_u8(va.val[1]);

      if (xor) {
          va0 = vld1q_u16(dst);
          va1 = vld1q_u16(dst + 8);
          r0 = veorq_u16(r0, va0);
          r1 = veorq_u16(r1, va1);
      }
      vst1q_u16(dst, r0);
      vst1q_u16(dst + 8, r1);

      src += 16;
      dst += 16;
  }
}

static
inline
void
neon_w16_split_4_altmap_multiply_region(gf_t *gf, uint8_t *src,
                                        uint8_t *dst, uint8_t *d_end,
                                        uint8_t *tbl, gf_val_32_t val,
                                        int xor)
{
  unsigned i;
  uint8_t *high = tbl + 4 * 16;
  uint8x16_t vh, vl, rh, rl;
  uint8x16_t loset;

  uint8x16_t tbl_h[4], tbl_l[4];
  for (i = 0; i < 4; i++) {
      tbl_l[i] = vld1q_u8(tbl + i*16);
      tbl_h[i] = vld1q_u8(high + i*16);
  }

  loset = vdupq_n_u8(0xf);

  while (dst < d_end) {
      vh = vld1q_u8(src);
      vl = vld1q_u8(src + 16);

      rl = vqtbl1q_u8(tbl_l[0], vandq_u8(vl, loset));
      rh = vqtbl1q_u8(tbl_h[0], vandq_u8(vl, loset));
      rl = veorq_u8(rl, vqtbl1q_u8(tbl_l[2], vandq_u8(vh, loset)));
      rh = veorq_u8(rh, vqtbl1q_u8(tbl_h[2], vandq_u8(vh, loset)));

      vl = vshrq_n_u8(vl, 4);
      vh = vshrq_n_u8(vh, 4);

      rl = veorq_u8(rl, vqtbl1q_u8(tbl_l[1], vl));
      rh = veorq_u8(rh, vqtbl1q_u8(tbl_h[1], vl));
      rl = veorq_u8(rl, vqtbl1q_u8(tbl_l[3], vh));
      rh = veorq_u8(rh, vqtbl1q_u8(tbl_h[3], vh));

      if (xor) {
          vh = vld1q_u8(dst);
          vl = vld1q_u8(dst + 16);
          rh = veorq_u8(rh, vh);
          rl = veorq_u8(rl, vl);
      }
      vst1q_u8(dst, rh);
      vst1q_u8(dst + 16, rl);

      src += 32;
      dst += 32;
  }
}

#else /* ARCH_AARCH64 */

static
inline
void
neon_w16_split_4_multiply_region(gf_t *gf, uint16_t *src, uint16_t *dst,
                                 uint16_t *d_end, uint8_t *tbl,
                                 gf_val_32_t val, int xor)
{
  unsigned i;
  uint8_t *high = tbl + 4 * 16;
  uint16x8_t va, r;
  uint8x8_t loset, vb, vc, rl, rh;

  uint8x8x2_t tbl_h[4], tbl_l[4];
  for (i = 0; i < 4; i++) {
      tbl_l[i].val[0] = vld1_u8(tbl + i*16);
      tbl_l[i].val[1] = vld1_u8(tbl + i*16 + 8);
      tbl_h[i].val[0] = vld1_u8(high + i*16);
      tbl_h[i].val[1] = vld1_u8(high + i*16 + 8);
  }

  loset = vdup_n_u8(0xf);

  while (dst < d_end) {
      va = vld1q_u16(src);

      vb = vmovn_u16(va);
      vc = vshrn_n_u16(va, 8);

      rl = vtbl2_u8(tbl_l[0], vand_u8(vb, loset));
      rh = vtbl2_u8(tbl_h[0], vand_u8(vb, loset));
      vb = vshr_n_u8(vb, 4);
      rl = veor_u8(rl, vtbl2_u8(tbl_l[2], vand_u8(vc, loset)));
      rh = veor_u8(rh, vtbl2_u8(tbl_h[2], vand_u8(vc, loset)));
      vc = vshr_n_u8(vc, 4);
      rl = veor_u8(rl, vtbl2_u8(tbl_l[1], vb));
      rh = veor_u8(rh, vtbl2_u8(tbl_h[1], vb));
      rl = veor_u8(rl, vtbl2_u8(tbl_l[3], vc));
      rh = veor_u8(rh, vtbl2_u8(tbl_h[3], vc));

      r  = vmovl_u8(rl);
      r  = vorrq_u16(r, vshll_n_u8(rh, 8));

      if (xor) {
          va = vld1q_u16(dst);
          r = veorq_u16(r, va);
      }
      vst1q_u16(dst, r);

      src += 8;
      dst += 8;
  }
}

static
inline
void
neon_w16_split_4_altmap_multiply_region(gf_t *gf, uint8_t *src,
                                        uint8_t *dst, uint8_t *d_end,
                                        uint8_t *tbl, gf_val_32_t val,
                                        int xor)
{
  unsigned i;
  uint8_t *high = tbl + 4 * 16;
  uint8x8_t vh0, vh1, vl0, vl1, r0, r1, r2, r3;
  uint8x8_t loset;

  uint8x8x2_t tbl_h[4], tbl_l[4];
  for (i = 0; i < 4; i++) {
      tbl_l[i].val[0] = vld1_u8(tbl + i*16);
      tbl_l[i].val[1] = vld1_u8(tbl + i*16 + 8);
      tbl_h[i].val[0] = vld1_u8(high + i*16);
      tbl_h[i].val[1] = vld1_u8(high + i*16 + 8);
  }

  loset = vdup_n_u8(0xf);

  while (dst < d_end) {
      vh0 = vld1_u8(src);
      vh1 = vld1_u8(src + 8);
      vl0 = vld1_u8(src + 16);
      vl1 = vld1_u8(src + 24);

      r0 = vtbl2_u8(tbl_l[0], vand_u8(vh0, loset));
      r1 = vtbl2_u8(tbl_h[0], vand_u8(vh1, loset));
      r2 = vtbl2_u8(tbl_l[2], vand_u8(vl0, loset));
      r3 = vtbl2_u8(tbl_h[2], vand_u8(vl1, loset));

      vh0 = vshr_n_u8(vh0, 4);
      vh1 = vshr_n_u8(vh1, 4);
      vl0 = vshr_n_u8(vl0, 4);
      vl1 = vshr_n_u8(vl1, 4);

      r0 = veor_u8(r0, vtbl2_u8(tbl_l[1], vh0));
      r1 = veor_u8(r1, vtbl2_u8(tbl_h[1], vh1));
      r2 = veor_u8(r2, vtbl2_u8(tbl_l[3], vl0));
      r3 = veor_u8(r3, vtbl2_u8(tbl_h[3], vl1));

      if (xor) {
          vh0 = vld1_u8(dst);
          vh1 = vld1_u8(dst + 8);
          vl0 = vld1_u8(dst + 16);
          vl1 = vld1_u8(dst + 24);
          r0  = veor_u8(r0, vh0);
          r1  = veor_u8(r1, vh1);
          r2  = veor_u8(r2, vl0);
          r3  = veor_u8(r3, vl1);
      }
      vst1_u8(dst,      r0);
      vst1_u8(dst +  8, r1);
      vst1_u8(dst + 16, r2);
      vst1_u8(dst + 24, r3);

      src += 32;
      dst += 32;
  }
}
#endif /* ARCH_AARCH64 */

static
inline
void
neon_w16_split_4_16_lazy_multiply_region(gf_t *gf, void *src, void *dest,
                                         gf_val_32_t val, int bytes, int xor,
                                         int altmap)
{
  gf_region_data rd;
  unsigned i, j;
  uint64_t c, prod;
  uint8_t tbl[2 * 4 * 16];
  uint8_t *high = tbl + 4 * 16;

  if (val == 0) { gf_multby_zero(dest, bytes, xor); return; }
  if (val == 1) { gf_multby_one(src, dest, bytes, xor); return; }

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 16; j++) {
      c = (j << (i*4));
      prod = gf->multiply.w32(gf, c, val);
      tbl[i*16 + j]  = prod & 0xff;
      high[i*16 + j] = prod >> 8;
    }
  }

  gf_set_region_data(&rd, gf, src, dest, bytes, val, xor, 32);
  gf_do_initial_region_alignment(&rd);

  if (altmap) {
    uint8_t *s8   = rd.s_start;
    uint8_t *d8   = rd.d_start;
    uint8_t *end8 = rd.d_top;
    if (xor)
      neon_w16_split_4_altmap_multiply_region(gf, s8, d8, end8, tbl, val, 1);
    else
      neon_w16_split_4_altmap_multiply_region(gf, s8, d8, end8, tbl, val, 0);
  } else {
    uint16_t *s16   = rd.s_start;
    uint16_t *d16   = rd.d_start;
    uint16_t *end16 = rd.d_top;
    if (xor)
      neon_w16_split_4_multiply_region(gf, s16, d16, end16, tbl, val, 1);
    else
      neon_w16_split_4_multiply_region(gf, s16, d16, end16, tbl, val, 0);
  }

  gf_do_final_region_alignment(&rd);
}

static
void
gf_w16_split_4_16_lazy_multiply_region_neon(gf_t *gf, void *src, void *dest,
                                            gf_val_32_t val, int bytes, int xor)
{
  neon_w16_split_4_16_lazy_multiply_region(gf, src, dest, val, bytes, xor, 0);
}

static
void
gf_w16_split_4_16_lazy_altmap_multiply_region_neon(gf_t *gf, void *src,
                                                   void *dest,
                                                   gf_val_32_t val, int bytes,
                                                   int xor)
{
  neon_w16_split_4_16_lazy_multiply_region(gf, src, dest, val, bytes, xor, 1);
}


void gf_w16_neon_split_init(gf_t *gf)
{
  gf_internal_t *h = (gf_internal_t *) gf->scratch;

  if (h->region_type & GF_REGION_ALTMAP)
    gf->multiply_region.w32 = gf_w16_split_4_16_lazy_altmap_multiply_region_neon;
  else
    gf->multiply_region.w32 = gf_w16_split_4_16_lazy_multiply_region_neon;
}
