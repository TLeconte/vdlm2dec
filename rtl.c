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
#include <string.h>
#include <pthread.h>
#include <sys/resource.h>

#include <math.h>
#include <rtl-sdr.h>
#include "vdlm2.h"

static const float complex wf[RTLMULT] = {
    -0.99144 + 0.13053i, -0.92388 + 0.38268i, -0.79335 + 0.60876i,
    -0.60876 + 0.79335i, -0.38268 + 0.92388i, -0.13053 + 0.99144i,
    0.13053 + 0.99144i, 0.38268 + 0.92388i, 0.60876 + 0.79335i,
    0.79335 + 0.60876i, 0.92388 + 0.38268i, 0.99144 + 0.13053i,
    0.99144 - 0.13053i, 0.92388 - 0.38268i, 0.79335 - 0.60876i,
    0.60876 - 0.79335i, 0.38268 - 0.92388i, 0.13053 - 0.99144i,
    -0.13053 - 0.99144i, -0.38268 - 0.92388i, -0.60876 - 0.79335i,
    -0.79335 - 0.60876i, -0.92388 - 0.38268i, -0.99144 - 0.13053i
};

static rtlsdr_dev_t *dev = NULL;

int nearest_gain(int target_gain)
{
    int i, err1, err2, count, close_gain;
    int *gains;
    count = rtlsdr_get_tuner_gains(dev, NULL);
    if (count <= 0) {
        return 0;
    }
    gains = malloc(sizeof(int) * count);
    count = rtlsdr_get_tuner_gains(dev, gains);
    close_gain = gains[0];
    for (i = 0; i < count; i++) {
        err1 = abs(target_gain - close_gain);
        err2 = abs(target_gain - gains[i]);
        if (err2 < err1) {
            close_gain = gains[i];
        }
    }
    free(gains);
    if (verbose)
        fprintf(stderr, "Tuner gain : %f\n", (float)close_gain / 10.0);
    return close_gain;
}

int initRtl(int dev_index, int gain, int freq)
{
    int r, n;

    n = rtlsdr_get_device_count();
    if (!n) {
        fprintf(stderr, "No supported devices found.\n");
        exit(1);
    }

    if (verbose)
        fprintf(stderr, "Using device %d: %s\n", dev_index, rtlsdr_get_device_name(dev_index));

    r = rtlsdr_open(&dev, dev_index);
    if (r < 0) {
        fprintf(stderr, "Failed to open rtlsdr device\n");
        return r;
    }

    rtlsdr_set_tuner_gain_mode(dev, 1);
    r = rtlsdr_set_tuner_gain(dev, nearest_gain(gain));
    if (r < 0)
        fprintf(stderr, "WARNING: Failed to set gain.\n");

    r = rtlsdr_set_center_freq(dev, freq - 10500 * 2 * RTLDWN);
    if (r < 0) {
        fprintf(stderr, "WARNING: Failed to set center freq.\n");
    }

    r = rtlsdr_set_sample_rate(dev, RTLINRATE);
    if (r < 0) {
        fprintf(stderr, "WARNING: Failed to set sample rate.\n");
    }

    r = rtlsdr_reset_buffer(dev);
    if (r < 0) {
        fprintf(stderr, "WARNING: Failed to reset buffers.\n");
    }

    return 0;
}

void in_callback(unsigned char *rtlinbuff, unsigned int nread, void *ctx)
{
    int r, n;
    int i;

    if (nread == 0) {
        return;
    }

    if (nread % (RTLMULT * 2) != 0) {
        fprintf(stderr, "buff error\n");
        return;
    }
    for (i = 0; i < nread;) {
        float complex D;
        int k;

        D = 0;
        for (k = 0; k < RTLMULT; k++) {
            float si, sq;
            complex float S;

            si = (float)rtlinbuff[i] - 127.5;
            i++;
            sq = (float)rtlinbuff[i] - 127.5;
            i++;
            S = si + sq * I;
            D += S * wf[k];
        }
        demodD8psk(D / RTLMULT / 128.0);
    }
}

int runRtlSample(void)
{
    int r;

    setpriority(PRIO_PROCESS, 0, -10);
    while (1) {
        rtlsdr_read_async(dev, in_callback, NULL, 8, INBUFSZ);
    }

    return r;
}

void resetRtl(void)
{
    rtlsdr_cancel_async(dev);
}
