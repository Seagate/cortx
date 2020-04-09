/* gf_ilog.c
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

main(int argc, char **argv)
{
  unsigned int x, w;

  if (argc != 3) {
    fprintf(stderr, "usage: gf_ilog x w - returns the discrete inverse log if x in GF(2^w)\n");
    exit(1);
  }

  sscanf(argv[1], "%u", &x);
  w = atoi(argv[2]);

  if (w < 1 || w > 32) { fprintf(stderr, "Bad w\n"); exit(1); }

  if (w < 32 && x >= (1 << w)-1) { fprintf(stderr, "x must be in [0,%d]\n", (1 << w)-2); exit(1); }

  printf("%u\n", galois_ilog(x, w));
  exit(0);
}
