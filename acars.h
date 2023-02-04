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
typedef struct {
	char mode;
	char reg[9];
	char ack;
	char label[3];
	char bid;
	char no[5];
	char fid[7];
	char bs, be;
	char txt[17000];
} acarsmsg_t;

typedef struct {
	char da[5];
	char sa[5];
	char eta[5];
	char gout[5];
	char gin[5];
	char woff[5];
	char won[5];
	// not really oooi 
	float lon,lat;
	int epu;
	int alt;
} oooi_t;

typedef struct flight_s flight_t;
struct flight_s {
        flight_t *next;
        uint32_t addr;
        char reg[9];
        char fid[7];
        struct timeval ts,tl;
        int nbm;
        int rt,gt;
        oooi_t oooi;
};

