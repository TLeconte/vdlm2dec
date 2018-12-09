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
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <time.h>
#include <netdb.h>

#include "vdlm2.h"
#include "acars.h"
#include "cJSON.h"

extern char *idstation;
extern int jsonout;
extern int routeout;
extern int mdly;

extern int outxid(flight_t *fl, unsigned char *p, int len);
extern int outacars(flight_t *fl, unsigned char *txt, int len);

cJSON *json_obj=NULL;
static int sockfd=-1;

static char *outbuff=NULL,*ptroutbuff;

int initNetOutput(char *Rawaddr)
{
	char *addr;
	char *port;
	struct addrinfo hints, *servinfo, *p;
	int rv;

		memset(&hints, 0, sizeof hints);
		if (Rawaddr[0] == '[') {
			hints.ai_family = AF_INET6;
			addr = Rawaddr + 1;
			port = strstr(addr, "]");
			if (port == NULL) {
				fprintf(stderr, "Invalid IPV6 address\n");
				return -1;
			}
			*port = 0;
			port++;
			if (*port != ':')
				port = "5555";
			else
				port++;
		} else {
			hints.ai_family = AF_UNSPEC;
			addr = Rawaddr;
			port = strstr(addr, ":");
			if (port == NULL)
				port = "5555";
			else {
				*port = 0;
				port++;
			}
		}

		hints.ai_socktype = SOCK_DGRAM;

		if ((rv = getaddrinfo(addr, port, &hints, &servinfo)) != 0) {
			fprintf(stderr, "Invalid/unknown address %s\n", addr);
			return -1;
		}

		for (p = servinfo; p != NULL; p = p->ai_next) {
			if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			continue;
			}

			if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				close(sockfd);
				continue;
			}
			break;
		}
		if (p == NULL) {
			fprintf(stderr, "failed to connect\n");
			return -1;
		}

		freeaddrinfo(servinfo);
	
	return 0;
}

static void outjson()
{
	int ok;
	int lp;
	char jsonbuf[JSONBUFLEN];

        ok = cJSON_PrintPreallocated(json_obj, jsonbuf, JSONBUFLEN, 0);
        cJSON_Delete(json_obj);
	json_obj=NULL;

	if(!ok) return;
	
	lp=strlen(jsonbuf);
	jsonbuf[lp]='\n'; jsonbuf[lp+1]='\0';

        if(jsonout) {
               fwrite(jsonbuf,1,lp+1,logfd);
               fflush(logfd);
        }

        if (sockfd > 0 ) {
        	ok=write(sockfd, jsonbuf, lp+1);
	}

}

static void buildjsonobj(unsigned int faddr,unsigned int taddr,int fromair,msgblk_t * blk)
{
        char convert_tmp[8];
        double t = (double)blk->tv.tv_sec + ((double)blk->tv.tv_usec)/1e6;

        json_obj = cJSON_CreateObject();
	if(json_obj==NULL) return ;
	
        cJSON_AddNumberToObject(json_obj, "timestamp", t);
        if(idstation[0]) cJSON_AddStringToObject(json_obj, "station_id", idstation);

        cJSON_AddNumberToObject(json_obj, "channel", blk->chn);

        snprintf(convert_tmp, sizeof(convert_tmp), "%3.3f", blk->Fr/1000000.0);
        cJSON_AddRawToObject(json_obj, "freq", convert_tmp);

	if(fromair) {
        	cJSON_AddNumberToObject(json_obj, "icao", faddr & 0xffffff);
        	cJSON_AddNumberToObject(json_obj, "toaddr", taddr & 0xffffff);
	} else {
        	cJSON_AddNumberToObject(json_obj, "fromaddr", faddr & 0xffffff);
        	cJSON_AddNumberToObject(json_obj, "icao", taddr & 0xffffff);
	}
}


