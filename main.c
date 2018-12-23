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
#include <sys/param.h>

#include "vdlm2.h"
extern void build_label_filter(char *arg);

int verbose = 1;
int grndmess = 0;
int emptymess = 0;
int undecmess = 0;
int jsonout = 0;
int routeout = 0;
int regout = 0;
int mdly=1800;

char *Rawaddr = NULL;
char *idstation = NULL ;
FILE *logfd;

#ifdef WITH_RTL
int gain = 1000;
int ppm = 0;
#endif
#ifdef WITH_AIR
int gain = 21;
#endif

int nbch;
thread_param_t tparam[MAXNBCHANNELS];
pthread_barrier_t Bar1, Bar2;

static void usage(void)
{
	fprintf(stderr,
		"vdlm2dec V2.0 Copyright (c) 2016-2018 Thierry Leconte \n\n");
	fprintf(stderr, "Usage: vdlm2dec  [-J] [-q] [-j addr:port ] [-l logfile] [-g gain]");
#ifdef WITH_RTL
	fprintf(stderr, " [-p ppm] -r rtldevicenumber");
#endif
	fprintf(stderr, " Frequencies(Mhz)\n\n");

	fprintf(stderr, " -i stid :\t\tlocal receiver station id (for json output)\n");
	fprintf(stderr, " -v :\t\t\tverbose output\n");
	fprintf(stderr, " -q :\t\t\tquiet output\n");
	fprintf(stderr, " -J :\t\t\tjson output\n");
	fprintf(stderr, " -R :\t\t\troute json output\n");
	fprintf(stderr, " -a :\t\t\taircraft registration csv output\n");
	fprintf(stderr, " -G :\t\t\toutput messages from ground station\n");
	fprintf(stderr, " -E :\t\t\toutput empty messages\n");
	fprintf(stderr, " -U :\t\t\toutput undecoded messages\n");
	fprintf(stderr, " -b filter :\t\tfilter acars output by label (ex: -b \"H1:Q0\" : only output messages  with label H1 or Q0)\n");
	fprintf(stderr, " -j addr:port :\t\toutput UDP packet to addr:port\n");
	fprintf(stderr, " -l logfile :\t\toutput log (stdout by default)\n\n");

#ifdef WITH_RTL
	fprintf(stderr,
		" -r rtldevicenumber :\tdecode from rtl dongle number rtldevicenumber.(MANDATORY parameter)\n");
	fprintf(stderr,
		" -g gain :\t\tset rtl preamp gain in tenth of db (ie -g 90 for +9db).By default use maximum gain\n");
	fprintf(stderr, " -p ppm :\t\tppm frequency correction\n");
#endif
#ifdef WITH_AIR
	fprintf(stderr,
		" -g gain :\t\tset linearity gain (0 to 21).By default use maximum gain\n");
#endif
	exit(1);
}

static void sighandler(int signum)
{
	stopVdlm2();
	exit(1);
}

int main(int argc, char **argv)
{
	int n,c;
	int res=0;
	struct sigaction sigact;
        char sys_hostname[HOST_NAME_MAX+1];
        char *lblf=NULL;

        gethostname(sys_hostname, sizeof(sys_hostname));
	sys_hostname[sizeof(sys_hostname) - 1] = '\0';
        idstation = strdup(sys_hostname);

	nbch = 0;
	logfd = stdout;

	while ((c = getopt(argc, argv, "vqrp:g:l:JRj:i:GEUt:b:a:")) != EOF) {
		switch (c) {
		case 'v':
			verbose = 2;
			break;
		case 'q':
			verbose = 0;
			break;
		case 'l':
			logfd = fopen(optarg, "a+");
			if (logfd == NULL) {
				fprintf(stderr, "unable top open %s\n", optarg);
				exit(1);
			}
			break;
#ifdef WITH_RTL
		case 'r':
			res = initRtl(argv, optind, tparam);
			break;
		case 'p':
			ppm = atoi(optarg);
			break;
		case 'g':
			gain = atoi(optarg);
			break;
#endif
#ifdef WITH_AIR
		case 'g':
			gain = atoi(optarg);
			break;
#endif
		case 'j':
			Rawaddr = optarg;
			initNetOutput(Rawaddr);
			break;
		case 'J':
			jsonout = 1;
			break;
		case 'R':
			routeout = 1;
			jsonout = 1;
			break;
		case 'a':
			regout = 1;
			jsonout = 0;
			break;
                case 't':
                        mdly = atoi(optarg);
                        break;
		case 'i':
			free(idstation);
			idstation = strdup(optarg);
			break;
		case 'b':
                        lblf=optarg;
			break;
		case 'G':
			grndmess=1;
			break;
		case 'E':
			emptymess = 1;
			break;
		case 'U':
			undecmess = 1;
			break;

		default:
			usage();
			return (1);
		}
	}

	if(jsonout || regout) 
		verbose=0;

        build_label_filter(lblf);

#ifdef WITH_AIR
	res=initAirspy(argv, optind, tparam);
#endif

	if (res) {
		fprintf(stderr, "Unable to init input\n");
		exit(-1);
	}

	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);

	pthread_barrier_init(&Bar1, NULL, nbch + 1);
	pthread_barrier_init(&Bar2, NULL, nbch + 1);

	for (n = 0; n < nbch; n++) {
		pthread_t th;
		pthread_create(&th, NULL, rcv_thread, &(tparam[n]));
	}

#if DEBUG
	initSndWrite();
#endif

#ifdef WITH_RTL
	runRtlSample();
#endif
#ifdef WITH_AIR
	runAirspySample();
#endif

	stopVdlm2();

	exit(1);

}
