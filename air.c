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
extern uint64_t airspy_serial;
extern int verbose;

unsigned int SDRINRATE = 6000000;
unsigned int SDRCLK = 1500;

unsigned int Fc;

static struct airspy_device* device = NULL;

static const unsigned int r820t_hf[]={1953050,1980748,2001344,2032592,2060291,2087988};
static const unsigned int r820t_lf[]={525548,656935,795424,898403,1186034,1502073,1715133,1853622};

static unsigned int chooseFc(unsigned int minF,unsigned int maxF)
{
        unsigned int bw=maxF-minF+2*STEPRATE;
        unsigned int off=0;
        int i,j;

        if(SDRINRATE == 5000000) {
            /* This feature is specific to the R820T2 tuner in the Airspy R2 where the Mini has an R860. */
            for(i=7;i>=0;i--)
                if((r820t_hf[5]-r820t_lf[i])>=bw) break;
            if(i<0) return 0;

            for(j=5;j>=0;j--)
                if((r820t_hf[j]-r820t_lf[i])<=bw) break;
            j++;

            off=(r820t_hf[j]+r820t_lf[i])/2-SDRINRATE/4;

            airspy_r820t_write(device, 10, 0xB0 | (15-j));
            airspy_r820t_write(device, 11, 0xE0 | (15-i));
        }

        return(((maxF+minF)/2+off+STEPRATE/2)/STEPRATE*STEPRATE);
}


int initAirspy(char **argv, int optind, thread_param_t * param)
{
	int n;
	char *argF;
	unsigned int F0,minFc=140000000,maxFc=0;
	unsigned int Fd[MAXNBCHANNELS];
	int result;
	uint32_t i,count;
	uint32_t * supported_samplerates;


	/* parse args */
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
                if(Fd[nbch]<minFc) minFc= Fd[nbch];
                if(Fd[nbch]>maxFc) maxFc= Fd[nbch];
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

        if( airspy_serial ) {
	    if (verbose>1) {
		fprintf(stderr, "Attempting to open airspy device 0x%016lx\n", airspy_serial);
	    }
	    result = airspy_open_sn(&device, airspy_serial);
	} else {
	    result = airspy_open(&device);
	}
        
        if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_open() failed: %s (%d)\n", airspy_error_name(result), result);
		return -1;
	}

	result = airspy_set_sample_type(device, AIRSPY_SAMPLE_FLOAT32_REAL);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_set_sample_type() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_close(device);
		return -1;
	}

	airspy_get_samplerates(device, &count, 0);
	supported_samplerates = (uint32_t *) malloc(count * sizeof(uint32_t));
	airspy_get_samplerates(device, supported_samplerates, count);
	for(i=0;i<count;i++) {
            if( (supported_samplerates[i] == 5000000) /* AirSpy R2 */
                    || (supported_samplerates[i] == 6000000)) /* AirSpy Mini */
            {
                SDRINRATE = supported_samplerates[i];
                SDRCLK = SDRINRATE/4000;
                break;
            }
        }

	if(i>=count) {
		fprintf(stderr,"did not find needed sampling rate\n");
		return -1;
	}
	free(supported_samplerates);

	result = airspy_set_samplerate(device, i);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_set_samplerate() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_close(device);
		return -1;
	}

       /* had problem with packing , disable it*/
        airspy_set_packing(device, 0);

        result = airspy_set_linearity_gain(device, gain);
        if( result != AIRSPY_SUCCESS ) {
                fprintf(stderr,"airspy_set_linearity_gain() failed: %s (%d)\n", airspy_error_name(result), result);
        }

	Fc = chooseFc(minFc, maxFc);
        if (Fc == 0) {
		fprintf(stderr,"Frequencies too far apart\n");
		airspy_close(device);
		airspy_exit();
                return 1;
	}

	result = airspy_set_freq(device, Fc);
	if( result != AIRSPY_SUCCESS ) {
		fprintf(stderr,"airspy_set_freq() failed: %s (%d)\n", airspy_error_name(result), result);
		airspy_close(device);
		return -1;
	}
	if (verbose>1)
		fprintf(stderr, "Set freq. to %d hz\n", Fc);

	/* compute mixers osc. */
	F0=Fc+SDRINRATE/4;
	for (n = 0; n < nbch; n++) {
                param[n].Fo = param[n].Fr-F0;
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
