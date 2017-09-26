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
#include <pthread.h>
#include <complex.h>
#include <stdint.h>

#define MAXNBCHANNELS 1
#define INTRATE 25000

#define RTLMULT 24
#define RTLDWN 4
#define RTLINRATE (10500*RTLMULT*2*RTLDWN)
#define RTLINBUFSZ (RTLMULT*4096)

#define MFLTLEN 17

typedef struct mskblk_s {
    struct mskblk_s *prev;
    int chn;
    struct timespec ts;
    float df, lvl;
    int nbrow, nbbyte, nblst;
    unsigned char data[65][255];
    float pr[65][255];
} msgblk_t;

#define NBPH 17
typedef struct {
    int chn;
    float Fr;
    complex float *wf;
    complex float Inbuff[MFLTLEN];
    float Ph[NBPH * RTLDWN];
    int Phidx;
    float df, lv;
    unsigned int nlv;
    int clk;
    float p2err, perr;
    float pfr;
    float P1;

    unsigned int scrambler;
    unsigned int nbits;
    unsigned int nrow;
    unsigned int nbyte;
    unsigned char bits;
    float pr;

    enum { WSYNC, GETHEAD, GETDATA } state;
    msgblk_t *blk;

} channel_t;

#ifdef WITH_RTL
extern int gain;
extern int ppm;
#endif

extern int verbose;
extern FILE *logfd;

#ifdef WITH_RTL
extern int initRtl(char **argv,int optind);
extern int runRtlSample(void);
#endif

extern void in_callback(unsigned char *rtlinbuff, uint32_t nread, void *ctx);

extern int initFile(char *file);
extern int runFileSample(void);

extern int initD8psk(channel_t *ch);
extern void demodD8psk(channel_t *ch,const float complex E);

extern int initVdlm2(channel_t *ch);
extern void stopVdlm2(void);
extern void decodeVdlm2(channel_t *ch);

extern void init_crc_tab(void);
extern unsigned short update_crc(const unsigned short crc, const unsigned char c);

extern void viterbi_init(void);
extern void viterbi_add(float V, int n);
extern float viterbi_end(unsigned int *bits);

extern unsigned int reversebits(const unsigned int bits, const int n);

extern unsigned short pppfcs16(unsigned char *cp, int len);
extern int rs(unsigned char *data, int *eras_pos, int no_eras);

extern void dumpdata(unsigned char *p, int len);
extern void outxid(unsigned char *p, int len);
