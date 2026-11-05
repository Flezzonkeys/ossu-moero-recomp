#include "rt.h"
#include <math.h>
typedef struct{u32 cnt,sad,len;u16 tmr,pnt;u32 run;double pos;u32 lastidx;s32 adpcm,idx,lpa,lpi;u32 noise;s32 cur;}Ch;
static Ch ch[16];static u16 sndcnt,bias=0x200;static u8 cap[16];
static const s16 ima_idx[8]={-1,-1,-1,-1,2,4,6,8};
static const u16 ima_step[89]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static inline u8 rd8(u32 a){if((a>>24)==2)return mainram[a&0x3fffff];return a7_r8(a);}
static inline s16 rd16(u32 a){if((a>>24)==2)return *(s16*)(mainram+(a&0x3ffffe));return (s16)a7_r16(a);}
static void start(int i){Ch*c=&ch[i];c->run=1;c->pos=0;c->lastidx=0xffffffff;c->noise=0x7fff;c->cur=0;int fmt=(c->cnt>>29)&3;if(fmt==2){u32 h=*(u32*)(mainram+(c->sad&0x3ffffc));c->adpcm=(s16)(h&0xffff);c->idx=(h>>16)&0x7f;if(c->idx>88)c->idx=88;c->lpa=c->adpcm;c->lpi=c->idx;}}
static s32 step(int i,double adv){Ch*c=&ch[i];if(!c->run)return 0;int fmt=(c->cnt>>29)&3;u32 rep=(c->cnt>>27)&3;double p=c->pos+adv;c->pos=p;
 if(fmt==3){if(i>=8&&i<=13){int d=(c->cnt>>24)&7;u32 ph=(u32)(p/1)%8;return ph<=(u32)d?-0x7fff:0x7fff;}if(i>=14){u32 k=(u32)p;while(c->lastidx!=k){c->lastidx=c->lastidx==0xffffffff?0:c->lastidx+1;c->cur=(c->noise&1)?-0x7fff:0x7fff;c->noise=(c->noise&1)?(c->noise>>1)^0x6000:c->noise>>1;}return c->cur;}return 0;}
 u32 lstart=c->pnt*4,tot=(c->pnt+c->len)*4;u32 bytes=fmt==0?1:fmt==1?2:0;
 if(fmt<2){u32 idx=(u32)p;u32 off=idx*bytes;if(off>=tot){if(rep==1&&c->len){u32 span=c->len*4/bytes;double over=p-(double)(tot/bytes);over=fmod(over,span);c->pos=(double)(lstart/bytes)+over;idx=(u32)c->pos;off=idx*bytes;}else{c->run=0;c->cnt&=~(1u<<31);return 0;}}
  return fmt==0?((s32)(s8)rd8(c->sad+off))<<8:rd16(c->sad+off);}
 /* ADPCM: 4 bytes header then nibbles */
 u32 nib=(u32)p;u32 total=(tot>4)?(tot-4)*2:0,lnib=lstart>=4?(lstart-4)*2:0;if(nib>=total){if(rep==1&&c->len){u32 span=total-lnib;double over=p-(double)total;over=fmod(over,span);c->pos=lnib+over;nib=(u32)c->pos;c->lastidx=lnib?lnib-1:0xffffffff;c->adpcm=c->lpa;c->idx=c->lpi;if(!lnib){u32 h=*(u32*)(mainram+(c->sad&0x3ffffc));c->adpcm=(s16)h;c->idx=(h>>16)&0x7f;}}else{c->run=0;c->cnt&=~(1u<<31);return 0;}}
 while(c->lastidx==0xffffffff||c->lastidx<nib){u32 k=c->lastidx==0xffffffff?0:c->lastidx+1;c->lastidx=k;if(k==lnib){c->lpa=c->adpcm;c->lpi=c->idx;}u8 b=rd8(c->sad+4+k/2);int n=(k&1)?b>>4:b&15;int st=ima_step[c->idx];int d=st>>3;if(n&1)d+=st>>2;if(n&2)d+=st>>1;if(n&4)d+=st;if(n&8){c->adpcm-=d;if(c->adpcm<-32767)c->adpcm=-32767;}else{c->adpcm+=d;if(c->adpcm>32767)c->adpcm=32767;}c->idx+=ima_idx[n&7];if(c->idx<0)c->idx=0;if(c->idx>88)c->idx=88;}
 return c->adpcm;}
void spu_mix(s16*o,int n){double base=33513982.0/2.0/32768.0;int mv=sndcnt&0x7f,en=(sndcnt>>15)&1;
 for(int s=0;s<n;s++){s64 L=0,R=0;if(en)for(int i=0;i<16;i++){Ch*c=&ch[i];if(!c->run)continue;double adv=base/(0x10000-c->tmr);s32 v=step(i,adv);int vol=c->cnt&0x7f,dv=(c->cnt>>8)&3,pan=(c->cnt>>16)&0x7f;static const int sh[4]={0,1,2,4};s64 x=((s64)v*vol)>>(7+sh[dv]);L+=x*(127-pan)>>7;R+=x*pan>>7;}
  L=(L*mv)>>9;R=(R*mv)>>9;if(L>32767)L=32767;if(L<-32768)L=-32768;if(R>32767)R=32767;if(R<-32768)R=-32768;o[s*2]=L;o[s*2+1]=R;}}
void spu_write(u32 a,u32 v,int size){if(a>=0x400&&a<0x500){int i=(a-0x400)>>4,r=(a-0x400)&15;Ch*c=&ch[i];u8 tmp[16];memcpy(tmp,&c->cnt,4);memcpy(tmp+4,&c->sad,4);memcpy(tmp+8,&c->tmr,2);memcpy(tmp+10,&c->pnt,2);memcpy(tmp+12,&c->len,4);
  for(int k=0;k<size&&r+k<16;k++)tmp[r+k]=v>>(k*8);u32 old=c->cnt;memcpy(&c->cnt,tmp,4);memcpy(&c->sad,tmp+4,4);memcpy(&c->tmr,tmp+8,2);memcpy(&c->pnt,tmp+10,2);memcpy(&c->len,tmp+12,4);c->sad&=0x07fffffc;c->len&=0x3fffff;if((c->cnt>>31)&&!(old>>31))start(i);else if(!(c->cnt>>31))c->run=0;return;}
 if(a==0x500)sndcnt=size==1?(sndcnt&0xff00)|(v&0xff):v;else if(a==0x501)sndcnt=(sndcnt&0xff)|(v<<8);else if(a==0x504)bias=v;else if(a>=0x508&&a<0x518)cap[a-0x508]=v;}
u32 spu_read(u32 a,int size){if(a>=0x400&&a<0x500){int i=(a-0x400)>>4,r=(a-0x400)&15;Ch*c=&ch[i];u32 x=0;switch(r&~3){case 0:x=c->cnt;if(!c->run)x&=~(1u<<31);break;case 4:x=c->sad;break;case 8:x=c->tmr|((u32)c->pnt<<16);break;default:x=c->len;}x>>=(r&3)*8;return size==1?x&0xff:size==2?x&0xffff:x;}
 if(a==0x500)return sndcnt;if(a==0x501)return sndcnt>>8;if(a==0x504)return bias;if(a>=0x508&&a<0x518)return cap[a-0x508];return 0;}
