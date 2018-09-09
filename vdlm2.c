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
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "vdlm2.h"
#include "acars.h"
#include "crc.h"

#define PPPINITFCS16    0xffff	/* Initial FCS value */
#define PPPGOODFCS16    0xf0b8	/* Good final FCS value */

static pthread_mutex_t blkmtx;
static pthread_cond_t blkwcd;
static msgblk_t *blkq_s = NULL;
static msgblk_t *blkq_e = NULL;

static void check_frame(msgblk_t * blk, unsigned char *hdata, int l)
{
	int i;
	unsigned short crc;

	if (l < 13) {
		if (verbose > 2)
			fprintf(logfd, "#%d error too short\n", blk->chn + 1);
		return;
	}

	/* crc */
	crc = PPPINITFCS16;
	for (i = 1; i < l - 1; i++) {
		update_crc(crc, hdata[i]);
	}
	if (crc != PPPGOODFCS16) {
		if (verbose > 2)
			fprintf(logfd, "#%d error crc\n", blk->chn + 1);
		return;
	}

	out(blk, hdata, l);
}

static int set_eras(int *eras_pos, int nb)
{
	int nbera = 0;

	if (nb <= 67) {
		nbera = 2;
		eras_pos[0] = 253;
		eras_pos[1] = 254;
	}
	if (nb <= 30) {
		nbera = 4;
		eras_pos[0] = 251;
		eras_pos[1] = 252;
		eras_pos[2] = 253;
		eras_pos[3] = 254;
	}

	return nbera;
}

static void *blk_thread(void *arg)
{
	do {
		msgblk_t *blk;
		int i, n, k, r, s, t;
		unsigned char hdata[65 * 249];
		int nbera;
		int eras_pos[6];

		pthread_mutex_lock(&blkmtx);
		while (blkq_e == NULL)
			pthread_cond_wait(&blkwcd, &blkmtx);

		blk = blkq_e;
		blkq_e = blk->prev;
		if (blkq_e == NULL)
			blkq_s = NULL;
		pthread_mutex_unlock(&blkmtx);

		k = s = t = 0;
		hdata[k] = 0;
		for (r = 0; r < blk->nbrow; r++) {
			int by;

			if (r == blk->nbrow - 1) {
				by = blk->nlbyte;
				nbera = set_eras(eras_pos, by);
			} else {
				by = 249;
				nbera = 0;
			}

			/* reed solomon FEC */
			rs(blk->data[r], eras_pos, nbera);

			/* HDLC bit un stuffing */
			for (i = 0; i < by; i++) {

				for (n = 0; n < 8; n++) {
					if (blk->data[r][i] & (1 << n)) {
						hdata[k] |= 1 << s;
						t++;
					} else {
						if (t == 5) {
							t = 0;
							continue;
						}
						t = 0;
					}
					s++;
					if (s == 8) {
						s = 0;
						if (hdata[k] == 0x7e) {
							if (k == 0) {
								k++;
								hdata[k] = 0;
							} else if (k == 1) {
								hdata[1] = 0;
							} else if (k > 1) {
								check_frame(blk, hdata, k + 1);
								k++;
								hdata[k] = 0;
							}
						} else if (k > 0) {
							k++;
							hdata[k] = 0;
						}
					}
				}
			}

		}

		free(blk);
	} while (1);

	return NULL;
}

int initVdlm2(channel_t * ch)
{
	pthread_t th;

	ch->state = WSYNC;
	ch->blk = calloc(sizeof(msgblk_t), 1);
	ch->blk->chn = ch->chn;
	ch->blk->Fr = ch->Fr;

	if (ch->chn == 0) {
		pthread_mutex_init(&blkmtx, NULL);
		pthread_cond_init(&blkwcd, NULL);

		pthread_create(&th, NULL, blk_thread, NULL);
	}

	return 0;
}

void stopVdlm2(void)
{
	int c;
	for (c = 0; blkq_e && c < 5; c++)
		sleep(1);
}

void decodeVdlm2(channel_t * ch)
{
	pthread_mutex_lock(&blkmtx);
	ch->blk->prev = NULL;
	if (blkq_s)
		blkq_s->prev = ch->blk;
	blkq_s = ch->blk;
	if (blkq_e == NULL)
		blkq_e = blkq_s;
	pthread_cond_signal(&blkwcd);
	pthread_mutex_unlock(&blkmtx);

	ch->blk = calloc(sizeof(msgblk_t), 1);
	ch->blk->chn = ch->chn;
	ch->blk->Fr = ch->Fr;

	return;
}
