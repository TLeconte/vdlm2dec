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


void *rcv_thread(void *arg);
int initD8psk(channel_t *ch)
{
    pthread_t th;

    ch->ink = 0;
    ch->Phidx = 0;
    ch->df = 0;
    ch->perr = 100;
    ch->P1 = 0;

    pthread_create(&th, NULL, rcv_thread, ch);

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

static inline float descrambler(channel_t *ch,const float V)
{
    int b;

    b = (ch->scrambler ^ (ch->scrambler >> 14)) & 1;
    ch->scrambler = (ch->scrambler << 1) | b;
    if (b)
        return (1.0 - V);
    else
        return V;

}

static inline void putbit(channel_t *ch,const float SV)
{
    float V;

    V = descrambler(ch,SV);

    switch (ch->state) {
    case WSYNC:
        /* ???? */
        return;
    case GETHEAD:
        {
            unsigned int bits, len;
            float vp;

            viterbi_add(V, ch->nbits);
            ch->nbits++;
            if (ch->nbits < 25)
                return;

            vp = viterbi_end(&bits);

            if (vp < 0.04) {
                //fprintf(stderr,"error viterbi\n");
                ch->state = WSYNC;
                return;
            }

            bits >>= 5;         /* remove FEC */

            len = reversebits(bits, 17);

            if (len < 13 * 8) {
                //fprintf(stderr,"error too short\n");
                ch->state = WSYNC;
                return;
            }

            ch->blk->nbrow = len / 1992 + 1;
            ch->blk->nbbyte = (len % 1992 + 7) / 8;

            if (ch->blk->nbrow > 65) {
                //fprintf(stderr,"error too long\n");
                ch->state = WSYNC;
                return;
            }
            //fprintf(stderr,"row %d byte %d v: %f\n",ch->blk->nbrow,ch->blk->nbbyte,vp);

            ch->state = GETDATA;
            ch->nrow = ch->nbyte = 0;
            ch->nbits = ch->bits = 0;
            ch->pr = 1.0;

            return;
        }
    case GETDATA:
        {
            if (V > 0.5) {
                ch->bits |= 1 << ch->nbits;
                ch->pr *= V;
            } else {
                ch->pr *= (1.0 - V);
            }
            ch->nbits++;
            if (ch->nbits < 8) {
                return;
            }

            ch->blk->data[ch->nrow][ch->nbyte] = ch->bits;
            ch->blk->pr[ch->nrow][ch->nbyte] = ch->pr;

            ch->nbits = ch->bits = 0;
            ch->pr = 1.0;
            ch->nrow++;

            if (ch->nrow == ch->blk->nbrow ||
                ((ch->nbyte >= ch->blk->nbbyte && ch->nbyte < 249)
                 && ch->nrow == ch->blk->nbrow - 1)
                ) {
                ch->nrow = 0;
                ch->nbyte++;

                if (ch->blk->nbrow == 1 && ch->nbyte == ch->blk->nbbyte)
                    ch->nbyte = 249;

            }

            if (ch->nbyte == 255 ||
                (ch->nrow == ch->blk->nbrow - 1 &&
                 (ch->blk->nbbyte < 30 && ch->nbyte == 251) ||
                 (ch->blk->nbbyte < 68 && ch->nbyte == 253)
                )
                ) {

                decodeVdlm2(ch);

                ch->state = WSYNC;
                return;
            }
            return;
        }
    }

}

static inline void putgreycode(channel_t *ch,const float v)
{
    int i = (int)roundf(128.0 * v / M_PI + 128.0);
    putbit(ch,Grey1[i]);
    putbit(ch,Grey2[i]);
    putbit(ch,Grey3[i]);
}

static inline float filteredphase(channel_t *ch)
{
    int i,k;
    complex float S = 0;

    for (i = 0; i*4+ch->clk < 4*MFLTLEN; i++) {
        k = (ch->ink + i) % MFLTLEN;
        S += ch->Inbuff[k] * mflt[i*4+ch->clk];
    }
    /* phase */
    return cargf(S);
}


static void demodD8psk(channel_t *ch,const complex float E)
{
    int i, k;
    complex float S;
    float P;
    float d[4];

    ch->Inbuff[ch->ink] = E;
    ch->ink = (ch->ink + 1) % MFLTLEN;

    ch->clk+=4;


    if (ch->state == WSYNC) {

        float fr, M;
        float Pr[NBPH], Pu, Pv;
        int l;
        float p, err;

	if(ch->clk<8) return;
	ch->clk-=8;

	P=filteredphase(ch);

        ch->Phidx = (ch->Phidx + 1) % (NBPH * RTLDWN);
        ch->Ph[ch->Phidx] = P;

        /* detect sync */

        Pu = 0;
        M = Pv = Pr[0] = ch->Ph[(ch->Phidx + RTLDWN) % (NBPH * RTLDWN)] - SW[0];
        for (l = 1; l < NBPH; l++) {
            float Pc, Pd;
            Pc = ch->Ph[(ch->Phidx + (l + 1) * RTLDWN) % (NBPH * RTLDWN)] - SW[l];
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

        //fprintf(stderr,"err %f %f\n",ch->perr,fr);
        if (ch->perr < 4.0 && err > ch->perr) {
            float of;
            double du;
            clock_gettime(CLOCK_REALTIME, &(ch->blk->ts));
            ch->state = GETHEAD;
            ch->nbits = 0;
            ch->scrambler = 0x4D4B;
            viterbi_init();
            ch->df = ch->pfr;
	    ch->blk->ppm=10500*ch->df/(2*M_PI*ch->Fr)*1e6;
            of = 4*(ch->p2err - 4 * ch->perr + 3 * err) / (ch->p2err - 2 * ch->perr + err);
            ch->clk = (int)roundf(of);
    	    ch->P1 = filteredphase(ch);

            //fprintf(stderr,"head %f %d %f %f\n",of,ch->clk, err,ch->df);
            ch->perr = ch->p2err = 500;
        } else {
            ch->p2err = ch->perr;
            ch->perr = err;
            ch->pfr = fr;
        }
    } else {
        int v;
        float D, err;

        if (ch->clk <32) return;
          ch->clk -=32; 

	    P=filteredphase(ch);

            D = (P - ch->P1) - ch->df;
            if (D > M_PI)
                D -= 2 * M_PI;
            if (D < -M_PI)
                D += 2 * M_PI;

            putgreycode(ch,D);

            ch->P1 = P;
    }

    //SndWrite(d);

}

extern pthread_barrier_t Bar1,Bar2;
extern complex float Cbuff[RTLINBUFSZ/2];
void *rcv_thread(void *arg)
{
   channel_t *ch=(channel_t *)arg;

   pthread_barrier_wait(&Bar1);
   do {
     int i;
     complex float D;

     pthread_barrier_wait(&Bar2);
     for (i = 0; i < RTLINBUFSZ/2;) {
           int ind;

           D = 0;
           for (ind = 0; ind < RTLMULT; ind++) {

               D+=Cbuff[i++]*cexpf(-ch->Posc*I);

               ch->Posc+=ch->Fosc;
               if(ch->Posc>M_PI) ch->Posc-=2*M_PI;
               if(ch->Posc<-M_PI) ch->Posc+=2*M_PI;
            }
            demodD8psk(ch,D);
     }
     pthread_barrier_wait(&Bar1);
   } while(1);
}
