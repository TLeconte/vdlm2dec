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

char *jsonbuf=NULL;

int sockfd=-1;

extern void outxid(unsigned int vaddr,msgblk_t * blk,unsigned char *p, int len);
extern void outacars(unsigned int vaddr,msgblk_t * blk,unsigned char *txt, int len);

int initOutput(char *Rawaddr)
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

void initJson(void)
{
	jsonbuf=malloc(JSONBUFLEN);
}

void outjson()
{
        char pkt[500];

	if(jsonbuf==NULL || sockfd<0) return;

        snprintf(pkt, sizeof(pkt), "%s\n", jsonbuf);
        write(sockfd, pkt, strlen(pkt));
}

void dumpdata(unsigned char *p, int len)
{
	int i, k;

	for (i = 0; i < len; i += 16) {
		for (k = 0; k < 16; k++)
			if (i + k < len)
				fprintf(logfd, "%02hhx ", p[i + k]);
			else
				fprintf(logfd, "   ");
		fprintf(logfd, "   |");
		for (k = 0; k < 16; k++)
			if (i + k < len && p[i + k] >= ' ' && p[i + k] <= '~')
				fprintf(logfd, "%c", p[i + k]);
			else
				fprintf(logfd, ".");
		fprintf(logfd, "|\n");
	}
}

static unsigned int vdlm2addr(unsigned char *hdata)
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
			fprintf(logfd, "T%1d:%06X ", type,addr);
			break;
		case 1:
			fprintf(logfd, "Aircraft:%06X ", addr);
			break;
		case 2:
		case 3:
			fprintf(logfd, "T%1d:%06X ", type,addr);
			break;
		case 4:
			fprintf(logfd, "GroundA:%06X ", addr);
			break;
		case 5:
			fprintf(logfd, "GroundD:%06X ", addr);
			break;
		case 6:
			fprintf(logfd, "T%1d:%06X ", type,addr);
			break;
		case 7:
			fprintf(logfd, "All ");
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

	fprintf(logfd, "Frame-");
	if (lc & 1) {
		if (lc & 2) {
			fprintf(logfd, "U: ");
			fprintf(logfd, "%s\n",
				Ufrm[rep][((lc >> 3) & 0x1c) |
					  ((lc >> 2) & 0x3)]);
		} else {
			fprintf(logfd, "S: ");
			fprintf(logfd, "Nr:%01d %s\n", (lc >> 5) & 0x7,
				Sfrm[(lc >> 2) & 0x3]);
		}
	} else {
		fprintf(logfd, "I: ");
		fprintf(logfd, "Ns:%01d Nr:%01d\n", (lc >> 1) & 0x7,
			(lc >> 5) & 0x7);
	}
}

static void printdate(struct timeval tv)
{
        struct tm tmp;

        gmtime_r(&(tv.tv_sec), &tmp);

        fprintf(logfd, "%02d/%02d/%04d %02d:%02d:%02d.%03d",
                tmp.tm_mday, tmp.tm_mon + 1, tmp.tm_year + 1900,
                tmp.tm_hour, tmp.tm_min, tmp.tm_sec,(int)tv.tv_usec/1000);
}


void out(msgblk_t * blk, unsigned char *hdata, int l)
{
	int d;
	int rep = (hdata[5] & 2) >> 1;
	int gnd = hdata[1] & 2;
	unsigned int faddr,taddr;

	faddr=vdlm2addr(&(hdata[5]));
	taddr=vdlm2addr(&(hdata[1]));

	if(verbose) {
        	fprintf(logfd, "\n[#%1d (F:%3.3f P:%.1f) ", blk->chn + 1,
                        blk->Fr / 1000000.0, blk->ppm);
        	printdate(blk->tv);
        	fprintf(logfd, " --------------------------------\n");

		fprintf(logfd, "%s from ", rep ? "Response" : "Command");
		outaddr(faddr);
		fprintf(logfd, "(%s) to ", gnd ? "on ground" : "airborne");
		outaddr(taddr);
		fprintf(logfd, "\n");

		outlinkctrl(hdata[9], rep);
	}

	d = 0;
	if (l == 13) {
		d = 1;
	}

	if (l >= 16 && hdata[10] == 0xff && hdata[11] == 0xff && hdata[12] == 1) {
		outacars(faddr,blk,&(hdata[13]), l - 16);
		d = 1;
	}

	if (l >= 14 && hdata[10] == 0x82) {
		outxid(faddr,blk,&(hdata[11]), l - 14);
		d = 1;
	}

	if (d == 0) {
		if(verbose) 
			fprintf(logfd, "unknown data\n");
		if(verbose>1) 
			dumpdata(&(hdata[10]), l - 13);
	}

	fflush(logfd);

}
