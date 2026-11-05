#include "rt.h"
u8 mainram[0x400000],sharedwram[0x8000],wram7[0x10000],itcm[0x8000],dtcm[0x4000],vram[0xA4000],pal[0x800],oam[0x800];
u8 io9[0x2000],io7[0x2000];u32 dtcm_base=0x027C0000;
static u8*p_abg[32],*p_aobj[16],*p_bbg[8],*p_bobj[8],*p_lcdc[42],*p_tex[32],*p_texpal[8],*p_a7[16];
static u8*x_abg[4],*x_aobj,*x_bbg[4],*x_bobj;static u8 dummy[0x4000];
void vram_remap(void){memset(p_abg,0,sizeof p_abg);memset(p_aobj,0,sizeof p_aobj);memset(p_bbg,0,sizeof p_bbg);memset(p_bobj,0,sizeof p_bobj);memset(p_lcdc,0,sizeof p_lcdc);memset(p_tex,0,sizeof p_tex);memset(p_texpal,0,sizeof p_texpal);memset(p_a7,0,sizeof p_a7);memset(x_abg,0,sizeof x_abg);x_aobj=0;memset(x_bbg,0,sizeof x_bbg);x_bobj=0;
 static const u32 off[9]={0,0x20000,0x40000,0x60000,0x80000,0x90000,0x94000,0x98000,0xA0000},sz[9]={0x20000,0x20000,0x20000,0x20000,0x10000,0x4000,0x4000,0x8000,0x4000},lc[9]={0,0x20000,0x40000,0x60000,0x80000,0x90000,0x94000,0x98000,0xA0000};
#define MAP(tb,mask,base,n) for(u32 k_=0;k_<(n)/0x4000;k_++)tb[((base)/0x4000+k_)&(mask)]=b+k_*0x4000
 for(int i=0;i<9;i++){u8 cn=io9[i<7?0x240+i:0x241+i];if(!(cn&0x80))continue;int m=cn&7,o=(cn>>3)&3;u8*b=vram+off[i];u32 n=sz[i];
  if(m==0){MAP(p_lcdc,63,lc[i],n);continue;}
  if(i<=3){if(m==1){MAP(p_abg,31,o*0x20000,n);}else if(m==2&&i<=1){MAP(p_aobj,15,(o&1)*0x20000,n);}else if(m==2){MAP(p_a7,15,(o&1)*0x20000,n);}else if(m==3){MAP(p_tex,31,o*0x20000,n);}else if(m==4&&i==2){MAP(p_bbg,7,0,n);}else if(m==4&&i==3){MAP(p_bobj,7,0,n);}}
  else if(i==4){if(m==1){MAP(p_abg,31,0,n);}else if(m==2){MAP(p_aobj,15,0,n);}else if(m==3){MAP(p_texpal,7,0,n);}else if(m==4){for(int s=0;s<4;s++)x_abg[s]=b+s*0x2000;}}
  else if(i<=6){u32 fo=(o&1)*0x4000+(o>>1)*0x10000;if(m==1){MAP(p_abg,31,fo,n);}else if(m==2){MAP(p_aobj,15,fo,n);}else if(m==3){p_texpal[(o&1)+(o>>1)*4]=b;}else if(m==4){int s=(o&1)*2;x_abg[s]=b;x_abg[s+1]=b+0x2000;}else if(m==5){x_aobj=b;}}
  else if(i==7){if(m==1){MAP(p_bbg,7,0,n);}else if(m==2){for(int s=0;s<4;s++)x_bbg[s]=b+s*0x2000;}}
  else{if(m==1){MAP(p_bbg,7,0x8000,n);}else if(m==2){MAP(p_bobj,7,0,n);}else if(m==3){x_bobj=b;}}
 }}
