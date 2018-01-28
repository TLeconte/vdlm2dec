/*
 *  Copyright (c) 2015 Thierry Leconte
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
#ifdef WITH_AIR

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <libairspy/airspy.h>
#include "vdlm2.h"

extern int nbch;
extern pthread_barrier_t Bar1, Bar2;
extern int gain;

unsigned int Fc;

static struct airspy_device* device = NULL;

static unsigned int chooseFc(unsigned int *Fd, const unsigned int nbch)
{
        int n;
        int ne;
        int Fc;
        do {
                ne = 0;
                for (n = 0; n < nbch - 1; n++) {
                        if (Fd[n] > Fd[n + 1]) {
                                unsigned int t;
                                t = Fd[n + 1];
                                Fd[n + 1] = Fd[n];
                                Fd[n] = t;
                                ne = 1;
                        }
                }
        } while (ne);

        if ((Fd[nbch - 1] - Fd[0]) > SDRINRATE - 4 * STEPRATE) {
                fprintf(stderr, "Frequencies too far apart\n");
                return 0;
        }

        for (Fc = Fd[nbch - 1] + 2 * STEPRATE; Fc > Fd[0] - 2 * STEPRATE; Fc--) {
                for (n = 0; n < nbch; n++) {
                        if (abs(Fc - Fd[n]) > SDRINRATE / 2 - 2 * STEPRATE)
                                break;
                        if (abs(Fc - Fd[n]) < 2 * STEPRATE)
                                break;
                        if (n > 0 && Fc - Fd[n - 1] == Fd[n] - Fc)
                                break;
                }
                if (n == nbch)
                        break;
        }

        return Fc;
}


int initAirspy(char **argv, int optind, thread_param_t * param)
{
	int n;
	char *argF;
	unsigned int F0;
	unsigned int Fd[MAXNBCHANNELS];
	int result;
	uint32_t i,count;
	uint32_t * supported_samplerates;


	nbch = 0;
	while ((argF = argv[optind]) && nbch < MAXNBCHANNELS) {
                Fd[nbch] = (int)(1000000 * atof(argF));
		optind++;
		if (Fd[nbch] < 118000000 || Fd[nbch] > 138000000) {
			fprintf(stderr, "WARNING: Invalid frequency %d\n",
				Fd[nbch]);
			continue;
		}
		param[nbch].chn = nbch;
		param[nbch].Fr = Fd[nbch];
		nbch++;
	};
	if (nbch > MAXNBCHANNELS)
		fprintf(stderr,
			"WARNING: too many frequencies, taking only the first %d\n",
			MAXNBCHANNELS);

	if (nbch == 0) {
		fprintf(stderr, "Need a least one frequency\n");
		return 1;
	}

	Fc = chooseFc(Fd, nbch);
        if (Fc == 0)
                return 1;

	F0=Fc+SDRINRATE/4;

	for (n = 0; n < nbch; n++) {
                param[n].Fo = param[n].Fr-F0;
        }

	result = airspy_init();
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_init() failed: %s (%d)\n", airspy_error_name(result), result);
		return -1;
	}

	result = airspy_open(&device);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_open() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_exit();
		return -1;
	}

	airspy_get_samplerates(device, &count, 0);
	supported_samplerates = (uint32_t *) malloc(count * sizeof(uint32_t));
	airspy_get_samplerates(device, supported_samplerates, count);
	for(i=0;i<count;i++)
		if(supported_samplerates[i]==SDRINRATE/2) break;
	if(i>=count) {
		fprintf(stderr,"did not find needed sampling rate\n");
		airspy_exit();
		return -1;
	}
	free(supported_samplerates);

	result = airspy_set_samplerate(device, i);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_set_samplerate() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_close(device);
		airspy_exit();
		return -1;
	}

	result = airspy_set_sample_type(device, AIRSPY_SAMPLE_FLOAT32_REAL);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_set_sample_type() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_close(device);
		airspy_exit();
		return -1;
	}

        result = airspy_set_packing(device, 1);
        if( result != AIRSPY_SUCCESS ) {
                fprintf(stderr,"airspy_set_packing true failed: %s (%d)\n", airspy_error_name(result), result);
        }

        result = airspy_set_linearity_gain(device, gain);
        if( result != AIRSPY_SUCCESS ) {
                fprintf(stderr,"airspy_set_linearity_gain() failed: %s (%d)\n", airspy_error_name(result), result);
        }

	if (verbose)
		fprintf(stderr, "Set freq. to %d hz\n", Fc);

	result = airspy_set_freq(device, Fc);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_set_freq() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_close(device);
		airspy_exit();
		return -1;
	}


	return 0;
}

float Cbuff[RTLINBUFSZ / 2];
static int rx_callback(airspy_transfer_t* transfer)
{
	static int ind=0;
        float* pt_rx_buffer;
        int n;
        pt_rx_buffer = (float *)(transfer->samples);

	n=0;
	while(n<transfer->sample_count)
	 {		
		int i;

       		if(ind==0)
			 pthread_barrier_wait(&Bar1);

        	for (i = ind; i < RTLINBUFSZ/2 && n<transfer->sample_count;i++,n++) {
                	Cbuff[i] = pt_rx_buffer[n];
        	}

		if(i==RTLINBUFSZ/2) {
        		pthread_barrier_wait(&Bar2);
			ind=0;
		} else
			ind=i;
	}
	return 0;
}


int runAirspySample(void) 
{
 int result ;

 result = airspy_start_rx(device, rx_callback, NULL);
 if( result != AIRSPY_SUCCESS ) {
	fprintf(stderr,"airspy_start_rx() failed: %s (%d)\n", airspy_error_name(result), result);
	airspy_close(device);
	airspy_exit();
	return -1;
 }

 while(airspy_is_streaming(device) == AIRSPY_TRUE) {
 	sleep(2);
 }

 return 0;
}

#endif
