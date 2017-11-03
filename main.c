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
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <sys/types.h>

#include "vdlm2.h"

int verbose = 1;
int itype = -1;
#ifdef WITH_RTL
int gain = 1000;
int ppm = 0;
#endif

channel_t channel[MAXNBCHANNELS];
int nbch;

pthread_barrier_t Bar1,Bar2;

FILE *logfd;

static void usage(void)
{
    fprintf(stderr, "vdlm2dec V1.0 Copyright (c) 2016 Thierry Leconte \n\n");
    fprintf(stderr, "Usage: vdlm2dec  [-v|V] [-l logfile] ");
#ifdef WITH_RTL
    fprintf(stderr, " [-g gain] [-r rtldevicenumber] ");
#endif
    fprintf(stderr, " Frequency(Mhz)\n");
#ifdef WITH_RTL
    fprintf(stderr, " -r rtldevicenumber :\tdecode from rtl dongle number rtldevicenumber \n");
    fprintf(stderr, " -g gain :\t\tset rtl preamp gain in tenth of db (ie -g 90 for +9db).\n");
    fprintf(stderr, " -p ppm :\t\tppm frequency correction\n");
#endif
    fprintf(stderr, " -v :\t\t\tverbose\n");
    fprintf(stderr, " -q :\t\t\tquiet\n");
    fprintf(stderr, " -l logfile :\t\toutput log (stderr by default)\n");
    exit(1);
}

static void sighandler(int signum)
{
    stopVdlm2();
    exit(1);
}

static int infd;
int initFile(char *file)
{
    channel_t *ch = &(channel[0]);
    int ind;
    float AMFreq;

    nbch=1;

    infd = open(file, O_RDONLY);
    if (infd < 0) {
        fprintf(stderr, "could not open data\n ");
        return 1;
    }

    ch->Fosc = (10500*2*RTLDWN) / (float)(RTLINRATE) * 2.0 * M_PI;
    ch->Posc=0;

    return 0;
}

int runFileSample(void)
{
    unsigned char buffer[RTLINBUFSZ];

    size_t rd;

    do {
        rd = read(infd, buffer, RTLINBUFSZ);
        if (rd)
            in_callback(buffer, rd, NULL);
    } while (rd);

}

int main(int argc, char **argv)
{
    int c;
    int res, n;
    struct sigaction sigact;
    char *fname;
    int rdev;
    double freq;

    nbch=0;
    logfd = stderr;

    while ((c = getopt(argc, argv, "vqt:rp:g:l:")) != EOF) {
        switch (c) {
        case 'v':
            verbose = 2;
            break;
        case 'q':
            verbose = 0;
            break;
        case 't':
            res = initFile(optarg);
            itype = 1;
            break;
        case 'l':
            logfd = fopen(optarg, "w+");
            if (logfd == NULL) {
                fprintf(stderr, "unable top open %s\n", optarg);
                exit(1);
            }
            break;
#ifdef WITH_RTL
        case 'r':
            res = initRtl(argv, optind);
            itype = 0;
            break;
        case 'g':
            gain = atoi(optarg);
            break;
        case 'p':
            ppm = atoi(optarg);
            break;
#endif
        default:
            usage();
            return (1);
        }
    }

    if (res) {
        fprintf(stderr, "Unable to init input\n");
        exit(res);
    }

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);

    pthread_barrier_init(&Bar1,NULL,nbch+1);
    pthread_barrier_init(&Bar2,NULL,nbch+1);

    for (n = 0; n < nbch; n++) {
         channel[n].chn = n;

         res = initD8psk(&(channel[n]));
         if (res)
                   break;
         res = initVdlm2(&(channel[n]));
         if (res)
                  break;
    }

#if DEBUG
    initSndWrite();
#endif

    switch (itype) {
#ifdef WITH_RTL
    case 0:
        res = runRtlSample();
        break;
#endif
    case 1:
        res = runFileSample();
        break;
    }

    stopVdlm2();

    exit(1);

}
