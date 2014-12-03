/*
 * @file
 * @author Piotr Krysik <pkrysik@stud.elka.pw.edu.pl>
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <stdio.h>

#include "a5-1-2.h"


void decrypt(const unsigned char *burst_binary, byte *KC, unsigned char *decrypted_data, unsigned FN, int ul)
{
  byte AtoB[114];
  int i;

  /* KC is all zero: no decryption */

  if(KC[0] == 0 && KC[1] == 0 && KC[2] == 0 && KC[3] == 0 &
     KC[4] == 0 && KC[5] == 0 && KC[6] == 0 && KC[7] == 0) {
    for (i = 0; i < 148; i++) {
      decrypted_data[i] = burst_binary[i];
    }
    return;
  }

  keysetup(KC, FN);
  runA51(AtoB);
  if (ul)
	runA51(AtoB);

  for (i = 0; i < 57; i++) {
    decrypted_data[i] = AtoB[i] ^ burst_binary[i];
  }

  /* stealing bits and midamble */
  for (i = 57; i < 85; i++) {
    decrypted_data[i] = burst_binary[i];
  }

  for (i = 0; i < 57; i++) {
    decrypted_data[i+85] = AtoB[i+57] ^ burst_binary[i+85];
  }

}

void read_key(char *key, unsigned char *dst)
{
//	printf("Key: '%s'\n", key);
	sscanf(key, "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx", dst, dst+1, dst+2, dst+3, dst+4, dst+5, dst+6, dst+7);

}
