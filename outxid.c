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
extern cJSON *json_obj;

extern void dumpdata(unsigned char *p, int len);

extern unsigned int icaoaddr(unsigned char *p);

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
			vout( "Connection management: ");
			if (p[i + 2] & 1)
				vout( "HO|");
			else if (p[i + 2] & 2)
				vout( "LCR|");
			else
				vout( "LE|");
			if (p[i + 2] & 4)
				vout( "GDA|");
			else
				vout( "VDA|");
			if (p[i + 2] & 8)
				vout( "ESS\n");
			else
				vout( "ESN\n");
			break;
		case 0x02:
			vout( "Signal quality %01d\n", p[i + 2]);
			break;
		case 0x03:
			vout( "XID sequencing %1d:%1d\n",
				p[i + 2] >> 4, p[i + 2] & 0x7);
			break;
		case 0x04:
			vout( "Specific options: ");
			if (p[i + 2] & 1)
				vout( "GDA:");
			else
				vout( "VDA:");
			if (p[i + 2] & 2)
				vout( "ESS:");
			else
				vout( "ESN:");
			if (p[i + 2] & 4)
				vout( "IHS:");
			else
				vout( "IHN:");
			if (p[i + 2] & 8)
				vout( "BHS:");
			else
				vout( "BHN:");
			if (p[i + 2] & 0x10)
				vout( "BCS\n");
			else
				vout( "BCN\n");
			break;
		case 0x05:
			vout( "Expedited subnetwork connection %02x\n",
				p[i + 2]);
			break;
		case 0x06:
			vout( "LCR cause %02x\n", p[i + 2]);
			break;
		case 0x81:
			vout( "Modulation support %02x\n", p[i + 2]);
			break;
		case 0x82:{
				unsigned int addr;
				int n=0;

				vout( "Acceptable alternative ground stations : ");

				while(n<p[i+1]) {
				 addr = icaoaddr(&(p[i+2+n]));
				 vout("%06X ", addr & 0xffffff);
				 n+=4;
				}
				vout("\n");
				break;
			}
		case 0x83:{
				char da[5];
				da[4]='\0';
				memcpy(da,&(p[i+2]),4);
				vout( "Destination airport %s\n", da);
				break;
			}
		case 0x84:{
				int alt;
				float lat, lon;

				getlatlon(&(p[i + 2]),&lat,&lon);
				alt = p[i + 5] * 1000;
				vout( "Aircraft Position %5.1f %6.1f ", lat, lon );
				if(alt==0)
					vout( "alt: <=999\n");
				else if(alt==255000)
					vout( "alt: >=255000\n");
					else vout( "alt: %d\n",alt);
				break;
			}
		case 0xc0:{
				unsigned int addr,mod,freq;
				int n=0;

				vout( "Frequency support : ");

				while(n<p[i+1]) {
				 mod = (uint32_t) (p[i+2+n]&0xf0) >> 4;
				 freq = ((uint32_t) (p[i+2+n]&0x0f) << 8) | ((uint32_t) (p[i + 3])) ;
				 addr = icaoaddr(&(p[i+4+n]));
				 vout("%03.2f (%01X) %06X ", (float)(freq+10000)/100.0,mod&0x0f,addr & 0xffffff);
				 n+=6;
				}
				vout("\n");
				break;
			}
		case 0xc1:{
				char id[5];
				int n=0;

				id[4]='\0';

				vout( "Airport coverage : ");

				while(n<p[i+1]) {
				 memcpy(id,&(p[i+2+n]),4);
				 vout("%s ",id);
				 n+=4;
				}
				vout("\n");
				break;
			}
		case 0xc3:{
				char id[5];
				id[4]='\0';

				memcpy(id,&(p[i+2]),4);
				vout( "Nearest Airport : %s\n",id);
				break;
			}
		case 0xc4:{
				unsigned int adm,ars;
				
				adm= ((uint32_t) (p[i + 2]) << 16) | ((uint32_t) (p[i + 3]) << 8) | ((uint32_t) (p[i + 4]));
				ars= ((uint32_t) (p[i + 5]) << 16) | ((uint32_t) (p[i + 6]) << 8) | ((uint32_t) (p[i + 7]));
				vout( "ATN router nets : ADM: %06X ARS : %06X\n",adm,ars);
				break;
			}
		case 0xc5:{
				unsigned int mask;
				mask= icaoaddr(&(p[i+2]));

				vout( "Station system mask : %06X\n", mask & 0xffffff);
				break;
			}
		case 0xc8:{
				float lat, lon;

				getlatlon(&(p[i + 2]),&lat,&lon);
				vout( "Station Position %4.1f %5.1f\n", lat, lon) ;
				break;
			}
		default:
			vout( "unknown private id %02x\n", p[i]);
			if (verbose > 1) {
				dumpdata(&(p[i]), len + 2);
			}

			break;
		}
		i += 2 + len;
	} while (i < len);

	fflush(logfd);
}

static void buildxidjson(unsigned char *p, int len)
{
	int i;
	char da[5];
	int alt=0, pos=0;
	float lat=0, lon=0;

	i = 0;
	da[0]=da[4]='\0';
	do {
		short slen = p[i + 1];

		if (p[i] == 0x83) {
			memcpy(da,&(p[i+2]),4);
		}
		if (p[i] == 0x84) {
			getlatlon(&(p[i + 2]),&lat,&lon);
			alt = p[i + 5] * 1000;
			if(lat!=0 || lon !=0)  pos=1;
		}
		i += 2 + slen;
	} while (i < len);

	if(da[0]) cJSON_AddStringToObject(json_obj, "dsta", da);

	if(pos) {
                char convert_tmp[10];
                snprintf(convert_tmp, sizeof(convert_tmp), "%3.1f", lat);
                cJSON_AddRawToObject(json_obj, "lat", convert_tmp);
                snprintf(convert_tmp, sizeof(convert_tmp), "%4.1f", lon);
                cJSON_AddRawToObject(json_obj, "lon", convert_tmp);
		cJSON_AddNumberToObject(json_obj, "epu", 6 );

		if(alt) cJSON_AddNumberToObject(json_obj, "alt", alt);
	}
}

void addda(flight_t *fl, unsigned char *p, int len)
{
	int i;
	i = 0;

	do {
		short slen = p[i + 1];

		if (p[i] == 0x83) {
			memcpy(fl->oooi.da,&(p[i+2]),4);
		}
		i += 2 + slen;
	} while (i < len);

}

int outxid(flight_t *fl, unsigned char *p, int len)
{
	int i,dec;

	dec=0;

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

			if(json_obj)
				buildxidjson(&(p[i + 3]), glen);

			addda(fl,&(p[i + 3]), glen);

			i += 3 + glen;
			dec=1;
			break;
		}
		if (verbose > 1) {
			vout( "unknown group %02x\n", p[i]);
			dumpdata(&(p[i]), glen + 3);
		}
		i += 3 + glen;
	} while (i < len);

     return dec;
}


