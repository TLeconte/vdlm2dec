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
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <netdb.h>
#ifdef HAVE_LIBACARS
#include <libacars/libacars.h>
#include <libacars/acars.h>
#include <libacars/vstring.h>
#else
typedef void la_proto_node;
#endif

#include "vdlm2.h"
#include "crc.h"
#include "acars.h"
#include "cJSON.h"

extern int verbose;
extern cJSON *json_obj;

extern int label_filter(char *lbl);
extern int DecodeLabel(acarsmsg_t *msg,oooi_t *oooi);

const char *regpre1[] =
    { "C", "B", "F", "D", "2", "I", "P", "M", "G", "Z", "" };
const char *regpre2[] = {
        "YA", "ZA", "7T", "C3", "D2", "VP", "V2", "LV", "LQ", "EK", "P4", "VH",
            "OE", "4K", "C6",
        "S2", "8P", "EW", "OO", "V3", "TY", "VQ", "A5", "CP", "T9", "E7", "A2",
            "PP", "PR", "PT",
        "PU", "V8", "LZ", "XT", "9U", "XU", "TJ", "D4", "TL", "TT", "CC", "HJ",
            "HK", "D6", "TN",
        "E5", "9Q", "TI", "TU", "9A", "CU", "5B", "OK", "OY", "J2", "J7", "HI",
            "4W", "HC", "SU",
        "YS", "3C", "E3", "ES", "ET", "DQ", "OH", "TR", "C5", "4L", "9G", "SX",
            "J3", "TG", "3X",
        "J5", "8R", "HH", "HR", "HA", "TF", "VT", "PK", "EP", "YI", "EI", "EJ",
            "4X", "6Y", "ZJ", "JY", "Z6",
        "UP", "5Y", "T3", "9K", "EX", "YL", "OD", "7P", "A8", "5A", "HB",
            "LY", "LX", "Z3",
        "5R", "7Q", "9M", "8Q", "TZ", "9H", "V7", "5T", "3B", "XA", "XB", "XC",
            "V6", "ER", "3A",
        "JU", "4O", "CN", "C9", "XY", "XZ", "V5", "C2", "9N", "PH", "PJ", "ZK",
            "ZL", "ZM", "YN", "5U", "LN",
        "AP", "SU", "E4", "HP", "P2", "ZP", "OB", "RP", "SP", "SN", "CR",
            "CS", "A7", "YR",
        "RA", "RF", "V4", "J6", "J8", "5W", "T7", "S9", "HZ", "6V", "6W", "YU", "S7",
            "9L", "9V", "OM",
        "S5", "H4", "6O", "ZS", "ZT", "ZU", "Z8", "EC", "4R", "ST", "PZ", "SE",
            "HB", "YK", "EY",
        "5H", "HS", "5V", "A3", "9Y", "TS", "TC", "EZ", "T2", "5X", "UR", "A6",
            "4U", "CX",
        "YJ", "VN", "7O", "9J", ""
};
const char *regpre3[] = { "A9C", "A4O", "9XR", "3DC", "" };

static void fixreg(char *reg, char *add)
{
	char src[8];
        char *p, *t;
        int i;

	memcpy(src,add,7);src[7]=0;

        for (p = src; *p == '.'; p++) ;

        if (strlen(p) >= 4) {
                t = NULL;
                for (i = 0; regpre3[i][0] != 0; i++)
                        if (memcmp(p, regpre3[i], 3) == 0) {
                                t = p + 3;
                                break;
                        }
                if (t == NULL) {
                        for (i = 0; regpre2[i][0] != 0; i++)
                                if (memcmp(p, regpre2[i], 2) == 0) {
                                        t = p + 2;
                                        break;
                                }
                }
                if (t == NULL) {
                        for (i = 0; regpre1[i][0] != 0; i++)
                                if (*p == regpre1[i][0]) {
                                        t = p + 1;
                                        break;
                                }
                }
                if (t && *t != '-') {
                        memcpy(reg, p, t - p);
                        reg[t - p] = 0;
                        strncat(reg, "-", 8);
                        strncat(reg, t, 8);
                        reg[8] = 0;
                        return;
                }
        }

        strncpy(reg, p, 8);
        reg[8] = 0;

}

