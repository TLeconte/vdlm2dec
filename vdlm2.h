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

#define VDLM2DEC_VERSION "2.2"

#define MAXNBCHANNELS 8

#define MAX_ID_LEN 48

extern unsigned int SDRINRATE;
extern unsigned int SDRCLK;

#define STEPRATE 25000

#define RTLINBUFSZ (16*4096)

#define MFLTLEN 65
#define MBUFLEN 17
typedef struct mskblk_s {
	struct mskblk_s *prev;
	int chn;
	int Fr;
	struct timeval tv;
	float ppm;
	int nbrow, nlbyte;
	unsigned char data[65][255];
} msgblk_t;

typedef struct {
	int chn;
	int Fr,Fo;
} thread_param_t;

#define NBPH 17
#define D8DWN 4
typedef struct {
	complex float Inbuff[MBUFLEN];
	float Ph[NBPH * D8DWN];

	int chn;
	int Fr;
	int ink, Phidx;
	float df;
	int clk;
	float p2err, perr;
	float pfr;
	float P1;

	unsigned int scrambler;
	unsigned int nbits;
	unsigned int nbyte;
	unsigned int nrow, nbrow;
	unsigned int nlbyte;
	unsigned char bits;

	enum { WSYNC, GETHEAD, GETDATA, GETFEC } state;
	msgblk_t *blk;

} channel_t;

#define JSONBUFLEN 50000
#define OUTBUFLEN 50000

extern unsigned int Fc;
extern pthread_barrier_t Bar1, Bar2;

#ifdef WITH_RTL
extern int ppm;
extern complex float Cbuff[RTLINBUFSZ / 2];
#endif
#ifdef WITH_AIR
extern float Cbuff[RTLINBUFSZ / 2];
#endif

extern int grndmess;
extern int emptymess;
extern int undecmess;

extern FILE *logfd;

#ifdef WITH_RTL
extern int initRtl(char **argv, int optind, thread_param_t * param);
extern int runRtlSample(void);
#endif
#ifdef WITH_AIR
extern int initAirspy(char **argv, int optind, thread_param_t * param);
extern int runAirspySample(void);
#endif

extern int initFile(char *file);
extern int runFileSample(void);

extern int initD8psk(channel_t * ch);
extern void *rcv_thread(void *arg);

extern int initVdlm2(channel_t * ch);
extern void stopVdlm2(void);
extern void decodeVdlm2(channel_t * ch);

extern void init_crc_tab(void);
extern unsigned short update_crc(const unsigned short crc,
				 const unsigned char c);

extern void viterbi_init(void);
extern void viterbi_add(float V, int n);
extern float viterbi_end(unsigned int *bits);

extern unsigned int reversebits(const unsigned int bits, const int n);

extern unsigned short pppfcs16(unsigned char *cp, int len);
extern int rs(unsigned char *data, int *eras_pos, int no_eras);

extern int initNetOutput(char *Rawaddr);
extern void vout(char *format, ...);
extern void out(msgblk_t * blk, unsigned char *hdata, int l);
