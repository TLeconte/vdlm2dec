#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vdlm2.h"
#include "acars.h"

static char *lblfilter[1024];

void build_label_filter(char *arg)
{
   int i=0;
   char *aptr;

   lblfilter[0]=NULL;
   if(arg==NULL) return;

   aptr=strtok(strdup(arg),":");
   while(aptr) {
	lblfilter[i]=aptr; i++;
	aptr=strtok(NULL,":");
	}
   lblfilter[i]=NULL;
}

int label_filter(char *lbl)
{
   int i;

    if(lblfilter[0]==NULL) return 1;

   i=0;
   while(lblfilter[i]) {
	if(strcmp(lbl,lblfilter[i])==0) return 1;
	i++;
   }

   return 0;
}

static int convpos(char *txt,oooi_t *oooi)
{
  char tmp[10];

  if(txt[0] != 'N' && txt[0] != 'S') return 0;
  if(txt[6] != 'W' && txt[6] != 'E') return 0;

  memcpy(tmp,&(txt[1]),5);tmp[5]=0;
  oooi->lat=atof(tmp)/1000.0;
  if(txt[0] == 'S') oooi->lat=-oooi->lat;
  
  memcpy(tmp,&(txt[7]),6);tmp[6]=0;
  oooi->lon=atof(tmp)/1000.0;
  if(txt[6] == 'W') oooi->lon=-oooi->lon;
  oooi->epu=1;
  return 1;
}

static int label_q1(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<28) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->gout,&(txt[4]),4);
    memcpy(oooi->woff,&(txt[8]),4);
    memcpy(oooi->won,&(txt[12]),4);
    memcpy(oooi->gin,&(txt[16]),4);
    memcpy(oooi->da,&(txt[24]),4);
    return 1;
}
static int label_q2(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<8) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->eta,&(txt[4]),4);
    return 1;
}
static int label_qa(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<8) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->gout,&(txt[4]),4);
    return 1;
}
static int label_qb(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<8) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->woff,&(txt[4]),4);
    return 1;
}
static int label_qc(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<8) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->won,&(txt[4]),4);
    return 1;
}
static int label_qd(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<8) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->gin,&(txt[4]),4);
    return 1;
}
static int label_qe(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<12) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->gout,&(txt[4]),4);
    memcpy(oooi->da,&(txt[8]),4);
    return 1;
}
static int label_qf(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<12) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->woff,&(txt[4]),4);
    memcpy(oooi->da,&(txt[8]),4);
    return 1;
}
static int label_qg(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<12) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->gout,&(txt[4]),4);
    memcpy(oooi->gin,&(txt[8]),4);
    return 1;
}
static int label_qh(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<8) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->gout,&(txt[4]),4);
    return 1;
}
static int label_qk(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<12) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->won,&(txt[4]),4);
    memcpy(oooi->da,&(txt[8]),4);
    return 1;
}
static int label_ql(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<17) return 0;
    memcpy(oooi->da,txt,4);
    memcpy(oooi->gin,&(txt[8]),4);
    memcpy(oooi->sa,&(txt[13]),4);
    return 1;
}
static int label_qm(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<12) return 0;
    memcpy(oooi->da,txt,4);
    memcpy(oooi->sa,&(txt[8]),4);
    return 1;
}
static int label_qn(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<12) return 0;
    memcpy(oooi->da,&(txt[4]),4);
    memcpy(oooi->eta,&(txt[8]),4);
    return 1;
}
static int label_qp(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<12) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->da,&(txt[4]),4);
    memcpy(oooi->gout,&(txt[8]),4);
    return 1;
}
static int label_qq(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<12) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->da,&(txt[4]),4);
    memcpy(oooi->woff,&(txt[8]),4);
    return 1;
}
static int label_qr(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<12) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->da,&(txt[4]),4);
    memcpy(oooi->won,&(txt[8]),4);
    return 1;
}
static int label_qs(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<12) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->da,&(txt[4]),4);
    memcpy(oooi->gin,&(txt[8]),4);
    return 1;
}
static int label_qt(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<16) return 0;
    memcpy(oooi->sa,txt,4);
    memcpy(oooi->da,&(txt[4]),4);
    memcpy(oooi->gout,&(txt[8]),4);
    memcpy(oooi->gin,&(txt[12]),4);
    return 1;
}

