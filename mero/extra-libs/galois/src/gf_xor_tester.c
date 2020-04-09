/* gf_xor_tester.c
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
#include <sys/time.h>
#include <galois/galois.h>

#define BUFSIZE (50000)
#define S_BUFSIZE (4096)
#define DEFAULT 1
#define MULT 2
#define LOG 3
#define SHIFT 4

void usage(char *s)
{
  fprintf(stderr, "usage: gf_xor_tester\n");
  if (s != NULL) fprintf(stderr, "%s\n", s);
  exit(1);
}

main(int argc, char **argv)
{
  int i, x;
  struct timeval t1, t2;
  struct timezone tz;
  long t0, now;
  double total, tsec;
  unsigned char *cb1, *cb2, *cb3;

  if (argc != 1) usage(NULL);

  t0 = time(0);
  srand48(t0); 
  printf("XOR Tester\n");
  printf("Seeding random number generator with %u\n", t0);
  
  cb1 = (unsigned char *) malloc(sizeof(unsigned char) * BUFSIZE);
  cb2 = (unsigned char *) malloc(sizeof(unsigned char) * BUFSIZE);
  cb3 = (unsigned char *) malloc(sizeof(unsigned char) * BUFSIZE);
  for (i = 0; i < BUFSIZE; i++) cb1[i] = lrand48() % 256;
  for (i = 0; i < BUFSIZE; i++) cb2[i] = lrand48() % 256;

  galois_region_xor((char *) cb1, (char *) cb2, (char *) cb3, BUFSIZE);
  for (i = 0; i < BUFSIZE; i++) {
    if (cb3[i] != (cb1[i] ^ cb2[i])) {
      fprintf(stderr, "Failed test: %u ^ %u = %u.  Should be %u\n",
        cb1[i], cb2[i], cb3[i], (cb1[i] ^ cb2[i]));
      exit(1);
    }
  }
      
  printf("Passed correctness test -- doing 10-second timing\n");

  t0 = time(0);
  gettimeofday(&t1, &tz);
  total = 0;
  while (time(0) - t0 < 10) {
    for (x = 0; x < 50; x++) {
      galois_region_xor((char *) cb1, (char *) cb2, (char *) cb3, BUFSIZE);
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
  printf("%.5lf Megabytes of XORs per second\n", total/tsec/1024.0/1024.0);
  exit(0);
}
