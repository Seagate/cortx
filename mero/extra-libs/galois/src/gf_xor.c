/* gf_xor.c
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

main( int argc, char **argv) 
{
  unsigned int n, m, i;

  if (argc == 1) {
    fprintf(stderr, "usage: gf_xor n1 n2 ...\n");
    exit(1);
  }

  m = 0;
  for (i = 1; i < argc; i++) {
    sscanf(argv[i], "%u", &n);
    m = m ^ n;
  }
  printf("%u\n", m);
  exit(0);
}
