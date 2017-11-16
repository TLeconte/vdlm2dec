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
#include "vdlm2.h"
#include "acars.h"

void dumpdata(unsigned char *p, int len)
{
    int i,k;

    for (i = 0; i < len; i+=16) {
         for (k = 0; k<16 ; k++) 
           if(i+k<len)
        	fprintf(logfd, "%02hhx ", p[i+k]);
	   else
        	fprintf(logfd, "   ");
       	fprintf(logfd, "   |");
         for (k = 0; k<16; k++) 
           if(i+k<len && p[i+k]>=' ' && p[i+k]<='~')
        	fprintf(logfd, "%c", p[i+k]);
	   else
        	fprintf(logfd, ".");
       	fprintf(logfd, "|\n");
    }
}

static void outaddr(unsigned char *hdata)
{
 unsigned int addr =
         (reversebits(hdata[0] >> 2, 6) << 21) | (reversebits(hdata[2] >> 1, 7) << 14) |
         (reversebits(hdata[2] >> 1, 7) << 7) | (reversebits(hdata[3] >> 1, 7));

 unsigned int type = addr >>24; 
 
 addr=addr&0xffffff; 

 if(type==7) 
  fprintf(logfd, "All    ",addr);
 else 
  fprintf(logfd, "%06x ",addr);

}

static const char *Sfrm [4]={"RR", "RNR","REJ","SREJ"};
static const char *Ufrm [2][32]={
{"UI", "SIM","0x02","SARM","UP","0x05","0x06","SABM",
"DISC","0x09","0x0a","SARME","0x0c","0x0d","0x0e","SABME",
"SNRM","0x11","0x12","RSET","0x14","0x15","0x16","XID",
"0x18","0x19","0x1a","SNRME","TEST","0x1d","0x1e","0x1f"
},
{"UI", "RIM","0x02","DM","0x04","0x05","0x06","0x07",
"RD","0x09","0x0a","0x0b","UA","0x0d","0x0e","0x0f",
"0x10","FRMR","0x12","0x13","0x14","0x15","0x16","XID",
"0x18","0x19","0x1a","0x1b","TEST","0x1d","0x1e","0x1f"
}
};
static void outlinkctrl(unsigned char lc,int rep)
{

    fprintf(logfd, "Frame-");
    if(lc&1) {
	if(lc&2) {
                   fprintf(logfd, "U: ");
                   fprintf(logfd, "%s ", (lc & 0x10) ? "P/F" : "-/-");
                   fprintf(logfd, "%s\n", Ufrm[rep][((lc >> 3) & 0x1c) | ((lc >> 2) & 0x3)]);
	} else {
                   fprintf(logfd, "S: ");
                   fprintf(logfd, "%s ", (lc & 0x10) ? "P/F" : "-/-");
                   fprintf(logfd, "Nr:%01d %s\n", (lc >> 5) & 0x7, Sfrm[(lc >> 2) & 0x3]);
	}
   } else {
                fprintf(logfd, "I: ");
                fprintf(logfd, "%s ", (lc & 0x10) ? "P/F" : "-/-");
                fprintf(logfd, "Ns:%01d Nr:%01d\n", (lc >> 1) & 0x7, (lc >> 5) & 0x7);
   }
}

void out(msgblk_t *blk,unsigned char *hdata,int l)
{
	int i,d;
        int rep = (hdata[5] & 2)>>1;
        int gnd = hdata[1] & 2;

            fprintf(logfd, "-----------------------------------------------------------------------\n");
            fprintf(logfd, "%s ", rep ? "Response":"Command");
            fprintf(logfd, "from %s: ",gnd ? "Ground":"Aircraft"); outaddr(&(hdata[5]));
            fprintf(logfd, "to: ");outaddr(&(hdata[1]));
            fprintf(logfd, "\n");

	    outlinkctrl(hdata[9],rep);

            d = 0;

            if (l == 13) {
                d = 1;
            }

            if (l >= 16 && hdata[10] == 0xff && hdata[11] == 0xff && hdata[12] == 1) {
                outacars(&(hdata[13]), l - 16);
                d = 1;
            }
            if (l >= 14 && hdata[10] == 0x82) {
                outxid(&(hdata[11]), l - 14);
                d = 1;
            }

            if (d == 0) {
                fprintf(logfd, "unknown data\n");
                if (verbose > 1)
                    dumpdata(&(hdata[10]), l - 13);
            }
            fflush(logfd);

}
