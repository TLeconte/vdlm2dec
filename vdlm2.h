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

#define MAXNBCHANNELS 8

#define RTLMULT 24
#define RTLDWN 4
#define INTRATE  10500*2*RTLDWN
#define RTLINRATE (RTLMULT*INTRATE)
#define RTLINBUFSZ (RTLMULT*4096)

#define MFLTLEN 16

typedef struct mskblk_s {
    struct mskblk_s *prev;
    int chn;
    struct timespec ts;
    float ppm;
    int nbrow, nlbyte;
    unsigned char data[65][255];
} msgblk_t;

typedef struct {
    int chn;
    float Fr;
    float Fosc;
} thread_param_t;

#define NBPH 17
typedef struct {
    complex float Inbuff[MFLTLEN];
    float Ph[NBPH * RTLDWN];

    int chn;
    float Fr;
    float Posc,Fosc;
    int ink,Phidx;
    float df, lv;
    unsigned int nlv;
    int clk;
    float p2err, perr;
    float pfr;
    float P1;

    unsigned int scrambler;
    unsigned int nbits;
    unsigned int nbyte;
    unsigned int nrow,nbrow;
    unsigned int nlbyte;
    unsigned char bits;

    enum { WSYNC, GETHEAD, GETDATA, GETFEC } state;
    msgblk_t *blk;

} channel_t;

#ifdef WITH_RTL
extern int gain;
extern int ppm;
#endif

extern int verbose;
extern FILE *logfd;

#ifdef WITH_RTL
extern int initRtl(char **argv,int optind,thread_param_t* param);
extern int runRtlSample(void);
#endif

extern void in_callback(unsigned char *rtlinbuff, uint32_t nread, void *ctx);

extern int initFile(char *file);
extern int runFileSample(void);

extern int initD8psk(channel_t *ch);
extern void *rcv_thread(void *arg);

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

extern int netconnect(char *inaddr);
extern int transmit(unsigned int addr,char *st,int len,uint64_t tm);