static void printmsg(const acarsmsg_t * msg, oooi_t *oooi, la_proto_node *lanode)
{
	vout( "ACARS\n");

	if (msg->mode < 0x5d) {
		vout( "Aircraft reg: %s ", msg->reg);
		vout( "Flight id: %s", msg->fid);
		vout( "\n");
	}
	vout( "Mode: %1c ", msg->mode);
	vout( "Msg. label: %s\n", msg->label);
	vout( "Block id: %c ", msg->bid);
	vout( "Ack: %c\n", msg->ack);
	vout( "Msg. no: %s\n", msg->no);
	if(msg->txt[0]) vout( "Message :\n%s\n", msg->txt);
	if (msg->be == 0x17)
		vout( "Block End\n");

#ifdef HAVE_LIBACARS
        if(lanode != NULL) {
                la_vstring *vstr = la_proto_tree_format_text(NULL, lanode);
                vout( "%s\n", vstr->str);
                la_vstring_destroy(vstr, true);
        }
#endif

	fflush(logfd);
}

static void addacarsjson(acarsmsg_t * msg,oooi_t *oooi, la_proto_node *lanode)
{

	char convert_tmp[8];

	snprintf(convert_tmp, sizeof(convert_tmp), "%c", msg->mode);
	cJSON_AddStringToObject(json_obj, "mode", convert_tmp);

	cJSON_AddStringToObject(json_obj, "label", msg->label);

	if(msg->bid) {
		snprintf(convert_tmp, sizeof(convert_tmp), "%c", msg->bid);
		cJSON_AddStringToObject(json_obj, "block_id", convert_tmp);

		if(msg->ack == 0x15) {
			cJSON_AddFalseToObject(json_obj, "ack");
		} else {
			snprintf(convert_tmp, sizeof(convert_tmp), "%c", msg->ack);
			cJSON_AddStringToObject(json_obj, "ack", convert_tmp);
		}

		cJSON_AddStringToObject(json_obj, "tail", msg->reg);
		if(msg->mode <= 'Z') {
			cJSON_AddStringToObject(json_obj, "flight", msg->fid);
			cJSON_AddStringToObject(json_obj, "msgno", msg->no);
		}
	}
	if(msg->txt[0])
		cJSON_AddStringToObject(json_obj, "text", msg->txt);

	if (msg->be == 0x17)
		cJSON_AddTrueToObject(json_obj, "end");

	if(oooi) {
		if(oooi->sa[0])
			cJSON_AddStringToObject(json_obj, "depa", oooi->sa);
		if(oooi->da[0])
			cJSON_AddStringToObject(json_obj, "dsta", oooi->da);
		if(oooi->eta[0])
			cJSON_AddStringToObject(json_obj, "eta", oooi->eta);
		if(oooi->gout[0])
			cJSON_AddStringToObject(json_obj, "gtout", oooi->gout);
		if(oooi->gin[0])
			cJSON_AddStringToObject(json_obj, "gtin", oooi->gin);
		if(oooi->woff[0])
			cJSON_AddStringToObject(json_obj, "wloff", oooi->woff);
		if(oooi->won[0])
			cJSON_AddStringToObject(json_obj, "wlin", oooi->won);
		if(oooi->epu) {
			convert_tmp[16];
		        snprintf(convert_tmp, sizeof(convert_tmp), "%3.3f", oooi->lat);
       		        cJSON_AddRawToObject(json_obj, "lat", convert_tmp);
		        snprintf(convert_tmp, sizeof(convert_tmp), "%4.3f", oooi->lon);
       		        cJSON_AddRawToObject(json_obj, "lon", convert_tmp);

                	cJSON_AddNumberToObject(json_obj, "epu", oooi->epu);
		}
		if(oooi->alt)
		        cJSON_AddNumberToObject(json_obj, "alt", oooi->alt);
	}
}

