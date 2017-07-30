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

#define PPPINITFCS16    0xffff  /* Initial FCS value */
#define PPPGOODFCS16    0xf0b8  /* Good final FCS value */

static pthread_mutex_t blkmtx;
static pthread_cond_t blkwcd;
static msgblk_t *blkq_s = NULL;
static msgblk_t *blkq_e = NULL;

void dumpdata(unsigned char *p, int len)
{
    int i;

    fprintf(logfd, "[ ");
    for (i = 0; i < len; i++)
        fprintf(logfd, "%02hhx ", p[i]);
    fprintf(logfd, "]\n");
}

static int set_eras(float *pr, int *eras_pos, int nb)
{
    int i, nbera = 0;
    float eras_pr[6];

    if (nb <= 67) {
        nbera = 2;
        eras_pos[0] = 253;
        eras_pos[1] = 254;
        eras_pr[0] = eras_pr[1] = 0;
    }
    if (nb <= 30) {
        nbera = 4;
        eras_pos[0] = 251;
        eras_pos[1] = 252;
        eras_pos[2] = 253;
        eras_pos[3] = 254;
        eras_pr[0] = eras_pr[1] = eras_pr[2] = eras_pr[3] = 0;
    }

    for (i = 0; i < nb; i++) {
        int k, l;

        if (pr[i] > 0.3)
            continue;

        k = 0;
        while (pr[i] > eras_pr[k] && k < nbera)
            k++;
        if (k == 6)
            continue;
        for (l = 5; l > k; l--) {
            eras_pr[l] = eras_pr[l - 1];
            eras_pos[l] = eras_pos[l - 1];
        }
        eras_pos[k] = i;
        eras_pr[k] = pr[i];
        if (k >= nbera)
            nbera = k + 1;
    }

    return nbera;
}

static void *blk_thread(void *arg)
{
    do {
        msgblk_t *blk;
        int r, i, n, k, s, t;
        unsigned short crc;
        unsigned char hdata[17000];
        int fec, nbera;
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
                by = blk->nbbyte;
            } else {
                by = 249;
            }
            nbera = set_eras(blk->pr[r], eras_pos, by);

            /* reed solomon FEC */
            fec = rs(blk->data[r], eras_pos, nbera);
            if (fec < 0)
                break;

            /* HDLC bit un stuffing */
            for (i = 0; i < by; i++) {
                int l;

                if (blk->nblst != 0 && r == blk->nbrow - 1 && i == blk->nbbyte - 1)
                    l = blk->nblst;
                else
                    l = 8;

                for (n = 0; n < l; n++) {
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
                        k++;
                        hdata[k] = 0;
                        s = 0;
                    }
                }
            }

        }

        if (verbose > 1)
            fprintf(logfd, "received len:%d\n", k);

        if (fec < 0) {
            if (verbose > 1)
                fprintf(logfd, "error fec\n");
            free(blk);
            continue;
        }

        if (k < 13) {
            if (verbose > 1)
                fprintf(logfd, "error too short\n");
            continue;
        }

        /* crc */
        crc = PPPINITFCS16;
        for (i = 1; i <= k - 2; i++) {
            update_crc(crc, hdata[i]);
        }
        if (crc != PPPGOODFCS16) {
            if (verbose > 1)
                fprintf(logfd, "error crc\n");
            free(blk);
            continue;
        }

        if (verbose) {
            int rep = hdata[5] & 2;
            unsigned int toaddr =
                (reversebits(hdata[1] >> 2, 6) << 21) | (reversebits(hdata[2] >> 1, 7) << 14) |
                (reversebits(hdata[3] >> 1, 7) << 7) | (reversebits(hdata[4] >> 1, 7));
            unsigned int fraddr =
                (reversebits(hdata[5] >> 2, 6) << 21) | (reversebits(hdata[6] >> 1, 7) << 14) |
                (reversebits(hdata[7] >> 1, 7) << 7) | (reversebits(hdata[8] >> 1, 7));

            fprintf(logfd, "%s from %s %06x to %06x\n", rep ? "response" : "command",
                    (hdata[1] & 2) ? "ground" : "airborne", fraddr & 0xffffff, toaddr & 0xffffff);

            fprintf(logfd, "Link Control : ");
            if (hdata[9] & 1) {
                if (hdata[9] & 2) {
                    fprintf(logfd, "U ");
                    fprintf(logfd, "%c ", (hdata[9] & 0x10) ? (rep ? 'F' : 'P') : '-');
                    fprintf(logfd, "%01x %01x\n", (hdata[9] >> 5) & 0x7, (hdata[9] >> 2) & 0x3);
                } else {
                    fprintf(logfd, "S ");
                    fprintf(logfd, "%c ", (hdata[9] & 0x10) ? (rep ? 'F' : 'P') : '-');
                    fprintf(logfd, "Nr:%01d %01x\n", (hdata[9] >> 5) & 0x7, (hdata[9] >> 2) & 0x3);
                }
            } else {
                fprintf(logfd, "I ");
                fprintf(logfd, "%c ", (hdata[9] & 0x10) ? (rep ? 'F' : 'P') : '-');
                fprintf(logfd, "Ns:%01d Nr:%01d\n", (hdata[9] >> 1) & 0x7, (hdata[9] >> 5) & 0x7);
            }
            r = 0;

            if (k == 13) {
                if (verbose > 1)
                    fprintf(logfd, "empty\n");
                r = 1;
            }

            if (k >= 16 && hdata[10] == 0xff && hdata[11] == 0xff && hdata[12] == 1) {
                outacars(&(hdata[13]), k - 16);
                r = 1;
            }
            if (k >= 14 && hdata[10] == 0x82) {
                outxid(&(hdata[11]), k - 14);
                r = 1;
            }

            if (r == 0) {
                fprintf(logfd, "not decoded\n");
                if (verbose > 1)
                    dumpdata(&(hdata[10]), k - 13);
            }
            fprintf(logfd, "-----------------------------------------------------------------------\n");
            fflush(logfd);

        }

        free(blk);
    } while (1);

    return NULL;
}

int initVdlm2(char *dbname)
{
    pthread_t th;
    int chn;

    channel.state = WSYNC;

    channel.blk = calloc(sizeof(msgblk_t), 1);

    pthread_mutex_init(&blkmtx, NULL);
    pthread_cond_init(&blkwcd, NULL);

    pthread_create(&th, NULL, blk_thread, NULL);

    return 0;
}

void stopVdlm2(void)
{
    int c;
    for (c = 0; blkq_e && c < 5; c++)
        sleep(1);
}

void decodeVdlm2()
{
    pthread_mutex_lock(&blkmtx);
    channel.blk->prev = NULL;
    if (blkq_s)
        blkq_s->prev = channel.blk;
    blkq_s = channel.blk;
    if (blkq_e == NULL)
        blkq_e = blkq_s;
    pthread_cond_signal(&blkwcd);
    pthread_mutex_unlock(&blkmtx);

    channel.blk = calloc(sizeof(msgblk_t), 1);

    return;
}