static  flight_t *addFlight(uint32_t addr, struct timeval tv)
{
	static flight_t  *flight_head=NULL;
        flight_t *fl,*fld,*flp;

	/* find aircraft */
        fl=flight_head;
        flp=NULL;
        while(fl) {
                if(addr==fl->addr) break;
                flp=fl;
                fl=fl->next;
        }

        if(fl==NULL) {
                fl=calloc(1,sizeof(flight_t));
                fl->addr=addr;
                fl->nbm=0;
                fl->ts=tv;
                fl->rt=0;
                fl->next=NULL;
        }

        fl->tl=tv;
        fl->nbm+=1;

        if(flp) {
                flp->next=fl->next;
                fl->next=flight_head;
        }
        flight_head=fl;

	/* remove olds */
        flp=NULL;fld=fl;
        while(fld) {
                if(fld->tl.tv_sec<(tv.tv_sec-mdly)) {
                        if(flp) {
                                flp->next=fld->next;
                                free(fld);
                                fld=flp->next;
                        } else {
                                flight_head=fld->next;
                                free(fld);
                                fld=flight_head;
                        }
                } else {
                        flp=fld;
                        fld=fld->next;
                }
        }

        return(fl);
}

static void routejson(flight_t *fl,struct timeval tv)
{
  if(fl==NULL)
        return;

  if(fl->rt==0 && fl->fid[0] && fl->oooi.sa[0] && fl->oooi.da[0]) {

        json_obj = cJSON_CreateObject();
        if (json_obj == NULL)
                return ;

        double t = (double)tv.tv_sec + ((double)tv.tv_usec)/1e6;
        cJSON_AddNumberToObject(json_obj, "timestamp", t);
        if(idstation[0]) cJSON_AddStringToObject(json_obj, "station_id", idstation);
        cJSON_AddStringToObject(json_obj, "flight", fl->fid);
        cJSON_AddStringToObject(json_obj, "depa", fl->oooi.sa);
        cJSON_AddStringToObject(json_obj, "dsta", fl->oooi.da);

        fl->rt=1;
 }
}

void vout(char *format, ...)
{
   va_list va;

   if(outbuff==NULL) return;

   va_start(va, format);

   vsprintf(ptroutbuff,format,va);
   ptroutbuff+=strlen(ptroutbuff);

}


void dumpdata(unsigned char *p, int len)
{
	int i, k;

	for (i = 0; i < len; i += 16) {
		for (k = 0; k < 16; k++)
			if (i + k < len)
				vout( "%02hhx ", p[i + k]);
			else
				vout( "   ");
		vout( "   |");
		for (k = 0; k < 16; k++)
			if (i + k < len && p[i + k] >= ' ' && p[i + k] <= '~')
				vout( "%c", p[i + k]);
			else
				vout( ".");
		vout( "|\n");
	}
}

static void outundec(unsigned char *p, int len)
{
 char *mbuff;

 if(json_obj) {
	int i;
	mbuff=malloc(2*len+1);
	if(mbuff==0) return;
	for (i = 0; i < len; i++) 
		sprintf(&(mbuff[2*i]), "%02hhx ", p[i]);
	mbuff[2*i]='\0';
        cJSON_AddStringToObject(json_obj, "data", mbuff);
	free(mbuff);
 }

 if(verbose>1)
 	dumpdata(p,len);

}

unsigned int icaoaddr(unsigned char *hdata)
{
    unsigned int addr =
    (reversebits(hdata[0] >> 2, 6) << 21) |
    (reversebits(hdata[1] >> 1, 7) << 14) |
    (reversebits(hdata[2] >> 1, 7) << 7) |
    (reversebits(hdata[3] >> 1, 7));

    return addr;
}

static void outaddr(unsigned int addr)
{
	unsigned int type = addr >> 24;

	addr = addr & 0xffffff;

	switch(type) {
		case 0:
			vout( "T%1d:%06X ", type,addr);
			break;
		case 1:
			vout( "Aircraft:%06X ", addr);
			break;
		case 2:
		case 3:
			vout( "T%1d:%06X ", type,addr);
			break;
		case 4:
			vout( "GroundA:%06X ", addr);
			break;
		case 5:
			vout( "GroundD:%06X ", addr);
			break;
		case 6:
			vout( "T%1d:%06X ", type,addr);
			break;
		case 7:
			vout( "All ");
			break;

	}

}

static const char *Sfrm[4] = { "RR", "RNR", "REJ", "SREJ" };

