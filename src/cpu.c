#include "rt.h"
CPU cpu9,cpu7;
extern const Block a9_blocks[],a7_blocks[];extern const u32 a9_nblocks,a7_nblocks;
#define HSZ (1u<<20)
static const Block* h9[HSZ];static const Block* h7[HSZ];
static inline u32 hsh(u32 k){return (k*0x9E3779B1u)>>12;}
static void ins(const Block**h,const Block*b){u32 i=hsh(b->key);while(h[i])i=(i+1)&(HSZ-1);h[i]=b;}
void cpu_init_tables(void){for(u32 i=0;i<a9_nblocks;i++)ins(h9,&a9_blocks[i]);for(u32 i=0;i<a7_nblocks;i++)ins(h7,&a7_blocks[i]);}
static inline void(*look(CPU*c,u32 k))(CPU*){const Block**h=c->id==9?h9:h7;u32 i=hsh(k);while(h[i]){if(h[i]->key==k)return h[i]->fn;i=(i+1)&(HSZ-1);}return 0;}
static int bidx(u32 m){switch(m&31){case 0x11:return 1;case 0x12:return 2;case 0x13:return 3;case 0x17:return 4;case 0x1b:return 5;default:return 0;}}
u32 cpsr_get(CPU*c){return c->n<<31|(c->z?1u:0)<<30|(c->cf?1u:0)<<29|(c->v?1u:0)<<28|c->q<<27|c->I<<7|c->F<<6|c->t<<5|c->mode;}
void cpsr_set(CPU*c,u32 v){int o=bidx(c->mode),n=bidx(v);if(o!=n){c->bank13[o]=c->r[13];c->bank14[o]=c->r[14];c->r[13]=c->bank13[n];c->r[14]=c->bank14[n];}
 c->n=v>>31;c->z=(v>>30)&1;c->cf=(v>>29)&1;c->v=(v>>28)&1;c->q=(v>>27)&1;c->I=(v>>7)&1;c->F=(v>>6)&1;c->t=(v>>5)&1;c->mode=v&31;}
u32 spsr_get(CPU*c){return c->spsr[bidx(c->mode)];}
static u32 fmask(u32 f){u32 m=0;if(f&1)m|=0xff;if(f&2)m|=0xff00;if(f&4)m|=0xff0000;if(f&8)m|=0xff000000u;return m;}
void msr_cpsr(CPU*c,u32 v,u32 f){u32 m=fmask(f);if(c->mode==0x10)m&=0xff000000u;cpsr_set(c,(cpsr_get(c)&~m)|(v&m));}
#include <math.h>
const s16 bios_sin(int i){return (s16)lround(sin(i*3.14159265358979/128.0)*0x7fff);}
const u32 bios_pitch(int i){return (u32)floor(65536.0*(pow(2.0,i/768.0)-1.0)+0.5)&0xffff;}
const u32 bios_vol(int i){if(i>723)i=723;if(i<0)i=0;int db=i-723;int sh=db<-240?4:db<-120?2:db<-60?1:0;double a=127.0*pow(10.0,db/200.0)*(1<<sh);int v=(int)floor(a+0.5);return v>127?127:v;}
void dbg_ctx(CPU*,u32);void msr_spsr(CPU*c,u32 v,u32 f){u32 m=fmask(f);int b=bidx(c->mode);c->spsr[b]=(c->spsr[b]&~m)|(v&m);}
void spsr_restore(CPU*c){cpsr_set(c,c->spsr[bidx(c->mode)]);}
u32 usr_get(CPU*c,int n){if(bidx(c->mode)==0)return c->r[n];return n==13?c->bank13[0]:c->bank14[0];}
void usr_set(CPU*c,int n,u32 v){if(bidx(c->mode)==0){c->r[n]=v;return;}if(n==13)c->bank13[0]=v;else c->bank14[0]=v;}
u32 sh0(CPU*c,u32 v,u32 s){if(!s){c->sc=c->cf;return v;}if(s<32){c->sc=(v>>(32-s))&1;return v<<s;}c->sc=s==32?v&1:0;return 0;}
u32 sh1(CPU*c,u32 v,u32 s){if(!s){c->sc=c->cf;return v;}if(s<32){c->sc=(v>>(s-1))&1;return v>>s;}c->sc=s==32?v>>31:0;return 0;}
u32 sh2(CPU*c,u32 v,u32 s){if(!s){c->sc=c->cf;return v;}if(s<32){c->sc=(v>>(s-1))&1;return (u32)((s32)v>>s);}c->sc=v>>31;return (u32)((s32)v>>31);}
u32 sh3(CPU*c,u32 v,u32 s){if(!s){c->sc=c->cf;return v;}s&=31;if(!s){c->sc=v>>31;return v;}c->sc=(v>>(s-1))&1;return (v>>s)|(v<<(32-s));}
void irq_raise(CPU*c,int b){c->if_|=1u<<b;}
#define BIOS_RET 0xFFFF0100u
static u32 R32c(CPU*c,u32 a){return c->id==9?(a-dtcm_base<0x4000?*(u32*)(dtcm+(a&0x3fff)):(a>>24)==2?*(u32*)(mainram+(a&0x3fffff)):a9_r32(a)):((a>>24)==2?*(u32*)(mainram+(a&0x3fffff)):a7_r32(a));}
static void W32c(CPU*c,u32 a,u32 v){if(c->id==9){if(a-dtcm_base<0x4000)*(u32*)(dtcm+(a&0x3fff))=v;else if((a>>24)==2)*(u32*)(mainram+(a&0x3fffff))=v;else a9_w32(a,v);}else{if((a>>24)==2)*(u32*)(mainram+(a&0x3fffff))=v;else a7_w32(a,v);}}
static void take_irq(CPU*c){u32 old=cpsr_get(c);u32 ret=c->pc;cpsr_set(c,(old&~0x3fu)|0x92);c->spsr[2]=old;c->r[14]=ret+4;
 u32 sp=c->r[13]-24;c->r[13]=sp;W32c(c,sp,c->r[0]);W32c(c,sp+4,c->r[1]);W32c(c,sp+8,c->r[2]);W32c(c,sp+12,c->r[3]);W32c(c,sp+16,c->r[12]);W32c(c,sp+20,c->r[14]);
 c->r[0]=0x04000000;c->r[14]=BIOS_RET;u32 h=c->id==9?R32c(c,dtcm_base+0x3ffc):R32c(c,0x0380fffc);c->t=h&1;c->pc=h&~1u;c->I=1;}
