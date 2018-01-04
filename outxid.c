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
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include "acars.h"
#include "vdlm2.h"

extern int verbose;

void outpublicgr(unsigned char *p, int len)
{
	int i;

	i = 0;
	do {
		short len = p[i + 1];

		switch (p[i]) {
		case 0:
			break;
		case 0x01:
			fprintf(logfd, "Parameter set ID\n");
			break;
		case 0x02:
			fprintf(logfd, "Procedure classes\n");
			break;
		case 0x03:
			fprintf(logfd, "HDLC options\n");
			break;
		case 0x09:
			fprintf(logfd, "Timer T1 downlink\n");
			break;
		default:
			fprintf(logfd, "unknown public id %02x\n", p[i]);
			if (verbose > 1) {
				dumpdata(&(p[i]), len + 2);
			}
			break;
		}
		i += 2 + len;
	} while (i < len);
}

void outprivategr(unsigned char *p, int len)
{
	int i;

	i = 0;
	do {
		short len = p[i + 1];

		switch (p[i]) {
		case 0:
			break;
		case 0x01:
			fprintf(logfd, "Connection management: ");
			if (p[i + 2] & 1)
				fprintf(logfd, "HO  ");
			else if (p[i + 2] & 2)
				fprintf(logfd, "LCR ");
			else
				fprintf(logfd, "LE  ");
			if (p[i + 2] & 4)
				fprintf(logfd, "GDA:");
			else
				fprintf(logfd, "VDA:");
			if (p[i + 2] & 8)
				fprintf(logfd, "ESS\n");
			else
				fprintf(logfd, "ESN\n");
			break;
		case 0x02:
			fprintf(logfd, "Signal quality %01d\n", p[i + 2]);
			break;
		case 0x03:
			fprintf(logfd, "XID sequencing %1d:%1d\n",
				p[i + 2] >> 4, p[i + 2] & 0x7);
			break;
		case 0x04:
			fprintf(logfd, "Specific options: ");
			if (p[i + 2] & 1)
				fprintf(logfd, "GDA:");
			else
				fprintf(logfd, "VDA:");
			if (p[i + 2] & 2)
				fprintf(logfd, "ESS:");
			else
				fprintf(logfd, "ESN:");
			if (p[i + 2] & 4)
				fprintf(logfd, "IHS:");
			else
				fprintf(logfd, "IHN:");
			if (p[i + 2] & 8)
				fprintf(logfd, "BHS:");
			else
				fprintf(logfd, "BHN:");
			if (p[i + 2] & 0x10)
				fprintf(logfd, "BCS\n");
			else
				fprintf(logfd, "BCN\n");
			break;
		case 0x05:
			fprintf(logfd, "Expedited subnetwork connection %02x\n",
				p[i + 2]);
			break;
		case 0x06:
			fprintf(logfd, "LCR cause %02x\n", p[i + 2]);
			break;
		case 0x81:
			fprintf(logfd, "Modulation support %02x\n", p[i + 2]);
			break;
		case 0x82:{
				unsigned int addr;
				addr =
				    (reversebits(p[i + 2] >> 2, 6) << 21) |
				    (reversebits(p[i + 3] >> 1, 7) << 14) |
				    (reversebits(p[i + 4] >> 1, 7) << 7) |
				    (reversebits(p[i + 5] >> 1, 7));
				fprintf(logfd,
					"Acceptable alternative ground station %06x\n",
					addr & 0xffffff);
				break;
			}
		case 0x83:{
				oooi_t oooi;
				memset(&oooi, 0, sizeof(oooi));
				memcpy(oooi.da, &(p[i + 2]), 4);
				fprintf(logfd, "Destination airport %s\n",
					oooi.da);
				break;
			}
		case 0x84:{
				int alt;
				short lat, lon;
				lat =
				    (((uint32_t) (p[i + 2]) << 8) |
				     (p[i + 3] & 0xf0));
				lon =
				    (((uint32_t) (p[i + 3] & 0xf) << 12) |
				     (uint32_t) (p[i + 4]) << 4);
				alt = p[i + 5] * 1000;
				fprintf(logfd,
					"Aircraft Position   %4.1f %5.1f alt:%d\n",
					(float)lat / 160.0, (float)lon / 160.0,
					alt);
				break;
			}
		default:
			fprintf(logfd, "unknown private id %02x\n", p[i]);
			if (verbose > 1) {
				dumpdata(&(p[i]), len + 2);
			}

			break;
		}
		i += 2 + len;
	} while (i < len);
}

void outxid(unsigned char *p, int len)
{
	int i;

	i = 0;
	do {
		short glen = p[i + 1] * 256 + p[i + 2];

		if (p[i] == 0x80) {
			outpublicgr(&(p[i + 3]), glen);
			i += 3 + glen;
			continue;
		}
		if (p[i] == 0xf0) {
			outprivategr(&(p[i + 3]), glen);
			i += 3 + glen;
			continue;
		}
		if (verbose > 1) {
			fprintf(logfd, "unknown group %02x\n", p[i]);
			dumpdata(&(p[i]), glen + 3);
		}
		i += 3 + glen;
	} while (i < len);
}