static const char *Ufrm[2][32] = {
	{"UI", "SIM", "0x02", "SARM", "UP", "0x05", "0x06", "SABM",
	 "DISC", "0x09", "0x0a", "SARME", "0x0c", "0x0d", "0x0e", "SABME",
	 "SNRM", "0x11", "0x12", "RSET", "0x14", "0x15", "0x16", "XID",
	 "0x18", "0x19", "0x1a", "SNRME", "TEST", "0x1d", "0x1e", "0x1f"},
	{"UI", "RIM", "0x02", "DM", "0x04", "0x05", "0x06", "0x07",
	 "RD", "0x09", "0x0a", "0x0b", "UA", "0x0d", "0x0e", "0x0f",
	 "0x10", "FRMR", "0x12", "0x13", "0x14", "0x15", "0x16", "XID",
	 "0x18", "0x19", "0x1a", "0x1b", "TEST", "0x1d", "0x1e", "0x1f"}
};

static void outlinkctrl(unsigned char lc, int rep)
{

	vout( "Frame-");
	if (lc & 1) {
		if (lc & 2) {
			vout( "U: ");
			vout( "%s\n",
				Ufrm[rep][((lc >> 3) & 0x1c) |
					  ((lc >> 2) & 0x3)]);
		} else {
			vout( "S: ");
			vout( "Nr:%01d %s\n", (lc >> 5) & 0x7,
				Sfrm[(lc >> 2) & 0x3]);
		}
	} else {
		vout( "I: ");
		vout( "Ns:%01d Nr:%01d\n", (lc >> 1) & 0x7,
			(lc >> 5) & 0x7);
	}
}

static void printdate(struct timeval tv)
{
        struct tm tmp;

        gmtime_r(&(tv.tv_sec), &tmp);

        vout( "%02d/%02d/%04d %02d:%02d:%02d.%03d",
                tmp.tm_mday, tmp.tm_mon + 1, tmp.tm_year + 1900,
                tmp.tm_hour, tmp.tm_min, tmp.tm_sec,(int)tv.tv_usec/1000);
}


void out(msgblk_t * blk, unsigned char *hdata, int l)
{
	int rep = (hdata[5] & 2) >> 1;
	int gnd = hdata[1] & 2;
	unsigned int faddr,taddr;
	int dec,fromair;
	flight_t *fl=NULL;

	faddr=icaoaddr(&(hdata[5]));
	taddr=icaoaddr(&(hdata[1]));

	fromair=((faddr >> 24)==1);

	if(!grndmess && !fromair) return;
	if(!emptymess && l<=13)  return;

	if(fromair)
		fl=addFlight(faddr,blk->tv);

	if(verbose) {

		ptroutbuff=outbuff=malloc(OUTBUFLEN);

        	vout("\n[#%1d (F:%3.3f P:%+05.1f) ", blk->chn + 1, blk->Fr / 1000000.0, blk->ppm);
        	printdate(blk->tv);
        	vout( " --------------------------------\n");

		vout( "%s from ", rep ? "Response" : "Command");
		outaddr(faddr);
		vout( "(%s) to ", gnd ? "on ground" : "airborne");
		outaddr(taddr);
		vout( "\n");

		outlinkctrl(hdata[9], rep);
	}

	if((jsonout || sockfd >0) && !routeout) 
		buildjsonobj(faddr,taddr,fromair,blk);

	dec=0;

	if (l >= 16 && hdata[10] == 0xff && hdata[11] == 0xff && hdata[12] == 1) {
		dec|=outacars(fl,&(hdata[13]), l - 16);
	}

	if (l >= 14 && hdata[10] == 0x82) {
		dec|=outxid(fl,&(hdata[11]), l - 14);
	}

	if(l>13 && dec==0 && undecmess) {
		if(verbose) 
			vout( "unknown data\n");
		if(json_obj || verbose>1)
			outundec(&(hdata[10]), l - 13);
	}

	if(l>13 && dec==0 && !undecmess && json_obj) {
        	cJSON_Delete(json_obj);
		json_obj=NULL;
	}

	if(fl)
		routejson(fl,blk->tv);

	if(json_obj)
             outjson();

	if(verbose) {
		if(dec || undecmess) {
			fwrite(outbuff,ptroutbuff-outbuff,1,logfd);
			fflush(logfd);
		}
		free(outbuff);outbuff=NULL;
	}
}