static void bios_ret(CPU*c){u32 sp=c->r[13];c->r[0]=R32c(c,sp);c->r[1]=R32c(c,sp+4);c->r[2]=R32c(c,sp+8);c->r[3]=R32c(c,sp+12);c->r[12]=R32c(c,sp+16);c->r[14]=R32c(c,sp+20);c->r[13]=sp+24;
 u32 ret=c->r[14]-4;spsr_restore(c);c->pc=ret;}
extern const char* discovered_path(int id);
static void fatal_note(const char*k,CPU*c){FILE*f=fopen("ossu_fehler.txt","a");if(f){fprintf(f,"%s cpu%d pc=%08x t=%d lr=%08x frame=%llu\n",k,c->id,c->pc,c->t,c->r[14],(unsigned long long)frame_no);fclose(f);}}
u64 trace_frame;static u32 ring[2][64];static int rp[2];
static void dump(CPU*c){int k=c->id==9;fprintf(stderr,"regs:");for(int i=0;i<16;i++)fprintf(stderr," r%d=%08x",i,c->r[i]);fprintf(stderr," cpsr=%08x\nlast blocks:",cpsr_get(c));for(int i=0;i<24;i++)fprintf(stderr," %08x",ring[k][(rp[k]-24+i)&63]);fprintf(stderr,"\n");}
void cpu_run(CPU*c,s32 budget){c->cyc+=budget;if(c->cyc>budget)c->cyc=budget;
 while(c->cyc>0){
  if(c->id==9&&(io9[0x603]>>6))c->if_|=1u<<21;
  if(c->ie&c->if_)c->halt=0;
  if(c->halt){c->cyc=0;break;}
  if(c->ime&&!c->I&&(c->ie&c->if_))take_irq(c);
  if(c->pc==BIOS_RET){bios_ret(c);continue;}
  void(*fn)(CPU*)=look(c,c->pc|c->t);
  if(!fn){c->pc&=~1u;fn=look(c,c->pc|c->t);}
  if(!fn){FILE*f=fopen(discovered_path(c->id),"a");if(f){fprintf(f,"%x %d\n",c->pc,c->t);fclose(f);}
    fatal_note("MISSING",c);fprintf(stderr,"MISSING cpu%d pc=%08x t=%d lr=%08x frame=%llu\n",c->id,c->pc,c->t,c->r[14],(unsigned long long)frame_no);dump(c);exit(42);}
  {int k_=c->id==9;ring[k_][rp[k_]++&63]=c->pc|c->t;if(trace_frame&&frame_no==trace_frame&&k_)fprintf(stderr,"T %08x\n",c->pc|c->t);}
  fn(c);c->ninstr++;
 }}
