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
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include "vdlm2.h"
#include "d8psk.h"

static struct timespec ftime;
static unsigned int ink;

int initD8psk()
{
    ink = 0;
    channel.Phidx = 0;
    channel.df = 0;
    channel.perr = 100;
    channel.P1 = 0;
    channel.off = 0;

    return 0;
}

unsigned int reversebits(const unsigned int bits, const int n)
{
    int i;
    unsigned int in, out;

    in = bits;
    out = 0;
    for (i = 0; i < n; i++) {
        out <<= 1;
        out |= in & 1;
        in >>= 1;
    }
    return out;
}

static float descrambler(const float V)
{
    int b;

    b = (channel.scrambler ^ (channel.scrambler >> 14)) & 1;
    channel.scrambler = (channel.scrambler << 1) | b;
    if (b)
        return (1.0 - V);
    else
        return V;

}

static inline void putbit(const float SV)
{
    float V;

    V = descrambler(SV);

    switch (channel.state) {
    case WSYNC:
        /* ???? */
        return;
    case GETHEAD:
        {
            unsigned int bits, len;
            float vp;

            viterbi_add(V, channel.nbits);
            channel.nbits++;
            if (channel.nbits < 25)
                return;

            vp = viterbi_end(&bits);

            if (vp < 0.04) {
                //fprintf(stderr,"error viterbi\n");
                channel.state = WSYNC;
                return;
            }

            bits >>= 5;         /* remove FEC */

            len = reversebits(bits, 17);

            if (len < 13 * 8) {
                //fprintf(stderr,"error too short\n");
                channel.state = WSYNC;
                return;
            }

            channel.blk->nbrow = len / 1992 + 1;
            channel.blk->nbbyte = (len % 1992 + 7) / 8;

            if (channel.blk->nbrow > 65) {
                //fprintf(stderr,"error too long\n");
                channel.state = WSYNC;
                return;
            }
            //fprintf(stderr,"row %d byte %d v: %f\n",channel.blk->nbrow,channel.blk->nbbyte,vp);

            channel.state = GETDATA;
            channel.nrow = channel.nbyte = 0;
            channel.nbits = channel.bits = 0;
            channel.pr = 1.0;

            return;
        }
    case GETDATA:
        {
            if (V > 0.5) {
                channel.bits |= 1 << channel.nbits;
                channel.pr *= V;
            } else {
                channel.pr *= (1.0 - V);
            }
            channel.nbits++;
            if (channel.nbits < 8) {
                return;
            }

            channel.blk->data[channel.nrow][channel.nbyte] = channel.bits;
            channel.blk->pr[channel.nrow][channel.nbyte] = channel.pr;

            channel.nbits = channel.bits = 0;
            channel.pr = 1.0;
            channel.nrow++;

            if (channel.nrow == channel.blk->nbrow ||
                ((channel.nbyte >= channel.blk->nbbyte && channel.nbyte < 249)
                 && channel.nrow == channel.blk->nbrow - 1)
                ) {
                channel.nrow = 0;
                channel.nbyte++;

                if (channel.blk->nbrow == 1 && channel.nbyte == channel.blk->nbbyte)
                    channel.nbyte = 249;

            }

            if (channel.nbyte == 255 ||
                (channel.nrow == channel.blk->nbrow - 1 &&
                 (channel.blk->nbbyte < 30 && channel.nbyte == 251) ||
                 (channel.blk->nbbyte < 68 && channel.nbyte == 253)
                )
                ) {
                decodeVdlm2();

                channel.state = WSYNC;
                return;
            }
            return;
        }
    }

}

static inline void putgreycode(const float v)
{
    int i = (int)roundf(128.0 * v / M_PI + 128.0);
    putbit(Grey1[i]);
    putbit(Grey2[i]);
    putbit(Grey3[i]);
}

void demodD8psk(const float complex E)
{
    int i, k;
    complex float S;
    float P;
    float d[4];

    channel.Inbuff[ink] = E;
    ink = (ink + 1) % MFLTLEN;

    channel.clk++;
    if (channel.clk & 1)
        return;

    /* filter */
    S = 0;
    for (i = 0; i < MFLTLEN; i++) {
        k = (ink + i) % MFLTLEN;
        S += channel.Inbuff[k] * mflt[channel.off][i];
    }

    /* phase */
    P = cargf(S);

    channel.Phidx = (channel.Phidx + 1) % (NBPH * RTLDWN);
    channel.Ph[channel.Phidx] = P;

    if (channel.state == WSYNC) {

        float fr, M;
        float Pr[NBPH], Pu, Pv;
        int l;
        float p, err;

        /* detect sync */
        channel.off = 0;

        Pu = 0;
        M = Pv = Pr[0] = channel.Ph[(channel.Phidx + RTLDWN) % (NBPH * RTLDWN)] - SW[0];
        for (l = 1; l < NBPH; l++) {
            float Pc, Pd;
            Pc = channel.Ph[(channel.Phidx + (l + 1) * RTLDWN) % (NBPH * RTLDWN)] - SW[l];
            /* unwarp */
            Pd = Pc - Pv;
            Pv = Pc;
            if (Pd > M_PI) {
                Pu -= 2 * M_PI;
            } else if (Pd < -M_PI) {
                Pu += 2 * M_PI;
            }
            Pr[l] = Pc + Pu;
            M += Pr[l];
        }
        M /= NBPH;
        fr = 0;
        for (l = 0; l < NBPH; l++) {
            Pr[l] -= M;
            fr += Pr[l] * (l - (NBPH - 1) / 2);
        }
        fr /= 408;
        err = 0;
        for (l = 0; l < NBPH; l++) {
            float e;
            e = Pr[l] - (l - (NBPH - 1) / 2) * fr;
            err += e * e;
        }

        //fprintf(stderr,"err %f %f\n",channel.perr,fr);
        if (channel.perr < 4.0 && err > channel.perr) {
            float of;
            double du;
            clock_gettime(CLOCK_REALTIME, &(channel.blk->ts));
            channel.state = GETHEAD;
            channel.nbits = 0;
            channel.scrambler = 0x4D4B;
            viterbi_init();
            channel.df = channel.pfr;
            of = (channel.p2err - 4 * channel.perr + 3 * err) / (channel.p2err - 2 * channel.perr + err);
            channel.clk = (int)roundf(of);
            channel.off = (int)roundf((of - channel.clk) * 8) + 4;
            //fprintf(stderr,"head %f %d %d %f %f\n",of,channel.clk, channel.off,err,channel.df);
            channel.perr = channel.p2err = 500;
        } else {
            channel.p2err = channel.perr;
            channel.perr = err;
            channel.pfr = fr;
            channel.P1 = P;
        }
    } else {
        int v;

        if (channel.clk == 8) {
            float D, err;

            channel.clk = 0;

            D = (P - channel.P1) - channel.df;
            if (D > M_PI)
                D -= 2 * M_PI;
            if (D < -M_PI)
                D += 2 * M_PI;

            putgreycode(D);

            channel.P1 = P;
        }
    }

    //SndWrite(d);

}