static inline u8* pg(u8*p,u32 a){return p?p+(a&0x3fff):dummy+(a&0x3fff);}
u8* vmap_a_bg(u32 o){return pg(p_abg[(o>>14)&31],o);}u8* vmap_a_obj(u32 o){return pg(p_aobj[(o>>14)&15],o);}u8* vmap_b_bg(u32 o){return pg(p_bbg[(o>>14)&7],o);}u8* vmap_b_obj(u32 o){return pg(p_bobj[(o>>14)&7],o);}
u8* vmap_tex(u32 o){return pg(p_tex[(o>>14)&31],o);}u8* vmap_texpal(u32 o){return pg(p_texpal[(o>>14)&7],o);}
u8* vmap_extpal(int e,int obj,u32 o){u8*p;if(e==0)p=obj?x_aobj:x_abg[(o>>13)&3];else p=obj?x_bobj:x_bbg[(o>>13)&3];return p?p+(o&0x1fff):dummy;}
u8* vram_page(u32 a){switch(a&0xE00000){case 0:return vmap_a_bg(a);case 0x200000:return vmap_b_bg(a);case 0x400000:return vmap_a_obj(a);case 0x600000:return vmap_b_obj(a);default:return pg(p_lcdc[(a>>14)&63],a);}}
static u8 wcnt(void){return io9[0x247]&3;}
static u8* w9(u32 a){switch(wcnt()){case 0:return sharedwram+(a&0x7fff);case 1:return sharedwram+0x4000+(a&0x3fff);case 2:return sharedwram+(a&0x3fff);default:return dummy+(a&0x3fff);}}
static u8* w7(u32 a){if(a>=0x03800000)return wram7+(a&0xffff);switch(wcnt()){case 0:return wram7+(a&0xffff);case 1:return sharedwram+(a&0x3fff);case 2:return sharedwram+0x4000+(a&0x3fff);default:return sharedwram+(a&0x7fff);}}
#define RD(T,p) (*(T*)(p))
static u8* a9p(u32 a){switch(a>>24){case 3:return w9(a);case 5:return pal+(a&0x7ff);case 6:return vram_page(a);case 7:return oam+(a&0x7ff);}return 0;}
static u8* a7p(u32 a){switch(a>>24){case 3:return w7(a);case 6:{u8*p=p_a7[(a>>14)&15];return p?p+(a&0x3fff):dummy;}}return 0;}
u32 a9_r8(u32 a){if((a>>24)==4)return io_r(&cpu9,a,1);u8*p=a9p(a);return p?*p:0;}
u32 a9_r16(u32 a){if((a>>24)==4)return io_r(&cpu9,a,2);u8*p=a9p(a);return p?RD(u16,p):0;}
u32 a9_r32(u32 a){if((a>>24)==4)return io_r(&cpu9,a,4);u8*p=a9p(a);return p?RD(u32,p):0;}
void a9_w8(u32 a,u32 v){if((a>>24)==4){io_w(&cpu9,a,v,1);return;}if((a>>24)==3){*w9(a)=v;return;}/* byte writes to VRAM/pal/oam are ignored */}
void a9_w16(u32 a,u32 v){if((a>>24)==4){io_w(&cpu9,a,v,2);return;}u8*p=a9p(a);if(p)RD(u16,p)=v;}
void a9_w32(u32 a,u32 v){if((a>>24)==4){io_w(&cpu9,a,v,4);return;}u8*p=a9p(a);if(p)RD(u32,p)=v;}
u32 a7_r8(u32 a){if((a>>24)==4)return io_r(&cpu7,a,1);u8*p=a7p(a);return p?*p:0;}
u32 a7_r16(u32 a){if((a>>24)==4)return io_r(&cpu7,a,2);u8*p=a7p(a);return p?RD(u16,p):0;}
u32 a7_r32(u32 a){if((a>>24)==4)return io_r(&cpu7,a,4);u8*p=a7p(a);return p?RD(u32,p):0;}
void a7_w8(u32 a,u32 v){if((a>>24)==4){io_w(&cpu7,a,v,1);return;}u8*p=a7p(a);if(p)*p=v;}
void a7_w16(u32 a,u32 v){if((a>>24)==4){io_w(&cpu7,a,v,2);return;}u8*p=a7p(a);if(p)RD(u16,p)=v;}
void a7_w32(u32 a,u32 v){if((a>>24)==4){io_w(&cpu7,a,v,4);return;}u8*p=a7p(a);if(p)RD(u32,p)=v;}