void trap(CPU*c,u32 a,u32 i){c->pc=a;fatal_note("TRAP",c);fprintf(stderr,"TRAP cpu%d pc=%08x op=%08x lr=%08x t=%d frame=%llu\n",c->id,a,i,c->r[14],c->t,(unsigned long long)frame_no);dump(c);exit(43);}
u32 cp15_read(CPU*c,u32 crn,u32 crm,u32 op2){if(crn==0)return op2==1?0x0F0D2112:0x41059461;if(crn==1)return 0x00052078;return 0;}
void cp15_write(CPU*c,u32 crn,u32 crm,u32 op2,u32 v){if(crn==9&&crm==1&&op2==0){dtcm_base=v&0xFFFFF000u;}if(crn==7&&crm==0&&op2==4)c->halt=1;}
/* BIOS */
static u16 crc16(u32 crc,u32 a,u32 n,CPU*c){static const u16 t[8]={0xC0C1,0xC181,0xC301,0xC601,0xCC01,0xD801,0xF001,0xA001};
 for(u32 i=0;i<n;i+=2){u32 w=c->id==9?a9_r16(a+i):a7_r16(a+i);for(int k=0;k<4;k++)for(int j=0;j<4;j++){(void)j;}
  for(int b=0;b<16;b++){int bit=(w>>b)&1;int m=(crc^bit)&1;crc>>=1;if(m)crc^=0xA001;}}(void)t;return crc;}
static u32 rd8(CPU*c,u32 a){return c->id==9?(a-dtcm_base<0x4000?dtcm[a&0x3fff]:(a>>24)==2?mainram[a&0x3fffff]:a9_r8(a)):((a>>24)==2?mainram[a&0x3fffff]:a7_r8(a));}
static u32 rd16(CPU*c,u32 a){return c->id==9?(a-dtcm_base<0x4000?*(u16*)(dtcm+(a&0x3fff)):(a>>24)==2?*(u16*)(mainram+(a&0x3fffff)):a9_r16(a)):((a>>24)==2?*(u16*)(mainram+(a&0x3fffff)):a7_r16(a));}
static void wr8(CPU*c,u32 a,u32 v){if(c->id==9){if(a-dtcm_base<0x4000)dtcm[a&0x3fff]=v;else if((a>>24)==2)mainram[a&0x3fffff]=v;else a9_w8(a,v);}else{if((a>>24)==2)mainram[a&0x3fffff]=v;else a7_w8(a,v);}}
static void wr16(CPU*c,u32 a,u32 v){if(c->id==9){if(a-dtcm_base<0x4000)*(u16*)(dtcm+(a&0x3fff))=v;else if((a>>24)==2)*(u16*)(mainram+(a&0x3fffff))=v;else a9_w16(a,v);}else{if((a>>24)==2)*(u16*)(mainram+(a&0x3fffff))=v;else a7_w16(a,v);}}
static void lz77(CPU*c,int vram_mode){u32 s=c->r[0],d=c->r[1];u32 hdr=R32c(c,s);s+=4;u32 n=hdr>>8,o=0;u32 buf=0;
#define OUT(b) do{ if(vram_mode){ if(o&1){buf|=(b)<<8;wr16(c,d+o-1,buf);}else buf=(b);}else wr8(c,d+o,(b)); o++; }while(0)
 u8*tmp=malloc(n+1);while(o<n){u32 f=rd8(c,s++);for(int k=0;k<8&&o<n;k++){if(f&(0x80>>k)){u32 a=rd8(c,s++),b=rd8(c,s++);u32 l=(a>>4)+3,di=((a&15)<<8|b)+1;for(u32 i=0;i<l&&o<n;i++){u8 v=tmp[o-di];tmp[o]=v;OUT(v);}}else{u8 v=rd8(c,s++);tmp[o]=v;OUT(v);}}}free(tmp);}
static void rle(CPU*c,int vram_mode){u32 s=c->r[0],d=c->r[1];u32 hdr=R32c(c,s);s+=4;u32 n=hdr>>8,o=0,buf=0;
 while(o<n){u32 f=rd8(c,s++);u32 l=f&0x7f;if(f&0x80){l+=3;u32 v=rd8(c,s++);for(u32 i=0;i<l&&o<n;i++)OUT(v);}else{l+=1;for(u32 i=0;i<l&&o<n;i++){u32 v=rd8(c,s++);OUT(v);}}}}
