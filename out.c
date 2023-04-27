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
extern int regout;
extern int verbose;

extern char *netOutJsonAddr;
extern char *netOutSbsAddr;

extern int outxid(flight_t *fl, unsigned char *p, int len);
extern int outacars(flight_t *fl, unsigned char *txt, int len);

cJSON *json_obj=NULL;

static int sock_json=-1;
static int sock_sbs=-1;


static char *outbuff=NULL,*ptroutbuff;

static int initNetOutput(int json_sbs)
{
	char *raddr,*addr;
	char *port;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int sockfd;

	memset(&hints, 0, sizeof hints);

        if(json_sbs) {
		if(netOutJsonAddr==NULL) return -1;
		raddr=strdup(netOutJsonAddr);
		hints.ai_socktype = SOCK_DGRAM;
	} else {
		if(netOutSbsAddr==NULL) return -1;
		raddr=strdup(netOutSbsAddr);
		hints.ai_socktype = SOCK_STREAM;
	}

	if (raddr[0] == '[') {
		hints.ai_family = AF_INET6;
		addr = raddr + 1;
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
		addr = raddr;
		port = strstr(addr, ":");
		if (port == NULL)
			port = "5555";
		else {
			*port = 0;
			port++;
		}
	}


	if ((rv = getaddrinfo(addr, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "Invalid/unknown address %s\n", addr);
		free(raddr);
		return -1;
	}

	sockfd=-1;
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
		continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			if(verbose > 1) fprintf(stderr, "failed to connect\n");
			close(sockfd);
			continue;
		}
		break;
	}

	free(raddr);
	freeaddrinfo(servinfo);
	
        if(json_sbs) 
		sock_json=sockfd;
	else
		sock_sbs=sockfd;

	if (sockfd == -1 ) 
		return -1;
	else
		return 0;
}

static int Netwrite(const void *buf, size_t count,int json_sbs) {
    int res;
    int *sockfd;

    if(json_sbs)
	    sockfd=&sock_json;
    else
	    sockfd=&sock_sbs;

    if(*sockfd == -1) {
        initNetOutput(json_sbs);
    }
    if(*sockfd == -1) return -1;

    res = write(*sockfd, buf, count);
    if (res != count) {
        close(*sockfd);
	*sockfd=-1;
    }
    return res;
}

static void outsbs(msgblk_t * blk,flight_t *fl)
{
	char outbuff[1024],*p;
	struct timespec now;
    	struct tm stTime_receive, stTime_now;

	if(fl->reg[0]==0 && fl->oooi.epu==0) return;

	clock_gettime(CLOCK_REALTIME, &now);
	gmtime_r(&now.tv_sec, &stTime_now);
        gmtime_r(&blk->tv.tv_sec, &stTime_receive);

	p=outbuff;

	if(fl->oooi.epu) {
		p+=sprintf(outbuff, "MSG,3,1,1,%06X,1,", fl->addr&0Xffffff);
	} else {
		p+=sprintf(outbuff, "MSG,1,1,1,%06X,1,", fl->addr&0Xffffff);
	}
	p += sprintf(p, "%04d/%02d/%02d,", (stTime_receive.tm_year + 1900), (stTime_receive.tm_mon + 1), stTime_receive.tm_mday);
	p += sprintf(p, "%02d:%02d:%02d.%03u,", stTime_receive.tm_hour, stTime_receive.tm_min, stTime_receive.tm_sec, (unsigned) (blk->tv.tv_usec/1000));
	p += sprintf(p, "%04d/%02d/%02d,", (stTime_now.tm_year + 1900), (stTime_now.tm_mon + 1), stTime_now.tm_mday);
	p += sprintf(p, "%02d:%02d:%02d.%03u", stTime_now.tm_hour, stTime_now.tm_min, stTime_now.tm_sec, (unsigned) (now.tv_nsec / 1000000U));
	if(fl->reg[0]) p += sprintf(p, ",%s", fl->reg); else p += sprintf(p, ",");
	if(fl->oooi.alt) p += sprintf(p, ",%d", fl->oooi.alt); else p += sprintf(p, ",");
	p += sprintf(p, ",,"); 
	if(fl->oooi.epu) p += sprintf(p, ",%1.6f,%1.6f", fl->oooi.lat, fl->oooi.lon); else p += sprintf(p, ",,");
	p += sprintf(p, ",,,,,,"); 
	if(fl->gnd) p += sprintf(p, "-1"); 
	p += sprintf(p, "\r\n"); 
       	Netwrite(outbuff, strlen(outbuff),0);
	//printf("%s",outbuff);
	//fflush(stdout);
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

        if (netOutJsonAddr) {
        	Netwrite(jsonbuf, lp+1,1);
        }
}