static int label_20(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<30) return 0;
    if(memcmp(txt,"RST",3)) return 0;
    memcpy(oooi->sa,&(txt[22]),4);
    memcpy(oooi->da,&(txt[26]),4);
    return 1;
}
static int label_2Z(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<4) return 0;
    memcpy(oooi->da,txt,4);
    return 1;
}
static int label_44(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<48) return 0;
    if(memcmp(txt,"POS0",4) || txt[5]!=',' ) return 0;
    if(convpos(&(txt[6]),oooi) == 0 ) return 0;

    if(txt[23]!=',') return 0;
    memcpy(oooi->da,&(txt[24]),4);
    if(txt[28]!=',') return 0;
    memcpy(oooi->sa,&(txt[29]),4);
    if(txt[43]!=',') return 0;
    memcpy(oooi->eta,&(txt[44]),4);		
    return 1;
}
static int label_15(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<26) return 0;
    if(memcmp(txt,"FST01",5)) return 0;
    memcpy(oooi->sa,&(txt[5]),4);
    memcpy(oooi->da,&(txt[9]),4);
    return convpos(&(txt[13]),oooi);
}
static int label_16(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<19) return 0;
    if(memcmp(txt,"POSA1",5)) return 0;
    return convpos(&(txt[6]),oooi);
}
static int label_H1(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<20) return 0;
    if(memcmp(txt,"#M1BPOS",7) && memcmp(txt,"#M2BPOS",7) && memcmp(txt,"#M3BPOS",7)) return 0;
    return convpos(&(txt[7]),oooi);
}
static int label_17(char *txt,oooi_t *oooi)
{
    if(strlen(txt)<18) return 0;
    if(memcmp(txt,"ETA ",4)) return 0;
    memcpy(oooi->eta,&(txt[4]),4);
    if(txt[8]!=',') return 0;
    memcpy(oooi->sa,&(txt[9]),4);
    if(txt[13]!=',') return 0;
    memcpy(oooi->da,&(txt[14]),4);
    return 1;
}


int DecodeLabel(acarsmsg_t *msg,oooi_t *oooi)
{
  int ov=0;
 
  memset(oooi,0,sizeof(oooi_t));

  switch(msg->label[0]) {
	case '1' :
		if(msg->label[1]=='5') 
			ov=label_15(msg->txt,oooi);
		if(msg->label[1]=='6') 
			ov=label_16(msg->txt,oooi);
		if(msg->label[1]=='6') 
			ov=label_17(msg->txt,oooi);
		break;
	case '2' :
		if(msg->label[1]=='0') 
			ov=label_20(msg->txt,oooi);
		if(msg->label[1]=='Z') 
			ov=label_2Z(msg->txt,oooi);
		break;
	case '4' :
		if(msg->label[1]=='4') 
			ov=label_44(msg->txt,oooi);
		break;
	case 'H' :
		if(msg->label[1]=='1') 
			ov=label_H1(msg->txt,oooi);
		break;
	case 'Q' :
  		switch(msg->label[1]) {
			case '1':ov=label_q1(msg->txt,oooi);break;
			case '2':ov=label_q2(msg->txt,oooi);break;
			case 'A':ov=label_qa(msg->txt,oooi);break;
			case 'B':ov=label_qb(msg->txt,oooi);break;
			case 'C':ov=label_qc(msg->txt,oooi);break;
			case 'D':ov=label_qd(msg->txt,oooi);break;
			case 'E':ov=label_qe(msg->txt,oooi);break;
			case 'F':ov=label_qf(msg->txt,oooi);break;
			case 'G':ov=label_qg(msg->txt,oooi);break;
			case 'H':ov=label_qh(msg->txt,oooi);break;
			case 'K':ov=label_qk(msg->txt,oooi);break;
			case 'L':ov=label_ql(msg->txt,oooi);break;
			case 'M':ov=label_qm(msg->txt,oooi);break;
			case 'N':ov=label_qn(msg->txt,oooi);break;
			case 'P':ov=label_qp(msg->txt,oooi);break;
			case 'Q':ov=label_qq(msg->txt,oooi);break;
			case 'R':ov=label_qr(msg->txt,oooi);break;
			case 'S':ov=label_qs(msg->txt,oooi);break;
			case 'T':ov=label_qt(msg->txt,oooi);break;
		}
		break;
  }

  return ov;
}