int outacars(flight_t *fl,unsigned char *txt, int len)
{
	acarsmsg_t msg;
	oooi_t oooi;
	int i, k, j;
	unsigned int crc;
	la_proto_node *lanode;

	crc = 0;
	/* test crc, set le and remove parity */
	for (i = 0; i < len - 1; i++) {
		update_crc(crc, txt[i]);
		txt[i] &= 0x7f;
	}
	if (crc) {
		if(verbose>1) vout( "crc error\n");
		return 0;
	}

	/* fill msg struct */

	k = 0;
	msg.mode = txt[k];
	k++;

	fixreg(msg.reg,&(txt[k]));
	k+=7;

	/* ACK/NAK */
	msg.ack = txt[k];
	if (msg.ack == 0x15)
		msg.ack = '!';
	k++;

	msg.label[0] = txt[k];
	k++;
	msg.label[1] = txt[k];
	if (msg.label[1] == 0x7f)
		msg.label[1] = 'd';
	k++;
	msg.label[2] = '\0';

	msg.bid = txt[k];
	if (msg.bid == 0)
		msg.bid = ' ';
	k++;

	/* txt start  */
	msg.bs = txt[k];
	k++;

	msg.no[0] = '\0';
	msg.fid[0] = '\0';
	msg.txt[0] = '\0';

	if (msg.bs != 0x03) {
		if (msg.mode <= 'Z' && msg.bid <= '9') {
			/* message no */
			for (i = 0; i < 4 && k < len - 4; i++, k++) {
				msg.no[i] = txt[k];
			}
			msg.no[i] = '\0';

			/* Flight id */
			for (i = 0; i < 6 && k < len - 4; i++, k++) {
				msg.fid[i] = txt[k];
			}
			msg.fid[i] = '\0';
		}

		/* Message txt */
		for (i = 0; (k < len - 4); i++, k++)
			msg.txt[i] = txt[k];
		msg.txt[i] = 0;
	}
	msg.be = txt[k];

	if(label_filter(msg.label)==0) return 0;

#ifdef HAVE_LIBACARS
	if (txt[0]) {
        	la_msg_dir direction;
        	if(msg.bid >= '0' && msg.bid <= '9')
                	direction = LA_MSG_DIR_AIR2GND;
        	else
                	direction = LA_MSG_DIR_GND2AIR;

        	lanode = la_acars_decode_apps(msg.label, msg.txt, direction);
	}
#endif

	DecodeLabel(&msg, &oooi);

	if (verbose) {
		printmsg(&msg, &oooi,lanode);
	}

	if(fl) {
		strncpy(fl->fid,msg.fid,7);fl->fid[6]=0;
		strncpy(fl->reg,msg.reg,8);fl->reg[8]=0;
		if(oooi.da[0]) memcpy(fl->oooi.da,oooi.da,5);
                if(oooi.sa[0]) memcpy(fl->oooi.sa,oooi.sa,5);
                if(oooi.eta[0]) memcpy(fl->oooi.eta,oooi.eta,5);
                if(oooi.gout[0]) memcpy(fl->oooi.gout,oooi.gout,5);
                if(oooi.gin[0]) memcpy(fl->oooi.gin,oooi.gin,5);
                if(oooi.woff[0]) memcpy(fl->oooi.woff,oooi.woff,5);
                if(oooi.won[0]) memcpy(fl->oooi.won,oooi.won,5);
	}


	if(json_obj)
		addacarsjson(&msg,&oooi,lanode);

#ifdef HAVE_LIBACARS
	if(lanode) 
        	la_proto_tree_destroy(lanode);
#endif
	return 1;

}
