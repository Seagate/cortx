/* gf_basic_tester.c
 * James S. Plank

Galois.tar - Fast Galois Field Arithmetic Library in C/C++
Copright (C) 2007 James S. Plank

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

James S. Plank
Department of Computer Science
University of Tennessee
Knoxville, TN 37996
plank@cs.utk.edu

 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <galois/galois.h>
#include <sys/time.h>

#define BUFSIZE (50000)
#define S_BUFSIZE (4096)
#define DEFAULT 1
#define MULT 2
#define LOG 3
#define SHIFT 4
#define SPLITW8 5

extern int printer;
int printer;

void usage(char *s)
{
  fprintf(stderr, "usage: gf_basic_tester w default|multtable|logtable|shift|splitw8 ntrials-for-correctness\n");
  if (s != NULL) fprintf(stderr, "%s\n", s);
  exit(1);
}

main(int argc, char **argv)
{
  unsigned int w, alltype, nt, i, x, y, z, top, elt, q1, q2;
  unsigned int *b1, *b2;
  unsigned int *ib1, *ib2, *ib3;
  unsigned short *sb1, *sb2, *sb3;
  struct timeval t1, t2;
  struct timezone tz;
  long t0, now;
  double total, tsec;
  unsigned char *cb1, *cb2, *cb3;
  int mtype;

  if (argc != 4) usage(NULL);

  w = atoi(argv[1]);
  if (w <= 0 || w > 32) usage("w must be in [1,32]\n");

  
  if (strcmp(argv[2], "default") == 0) {
    mtype = DEFAULT;
  } else if (strcmp(argv[2], "multtable") == 0) {
    mtype = MULT;
    if (galois_create_mult_tables(w) != 0) {
      printf("Cannot create multiplication tables for w = %d\n", w);
      exit(0);
    }
  } else if (strcmp(argv[2], "logtable") == 0) {
    if (galois_create_log_tables(w) != 0) {
      printf("Cannot create log tables for w = %d\n", w);
      exit(0);
    }
    mtype = LOG;
  } else if (strcmp(argv[2], "shift") == 0) {
    mtype = SHIFT;
  } else if (strcmp(argv[2], "splitw8") == 0) {
    mtype = SPLITW8;
    if (w != 32) {
      fprintf(stderr, "Splitw8 is only valid for w = 32\n");
      exit(1);
    }
    if (galois_create_split_w8_tables() != 0) {
      printf("Cannot create split_w8 tables for w = %d\n", w);
      exit(0);
    }
  } else {
    usage("arg 2 must be default, multtable, logtable, shift or splitw8\n");
  }
 
  nt = atoi(argv[3]);

  t0 = time(0);
  srand48(t0); 
  printf("W: %d\n", w);
  printf("Method: %s\n", argv[2]);
  printf("Seeding random number generator with %u\n", t0);
  
  top = (1 << w);

  b1 = (unsigned int *) malloc(sizeof(unsigned int) * S_BUFSIZE);
  b2 = (unsigned int *) malloc(sizeof(unsigned int) * S_BUFSIZE);
  if (w < 32 ) {
    for (i = 0; i < S_BUFSIZE; i++) b1[i] = lrand48() % top;
    for (i = 0; i < S_BUFSIZE; i++) b2[i] = lrand48() % top;
  } else {
    for (i = 0; i < S_BUFSIZE; i++) b1[i] = lrand48();
    for (i = 0; i < S_BUFSIZE; i++) b2[i] = lrand48();
  }

  if (w == 8 || w == 16 || w == 32) {
    cb1 = (unsigned char *) malloc(sizeof(unsigned char) * BUFSIZE);
    cb2 = (unsigned char *) malloc(sizeof(unsigned char) * BUFSIZE);
    cb3 = (unsigned char *) malloc(sizeof(unsigned char) * BUFSIZE);
    for (i = 0; i < BUFSIZE; i++) cb1[i] = lrand48() % 256;
    for (i = 0; i < BUFSIZE; i++) cb2[i] = lrand48() % 256;
  }

  printf("Doing %u trials for single-operation correctness.\n", nt);
  for (i = 0; i < nt; i++) {
    if (w < 32) {
      x = lrand48()%(top-1)+1;
      y = lrand48()%(top-1)+1;
    } else {
      for (x = 0; x == 0; x = lrand48()) ;
      for (y = 0; y == 0; y = lrand48()) ;
    }
    switch (mtype) {
      case DEFAULT: z = galois_single_multiply(x, y, w); 
                    q1 = galois_single_divide(z, x, w); 
                    q2 = galois_single_divide(z, y, w); 
                    break;
      case MULT:    z = galois_multtable_multiply(x, y, w);
                    q1 = galois_multtable_divide(z, x, w); 
                    q2 = galois_multtable_divide(z, y, w); 
                    break;
      case LOG:     z = galois_logtable_multiply(x, y, w);
                    q1 = galois_logtable_divide(z, x, w); 
                    q2 = galois_logtable_divide(z, y, w); 
                    break;
      case SHIFT:   z = galois_shift_multiply(x, y, w);
                    q1 = galois_shift_divide(z, x, w); 
                    q2 = galois_shift_divide(z, y, w); 
                    break;
      case SPLITW8: z = galois_split_w8_multiply(x, y);
                    q1 = galois_shift_divide(z, x, w); 
                    q2 = galois_shift_divide(z, y, w); 
                    break;
    }
    if (q1 != y) {
      fprintf(stderr, "Failed test: %u * %u = %u, but %u / %u = %u\n",
         x, y, z, z, x, q1);
      exit(1);
    }
    if (q2 != x) {
      fprintf(stderr, "Failed test: %u * %u = %u, but %u / %u = %u\n",
         y, x, z, z, y, q2);
      exit(1);
    }
    z = galois_inverse(x, w);
    q1 = galois_inverse(z, w);
    if (q1 != x) {
      fprintf(stderr, "Failed test: gf_inverse(%u) = %u, but gf_inverse(%u) = %u\n",
         x, z, z, q1);
      exit(1);
    }
  }
  printf("Passed Single-Operations Correctness Tests.\n");
  printf("\n");

  if (w == 8 && mtype == DEFAULT) {
    printf("Doing galois_w08_region_multiply correctness test.\n");
    for (x = 0; x < 10; x++) {

      /* First, test with r2 specified, and add = 0 */
      for (elt = 0; elt == 0; elt = lrand48()%top);
      galois_w08_region_multiply((char *) cb1, elt, BUFSIZE, (char *) cb3, 0);
      for (i = 0; i < BUFSIZE; i++) {
        if (galois_single_multiply(cb1[i], elt, w) != cb3[i]) {
          printf("Failed test (r2 != NULL, add == 0): %u * %u = %u, but it is %u in cb3\n", 
              cb1[i], elt, galois_single_multiply(cb1[i], elt, w), cb3[i]);
          exit(1);
        }
      }

      /* Next, test with r2 = NULL */
      for (elt = 0; elt == 0; elt = lrand48()%top);
      galois_w08_region_multiply((char *) cb1, elt, BUFSIZE, (char *) cb3, 0);
      elt = galois_single_divide(1, elt, w);
      galois_w08_region_multiply((char *) cb3, elt, BUFSIZE, NULL, 0);
      for (i = 0; i < BUFSIZE; i++) {
        if (cb1[i] != cb3[i]) {
          printf("Failed test (r2 == NULL): %u != %u\n", cb1[i], cb3[i]);
          exit(1);
        }
      }

      /* Finally, test with r2 specified, and add = 1 */

      for (elt = 0; elt == 0; elt = lrand48()%top);
      for (i = 0; i < BUFSIZE; i++) cb3[i] = cb2[i];
      galois_w08_region_multiply((char *) cb1, elt, BUFSIZE, (char *) cb3, 1);
      for (i = 0; i < BUFSIZE; i++) {
        if (cb3[i] != (cb2[i] ^ galois_single_multiply(cb1[i], elt, w))) {
          printf("Failed test (r2 != NULL && add == 1): (%u * %u) ^ %u = %u.  cb3 = %u\n", 
               cb1[i], elt, cb2[i], (galois_single_multiply(cb1[i], elt, w)^cb2[i]), cb3[i]);
          exit(1);
        }
      }
    }
    galois_w08_region_multiply((char *) cb1, 0, BUFSIZE, (char *) cb3, 0);
    for (i = 0; i < BUFSIZE; i++) {
      if (cb3[i] != 0) {
        printf("Failed multiply by zero test.  Byte %d = %d\n", i, cb3[i]);
        exit(1);
      }
    }
    printf("Passed galois_w08_region_multiply correctness test.\n");
    printf("\n");
  }
  if (w == 16 && mtype == DEFAULT) {
    printf("Doing galois_w16_region_multiply correctness test.\n");
    sb1 = (unsigned short *) cb1;
    sb2 = (unsigned short *) cb2;
    sb3 = (unsigned short *) cb3;
    for (x = 0; x < 10; x++) {

      /* First, test with r2 specified, and add = 0 */
      for (elt = 0; elt == 0; elt = lrand48()%top);
      galois_w16_region_multiply((char *) sb1, elt, BUFSIZE, (char *) sb3, 0);
      for (i = 0; i < BUFSIZE/2; i++) {
        if (galois_single_multiply(sb1[i], elt, w) != sb3[i]) {
          printf("Failed test (r2 != NULL, add == 0). Word %u: %u * %u = %u, but it is %u in sb3\n", 
              i, sb1[i], elt, galois_single_multiply(sb1[i], elt, w), sb3[i]);
          exit(1);
        }
      }

      /* Next, test with r2 = NULL */
      for (elt = 0; elt == 0; elt = lrand48()%top);
      galois_w16_region_multiply((char *) sb1, elt, BUFSIZE, (char *) sb3, 0);
      elt = galois_single_divide(1, elt, w);
      galois_w16_region_multiply((char *) sb3, elt, BUFSIZE, NULL, 0);
      for (i = 0; i < BUFSIZE/2; i++) {
        if (sb1[i] != sb3[i]) {
          printf("Failed test (r2 == NULL): %u != %u\n", sb1[i], sb3[i]);
          exit(1);
        }
      }

      /* Finally, test with r2 specified, and add = 1 */

      for (elt = 0; elt == 0; elt = lrand48()%top);
      for (i = 0; i < BUFSIZE/2; i++) sb3[i] = sb2[i];
      galois_w16_region_multiply((char *) sb1, elt, BUFSIZE, (char *) sb3, 1);
      for (i = 0; i < BUFSIZE/2; i++) {
        if (sb3[i] != (sb2[i] ^ galois_single_multiply(sb1[i], elt, w))) {
          printf("Failed test (r2 != NULL && add == 1) Byte %u: (%u * %u) ^ %u = %u.  sb3 = %u\n", 
               i, sb1[i], elt, sb2[i], (galois_single_multiply(sb1[i], elt, w)^sb2[i]), sb3[i]);
          exit(1);
        }
      }
    }
    galois_w16_region_multiply((char *) sb1, 0, BUFSIZE, (char *) sb3, 0);
    for (i = 0; i < BUFSIZE/2; i++) {
      if (sb3[i] != 0) {
        printf("Failed multiply by zero test.  Byte %d = %d\n", i, sb3[i]);
        exit(1);
      }
    }
    printf("Passed galois_w16_region_multiply correctness test.\n");
    printf("\n");
  }

  if (w == 32 && mtype == DEFAULT) {
    printf("Doing galois_w32_region_multiply correctness test.\n");
    ib1 = (unsigned int *) cb1;
    ib2 = (unsigned int *) cb2;
    ib3 = (unsigned int *) cb3;
    for (x = 0; x < 10; x++) {
      /* First, test with r2 specified, and add = 0 */
      for (elt = 0; elt == 0; elt = lrand48());
      galois_w32_region_multiply((char *) ib1, elt, BUFSIZE, (char *) ib3, 0);
      for (i = 0; i < BUFSIZE/4; i++) {
        printer = 1;
        if (galois_single_multiply(ib1[i], elt, w) != ib3[i]) {
          printf("Failed test (r2 != NULL, add == 0). Word %u: %u * %u = %u, but it is %u in ib3\n", 
              i, ib1[i], elt, galois_single_multiply(ib1[i], elt, w), ib3[i]);
          exit(1);
        }
      }

      /* Next, test with r2 = NULL */
      for (elt = 0; elt == 0; elt = lrand48());
      galois_w32_region_multiply((char *) ib1, elt, BUFSIZE, (char *) ib3, 0);
      elt = galois_single_divide(1, elt, w);
      galois_w32_region_multiply((char *) ib3, elt, BUFSIZE, NULL, 0);
      for (i = 0; i < BUFSIZE/4; i++) {
        if (ib1[i] != ib3[i]) {
          printf("Failed test (r2 == NULL): Byte %d (0x%x): %u != %u\n", i, ib3+i, ib1[i], ib3[i]);
          exit(1);
        }
      }

      /* Finally, test with r2 specified, and add = 1 */

      for (elt = 0; elt == 0; elt = lrand48());
      for (i = 0; i < BUFSIZE/4; i++) ib3[i] = ib2[i];
      galois_w32_region_multiply((char *) ib1, elt, BUFSIZE, (char *) ib3, 1);
      for (i = 0; i < BUFSIZE/4; i++) {
        if (ib3[i] != (ib2[i] ^ galois_single_multiply(ib1[i], elt, w))) {
          printf("Failed test (r2 != NULL && add == 1) Byte %u: (%u * %u) ^ %u = %u.  ib3 = %u\n", 
               i, ib1[i], elt, ib2[i], (galois_single_multiply(ib1[i], elt, w)^ib2[i]), ib3[i]);
          exit(1);
        }
      }
    }
    galois_w32_region_multiply((char *) ib1, 0, BUFSIZE, (char *) ib3, 0);
    for (i = 0; i < BUFSIZE/4; i++) {
      if (ib3[i] != 0) {
        printf("Failed multiply by zero test.  Byte %d = %d\n", i, ib3[i]);
        exit(1);
      }
    }
    printf("Passed galois_w32_region_multiply correctness test.\n");
    printf("\n");
  }


  printf("Speed Test #1: 10 Seconds of Multiply operations\n");
  t0 = time(0);
  gettimeofday(&t1, &tz);
  total = 0;
  while (time(0) - t0 < 10) {
    switch (mtype) {
      case DEFAULT: for (i = 0; i < S_BUFSIZE; i++) galois_single_multiply(b1[i], b2[i], w); break;
      case MULT:    for (i = 0; i < S_BUFSIZE; i++) galois_multtable_multiply(b1[i], b2[i], w); break;
      case LOG:     for (i = 0; i < S_BUFSIZE; i++) galois_logtable_multiply(b1[i], b2[i], w); break;
      case SHIFT:   for (i = 0; i < S_BUFSIZE; i++) galois_shift_multiply(b1[i], b2[i], w); break;
      case SPLITW8:   for (i = 0; i < S_BUFSIZE; i++) galois_split_w8_multiply(b1[i], b2[i]); break;
    }
    total++;
  }
  gettimeofday(&t2, &tz);
    
  tsec = 0;
  tsec += t2.tv_usec;
  tsec -= t1.tv_usec;
  tsec /= 1000000.0;
  tsec += t2.tv_sec;
  tsec -= t1.tv_sec;
  total *= S_BUFSIZE;
  printf("Speed Test #1: %.5lf Mega Multiplies per second\n", total/tsec/1024.0/1024.0);
  
  printf("Speed Test #2: 10 Seconds of Divide operations\n");
  t0 = time(0);
  gettimeofday(&t1, &tz);
  total = 0;
  while (time(0) - t0 < 10) {
    switch (mtype) {
      case DEFAULT: for (i = 0; i < S_BUFSIZE; i++) galois_single_divide(b1[i], b2[i], w); break;
      case MULT:    for (i = 0; i < S_BUFSIZE; i++) galois_multtable_divide(b1[i], b2[i], w); break;
      case LOG:     for (i = 0; i < S_BUFSIZE; i++) galois_logtable_divide(b1[i], b2[i], w); break;
      case SHIFT:   for (i = 0; i < S_BUFSIZE; i++) galois_shift_divide(b1[i], b2[i], w); break;
      case SPLITW8:   for (i = 0; i < S_BUFSIZE; i++) galois_shift_divide(b1[i], b2[i], w); break;
    }
    total++;
  }
  gettimeofday(&t2, &tz);
    
  tsec = 0;
  tsec += t2.tv_usec;
  tsec -= t1.tv_usec;
  tsec /= 1000000.0;
  tsec += t2.tv_sec;
  tsec -= t1.tv_sec;
  total *= S_BUFSIZE;
  printf("Speed Test #2: %.5lf Mega Divides per second\n", total/tsec/1024.0/1024.0);
  

  if (mtype != DEFAULT) exit(0);

  printf("\n");

  if (w == 8) {
    printf("Doing 10 seconds worth of region_multiplies - Three tests:\n");
    printf("   Test 0: Overwrite initial region\n");
    printf("   Test 1: Products to new region\n");
    printf("   Test 2: XOR products into second region\n\n");
    for (i = 0; i < 3; i++) {
      t0 = time(0);
      gettimeofday(&t1, &tz);
      total = 0;
      while (time(0) - t0 < 10) {
        for (x = 0; x < 10; x++) {
          for (elt = 0; elt == 0; elt = lrand48()%top) ;
          if (i == 0) {
            galois_w08_region_multiply((char *) cb1, elt, BUFSIZE, NULL, 0); 
          } else if (i == 1) {
            galois_w08_region_multiply((char *) cb1, elt, BUFSIZE, (char *) cb3, 0); 
          } else {
            galois_w08_region_multiply((char *) cb1, elt, BUFSIZE, (char *) cb3, 1); 
          }
          total += BUFSIZE;
        }
      }
      gettimeofday(&t2, &tz);
    
      tsec = 0;
      tsec += t2.tv_usec;
      tsec -= t1.tv_usec;
      tsec /= 1000000.0;
      tsec += t2.tv_sec;
      tsec -= t1.tv_sec;
      printf("   Test %u: %.5lf Megabytes of Multiplies per second\n", i, total/tsec/1024.0/1024.0);
    }
    
    
  }
  if (w == 16) {
    printf("Doing 10 seconds worth of region_multiplies - Three tests:\n");
    printf("   Test 0: Overwrite initial region\n");
    printf("   Test 1: Products to new region\n");
    printf("   Test 2: XOR products into second region\n\n");
    
    for (i = 0; i < 3; i++) {
      t0 = time(0);
      gettimeofday(&t1, &tz);
      total = 0;
      while (time(0) - t0 < 10) {
        for (x = 0; x < 10; x++) {
          for (elt = 0; elt == 0; elt = lrand48()%top) ;
          if (i == 0) {
            galois_w16_region_multiply((char *) sb1, elt, BUFSIZE, NULL, 0); 
          } else if (i == 1) {
            galois_w16_region_multiply((char *) sb1, elt, BUFSIZE, (char *) sb3, 0); 
          } else {
            galois_w16_region_multiply((char *) sb1, elt, BUFSIZE, (char *) sb3, 1); 
          }
          total += BUFSIZE;
        }
      }
      gettimeofday(&t2, &tz);
    
      tsec = 0;
      tsec += t2.tv_usec;
      tsec -= t1.tv_usec;
      tsec /= 1000000.0;
      tsec += t2.tv_sec;
      tsec -= t1.tv_sec;
      printf("   Test %u: %.5lf Megabytes of Multiplies per second\n", i, total/tsec/1024.0/1024.0);
    }
  }
  if (w == 32) {
    printf("Doing 10 seconds worth of region_multiplies - Three tests:\n");
    printf("   Test 0: Overwrite initial region\n");
    printf("   Test 1: Products to new region\n");
    printf("   Test 2: XOR products into second region\n\n");
    
    for (i = 0; i < 3; i++) {
      t0 = time(0);
      gettimeofday(&t1, &tz);
      total = 0;
      while (time(0) - t0 < 10) {
        for (x = 0; x < 10; x++) {
          for (elt = 0; elt == 0; elt = lrand48()) ;
          if (i == 0) {
            galois_w32_region_multiply((char *) ib1, elt, BUFSIZE, NULL, 0); 
          } else if (i == 1) {
            galois_w32_region_multiply((char *) ib1, elt, BUFSIZE, (char *) ib3, 0); 
          } else if (i == 2) {
            galois_w32_region_multiply((char *) ib1, elt, BUFSIZE, (char *) ib3, 1); 
          }
          total += BUFSIZE;
        }
      }
      gettimeofday(&t2, &tz);
    
      tsec = 0;
      tsec += t2.tv_usec;
      tsec -= t1.tv_usec;
      tsec /= 1000000.0;
      tsec += t2.tv_sec;
      tsec -= t1.tv_sec;
      printf("   Test %u: %.5lf Megabytes of Multiplies per second\n", i, total/tsec/1024.0/1024.0);
    }
  }
  exit(0);
}
