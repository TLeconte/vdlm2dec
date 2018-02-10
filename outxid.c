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
#include "vdlm2.h"
#include "acars.h"
#include "cJSON.h"

extern int verbose;
extern char *idstation;

extern int jsonout;
extern char* jsonbuf;
extern int sockfd;
extern  void outjson(void);

extern void dumpdata(unsigned char *p, int len);

static unsigned int geticaoaddr(unsigned char *p)
{
 unsigned int addr =
	    (reversebits(p[0] >> 2, 6) << 21) |
	    (reversebits(p[1] >> 1, 7) << 14) |
	    (reversebits(p[2] >> 1, 7) << 7) |
	    (reversebits(p[3] >> 1, 7));
 return addr;
}

static void getlatlon(unsigned char *p,  float *lat, float *lon)
{
short slat,slon;

slat=(((short)p[0])<<8) | (unsigned short)(p[1] & 0xf0);
slon=(((short)(p[1] & 0x0f))<< 12) | ((unsigned short)(p[2]) <<4);

*lat=(float)slat/160.0; 
*lon=(float)slon/160.0; 
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
				fprintf(logfd, "HO|");
			else if (p[i + 2] & 2)
				fprintf(logfd, "LCR|");
			else
				fprintf(logfd, "LE|");
			if (p[i + 2] & 4)
				fprintf(logfd, "GDA|");
			else
				fprintf(logfd, "VDA|");
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
				int n=0;

				fprintf(logfd, "Acceptable alternative ground stations : ");

				while(n<p[i+1]) {
				 addr = geticaoaddr(&(p[i+2+n]));
				 fprintf(logfd,"%06X ", addr & 0xffffff);
				 n+=4;
				}
				fprintf(logfd,"\n");
				break;
			}
		case 0x83:{
				char da[5];
				da[4]='\0';
				memcpy(da,&(p[i+2]),4);
				fprintf(logfd, "Destination airport %s\n", da);
				break;
			}
		case 0x84:{
				int alt;
				float lat, lon;

				getlatlon(&(p[i + 2]),&lat,&lon);
				alt = p[i + 5] * 1000;
				fprintf(logfd, "Aircraft Position %4.1f %5.1f ", lat, lon );
				if(alt==0)
					fprintf(logfd, "alt: <=999\n");
				else if(alt==255000)
					fprintf(logfd, "alt: >=255000\n");
					else fprintf(logfd, "alt: %d\n",alt);
				break;
			}
		case 0xc0:{
				unsigned int addr,mod,freq;
				int n=0;

				fprintf(logfd, "Frequency support : ");

				while(n<p[i+1]) {
				 mod = (uint32_t) (p[i+2+n]&0xf0) >> 4;
				 freq = ((uint32_t) (p[i+2+n]&0x0f) << 8) | ((uint32_t) (p[i + 3])) ;
				 addr = geticaoaddr(&(p[i+4+n]));
				 fprintf(logfd,"%03.2f (%01X) %06X ", (float)(freq+10000)/100.0,mod&0x0f,addr & 0xffffff);
				 n+=6;
				}
				fprintf(logfd,"\n");
				break;
			}
		case 0xc1:{
				char id[5];
				int n=0;

				id[4]='\0';

				fprintf(logfd, "Airport coverage : ");

				while(n<p[i+1]) {
				 memcpy(id,&(p[i+2+n]),4);
				 fprintf(logfd,"%s ",id);
				 n+=4;
				}
				fprintf(logfd,"\n");
				break;
			}
		case 0xc3:{
				char id[5];
				id[4]='\0';

				memcpy(id,&(p[i+2]),4);
				fprintf(logfd, "Nearest Airport : %s\n",id);
				break;
			}
		case 0xc4:{
				unsigned int adm,ars;
				
				adm= ((uint32_t) (p[i + 2]) << 16) | ((uint32_t) (p[i + 3]) << 8) | ((uint32_t) (p[i + 4]));
				ars= ((uint32_t) (p[i + 5]) << 16) | ((uint32_t) (p[i + 6]) << 8) | ((uint32_t) (p[i + 7]));
				fprintf(logfd, "ATN router nets : ADM: %06X ARS : %06X\n",adm,ars);
				break;
			}
		case 0xc5:{
				unsigned int mask;
				mask= geticaoaddr(&(p[i+2]));

				fprintf(logfd, "Station system mask : %06X\n", mask & 0xffffff);
				break;
			}
		case 0xc8:{
				float lat, lon;

				getlatlon(&(p[i + 2]),&lat,&lon);
				fprintf(logfd, "Station Position %4.1f %5.1f\n", lat, lon) ;
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

	fflush(logfd);
}

