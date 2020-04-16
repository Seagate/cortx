/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Abrarahmed Momin  <abrar.habib@seagate.com>
 * Original creation date: 23th-June-2016
 */

#include "s3_fi_common.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef ENABLE_FAULT_INJECTION

enum {
  S3_FP_ARRAY_SIZE = 64 * 1024,
};

static s3_fp *s3_fi_states[S3_FP_ARRAY_SIZE];
static uint32_t s3_fi_states_free_idx;

#define S3_FP_FETCH(tag)         \
  {                              \
    fp_ptr = s3_fp_find(tag);    \
    if (fp_ptr == NULL) {        \
      fp_ptr = s3_fp_alloc(tag); \
      M0_ASSERT(fp_ptr != NULL); \
    }                            \
  }

s3_fp *s3_fp_alloc(const char *tag) {
  s3_fp *fp_ptr;
  char *tag_ptr;

  /* alloc mem */
  fp_ptr = (s3_fp *)calloc(1, sizeof(s3_fp));
  M0_ASSERT(fp_ptr != NULL);
  tag_ptr = strdup(tag);
  M0_ASSERT(tag_ptr != NULL);
  fp_ptr->fp_tag = tag_ptr;
  fp_ptr->fp_func = S3_FI_FUNC_NAME;
  fp_ptr->fp_module = S3_MODULE_NAME;

  s3_fi_states[s3_fi_states_free_idx++] = fp_ptr;
  return fp_ptr;
}

static s3_fp *s3_fp_find(const char *tag) {
  int i;

  for (i = 0; i < s3_fi_states_free_idx; i++) {
    if (strcmp(s3_fi_states[i]->fp_tag, tag) == 0) return s3_fi_states[i];
  }
  return NULL;
}

int s3_fi_is_enabled(const char *tag) {
  s3_fp *fp_ptr;
  S3_FP_FETCH(tag);
  if (fp_ptr->fp_state == NULL) {
    m0_fi_register(fp_ptr);
    M0_ASSERT(fp_ptr->fp_state != NULL);
  }
  return m0_fi_enabled(fp_ptr->fp_state);
}

void s3_fi_enable(const char *tag) {
  s3_fp *fp_ptr;
  S3_FP_FETCH(tag);
  m0_fi_enable(fp_ptr->fp_func, fp_ptr->fp_tag);
}

void s3_fi_enable_once(const char *tag) {
  s3_fp *fp_ptr;
  S3_FP_FETCH(tag);
  m0_fi_enable_once(fp_ptr->fp_func, fp_ptr->fp_tag);
}

void s3_fi_enable_random(const char *tag, uint32_t p) {
  s3_fp *fp_ptr;
  S3_FP_FETCH(tag);
  m0_fi_enable_random(fp_ptr->fp_func, fp_ptr->fp_tag, p);
}

void s3_fi_enable_each_nth_time(const char *tag, uint32_t n) {
  s3_fp *fp_ptr;
  S3_FP_FETCH(tag);
  m0_fi_enable_each_nth_time(fp_ptr->fp_func, fp_ptr->fp_tag, n);
}

void s3_fi_enable_off_n_on_m(const char *tag, uint32_t n, uint32_t m) {
  s3_fp *fp_ptr;
  S3_FP_FETCH(tag);
  m0_fi_enable_off_n_on_m(fp_ptr->fp_func, fp_ptr->fp_tag, n, m);
}

void s3_fi_disable(const char *fp_tag) {
  s3_fp *fp_ptr;
  fp_ptr = s3_fp_find(fp_tag);
  if (fp_ptr == NULL) return;
  /* use with caution, non existent FP causes an ASSERTION */
  m0_fi_disable(fp_ptr->fp_func, fp_ptr->fp_tag);
}

#endif /* ENABLE_FAULT_INJECTION */