static void buildjsonobj(unsigned int faddr,unsigned int taddr,int fromair,int isresponse,int isonground,msgblk_t * blk)
{
        char convert_tmp[8];
        double t = (double)blk->tv.tv_sec + ((double)blk->tv.tv_usec)/1e6;

        json_obj = cJSON_CreateObject();
	if(json_obj==NULL) return ;
	
        cJSON_AddNumberToObject(json_obj, "timestamp", t);
        if(idstation[0]) cJSON_AddStringToObject(json_obj, "station_id", idstation);

        snprintf(convert_tmp, sizeof(convert_tmp), "%3.3f", blk->Fr/1000000.0);
        cJSON_AddRawToObject(json_obj, "freq", convert_tmp);

	if(fromair) {
        	snprintf(convert_tmp, sizeof(convert_tmp), "%06X", faddr & 0xffffff);
        	cJSON_AddStringToObject(json_obj, "hex", convert_tmp);
        	cJSON_AddNumberToObject(json_obj, "icao", faddr & 0xffffff);
        	cJSON_AddNumberToObject(json_obj, "toaddr", taddr & 0xffffff);
	} else {
        	cJSON_AddNumberToObject(json_obj, "fromaddr", faddr & 0xffffff);
        	cJSON_AddNumberToObject(json_obj, "icao", taddr & 0xffffff);
        	snprintf(convert_tmp, sizeof(convert_tmp), "%06X", taddr & 0xffffff);
        	cJSON_AddStringToObject(json_obj, "hex", convert_tmp);
	}

        if(isresponse) cJSON_AddNumberToObject(json_obj, "is_response", isresponse);
        if(isonground) cJSON_AddNumberToObject(json_obj, "is_onground", isonground);

	cJSON *app_info = cJSON_AddObjectToObject(json_obj, "app");
	if (app_info) {
		cJSON_AddStringToObject(app_info, "name", "vdlm2dec");
		cJSON_AddStringToObject(app_info, "ver", VDLM2DEC_VERSION);
	}
}


static  flight_t *addFlight(uint32_t addr, struct timeval tv)
{
	static flight_t  *flight_head=NULL;
        flight_t *fl,*fld,*flp;
	const static int mdly=1800;

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
                fl->rt=fl->gt=0;
                fl->next=NULL;
        }

        fl->tl=tv;
        fl->oooi.epu=fl->oooi.alt=0;
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
  double t;

  if(fl==NULL)
        return;

  if(fl->rt==0 && fl->fid[0] && fl->oooi.sa[0] && fl->oooi.da[0]) {

        json_obj = cJSON_CreateObject();
        if (json_obj == NULL)
                return ;

        t = (double)tv.tv_sec + ((double)tv.tv_usec)/1e6;
        cJSON_AddNumberToObject(json_obj, "timestamp", t);
        if(idstation[0]) cJSON_AddStringToObject(json_obj, "station_id", idstation);
        cJSON_AddStringToObject(json_obj, "flight", fl->fid);
        cJSON_AddStringToObject(json_obj, "depa", fl->oooi.sa);
        cJSON_AddStringToObject(json_obj, "dsta", fl->oooi.da);

        fl->rt=1;
 }

 if(fl->gt==0 && fl->reg[0]) {

	char hexicao[9];
	
	if(json_obj==NULL) {

        	json_obj = cJSON_CreateObject();
        	if (json_obj == NULL)
                	return ;

        	t = (double)tv.tv_sec + ((double)tv.tv_usec)/1e6;
        	cJSON_AddNumberToObject(json_obj, "timestamp", t);
        	if(idstation[0]) cJSON_AddStringToObject(json_obj, "station_id", idstation);
	}

	sprintf(hexicao,"%06X",fl->addr & 0xffffff);
       	cJSON_AddStringToObject(json_obj, "icao", hexicao);
        cJSON_AddStringToObject(json_obj, "tail", fl->reg);

        fl->gt=1;
 }

}

static void airreg(flight_t *fl,struct timeval tv)
{
  if(fl==NULL)
        return;

  if(fl->gt==0 && fl->reg[0]) {

	printf("%06X,%s\n",0Xffffff & fl->addr,fl->reg);
	fflush(stdout);

        fl->gt=1;
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
	unsigned int faddr,taddr;
	int dec,fromair;
	flight_t *fl=NULL;

	faddr=icaoaddr(&(hdata[5]));
	taddr=icaoaddr(&(hdata[1]));

	fromair=((faddr >> 24)==1);

	/* filter bad message */
	if(!grndmess && !fromair) return;
	if(!emptymess && l<=13)  return;
	if(!undecmess && fromair && ( (faddr & 0xffffff) == 0 || (faddr & 0xffffff) == 0xffffff)) return;

	if(fromair) {	
		fl=addFlight(faddr,blk->tv);
		fl->gnd = hdata[1] & 2;
	}

	if(verbose) {

		ptroutbuff=outbuff=malloc(OUTBUFLEN);

        	vout("\n[#%1d (F:%3.3f P:%+05.1f) ", blk->chn + 1, blk->Fr / 1000000.0, blk->ppm);
        	printdate(blk->tv);
        	vout( " --------------------------------\n");

		vout( "%s from ", rep ? "Response" : "Command");
		outaddr(faddr);
		vout( "(%s) to ", (fl && fl->gnd) ? "on ground" : "airborne");
		outaddr(taddr);
		vout( "\n");

		outlinkctrl(hdata[9], rep);
	}

	if((jsonout || netOutJsonAddr) && !routeout) {
		buildjsonobj(faddr,taddr,fromair,rep,(fl && fl->gnd),blk);
    }

	dec=0;

	if (l >= 14 && hdata[10] == 0x82) {
		dec|=outxid(fl,&(hdata[11]), l - 14);
	}

	if (l >= 16 && hdata[10] == 0xff && hdata[11] == 0xff && hdata[12] == 1) {
		dec|=outacars(fl,&(hdata[13]), l - 16);
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

	if(fl) {
		if(routeout) routejson(fl,blk->tv);
		if(regout) airreg(fl,blk->tv);
        	if (netOutSbsAddr) outsbs(blk,fl);
	}

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