static int buildxidjson(unsigned int vaddr,unsigned char *p, int len, int chn, int freqb, struct timeval tv)
{
	int i;
	char da[5];
	int alt, pos=0;
	float lat, lon;
	int ok = 0;
	cJSON *json_obj;
	char convert_tmp[8];
#if defined (WITH_RTL) || defined (WITH_AIR)
        float freq = freqb / 1000000.0;
#else
        float freq = 0;
#endif

	i = 0;
	da[0]=da[4]='\0';
	do {
		short len = p[i + 1];

		if (p[i] == 0x83) {
			memcpy(da,&(p[i+2]),4);
		}
		if (p[i] == 0x84) {
			getlatlon(&(p[i + 2]),&lat,&lon);
			alt = p[i + 5] * 1000;
			pos=1;
		}
		i += 2 + len;
	} while (i < len);
	if(da[0]=='\0' && pos==0) return 0;

	json_obj = cJSON_CreateObject();
	if (json_obj == NULL)
		return 0;

	double t = (double)tv.tv_sec + ((double)tv.tv_usec)/1e6;
	cJSON_AddNumberToObject(json_obj, "timestamp", t);
	cJSON_AddStringToObject(json_obj, "station_id", idstation);

	cJSON_AddNumberToObject(json_obj, "channel", chn);
        snprintf(convert_tmp, sizeof(convert_tmp), "%3.3f", freq);
        cJSON_AddRawToObject(json_obj, "freq", convert_tmp);

        cJSON_AddNumberToObject(json_obj, "icao", vaddr & 0xffffff);

	if(da[0]) cJSON_AddStringToObject(json_obj, "dsta", da);

	if(pos) {
		cJSON_AddNumberToObject(json_obj, "lat", lat);
		cJSON_AddNumberToObject(json_obj, "lon", lon);
		cJSON_AddNumberToObject(json_obj, "alt", alt);
	}

	ok = cJSON_PrintPreallocated(json_obj, jsonbuf, JSONBUFLEN, 0);
	cJSON_Delete(json_obj);
	return ok;
}

void outxid(unsigned int vaddr, msgblk_t * blk,unsigned char *p, int len)
{
	int i;

	i = 0;
	do {
		short glen = p[i + 1] * 256 + p[i + 2];

		if (p[i] == 0x80) {
			// don't mess with public parameters
			i += 3 + glen;
			continue;
		}
		if (p[i] == 0xf0) {
			if(verbose)
				outprivategr(&(p[i + 3]), glen);

			// build the JSON buffer if needed
			if(jsonbuf)
				buildxidjson(vaddr,&(p[i + 3]), glen, blk->chn, blk->Fr,blk->tv);

			if(jsonout) {
				fprintf(logfd, "%s\n", jsonbuf);
				fflush(logfd);
			}

			if (sockfd > 0) 
				outjson();

			i += 3 + glen;
			break;
		}
		if (verbose > 1) {
			fprintf(logfd, "unknown group %02x\n", p[i]);
			dumpdata(&(p[i]), glen + 3);
			fflush(logfd);
		}
		i += 3 + glen;
	} while (i < len);
}