static void cpuset(CPU*c){u32 s=c->r[0],d=c->r[1],cn=c->r[2];u32 n=cn&0x1fffff;int fill=(cn>>24)&1,w32=(cn>>26)&1;
 if(w32){s&=~3u;d&=~3u;u32 v=fill?R32c(c,s):0;for(u32 i=0;i<n;i++){W32c(c,d,fill?v:R32c(c,s));d+=4;if(!fill)s+=4;}}
 else{s&=~1u;d&=~1u;u32 v=fill?rd16(c,s):0;for(u32 i=0;i<n;i++){wr16(c,d,fill?v:rd16(c,s));d+=2;if(!fill)s+=2;}}}
static void fastset(CPU*c){u32 s=c->r[0]&~3u,d=c->r[1]&~3u,cn=c->r[2];u32 n=cn&0x1fffff;int fill=(cn>>24)&1;n=(n+7)&~7u;u32 v=fill?R32c(c,s):0;for(u32 i=0;i<n;i++){W32c(c,d,fill?v:R32c(c,s));d+=4;if(!fill)s+=4;}}
static void intrwait(CPU*c,u32 discard,u32 flags){u32 fa=c->id==9?dtcm_base+0x3ff8:0x0380fff8;
 if(!c->waiting&&discard){u32 v=R32c(c,fa);W32c(c,fa,v&~flags);}
 u32 v=R32c(c,fa);if(v&flags){W32c(c,fa,v&~flags);c->waiting=0;return;}
 c->waiting=1;c->waitflags=flags;c->ime=1;c->halt=1;c->pc-=c->t?2:4;c->r[0]=0;c->r[1]=flags;}
void hle_swi(CPU*c,u32 n){static u8 warned[256];
 switch(n){
 case 0:case 1:case 2:break;
 case 3:c->cyc-=c->r[0]>>1;break;
 case 4:intrwait(c,c->r[0],c->r[1]);break;
 case 5:intrwait(c,1,1);break;
 case 6:c->halt=1;break;
 case 7:c->halt=1;break;
 case 8:c->halt=1;break;
 case 9:{s32 a=c->r[0],b=c->r[1];if(!b){c->r[0]=a<0?-1:1;c->r[1]=a;c->r[3]=1;break;}if(a==(s32)0x80000000&&b==-1){c->r[0]=a;c->r[1]=0;c->r[3]=(u32)a;break;}c->r[0]=a/b;c->r[1]=a%b;c->r[3]=(u32)(((s32)c->r[0])<0?-(s32)c->r[0]:(s32)c->r[0]);break;}
 case 0xB:cpuset(c);break;case 0xC:fastset(c);break;
 case 0xD:{u32 x=c->r[0];u32 r=0,b=1u<<30;while(b>x)b>>=2;while(b){if(x>=r+b){x-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}c->r[0]=r;break;}
 case 0xE:c->r[0]=crc16(c->r[0]&0xffff,c->r[1],c->r[2]&~1u,c);break;
 case 0xF:c->r[0]=0;break;
 case 0x1A:{extern const s16 bios_sin(int);c->r[0]=(u16)bios_sin(c->r[0]&0x3f);break;}
 case 0x1B:{extern const u32 bios_pitch(int);c->r[0]=bios_pitch(c->r[0]&0x2ff);break;}
 case 0x1C:{extern const u32 bios_vol(int);c->r[0]=bios_vol(c->r[0]);break;}
 case 0x11:lz77(c,0);break;case 0x12:lz77(c,1);break;
 case 0x14:rle(c,0);break;case 0x15:rle(c,1);break;
 default:if(!warned[n]){warned[n]=1;fprintf(stderr,"SWI %02x unimplemented cpu%d pc=%08x\n",n,c->id,c->pc);}break;}
}
void dbg_ctx(CPU*c,u32 v){static int n;if(n++<3){u32 ctx=c->r[0]-4;fprintf(stderr,"msr spsr T cpu%d v=%08x ctx=%08x pc=%08x:",c->id,v,ctx,c->pc);for(int i=0;i<20;i++)fprintf(stderr," %08x",R32c(c,ctx+i*4));fprintf(stderr,"\n");dump(c);}}
