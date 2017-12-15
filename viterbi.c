/*
 *  Copyright (c) 2016 Thierry Leconte
 *
 *   
 *   This code is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define NBITS 25
#define NBSTATES 32
static double Pb[NBITS + 1][NBSTATES];
static int Bk[NBITS + 1][NBSTATES];
static int B[NBITS + 1][NBSTATES];

const int H[NBITS] = {
	0b00110, 0b00111, 0b01001, 0b01010, 0b01011,
	0b01100, 0b01110, 0b01111, 0b10001, 0b10011,
	0b10101, 0b10110, 0b11000, 0b11001, 0b11010,
	0b11011, 0b11100, 0b11101, 0b11110, 0b11111,
	0b10000, 0b01000, 0b00100, 0b00010, 0b00001
};

void viterbi_init(void)
{
	int s;
	Pb[0][0] = 1.0;
	for (s = 1; s < NBSTATES; s++)
		Pb[0][s] = 0;

}

void viterbi_add(float V, int n)
{
	int s;

	for (s = 0; s < NBSTATES; s++)
		Pb[n + 1][s] = 0;

	for (s = 0; s < NBSTATES; s++) {
		double np;
		int ns;

		if (Pb[n][s] == 0.0)
			continue;

		/* 1 */
		np = Pb[n][s] * V;
		ns = s ^ H[n];
		if (np > Pb[n + 1][ns]) {
			Pb[n + 1][ns] = np;
			Bk[n + 1][ns] = s;
			B[n + 1][ns] = 1;
		}
		/* 0 */
		np = Pb[n][s] * (1.0 - V);
		if (np > Pb[n + 1][s]) {
			Pb[n + 1][s] = np;
			Bk[n + 1][s] = s;
			B[n + 1][s] = 0;
		}

	}

}

float viterbi_end(unsigned int *bits)
{
	int n;
	int s;
	int b;

	s = 0;
	*bits = 0;
	b = 1;
	for (n = NBITS; n > 0; n--) {
		if (B[n][s])
			*bits |= b;
		s = Bk[n][s];
		b <<= 1;
	}
	return Pb[NBITS][0];
}

